#pragma once
#include <astro/Core.hpp>
#include <astro/HocusFocusMath.hpp>
namespace astro {
// Relative Canon moves are commands, not calibrated physical positions.
class StarAutofocus {
public:
    StarAutofocus(ICamera& camera,IClock& clock):camera_(camera),clock_(clock){}
    void start(const StarFocusOptions&);
    void tick(const StarField* field=nullptr,std::uint64_t revision=0);
    void cancel();
    void fail(const std::string&);
    const StarFocusProgress& progress() const {return progress_;}
    const std::vector<StarSample>& trackingTargets() const {return cohort_;}
private:
    struct Value {std::size_t id{};double hfd{},scatter{};};
    struct Measurement {int position{};std::vector<Value> values;};
    struct Comparison {double current{},reference{},delta{},noise{};int stars{};};
    std::optional<Comparison> compare(const Measurement&,const Measurement&) const;
    void alignStars(const StarField&);
    void sample(const StarField&);
    void completePoint(Measurement);
    void travel(int target,StarFocusPhase after,bool approach);
    void beginSamples(StarFocusPhase);
    void nextSearchPosition();
    void beginScan(Measurement center,int stride);
    void nextScanPosition(bool stopSide);
    void evaluateCurve();
    void changeMotorStep();
    void missingPoint();
    void restoreBest(const std::string&);
    void record(const Measurement&);
    hocus::Fit fitCurve(bool stability);
    bool probe(int target);
    int overshoot() const;
    ICamera& camera_;IClock& clock_;StarFocusOptions options_;
    StarFocusProgress progress_;
    StarFocusPhase afterMove_{};
    SteadyTime deadline_{},ready_{},sampleDeadline_{};
    std::deque<int> moves_;
    std::vector<StarSample> cohort_;
    std::vector<StarField> warmup_;
    std::vector<std::vector<double>> samples_;
    std::vector<int> lastSeen_;
    Measurement baseline_,best_,reference_;
    std::array<Measurement,3> stageReferences_;
    std::vector<Measurement> measured_;
    std::string restoreReason_;
    int discard_{},width_{},height_{};std::uint64_t lastRevision_{};
    int seenFrames_{},searchPosition_{},searchDirection_{1},scanCenter_{},direction_{-1},sidePoints_{},lastDirection_{};
    bool searching_{},bracketed_{},forwardFresh_{};
};
}
