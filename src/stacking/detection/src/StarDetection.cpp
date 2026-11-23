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
std::vector<float> luminance(const LinearImage& image) {
    std::vector<float> y(static_cast<std::size_t>(image.width) * image.height);
    for (std::size_t p = 0; p < y.size(); ++p) {
        y[p] = .2126f * image.rgb[p * 3] + .7152f * image.rgb[p * 3 + 1] + .0722f * image.rgb[p * 3 + 2];
        if (!std::isfinite(y[p])) throw std::invalid_argument("Non-finite RAW pixel");
    }
    return y;
}
std::optional<Point> measureStar(const LinearImage& image, const std::vector<float>& y, int x, int row,
                                 float background, float noise) {
    const int w = image.width;
    const auto p = row * w + x;
    // 7x7 background-subtracted moments give flux, centroid and second moments.
    double flux = 0, cx = 0, cy = 0, xx = 0, xy = 0, yy = 0;
    int support = 0;
    bool clipped = false;
    for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -3; dx <= 3; ++dx) {
            const auto pi = p + dy * w + dx;
            const float v = std::max(0.f, y[pi] - background);
            if (v > noise * 2) ++support;
            for (int c = 0; c < 3; ++c)
                if (image.rgb[pi * 3 + c] >= .995f) clipped = true;
            flux += v;
            cx += dx * v;
            cy += dy * v;
            xx += dx * dx * v;
            yy += dy * dy * v;
            xy += dx * dy * v;
        }
    if (clipped || support < 5 || flux <= 0) return std::nullopt;
    cx /= flux;
    cy /= flux;
    xx = xx / flux - cx * cx;
    yy = yy / flux - cy * cy;
    xy = xy / flux - cx * cy;
    // Reject hot pixels, elongated trails and off-centre blends by shape.
    const double disc = std::sqrt((xx - yy) * (xx - yy) + 4 * xy * xy), major = (xx + yy + disc) / 2,
                 minor = (xx + yy - disc) / 2;
    if (minor < .25 || major > 9 || minor / major < .35 || std::abs(cx) > 1.5 || std::abs(cy) > 1.5)
        return std::nullopt;
    // Half-flux diameter from the radial cumulative profile.
    std::vector<std::pair<double, double>> radii;
    for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -3; dx <= 3; ++dx)
            radii.emplace_back(std::hypot(dx - cx, dy - cy), std::max(0.f, y[p + dy * w + dx] - background));
    std::sort(radii.begin(), radii.end());
    double cumulative = 0, hfd = 0;
    for (auto [radius, value] : radii) {
        cumulative += value;
        if (cumulative >= flux * .5) {
            hfd = 2 * radius;
            break;
        }
    }
    return Point{x + cx, row + cy, flux, hfd, 2.354820045 * std::sqrt((xx + yy) * .5)};
}
namespace {
bool localMaximum(const std::vector<float>& y, int w, std::size_t p) {
    const float peak = y[p];
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
            if ((dx || dy) && y[p + dy * w + dx] >= peak) return false;
    return true;
}
// Local 64x64 background / MAD protects faint stars from gradients.
std::vector<Point> findStarsInTiles(const LinearImage& image, const std::vector<float>& y, double snr) {
    const int w = image.width, h = image.height;
    std::vector<Point> stars;
    for (int by = 4; by < h - 4; by += 64)
        for (int bx = 4; bx < w - 4; bx += 64) {
            const int yEnd = std::min(by + 64, h - 4), xEnd = std::min(bx + 64, w - 4);
            std::vector<float> sample;
            for (int yy = by; yy < yEnd; yy += 2)
                for (int x = bx; x < xEnd; x += 2)
                    sample.push_back(y[yy * w + x]);
            const float bg = median(sample);
            for (auto& v : sample)
                v = std::abs(v - bg);
            const float noise = std::max(1.f / 65535, 1.4826f * median(sample));
            const double threshold = bg + snr * noise;
            for (int yy = by; yy < yEnd; ++yy)
                for (int x = bx; x < xEnd; ++x) {
                    const auto p = std::size_t(yy) * w + x;
                    const float peak = y[p];
                    if (peak < threshold || peak >= .98f || !localMaximum(y, w, p)) continue;
                    if (auto star = measureStar(image, y, x, yy, bg, noise)) stars.push_back(*star);
                }
        }
    return stars;
}
}  // namespace
std::vector<Point> selectDistributedStars(std::vector<Point> stars, int w, int h) {
    // Brightest first, at most 8 per cell of an 8x8 grid, 6 px separation, 512 total.
    std::sort(stars.begin(), stars.end(), [](auto a, auto b) { return a.flux > b.flux; });
    std::vector<Point> result;
    std::array<int, 64> cells{};
    for (auto p : stars) {
        const int cell = std::min(7, int(p.y * 8 / h)) * 8 + std::min(7, int(p.x * 8 / w));
        if (cells[cell] >= 8) continue;
        if (std::any_of(result.begin(), result.end(), [&](auto q) { return dist(p, q) < 6; })) continue;
        result.push_back(p);
        ++cells[cell];
        if (result.size() == 512) break;
    }
    return result;
}
std::vector<Point> detectStars(const LinearImage& i, double snr) {
    validImage(i);
    const auto y = luminance(i);
    return selectDistributedStars(findStarsInTiles(i, y, snr), i.width, i.height);
}

}
