#pragma once
#include <astro/stacking/model/StackMath.hpp>
#include <functional>
namespace astro::north::detail {
std::vector<stacking::Point> catalog(const std::filesystem::path&);
}
