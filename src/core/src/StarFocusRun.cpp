#include <astro/StarFocusRun.hpp>
#include <algorithm>
namespace astro {
void StarFocusRun::start(const StarFocusOptions& options){
    if(progress_.active())throw std::logic_error("Stellar autofocus already running");
    focus_.cancel();options_=options;deadline_=clock_.steadyNow()+std::chrono::seconds(options.timeoutSeconds);
    homing_.start(options);preparing_=true;limitsValid_=true;limitFrames_=0;revision_=0;publish();
}
void StarFocusRun::publish(){
    const auto& home=homing_.progress();progress_=preparing_?home:focus_.progress();
    progress_.homingMoves=home.moves;progress_.homingLeg=home.homingLeg;progress_.homed=home.homed;
    progress_.nearLimitKnown=home.nearLimitKnown;progress_.farLimitKnown=home.farLimitKnown;progress_.coarseRange=home.coarseRange;
    progress_.travelChange=home.travelChange;progress_.travelThreshold=home.travelThreshold;
    if(!preparing_)progress_.moves+=home.moves;
    if(!limitsValid_){progress_.nearLimitKnown=progress_.farLimitKnown=progress_.homed=false;progress_.coarseRange=0;}
}
void StarFocusRun::invalidateLimits(){limitsValid_=false;publish();}
void StarFocusRun::fail(const std::string& detail){if(preparing_)homing_.fail(detail);else focus_.fail(detail);limitsValid_=false;publish();}
void StarFocusRun::cancel(){if(preparing_)homing_.cancel();else focus_.cancel();publish();}
void StarFocusRun::tick(const StarField* field,std::uint64_t revision){
    if(!progress_.active())return;
    if(clock_.steadyNow()>=deadline_){fail("Autofocus time limit reached");return;}
    if(preparing_){
        homing_.tick(field,revision);
        if(homing_.progress().phase==StarFocusPhase::Complete){
            auto remaining=options_;remaining.maxMoves-=homing_.progress().moves;
            remaining.timeoutSeconds=static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(deadline_-clock_.steadyNow()).count());
            if(remaining.maxMoves<20||remaining.timeoutSeconds<30){fail("Insufficient remaining budget after travel calibration");return;}
            try{focus_.start(remaining,homing_.progress().coarseRange);preparing_=false;}catch(const std::exception& e){homing_.fail(e.what());}
        }
    }else{
        const auto& p=focus_.progress();
        if(field&&revision!=revision_&&p.motorStep<3&&!field->focusSignature.empty()){
            revision_=revision;
            const auto close=[&](const FocusLimitObservation& end){return focusSignatureDistance(field->focusSignature,end.signal)<=std::max(options_.homingMinChange,options_.homingNoiseFactor*end.noise);};
            limitFrames_=(close(homing_.nearLimit())||close(homing_.farLimit()))?limitFrames_+1:0;
            if(limitFrames_>=options_.homingStablePoints){fail("Calibrated optical travel boundary reached during refinement");return;}
        }
        focus_.tick(field,revision);
    }
    publish();
}
}
