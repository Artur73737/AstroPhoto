#pragma once
#include <astro/LiveStack.hpp>
#include <map>
#include <functional>
namespace astro::stacking {
struct PlateSolution {bool solved{};std::string detail;std::map<std::string,double> wcs;};
PlateSolution parsePlateSolution(const std::string&);
PlateSolution solvePlate(const std::filesystem::path& fits,const StackOptions&,const std::function<bool()>& cancelled={});
}
