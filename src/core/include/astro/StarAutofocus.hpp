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
    ICamera& camera_;IClock& clock_;StarFocusOptions options_;
    StarFocusProgress progress_;
    StarFocusPhase afterMove_{};
    SteadyTime deadline_{},ready_{},sampleDeadline_{};
    std::deque<int> moves_;
    std::vector<StarSample> cohort_;
    std::vector<StarField> warmup_;
    std::vector<double> samples_;
    int discard_{},width_{},height_{};std::uint64_t lastRevision_{};
};
}
