#include "FocusPeaking.hpp"
#include <algorithm>
#include <cmath>
namespace astro {
PreviewPixels starFocusPeaking(const PreviewPixels& source,float threshold) {
    auto result=source;const int w=source.width,h=source.height;if(w<19||h<19)return result;
    const auto count=static_cast<std::size_t>(w)*h,stride=static_cast<std::size_t>(w)+1;
    std::vector<float> luma(count),smooth(count);
    std::vector<double> integral(stride*(h+1)),squared(integral.size());
    for(int y=0;y<h;++y) {
        double sum{},square{};
        for(int x=0;x<w;++x) {
            const auto i=static_cast<std::size_t>(y)*w+x;
            const float light=0.2126f*source.rgba[i*4]+0.7152f*source.rgba[i*4+1]+0.0722f*source.rgba[i*4+2];luma[i]=light;
            sum+=light;square+=light*light;
            integral[(y+1)*stride+x+1]=integral[y*stride+x+1]+sum;
            squared[(y+1)*stride+x+1]=squared[y*stride+x+1]+square;
        }
    }
    for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x) {
        const auto i=static_cast<std::size_t>(y)*w+x;
        smooth[i]=(luma[i-w-1]+2*luma[i-w]+luma[i-w+1]+2*luma[i-1]+4*luma[i]+2*luma[i+1]+luma[i+w-1]+2*luma[i+w]+luma[i+w+1])/16;
    }
    auto box=[&](const auto& table,int x,int y,int r) {return table[(y+r+1)*stride+x+r+1]-table[(y-r)*stride+x+r+1]-table[(y+r+1)*stride+x-r]+table[(y-r)*stride+x-r];};
    for(int y=9;y<h-9;++y)for(int x=9;x<w-9;++x) {
        const auto i=static_cast<std::size_t>(y)*w+x;const float peak=smooth[i];
        bool maximum=true;
        for(int dy=-1;dy<=1&&maximum;++dy)for(int dx=-1;dx<=1;++dx) {
            if(!dx&&!dy)continue;const auto j=static_cast<std::size_t>(y+dy)*w+x+dx;
            if(smooth[j]>peak||(j<i&&smooth[j]==peak)){maximum=false;break;}
        }
        if(!maximum||luma[i]>=250)continue; // clipped stars cannot provide a reliable sharpness cue
        // Local sky level and noise from a 17x17 annulus, excluding the central 9x9 source.
        const double background=(box(integral,x,y,8)-box(integral,x,y,4))/208;
        const double variance=std::max(0.0,(box(squared,x,y,8)-box(squared,x,y,4))/208-background*background);
        const double noise=std::sqrt(variance),contrast=peak-background;
        if(contrast<std::max(static_cast<double>(threshold)*0.5,5*noise))continue;
        int support{},halfMax{};double mass{},sx{},sy{},sxx{},syy{},sxy{};
        const double rawContrast=luma[i]-background;
        for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx) {
            const double excess=luma[static_cast<std::size_t>(y+dy)*w+x+dx]-background;
            if(excess>rawContrast*0.2)++support;if(excess>rawContrast*0.5)++halfMax;
            const double weight=std::max(0.0,excess-2*noise);
            mass+=weight;sx+=dx*weight;sy+=dy*weight;sxx+=dx*dx*weight;syy+=dy*dy*weight;sxy+=dx*dy*weight;
        }
        if(support<4||halfMax>37||mass<=0)continue; // reject isolated hot pixels and broad blobs
        const double vx=sxx/mass-(sx/mass)*(sx/mass),vy=syy/mass-(sy/mass)*(sy/mass),cov=sxy/mass-(sx/mass)*(sy/mass);
        const double trace=vx+vy,delta=std::sqrt(std::max(0.0,(vx-vy)*(vx-vy)+4*cov*cov));
        const double minor=(trace-delta)/2,major=(trace+delta)/2;
        if(minor<=0.12||major>minor*2.5||trace>7.84)continue; // compact, approximately round sources
        const int radius=std::clamp(static_cast<int>(std::ceil(std::sqrt(trace)*1.8)),2,5);
        for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx) {
            if(std::abs(std::hypot(static_cast<float>(dx),static_cast<float>(dy))-radius)>0.6f)continue;
            const auto pixel=(static_cast<std::size_t>(y+dy)*w+x+dx)*4;
            result.rgba[pixel]=255;result.rgba[pixel+1]=0;result.rgba[pixel+2]=255;
        }
    }
    return result;
}
}
