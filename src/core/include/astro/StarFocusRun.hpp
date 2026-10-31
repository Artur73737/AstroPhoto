#pragma once
#include <astro/StarAutofocus.hpp>
#include <astro/FocusHoming.hpp>
#include <astro/FocusSurvey.hpp>
namespace astro {
// Recalibrate limits on every run, or survey Near/Far with unknown limits when
// the optical signal cannot establish them. Always verify the acquisition return.
class StarFocusRun {
public:
    StarFocusRun(ICamera& camera,IClock& clock):clock_(clock),homing_(camera,clock),survey_(camera,clock),focus_(camera,clock){}
    void start(const StarFocusOptions&);
    void tick(const StarField* field=nullptr,std::uint64_t revision=0);
    void cancel();void fail(const std::string&);
    void invalidateLimits();
    const StarFocusProgress& progress()const{return progress_;}
    const std::vector<StarSample>& trackingTargets()const{return surveying_?survey_.trackingTargets():preparing_?empty_:focus_.trackingTargets();}
private:
    void publish();void startRefinement();IClock& clock_;FocusHoming homing_;FocusSurvey survey_;StarAutofocus focus_;StarFocusOptions options_;
    StarFocusProgress progress_;SteadyTime deadline_{};bool preparing_{},limitsValid_{};int limitFrames_{};std::uint64_t revision_{};
    std::vector<StarSample> empty_;
    bool surveying_{},surveyUsed_{};
};
}
