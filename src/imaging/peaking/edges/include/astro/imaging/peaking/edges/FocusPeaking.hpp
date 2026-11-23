#pragma once
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
namespace astro {
// Returns a display copy. Source pixels and downloaded photos stay untouched.
PreviewPixels focusPeaking(const PreviewPixels& source, float threshold, bool stars = false);
}  // namespace astro
