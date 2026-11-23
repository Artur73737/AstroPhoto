#pragma once
#include <astro/core/stacking/model/LiveStack.hpp>
#include <map>
#include <functional>
namespace astro::stacking {
struct PlateSolution {
    bool solved{};
    std::string detail;
    std::map<std::string, double> wcs;
    int width{},height{};
};
PlateSolution parsePlateSolution(const std::string&);
PlateSolution solvePlate(const std::filesystem::path& fits, const StackOptions&,
                         const std::function<bool()>& cancelled = {},
                         const std::function<void(const std::string&)>& progress = {});
}  // namespace astro::stacking
