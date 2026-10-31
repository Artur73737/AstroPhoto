#include "LiveViewPreview.hpp"
#include <algorithm>
#include <cmath>
namespace astro {
PreviewPixels prepareLiveViewPreview(PreviewPixels image,const LiveFrame& frame) {
    const auto& crop=frame.previewRegion;
    if(crop.x==0&&crop.y==0&&crop.width==1&&crop.height==1)return image;
    if(!std::isfinite(crop.x)||!std::isfinite(crop.y)||!std::isfinite(crop.width)||!std::isfinite(crop.height)||
        crop.x<0||crop.y<0||crop.width<=0||crop.height<=0||crop.x+crop.width>1||crop.y+crop.height>1||
        image.width<2||image.height<2||image.rgba.size()!=static_cast<std::size_t>(image.width)*image.height*4)
        throw std::runtime_error("Invalid Live View preview region");
    const int width=static_cast<int>(image.width*crop.width),height=static_cast<int>(image.height*crop.height);
    if(width<1||height<1)throw std::runtime_error("Empty Live View preview region");
    const int left=std::clamp(static_cast<int>(std::lround(crop.x*image.width)),0,image.width-width);
    const int top=std::clamp(static_cast<int>(std::lround(crop.y*image.height)),0,image.height-height);
    PreviewPixels result{width,height,std::vector<std::uint8_t>(static_cast<std::size_t>(width)*height*4),crop.description};
    for(int y=0;y<height;++y)std::copy_n(image.rgba.data()+(static_cast<std::size_t>(top+y)*image.width+left)*4,static_cast<std::size_t>(width)*4,result.rgba.data()+static_cast<std::size_t>(y)*width*4);
    return result;
}
}
