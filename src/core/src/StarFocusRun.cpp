#include <astro/StarFocusRun.hpp>
#include <algorithm>
namespace astro {
void StarFocusRun::start(const StarFocusOptions& options){
    if(progress_.active())throw std::logic_error("Stellar autofocus already running");
    focus_.cancel();options_=options;deadline_=clock_.steadyNow()+std::chrono::seconds(options.timeoutSeconds);
    survey_.cancel();surveying_=surveyUsed_=false;homing_.start(options);preparing_=true;limitsValid_=true;limitFrames_=0;revision_=0;publish();
}
void StarFocusRun::publish(){
    const auto& home=homing_.progress();progress_=surveying_?survey_.progress():preparing_?home:focus_.progress();
    progress_.homingMoves=home.moves;progress_.homingLeg=home.homingLeg;progress_.homed=home.homed;
    progress_.nearLimitKnown=home.nearLimitKnown;progress_.farLimitKnown=home.farLimitKnown;progress_.coarseRange=home.coarseRange;
    progress_.travelChange=home.travelChange;progress_.travelThreshold=home.travelThreshold;
    if(!preparing_)progress_.moves+=home.moves+(surveyUsed_?survey_.progress().moves:0);
    if(surveying_)progress_.moves+=home.moves;
    if(surveyUsed_){const auto& s=survey_.progress();progress_.surveyUsed=true;progress_.surveyMoves=s.moves;progress_.surveyPosition=s.surveyPosition;progress_.surveyBestPosition=s.surveyBestPosition;progress_.surveyBestStars=s.surveyBestStars;progress_.surveyBestHfd=s.surveyBestHfd;progress_.surveyReturnVerified=s.surveyReturnVerified;}
    if(!limitsValid_){progress_.nearLimitKnown=progress_.farLimitKnown=progress_.homed=false;progress_.coarseRange=0;}
}
void StarFocusRun::invalidateLimits(){limitsValid_=false;publish();}
void StarFocusRun::fail(const std::string& detail){if(surveying_)survey_.fail(detail);else if(preparing_)homing_.fail(detail);else focus_.fail(detail);limitsValid_=false;publish();}
void StarFocusRun::cancel(){if(surveying_)survey_.cancel();else if(preparing_)homing_.cancel();else focus_.cancel();publish();}
void StarFocusRun::startRefinement(){
    auto remaining=options_;remaining.maxMoves-=homing_.progress().moves+(surveyUsed_?survey_.progress().moves:0);
    remaining.timeoutSeconds=static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(deadline_-clock_.steadyNow()).count());
    if(remaining.maxMoves<20||remaining.timeoutSeconds<30){fail("Insufficient remaining budget after travel calibration");return;}
    try{focus_.start(remaining,surveyUsed_?-1:homing_.progress().coarseRange);preparing_=surveying_=false;}catch(const std::exception& e){fail(e.what());}
}
void StarFocusRun::tick(const StarField* field,std::uint64_t revision){
    if(!progress_.active())return;
    if(clock_.steadyNow()>=deadline_){fail("Autofocus time limit reached");return;}
    if(surveying_){survey_.tick(field,revision);if(survey_.progress().phase==StarFocusPhase::Complete)startRefinement();}
    else if(preparing_){
        homing_.tick(field,revision);
        if(homing_.progress().phase==StarFocusPhase::Complete){
            startRefinement();
        }else if(homing_.progress().phase==StarFocusPhase::Failed&&options_.searchEnabled){
            const auto& detail=homing_.progress().detail;
            if(detail=="No stable image signal for travel calibration"||detail=="Lens response not observable on reverse probe; travel limit unknown"){
                auto remaining=options_;remaining.maxMoves-=homing_.progress().moves;
                remaining.timeoutSeconds=static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(deadline_-clock_.steadyNow()).count());
                if(remaining.maxMoves>=20&&remaining.timeoutSeconds>=30){survey_.start(remaining);surveying_=surveyUsed_=true;limitsValid_=false;}
            }
        }
    }else{
        const auto& p=focus_.progress();
        if(limitsValid_&&field&&revision!=revision_&&p.motorStep<3&&!field->focusSignature.empty()){
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
