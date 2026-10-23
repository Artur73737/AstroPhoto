#pragma once
#include <array>
#include <cstdint>
namespace astro {
struct Histogram {
    // Red, green, blue and display luminance, measured before any overlays.
    std::array<std::array<std::uint64_t,256>,4> bins{};
    std::uint64_t pixels{};
};
struct FocusTarget {bool enabled{};float u{0.5f},v{0.5f};std::uint64_t revision{};};
struct FocusReading {
    bool valid{},bracketed{},nearBest{};
    double score{},best{},peakingFraction{};
    int samples{},x{},y{},width{},height{};
};
}
