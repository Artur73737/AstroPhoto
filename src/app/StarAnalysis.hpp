#pragma once
#include "PhotoPreview.hpp"
#include <astro/StarFocusData.hpp>
namespace astro {
StarField measureStars(const PreviewPixels&,double minimumSnr=6);
}
