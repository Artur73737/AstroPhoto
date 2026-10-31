#pragma once
#include <astro/Core.hpp>
namespace astro {
// Bounded, measured fallback when an optical endpoint cannot be established.
// Command counts are NOT physical lens positions or proof of a travel limit.
class FocusSurvey {
public:
    FocusSurvey(ICamera& c,IClock& t):camera_(c),clock_(t){}
    void start(const StarFocusOptions&);
    void tick(const StarField*,std::uint64_t);
    void cancel();void fail(const std::string&);
    const StarFocusProgress& progress()const{return progress_;}
    const std::vector<StarSample>& trackingTargets()const{return best_.stars;}
private:
    enum class Leg{Near,Far,Return,Approach,Verify};Leg leg_{};
    void move(int);void point();void sample(const StarField&);
    ICamera& camera_;IClock& clock_;StarFocusOptions options_;StarFocusProgress progress_;
    SteadyTime deadline_{},ready_{},lastFrameAt_{};std::uint64_t revision_{},moveRevision_{};
    int legMoves_{},discard_{},pendingMove_{},returnMoves_{},width_{},height_{};bool moving_{};
    std::vector<StarField> samples_;StarField best_;
};
}
