#pragma once
#include <astro/stacking/model/StackMath.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::stacking::detail {

inline float median(std::vector<float> v) {
    if (v.empty()) return 0;
    auto m = v.begin() + v.size() / 2;
    std::nth_element(v.begin(), m, v.end());
    return v.size() % 2 ? *m : (*m + *std::max_element(v.begin(), m)) * .5f;
}
inline void validImage(const LinearImage& i) {
    if (i.width < 8 || i.height < 8 || i.rgb.size() != static_cast<std::size_t>(i.width) * i.height * 3)
        throw std::invalid_argument("Invalid linear image dimensions");
    if (std::any_of(i.rgb.begin(), i.rgb.end(), [](float v) { return !std::isfinite(v); }))
        throw std::invalid_argument("Non-finite RAW pixel");
}
inline double dist(Point a, Point b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

}
