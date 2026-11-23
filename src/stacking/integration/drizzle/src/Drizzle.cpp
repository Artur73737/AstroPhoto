#include <astro/stacking/model/StackMath.hpp>
#include <algorithm>
#include <array>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>
#include <cmath>
#include <stdexcept>
namespace astro::stacking {
namespace {
// A clipped quad has at most 8 vertices; fixed storage keeps the hot loop allocation-free.
struct Polygon {
    std::array<Point, 12> v;
    int n = 0;
};
// Clip a projected drop to an output pixel; the contribution is its exact area.
double area(const Polygon& input, double x, double y) {
    Polygon p = input, q;
    auto clip = [&](bool axis, double edge, bool minimum) {
        q.n = 0;
        if (!p.n) return;
        auto at = [&](const Point& v) { return axis ? v.x : v.y; };
        auto prev = p.v[p.n - 1];
        bool pi = minimum ? at(prev) >= edge : at(prev) <= edge;
        for (int i = 0; i < p.n; ++i) {
            const auto v = p.v[i];
            const bool vi = minimum ? at(v) >= edge : at(v) <= edge;
            if (pi != vi) {
                const double t = (edge - at(prev)) / (at(v) - at(prev));
                q.v[q.n++] = {prev.x + t * (v.x - prev.x), prev.y + t * (v.y - prev.y)};
            }
            if (vi) q.v[q.n++] = v;
            prev = v;
            pi = vi;
        }
        p = q;
    };
    clip(true, x, true);
    clip(true, x + 1, false);
    clip(false, y, true);
    clip(false, y + 1, false);
    double a = 0;
    for (int i = 0; i < p.n; ++i) {
        const auto& r = p.v[(i + 1) % p.n];
        a += p.v[i].x * r.y - p.v[i].y * r.x;
    }
    return std::abs(a) * .5;
}
double overlap(double lo, double hi, int cell) {
    return std::max(0., std::min(hi, cell + 1.) - std::max(lo, double(cell)));
}
// Runs body(firstRow, endRow) over row bands on all cores. Bands are processed in two
// passes (even, then odd) so that bands writing to neighbouring output rows never run
// concurrently: each band is wider than the vertical footprint of one source row.
template <class F>
void parallelBands(int rows, int band, F body) {
    const int bands = (rows + band - 1) / band;
    const int workers = std::max(1u, std::min(16u, std::thread::hardware_concurrency()));
    for (int parity = 0; parity < 2; ++parity) {
        std::atomic<int> next{parity};
        std::vector<std::thread> threads;
        std::exception_ptr failure;
        std::mutex failureMutex;
        for (int w = 0; w < workers; ++w)
            threads.emplace_back([&] {
                try {
                    for (int b; (b = next.fetch_add(2)) < bands;)
                        body(b * band, std::min(rows, (b + 1) * band));
                } catch (...) {
                    std::lock_guard lock(failureMutex);
                    failure = std::current_exception();
                }
            });
        for (auto& t : threads)
            t.join();
        if (failure) std::rethrow_exception(failure);
    }
}
}  // namespace
DrizzleFrame drizzle(const LinearImage& input, const Transform& t, double scale, double pixfrac) {
    if (!plausible(t) || input.width < 8 || input.height < 8 ||
        input.rgb.size() != std::size_t(input.width) * input.height * 3 || !std::isfinite(scale) || scale < 1 ||
        scale > 3 || !std::isfinite(pixfrac) || pixfrac < .1 || pixfrac > 1)
        throw std::invalid_argument("Invalid drizzle input");
    DrizzleFrame out;
    out.image.width = int(std::ceil(input.width * scale));
    out.image.height = int(std::ceil(input.height * scale));
    out.image.previewGains = input.previewGains;
    out.image.whiteBalance = input.whiteBalance;
    const auto n = std::size_t(out.image.width) * out.image.height;
    out.weights.resize(n);
    out.image.rgb.resize(n * 3);
    const double det = t.a * t.d - t.b * t.c;
    auto project = [&](double x, double y) {
        return Point{((t.d * (x - t.tx) - t.b * (y - t.ty)) / det + .5) * scale,
                     ((-t.c * (x - t.tx) + t.a * (y - t.ty)) / det + .5) * scale};
    };
    const double half = pixfrac * .5;
    auto deposit = [&](std::size_t p, std::size_t source, double weight) {
        out.weights[p] += float(weight);
        for (int c = 0; c < 3; ++c)
            out.image.rgb[p * 3 + c] += float(input.rgb[source + c] * weight);
    };
    const bool axisAligned = std::abs(t.b) < 1e-12 && std::abs(t.c) < 1e-12;
    if (axisAligned) {
        // No rotation/shear (always the case without star alignment): each drop is an
        // axis-aligned rectangle, so its overlap with a pixel is a product of 1-D overlaps.
        parallelBands(input.height, 16, [&](int firstRow, int endRow) {
            for (int y = firstRow; y < endRow; ++y) {
                const auto a = project(0 - half, y - half), b = project(0 + half, y + half);
                const double y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
                const int yStart = std::max(0, int(std::floor(y0))),
                          yEnd = std::min(out.image.height, int(std::ceil(y1)));
                for (int x = 0; x < input.width; ++x) {
                    const auto c = project(x - half, y - half), d = project(x + half, y + half);
                    const double x0 = std::min(c.x, d.x), x1 = std::max(c.x, d.x);
                    const auto source = (std::size_t(y) * input.width + x) * 3;
                    for (int yy = yStart; yy < yEnd; ++yy) {
                        const double wy = overlap(y0, y1, yy);
                        if (wy <= 0) continue;
                        for (int xx = std::max(0, int(std::floor(x0)));
                             xx < std::min(out.image.width, int(std::ceil(x1))); ++xx) {
                            const double weight = wy * overlap(x0, x1, xx);
                            if (weight > 1e-12) deposit(std::size_t(yy) * out.image.width + xx, source, weight);
                        }
                    }
                }
            }
        });
    } else {
        for (int y = 0; y < input.height; ++y)
            for (int x = 0; x < input.width; ++x) {
                Polygon poly;
                poly.v[0] = project(x - half, y - half);
                poly.v[1] = project(x + half, y - half);
                poly.v[2] = project(x + half, y + half);
                poly.v[3] = project(x - half, y + half);
                poly.n = 4;
                double minx = poly.v[0].x, maxx = minx, miny = poly.v[0].y, maxy = miny;
                for (int i = 0; i < 4; ++i) {
                    minx = std::min(minx, poly.v[i].x);
                    maxx = std::max(maxx, poly.v[i].x);
                    miny = std::min(miny, poly.v[i].y);
                    maxy = std::max(maxy, poly.v[i].y);
                }
                const auto source = (std::size_t(y) * input.width + x) * 3;
                for (int yy = std::max(0, int(std::floor(miny))); yy < std::min(out.image.height, int(std::ceil(maxy)));
                     ++yy)
                    for (int xx = std::max(0, int(std::floor(minx)));
                         xx < std::min(out.image.width, int(std::ceil(maxx))); ++xx) {
                        const double weight = area(poly, xx, yy);
                        if (weight > 1e-12) deposit(std::size_t(yy) * out.image.width + xx, source, weight);
                    }
            }
    }
    for (std::size_t p = 0; p < n; ++p)
        if (out.weights[p] > 0)
            for (int c = 0; c < 3; ++c)
                out.image.rgb[p * 3 + c] /= out.weights[p];
    return out;
}
}  // namespace astro::stacking
