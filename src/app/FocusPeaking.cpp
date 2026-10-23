#include "FocusPeaking.hpp"
#include <cmath>
#include <limits>

namespace astro {
PreviewPixels starFocusPeaking(const PreviewPixels& source,float threshold);
PreviewPixels focusPeaking(const PreviewPixels& source,float threshold,bool stars) {
    const auto w=source.width,h=source.height;
    if(w<=0||h<=0||static_cast<std::size_t>(w)>std::numeric_limits<std::size_t>::max()/static_cast<std::size_t>(h)/4||source.rgba.size()!=static_cast<std::size_t>(w)*h*4)
        throw std::invalid_argument("Invalid peaking image dimensions");
    if(!std::isfinite(threshold)||threshold<10||threshold>100)throw std::invalid_argument("Invalid peaking threshold");
    if(stars)return starFocusPeaking(source,threshold);
    auto result=source;if(w<5||h<5)return result;
    const auto count=static_cast<std::size_t>(w)*h;
    std::vector<float> luma(count),smooth(count),gradient(count);
    for(std::size_t i=0;i<count;++i)luma[i]=0.2126f*source.rgba[i*4]+0.7152f*source.rgba[i*4+1]+0.0722f*source.rgba[i*4+2];
    // A small Gaussian reduces single-pixel noise before measuring edge steepness.
    for(int y=1;y<h-1;++y)for(int x=1;x<w-1;++x) {
        const auto i=static_cast<std::size_t>(y)*w+x;
        smooth[i]=(luma[i-w-1]+2*luma[i-w]+luma[i-w+1]+2*luma[i-1]+4*luma[i]+2*luma[i+1]+luma[i+w-1]+2*luma[i+w]+luma[i+w+1])/16;
    }
    auto derivatives=[&](std::size_t i) {
        return std::pair{
            (smooth[i-w+1]+2*smooth[i+1]+smooth[i+w+1]-smooth[i-w-1]-2*smooth[i-1]-smooth[i+w-1])/4,
            (smooth[i+w-1]+2*smooth[i+w]+smooth[i+w+1]-smooth[i-w-1]-2*smooth[i-w]-smooth[i-w+1])/4};
    };
    for(int y=2;y<h-2;++y)for(int x=2;x<w-2;++x) {
        const auto i=static_cast<std::size_t>(y)*w+x;const auto [gx,gy]=derivatives(i);gradient[i]=std::hypot(gx,gy);
    }
    // Keep local maxima across the edge, producing fine contours instead of filled areas.
    for(int y=3;y<h-3;++y)for(int x=3;x<w-3;++x) {
        const auto i=static_cast<std::size_t>(y)*w+x;if(gradient[i]<threshold)continue;
        const auto [gx,gy]=derivatives(i);int dx{},dy{};
        if(std::abs(gy)<std::abs(gx)*0.4142f)dx=1;
        else if(std::abs(gx)<std::abs(gy)*0.4142f)dy=1;
        else {dx=1;dy=gx*gy>=0?1:-1;}
        const auto before=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i)-dy*w-dx);
        const auto after=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i)+dy*w+dx);
        if(gradient[i]>=gradient[before]&&gradient[i]>gradient[after]) {
            result.rgba[i*4]=255;result.rgba[i*4+1]=0;result.rgba[i*4+2]=255;
        }
    }
    return result;
}
}
