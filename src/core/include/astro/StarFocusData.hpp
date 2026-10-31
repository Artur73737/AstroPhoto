#pragma once
#include <cstdint>
#include <array>
#include <string>
#include <vector>
namespace astro {
struct StarFocusOptions {
    int step{1},positions{9},framesPerPoint{5},minStars{5},settleMs{600},backlash{2};
    int maxMoves{600},timeoutSeconds{600};
    bool searchEnabled{true};
    int searchStep{3},searchSpan{16},maxPasses{8},maxStride{16};
    double detectionSnr{6},matchRadius{8},maxScatter{.12},verificationTolerance{.10};
    double fitMinRSquared{.85},fitMaxUncertainty{1};
    int homingMaxMoves{160},homingStablePoints{5},homingProbeMoves{2};
    double homingNoiseFactor{4},homingMinChange{.002},homingMinContrast{1};
    int smallFramesPerPoint{11},smallSettleTimeoutMs{5000},backlashProbeLimit{16};
    bool autoBacklash{true},allowPrecisionWarning{true};
    double smallResponseSigma{3},smallStabilityFraction{.01};
    int acquisitionNearMoves{20},acquisitionFarMoves{40};
};
void validateStarFocus(const StarFocusOptions&);
struct StarSample {double x{},y{},hfd{},flux{},snr{},roundness{},peak{},background{};};
struct StarField {
    int width{},height{},saturated{},rejected{},acquisitionCandidates{};
    double background{},noise{},clippedFraction{};
    std::vector<StarSample> stars;
    // Normalized spatial signal, independent of stellar detections; used only
    // for optical travel calibration, never as a stellar sharpness metric.
    std::vector<double> focusSignature;
    double focusContrast{};
};
enum class StarFocusPhase {Idle,Homing,Baseline,Searching,Reanchoring,Moving,Sampling,Returning,Verifying,Complete,Failed,Cancelled,Warning};
struct StarFocusPoint {int position{};double hfd{},scatter{};};
struct FocusObservation {
    int position{},motorStep{},commands{},direction{},frames{},elapsedMs{};
    double hfd{},noise{},delta{},threshold{};std::string purpose;
};
struct StarFocusProgress {
    StarFocusPhase phase{StarFocusPhase::Idle};
    int position{},moves{},frames{},stars{};
    int searchMoves{},pass{},stride{1},motorStep{1},missingPoints{};
    int pairedStars{};bool bracketed{},restored{};
    // Verified best at each completed tier: large, medium, small. Zero = pending.
    std::array<double,3> stageBest{};
    double baseline{},current{},best{},verified{},curveR2{};
    bool fitValid{};double fitMinimum{},fitUncertainty{};
    std::array<double,4> fitParameters{};
    std::string detail;
    std::vector<StarFocusPoint> curve;
    int homingMoves{},homingLeg{},coarseRange{};bool nearLimitKnown{},farLimitKnown{},homed{};
    double travelChange{},travelThreshold{};
    int requiredFrames{},settledMs{},backlashNear{},backlashFar{};
    bool backlashRepeatable{},backlashReturnVerified{};
    double stationaryNoise{},responsePerCommand{},responseThreshold{};
    std::string smallDiagnostic;
    std::uint64_t observationRevision{};
    std::vector<FocusObservation> observations;
    bool surveyUsed{},surveyReturnVerified{};
    int surveyMoves{},surveyPosition{},surveyBestPosition{},surveyBestStars{};
    double surveyBestHfd{};
    bool active() const {return phase>=StarFocusPhase::Homing&&phase<=StarFocusPhase::Verifying;}
};
}
