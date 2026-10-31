#include <astro/FocusHoming.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
namespace astro {
double focusSignatureDistance(const std::vector<double>& a,const std::vector<double>& b){
    if(a.empty()||a.size()!=b.size())return std::numeric_limits<double>::infinity();
    double sum=0;for(std::size_t i=0;i<a.size();++i){if(!std::isfinite(a[i])||!std::isfinite(b[i]))return std::numeric_limits<double>::infinity();sum+=(a[i]-b[i])*(a[i]-b[i]);}
    return std::sqrt(sum/a.size());
}
void FocusHoming::start(const StarFocusOptions& o){
    validateStarFocus(o);if(!camera_.connected()||camera_.lensFocusStatus().state!=LensFocusState::Ready||!camera_.manualExposureForFocus())throw CameraError("AF lens, M and fixed exposure required for travel calibration");
    options_=o;progress_={};progress_.phase=StarFocusPhase::Homing;progress_.motorStep=3;
    progress_.detail="Calibrating near travel limit";mode_=Mode::Seek;stable_=legMoves_=width_=height_=recalibrations_=0;revision_=moveRevision_=0;lastFrameAt_=clock_.steadyNow();
    near_={};far_={};previous_={};candidate_={};moves_.clear();deadline_=clock_.steadyNow()+std::chrono::seconds(o.timeoutSeconds);beginSamples();
}
void FocusHoming::fail(const std::string& reason){moves_.clear();progress_.phase=StarFocusPhase::Failed;progress_.detail=reason;progress_.homed=progress_.nearLimitKnown=progress_.farLimitKnown=false;progress_.coarseRange=0;}
void FocusHoming::cancel(){if(progress_.active()){moves_.clear();progress_.phase=StarFocusPhase::Cancelled;progress_.detail="Travel calibration cancelled; limits invalidated";progress_.nearLimitKnown=progress_.farLimitKnown=progress_.homed=false;}}
void FocusHoming::beginSamples(){samples_.clear();discard_=2;progress_.frames=0;ready_=clock_.steadyNow()+std::chrono::milliseconds(options_.settleMs);sampleDeadline_=ready_+std::chrono::seconds(12);}
void FocusHoming::move(int direction,int count){
    if(progress_.moves+count>std::min(options_.maxMoves,options_.homingMaxMoves)){fail("Travel calibration movement limit reached; endpoint unconfirmed");return;}
    for(int i=0;i<count;++i)moves_.push_back(direction);ready_=clock_.steadyNow();
}
double FocusHoming::threshold(const FocusLimitObservation& a,const FocusLimitObservation& b)const{return std::max(options_.homingMinChange,options_.homingNoiseFactor*std::hypot(a.noise,b.noise));}
void FocusHoming::tick(const StarField* field,std::uint64_t revision){
    if(!progress_.active())return;
    try{
        const auto now=clock_.steadyNow();if(now>=deadline_){fail("Travel calibration timeout; endpoint unconfirmed");return;}
        if(!camera_.connected()){fail("Camera disconnected during travel calibration");return;}
        const bool fresh=field&&revision!=revision_;
        if(fresh){revision_=revision;lastFrameAt_=now;}
        if(now-lastFrameAt_>=std::chrono::seconds(12)){fail("No fresh Live View frames during travel calibration");return;}
        if(!moves_.empty()){
            if(now<ready_||moveRevision_==revision_)return;const auto direction=moves_.front();
            if(camera_.lensFocusStatus().state!=LensFocusState::Ready||!camera_.manualExposureForFocus()){fail("Camera or AF lens unavailable during travel calibration");return;}
            camera_.focus(direction*3);moveRevision_=revision_;moves_.pop_front();++progress_.moves;progress_.homingMoves=progress_.moves;
            if(mode_==Mode::Seek)++legMoves_;
            if(moves_.empty())beginSamples();else ready_=now+std::chrono::milliseconds(options_.settleMs);return;
        }
        if(now>=sampleDeadline_){fail("No stable image signal for travel calibration");return;}
        if(!fresh||now<ready_)return;if(discard_){--discard_;return;}sample(*field);
    }catch(const std::exception& e){fail(e.what());}
}
void FocusHoming::sample(const StarField& field){
    if(width_&&(width_!=field.width||height_!=field.height)){fail("Live View geometry changed during travel calibration");return;}
    width_=field.width;height_=field.height;
    if(!std::isfinite(field.focusContrast)||field.focusContrast<options_.homingMinContrast||field.focusSignature.empty()||!std::isfinite(focusSignatureDistance(field.focusSignature,field.focusSignature)))return;
    if(!samples_.empty()&&samples_.front().size()!=field.focusSignature.size()){fail("Travel calibration signal geometry changed");return;}
    samples_.push_back(field.focusSignature);progress_.frames=static_cast<int>(samples_.size());
    if(samples_.size()<static_cast<std::size_t>(options_.framesPerPoint))return;
    FocusLimitObservation result;result.signal.resize(field.focusSignature.size());
    for(std::size_t i=0;i<result.signal.size();++i){std::vector<double> values;for(const auto& frame:samples_)values.push_back(frame[i]);std::sort(values.begin(),values.end());result.signal[i]=values[values.size()/2];}
    std::vector<double> residuals;for(const auto& frame:samples_)residuals.push_back(focusSignatureDistance(frame,result.signal));std::sort(residuals.begin(),residuals.end());
    // Compare two temporal median images, not two individual noisy frames.
    // Median efficiency is 1.253; count only half the samples as independent
    // to conservatively account for adjacent JPEG-frame correlation.
    result.noise=residuals[residuals.size()/2]*1.253/std::sqrt(std::max(1.,samples_.size()/2.));
    point(std::move(result));
}
void FocusHoming::point(FocusLimitObservation current){
    const int direction=progress_.homingLeg==1?1:-1;
    if(previous_.signal.empty()){previous_=std::move(current);move(direction,1);return;}
    progress_.travelChange=focusSignatureDistance(current.signal,previous_.signal);progress_.travelThreshold=threshold(current,previous_);
    if(mode_==Mode::Seek){
        if(stable_==0)plateau_=previous_;
        // Slow cumulative motion is not a plateau, even when each individual
        // change falls below the noise threshold.
        const bool stationary=progress_.travelChange<=progress_.travelThreshold&&focusSignatureDistance(current.signal,plateau_.signal)<=threshold(current,plateau_);
        stable_=stationary?stable_+1:0;
        previous_=current;
        if(stable_<options_.homingStablePoints){move(direction,1);return;}
        candidate_=std::move(current);mode_=Mode::ReverseProbe;
        progress_.detail="Checking reverse movement before accepting an optical limit";
        move(-direction,options_.homingProbeMoves);return;
    }
    const double difference=focusSignatureDistance(current.signal,candidate_.signal),limit=threshold(current,candidate_);
    progress_.travelChange=difference;progress_.travelThreshold=limit;
    if(mode_==Mode::ReverseProbe){
        if(difference<=limit*2){fail("Lens response not observable on reverse probe; travel limit unknown");return;}
        mode_=Mode::ConfirmReturn;progress_.detail="Verifying return to the optical travel limit";
        move(direction,options_.homingProbeMoves+options_.homingStablePoints);return;
    }
    if(difference>limit){
        // The first apparent plateau was premature. Continue seeking in the
        // same direction, within the original time and movement budgets.
        mode_=Mode::Seek;stable_=0;previous_=std::move(current);
        progress_.detail="Premature plateau: continuing travel calibration";move(direction,1);return;
    }
    if(progress_.homingLeg==0){near_=current;progress_.nearLimitKnown=true;progress_.homingLeg=1;progress_.detail="Calibrating far travel limit";}
    else if(progress_.homingLeg==1){
        const int range=legMoves_-options_.homingStablePoints;
        if(range<2){fail("Travel range is not measurable");return;}
        far_=current;progress_.farLimitKnown=true;progress_.coarseRange=range;progress_.homingLeg=2;progress_.detail="Resetting lens to calibrated near limit";
    }else{
        const auto repeatDifference=focusSignatureDistance(current.signal,near_.signal),repeatLimit=threshold(current,near_);
        if(repeatDifference>repeatLimit*2){
            progress_.travelChange=repeatDifference;progress_.travelThreshold=repeatLimit;
            if(recalibrations_++==0){
                near_=current;far_={};progress_.farLimitKnown=false;progress_.coarseRange=0;progress_.homingLeg=1;
                previous_=std::move(current);mode_=Mode::Seek;stable_=legMoves_=0;
                progress_.detail="Near limit changed: repeating full travel verification";move(1,1);return;
            }
            fail("Near limit changed during calibration; scene or lens moved");return;
        }
        near_=current;progress_.homed=true;progress_.phase=StarFocusPhase::Complete;progress_.detail="Optical travel limits calibrated; lens reset near";return;
    }
    previous_=std::move(current);mode_=Mode::Seek;stable_=legMoves_=0;move(progress_.homingLeg==1?1:-1,1);
}
}
