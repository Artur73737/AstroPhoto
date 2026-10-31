#include "../include/Module.hpp"
#include <array>
namespace astro::camera_models {
CameraModuleInfo CanonEos1100d::info() const{return {"canon_eos_1100d","Canon EOS 1100D",{1,5,10},{1,2,3},"CR2"};}
std::span<const std::string_view> CanonEos1100d::modelNames() const{
    // Only the physically validated model; regional aliases are not guessed.
    static constexpr std::array<std::string_view,1> names{"Canon EOS 1100D"};return names;
}
std::span<const Property> CanonEos1100d::properties() const{
    static constexpr std::array values{Property::Iso,Property::Aperture,Property::Shutter,Property::ExposureMode,Property::WhiteBalance,Property::ColorTemperature,Property::PictureStyle,Property::ImageQuality};return values;
}
const CameraModule& canonEos1100d(){static const CanonEos1100d module;return module;}
}
