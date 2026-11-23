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
Alignment align(const std::vector<Point>& ref, const std::vector<Point>& cur, int w, int h, const StackOptions& o) {
    Alignment best;
    best.reason = "Insufficient distributed alignment stars";
    const int required = std::max(o.minimumStars, int(std::ceil(std::min(ref.size(), cur.size()) * .2)));
    if (ref.size() < required || cur.size() < required || !coverage(ref, w, h) || !coverage(cur, w, h)) return best;
    // Spatial bins bound nearest-neighbour cost for each candidate transform.
    auto key = [](int x, int y) { return (std::uint64_t(std::uint32_t(x)) << 32) | std::uint32_t(y); };
    std::unordered_map<std::uint64_t, std::vector<int>> grid;
    for (int i = 0; i < int(cur.size()); ++i)
        grid[key(int(std::floor(cur[i].x / 4)), int(std::floor(cur[i].y / 4)))].push_back(i);
    auto matches = [&](Transform t, double radius) {
        std::vector<int> owner(cur.size(), -1);
        std::vector<double> distances(cur.size(), radius * radius);
        for (int i = 0; i < int(ref.size()); ++i) {
            auto p = t.apply(ref[i]);
            int bx = int(std::floor(p.x / 4)), by = int(std::floor(p.y / 4)), index = -1;
            double bestD = radius * radius;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    auto it = grid.find(key(bx + dx, by + dy));
                    if (it == grid.end()) continue;
                    for (int j : it->second) {
                        double d = std::pow(p.x - cur[j].x, 2) + std::pow(p.y - cur[j].y, 2);
                        if (d < bestD) {
                            index = j;
                            bestD = d;
                        }
                    }
                }
            if (index >= 0 && bestD < distances[index]) {
                owner[index] = i;
                distances[index] = bestD;
            }
        }
        std::vector<std::pair<Point, Point>> pairs;
        for (int j = 0; j < int(cur.size()); ++j)
            if (owner[j] >= 0) pairs.emplace_back(ref[owner[j]], cur[j]);
        return pairs;
    };
    auto assess = [&](Transform t) {
        if (!plausible(t)) return;
        auto pairs = matches(t, 4);
        if (pairs.size() < required) return;
        for (int k = 0; k < 3; ++k) {
            t = o.affineAlignment ? fitAffine(pairs) : fitSimilarity(pairs);
            if (!plausible(t)) return;
            pairs = matches(t, k == 0 ? 3 : 2);
            if (pairs.size() < required) return;
        }
        double err = 0;
        std::vector<Point> a, b;
        for (auto [r, s] : pairs) {
            err += std::pow(dist(t.apply(r), s), 2);
            a.push_back(r);
            b.push_back(s);
        }
        double rms = std::sqrt(err / pairs.size()), ov = overlap(t, w, h);
        if (rms > o.maximumRms || ov < .25 || !coverage(a, w, h) || !coverage(b, w, h)) return;
        // Reject likely accidental correspondences using a conservative local-density test.
        double expected = 0;
        int observed = 0;
        for (auto r : ref) {
            auto p = t.apply(r);
            int nearby = 0;
            for (auto q : cur)
                if (dist(p, q) < 16) ++nearby;
            expected += 1 - std::exp(-nearby / 256.);
        }
        for (auto [r, s] : pairs)
            if (dist(t.apply(r), s) <= 1) ++observed;
        if (observed <= expected || (expected > 0 && observed - expected - observed * std::log(observed / expected) >
                                                         std::log(1e-5 / (3 * 32768))))
            return;
        if (!best.accepted || pairs.size() > best.matches || (pairs.size() == best.matches && rms < best.rms))
            best = {true, t, int(pairs.size()), rms, ov, "Aligned and integrated"};
    };
    assess({});
    if (best.accepted) return best;
    // Canonical triangles provide translation/rotation-invariant hypotheses.
    struct Tri {
        std::array<Point, 3> p;
        double r1, r2;
    };
    auto triangles = [](const std::vector<Point>& all) {
        std::vector<Point> pts;
        std::array<int, 16> cells{};
        double maxx = 1, maxy = 1;
        for (auto p : all) {
            maxx = std::max(maxx, p.x);
            maxy = std::max(maxy, p.y);
        }
        for (auto p : all) {
            auto cell = std::min(3, int(p.y * 4 / maxy)) * 4 + std::min(3, int(p.x * 4 / maxx));
            if (cells[cell]++ < 3) pts.push_back(p);
            if (pts.size() == 40) break;
        }
        std::vector<Tri> ts;
        for (int a = 0; a < int(pts.size()); ++a)
            for (int b = a + 1; b < int(pts.size()); ++b)
                for (int c = b + 1; c < int(pts.size()); ++c) {
                    std::array<Point, 3> p{pts[a], pts[b], pts[c]};
                    std::array<std::pair<double, int>, 3> d{
                        {{dist(p[1], p[2]), 0}, {dist(p[0], p[2]), 1}, {dist(p[0], p[1]), 2}}};
                    std::sort(d.begin(), d.end());
                    if (d[2].first < 12 || d[0].first / d[2].first < .1) continue;
                    ts.push_back({{p[d[0].second], p[d[1].second], p[d[2].second]},
                                  d[0].first / d[2].first,
                                  d[1].first / d[2].first});
                }
        std::sort(ts.begin(), ts.end(), [](auto a, auto b) { return a.r1 < b.r1; });
        return ts;
    };
    auto rt = triangles(ref), ct = triangles(cur);
    int hypotheses = 0;
    for (const auto& a : rt) {
        auto it = std::lower_bound(ct.begin(), ct.end(), a.r1 - .002, [](const auto& t, double v) { return t.r1 < v; });
        for (; it != ct.end() && it->r1 < a.r1 + .002; ++it) {
            if (std::abs(a.r2 - it->r2) > .002) continue;
            auto t = fitSimilarity({{a.p[0], it->p[0]}, {a.p[1], it->p[1]}, {a.p[2], it->p[2]}});
            if (!plausible(t)) continue;
            assess(t);
            if (best.accepted && best.matches >= required) return best;
            if (++hypotheses >= 4096) break;
        }
        if (hypotheses >= 4096) break;
    }
    if (!best.accepted && o.affineAlignment) {
        // A fourth star's barycentric coordinates are invariant under an affine
        // mapping. Try bounded quad hypotheses if triangle ratios did not recover.
        struct Quad {
            std::array<Point, 4> p;
            double u, v;
        };
        auto quads = [](const std::vector<Point>& all) {
            std::vector<Point> points;
            std::array<int, 16> cells{};
            double mx = 1, my = 1;
            for (auto p : all) {
                mx = std::max(mx, p.x);
                my = std::max(my, p.y);
            }
            for (auto p : all) {
                int cell = std::min(3, int(p.y * 4 / my)) * 4 + std::min(3, int(p.x * 4 / mx));
                if (cells[cell]++ < 2) points.push_back(p);
                if (points.size() == 16) break;
            }
            std::vector<Quad> result;
            for (int a = 0; a < int(points.size()); ++a)
                for (int b = a + 1; b < int(points.size()); ++b)
                    for (int c = b + 1; c < int(points.size()); ++c)
                        for (int d = c + 1; d < int(points.size()); ++d) {
                            std::array<int, 4> ids{a, b, c, d};
                            do {
                                auto p = points[ids[0]], q = points[ids[1]], r = points[ids[2]], s = points[ids[3]];
                                double x = q.x - p.x, y = q.y - p.y, xx = r.x - p.x, yy = r.y - p.y,
                                       det = x * yy - y * xx;
                                if (std::abs(det) < 100) continue;
                                double u = ((s.x - p.x) * yy - (s.y - p.y) * xx) / det,
                                       v = (x * (s.y - p.y) - y * (s.x - p.x)) / det;
                                if (std::abs(u) > 4 || std::abs(v) > 4) continue;
                                result.push_back({{p, q, r, s}, u, v});
                            } while (std::next_permutation(ids.begin(), ids.end()));
                        }
            std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.u < b.u; });
            return result;
        };
        auto rq = quads(ref), cq = quads(cur);
        int trials = 0;
        for (const auto& q : rq) {
            auto at =
                std::lower_bound(cq.begin(), cq.end(), q.u - .001, [](const auto& p, double v) { return p.u < v; });
            for (; at != cq.end() && at->u < q.u + .001; ++at) {
                if (std::abs(q.v - at->v) > .001) continue;
                std::vector<std::pair<Point, Point>> seed;
                for (int i = 0; i < 4; ++i)
                    seed.emplace_back(q.p[i], at->p[i]);
                try {
                    auto t = fitAffine(seed);
                    if (plausible(t)) assess(t);
                } catch (const std::invalid_argument&) {
                }
                if (best.accepted) {
                    best.reason = "Quad recovery / affine refinement";
                    return best;
                }
                if (++trials >= 4096) break;
            }
            if (trials >= 4096) break;
        }
    }
    if (!best.accepted) best.reason = "Alignment rejected: insufficient unique matches, geometry or residual quality";
    return best;
}

}
