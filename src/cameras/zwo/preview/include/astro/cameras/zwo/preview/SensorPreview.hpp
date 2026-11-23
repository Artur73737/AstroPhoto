#pragma once
#include <astro/cameras/zwo/sdk/ZwoCamera.hpp>
namespace astro::zwo {
// Display-only RAW16 demosaic/stretch. Sensor/FITS samples remain unchanged.
std::vector<std::uint8_t> previewJpeg(const Frame&, const Device&);
}
