#pragma once
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <astro/core/focus/model/StarFocusData.hpp>
#include <span>
namespace astro {
StarField measureStars(const PreviewPixels&, double minimumSnr = 6, std::span<const StarSample> tracked = {},
                       double matchRadius = 8);
// Identical native-resolution photometry, without AF travel signatures or
// multiscale acquisition searches which cannot contribute peaking markers.
StarField measurePeakingStars(const PreviewPixels&, double minimumSnr = 5);
}  // namespace astro
