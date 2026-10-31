#pragma once
#include <astro/StarAutofocus.hpp>
#include <astro/FocusHoming.hpp>
namespace astro {
// Production entry point: every run recalibrates limits and resets near first.
class StarFocusRun {
public:
    StarFocusRun(ICamera& camera,IClock& clock):clock_(clock),homing_(camera,clock),focus_(camera,clock){}
    void start(const StarFocusOptions&);
    void tick(const StarField* field=nullptr,std::uint64_t revision=0);
    void cancel();void fail(const std::string&);
    void invalidateLimits();
    const StarFocusProgress& progress()const{return progress_;}
    const std::vector<StarSample>& trackingTargets()const{return preparing_?empty_:focus_.trackingTargets();}
private:
    void publish();IClock& clock_;FocusHoming homing_;StarAutofocus focus_;StarFocusOptions options_;
    StarFocusProgress progress_;SteadyTime deadline_{};bool preparing_{},limitsValid_{};int limitFrames_{};std::uint64_t revision_{};
    std::vector<StarSample> empty_;
};
}
