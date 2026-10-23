#include "ImageAnalysis.hpp"
#include <algorithm>
#include <cmath>
namespace astro {
namespace {
void validatePixels(const PreviewPixels& p) {
    if(p.width<=0||p.height<=0||p.rgba.size()!=static_cast<std::size_t>(p.width)*p.height*4)
        throw std::invalid_argument("Invalid analysis image");
}
float luminance(const PreviewPixels& p,int x,int y) {
    const auto i=(static_cast<std::size_t>(y)*p.width+x)*4;
    return .2126f*p.rgba[i]+.7152f*p.rgba[i+1]+.0722f*p.rgba[i+2];
}
}
Histogram imageHistogram(const PreviewPixels& p) {
    validatePixels(p);Histogram h;h.pixels=static_cast<std::uint64_t>(p.width)*p.height;
    for(std::size_t i=0;i<p.rgba.size();i+=4) {
        for(int c=0;c<3;++c)++h.bins[c][p.rgba[i+c]];
        const auto l=static_cast<unsigned>(std::lround(.2126*p.rgba[i]+.7152*p.rgba[i+1]+.0722*p.rgba[i+2]));
        ++h.bins[3][std::min(l,255u)];
    }return h;
}
FocusReading measureFocus(const PreviewPixels& p,FocusTarget target,const PreviewPixels* peaking) {
    validatePixels(p);FocusReading r;
    if(!target.enabled||p.width<8||p.height<8)return r;
    if(!std::isfinite(target.u)||!std::isfinite(target.v))throw std::invalid_argument("Invalid focus point");
    r.width=std::min(64,p.width-2);r.height=std::min(64,p.height-2);
    r.x=std::clamp(static_cast<int>(std::clamp(target.u,0.f,1.f)*p.width)-r.width/2,1,p.width-r.width-1);
    r.y=std::clamp(static_cast<int>(std::clamp(target.v,0.f,1.f)*p.height)-r.height/2,1,p.height-r.height-1);
    const bool overlay=peaking&&peaking->width==p.width&&peaking->height==p.height&&peaking->rgba.size()==p.rgba.size();
    double sum{},sum2{},gradient{};int clipped{},marked{};
    // Central differences after a small perpendicular smoothing suppress isolated sensor noise.
    for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x) {
        const double l=luminance(p,x,y);sum+=l;sum2+=l*l;if(l>=250)++clipped;
        const double gx=(luminance(p,x+1,y-1)+2*luminance(p,x+1,y)+luminance(p,x+1,y+1)-luminance(p,x-1,y-1)-2*luminance(p,x-1,y)-luminance(p,x-1,y+1))/8;
        const double gy=(luminance(p,x-1,y+1)+2*luminance(p,x,y+1)+luminance(p,x+1,y+1)-luminance(p,x-1,y-1)-2*luminance(p,x,y-1)-luminance(p,x+1,y-1))/8;
        gradient+=gx*gx+gy*gy;
        const auto i=(static_cast<std::size_t>(y)*p.width+x)*4;
        if(overlay&&peaking->rgba[i]==255&&peaking->rgba[i+1]==0&&peaking->rgba[i+2]==255&&
           (p.rgba[i]!=255||p.rgba[i+1]!=0||p.rgba[i+2]!=255))++marked;
    }
    const double n=r.width*r.height,mean=sum/n,variance=std::max(0.0,sum2/n-mean*mean);
    r.valid=variance>4&&clipped/n<.05&&gradient/n>.1;
    r.score=gradient/n/std::max(variance,1.0)*100;
    r.peakingMeasured=overlay;r.peakingPixels=marked;r.peakingFraction=marked/n;return r;
}
void FocusTracker::reset(){*this=FocusTracker{};}
FocusReading FocusTracker::update(FocusReading r,std::uint64_t frame,std::uint64_t context) {
    if(context!=context_){reset();context_=context;}
    if(frame==frame_&&samples_){
        // Changing the peaking mode/threshold reprocesses the same source frame.
        // Refresh coverage immediately without treating it as a new focus sample.
        reading_.peakingMeasured=r.peakingMeasured;reading_.peakingPixels=r.peakingPixels;reading_.peakingFraction=r.peakingFraction;return reading_;
    }
    frame_=frame;
    if(!r.valid){smooth_=minimum_=best_=0;samples_=stable_=declining_=0;bracketed_=false;reading_=r;return reading_;}
    smooth_=samples_?smooth_*.65+r.score*.35:r.score;++samples_;
    minimum_=samples_==1?smooth_:std::min(minimum_,smooth_);
    if(smooth_>best_*1.03){bracketed_=false;declining_=0;}
    best_=std::max(best_,smooth_);
    // A stationary image must never declare success. Require a sweep through a peak,
    // several lower readings, then a stable return to within 5% of that measured peak.
    if(samples_>=8&&minimum_<best_*.8&&smooth_<best_*.85)++declining_;else declining_=0;
    if(declining_>=3)bracketed_=true;
    if(bracketed_&&smooth_>=best_*.95)++stable_;else stable_=0;
    r.score=smooth_;r.best=best_;r.samples=samples_;r.bracketed=bracketed_;r.nearBest=stable_>=3;
    reading_=r;return reading_;
}
}
