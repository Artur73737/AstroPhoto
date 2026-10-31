#pragma once
#include "PhotoPreview.hpp"
#include <string>
namespace astro::remote {
// Bounded JPEG previews; never transmit full-resolution camera files.
std::string previewJpeg(PreviewPixels pixels);
std::string cameraJpeg(const std::string& jpeg,const LiveFrame& frame={});
}
