#include <astro/features/true_north/astronomy/TrueNorth.hpp>
#include <erfa.h>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <cstdio>
#include <cctype>
namespace astro::north {
namespace {
constexpr double r = std::numbers::pi / 180.;
double wrap(double d) {
    d = std::fmod(d, 360.);
    return d < 0 ? d + 360 : d;
}
double angle(double d) {
    return std::remainder(d, 360.);
}
void check(int status) {
    if (status < 0) throw std::invalid_argument("Invalid UTC / ERFA input");
}
}  // namespace
void utc(Observation& o, const std::string& text) {
    if (text.size() < 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' || text[13] != ':' || text[16] != ':' ||
        text.back() != 'Z')
        throw std::invalid_argument("UTC must use YYYY-MM-DDTHH:MM:SS[.fraction]Z");
    for (std::size_t i = 0; i < text.size(); ++i)
        if (i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && i != text.size() - 1 && !(i == 19 && text[i] == '.') &&
            !std::isdigit(static_cast<unsigned char>(text[i])))
            throw std::invalid_argument("Invalid UTC syntax");
    int y, m, d, h, n;
    double s;
    int used = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%dT%d:%d:%lfZ%n", &y, &m, &d, &h, &n, &s, &used) != 6 ||
        used != int(text.size()))
        throw std::invalid_argument("UTC must include seconds and Z; use exposure midpoint");
    const auto status = eraDtf2d("UTC", y, m, d, h, n, s, &o.utc1, &o.utc2);
    if (status < 0 || status > 1) throw std::invalid_argument("Invalid UTC calendar/time");
}
void validate(const Wcs& w, const Observation& o) {
    for (double v : {w.crpix1,        w.crpix2,        w.ra,          w.dec,           w.cd[0],          w.cd[1],
                     w.cd[2],         w.cd[3],         o.latitude,    o.longitude,     o.height,         o.utc1,
                     o.utc2,          o.pressure,      o.temperature, o.humidity,      o.dut1,           o.roll,
                     o.rollTolerance, o.levelingSigma, o.timeSigma,   o.latitudeSigma, o.longitudeSigma, o.solveSigma})
        if (!std::isfinite(v)) throw std::invalid_argument("Non-finite True North parameter");
    if (w.width < 2 || w.height < 2 || w.ra < 0 || w.ra >= 360 || std::abs(w.dec) > 90 ||
        std::abs(w.cd[0] * w.cd[3] - w.cd[1] * w.cd[2]) < 1e-15)
        throw std::invalid_argument("Invalid TAN WCS");
    if (std::abs(o.latitude) >= 89.9 || std::abs(o.longitude) > 180 || o.utc1 + o.utc2 < 2441317.5 || o.pressure < 0 ||
        o.pressure > 1100 || o.temperature < -90 || o.temperature > 60 || o.humidity < 0 || o.humidity > 1 ||
        std::abs(o.dut1) > 2)
        throw std::invalid_argument("Invalid site, time or atmosphere");
    if (!o.physicalContractConfirmed || !o.rigid || o.mirrored || o.inverted)
        throw std::invalid_argument("Section 3.6: confirm level, rigid, upright and unmirrored physical camera");
    if (std::abs(o.roll) > .1 || o.rollTolerance <= 0 || o.rollTolerance > 1 || o.levelingSigma <= 0 ||
        o.timeSigma < 0 || o.latitudeSigma < 0 || o.longitudeSigma < 0 || o.solveSigma < 0)
        throw std::invalid_argument("Landscape v1 requires known roll zero +/- 0.1 degree and declared uncertainties");
}
Sky skyAt(const Wcs& w, Pixel p) {
    double x = (p.x - w.crpix1) * w.cd[0] + (p.y - w.crpix2) * w.cd[1],
           y = (p.x - w.crpix1) * w.cd[2] + (p.y - w.crpix2) * w.cd[3];
    double ra, dec;
    eraTpsts(x * r, y * r, w.ra * r, w.dec * r, &ra, &dec);
    return {wrap(ra / r), dec / r};
}
std::optional<Pixel> pixelAt(const Wcs& w, Sky s) {
    double x, y;
    if (eraTpxes(s.ra * r, s.dec * r, w.ra * r, w.dec * r, &x, &y) != 0) return {};
    x /= r;
    y /= r;
    double det = w.cd[0] * w.cd[3] - w.cd[1] * w.cd[2];
    return Pixel{w.crpix1 + (w.cd[3] * x - w.cd[1] * y) / det, w.crpix2 + (-w.cd[2] * x + w.cd[0] * y) / det};
}
Horizontal observed(Sky s, const Observation& o) {
    double az, z, ha, dec, ra, eo;
    check(eraAtco13(s.ra * r, s.dec * r, 0, 0, 0, 0, o.utc1, o.utc2, o.dut1, o.longitude * r, o.latitude * r, o.height,
                    0, 0, o.pressure, o.temperature, o.humidity, .55, &az, &z, &ha, &dec, &ra, &eo));
    return {wrap(az / r), 90 - z / r};
}
std::optional<Pixel> celestialPole(const Wcs& w, const Observation& o) {
    double tt1, tt2, tai1, tai2, ra, dec, matrix[3][3], axis[3]{0, 0, 1}, icrs[3];
    check(eraUtctai(o.utc1, o.utc2, &tai1, &tai2));
    check(eraTaitt(tai1, tai2, &tt1, &tt2));
    // A physical axis is a direction, not an apparent star: do not apply annual aberration.
    eraC2i06a(tt1, tt2, matrix);
    eraTrxp(matrix, axis, icrs);
    eraC2s(icrs, &ra, &dec);
    return pixelAt(w, {wrap(ra / r), dec / r});
}
std::array<double, 2> fieldOfView(const Wcs& w) {
    auto separation = [](Sky a, Sky b) { return eraSeps(a.ra * r, a.dec * r, b.ra * r, b.dec * r) / r; };
    return {separation(skyAt(w, {.5, (w.height + 1) * .5}), skyAt(w, {w.width + .5, (w.height + 1) * .5})),
            separation(skyAt(w, {(w.width + 1) * .5, .5}), skyAt(w, {(w.width + 1) * .5, w.height + .5}))};
}
Result calculate(const Wcs& w, const Observation& o) {
    validate(w, o);
    Result out;
    Pixel c{(w.width + 1) * .5, (w.height + 1) * .5};
    auto centre = skyAt(w, c);
    out.centre = observed(centre, o);
    int i = 0;
    for (Pixel p :
         {Pixel{1, 1}, Pixel{double(w.width), 1}, Pixel{double(w.width), double(w.height)}, Pixel{1, double(w.height)}})
        out.corners[i++] = observed(skyAt(w, p), o);
    // Project geometric (unrefracted) terrestrial north; refraction at horizon is unreliable.
    double ra, dec;
    auto vacuum = o;
    vacuum.pressure = 0;
    check(eraAtoc13("A", 0, std::numbers::pi / 2, o.utc1, o.utc2, o.dut1, o.longitude * r, o.latitude * r, o.height, 0,
                    0, 0, o.temperature, o.humidity, .55, &ra, &dec));
    out.northHorizon = pixelAt(w, {ra / r, dec / r});
    out.pole = celestialPole(w, o);
    // Local gravity projected at the optical direction gives an independent orientation consistency check.
    check(eraAtoc13("A", out.centre.azimuth * r, (90 - out.centre.altitude - .01) * r, o.utc1, o.utc2, o.dut1,
                    o.longitude * r, o.latitude * r, o.height, 0, 0, o.pressure, o.temperature, o.humidity, .55, &ra,
                    &dec));
    auto up = pixelAt(w, {wrap(ra / r), dec / r});
    if (!up) throw std::runtime_error("Vertical projection unavailable");
    out.measuredRoll = std::atan2(up->x - c.x, up->y - c.y) / r;
    if (std::abs(angle(out.measuredRoll - o.roll)) > o.rollTolerance + o.levelingSigma * 3)
        throw std::invalid_argument("Solved orientation contradicts section 3.6 landscape/level declaration");
    auto north = out.northHorizon;
    if (north) out.northBearingInImage = std::atan2(north->x - c.x, north->y - c.y) / r;
    // Propagate local sensitivity, including circular azimuth and the zenith singularity.
    double geometry = std::max(.001, std::abs(std::cos(out.centre.altitude * r)));
    double variance = (o.levelingSigma * o.levelingSigma + o.solveSigma * o.solveSigma) / (geometry * geometry);
    auto sensitivity = [&](Observation plus, Observation minus) {
        double delta = angle(observed(centre, plus).azimuth - observed(centre, minus).azimuth) * .5;
        variance += delta * delta;
    };
    auto plus = o, minus = o;
    plus.utc2 += o.timeSigma / 86400;
    minus.utc2 -= o.timeSigma / 86400;
    sensitivity(plus, minus);
    plus = minus = o;
    plus.latitude += o.latitudeSigma;
    minus.latitude -= o.latitudeSigma;
    sensitivity(plus, minus);
    plus = minus = o;
    plus.longitude += o.longitudeSigma;
    minus.longitude -= o.longitudeSigma;
    sensitivity(plus, minus);
    out.uncertaintyDegrees = std::sqrt(variance);
    out.warnings =
        "Physical section 3.6 compliance is user-declared, not measured. DUT1/polar motion and atmosphere limit "
        "accuracy. Uncertainty is a local estimate, not a certification.";
    if (out.centre.altitude < 10) out.warnings += " Low altitude: strong atmospheric uncertainty.";
    if (out.centre.altitude > 89) out.warnings += " Near zenith: azimuth is ill-conditioned.";
    if (!north) out.warnings += " North horizon is outside the TAN projection hemisphere; no finite pixel.";
    return out;
}
}  // namespace astro::north
