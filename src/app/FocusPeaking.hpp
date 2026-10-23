#pragma once
#include "PhotoPreview.hpp"
namespace astro {
// Returns a display copy. Source pixels and downloaded photos stay untouched.
PreviewPixels focusPeaking(const PreviewPixels& source,float threshold);
}
