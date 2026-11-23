#include <astro/features/true_north/astronomy/TrueNorth.hpp>
#include <astro/stacking/astrometry/PlateSolve.hpp>
#include <gtest/gtest.h>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <erfa.h>
#include <numbers>
using namespace astro;
class NorthDss : public testing::TestWithParam<int> {};
TEST_P(NorthDss, RealDss2FieldSolvesAndRetainsOffCentrePole) {
    auto root = std::getenv("ASTRO_NORTH_DSS_DIRECTORY");
    auto executable = std::getenv("ASTRO_ASTAP_EXE");
    auto database = std::getenv("ASTRO_ASTAP_DATABASE");
    if (!root || !executable || !database) GTEST_SKIP() << "Opt-in genuine DSS2 FITS plus real ASTAP/catalog";
    int degrees = GetParam();
    auto image = std::filesystem::path(root) / ("pole-offset-" + std::to_string(degrees) + ".fits");
    std::ifstream input(image, std::ios::binary);
    char signature[7]{};
    input.read(signature, 6);
    ASSERT_EQ(std::string(signature), "SIMPLE");
    StackOptions options;
    options.solverExecutable = executable;
    options.solverDatabase = database;
    options.solverFov = degrees;
    auto solve = stacking::solvePlate(image, options);
    ASSERT_TRUE(solve.solved) << solve.detail;
    ASSERT_GT(solve.width, 500);
    ASSERT_GT(solve.height, 500);
    auto& m = solve.wcs;
    north::Wcs w{m.at("CRPIX1"),
                 m.at("CRPIX2"),
                 m.at("CRVAL1"),
                 m.at("CRVAL2"),
                 {m.at("CD1_1"), m.at("CD1_2"), m.at("CD2_1"), m.at("CD2_2")},
                 solve.width,
                 solve.height};
    auto centre = north::skyAt(w, {(w.width + 1) * .5, (w.height + 1) * .5});
    EXPECT_NEAR(centre.dec, 88.7, .02);
    EXPECT_LT(std::abs(std::remainder(centre.ra - 40., 360.)) * std::cos(88.7 * 3.141592653589793 / 180), .02);
    auto pole = north::pixelAt(w, {0, 90});
    ASSERT_TRUE(pole);
    EXPECT_GT(std::hypot(pole->x - (w.width + 1) * .5, pole->y - (w.height + 1) * .5), w.height * .25);
    north::Observation o;
    north::utc(o, "2026-11-22T20:00:00Z");
    EXPECT_THROW(north::calculate(w, o), std::invalid_argument);
    // DSS has no capture-site/gravity contract. This observer is explicitly a
    // numerical fixture chosen to make the archive image upright, never field evidence.
    constexpr double rad = std::numbers::pi / 180.;
    o.latitude = 46;
    o.height = 300;
    o.pressure = 0;
    auto rollAt = [&](double longitude) {
        auto trial = o;
        trial.longitude = longitude;
        auto h = north::observed(centre, trial);
        double ra, dec;
        eraAtoc13("A", h.azimuth * rad, (90 - h.altitude - .01) * rad, trial.utc1, trial.utc2, 0, longitude * rad,
                  trial.latitude * rad, trial.height, 0, 0, 0, 10, .5, .55, &ra, &dec);
        auto up = north::pixelAt(w, {ra / rad, dec / rad});
        return up ? std::abs(std::atan2(up->x - (w.width + 1) * .5, up->y - (w.height + 1) * .5) / rad) : 180.;
    };
    double best = 0, score = 180;
    for (double lon = -179; lon < 179; lon += .5) {
        double candidate = rollAt(lon);
        if (candidate < score) {
            best = lon;
            score = candidate;
        }
    }
    for (double step = .1; step > 1e-7; step *= .1) {
        for (int n = -10; n <= 10; ++n) {
            double candidate = std::clamp(best + n * step, -179.99, 179.99), value = rollAt(candidate);
            if (value < score) {
                best = candidate;
                score = value;
            }
        }
    }
    ASSERT_LT(score, .05);
    o.longitude = best;
    o.physicalContractConfirmed = o.rigid = true;
    auto numerical = north::calculate(w, o);
    EXPECT_TRUE(std::isfinite(numerical.centre.azimuth));
    EXPECT_TRUE(numerical.pole);
    EXPECT_GT(numerical.uncertaintyDegrees, 0);
    std::cout << "Synthetic observer fixture only: longitude " << best << ", azimuth " << numerical.centre.azimuth
              << ", roll " << numerical.measuredRoll << "\n";

    std::cout << "DSS2 " << degrees << " deg: " << w.width << "x" << w.height << ", RA " << centre.ra << ", Dec "
              << centre.dec << ", J2000 pole pixel " << pole->x << "," << pole->y
              << ". Physical compass contract deliberately NOT asserted for archive data.\n";
}
INSTANTIATE_TEST_SUITE_P(FieldsOneToThreeDegrees, NorthDss, testing::Values(1, 2, 3));
