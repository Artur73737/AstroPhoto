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
BandingResult reduceHorizontalBanding(LinearImage& image, const StackOptions& options) {
    validateStack(options);
    validImage(image);
    BandingResult result;
    if (!options.horizontalBanding || options.bandingStrength == 0) return result;
    const int w = image.width, h = image.height, stride = std::max(1, w / 2048);
    std::vector<std::array<float, 3>> rows(h), offsets(h);
    std::vector<bool> valid(h);
    // Robust channel row backgrounds exclude clipped pixels; fewer than half the
    // samples can be stars without moving the median. Smooth vertical gradients
    // are retained by subtracting only the residual against a local row median.
    for (int y = 0; y < h; ++y) {
        std::array<std::vector<float>, 3> samples;
        for (int x = 0; x < w; x += stride) {
            auto p = (std::size_t(y) * w + x) * 3;
            if (std::max({image.rgb[p], image.rgb[p + 1], image.rgb[p + 2]}) >= .98f) continue;
            for (int c = 0; c < 3; ++c)
                samples[c].push_back(image.rgb[p + c]);
        }
        if (samples[0].size() < std::size_t(std::max(4, w / stride / 4))) continue;
        valid[y] = true;
        for (int c = 0; c < 3; ++c)
            rows[y][c] = median(std::move(samples[c]));
    }
    for (int y = 0; y < h; ++y)
        if (valid[y])
            for (int c = 0; c < 3; ++c) {
                // Symmetric windows avoid inventing an offset at the top/bottom of a gradient.
                int radius = std::min({options.bandingRadius, y, h - 1 - y});
                if (radius < 2) continue;
                std::vector<float> neighbours;
                for (int yy = y - radius; yy <= y + radius; ++yy)
                    if (yy != y && valid[yy]) neighbours.push_back(rows[yy][c]);
                if (neighbours.size() < 4) continue;
                offsets[y][c] = rows[y][c] - median(std::move(neighbours));
            }
    std::array<float, 3> pedestal{};
    for (int c = 0; c < 3; ++c) {
        std::vector<float> all;
        for (int y = 0; y < h; ++y)
            if (valid[y]) all.push_back(offsets[y][c]);
        pedestal[c] = median(std::move(all));
    }
    for (int y = 0; y < h; ++y)
        if (valid[y]) {
            bool limited = false;
            for (int c = 0; c < 3; ++c) {
                double offset = offsets[y][c] - pedestal[c];
                limited |= std::abs(offset) > options.bandingMaxOffset;
                offset =
                    std::clamp(offset, -options.bandingMaxOffset, options.bandingMaxOffset) * options.bandingStrength;
                result.rms += offset * offset;
                for (int x = 0; x < w; ++x)
                    image.rgb[(std::size_t(y) * w + x) * 3 + c] -=
                        float(offset);  // retain negative linear samples; no black clipping bias
            }
            if (limited) ++result.limitedRows;
        }
    result.rms = std::sqrt(result.rms / (h * 3));
    return result;
}

}
