#include <astro/Calibration.hpp>
#include <astro/Core.hpp>
#include <libraw/libraw.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

namespace astro::stacking {
namespace {
void check(int e){if(e!=LIBRAW_SUCCESS)throw std::runtime_error(std::string("Calibration RAW: ")+libraw_strerror(e));}
int color(const SensorFrame& f,int x,int y){return f.cfa[(y&1)*2+(x&1)];}
void valid(const SensorFrame& f){
    if(f.width<4||f.height<4||f.pixels.size()!=std::size_t(f.width)*f.height||f.iso<=0||!std::isfinite(f.exposure)||f.exposure<=0)throw std::invalid_argument("Invalid calibration sensor metadata");
    std::array<int,3> counts{};for(int c:f.cfa){if(c<0||c>2)throw std::invalid_argument("Only RGB Bayer calibration is supported");++counts[c];}
    if(counts!=std::array<int,3>{1,2,1})throw std::invalid_argument("Unsupported calibration CFA");
    if(std::any_of(f.pixels.begin(),f.pixels.end(),[](float v){return !std::isfinite(v);}))throw std::invalid_argument("Non-finite calibration sample");
}
float median(std::vector<float> values){if(values.empty())throw std::invalid_argument("Empty calibration samples");auto p=values.begin()+values.size()/2;std::nth_element(values.begin(),p,values.end());return *p;}
void updateMean(SensorFrame& sum,const SensorFrame& next,int count){validateSensorMatch(sum,next,true);for(std::size_t i=0;i<sum.pixels.size();++i)sum.pixels[i]+=float((double(next.pixels[i])-sum.pixels[i])/count);}
}
SensorFrame readSensor(const std::filesystem::path& path,int memoryMb){
    LibRaw raw;check(raw.open_file(path.c_str()));const auto& s=raw.imgdata.sizes;
    const auto n=std::uint64_t(s.raw_width)*s.raw_height;
    if(!n||n*128>std::uint64_t(memoryMb)*1024*1024)throw std::runtime_error("Calibration exceeds configured memory budget");
    check(raw.unpack());const auto& d=raw.imgdata;
    if(!d.rawdata.raw_image||d.idata.filters<=1000||d.idata.colors!=3||d.color.cblack[4]||d.color.cblack[5])throw std::invalid_argument("Unsupported calibration RAW layout or patterned black level");
    SensorFrame f;f.width=s.width;f.height=s.height;f.iso=int(std::lround(d.other.iso_speed));f.exposure=d.other.shutter;f.model=std::string(d.idata.make)+" "+d.idata.model;
    for(int y=0;y<2;++y)for(int x=0;x<2;++x){int c=raw.COLOR(y+s.top_margin,x+s.left_margin);f.cfa[y*2+x]=c==3?1:c;}
    for(int c=0;c<4;++c)f.wb[c]=d.color.cam_mul[c]>0?d.color.cam_mul[c]:d.color.pre_mul[c];
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)f.matrix[r][c]=d.color.rgb_cam[r][c];
    f.pixels.resize(std::size_t(f.width)*f.height);const auto stride=s.raw_pitch/sizeof(std::uint16_t);
    for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){int c=raw.COLOR(y+s.top_margin,x+s.left_margin);const auto black=d.color.black+d.color.cblack[c];const double range=double(d.color.maximum)-black;if(range<=0)throw std::invalid_argument("Invalid sensor white level");f.pixels[std::size_t(y)*f.width+x]=float((double(d.rawdata.raw_image[(y+s.top_margin)*stride+x+s.left_margin])-black)/range);}
    valid(f);return f;
}
void validateSensorMatch(const SensorFrame& a,const SensorFrame& b,bool exposure){
    valid(a);valid(b);
    if(a.width!=b.width||a.height!=b.height||a.model!=b.model||a.cfa!=b.cfa||a.iso!=b.iso)throw std::invalid_argument("Calibration camera, ISO, dimensions or CFA mismatch");
    if(exposure&&std::abs(a.exposure-b.exposure)>std::max(.00001,a.exposure*.01))throw std::invalid_argument("Calibration exposure mismatch; dark scaling is not applied");
}
SensorFrame meanMaster(const std::vector<SensorFrame>& frames){if(frames.empty())throw std::invalid_argument("No calibration frames");valid(frames.front());auto m=frames.front();for(std::size_t i=1;i<frames.size();++i)updateMean(m,frames[i],int(i+1));return m;}
void addToMaster(SensorFrame& master,const SensorFrame& frame,int count){if(count<1)throw std::invalid_argument("Invalid master count");if(count==1)master=frame;else updateMean(master,frame,count);}
std::vector<std::uint8_t> calibrationDefects(const SensorFrame& master){
    valid(master);std::vector<std::uint8_t> mask(master.pixels.size());
    for(int y=2;y<master.height-2;++y)for(int x=2;x<master.width-2;++x){std::vector<float> neighbours;for(int dy=-2;dy<=2;dy+=2)for(int dx=-2;dx<=2;dx+=2)if(dx||dy)neighbours.push_back(master.pixels[std::size_t(y+dy)*master.width+x+dx]);const auto level=median(neighbours);for(auto& v:neighbours)v=std::abs(v-level);const auto noise=1.4826f*median(neighbours);const auto p=std::size_t(y)*master.width+x;mask[p]=std::abs(master.pixels[p]-level)>std::max(.005f,8*noise);}
    return mask;
}
std::size_t repairCalibrationDefects(SensorFrame& science,std::span<const std::uint8_t> mask){
    valid(science);if(mask.size()!=science.pixels.size())throw std::invalid_argument("Defect map dimensions mismatch");auto original=science.pixels;std::size_t count=0;
    for(int y=0;y<science.height;++y)for(int x=0;x<science.width;++x){auto p=std::size_t(y)*science.width+x;if(!mask[p])continue;std::vector<float> samples;
        for(int r=2;r<=6&&samples.size()<4;r+=2){samples.clear();for(int dy=-r;dy<=r;dy+=2)for(int dx=-r;dx<=r;dx+=2)if(dx||dy){int xx=x+dx,yy=y+dy;if(xx<0||yy<0||xx>=science.width||yy>=science.height)continue;auto q=std::size_t(yy)*science.width+xx;if(!mask[q])samples.push_back(original[q]);}}
        if(samples.size()>=4){science.pixels[p]=median(std::move(samples));++count;}}
    return count;
}
void calibrateSensor(SensorFrame& light,const SensorFrame* bias,const SensorFrame* dark,const SensorFrame* flat,const SensorFrame* darkFlat){
    valid(light);for(auto f:{bias,dark,flat,darkFlat})if(f)validateSensorMatch(light,*f,f==dark);
    if(flat&&!bias&&!darkFlat)throw std::invalid_argument("Flat requires bias or dark-flat calibration");
    if(flat&&darkFlat)validateSensorMatch(*flat,*darkFlat,true);
    std::array<float,3> normal{1,1,1};
    if(flat){std::array<std::vector<float>,3> samples;for(int y=0;y<flat->height;++y)for(int x=0;x<flat->width;++x){auto i=std::size_t(y)*flat->width+x;float v=flat->pixels[i]-(darkFlat?darkFlat->pixels[i]:bias->pixels[i]);if(flat->pixels[i]>=.98f)throw std::invalid_argument("Flat contains saturated pixels");samples[color(*flat,x,y)].push_back(v);}for(int c=0;c<3;++c){normal[c]=median(std::move(samples[c]));if(normal[c]<.001f)throw std::invalid_argument("Flat signal too low");}}
    // Validate the entire flat before changing a single light sample.
    if(flat)for(int y=0;y<light.height;++y)for(int x=0;x<light.width;++x){auto i=std::size_t(y)*light.width+x;float response=(flat->pixels[i]-(darkFlat?darkFlat->pixels[i]:bias->pixels[i]))/normal[color(light,x,y)];if(response<.05f||response>20)throw std::invalid_argument("Flat has invalid or near-zero response; replace the flat set");}
    for(int y=0;y<light.height;++y)for(int x=0;x<light.width;++x){auto i=std::size_t(y)*light.width+x;double value=light.pixels[i]-(dark?dark->pixels[i]:bias?bias->pixels[i]:0);if(flat)value/=(flat->pixels[i]-(darkFlat?darkFlat->pixels[i]:bias->pixels[i]))/normal[color(light,x,y)];light.pixels[i]=float(value);}
}
LinearImage demosaicCalibrated(const SensorFrame& f,const std::array<float,4>& selected){
    valid(f);const auto wb=selected[0]>0?selected:f.wb;
    for(int c=0;c<3;++c)if(!std::isfinite(wb[c])||wb[c]<=0)throw std::invalid_argument("Invalid calibration white balance");
    LinearImage out;out.width=f.width;out.height=f.height;out.whiteBalance=wb;out.rgb.resize(f.pixels.size()*3);
    // Signed float bilinear demosaic: no integer requantization or black clipping.
    for(int y=0;y<f.height;++y)for(int x=0;x<f.width;++x){double camera[3]{};for(int c=0;c<3;++c){int count=0;if(color(f,x,y)==c){camera[c]=f.pixels[std::size_t(y)*f.width+x];count=1;}else for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){int xx=x+dx,yy=y+dy;if(xx>=0&&yy>=0&&xx<f.width&&yy<f.height&&color(f,xx,yy)==c){camera[c]+=f.pixels[std::size_t(yy)*f.width+xx];++count;}}if(!count)throw std::invalid_argument("Cannot interpolate CFA edge");camera[c]=camera[c]/count*wb[c]/wb[1];}
        for(int c=0;c<3;++c){double value=0;for(int k=0;k<3;++k)value+=f.matrix[c][k]*camera[k];out.rgb[(std::size_t(y)*f.width+x)*3+c]=float(value);}}
    return out;
}
Calibration::Calibration(const StackOptions& o,const std::function<bool()>& cancelled):options_(o){
    nlohmann::json report=nlohmann::json::object();
    auto load=[&](const std::string& folder,const char* name){std::unique_ptr<SensorFrame> master;if(folder.empty())return master;
        std::vector<std::filesystem::path> paths;for(auto& e:std::filesystem::directory_iterator(pathFromUtf8(folder))){auto ext=e.path().extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});if(e.is_regular_file()&&ext==".cr2")paths.push_back(e.path());}
        if(paths.empty()||paths.size()>100000)throw std::invalid_argument("Calibration folder must contain 1 to 100000 CR2 files");std::sort(paths.begin(),paths.end());int count=0;auto& list=report[name];list=nlohmann::json::array();
        for(const auto& p:paths){if(cancelled&&cancelled())throw std::runtime_error("Calibration preparation cancelled");auto frame=readSensor(p,o.memoryLimitMb);++count;if(!master)master=std::make_unique<SensorFrame>(std::move(frame));else updateMean(*master,frame,count);auto u=p.u8string();list.push_back(std::string(u.begin(),u.end()));}return master;};
    bias_=load(o.biasDirectory,"bias");dark_=load(o.darkDirectory,"dark");flat_=load(o.flatDirectory,"flat");darkFlat_=load(o.darkFlatDirectory,"dark_flat");
    const SensorFrame* base=bias_?bias_.get():dark_?dark_.get():flat_.get();if(base)for(auto p:{bias_.get(),dark_.get(),flat_.get(),darkFlat_.get()})if(p)validateSensorMatch(*base,*p,false);
    if(flat_&&darkFlat_)validateSensorMatch(*flat_,*darkFlat_,true);if(o.cosmeticCorrection&&(dark_||bias_)){defects_=calibrationDefects(dark_?*dark_:*bias_);report["calibration_defect_pixels"]=std::count(defects_.begin(),defects_.end(),std::uint8_t(1));}report["method"]="black-subtracted Bayer arithmetic masters; matched dark subtraction; per-CFA normalized flat; signed float bilinear demosaic";report_=report.dump();
}
bool Calibration::enabled()const{return bias_||dark_||flat_;}
LinearImage Calibration::decode(const std::filesystem::path& path,const std::array<float,4>& wb)const{auto f=readSensor(path,options_.memoryLimitMb);calibrateSensor(f,bias_.get(),dark_.get(),flat_.get(),darkFlat_.get());auto count=defects_.empty()?0:repairCalibrationDefects(f,defects_);auto image=demosaicCalibrated(f,wb);image.correctedPixels=count;return image;}
std::string Calibration::report()const{return report_;}
}
