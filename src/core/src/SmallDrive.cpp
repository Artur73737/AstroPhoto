#include <astro/StarAutofocus.hpp>
#include <algorithm>
#include <cmath>
namespace astro {
void StarAutofocus::observe(const Measurement& m){
    const auto self=compare(m,m);if(!self)return;
    auto paired=compare(m,lastObservation_);
    if(!progress_.observations.empty()&&progress_.observations.back().motorStep!=progress_.motorStep)paired.reset();
    progress_.settledMs=static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(clock_.steadyNow()-samplingStarted_).count());
    const int commands=progress_.moves-lastObservationMoves_;
    // A return contains opposite commands. Net HFD / total command count there
    // is not a lens-response measurement; retain the last one-way estimate.
    const bool oneWay=paired&&commands>0&&std::abs(m.position-lastObservation_.position)==commands;
    const double threshold=paired?responseThreshold(*paired):0;
    progress_.responseThreshold=threshold;
    if(oneWay)progress_.responsePerCommand=paired->delta/commands;
    progress_.observations.push_back({m.position,progress_.motorStep,commands,oneWay?lastDirection_:0,progress_.frames,progress_.settledMs,self->current,self->noise/std::sqrt(2.),paired?paired->delta:0,threshold,progress_.smallDiagnostic});
    if(progress_.observations.size()>256)progress_.observations.erase(progress_.observations.begin());
    ++progress_.observationRevision;lastObservation_=m;lastObservationMoves_=progress_.moves;
}
void StarAutofocus::startSmallCalibration(Measurement center){
    measured_.clear();progress_.curve.clear();progress_.fitValid=false;progress_.curveR2=0;
    best_=smallOrigin_=center;noisePasses_=0;smallDiagnostic_=SmallDiagnostic::Noise;
    progress_.smallDiagnostic="Measuring stationary small-drive noise";beginSamples(StarFocusPhase::Sampling);
}
void StarAutofocus::calibrationMove(int target){
    // Reserve a return and approach, including a provisional upper bound on play.
    const int reserve=std::abs(target-smallOrigin_.position)+2*std::max(options_.backlash,options_.backlashProbeLimit);
    if(progress_.moves+std::abs(target-progress_.position)+reserve>options_.maxMoves){finishSmallCalibration();return;}
    travel(target,StarFocusPhase::Sampling,false);
}
void StarAutofocus::finishSmallCalibration(){
    auto repeatable=[](const std::vector<int>& v){return v.size()==2&&std::abs(v[0]-v[1])<=1;};
    progress_.backlashRepeatable=repeatable(nearProbes_)&&repeatable(farProbes_);
    if(progress_.backlashRepeatable)measuredBacklash_=std::max(progress_.backlashNear,progress_.backlashFar);
    smallDiagnostic_=SmallDiagnostic::Return;progress_.smallDiagnostic="Verifying small-drive calibration return";
    const int cost=std::abs(progress_.position-(smallOrigin_.position-overshoot()))+overshoot();
    if(progress_.moves+cost>options_.maxMoves){fail("Insufficient movement budget for calibration return");return;}
    travel(smallOrigin_.position,StarFocusPhase::Sampling,true);
}
void StarAutofocus::smallCalibrationPoint(Measurement m){
    if(smallDiagnostic_==SmallDiagnostic::Noise){
        if(auto c=compare(m,smallOrigin_))progress_.stationaryNoise=std::max({progress_.stationaryNoise,c->noise,std::abs(c->delta)});
        smallOrigin_=best_=m;
        if(++noisePasses_<2){beginSamples(StarFocusPhase::Sampling);return;}
        if(!options_.autoBacklash){smallDiagnostic_=SmallDiagnostic::None;progress_.smallDiagnostic="Small-drive calibration disabled";beginScan(std::move(m),1);return;}
        smallDiagnostic_=SmallDiagnostic::Wing;progress_.smallDiagnostic="Seeking a measurable small-drive wing";
        probeCommands_=1;calibrationMove(smallOrigin_.position+probeCommands_);return;
    }
    if(smallDiagnostic_==SmallDiagnostic::Wing){
        auto c=compare(m,smallOrigin_);
        if(c&&c->delta>responseThreshold(*c)){
            probeReference_=m;probeCommands_=0;smallDiagnostic_=SmallDiagnostic::Near;
            progress_.smallDiagnostic="Measuring Near1 reversal response";calibrationMove(m.position-1);return;
        }
        if(probeCommands_>=options_.backlashProbeLimit){finishSmallCalibration();return;}
        probeCommands_=std::min(options_.backlashProbeLimit,probeCommands_*2);calibrationMove(smallOrigin_.position+probeCommands_);return;
    }
    if(smallDiagnostic_==SmallDiagnostic::Return){
        const auto returned=compare(m,smallOrigin_);
        progress_.backlashReturnVerified=progress_.backlashRepeatable&&returned&&returned->delta<=responseThreshold(*returned);
        if(!progress_.backlashReturnVerified)measuredBacklash_=0;
        progress_.smallDiagnostic=progress_.backlashReturnVerified?"Repeatable reversal response and return verified":"Reversal response uncertain; configured compensation retained";
        smallDiagnostic_=SmallDiagnostic::None;progress_.position=0;m.position=0;lastObservation_={};
        // Rebase on a FRESH measurement. Never assume nominal return commands
        // reproduced an optical position; earlier tier references remain mandatory.
        beginScan(std::move(m),1);return;
    }
    const bool near=smallDiagnostic_==SmallDiagnostic::Near||smallDiagnostic_==SmallDiagnostic::NearConfirm;
    const bool confirming=smallDiagnostic_==SmallDiagnostic::NearConfirm||smallDiagnostic_==SmallDiagnostic::FarConfirm;
    auto c=compare(m,probeReference_);
    const bool response=c&&(near?-c->delta:c->delta)>responseThreshold(*c);
    if(!confirming)++probeCommands_;
    if(response&&!confirming){smallDiagnostic_=near?SmallDiagnostic::NearConfirm:SmallDiagnostic::FarConfirm;beginSamples(StarFocusPhase::Sampling);return;}
    if(response&&confirming){
        auto& estimates=near?nearProbes_:farProbes_;estimates.push_back(probeCommands_);
        (near?progress_.backlashNear:progress_.backlashFar)=*std::max_element(estimates.begin(),estimates.end());
        if(nearProbes_.size()==2&&farProbes_.size()==2){finishSmallCalibration();return;}
        probeReference_=m;probeCommands_=0;smallDiagnostic_=near?SmallDiagnostic::Far:SmallDiagnostic::Near;
        progress_.smallDiagnostic=near?"Measuring Far1 reversal response":"Repeating Near1 reversal response";
        calibrationMove(m.position+(near?1:-1));return;
    }
    if(probeCommands_>=options_.backlashProbeLimit){finishSmallCalibration();return;}
    smallDiagnostic_=near?SmallDiagnostic::Near:SmallDiagnostic::Far;calibrationMove(m.position+(near?-1:1));
}
}
