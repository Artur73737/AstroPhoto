#include <astro/features/true_north/astronomy/TrueNorth.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <erfa.h>
#include <numbers>
using namespace astro::north;
TEST(TrueNorth, WcsRoundTripOffCentreAndOutsideFrame) {
    Wcs w{500, 500, 25, 88.5, {-.002, 0, 0, .002}, 1000, 1000};
    for (Pixel p : {Pixel{1, 1}, Pixel{250, 800}, Pixel{-800, 1600}}) {
        auto q = pixelAt(w, skyAt(w, p));
        ASSERT_TRUE(q);
        EXPECT_NEAR(q->x, p.x, 1e-7);
        EXPECT_NEAR(q->y, p.y, 1e-7);
    }
    EXPECT_FALSE(pixelAt(w, {205, -88.5}));
}
TEST(TrueNorth, PhysicalContractAndUtcAreMandatory) {
    Observation o;
    utc(o, "2026-11-22T20:00:00Z");
    Wcs w{500, 500, 25, 88.5, {-.002, 0, 0, .002}, 1000, 1000};
    EXPECT_THROW(validate(w, o), std::invalid_argument);
    o.physicalContractConfirmed = o.rigid = true;
    EXPECT_NO_THROW(validate(w, o));
    o.mirrored = true;
    EXPECT_THROW(validate(w, o), std::invalid_argument);
    EXPECT_THROW(utc(o, "2026-11-22T20:00Z"), std::invalid_argument);
    EXPECT_THROW(utc(o, "2026-02-30T20:00:00Z"), std::invalid_argument);
}
TEST(TrueNorth, ObservedCoordinatesStayFiniteForSouthernReferenceSite) {
    Observation o;
    o.utc1 = 2456384.5;
    o.utc2 = .969254051;
    o.dut1 = .1550675;
    o.longitude = -.527800806 * 180 / 3.141592653589793;
    o.latitude = -1.2345856 * 180 / 3.141592653589793;
    o.height = 2738;
    o.pressure = 731;
    o.temperature = 12.8;
    o.humidity = .59;
    auto h = observed({2.71 * 180 / 3.141592653589793, .174 * 180 / 3.141592653589793}, o);
    EXPECT_TRUE(std::isfinite(h.azimuth));
    EXPECT_GE(h.azimuth, 0);
    EXPECT_LT(h.azimuth, 360);
}

TEST(TrueNorth, LevelLandscapeRecoversKnownTerrestrialAzimuthAtEveryRequestedFieldSize) {
    constexpr double rad = std::numbers::pi / 180.;
    Observation o;
    utc(o, "2026-11-22T20:00:00Z");
    o.latitude = 46;
    o.longitude = 11;
    o.height = 300;
    o.pressure = 0;
    o.physicalContractConfirmed = o.rigid = true;
    double ra, dec;
    ASSERT_GE(eraAtoc13("A", 12 * rad, 45 * rad, o.utc1, o.utc2, 0, o.longitude * rad, o.latitude * rad, o.height, 0, 0,
                        0, 10, .5, .55, &ra, &dec),
              0);
    for (double fov : {1., 2., 3.}) {
        double scale = fov / 1000;
        Wcs w{500.5, 500.5, ra / rad, dec / rad, {-scale, 0, 0, scale}, 1000, 1000};
        double ur, ud;
        eraAtoc13("A", 12 * rad, 44.99 * rad, o.utc1, o.utc2, 0, o.longitude * rad, o.latitude * rad, o.height, 0, 0, 0,
                  10, .5, .55, &ur, &ud);
        auto up = pixelAt(w, {ur / rad, ud / rad});
        ASSERT_TRUE(up);
        double theta = std::atan2(up->x - 500.5, up->y - 500.5);
        w.cd = {-scale * std::cos(theta), -scale * std::sin(theta), -scale * std::sin(theta), scale * std::cos(theta)};
        auto result = calculate(w, o);
        EXPECT_NEAR(result.centre.azimuth, 12, 1e-5);
        EXPECT_NEAR(result.centre.altitude, 45, 1e-5);
        EXPECT_NEAR(result.measuredRoll, 0, .001);
        EXPECT_TRUE(result.pole);
        EXPECT_GE(result.uncertaintyDegrees, .1);
        w.cd[1] *= -1;
        w.cd[2] *= -1;
        EXPECT_THROW(calculate(w, o), std::invalid_argument);
    }
}

TEST(TrueNorth, PlateOnlyPoleNeedsNoInventedPhysicalObserverAndUsesSolvedFov) {
    Observation epoch;utc(epoch,"2026-11-22T00:00:00Z");
    Wcs a{1000.5,450.5,0,89.5,{-.0025,0,0,.0025},2000,900};
    Wcs b=a;b.width=800;b.height=500;b.crpix1=400.5;b.crpix2=250.5;
    auto fa=fieldOfView(a),fb=fieldOfView(b);
    EXPECT_GT(fa[0],fb[0]);EXPECT_GT(fa[1],fb[1]);
    EXPECT_NEAR(fa[1],900*.0025,.001);
    EXPECT_TRUE(celestialPole(a,epoch));
    EXPECT_THROW(calculate(a,epoch),std::invalid_argument);
}
