#pragma once
#include <array>
#include <optional>
#include <string>
namespace astro::north {
struct Pixel {
    double x{}, y{};
};
struct Sky {
    double ra{}, dec{};
};
struct Horizontal {
    double azimuth{}, altitude{};
};
// FITS convention: one-based pixel centres, x right and y up. UI y must be flipped.
struct Wcs {
    double crpix1{}, crpix2{}, ra{}, dec{};
    std::array<double, 4> cd{};
    int width{}, height{};
};
struct Observation {
    double latitude{}, longitude{}, height{}, utc1{}, utc2{}, dut1{}, pressure{1013.25}, temperature{10}, humidity{.5};
    double levelingSigma{.1}, timeSigma{1}, latitudeSigma{1. / 3600}, longitudeSigma{1. / 3600}, solveSigma{10. / 3600},
        roll{}, rollTolerance{.1};
    bool physicalContractConfirmed{}, rigid{}, mirrored{}, inverted{};
};
struct Result {
    Horizontal centre;
    std::array<Horizontal, 4> corners;
    std::optional<Pixel> northHorizon, pole;
    double northBearingInImage{}, uncertaintyDegrees{}, measuredRoll{};
    std::string warnings;
};
// Pole projection requires an epoch, but no assumed terrestrial observer or level declaration.
std::optional<Pixel> celestialPole(const Wcs&, const Observation& epoch);
std::array<double, 2> fieldOfView(const Wcs&); // great-circle width/height in degrees
void validate(const Wcs&, const Observation&);
void utc(Observation&, const std::string& iso8601MidExposure);
Sky skyAt(const Wcs&, Pixel);
std::optional<Pixel> pixelAt(const Wcs&, Sky);
Horizontal observed(Sky, const Observation&);
Result calculate(const Wcs&, const Observation&);
}  // namespace astro::north
