#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace astro {
struct StarFocusOptions {
    int step{1},positions{9},framesPerPoint{5},minStars{5},settleMs{600},backlash{2};
    int maxMoves{200},timeoutSeconds{600};
    bool searchEnabled{true};
    int searchStep{3},searchSpan{16},maxPasses{6},maxStride{8};
    double detectionSnr{6},matchRadius{8},maxScatter{.12},verificationTolerance{.10};
};
void validateStarFocus(const StarFocusOptions&);
struct StarSample {double x{},y{},hfd{},flux{},snr{},roundness{},peak{},background{};};
struct StarField {
    int width{},height{},saturated{},rejected{},acquisitionCandidates{};
    double background{},noise{},clippedFraction{};
    std::vector<StarSample> stars;
};
enum class StarFocusPhase {Idle,Baseline,Searching,Reanchoring,Moving,Sampling,Returning,Verifying,Complete,Failed,Cancelled};
struct StarFocusPoint {int position{};double hfd{},scatter{};};
struct StarFocusProgress {
    StarFocusPhase phase{StarFocusPhase::Idle};
    int position{},moves{},frames{},stars{};
    int searchMoves{},pass{},stride{1},motorStep{1},missingPoints{};
    double baseline{},current{},best{},verified{},curveR2{};
    std::string detail;
    std::vector<StarFocusPoint> curve;
    bool active() const {return phase>=StarFocusPhase::Baseline&&phase<=StarFocusPhase::Verifying;}
};
}
