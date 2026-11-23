#pragma once
#include <astro/core/domain/Core.hpp>
namespace astro::edsdk {
// Decode Canon protocol values without adding choices unsupported by the camera.
std::string propertyLabel(Property property, std::uint32_t value);
}  // namespace astro::edsdk
