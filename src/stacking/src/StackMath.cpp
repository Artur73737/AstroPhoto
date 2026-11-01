#include <astro/StackMath.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
namespace astro::stacking {
namespace {
float median(std::vector<float> v){if(v.empty())return 0;auto m=v.begin()+v.size()/2;std::nth_element(v.begin(),m,v.end());return v.size()%2?*m:(*m+*std::max_element(v.begin(),m))*.5f;}
void validImage(const LinearImage& i){if(i.width<8||i.height<8||i.rgb.size()!=static_cast<std::size_t>(i.width)*i.height*3)throw std::invalid_argument("Invalid linear image dimensions");if(std::any_of(i.rgb.begin(),i.rgb.end(),[](float v){return !std::isfinite(v);}))throw std::invalid_argument("Non-finite RAW pixel");}
double dist(Point a,Point b){return std::hypot(a.x-b.x,a.y-b.y);}
}
BandingResult reduceHorizontalBanding(LinearImage& image,const StackOptions& options){
    validateStack(options);validImage(image);BandingResult result;
    if(!options.horizontalBanding||options.bandingStrength==0)return result;
    const int w=image.width,h=image.height,stride=std::max(1,w/2048);
    std::vector<std::array<float,3>> rows(h),offsets(h);std::vector<bool> valid(h);
    // Robust channel row backgrounds exclude clipped pixels; fewer than half the
    // samples can be stars without moving the median. Smooth vertical gradients
    // are retained by subtracting only the residual against a local row median.
    for(int y=0;y<h;++y){std::array<std::vector<float>,3> samples;
        for(int x=0;x<w;x+=stride){auto p=(std::size_t(y)*w+x)*3;if(std::max({image.rgb[p],image.rgb[p+1],image.rgb[p+2]})>=.98f)continue;for(int c=0;c<3;++c)samples[c].push_back(image.rgb[p+c]);}
        if(samples[0].size()<std::size_t(std::max(4,w/stride/4)))continue;
        valid[y]=true;for(int c=0;c<3;++c)rows[y][c]=median(std::move(samples[c]));
    }
    for(int y=0;y<h;++y)if(valid[y])for(int c=0;c<3;++c){
        // Symmetric windows avoid inventing an offset at the top/bottom of a gradient.
        int radius=std::min({options.bandingRadius,y,h-1-y});if(radius<2)continue;
        std::vector<float> neighbours;for(int yy=y-radius;yy<=y+radius;++yy)if(yy!=y&&valid[yy])neighbours.push_back(rows[yy][c]);
        if(neighbours.size()<4)continue;offsets[y][c]=rows[y][c]-median(std::move(neighbours));
    }
    std::array<float,3> pedestal{};for(int c=0;c<3;++c){std::vector<float> all;for(int y=0;y<h;++y)if(valid[y])all.push_back(offsets[y][c]);pedestal[c]=median(std::move(all));}
    for(int y=0;y<h;++y)if(valid[y]){bool limited=false;for(int c=0;c<3;++c){double offset=offsets[y][c]-pedestal[c];limited|=std::abs(offset)>options.bandingMaxOffset;offset=std::clamp(offset,-options.bandingMaxOffset,options.bandingMaxOffset)*options.bandingStrength;result.rms+=offset*offset;
        for(int x=0;x<w;++x)image.rgb[(std::size_t(y)*w+x)*3+c]-=float(offset); // retain negative linear samples; no black clipping bias
    }if(limited)++result.limitedRows;}
    result.rms=std::sqrt(result.rms/(h*3));return result;
}
std::vector<Point> detectStars(const LinearImage& i,double snr){
    validImage(i);const int w=i.width,h=i.height;std::vector<float> y(static_cast<std::size_t>(w)*h);
    for(std::size_t p=0;p<y.size();++p){y[p]=.2126f*i.rgb[p*3]+.7152f*i.rgb[p*3+1]+.0722f*i.rgb[p*3+2];if(!std::isfinite(y[p]))throw std::invalid_argument("Non-finite RAW pixel");}
    std::vector<Point> stars;
    // Local 64x64 background / MAD protects faint stars from gradients.
    for(int by=4;by<h-4;by+=64)for(int bx=4;bx<w-4;bx+=64){
        std::vector<float> sample;for(int yy=by;yy<std::min(by+64,h-4);yy+=2)for(int x=bx;x<std::min(bx+64,w-4);x+=2)sample.push_back(y[yy*w+x]);
        const float bg=median(sample);for(auto& v:sample)v=std::abs(v-bg);const float noise=std::max(1.f/65535,1.4826f*median(sample));const double threshold=bg+snr*noise;
        for(int yy=by;yy<std::min(by+64,h-4);++yy)for(int x=bx;x<std::min(bx+64,w-4);++x){
            const auto p=yy*w+x;float peak=y[p];if(peak<threshold||peak>=.98f)continue;
            bool maximum=true;for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if((dx||dy)&&y[p+dy*w+dx]>=peak)maximum=false;if(!maximum)continue;
            double flux=0,cx=0,cy=0,xx=0,xy=0,yy2=0;int support=0;bool clipped=false;
            for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx){auto pi=p+dy*w+dx;float v=std::max(0.f,y[pi]-bg);if(v>noise*2)++support;for(int c=0;c<3;++c)if(i.rgb[pi*3+c]>=.995f)clipped=true;flux+=v;cx+=dx*v;cy+=dy*v;xx+=dx*dx*v;yy2+=dy*dy*v;xy+=dx*dy*v;}
            if(clipped||support<5||flux<=0)continue;cx/=flux;cy/=flux;xx=xx/flux-cx*cx;yy2=yy2/flux-cy*cy;xy=xy/flux-cx*cy;
            double disc=std::sqrt((xx-yy2)*(xx-yy2)+4*xy*xy),major=(xx+yy2+disc)/2,minor=(xx+yy2-disc)/2;
            if(minor<.25||major>9||minor/major<.35||std::abs(cx)>1.5||std::abs(cy)>1.5)continue;
            stars.push_back({x+cx,yy+cy,flux});
        }
    }
    std::sort(stars.begin(),stars.end(),[](auto a,auto b){return a.flux>b.flux;});
    std::vector<Point> result;std::array<int,64> cells{};
    for(auto p:stars){int cell=std::min(7,int(p.y*8/h))*8+std::min(7,int(p.x*8/w));if(cells[cell]>=8)continue;if(std::any_of(result.begin(),result.end(),[&](auto q){return dist(p,q)<6;}))continue;result.push_back(p);++cells[cell];if(result.size()==512)break;}
    return result;
}
Alignment align(const std::vector<Point>& ref,const std::vector<Point>& cur,int w,int h,const StackOptions& o){
    Alignment best;best.reason="Insufficient distributed alignment stars";
    const int required=std::max(o.minimumStars,int(std::ceil(std::min(ref.size(),cur.size())*.2)));
    if(ref.size()<required||cur.size()<required||!coverage(ref,w,h)||!coverage(cur,w,h))return best;
    // Spatial bins bound nearest-neighbour cost for each candidate transform.
    auto key=[](int x,int y){return (std::uint64_t(std::uint32_t(x))<<32)|std::uint32_t(y);};
    std::unordered_map<std::uint64_t,std::vector<int>> grid;for(int i=0;i<int(cur.size());++i)grid[key(int(std::floor(cur[i].x/4)),int(std::floor(cur[i].y/4)))].push_back(i);
    auto matches=[&](Transform t,double radius){std::vector<int> owner(cur.size(),-1);std::vector<double> distances(cur.size(),radius*radius);
        for(int i=0;i<int(ref.size());++i){auto p=t.apply(ref[i]);int bx=int(std::floor(p.x/4)),by=int(std::floor(p.y/4)),index=-1;double bestD=radius*radius;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){auto it=grid.find(key(bx+dx,by+dy));if(it==grid.end())continue;for(int j:it->second){double d=std::pow(p.x-cur[j].x,2)+std::pow(p.y-cur[j].y,2);if(d<bestD){index=j;bestD=d;}}}if(index>=0&&bestD<distances[index]){owner[index]=i;distances[index]=bestD;}}
        std::vector<std::pair<Point,Point>> pairs;for(int j=0;j<int(cur.size());++j)if(owner[j]>=0)pairs.emplace_back(ref[owner[j]],cur[j]);return pairs;};
    auto assess=[&](Transform t){
        if(!plausible(t))return;auto pairs=matches(t,4);if(pairs.size()<required)return;
        for(int k=0;k<3;++k){t=fitSimilarity(pairs);if(!plausible(t))return;pairs=matches(t,k==0?3:2);if(pairs.size()<required)return;}
        double err=0;std::vector<Point> a,b;for(auto [r,s]:pairs){err+=std::pow(dist(t.apply(r),s),2);a.push_back(r);b.push_back(s);}double rms=std::sqrt(err/pairs.size()),ov=overlap(t,w,h);
        if(rms>o.maximumRms||ov<.25||!coverage(a,w,h)||!coverage(b,w,h))return;
        // Reject likely accidental correspondences using a conservative local-density test.
        double expected=0;int observed=0;for(auto r:ref){auto p=t.apply(r);int nearby=0;for(auto q:cur)if(dist(p,q)<16)++nearby;expected+=1-std::exp(-nearby/256.);}
        for(auto [r,s]:pairs)if(dist(t.apply(r),s)<=1)++observed;
        if(observed<=expected|| (expected>0&&observed-expected-observed*std::log(observed/expected)>std::log(1e-5/(3*32768))))return;
        if(!best.accepted||pairs.size()>best.matches||(pairs.size()==best.matches&&rms<best.rms))best={true,t,int(pairs.size()),rms,ov,"Aligned and integrated"};
    };
    assess({});if(best.accepted)return best;
    // Canonical triangles provide translation/rotation-invariant hypotheses.
    struct Tri {std::array<Point,3> p;double r1,r2;};
    auto triangles=[](const std::vector<Point>& all){std::vector<Point> pts;std::array<int,16> cells{};double maxx=1,maxy=1;for(auto p:all){maxx=std::max(maxx,p.x);maxy=std::max(maxy,p.y);}for(auto p:all){auto cell=std::min(3,int(p.y*4/maxy))*4+std::min(3,int(p.x*4/maxx));if(cells[cell]++<3)pts.push_back(p);if(pts.size()==40)break;}
        std::vector<Tri> ts;for(int a=0;a<int(pts.size());++a)for(int b=a+1;b<int(pts.size());++b)for(int c=b+1;c<int(pts.size());++c){std::array<Point,3> p{pts[a],pts[b],pts[c]};std::array<std::pair<double,int>,3>d{{{dist(p[1],p[2]),0},{dist(p[0],p[2]),1},{dist(p[0],p[1]),2}}};std::sort(d.begin(),d.end());if(d[2].first<12||d[0].first/d[2].first<.1)continue;ts.push_back({{p[d[0].second],p[d[1].second],p[d[2].second]},d[0].first/d[2].first,d[1].first/d[2].first});}std::sort(ts.begin(),ts.end(),[](auto a,auto b){return a.r1<b.r1;});return ts;};
    auto rt=triangles(ref),ct=triangles(cur);int hypotheses=0;
    for(const auto& a:rt){auto it=std::lower_bound(ct.begin(),ct.end(),a.r1-.002,[](const auto& t,double v){return t.r1<v;});for(;it!=ct.end()&&it->r1<a.r1+.002;++it){if(std::abs(a.r2-it->r2)>.002)continue;auto t=fitSimilarity({{a.p[0],it->p[0]},{a.p[1],it->p[1]},{a.p[2],it->p[2]}});if(!plausible(t))continue;assess(t);if(best.accepted&&best.matches>=required)return best;if(++hypotheses>=4096)break;}if(hypotheses>=4096)break;}
    if(!best.accepted)best.reason="Alignment rejected: insufficient unique matches, geometry or residual quality";return best;
}
void Accumulator::add(const LinearImage& image,const Transform& t,const StackOptions& o){
    validImage(image);if(!plausible(t))throw std::invalid_argument("Invalid integration transform");
    if(!frames){previewGains=image.previewGains;width=image.width;height=image.height;const auto n=std::size_t(width)*height;if(n*80>std::size_t(o.memoryLimitMb)*1024*1024)throw std::runtime_error("RAW exceeds configured stack memory budget");mean_.resize(n*3);m2_.resize(n*3);count.resize(n);}
    if(image.width!=width||image.height!=height)throw std::invalid_argument("RAW dimensions changed");
    for(int y=0;y<height;++y)for(int x=0;x<width;++x){auto q=t.apply({double(x),double(y)});if(q.x<0||q.y<0||q.x>width-1||q.y>height-1)continue;int x0=int(q.x),y0=int(q.y),x1=std::min(x0+1,width-1),y1=std::min(y0+1,height-1);double fx=q.x-x0,fy=q.y-y0;auto p=std::size_t(y)*width+x;std::array<double,3> rgb{};bool reject=false;
        for(int c=0;c<3;++c){auto at=[&](int xx,int yy){return image.rgb[(std::size_t(yy)*width+xx)*3+c];};double v=(at(x0,y0)*(1-fx)+at(x1,y0)*fx)*(1-fy)+(at(x0,y1)*(1-fx)+at(x1,y1)*fx)*fy;if(!std::isfinite(v))throw std::invalid_argument("Non-finite RAW sample");rgb[c]=v;
            if(o.rejection&&count[p]>=5){double noise=std::sqrt(std::max(1./(65535.*65535.),double(m2_[p*3+c])/(count[p]-1)));if(std::abs(v-mean_[p*3+c])>o.sigma*noise)reject=true;}}
        if(reject){++clipped;continue;}++count[p];for(int c=0;c<3;++c){auto k=p*3+c;double delta=rgb[c]-mean_[k];mean_[k]+=delta/count[p];m2_[k]+=float(delta*(rgb[c]-mean_[k]));}
    }++frames;
}
float Accumulator::value(std::size_t pixel,int channel)const{return count.at(pixel)?float(mean_.at(pixel*3+channel)):0;}
StackPreview render(const Accumulator& a,StackDisplay d,std::uint64_t revision){
    validateStackDisplay(d);StackPreview p;p.display=d;p.revision=revision;if(!a.frames)return p;
    double scale=std::min(1.,1280./std::max(a.width,a.height));p.width=std::max(1,int(a.width*scale));p.height=std::max(1,int(a.height*scale));p.rgba.resize(std::size_t(p.width)*p.height*4);
    for(int y=0;y<p.height;++y)for(int x=0;x<p.width;++x){int x0=x*a.width/p.width,x1=(x+1)*a.width/p.width,y0=y*a.height/p.height,y1=(y+1)*a.height/p.height;double rgb[3]{},n=0;
        for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx){auto i=std::size_t(yy)*a.width+xx;if(!a.count[i])continue;for(int c=0;c<3;++c)rgb[c]+=a.value(i,c);++n;}auto out=(std::size_t(y)*p.width+x)*4;
        double lum=0;for(int c=0;c<3;++c){double linear=n?rgb[c]/n*a.previewGains[c]:0;lum+=linear*(c==0?.2126:c==1?.7152:.0722);double v=std::clamp((linear-d.low)/(d.high-d.low),0.,1.);double mapped=v==0?0:v==1?1:((d.mid-1)*v)/((2*d.mid-1)*v-d.mid);p.rgba[out+c]=std::uint8_t(std::clamp(mapped*255.,0.,255.));}p.rgba[out+3]=255;++p.histogram[std::clamp(int(lum*255),0,255)];
    }for(auto& bin:p.histogram)bin=std::log1p(bin);return p;
}
}
