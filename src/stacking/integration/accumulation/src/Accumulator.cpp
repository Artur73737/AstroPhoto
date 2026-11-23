#define NOMINMAX
#include <windows.h>
#include <astro/stacking/model/StackMath.hpp>
#include <astro/stacking/common/ImageMath.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <unordered_map>
namespace astro::stacking {
using detail::median;using detail::validImage;using detail::dist;
std::uint64_t stackMemoryBytes(int width, int height, double drizzleScale) {
    const auto target =
        std::uint64_t(std::ceil(width * drizzleScale)) * std::uint64_t(std::ceil(height * drizzleScale));
    // Per output pixel: running mean (3 x double) + M2 (3 x float) + count + weight = 48 B,
    // drizzle projection (RGB + weight float) = 16 B, preview/export scratch = 16 B, slack 8 B.
    // Per source pixel: decoded RGB float + demosaic/calibration scratch = 32 B.
    return target * 88 + std::uint64_t(width) * height * 32;
}
std::uint64_t stackMemoryBudgetBytes(int configuredMb) {
    const auto configured = std::uint64_t(configuredMb) * 1024 * 1024;
    // The configured value is a floor, not a hard cap: drizzle at 2x/3x on a 12 MP sensor
    // needs 5-10 GB, so allow up to 75% of physical RAM before refusing.
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status)) return configured;
    return std::max(configured, status.ullTotalPhys / 4 * 3);
}
void Accumulator::add(const LinearImage& input, const Transform& transform, const StackOptions& o) {
    validImage(input);
    if (!plausible(transform)) throw std::invalid_argument("Invalid integration transform");
    const int targetWidth = int(std::ceil(input.width * o.drizzleScale)),
              targetHeight = int(std::ceil(input.height * o.drizzleScale));
    if (stackMemoryBytes(input.width, input.height, o.drizzleScale) > stackMemoryBudgetBytes(o.memoryLimitMb))
        throw std::runtime_error("Stack / drizzle exceeds memory budget; lower scale or increase budget");
    if (frames && (sourceWidth_ != input.width || sourceHeight_ != input.height))
        throw std::invalid_argument("RAW dimensions changed");
    DrizzleFrame projected;
    if (o.drizzleScale > 1) projected = drizzle(input, transform, o.drizzleScale, o.drizzlePixfrac);
    const auto& image = o.drizzleScale > 1 ? projected.image : input;
    const auto t = o.drizzleScale > 1 ? Transform{} : transform;
    if (!frames) {
        pixelScale = o.drizzleScale;
        pixfrac = o.drizzlePixfrac;
        sourceWidth_ = input.width;
        sourceHeight_ = input.height;
        previewGains = input.previewGains;
        width = targetWidth;
        height = targetHeight;
        const auto n = std::size_t(width) * height;
        mean_.resize(n * 3);
        m2_.resize(n * 3);
        count.resize(n);
        weights.resize(n);
    }
    if (image.width != width || image.height != height) throw std::invalid_argument("Stack scale changed");
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            auto q = t.apply({double(x), double(y)});
            if (q.x < 0 || q.y < 0 || q.x > width - 1 || q.y > height - 1) continue;
            int x0 = int(q.x), y0 = int(q.y), x1 = std::min(x0 + 1, width - 1), y1 = std::min(y0 + 1, height - 1);
            double fx = q.x - x0, fy = q.y - y0;
            auto p = std::size_t(y) * width + x;
            const double weight = o.drizzleScale > 1 ? projected.weights[p] : 1.;
            if (weight <= 0) continue;
            std::array<double, 3> rgb{};
            bool reject = false, limited = false;
            for (int c = 0; c < 3; ++c) {
                auto at = [&](int xx, int yy) { return image.rgb[(std::size_t(yy) * width + xx) * 3 + c]; };
                double v = (at(x0, y0) * (1 - fx) + at(x1, y0) * fx) * (1 - fy) +
                           (at(x0, y1) * (1 - fx) + at(x1, y1) * fx) * fy;
                rgb[c] = v;
                if (o.rejection && count[p] >= kMinimumRejectionSamples) {
                    double noise = std::sqrt(
                        std::max(1. / (65535. * 65535.),
                                 double(m2_[p * 3 + c]) / std::max(1e-12, weights[p] * (count[p] - 1) / count[p])));
                    // Prediction interval for a new sample: the running mean is itself uncertain,
                    // and with few samples the per-pixel noise estimate is often far too small.
                    // Student-t correction (first Cornish-Fisher term) keeps the false-rejection
                    // rate close to the Gaussian one for the chosen sigma.
                    const double dof = std::max(1., double(count[p]) - 1);
                    double bound =
                        o.sigma * noise * std::sqrt(1. + 1. / count[p]) * (1. + (o.sigma * o.sigma + 1.) / (4. * dof));
                    if (std::abs(v - mean_[p * 3 + c]) > bound) {
                        if (o.winsorizedRejection) {
                            rgb[c] = std::clamp(v, mean_[p * 3 + c] - bound, mean_[p * 3 + c] + bound);
                            limited = true;
                        } else
                            reject = true;
                    }
                }
            }
            if (reject) {
                ++clipped;
                ++rejectedSamples;
                continue;
            }
            if (limited) {
                ++clipped;
                ++winsorizedSamples;
            }
            ++count[p];
            weights[p] += weight;
            for (int c = 0; c < 3; ++c) {
                auto k = p * 3 + c;
                double delta = rgb[c] - mean_[k];
                mean_[k] += weight * delta / weights[p];
                m2_[k] += float(weight * delta * (rgb[c] - mean_[k]));
            }
        }
    ++frames;
}
float Accumulator::value(std::size_t pixel, int channel) const {
    return count.at(pixel) ? float(mean_.at(pixel * 3 + channel)) : 0;
}

}
