#include "../../include/Module.hpp"
namespace astro::camera_models {
LensFocusState CanonEos1100d::lensState(std::uint32_t lens,std::uint32_t af) const{
    if(lens==0)return LensFocusState::NoLens;
    if(lens!=1)return LensFocusState::Unknown;
    if(af==3)return LensFocusState::ManualFocus;
    return af<=2?LensFocusState::Ready:LensFocusState::Unknown;
}
bool CanonEos1100d::manualExposureForFocus(std::uint32_t ae,std::uint32_t iso,std::uint32_t tv) const{
    return ae==3&&iso!=0&&iso!=0xffffffff&&tv>0x0c&&tv!=0xffffffff;
}
std::uint32_t CanonEos1100d::focusDrive(int step) const{
    switch(step){case -3:return 3;case -2:return 2;case -1:return 1;case 1:return 0x8001;case 2:return 0x8002;case 3:return 0x8003;default:throw std::invalid_argument("Invalid focus drive step");}
}
}
