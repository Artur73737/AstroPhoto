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
    r.width=std::min(64,p.width-4);r.height=std::min(64,p.height-4);
    r.x=std::clamp(static_cast<int>(std::clamp(target.u,0.f,1.f)*p.width)-r.width/2,2,p.width-r.width-2);
    r.y=std::clamp(static_cast<int>(std::clamp(target.v,0.f,1.f)*p.height)-r.height/2,2,p.height-r.height-2);
    const bool overlay=peaking&&peaking->width==p.width&&peaking->height==p.height&&peaking->rgba.size()==p.rgba.size();
    // Estimate sensor noise from the robust median of a diagonal high-pass response.
    // This rejects isolated pixels and does not treat a straight edge as noise.
    std::vector<float> residuals;residuals.reserve(static_cast<std::size_t>(r.width)*r.height);
    for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)
        residuals.push_back(std::abs(luminance(p,x,y)-luminance(p,x+1,y)-luminance(p,x,y+1)+luminance(p,x+1,y+1))*.5f);
    auto middle=residuals.begin()+residuals.size()/2;std::nth_element(residuals.begin(),middle,residuals.end());
    const double noise=*middle/.67448975;
    // Smooth before measuring derivatives. The noise gate is deliberately conservative:
    // a noisy flat region must not produce a new focus record.
    auto smooth=[&](int x,int y){return (luminance(p,x-1,y-1)+2*luminance(p,x,y-1)+luminance(p,x+1,y-1)+2*luminance(p,x-1,y)+4*luminance(p,x,y)+2*luminance(p,x+1,y)+luminance(p,x-1,y+1)+2*luminance(p,x,y+1)+luminance(p,x+1,y+1))/16;};
    const double floor=std::max(.5,noise*.9),noiseEnergy=floor*floor;
    double gradient{};int clipped{},marked{},detailPixels{};
    for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x) {
        if(luminance(p,x,y)>=250)++clipped;
        const double gx=(smooth(x+1,y)-smooth(x-1,y))/2,gy=(smooth(x,y+1)-smooth(x,y-1))/2;
        const double energy=gx*gx+gy*gy;
        if(energy>noiseEnergy){gradient+=energy-noiseEnergy;++detailPixels;}
        const auto i=(static_cast<std::size_t>(y)*p.width+x)*4;
        if(overlay&&peaking->rgba[i]==255&&peaking->rgba[i+1]==0&&peaking->rgba[i+2]==255&&
           (p.rgba[i]!=255||p.rgba[i+1]!=0||p.rgba[i+2]!=255))++marked;
    }
    const double n=r.width*r.height;
    r.valid=detailPixels>=6&&clipped/n<.05&&gradient/n>.02;
    // Absolute, noise-gated detail energy. Never divide by local contrast/variance:
    // that normalization can promote defocused sensor noise above real detail.
    r.score=r.valid?gradient/n:0;
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
    if(!r.valid||!std::isfinite(r.score)||r.score<=0){r.valid=false;r.score=0;recent_.clear();stable_=declining_=0;bracketed_=false;r.best=best_;r.samples=samples_;reading_=r;return reading_;}
    ++samples_;recent_.push_back(r.score);if(recent_.size()>3)recent_.pop_front();
    auto sorted=std::vector<double>(recent_.begin(),recent_.end());std::sort(sorted.begin(),sorted.end());
    const double current=sorted[sorted.size()/2];
    minimum_=samples_==1?current:std::min(minimum_,current);
    const bool stableWindow=sorted.size()==3&&sorted.front()>=sorted.back()*.9;
    // A record needs three consistent frames. A single spike never becomes "best".
    if(stableWindow&&(best_==0||sorted.front()>best_*1.03)){best_=sorted.front();bracketed_=false;declining_=0;}
    // A stationary image must never declare success. Require a sweep through a peak,
    // several lower readings, then a stable return to within 5% of that measured peak.
    if(samples_>=8&&best_>0&&minimum_<best_*.8&&current<best_*.85)++declining_;else declining_=0;
    if(declining_>=3)bracketed_=true;
    if(bracketed_&&stableWindow&&current>=best_*.95&&current<=best_*1.05)++stable_;else stable_=0;
    r.score=current;r.best=best_;r.samples=samples_;r.bracketed=bracketed_;r.nearBest=stable_>=3;
    reading_=r;return reading_;
}
}
