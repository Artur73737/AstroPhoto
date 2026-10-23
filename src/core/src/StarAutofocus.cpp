#include <astro/StarAutofocus.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
namespace astro {
namespace {
double median(std::vector<double> v){std::sort(v.begin(),v.end());return v.size()%2?v[v.size()/2]:(v[v.size()/2-1]+v[v.size()/2])*.5;}
}
void validateStarFocus(const StarFocusOptions& o){
    if(o.searchStep<1||o.searchStep>3||o.searchSpan<2||o.searchSpan>40||o.maxPasses<1||o.maxPasses>8||o.maxStride<1||o.maxStride>16)throw std::invalid_argument("Invalid stellar acquisition options");
    if(o.step<1||o.step>3||o.positions<7||o.positions>21||o.positions%2==0||o.framesPerPoint<3||o.framesPerPoint>15||o.minStars<3||o.minStars>20||o.settleMs<200||o.settleMs>5000||o.backlash<1||o.backlash>10||o.maxMoves<20||o.maxMoves>200||o.timeoutSeconds<30||o.timeoutSeconds>600||!std::isfinite(o.detectionSnr)||o.detectionSnr<4||o.detectionSnr>20||!std::isfinite(o.matchRadius)||o.matchRadius<2||o.matchRadius>30||!std::isfinite(o.maxScatter)||o.maxScatter<.02||o.maxScatter>.25||!std::isfinite(o.verificationTolerance)||o.verificationTolerance<.02||o.verificationTolerance>.20)throw std::invalid_argument("Invalid stellar autofocus options");
}
void StarAutofocus::start(const StarFocusOptions& o){
    if(progress_.active())throw std::logic_error("Stellar autofocus already running");validateStarFocus(o);
    if(!camera_.connected()||camera_.lensFocusStatus().state!=LensFocusState::Ready)throw CameraError("AF lens required");
    if(!camera_.manualExposureForFocus())throw CameraError("Use M, fixed ISO and non-Bulb Tv for stellar autofocus");
    options_=o;progress_={};moves_.clear();cohort_.clear();warmup_.clear();samples_.clear();lastRevision_=0;width_=height_=0;
    searching_=refining_=searchRefined_=false;searchPosition_=0;searchDirection_=1;coarseBest_=0;stepCeiling_=3;progress_.motorStep=o.step;
    deadline_=clock_.steadyNow()+std::chrono::seconds(o.timeoutSeconds);beginSamples(StarFocusPhase::Baseline);
}
void StarAutofocus::fail(const std::string& s){moves_.clear();progress_.phase=StarFocusPhase::Failed;progress_.detail=s;}
void StarAutofocus::cancel(){if(progress_.active()){moves_.clear();progress_.phase=StarFocusPhase::Cancelled;progress_.detail="Cancelled; lens left at current position";}}
void StarAutofocus::beginSamples(StarFocusPhase phase){progress_.phase=phase;samples_.clear();warmup_.clear();seenFrames_=broadFrames_=0;progress_.frames=0;discard_=2;ready_=clock_.steadyNow()+std::chrono::milliseconds(options_.settleMs);sampleDeadline_=ready_+std::chrono::seconds(12);progress_.detail=phase==StarFocusPhase::Searching?"Acquiring stars at search position":"Waiting for stable star samples";}
void StarAutofocus::travel(int target,StarFocusPhase after,bool approach){
    int position=progress_.position;moves_.clear();
    auto go=[&](int to){while(position!=to){int direction=to>position?1:-1;moves_.push_back(direction);position+=direction;}};
    if(approach)go(target-options_.backlash);go(target);
    afterMove_=after;progress_.phase=after==StarFocusPhase::Verifying?StarFocusPhase::Returning:StarFocusPhase::Moving;ready_=clock_.steadyNow();
    progress_.detail=after==StarFocusPhase::Verifying?"Returning from the near side; verification required":"Sweeping toward infinity";
}
void StarAutofocus::tick(const StarField* field,std::uint64_t revision){
    if(!progress_.active())return;
    try{
        const auto now=clock_.steadyNow();
        if(now>=deadline_){fail("Autofocus time limit reached");return;}
        if(!camera_.connected()){fail("Camera disconnected during autofocus");return;}
        if(progress_.phase==StarFocusPhase::Moving||progress_.phase==StarFocusPhase::Returning){
            if(now<ready_)return;
            if(moves_.empty()){beginSamples(afterMove_);return;}
            if(progress_.moves>=options_.maxMoves){fail("Focus movement limit reached");return;}
            if(camera_.lensFocusStatus().state!=LensFocusState::Ready){fail("Lens is no longer AF-ready");return;}
            if(!camera_.manualExposureForFocus()){fail("Use M, fixed ISO and non-Bulb Tv for stellar autofocus");return;}
            const int direction=moves_.front();
            // Never retry an ambiguous motor command.
            camera_.focus(direction*progress_.motorStep);moves_.pop_front();++progress_.moves;progress_.position+=direction;
            if(searching_)++progress_.searchMoves;
            ready_=now+std::chrono::milliseconds(options_.settleMs);return;
        }
        if(now>=sampleDeadline_){
            if(progress_.phase==StarFocusPhase::Sampling&&seenFrames_>0){++progress_.missingPoints;nextScanPosition();}
            else fail("Not enough stable matching stars; adjust exposure or focus manually");
            return;
        }
        if(!field||revision==lastRevision_)return;lastRevision_=revision;
        if(now<ready_)return;if(discard_>0){--discard_;return;}
        sample(*field);
        // Acquisition moves require fresh, valid image geometry. A stalled stream
        // must never turn into an unobserved lens sweep.
        if((progress_.phase==StarFocusPhase::Baseline||progress_.phase==StarFocusPhase::Searching)&&cohort_.empty()&&seenFrames_>=options_.framesPerPoint*2){
            if(options_.searchEnabled)nextSearchPosition();
            else fail("No persistent stars detected; enable automatic star search");
        }else if(progress_.phase==StarFocusPhase::Sampling&&samples_.empty()&&seenFrames_>=options_.framesPerPoint*2){++progress_.missingPoints;nextScanPosition();}
    }catch(const std::exception& e){fail(e.what());}
}
void StarAutofocus::sample(const StarField& f){
    if(f.width<=0||f.height<=0)return;
    if(std::any_of(f.stars.begin(),f.stars.end(),[&](const auto& s){return !std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.hfd)||!std::isfinite(s.flux)||!std::isfinite(s.snr)||s.x<0||s.x>=f.width||s.y<0||s.y>=f.height||s.hfd<=0||s.flux<=0||s.snr<=0;}))return;
    if(width_&&(f.width!=width_||f.height!=height_)){fail("Live View geometry changed");return;}
    ++seenFrames_;
    if(f.acquisitionCandidates>=options_.minStars)++broadFrames_;else broadFrames_=0;
    if(cohort_.empty()){
        warmup_.push_back(f);if(warmup_.size()>static_cast<std::size_t>(options_.framesPerPoint))warmup_.erase(warmup_.begin());
        if(warmup_.size()<static_cast<std::size_t>(options_.framesPerPoint))return;
        // Establish persistence before selecting the cohort. A transient detection
        // in the first frame must not dictate the entire sweep.
        std::vector<StarSample> persistent;
        for(auto candidate:f.stars){
            std::vector<double> fluxes;double worstSnr=candidate.snr;
            for(const auto& previous:warmup_){
                auto match=std::min_element(previous.stars.begin(),previous.stars.end(),[&](auto a,auto b){return std::hypot(a.x-candidate.x,a.y-candidate.y)<std::hypot(b.x-candidate.x,b.y-candidate.y);});
                if(match==previous.stars.end()||std::hypot(match->x-candidate.x,match->y-candidate.y)>options_.matchRadius||match->flux<candidate.flux*.65||match->flux>candidate.flux*1.5)break;
                fluxes.push_back(match->flux);worstSnr=std::min(worstSnr,match->snr);
            }
            if(fluxes.size()==warmup_.size()){candidate.flux=median(fluxes);candidate.snr=worstSnr;persistent.push_back(candidate);}
        }
        if(persistent.size()<static_cast<std::size_t>(options_.minStars))return;
        width_=f.width;height_=f.height;cohort_=std::move(persistent);warmup_.clear();
        std::sort(cohort_.begin(),cohort_.end(),[](auto a,auto b){return a.snr>b.snr;});cohort_.resize(options_.minStars);
    }
    std::vector<double> values;std::vector<StarSample> matched;std::vector<bool> used(f.stars.size());
    for(const auto& star:cohort_){
        double distance=options_.matchRadius*options_.matchRadius;std::size_t best=f.stars.size();
        for(std::size_t i=0;i<f.stars.size();++i)if(!used[i]){const auto& s=f.stars[i];const double d=(s.x-star.x)*(s.x-star.x)+(s.y-star.y)*(s.y-star.y);if(d<distance){distance=d;best=i;}}
        if(best==f.stars.size())return;
        const auto& s=f.stars[best];if(!std::isfinite(s.hfd)||s.hfd<=0||s.flux<star.flux*.65||s.flux>star.flux*1.5)return;
        used[best]=true;values.push_back(s.hfd);matched.push_back(s);
    }
    // Keep the initial flux reference, but track small translation/breathing between frames.
    for(std::size_t i=0;i<cohort_.size();++i){cohort_[i].x=matched[i].x;cohort_[i].y=matched[i].y;}
    progress_.stars=static_cast<int>(values.size());progress_.current=median(values);samples_.push_back(progress_.current);progress_.frames=static_cast<int>(samples_.size());
    if(progress_.frames>=options_.framesPerPoint*(progress_.phase==StarFocusPhase::Verifying?2:1))completePoint();
}
void StarAutofocus::completePoint(){
    const double hfd=median(samples_);std::vector<double> errors;for(double v:samples_)errors.push_back(std::abs(v-hfd));
    const double scatter=1.4826*median(errors);
    const auto [lo,hi]=std::minmax_element(samples_.begin(),samples_.end());
    if(scatter/hfd>options_.maxScatter||(*hi-*lo)/hfd>options_.maxScatter*3){
        // Keep the fixed position/cohort and wait for a clean sliding window.
        // The existing per-position deadline prevents unbounded retries.
        samples_.erase(samples_.begin());progress_.frames=static_cast<int>(samples_.size());
        progress_.detail="Waiting for stable HFD; rejecting temporal outliers";return;
    }
    if(progress_.phase==StarFocusPhase::Baseline||progress_.phase==StarFocusPhase::Searching){
        progress_.baseline=hfd;searching_=false;progress_.motorStep=options_.step;
        // Search and precision drives have different, uncalibrated units.
        // The acquired optical position becomes the new precision origin.
        progress_.position=0;beginScan(0,1);return;
    }
    if(progress_.phase==StarFocusPhase::Reanchoring){
        progress_.motorStep=pendingStep_;progress_.position=0;refining_=pendingRefinement_;
        beginScan(0,pendingStride_);return;
    }
    if(progress_.phase==StarFocusPhase::Verifying){
        progress_.verified=hfd;
        if(hfd>progress_.best*(1+options_.verificationTolerance)||hfd>progress_.baseline*(1+options_.verificationTolerance)||(coarseBest_>0&&hfd>coarseBest_*(1+options_.verificationTolerance))){
            // The lens did not reproduce the commanded position. Measure locally
            // again; never accept the predicted position or blindly retry a move.
            if(progress_.pass<options_.maxPasses){options_.backlash=std::min(10,options_.backlash+1);refining_=false;beginScan(progress_.position,1);progress_.detail="Return mismatch: measuring a new local bracket";return;}
            fail("Return did not reproduce the minimum; adjust backlash or use a smaller step");return;
        }
        progress_.phase=StarFocusPhase::Complete;progress_.detail="Verified stellar focus";return;
    }
    progress_.curve.push_back({progress_.position,hfd,scatter});
    nextScanPosition();
}
void StarAutofocus::nextSearchPosition(){
    if(!searching_){searching_=true;progress_.motorStep=options_.searchStep;progress_.position=0;searchPosition_=0;searchDirection_=1;}
    if(!searchRefined_&&progress_.motorStep>1&&broadFrames_>=options_.framesPerPoint){
        // Coarse-scale detections guide acquisition only; they never enter the
        // precision HFD curve. Re-anchor because Canon drive sizes are nonlinear.
        searchRefined_=true;--progress_.motorStep;progress_.position=searchPosition_=0;searchDirection_=1;
    }
    if(searchPosition_>=options_.searchSpan)searchDirection_=-1;
    if(searchPosition_<=-options_.searchSpan){fail("No persistent stars within search range; check exposure, field or increase search span");return;}
    searchPosition_+=searchDirection_;
    cohort_.clear();warmup_.clear();travel(searchPosition_,StarFocusPhase::Searching,false);
    progress_.detail=searchDirection_>0?"Searching for stars toward infinity":"Searching for stars toward near";
}
void StarAutofocus::beginScan(int center,int stride){
    scanCenter_=center;scanEnd_=center+(options_.positions/2)*stride;
    progress_.stride=stride;++progress_.pass;progress_.curve.clear();progress_.missingPoints=0;progress_.curveR2=0;
    travel(center-(options_.positions/2)*stride,StarFocusPhase::Sampling,true);
}
void StarAutofocus::nextScanPosition(){
    if(progress_.position<scanEnd_)travel(progress_.position+progress_.stride,StarFocusPhase::Sampling,false);
    else evaluateCurve();
}
void StarAutofocus::adaptScan(const std::string& reason,int center,int stride){
    if(progress_.pass>=options_.maxPasses||stride>options_.maxStride){fail(reason);return;}
    beginScan(center,stride);progress_.detail="Adapting focus scan: recentering and changing spacing";
}
void StarAutofocus::changeMotorStep(int step,int target,bool refinement,int stride){
    pendingStep_=step;pendingRefinement_=refinement;pendingStride_=stride;
    travel(target,StarFocusPhase::Reanchoring,true);
    progress_.detail=refinement?"Refining with a smaller Canon drive":"Small drive response unresolved; increasing Canon drive";
}
void StarAutofocus::evaluateCurve(){
    const auto& points=progress_.curve;
    if(points.empty()){fail("Tracked stars lost throughout scan; no reliable focus measurement");return;}
    const auto best=std::min_element(points.begin(),points.end(),[](auto a,auto b){return a.hfd<b.hfd;});
    const auto index=std::distance(points.begin(),best);progress_.best=best->hfd;
    const int bestPosition=best->position;
    if(points.size()<5){
        // A coarse drive can skip the narrow measurable region. Re-anchor with
        // the old drive, then densely sample with the smaller command. This is
        // not a validated bracket: a flat follow-up must still fail/adapt.
        if(points.size()>=3&&progress_.motorStep>options_.step&&progress_.pass<options_.maxPasses){
            stepCeiling_=progress_.motorStep-1;
            changeMotorStep(stepCeiling_,bestPosition,false,std::min(2,options_.maxStride));
            progress_.detail="Coarse scan undersampled; measuring with smaller commands";return;
        }
        adaptScan("Too few measured points for a reliable focus curve",bestPosition,std::max(1,progress_.stride/2));return;
    }
    const double margin=std::max(best->hfd*.08,3*best->scatter);
    const double range=std::max_element(points.begin(),points.end(),[](auto a,auto b){return a.hfd<b.hfd;})->hfd-best->hfd;
    if(range<margin){
        if(refining_){
            const auto center=std::min_element(points.begin(),points.end(),[&](auto a,auto b){return std::abs(a.position-scanCenter_)<std::abs(b.position-scanCenter_);});
            if(center->hfd<=coarseBest_*(1+options_.verificationTolerance)){progress_.best=center->hfd;travel(center->position,StarFocusPhase::Verifying,true);return;}
        }
        if(!refining_&&progress_.motorStep<stepCeiling_&&progress_.pass<options_.maxPasses){changeMotorStep(progress_.motorStep+1,scanCenter_,false);return;}
        adaptScan("Flat focus curve after adaptive scans; no reliable motor response",scanCenter_,progress_.stride*2);return;
    }
    if(index<2||index>static_cast<int>(points.size())-3){
        adaptScan("Minimum remains outside the measured bracket",bestPosition,progress_.stride);return;
    }
    if(points.front().hfd<best->hfd+margin||points.back().hfd<best->hfd+margin){
        // A fine plateau is usable only after an independently bracketed coarse
        // curve. A flat first scan is never evidence of correct focus.
        if(refining_&&best->hfd<=coarseBest_*(1+options_.verificationTolerance)){
            travel(bestPosition,StarFocusPhase::Verifying,true);return;
        }
        adaptScan("Flat focus curve after adaptive scans; no reliable motor response",bestPosition,progress_.stride*2);return;
    }
    for(std::size_t i=1;i<points.size();++i){const double tolerance=std::max(margin,3*std::max(points[i].scatter,points[i-1].scatter));
        if((i<=static_cast<std::size_t>(index)&&points[i].hfd>points[i-1].hfd+tolerance)||(i>static_cast<std::size_t>(index)&&points[i].hfd<points[i-1].hfd-tolerance)){
            adaptScan("Irregular focus curve; no reliable minimum",bestPosition,progress_.stride);return;
        }
    }
    // General least-squares quadratic, valid for recentered scans and missing
    // samples. Fit is a consistency check; the target is always measured.
    double matrix[3][4]{};
    for(auto p:points){const double x=double(p.position-scanCenter_)/progress_.stride;const double basis[]{1,x,x*x};
        for(int i=0;i<3;++i){for(int j=0;j<3;++j)matrix[i][j]+=basis[i]*basis[j];matrix[i][3]+=basis[i]*p.hfd;}}
    for(int column=0;column<3;++column){int pivot=column;for(int i=column+1;i<3;++i)if(std::abs(matrix[i][column])>std::abs(matrix[pivot][column]))pivot=i;
        if(std::abs(matrix[pivot][column])<1e-9){fail("Focus curve fit is inconsistent");return;}
        for(int j=0;j<4;++j)std::swap(matrix[column][j],matrix[pivot][j]);
        const double divisor=matrix[column][column];for(int j=column;j<4;++j)matrix[column][j]/=divisor;
        for(int i=0;i<3;++i)if(i!=column){const double factor=matrix[i][column];for(int j=column;j<4;++j)matrix[i][j]-=factor*matrix[column][j];}}
    const double c=matrix[0][3],b=matrix[1][3],a=matrix[2][3];double mean{};for(auto p:points)mean+=p.hfd;mean/=points.size();
    double residual{},total{};for(auto p:points){const double x=double(p.position-scanCenter_)/progress_.stride;residual+=std::pow(p.hfd-(a*x*x+b*x+c),2);total+=std::pow(p.hfd-mean,2);}
    progress_.curveR2=total>0?1-residual/total:0;
    if(a<=0||progress_.curveR2<.7||std::abs(-b/(2*a)-double(bestPosition-scanCenter_)/progress_.stride)>1.5){
        adaptScan("Focus curve fit is inconsistent",bestPosition,progress_.stride);return;
    }
    if((progress_.stride>1||progress_.motorStep>options_.step)&&!refining_){
        if(progress_.pass>=options_.maxPasses){fail("Coarse bracket found but no refinement pass remains");return;}
        coarseBest_=best->hfd;
        if(progress_.motorStep>options_.step){changeMotorStep(progress_.motorStep-1,bestPosition,true);return;}
        refining_=true;beginScan(bestPosition,1);progress_.detail="Bracket found; refining with smallest scan spacing";return;
    }
    travel(bestPosition,StarFocusPhase::Verifying,true);
}
}
