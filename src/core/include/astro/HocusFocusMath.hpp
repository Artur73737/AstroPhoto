/* Copyright (c) 2021-2026 George Hilios <ghilios+NINA@googlemail.com>
 * C++ adaptation for AstroPhoto, 2026.
 * This Source Code Form is subject to the Mozilla Public License, v. 2.0.
 * See licenses/hocus-focus/LICENSE.txt or https://mozilla.org/MPL/2.0/.
 */
#pragma once
#include <array>
#include <span>
#include <vector>
#include <limits>
namespace astro::hocus {
struct Point { double x{}, y{}, sigma{}; };
struct Fit {
    bool valid{};
    double minimum{},value{},rSquared{},reducedChiSquared{};
    double positionSigma{std::numeric_limits<double>::quiet_NaN()};
    double leaveOneOutSigma{std::numeric_limits<double>::quiet_NaN()};
    int leaveOneOutSolved{};
    std::array<double,4> parameters{};
};
std::vector<Point> regularizeWeights(std::span<const Point> points);
// Native port of the symmetric Hocus Focus model, Huber weighting, covariance
// and leave-one-out stability. Uses a bounded C++ solver instead of ALGLIB/.NET.
Fit fitHyperbola(std::span<const Point> points,bool leaveOneOut=true);
double evaluate(const Fit&,double x);
struct BackgroundPixel { double x{},y{},value{}; };
struct BackgroundPlane {
    bool valid{};double offset{},dx{},dy{},sigma{};
    double at(double x,double y) const { return offset+dx*x+dy*y; }
};
BackgroundPlane fitBackground(std::span<const BackgroundPixel> pixels);
}
