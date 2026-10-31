#include "../../include/Module.hpp"
#include <algorithm>
namespace astro::camera_models {
PreviewRegion CanonEos1100d::previewRegion(const LiveFrame& frame) const{
    if(frame.zoom!=10)return {};
    const auto& rect=frame.zoomRect;
    if(rect.width<=0||rect.height<=0||frame.coordinateWidth<=0||frame.coordinateHeight<=0)
        throw CameraError("Invalid Canon 1100D 10x Live View geometry",true);
    // EDSDK 13.19 section 5.2.59: 10x still transfers a 5x JPEG. Preserve
    // the validated off-centre crop relative to Evf_ImagePosition.
    return {std::clamp((static_cast<double>(rect.x)-frame.imageX)/(2.0*rect.width),0.0,0.5),
        std::clamp((static_cast<double>(rect.y)-frame.imageY)/(2.0*rect.height),0.0,0.5),
        .5,.5,"Canon 1100D 10x (5x USB frame + 2x crop)"};
}
}
