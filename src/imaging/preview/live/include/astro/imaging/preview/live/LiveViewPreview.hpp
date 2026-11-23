#pragma once
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
namespace astro {
PreviewPixels prepareLiveViewPreview(PreviewPixels image, const LiveFrame& frame);
}
