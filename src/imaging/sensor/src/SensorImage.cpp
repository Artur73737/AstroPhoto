#include <astro/imaging/sensor/SensorImage.hpp>
#include <algorithm>
#include <stdexcept>
namespace astro::sensor {
void validate(const Frame& f) {
    const int bpp = f.format == Raw16 ? 2 : f.format == Bgr24 ? 3 : 1;
    if (f.width < 1 || f.height < 1 || f.width > 32768 || f.height > 32768 || f.format < 0 || f.format > 3 ||
        std::uint64_t(f.width) * f.height * bpp > 512ull * 1024 * 1024 ||
        f.bytes.size() != std::size_t(f.width) * f.height * bpp)
        throw std::invalid_argument("Invalid native sensor frame");
}
Display display(const Frame& f, const std::string& pattern, int maxWidth) {
    validate(f);
    if (!pattern.empty() && pattern.size() != 4) throw std::invalid_argument("Invalid CFA pattern");
    const int w = std::min(f.width, maxWidth > 0 ? maxWidth : f.width),
              h = std::max(1, int(double(f.height) * w / f.width));
    Display out{w, h, std::vector<std::uint8_t>(std::size_t(w) * h * 3)};
    auto sample = [&](int x, int y) -> unsigned {
        x = std::clamp(x, 0, f.width - 1);
        y = std::clamp(y, 0, f.height - 1);
        auto i = std::size_t(y) * f.width + x;
        return f.format == Raw16 ? (unsigned(f.bytes[i * 2]) | (unsigned(f.bytes[i * 2 + 1]) << 8)) >> 8 : f.bytes[i];
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int sx = x * f.width / w, sy = y * f.height / h;
            auto index = (std::size_t(y) * w + x) * 3;
            if (f.format == Bgr24) {
                auto src = (std::size_t(sy) * f.width + sx) * 3;
                for (int c = 0; c < 3; ++c)
                    out.bgr[index + c] = f.bytes[src + c];
                continue;
            }
            unsigned r, g, b, v = sample(sx, sy);
            if (pattern.empty() || f.format == Mono8)
                r = g = b = v;
            else {
                const char c = pattern[(sy & 1) * 2 + (sx & 1)];
                const unsigned cross =
                    (sample(sx - 1, sy) + sample(sx + 1, sy) + sample(sx, sy - 1) + sample(sx, sy + 1) + 2) / 4;
                const unsigned diagonal = (sample(sx - 1, sy - 1) + sample(sx + 1, sy - 1) + sample(sx - 1, sy + 1) +
                                           sample(sx + 1, sy + 1) + 2) /
                                          4;
                if (c == 'R') {
                    r = v;
                    g = cross;
                    b = diagonal;
                } else if (c == 'B') {
                    b = v;
                    g = cross;
                    r = diagonal;
                } else {
                    g = v;
                    auto horizontal = (sample(sx - 1, sy) + sample(sx + 1, sy) + 1) / 2;
                    auto vertical = (sample(sx, sy - 1) + sample(sx, sy + 1) + 1) / 2;
                    if (pattern[(sy & 1) * 2 + ((sx + 1) & 1)] == 'R') {
                        r = horizontal;
                        b = vertical;
                    } else {
                        b = horizontal;
                        r = vertical;
                    }
                }
            }
            out.bgr[index] = std::uint8_t(b);
            out.bgr[index + 1] = std::uint8_t(g);
            out.bgr[index + 2] = std::uint8_t(r);
        }
    return out;
}
}  // namespace astro::sensor
