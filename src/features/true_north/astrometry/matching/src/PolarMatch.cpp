#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <map>
#include <astro/features/true_north/astrometry/matching/PolarMatch.hpp>
namespace astro::north::detail {
using stacking::Point;
using stacking::Transform;
struct Triangle {
    std::array<int, 3> ids;
    double r1, r2;
};
std::vector<Triangle> triangles(const std::vector<Point>& points, int limit) {
    std::vector<Triangle> result;
    int n = std::min(limit, int(points.size()));
    for (int a = 0; a < n; ++a)
        for (int b = a + 1; b < n; ++b)
            for (int c = b + 1; c < n; ++c) {
                std::array<int, 3> ids{a, b, c};
                auto distance = [&](int i, int j) {
                    return std::hypot(points[i].x - points[j].x, points[i].y - points[j].y);
                };
                std::array<std::pair<double, int>, 3> sides{
                    {{distance(b, c), a}, {distance(a, c), b}, {distance(a, b), c}}};
                std::sort(sides.begin(), sides.end());
                if (sides[2].first <= 0 || sides[0].first / sides[2].first < .15) continue;
                if ((sides[0].first + sides[1].first) / sides[2].first < 1.05) continue;
                result.push_back({{sides[0].second, sides[1].second, sides[2].second},
                                  sides[0].first / sides[2].first,
                                  sides[1].first / sides[2].first});
            }
    std::sort(result.begin(), result.end(), [](auto a, auto b) { return a.r1 < b.r1; });
    return result;
}
bool shape(const Transform& t, int height) {
    double x = t.a * t.a + t.c * t.c, y = t.b * t.b + t.d * t.d, cross = t.a * t.b + t.c * t.d,
           scale = std::sqrt((x + y) * .5);
    return std::isfinite(scale) && scale > height / 12. && scale < height / .1 && std::abs(x - y) / (x + y) < .015 &&
           std::abs(cross) / (x + y) < .008;
}
Match recover(const std::vector<Point>& reference, const std::vector<Point>& sources, int width, int height,
              int timeoutSeconds, const std::function<bool()>& cancelled,
              const std::function<void(const std::string&)>& progress) {
    auto start = std::chrono::steady_clock::now();
    if (sources.size() < 12) throw std::runtime_error("Insufficient independent polar stars");
    if (progress) progress("Polar recovery: matching scale-invariant Gaia triangle patterns");
    auto rt = triangles(reference, 130), it = triangles(sources, 45);
    // Uniform image bins make unique nearest-neighbour verification bounded.
    std::map<std::pair<int, int>, std::vector<int>> bins;
    for (int i = 0; i < int(sources.size()); ++i)
        bins[{int(sources[i].x / 4), int(sources[i].y / 4)}].push_back(i);
    auto pairs = [&](Transform t, double radius) {
        std::vector<int> owner(sources.size(), -1);
        std::vector<double> error(sources.size(), radius * radius);
        for (int i = 0; i < int(reference.size()); ++i) {
            auto p = t.apply(reference[i]);
            if (p.x < 1 || p.x > width || p.y < 1 || p.y > height) continue;
            int bx = int(p.x / 4), by = int(p.y / 4);
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    auto cell = bins.find({bx + dx, by + dy});
                    if (cell == bins.end()) continue;
                    for (int j : cell->second) {
                        auto q = sources[j];
                        double d = (p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y);
                        if (d < error[j]) {
                            error[j] = d;
                            owner[j] = i;
                        }
                    }
                }
        }
        std::vector<std::pair<Point, Point>> result;
        for (int j = 0; j < int(sources.size()); ++j)
            if (owner[j] >= 0) result.emplace_back(reference[owner[j]], sources[j]);
        return result;
    };
    int hypotheses = 0;
    Transform best;
    std::vector<std::pair<Point, Point>> chosen;
    for (auto a : it) {
        auto r =
            std::lower_bound(rt.begin(), rt.end(), a.r1 - .0015, [](auto tri, double value) { return tri.r1 < value; });
        for (; r != rt.end() && r->r1 < a.r1 + .0015; ++r) {
            if (std::abs(r->r2 - a.r2) > .0015) continue;
            if (++hypotheses > 25000 || std::chrono::steady_clock::now() - start > std::chrono::seconds(timeoutSeconds))
                throw std::runtime_error("Polar matching search budget exhausted");
            if (cancelled && cancelled()) throw std::runtime_error("Polar matching cancelled");
            std::vector<std::pair<Point, Point>> seed;
            for (int k = 0; k < 3; ++k)
                seed.emplace_back(reference[r->ids[k]], sources[a.ids[k]]);
            Transform t;
            try {
                t = stacking::fitAffine(seed);
            } catch (...) {
                continue;
            }
            if (!shape(t, height)) continue;
            auto matched = pairs(t, 3);
            if (matched.size() < 12) continue;
            for (int n = 0; n < 3; ++n) {
                t = stacking::fitAffine(matched);
                matched = pairs(t, n < 2 ? 2 : 1.5);
                if (matched.size() < 12) break;
            }
            if (matched.size() < 12 || !shape(t, height)) continue;
            double rms = 0, minx = width, maxx = 0, miny = height, maxy = 0;
            std::array<int, 9> cells{};
            for (auto [p, q] : matched) {
                auto z = t.apply(p);
                rms += std::pow(z.x - q.x, 2) + std::pow(z.y - q.y, 2);
                minx = std::min(minx, q.x);
                maxx = std::max(maxx, q.x);
                miny = std::min(miny, q.y);
                maxy = std::max(maxy, q.y);
                ++cells[std::min(2, int(q.y * 3 / height)) * 3 + std::min(2, int(q.x * 3 / width))];
            }
            rms = std::sqrt(rms / matched.size());
            double expected =
                reference.size() * sources.size() * 3.141592653589793 * 1.5 * 1.5 / (double(width) * height);
            // Only catalogue sources projected inside this frame contribute chance matches.
            int visible = 0;
            for (auto p : reference) {
                auto q = t.apply(p);
                if (q.x >= 1 && q.x <= width && q.y >= 1 && q.y <= height) ++visible;
            }
            expected = std::max(1e-9, visible * sources.size() * 3.141592653589793 * 2.25 / (double(width) * height));
            double probability = matched.size() * (1 + std::log(expected / matched.size())) - expected;
            if (rms > .8 || maxx - minx < width * .45 || maxy - miny < height * .45 ||
                std::count_if(cells.begin(), cells.end(), [](int n) { return n > 0; }) < 4 ||
                probability > std::log(1e-6 / 25000))
                continue;
            best = t;
            chosen = std::move(matched);
            break;
        }
        if (!chosen.empty()) break;
    }
    if (chosen.empty()) throw std::runtime_error("No statistically validated polar catalogue match");
    // Refit to all confirmed unique matches, and verify stars not used in the seed.
    best = stacking::fitAffine(chosen);
    double det = best.a * best.d - best.b * best.c, rms = 0;
    for (auto [p, q] : chosen) {
        auto z = best.apply(p);
        rms += std::pow(z.x - q.x, 2) + std::pow(z.y - q.y, 2);
    }
    rms = std::sqrt(rms / chosen.size());
    return {best, int(chosen.size()), rms};
}

}  // namespace astro::north::detail
