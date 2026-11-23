#pragma once
#include <astro/stacking/astrometry/PlateSolve.hpp>
namespace astro::north {
// Fresh source extraction and catalogue matching. No image-specific coordinates or cached WCS.
stacking::PlateSolution solvePolarField(const std::filesystem::path&, const StackOptions&,
                                        const std::function<bool()>& cancelled = {},
                                        const std::function<void(const std::string&)>& progress = {});
}  // namespace astro::north
