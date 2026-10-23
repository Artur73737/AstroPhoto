#include "FocusPeaking.hpp"
#include "StarAnalysis.hpp"
#include <algorithm>
#include <cmath>
namespace astro {
PreviewPixels starFocusPeaking(const PreviewPixels& source,float threshold) {
    auto result=source;
    const auto field=measureStars(source,5);
    for(const auto& star:field.stars){
        // AF retains wider stars to measure defocus; peaking marks only compact ones.
        if(star.hfd>4.7||star.peak-star.background<=threshold*.6)continue;
        const int x=static_cast<int>(std::lround(star.x)),y=static_cast<int>(std::lround(star.y));
        const int radius=std::clamp(static_cast<int>(std::ceil(star.hfd*.85)),2,5);
        for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx){
            if(std::abs(std::hypot(static_cast<float>(dx),static_cast<float>(dy))-radius)>.6f)continue;
            if(x+dx<0||x+dx>=source.width||y+dy<0||y+dy>=source.height)continue;
            const auto i=(static_cast<std::size_t>(y+dy)*source.width+x+dx)*4;
            result.rgba[i]=255;result.rgba[i+1]=0;result.rgba[i+2]=255;
        }
    }
    return result;
}
}
