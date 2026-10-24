#include <astro/StarAutofocus.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
namespace astro {
namespace {
double median(std::vector<double> v){std::sort(v.begin(),v.end());return v.size()%2?v[v.size()/2]:(v[v.size()/2-1]+v[v.size()/2])*.5;}
double scatter(const std::vector<double>& values,double center){std::vector<double> errors;for(double v:values)errors.push_back(std::abs(v-center));return 1.4826*median(errors);}
}
void validateStarFocus(const StarFocusOptions& o){
    if(!std::isfinite(o.fitMinRSquared)||o.fitMinRSquared<.5||o.fitMinRSquared>.9999||!std::isfinite(o.fitMaxUncertainty)||o.fitMaxUncertainty<.1||o.fitMaxUncertainty>3)throw std::invalid_argument("Invalid focus curve quality limits");
    if(o.searchStep<1||o.searchStep>3||o.searchSpan<2||o.searchSpan>40||o.maxPasses<1||o.maxPasses>8||o.maxStride<1||o.maxStride>16)throw std::invalid_argument("Invalid stellar acquisition options");
    if(o.step<1||o.step>3||o.positions<7||o.positions>21||o.positions%2==0||o.framesPerPoint<3||o.framesPerPoint>15||o.minStars<3||o.minStars>20||o.settleMs<200||o.settleMs>5000||o.backlash<1||o.backlash>10||o.maxMoves<20||o.maxMoves>200||o.timeoutSeconds<30||o.timeoutSeconds>600||!std::isfinite(o.detectionSnr)||o.detectionSnr<4||o.detectionSnr>20||!std::isfinite(o.matchRadius)||o.matchRadius<2||o.matchRadius>30||!std::isfinite(o.maxScatter)||o.maxScatter<.02||o.maxScatter>.25||!std::isfinite(o.verificationTolerance)||o.verificationTolerance<.02||o.verificationTolerance>.20)throw std::invalid_argument("Invalid stellar autofocus options");
}
void StarAutofocus::start(const StarFocusOptions& o){
    if(progress_.active())throw std::logic_error("Stellar autofocus already running");validateStarFocus(o);
    if(!camera_.connected()||camera_.lensFocusStatus().state!=LensFocusState::Ready)throw CameraError("AF lens required");
    if(!camera_.manualExposureForFocus())throw CameraError("Use M, fixed ISO and non-Bulb Tv for stellar autofocus");
    options_=o;options_.searchStep=3;options_.step=1;options_.maxPasses=std::max(3,o.maxPasses);
    progress_={};moves_.clear();cohort_.clear();warmup_.clear();samples_.clear();lastRevision_=0;width_=height_=0;
    baseline_={};best_={};reference_={};stageReferences_={};measured_.clear();restoreReason_.clear();
    searching_=bracketed_=forwardFresh_=false;searchPosition_=lastDirection_=0;searchDirection_=1;
    progress_.motorStep=3;
    deadline_=clock_.steadyNow()+std::chrono::seconds(o.timeoutSeconds);beginSamples(StarFocusPhase::Baseline);
}
void StarAutofocus::fail(const std::string& s){moves_.clear();progress_.phase=StarFocusPhase::Failed;progress_.detail=s;}
void StarAutofocus::cancel(){if(progress_.active()){moves_.clear();progress_.phase=StarFocusPhase::Cancelled;progress_.detail="Cancelled; lens left at current position";}}
int StarAutofocus::overshoot() const {return progress_.motorStep>options_.step?1:options_.backlash;}
void StarAutofocus::beginSamples(StarFocusPhase phase){
    progress_.phase=phase;samples_.assign(cohort_.size(),{});lastSeen_.assign(cohort_.size(),0);warmup_.clear();seenFrames_=0;progress_.frames=0;progress_.stars=0;discard_=2;
    ready_=clock_.steadyNow()+std::chrono::milliseconds(options_.settleMs);sampleDeadline_=ready_+std::chrono::seconds(12);
    progress_.detail=phase==StarFocusPhase::Searching?"Acquiring stars at search position":"Waiting for stable star samples";
}
void StarAutofocus::travel(int target,StarFocusPhase after,bool approach){
    int position=progress_.position;moves_.clear();
    auto go=[&](int to){while(position!=to){int direction=to>position?1:-1;moves_.push_back(direction);position+=direction;}};
    // Already moving toward infinity: do not reverse and introduce backlash
    // again just to visit the next point on the same side of the scan.
    if(approach&&!(target>=position&&lastDirection_==1))go(target-overshoot());go(target);
    afterMove_=after;progress_.phase=after==StarFocusPhase::Verifying?StarFocusPhase::Returning:StarFocusPhase::Moving;ready_=clock_.steadyNow();
    progress_.detail=after==StarFocusPhase::Verifying?"Returning from the near side; verification required":"Measuring around the best position";
}
bool StarAutofocus::probe(int target){
    // Reserve enough commands to get back to the last measured best, including
    // the near-side approach. The total move budget is never exceeded.
    const int cost=std::abs(progress_.position-target);
    const int back=std::abs(target-(best_.position-overshoot()))+overshoot();
    if(progress_.moves+cost+back>options_.maxMoves){restoreBest("Focus movement budget exhausted; optimum not confirmed");return false;}
    // Scan each leg continuously. Reversing at EVERY near-side sample eats
    // fine commands in the gear play and makes nominal coordinates drift.
    // The measured best is independently approached/verified before promotion.
    travel(target,StarFocusPhase::Sampling,false);return true;
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
            camera_.focus(direction*progress_.motorStep);lastDirection_=direction;moves_.pop_front();++progress_.moves;progress_.position+=direction;
            if(searching_)++progress_.searchMoves;
            ready_=now+std::chrono::milliseconds(options_.settleMs);return;
        }
        if(now>=sampleDeadline_){
            if(progress_.phase==StarFocusPhase::Sampling&&seenFrames_>0)missingPoint();
            else fail("Not enough stable matching stars at return or baseline");
            return;
        }
        if(!field||revision==lastRevision_)return;lastRevision_=revision;
        if(now<ready_)return;if(discard_>0){--discard_;return;}
        sample(*field);
        if((progress_.phase==StarFocusPhase::Baseline||progress_.phase==StarFocusPhase::Searching)&&cohort_.empty()&&seenFrames_>=options_.framesPerPoint*2){
            if(options_.searchEnabled)nextSearchPosition();else fail("No persistent stars detected; enable automatic star search");
        }else if(progress_.phase==StarFocusPhase::Sampling&&progress_.frames==0&&seenFrames_>=options_.framesPerPoint*2)missingPoint();
    }catch(const std::exception& e){fail(e.what());}
}
void StarAutofocus::sample(const StarField& f){
    if(f.width<=0||f.height<=0)return;
    if(std::any_of(f.stars.begin(),f.stars.end(),[&](const auto& s){return !std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.hfd)||!std::isfinite(s.flux)||!std::isfinite(s.snr)||s.x<0||s.x>=f.width||s.y<0||s.y>=f.height||s.hfd<=0||s.flux<=0||s.snr<=0;}))return;
    if(width_&&(f.width!=width_||f.height!=height_)){fail("Live View geometry changed");return;}
    ++seenFrames_;
    if(cohort_.empty()){
        warmup_.push_back(f);if(warmup_.size()>static_cast<std::size_t>(options_.framesPerPoint))warmup_.erase(warmup_.begin());
        if(warmup_.size()<static_cast<std::size_t>(options_.framesPerPoint))return;
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
        std::sort(cohort_.begin(),cohort_.end(),[](auto a,auto b){return a.snr>b.snr;});
        if(cohort_.size()>40)cohort_.resize(40);
        samples_.assign(cohort_.size(),{});lastSeen_.assign(cohort_.size(),0);
    }
    alignStars(f);
    // EVF JPEG brightness can change even with fixed exposure (and a monitor
    // can flicker). Use a robust common flux scale, not a looser per-star gate.
    std::vector<double> ratios;
    for(const auto& star:cohort_){const StarSample* nearest=nullptr;double distance=options_.matchRadius;
        for(const auto& s:f.stars){double d=std::hypot(s.x-star.x,s.y-star.y);if(d<distance){distance=d;nearest=&s;}}
        if(nearest)ratios.push_back(nearest->flux/star.flux);
    }
    const double fluxScale=ratios.size()>=static_cast<std::size_t>(options_.minStars)?std::clamp(median(ratios),.5,2.):1.;
    const int required=options_.framesPerPoint*(progress_.phase==StarFocusPhase::Verifying?2:1);
    std::vector<bool> used(f.stars.size());std::vector<double> current;
    Measurement point;point.position=progress_.position;std::vector<int> counts;
    for(std::size_t id=0;id<cohort_.size();++id){auto& star=cohort_[id];
        double distance=options_.matchRadius*options_.matchRadius;std::size_t best=f.stars.size();
        for(std::size_t i=0;i<f.stars.size();++i)if(!used[i]){const auto& s=f.stars[i];const double d=(s.x-star.x)*(s.x-star.x)+(s.y-star.y)*(s.y-star.y);if(d<distance&&s.flux>=star.flux*fluxScale*.65&&s.flux<=star.flux*fluxScale*1.5){distance=d;best=i;}}
        if(best!=f.stars.size()){
            const auto& s=f.stars[best];used[best]=true;star.x=s.x;star.y=s.y;lastSeen_[id]=seenFrames_;
            auto& values=samples_[id];values.push_back(s.hfd);if(values.size()>static_cast<std::size_t>(required))values.erase(values.begin());current.push_back(s.hfd);
        }
        // A temporarily missing star does not discard all other measurements.
        // No new identity enters the cohort after baseline acquisition.
        if(seenFrames_-lastSeen_[id]>options_.framesPerPoint){samples_[id].clear();continue;}
        const auto& values=samples_[id];counts.push_back(static_cast<int>(values.size()));
        if(values.size()<static_cast<std::size_t>(required))continue;
        const double hfd=median(values),sigma=scatter(values,hfd);const auto [lo,hi]=std::minmax_element(values.begin(),values.end());
        if(sigma/hfd>options_.maxScatter||(*hi-*lo)/hfd>options_.maxScatter*3)continue;
        point.values.push_back({id,hfd,sigma});
    }
    progress_.stars=static_cast<int>(current.size());
    if(!current.empty())progress_.current=median(current);
    std::sort(counts.begin(),counts.end(),std::greater<>());
    progress_.frames=counts.size()>=static_cast<std::size_t>(options_.minStars)?counts[options_.minStars-1]:0;
    if(point.values.size()<static_cast<std::size_t>(options_.minStars)||seenFrames_<required*2)return;
    // Wait for common identities instead of closing a point as soon as any
    // minimum-sized subset happens to finish first. This matters with flicker
    // and marginal detections: other cohort members may need a few more frames.
    if((progress_.phase==StarFocusPhase::Sampling||progress_.phase==StarFocusPhase::Reanchoring||progress_.phase==StarFocusPhase::Verifying)&&!compare(point,best_))return;
    completePoint(std::move(point));
}
void StarAutofocus::alignStars(const StarField& f){
    // Register a small uniform scale + translation using spatial consensus.
    // Focus breathing is coherent across the field; an isolated nearest match
    // must not drag the cohort onto an unrelated star. No rotation or unbounded
    // jump is allowed. HFD remains measured in the original image pixels.
    struct Pair {StarSample from,to;};std::vector<Pair> seeds;
    const double cx=f.width*.5,cy=f.height*.5,limit=options_.matchRadius*2;
    for(std::size_t i=0;i<std::min<std::size_t>(12,cohort_.size());++i){
        const auto& star=cohort_[i];double nearest=limit*2;const StarSample* found=nullptr;
        for(const auto& s:f.stars){double d=std::hypot(s.x-star.x,s.y-star.y);if(d<nearest){nearest=d;found=&s;}}
        if(found)seeds.push_back({star,*found});
    }
    int bestCount=0;double bestError=std::numeric_limits<double>::infinity(),bestScale=1,bestX=0,bestY=0;
    auto score=[&](double scale,double tx,double ty){
        if(scale<.92||scale>1.08||std::hypot(tx,ty)>limit)return;
        std::vector<bool> used(f.stars.size());int count=0;double error=0;
        for(const auto& star:cohort_){const double x=cx+(star.x-cx)*scale+tx,y=cy+(star.y-cy)*scale+ty;
            double nearest=std::max(1.5,options_.matchRadius*.25);std::size_t match=f.stars.size();
            for(std::size_t i=0;i<f.stars.size();++i)if(!used[i]){double d=std::hypot(f.stars[i].x-x,f.stars[i].y-y);if(d<nearest){nearest=d;match=i;}}
            if(match<f.stars.size()){used[match]=true;++count;error+=nearest*nearest;}
        }
        if(count>bestCount||(count==bestCount&&error<bestError)){bestCount=count;bestError=error;bestScale=scale;bestX=tx;bestY=ty;}
    };
    score(1,0,0);
    for(std::size_t i=0;i<seeds.size();++i)for(std::size_t j=i+1;j<seeds.size();++j){
        const auto a=seeds[i],b=seeds[j];const double dx=b.from.x-a.from.x,dy=b.from.y-a.from.y,den=dx*dx+dy*dy;
        if(den<2500)continue;
        const double scale=((b.to.x-a.to.x)*dx+(b.to.y-a.to.y)*dy)/den;
        score(scale,a.to.x-cx-(a.from.x-cx)*scale,a.to.y-cy-(a.from.y-cy)*scale);
    }
    if(bestCount<options_.minStars)return;
    for(auto& star:cohort_){star.x=cx+(star.x-cx)*bestScale+bestX;star.y=cy+(star.y-cy)*bestScale+bestY;}
}
std::optional<StarAutofocus::Comparison> StarAutofocus::compare(const Measurement& a,const Measurement& b) const{
    std::vector<double> current,reference,deltas,noises;
    for(auto av:a.values)for(auto bv:b.values)if(av.id==bv.id){current.push_back(av.hfd);reference.push_back(bv.hfd);deltas.push_back(av.hfd-bv.hfd);noises.push_back(std::hypot(av.scatter,bv.scatter));break;}
    if(current.size()<static_cast<std::size_t>(options_.minStars))return {};
    // Paired differences prevent changes in the detected star population from
    // masquerading as an improvement. Compare only the SAME identities.
    return Comparison{median(current),median(reference),median(deltas),median(noises),static_cast<int>(current.size())};
}
void StarAutofocus::record(const Measurement& m){
    // A return over a position replaces its scout measurement. Mixing both
    // approach directions at one nominal Canon coordinate biases a curve fit.
    std::erase_if(measured_,[&](const auto& old){return old.position==m.position;});measured_.push_back(m);
    progress_.curve.clear();
    for(const auto& point:measured_){std::vector<double> values,noises;for(auto v:point.values){values.push_back(v.hfd);noises.push_back(v.scatter);}
        progress_.curve.push_back({point.position,median(values),median(noises)});}
    fitCurve(false);
}
hocus::Fit StarAutofocus::fitCurve(bool stability){
    progress_.fitValid=false;progress_.curveR2=0;
    if(measured_.size()<5)return {};
    std::vector<std::size_t> ids;for(auto v:measured_.front().values){bool present=true;
        for(const auto& point:measured_)if(std::none_of(point.values.begin(),point.values.end(),[&](auto s){return s.id==v.id;})){present=false;break;}
        if(present)ids.push_back(v.id);
    }
    if(ids.size()<static_cast<std::size_t>(options_.minStars))return {};
    std::vector<hocus::Point> points;
    for(const auto& point:measured_){std::vector<double> values,noises;
        for(auto v:point.values)if(std::find(ids.begin(),ids.end(),v.id)!=ids.end()){values.push_back(v.hfd);noises.push_back(v.scatter);}
        points.push_back({double(point.position),median(values),median(noises)});
    }
    auto fit=hocus::fitHyperbola(points,stability);progress_.fitValid=fit.valid;progress_.curveR2=fit.rSquared;
    progress_.fitMinimum=fit.minimum;progress_.fitParameters=fit.parameters;
    progress_.fitUncertainty=stability?fit.leaveOneOutSigma:fit.positionSigma;return fit;
}
void StarAutofocus::completePoint(Measurement m){
    if(progress_.phase==StarFocusPhase::Baseline||progress_.phase==StarFocusPhase::Searching){
        baseline_=m;progress_.baseline=compare(m,m)->current;searching_=false;
        // Keep the coarse drive used for acquisition. Never jump directly to 1.
        progress_.motorStep=std::max(options_.step,progress_.motorStep);progress_.position=0;m.position=0;
        beginScan(std::move(m),1);return;
    }
    if(progress_.phase==StarFocusPhase::Reanchoring){
        const auto returned=compare(m,best_);
        if(!returned||returned->delta>returned->reference*options_.verificationTolerance){
            // Canon commands are not absolute positions. Never promote a tier
            // merely because its nominal return commands have been sent.
            bracketed_=false;progress_.bracketed=false;reference_={};
            beginScan(std::move(m),1);progress_.detail="Stage return mismatch: remeasuring this drive";return;
        }
        progress_.stageBest[3-progress_.motorStep]=returned->current;
        stageReferences_[3-progress_.motorStep]=best_;
        --progress_.motorStep;progress_.position=0;m.position=0;beginScan(std::move(m),1);return;
    }
    if(progress_.phase==StarFocusPhase::Verifying){
        const auto result=compare(m,best_);const auto baseline=compare(m,baseline_);
        const auto reference=reference_.values.empty()?result:compare(m,reference_);
        if(!result||!reference){fail("Not enough common stars to verify the return");return;}
        if(!restoreReason_.empty()){
            if(result->delta<=result->reference*options_.verificationTolerance){progress_.restored=true;fail(restoreReason_);}
            else fail("Could not reproduce the best position; stellar focus not confirmed");return;
        }
        progress_.verified=result->current;
        bool worseThanEarlierTier=false;
        for(const auto& tier:stageReferences_)if(!tier.values.empty()){
            const auto compared=compare(m,tier);
            if(!compared){fail("Not enough common stars to verify the return");return;}
            worseThanEarlierTier|=compared->delta>compared->reference*options_.verificationTolerance;
        }
        if(worseThanEarlierTier||result->delta>result->reference*options_.verificationTolerance||(baseline&&baseline->delta>baseline->reference*options_.verificationTolerance)||reference->delta>reference->reference*options_.verificationTolerance){
            if(progress_.pass<options_.maxPasses){options_.backlash=std::min(10,options_.backlash+1);bracketed_=false;progress_.bracketed=false;reference_={};beginScan(std::move(m),1);progress_.detail="Return mismatch: measuring a new local bracket";return;}
            fail("Return did not reproduce the minimum; adjust backlash or use a smaller step");return;
        }
        if(progress_.motorStep!=1||progress_.stageBest[0]<=0||progress_.stageBest[1]<=0){fail("All three focus stages must be verified");return;}
        progress_.stageBest[2]=result->current;
        progress_.phase=StarFocusPhase::Complete;progress_.detail="Verified stellar focus";return;
    }
    // Near-side scouting locates a wing. Only the subsequent continuous
    // farward leg supplies a precision curve, as in Hocus Focus's sweep.
    // A Canon return command cannot be treated as an encoder coordinate.
    if(forwardFresh_){best_=m;forwardFresh_=false;}
    const auto compared=compare(m,best_);
    if(!compared){missingPoint();return;}
    progress_.pairedStars=compared->stars;
    record(m);++sidePoints_;
    // Ignore changes smaller than temporal noise when choosing a new best.
    const double indistinguishable=std::max(compared->reference*.005,compared->noise);
    if(compared->delta < -indistinguishable||(std::abs(compared->delta)<=indistinguishable&&std::abs(m.position-scanCenter_)<std::abs(best_.position-scanCenter_)))best_=m;
    progress_.best=compare(best_,best_)->current;
    const bool pastBest=direction_<0?m.position<best_.position:m.position>best_.position;
    const bool worse=pastBest&&compared->delta>std::max(compared->reference*.03,3*compared->noise);
    // Hocus Focus brackets with several measured points on each wing. A
    // single worse point is not enough to constrain a four-parameter curve.
    int beyond=0;for(const auto& p:measured_)if(direction_<0?p.position<best_.position:p.position>best_.position)++beyond;
    nextScanPosition((worse&&beyond>=3)||sidePoints_>=(direction_<0?options_.positions/2:options_.positions-1));
}
void StarAutofocus::nextSearchPosition(){
    if(!searching_){searching_=true;progress_.motorStep=std::max(options_.step,options_.searchStep);progress_.position=0;searchPosition_=0;searchDirection_=1;}
    // Acquisition stays on the large drive. Hints alone cannot complete a
    // measured tier or bypass large -> medium -> small.
    if(searchPosition_>=options_.searchSpan)searchDirection_=-1;
    if(searchPosition_<=-options_.searchSpan){fail("No persistent stars within search range; check exposure, field or increase search span");return;}
    searchPosition_+=searchDirection_;cohort_.clear();warmup_.clear();travel(searchPosition_,StarFocusPhase::Searching,false);
    progress_.detail=searchDirection_>0?"Searching for stars toward infinity":"Searching for stars toward near";
}
void StarAutofocus::beginScan(Measurement center,int stride){
    if(progress_.pass>=options_.maxPasses){restoreBest("Focus pass budget exhausted; optimum not confirmed");return;}
    scanCenter_=center.position;best_=center;direction_=-1;forwardFresh_=false;sidePoints_=0;progress_.stride=stride;++progress_.pass;
    measured_.clear();progress_.curve.clear();progress_.missingPoints=0;progress_.curveR2=0;record(center);
    progress_.best=compare(center,center)->current;
    probe(scanCenter_-stride);
}
void StarAutofocus::missingPoint(){
    ++progress_.missingPoints;
    // Do not continue walking away through an empty field. Measure the other
    // side of the known center, then return and reduce the command size.
    nextScanPosition(true);
}
void StarAutofocus::nextScanPosition(bool stopSide){
    if(!stopSide){probe(progress_.position+direction_*progress_.stride);return;}
    if(direction_<0){
        int wing=scanCenter_;for(const auto& p:measured_)wing=std::min(wing,p.position);
        direction_=1;sidePoints_=0;forwardFresh_=true;
        measured_.clear();progress_.curve.clear();progress_.fitValid=false;progress_.curveR2=0;
        // Include the leftmost valid scout coordinate again on the forward
        // approach, taking up play outside that wing first.
        travel(wing,StarFocusPhase::Sampling,true);return;
    }
    evaluateCurve();
}
void StarAutofocus::changeMotorStep(){
    if(progress_.pass>=options_.maxPasses){restoreBest("Focus pass budget exhausted; optimum not confirmed");return;}
    travel(best_.position,StarFocusPhase::Reanchoring,true);progress_.detail="Returning to best before reducing Canon drive";
}
void StarAutofocus::restoreBest(const std::string& reason){
    if(best_.values.empty()){fail(reason);return;}
    const int cost=std::abs(progress_.position-(best_.position-overshoot()))+overshoot();
    if(progress_.moves+cost>options_.maxMoves){fail(reason);return;}
    restoreReason_=reason;travel(best_.position,StarFocusPhase::Verifying,true);
    progress_.detail="Restoring best measured position; focus remains unconfirmed";
}
void StarAutofocus::evaluateCurve(){
    const auto fit=fitCurve(true);
    int leftCount=0,rightCount=0;
    for(const auto& p:measured_){leftCount+=p.position<fit.minimum;rightCount+=p.position>fit.minimum;}
    const bool reliableFit=fit.valid&&measured_.size()>=7&&leftCount>=2&&rightCount>=2&&fit.rSquared>=options_.fitMinRSquared
        &&fit.leaveOneOutSolved>=static_cast<int>(measured_.size())-1&&std::isfinite(fit.leaveOneOutSigma)
        &&fit.leaveOneOutSigma<=options_.fitMaxUncertainty*(progress_.motorStep==1?1:progress_.stride);
    bool localBracket=false;Measurement commonBest;
    for(const auto& left:measured_)if(left.position<best_.position)
    for(const auto& right:measured_)if(right.position>best_.position){
        Measurement shared;shared.position=best_.position;
        for(auto v:best_.values){auto has=[&](const Measurement& m){return std::any_of(m.values.begin(),m.values.end(),[&](auto x){return x.id==v.id;});};if(has(left)&&has(right))shared.values.push_back(v);}
        auto l=compare(left,shared),r=compare(right,shared);if(!l||!r)continue;
        if(l->delta>std::max(l->reference*.03,3*l->noise)&&r->delta>std::max(r->reference*.03,3*r->noise)){localBracket=true;commonBest=std::move(shared);}
    }
    if(localBracket){bracketed_=true;progress_.bracketed=true;reference_=std::move(commonBest);}
    if(reliableFit&&localBracket){
        // A numerically perfect fit can still describe variation smaller than
        // temporal measurement noise. Require a measured paired-star bracket
        // as well as fit quality; R2 alone is not evidence of optical response.
        best_.position=static_cast<int>(std::lround(fit.minimum));
        if(progress_.motorStep>1)changeMotorStep();else travel(best_.position,StarFocusPhase::Verifying,true);
        return;
    }
    // If this tier is still improving at a boundary, follow its best before
    // reducing the drive. Reserve at least one pass for each remaining tier.
    if(progress_.motorStep>1){
        const bool room=progress_.pass+progress_.motorStep-1<options_.maxPasses;
        if(!localBracket&&best_.position!=scanCenter_){
            if(room){beginScan(best_,progress_.stride);return;}
            restoreBest("Stage minimum is still on the boundary; increase pass budget");return;
        }
        if(!localBracket&&progress_.missingPoints==0&&!bracketed_){
            if(room&&progress_.stride<options_.maxStride){beginScan(best_,std::min(options_.maxStride,progress_.stride*2));return;}
            restoreBest("Flat focus response; no reliable bracket");return;
        }
        // An unmeasurable neighbor bounds the useful coarse region but is not
        // proof of final focus. Return and verify its measured best, then refine.
        changeMotorStep();return;
    }
    // The final small tier requires a stable fitted minimum, not just a low
    // sample or a flat plateau inherited from a coarser drive.
    if(progress_.pass>=options_.maxPasses){restoreBest("No reliable bracket after fine sampling");return;}
    if(best_.position!=scanCenter_){auto center=best_;
        // A new local pass uses the measured best as its origin in the same units.
        beginScan(std::move(center),progress_.stride);return;
    }
    if(progress_.missingPoints>0){restoreBest("Tracked stars missing on both sides of the best position");return;}
    if(progress_.stride<options_.maxStride){
        // When the sampled response is buried in temporal scatter, doubling
        // wastes the remaining pass on another essentially flat curve. Widen
        // faster using only the current (small) drive; never switch back to a
        // coarse motor command. The final uncertainty still has to be <= the
        // configured number of SMALL commands, not this enlarged spacing.
        std::vector<double> sizes,noises;
        for(const auto& point:progress_.curve){sizes.push_back(point.hfd);noises.push_back(point.scatter);}
        const auto bounds=std::minmax_element(sizes.begin(),sizes.end());
        const bool noiseLimited=!sizes.empty()&&*bounds.second-*bounds.first<3*median(noises);
        beginScan(best_,std::min(options_.maxStride,progress_.stride*(noiseLimited?4:2)));return;
    }
    restoreBest("Flat focus response; no reliable bracket");
}
}
