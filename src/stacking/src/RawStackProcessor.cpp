#include <astro/StackMath.hpp>
#include <astro/Core.hpp>
#include <astro/Calibration.hpp>
#include <astro/PlateSolve.hpp>
#include <libraw/libraw.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <thread>
#define NOMINMAX
#include <windows.h>
namespace astro::stacking {
using Json=nlohmann::json;
namespace {
std::string utf8(const std::filesystem::path& p){auto u=p.u8string();return {u.begin(),u.end()};}
void rawCheck(int error){if(error!=LIBRAW_SUCCESS)throw std::runtime_error(std::string("RAW decode: ")+libraw_strerror(error));}
void replace(const std::filesystem::path& tmp,const std::filesystem::path& target){if(!MoveFileExW(tmp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit stack output (Windows error "+std::to_string(GetLastError())+")");}
}
std::size_t correctIsolatedBayerPixels(std::span<std::uint16_t> pixels,int width,int height,int stride,float black,float white){
    if(width<5||height<5||stride<width||pixels.size()<std::size_t(stride)*height||!std::isfinite(black)||!std::isfinite(white)||white<=black)throw std::invalid_argument("Invalid Bayer buffer");
    // Read an immutable neighbourhood. Never smear a correction into the next pixel.
    std::vector<std::pair<std::size_t,std::uint16_t>> replacements;
    const float floor=std::max(32.f,(white-black)*.002f);
    for(int y=2;y<height-2;++y)for(int x=2;x<width-2;++x){
        const auto pos=std::size_t(y)*stride+x;const float peak=pixels[pos];
        std::array<std::uint16_t,8> neighbours{};int n=0;float maximum=0;
        for(int dy=-2;dy<=2;dy+=2)for(int dx=-2;dx<=2;dx+=2)if(dx||dy){auto v=pixels[pos+dy*stride+dx];neighbours[n++]=v;maximum=std::max(maximum,float(v));}
        if(peak<=maximum+floor)continue;
        std::sort(neighbours.begin(),neighbours.end());const float median=(neighbours[3]+neighbours[4])*.5f;
        const float excess=peak-median;
        if(excess<=std::max(floor,8.f*std::max(4.f,maximum-median)))continue;
        // A real optical PSF has neighbouring support. Preserve even very small stars;
        // only replace isolated high CFA impulses, not all bright/red pixels.
        int support=0;const float supportLevel=std::max(black,median)+std::max(16.f,excess*.02f);
        for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if((dx||dy)&&pixels[pos+dy*stride+dx]>supportLevel)++support;
        if(support>=2)continue;
        replacements.emplace_back(pos,std::uint16_t(median));
    }
    for(auto [p,value]:replacements)pixels[p]=value;
    return replacements.size();
}
LinearImage decodeRaw(const std::filesystem::path& path,int memoryLimitMb,const std::array<float,4>& whiteBalance,bool cosmetic){
    LibRaw raw;rawCheck(raw.open_file(path.c_str()));
    const auto n=std::uint64_t(raw.imgdata.sizes.raw_width)*raw.imgdata.sizes.raw_height;
    if(!n||n*100>std::uint64_t(memoryLimitMb)*1024*1024)throw std::runtime_error("RAW exceeds configured memory budget");
    auto& p=raw.imgdata.params;p.output_bps=16;p.no_auto_bright=1;p.adjust_maximum_thr=0;p.half_size=0;p.gamm[0]=1;p.gamm[1]=1;p.use_auto_wb=0;p.use_camera_wb=0;p.output_color=1;p.user_flip=0;p.user_qual=3;
    rawCheck(raw.unpack());LinearImage result;
    const auto* wb=whiteBalance[0]>0?whiteBalance.data():raw.imgdata.color.cam_mul;
    if(!(wb[0]>0&&wb[1]>0&&wb[2]>0))wb=raw.imgdata.color.pre_mul;
    for(int c=0;c<4;++c){const float v=wb[c]>0?wb[c]:wb[1];if(!std::isfinite(v)||v<=0)throw std::runtime_error("RAW white balance metadata is invalid");p.user_mul[c]=result.whiteBalance[c]=v;}
    const auto& sizes=raw.imgdata.sizes;
    if(cosmetic&&raw.imgdata.rawdata.raw_image&&raw.imgdata.idata.filters>1000){
        const int stride=int(sizes.raw_pitch/sizeof(std::uint16_t));
        const auto black=raw.imgdata.color.black+*std::max_element(raw.imgdata.color.cblack,raw.imgdata.color.cblack+4);
        result.correctedPixels=correctIsolatedBayerPixels({raw.imgdata.rawdata.raw_image,std::size_t(stride)*sizes.raw_height},sizes.raw_width,sizes.raw_height,stride,float(black),float(raw.imgdata.color.maximum));
    }
    rawCheck(raw.dcraw_process());int error=0;
    std::unique_ptr<libraw_processed_image_t,decltype(&LibRaw::dcraw_clear_mem)> bitmap(raw.dcraw_make_mem_image(&error),&LibRaw::dcraw_clear_mem);rawCheck(error);
    if(!bitmap||bitmap->type!=LIBRAW_IMAGE_BITMAP||bitmap->bits!=16||bitmap->colors!=3)throw std::runtime_error("Unsupported RAW output; 16-bit linear RGB required");
    result.width=bitmap->width;result.height=bitmap->height;const auto samples=std::size_t(result.width)*result.height*3;
    if(bitmap->data_size!=samples*2)throw std::runtime_error("RAW output size mismatch");result.rgb.resize(samples);
    const auto* pixels=reinterpret_cast<const std::uint16_t*>(bitmap->data);for(std::size_t i=0;i<samples;++i)result.rgb[i]=pixels[i]/65535.f;
    return result;
}
void writeFits(const std::filesystem::path& path,const Accumulator& a,double seconds){
    if(!a.frames)throw std::invalid_argument("Cannot save an empty stack");auto tmp=path;tmp+=".tmp";std::ofstream out(tmp,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Cannot create FITS");
    auto header=[&](int weights){std::string text;auto card=[&](std::string key,std::string value){std::string s=key;s.resize(8,' ');s+="= ";if(value.size()<20)s+=std::string(20-value.size(),' ');s+=value;s.resize(80,' ');text+=s;};
        if(weights){card("XTENSION","'IMAGE   '");card("BITPIX",weights==2?"-32":"32");card("NAXIS","2");}else{card("SIMPLE","T");card("BITPIX","-32");card("NAXIS","3");}card("NAXIS1",std::to_string(a.width));card("NAXIS2",std::to_string(a.height));if(!weights){card("NAXIS3","3");card("EXTEND","T");card("NCOMBINE",std::to_string(a.frames));card("EXPTIME",std::to_string(seconds));card("BUNIT","'normalized linear RGB'");card("COLORSPC","'LINEAR SRGB'");card("DRIZSCAL",std::to_string(a.pixelScale));card("PIXFRAC",std::to_string(a.pixfrac));}else{card("PCOUNT","0");card("GCOUNT","1");card("EXTNAME",weights==2?"'WEIGHT'":"'COVERAGE'");}std::string end="END";end.resize(80,' ');text+=end;text.resize(((text.size()+2879)/2880)*2880,' ');out.write(text.data(),text.size());};
    std::array<char,65536> buffer{};std::size_t used=0;auto integer=[&](std::uint32_t bits){for(int shift=24;shift>=0;shift-=8)buffer[used++]=char(bits>>shift);if(used==buffer.size()){out.write(buffer.data(),used);used=0;}};
    auto flush=[&](std::size_t bytes){if(used)out.write(buffer.data(),used);used=0;std::string pad((2880-bytes%2880)%2880,'\0');out.write(pad.data(),pad.size());};
    auto n=std::size_t(a.width)*a.height;header(false);for(int c=0;c<3;++c)for(std::size_t i=0;i<n;++i)integer(std::bit_cast<std::uint32_t>(a.value(i,c)));flush(n*12);
    header(1);for(auto count:a.count)integer(count);flush(n*4);if(a.pixelScale>1){header(2);for(auto weight:a.weights)integer(std::bit_cast<std::uint32_t>(float(weight)));flush(n*4);}out.flush();if(!out)throw std::runtime_error("FITS write failed; original RAW frames retained");out.close();replace(tmp,path);
}
class Processor final:public IStackProcessor {
    struct Work{int kind{};std::filesystem::path directory;StackOptions options;std::string text;StackFrameJob frame;StackDisplay display;bool failed{};};
    mutable std::mutex mutex_;std::condition_variable_any wake_;std::deque<Work> queue_;StackProcessResult result_;std::shared_ptr<const StackPreview> preview_;
    std::atomic<bool> cancelSolver_{};std::jthread thread_;Accumulator sum_;std::vector<Point> reference_;StackOptions options_;StackDisplay display_;std::filesystem::path directory_;Json report_;std::uint64_t revision_{};double seconds_{};bool begun_{},fatal_{};std::array<float,4> whiteBalance_{};std::unique_ptr<Calibration> calibration_;std::array<float,3> referenceBackground_{};bool haveBackground_{};Json sourceSignature_;SensorFrame master_;int masterFrames_{};
    void enqueue(Work w){std::lock_guard lock(mutex_);if(queue_.size()>=3)throw std::logic_error("Stack processor queue is full");queue_.push_back(std::move(w));wake_.notify_one();}
    void checkpoint(){auto path=directory_/"stack.json",tmp=directory_/"stack.json.tmp";std::ofstream out(tmp,std::ios::binary|std::ios::trunc);out<<report_.dump(2)<<'\n';out.flush();if(!out)throw std::runtime_error("Cannot write stack report");out.close();replace(tmp,path);}
    std::shared_ptr<const StackPreview> preview(){auto p=std::make_shared<StackPreview>(render(sum_,display_,++revision_));std::lock_guard lock(mutex_);preview_=p;return p;}
    void process(Work w){
        if(w.kind==0){sum_={};master_={};masterFrames_=0;whiteBalance_={};reference_.clear();directory_=w.directory;options_=w.options;begun_=true;fatal_=false;report_={{"schema_version",1},{"status","preparing"},{"frames",Json::array()},{"accepted",0},{"rejected",0}};if(!options_.solverExecutable.empty()&&!std::filesystem::is_regular_file(pathFromUtf8(options_.solverExecutable)))throw std::invalid_argument("ASTAP executable does not exist");if(!options_.solverDatabase.empty()&&!std::filesystem::is_directory(pathFromUtf8(options_.solverDatabase)))throw std::invalid_argument("ASTAP database directory does not exist");calibration_=std::make_unique<Calibration>(options_,[this]{return cancelSolver_.load()||thread_.get_stop_token().stop_requested();});haveBackground_=false;sourceSignature_=nullptr;display_={};seconds_=0;fatal_=false;begun_=true;report_={{"schema_version",1},{"status","running"},{"alignment",options_.alignment},{"integration","linear RGB mean; bilinear registration; online sigma rejection after 5 samples"},{"raw_decoder","LibRaw 16-bit linear sRGB, first RAW white balance frozen before demosaic, camera colour matrix, no auto brightness or scene white-point scaling"},{"metadata",Json::parse(w.text)},{"frames",Json::array()},{"accepted",0},{"rejected",0},{"calibration",Json::parse(calibration_->report())}};
            report_["integration"]=options_.drizzleScale>1?"area-overlap RGB drizzle; weighted online mean":"bilinear registration; online mean";
            report_["rejection"]=!options_.rejection?"off":options_.winsorizedRejection?"online Winsor limits after 5 samples":"online sigma after 5 samples";
            if(calibration_->enabled()||options_.frameType!="Light")report_["raw_decoder"]="LibRaw sensor unpack; signed float Bayer arithmetic and bilinear demosaic; frozen white balance and camera RGB matrix";
            checkpoint();{std::lock_guard lock(mutex_);preview_.reset();result_={};result_.ready=result_.prepared=true;}return;}
        if(w.kind==3){display_=w.display;if(sum_.frames)preview();return;}
        StackProcessResult result;result.ready=true;
        try{
            if(!begun_)throw std::logic_error("Stack processor not initialized");
            if(w.kind==2){
                result.finalized=true;report_["status"]=(fatal_||w.failed)?"error":sum_.frames?(report_["rejected"].get<int>()>0?"complete_with_rejections":"complete"):"empty";report_["completion_reason"]=w.text;
                if(sum_.frames){auto path=directory_/"stack-linear.fits";writeFits(path,sum_,seconds_);result.finalFile=utf8(path);report_["final_file"]="stack-linear.fits";if(options_.frameType!="Light"&&masterFrames_==sum_.frames){auto masterFile=directory_/("master-"+options_.frameType+".fits");writeSensorMaster(masterFile,master_,sum_.frames,options_.frameType);report_["sensor_master"]=utf8(masterFile.filename());report_["display_stack_file"]="stack-linear.fits";report_["final_file"]=utf8(masterFile.filename());result.finalFile=utf8(masterFile);}if(!options_.solverExecutable.empty()){auto solution=solvePlate(path,options_,[this]{return cancelSolver_.load()||thread_.get_stop_token().stop_requested();});result.warning=!solution.solved;result.astrometry=solution.solved?"Astrometry solved":"Astrometry: "+solution.detail;report_["astrometry"]={{"solved",solution.solved},{"detail",solution.detail},{"wcs",solution.wcs}};}if(options_.exportXisf){writeXisf(directory_/"stack-linear.xisf",sum_,seconds_);report_["xisf_file"]="stack-linear.xisf";}if(options_.exportTiff){writeTiff(directory_/"stack-linear.tiff",sum_);report_["tiff_file"]="stack-linear.tiff";}}
                report_["preview_white_balance_gains"]=sum_.previewGains;report_["width"]=sum_.width;report_["height"]=sum_.height;report_["integrated_seconds"]=seconds_;report_["clipped_samples"]=sum_.clipped;checkpoint();result.fatal=fatal_||w.failed;result.detail=(sum_.frames||report_["frames"].empty())?w.text:"No frame passed RAW/alignment validation; originals retained";if(!result.astrometry.empty())result.detail+="; "+result.astrometry;begun_=false;
            }else{
                Json entry={{"number",w.frame.number},{"exposure_seconds",w.frame.exposureSeconds},{"files",Json::array()}};std::filesystem::path raw;
                for(auto& p:w.frame.files){entry["files"].push_back(utf8(p.lexically_relative(directory_)));auto ext=p.extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return char(std::tolower(c));});if(ext==".cr2")raw=p;}
                const auto previousFrames=sum_.frames;
                try{
                    if(raw.empty())throw std::invalid_argument("No CR2 was downloaded; select RAW in Camera");
                    auto header=std::make_unique<LibRaw>();rawCheck(header->open_file(raw.c_str()));const auto& hd=header->imgdata;
                    Json signature={{"model",std::string(hd.idata.make)+" "+hd.idata.model},{"iso",int(std::lround(hd.other.iso_speed))},{"width",hd.sizes.width},{"height",hd.sizes.height},{"cfa",hd.idata.filters}};
                    const double actualExposure=hd.other.shutter;
                    if(!std::isfinite(actualExposure)||actualExposure<=0)throw std::invalid_argument("RAW exposure metadata missing");
                    if(!sourceSignature_.is_null()&&(signature!=sourceSignature_.at("camera")||std::abs(actualExposure-sourceSignature_.at("exposure").get<double>())>std::max(.00001,actualExposure*.01)))throw std::invalid_argument("RAW camera/ISO/exposure/geometry mismatch with reference");
                    result.exposureSeconds=actualExposure;entry["requested_exposure_seconds"]=w.frame.exposureSeconds;entry["exposure_seconds"]=result.exposureSeconds;entry["camera"]=signature;
auto image=calibration_->enabled()?calibration_->decode(raw,whiteBalance_):decodeRaw(raw,options_.memoryLimitMb,whiteBalance_,options_.cosmeticCorrection);if(options_.frameType!="Light"){auto sensor=readSensor(raw,options_.memoryLimitMb);addToMaster(master_,sensor,masterFrames_+1);++masterFrames_;image=demosaicCalibrated(sensor,whiteBalance_);}if(whiteBalance_[0]==0){whiteBalance_=image.whiteBalance;report_["white_balance_multipliers"]=whiteBalance_;}entry["cosmetic_corrected_pixels"]=image.correctedPixels;const auto banding=reduceHorizontalBanding(image,options_);entry["banding"]={{"enabled",options_.horizontalBanding},{"rms_correction",banding.rms},{"limited_rows",banding.limitedRows}};std::array<float,3> background{};if(options_.backgroundNormalization||options_.gradientRemoval){background=normalizeBackground(image,options_.backgroundNormalization&&haveBackground_?&referenceBackground_:nullptr,options_.gradientRemoval);entry["background"]=background;}auto stars=detectStars(image,options_.detectionSnr);if(!stars.empty()){std::vector<double> hfd,fwhm;for(const auto& star:stars){hfd.push_back(star.hfd);fwhm.push_back(star.fwhm);}std::sort(hfd.begin(),hfd.end());std::sort(fwhm.begin(),fwhm.end());result.hfd=hfd[hfd.size()/2];result.fwhm=fwhm[fwhm.size()/2];entry["median_hfd_pixels"]=result.hfd;entry["median_fwhm_pixels"]=result.fwhm;}result.stars=int(stars.size());Alignment match{true,{},int(stars.size()),0,1,"Integrated (alignment disabled)"};
                    if(options_.alignment){if(reference_.empty()){if(stars.size()<options_.minimumStars||!coverage(stars,image.width,image.height))match={false,{},0,0,0,"Reference requires more distributed, unsaturated stars"};else match.reason="Reference accepted";}else match=align(reference_,stars,image.width,image.height,options_);}
                    result.accepted=match.accepted;result.matches=match.matches;result.rms=match.rms;result.overlap=match.overlap;result.detail=match.reason;
                    if(match.accepted){if(sourceSignature_.is_null())sourceSignature_={{"camera",signature},{"exposure",actualExposure}};if(!haveBackground_){referenceBackground_=background;haveBackground_=true;}sum_.add(image,match.transform,options_);if(reference_.empty())reference_=std::move(stars);seconds_+=result.exposureSeconds;
                        if(sum_.frames==1){std::vector<float> levels;for(std::size_t p=0;p<sum_.count.size();p+=16)if(sum_.count[p])levels.push_back(.2126f*sum_.value(p,0)*sum_.previewGains[0]+.7152f*sum_.value(p,1)*sum_.previewGains[1]+.0722f*sum_.value(p,2)*sum_.previewGains[2]);if(!levels.empty()){std::sort(levels.begin(),levels.end());display_.low=std::clamp(levels[levels.size()/1000]*.8f,0.f,.999f);display_.high=std::clamp(levels[std::min(levels.size()-1,levels.size()*999/1000)]*1.5f,display_.low+.0001f,1.f);}}
                        LinearImage integrated{sum_.width,sum_.height,std::vector<float>(sum_.count.size()*3)};for(std::size_t p=0;p<sum_.count.size();++p)for(int c=0;c<3;++c)integrated.rgb[p*3+c]=sum_.value(p,c);auto stackStars=detectStars(integrated,options_.detectionSnr);if(!stackStars.empty()){std::vector<double> hfd,fwhm;for(const auto& star:stackStars){hfd.push_back(star.hfd);fwhm.push_back(star.fwhm);}std::sort(hfd.begin(),hfd.end());std::sort(fwhm.begin(),fwhm.end());result.stackHfd=hfd[hfd.size()/2]/options_.drizzleScale;result.stackFwhm=fwhm[fwhm.size()/2]/options_.drizzleScale;}report_["stack_hfd_native_pixels"]=result.stackHfd;report_["stack_fwhm_native_pixels"]=result.stackFwhm;
                        result.preview=preview();if(options_.checkpointFrames>0&&sum_.frames%options_.checkpointFrames==0){writeFits(directory_/"stack-checkpoint.fits",sum_,seconds_);report_["checkpoint_frames"]=sum_.frames;report_["checkpoint_file"]="stack-checkpoint.fits";}}
                    entry["transform_reference_to_source"]={match.transform.a,match.transform.b,match.transform.tx,match.transform.c,match.transform.d,match.transform.ty};
                }catch(const std::invalid_argument& e){result.accepted=sum_.frames>previousFrames;result.detail=e.what();}catch(const std::exception& e){result.accepted=sum_.frames>previousFrames;result.fatal=fatal_=true;result.detail=e.what();}
                entry["accepted"]=result.accepted;entry["stars"]=result.stars;entry["matches"]=result.matches;entry["rms_pixels"]=result.rms;entry["overlap"]=result.overlap;entry["reason"]=result.detail;
                report_["frames"].push_back(entry);report_["accepted"]=sum_.frames;report_["rejected"]=int(report_["frames"].size())-sum_.frames;checkpoint();
            }
        }catch(const std::exception& e){result.fatal=fatal_=true;result.finalized=w.kind==2;result.detail=e.what();}
        std::lock_guard lock(mutex_);result_=std::move(result);
    }
public:
    Processor():thread_([this](std::stop_token stop){while(true){Work work;{std::unique_lock lock(mutex_);wake_.wait(lock,stop,[&]{return !queue_.empty();});if(queue_.empty()){if(stop.stop_requested())break;continue;}work=std::move(queue_.front());queue_.pop_front();}try{process(std::move(work));}catch(const std::exception& e){std::lock_guard lock(mutex_);fatal_=!cancelSolver_.load();result_={};result_.ready=true;result_.fatal=fatal_;result_.prepared=!fatal_;result_.detail=e.what();}}}){}
    ~Processor(){thread_.request_stop();wake_.notify_all();thread_.join();}
    void begin(const std::filesystem::path& p,const StackOptions& o,const std::string& metadata)override{validateStack(o);cancelSolver_=false;{std::lock_guard lock(mutex_);result_={};preview_.reset();}Work w;w.directory=p;w.options=o;w.text=metadata;enqueue(std::move(w));}
    void submit(StackFrameJob job)override{Work w;w.kind=1;w.frame=std::move(job);enqueue(std::move(w));}
    StackProcessResult poll()override{std::lock_guard lock(mutex_);auto r=std::move(result_);result_={};r.preview=preview_;return r;}
    std::shared_ptr<const StackPreview> currentPreview() const override{std::lock_guard lock(mutex_);return preview_;}
    void finish(const std::string& reason,bool failed)override{Work w;w.kind=2;w.text=reason;w.failed=failed;enqueue(std::move(w));}
    void cancelOptionalProcessing()override{cancelSolver_=true;}
    void display(StackDisplay d)override{validateStackDisplay(d);std::lock_guard lock(mutex_);auto it=std::find_if(queue_.begin(),queue_.end(),[](const auto& w){return w.kind==3;});Work w;w.kind=3;w.display=d;if(it!=queue_.end())*it=w;else if(queue_.size()<3)queue_.push_back(w);wake_.notify_one();}
};
std::shared_ptr<IStackProcessor> makeStackProcessor(){return std::make_shared<Processor>();}
}
