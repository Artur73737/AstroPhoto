#include "../../include/Module.hpp"
namespace astro::camera_models {
CapturePolicy CanonEos1100d::capturePolicy(std::uint32_t quality) const{
    // Canon protocol: Tv Bulb=0x0c; TakePicture=0, BulbStart=2, BulbEnd=3.
    // Secondary image slot 0xff0f means there is only one transferred file.
    return {0x0c,0,2,3,(quality&0xffff)==0xff0f?1:2};
}
}
