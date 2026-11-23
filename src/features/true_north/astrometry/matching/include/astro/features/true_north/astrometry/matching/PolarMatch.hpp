#pragma once
#include <astro/stacking/model/StackMath.hpp>
#include <functional>
namespace astro::north::detail {
struct Match {
    stacking::Transform transform;
    int stars{};
    double rms{};
};
Match recover(const std::vector<stacking::Point>&, const std::vector<stacking::Point>&, int, int, int,
              const std::function<bool()>&, const std::function<void(const std::string&)>&);
}  // namespace astro::north::detail
