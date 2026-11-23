#include <astro/features/true_north/astronomy/TrueNorth.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <astro/features/true_north/astrometry/catalog/PolarCatalog.hpp>
namespace astro::north::detail {
using stacking::Point;
using stacking::Transform;
std::vector<Point> catalog(const std::filesystem::path& root) {
    std::ifstream in(root / "g05_1801.290", std::ios::binary);
    if (!in) throw std::runtime_error("Polar recovery requires the complete G05 catalogue");
    char header[110];
    if (!in.read(header, 110) || static_cast<unsigned char>(header[109]) != 5)
        throw std::runtime_error("Unsupported G05 record layout");
    std::vector<Point> points;
    int decHigh = 0;
    double magnitude = 0;
    unsigned char b[5];
    Wcs projection{0, 0, 0, 90, {1, 0, 0, 1}, 2, 2};
    while (in.read(reinterpret_cast<char*>(b), 5)) {
        unsigned ra = b[0] | (unsigned(b[1]) << 8) | (unsigned(b[2]) << 16);
        if (ra == 0xffffff) {
            decHigh = int(b[3]) - 128;
            magnitude = (int(b[4]) - 16) * .1;
            continue;
        }
        if (magnitude > 11.5) continue;
        double dec = (decHigh * 65536 + int(b[4]) * 256 + b[3]) * 90. / 8388607.;
        if (dec < 85) continue;
        auto pixel = pixelAt(projection, {ra * 360. / 16777215., dec});
        if (pixel) points.push_back({pixel->x, pixel->y, -magnitude});
    }
    if (!in.eof()) throw std::runtime_error("Truncated G05 star catalogue");
    std::stable_sort(points.begin(), points.end(), [](auto a, auto b) { return a.flux > b.flux; });
    // Remove unresolved catalogue double entries; do not count one image star twice.
    std::vector<Point> distinct;
    for (auto p : points) {
        bool duplicate = false;
        for (auto q : distinct)
            if (std::hypot(p.x - q.x, p.y - q.y) < .002) {
                duplicate = true;
                break;
            }
        if (!duplicate) distinct.push_back(p);
    }
    if (distinct.size() > 1800) distinct.resize(1800);
    return distinct;
}
}  // namespace astro::north::detail
