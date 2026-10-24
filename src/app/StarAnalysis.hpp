#pragma once
#include "PhotoPreview.hpp"
#include <astro/StarFocusData.hpp>
#include <span>
namespace astro {
StarField measureStars(const PreviewPixels&,double minimumSnr=6,std::span<const StarSample> tracked={},double matchRadius=8);
}
