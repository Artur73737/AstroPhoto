#pragma once
#include <chrono>
#include <algorithm>
#include <cstdint>
namespace astro {
struct StreamExposure {
    bool active{};
    double seconds{};
    std::chrono::steady_clock::time_point started{};
    std::uint64_t frame{};
    // ASI video exposes no hardware start/progress event. This is elapsed host
    // time since stream start or the latest native frame, including readout.
    double elapsed(std::chrono::steady_clock::time_point now) const {
        return active ? std::max(0., std::chrono::duration<double>(now - started).count()) : 0.;
    }
    double fraction(std::chrono::steady_clock::time_point now) const {
        return active && seconds > 0 ? std::clamp(elapsed(now) / seconds, 0., 1.) : 0.;
    }
};
}  // namespace astro
