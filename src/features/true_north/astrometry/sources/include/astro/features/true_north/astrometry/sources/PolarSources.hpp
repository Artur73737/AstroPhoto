#pragma once
#include <astro/stacking/model/StackMath.hpp>
#include <functional>
namespace astro::north::detail {
std::vector<stacking::Point> extract(const std::filesystem::path&, const StackOptions&, const std::function<bool()>&,
                                     const std::function<void(const std::string&)>&);
}
