#include "LiveViewPreview.hpp"
#include <algorithm>
#include <cmath>
namespace astro {
PreviewPixels prepareLiveViewPreview(const PreviewPixels& image,const LiveFrame& frame) {
    if(frame.zoom!=10)return image;
    const auto& rect=frame.zoomRect;
    if(image.width<2||image.height<2||image.rgba.size()!=static_cast<std::size_t>(image.width)*image.height*4||rect.width<=0||rect.height<=0||frame.coordinateWidth<=0||frame.coordinateHeight<=0)
        throw std::runtime_error("Invalid Canon 10x Live View geometry");
    // EDSDK 13.19 reference 5.2.59: 10x returns the same 5x JPEG.
    // The 10x zoom rectangle occupies half of that image in each dimension.
    // Its origin may be off-center: map it relative to Evf_ImagePosition.
    const double u=std::clamp((static_cast<double>(rect.x)-frame.imageX)/(2.0*rect.width),0.0,0.5);
    const double v=std::clamp((static_cast<double>(rect.y)-frame.imageY)/(2.0*rect.height),0.0,0.5);
    const int width=image.width/2,height=image.height/2;
    const int left=std::clamp(static_cast<int>(std::lround(u*image.width)),0,image.width-width);
    const int top=std::clamp(static_cast<int>(std::lround(v*image.height)),0,image.height-height);
    PreviewPixels result{width,height,std::vector<std::uint8_t>(static_cast<std::size_t>(width)*height*4),"Canon 10x (5x USB frame + 2x crop)"};
    for(int y=0;y<height;++y)std::copy_n(image.rgba.data()+(static_cast<std::size_t>(top+y)*image.width+left)*4,static_cast<std::size_t>(width)*4,result.rgba.data()+static_cast<std::size_t>(y)*width*4);
    return result;
}
}
