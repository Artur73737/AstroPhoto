#pragma once
#include <astro/Core.hpp>
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
private:
    void sample(const StarField&);
    void completePoint();
    void travel(int target,StarFocusPhase after,bool approach);
    void beginSamples(StarFocusPhase);
    void nextSearchPosition();
    void beginScan(int center,int stride);
    void nextScanPosition();
    void evaluateCurve();
    void adaptScan(const std::string& reason,int center,int stride);
    void changeMotorStep(int step,int target,bool refinement,int stride=1);
    ICamera& camera_;IClock& clock_;StarFocusOptions options_;
    StarFocusProgress progress_;
    StarFocusPhase afterMove_{};
    SteadyTime deadline_{},ready_{},sampleDeadline_{};
    std::deque<int> moves_;
    std::vector<StarSample> cohort_;
    std::vector<StarField> warmup_;
    std::vector<double> samples_;
    int discard_{},width_{},height_{};std::uint64_t lastRevision_{};
    int seenFrames_{},searchPosition_{},searchDirection_{1},scanCenter_{},scanEnd_{},broadFrames_{};
    bool searchRefined_{};
    int pendingStep_{},pendingStride_{1},stepCeiling_{3};bool pendingRefinement_{};
    bool searching_{},refining_{};
    double coarseBest_{};
};
}
