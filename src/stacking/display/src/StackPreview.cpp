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
StackPreview render(const Accumulator& a, StackDisplay d, std::uint64_t revision) {
    validateStackDisplay(d);
    StackPreview p;
    p.display = d;
    p.revision = revision;
    if (!a.frames) return p;
    double scale = std::min(1., 1280. / std::max(a.width, a.height));
    p.width = std::max(1, int(a.width * scale));
    p.height = std::max(1, int(a.height * scale));
    p.rgba.resize(std::size_t(p.width) * p.height * 4);
    p.rgba16.resize(p.rgba.size());
    for (int y = 0; y < p.height; ++y)
        for (int x = 0; x < p.width; ++x) {
            int x0 = x * a.width / p.width, x1 = (x + 1) * a.width / p.width, y0 = y * a.height / p.height,
                y1 = (y + 1) * a.height / p.height;
            double rgb[3]{}, n = 0;
            for (int yy = y0; yy < y1; ++yy)
                for (int xx = x0; xx < x1; ++xx) {
                    auto i = std::size_t(yy) * a.width + xx;
                    if (!a.count[i]) continue;
                    for (int c = 0; c < 3; ++c)
                        rgb[c] += a.value(i, c);
                    ++n;
                }
            auto out = (std::size_t(y) * p.width + x) * 4;
            double lum = 0;
            for (int c = 0; c < 3; ++c) {
                double linear = n ? rgb[c] / n * a.previewGains[c] : 0;
                lum += linear * (c == 0 ? .2126 : c == 1 ? .7152 : .0722);
                double v = std::clamp((linear - d.low) / (d.high - d.low), 0., 1.);
                double mapped = v == 0 ? 0 : v == 1 ? 1 : ((d.mid - 1) * v) / ((2 * d.mid - 1) * v - d.mid);
                if (d.asinh) {
                    const double strength = 1. / d.mid;
                    mapped = std::asinh(strength * v) / std::asinh(strength);
                }
                p.rgba[out + c] = std::uint8_t(std::clamp(mapped * 255., 0., 255.));
                p.rgba16[out + c] = std::uint16_t(std::clamp(mapped * 65535., 0., 65535.));
            }
            p.rgba[out + 3] = 255;
            p.rgba16[out + 3] = 65535;
            ++p.histogram[std::clamp(int(lum * 255), 0, 255)];
        }
    for (auto& bin : p.histogram)
        bin = std::log1p(bin);
    return p;
}

}
