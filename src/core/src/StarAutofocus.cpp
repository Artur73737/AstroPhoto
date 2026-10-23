#include <astro/StarAutofocus.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
namespace astro {
namespace {
double median(std::vector<double> v){std::sort(v.begin(),v.end());return v.size()%2?v[v.size()/2]:(v[v.size()/2-1]+v[v.size()/2])*.5;}
}
void validateStarFocus(const StarFocusOptions& o){
    if(o.step<1||o.step>3||o.positions<7||o.positions>21||o.positions%2==0||o.framesPerPoint<3||o.framesPerPoint>15||o.minStars<3||o.minStars>20||o.settleMs<200||o.settleMs>5000||o.backlash<1||o.backlash>10||o.maxMoves<20||o.maxMoves>200||o.timeoutSeconds<30||o.timeoutSeconds>600||!std::isfinite(o.detectionSnr)||o.detectionSnr<4||o.detectionSnr>20||!std::isfinite(o.matchRadius)||o.matchRadius<2||o.matchRadius>30||!std::isfinite(o.maxScatter)||o.maxScatter<.02||o.maxScatter>.25||!std::isfinite(o.verificationTolerance)||o.verificationTolerance<.02||o.verificationTolerance>.20)throw std::invalid_argument("Invalid stellar autofocus options");
}
void StarAutofocus::start(const StarFocusOptions& o){
    if(progress_.active())throw std::logic_error("Stellar autofocus already running");validateStarFocus(o);
    if(!camera_.connected()||camera_.lensFocusStatus().state!=LensFocusState::Ready)throw CameraError("AF lens required");
    if(!camera_.manualExposureForFocus())throw CameraError("Use M, fixed ISO and non-Bulb Tv for stellar autofocus");
    options_=o;progress_={};moves_.clear();cohort_.clear();warmup_.clear();samples_.clear();lastRevision_=0;width_=height_=0;
    deadline_=clock_.steadyNow()+std::chrono::seconds(o.timeoutSeconds);beginSamples(StarFocusPhase::Baseline);
}
void StarAutofocus::fail(const std::string& s){moves_.clear();progress_.phase=StarFocusPhase::Failed;progress_.detail=s;}
void StarAutofocus::cancel(){if(progress_.active()){moves_.clear();progress_.phase=StarFocusPhase::Cancelled;progress_.detail="Cancelled; lens left at current position";}}
void StarAutofocus::beginSamples(StarFocusPhase phase){progress_.phase=phase;samples_.clear();progress_.frames=0;discard_=2;ready_=clock_.steadyNow()+std::chrono::milliseconds(options_.settleMs);sampleDeadline_=ready_+std::chrono::seconds(12);progress_.detail="Waiting for stable star samples";}
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
            camera_.focus(direction*options_.step);moves_.pop_front();++progress_.moves;progress_.position+=direction;
            ready_=now+std::chrono::milliseconds(options_.settleMs);return;
        }
        if(now>=sampleDeadline_){fail("Not enough stable matching stars; adjust exposure or focus manually");return;}
        if(!field||revision==lastRevision_)return;lastRevision_=revision;
        if(now<ready_)return;if(discard_>0){--discard_;return;}
        sample(*field);
    }catch(const std::exception& e){fail(e.what());}
}
void StarAutofocus::sample(const StarField& f){
    if(f.width<=0||f.height<=0)return;
    if(std::any_of(f.stars.begin(),f.stars.end(),[&](const auto& s){return !std::isfinite(s.x)||!std::isfinite(s.y)||!std::isfinite(s.hfd)||!std::isfinite(s.flux)||!std::isfinite(s.snr)||s.x<0||s.x>=f.width||s.y<0||s.y>=f.height||s.hfd<=0||s.flux<=0||s.snr<=0;}))return;
    if(width_&&(f.width!=width_||f.height!=height_)){fail("Live View geometry changed");return;}
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
    if(progress_.frames>=options_.framesPerPoint)completePoint();
}
void StarAutofocus::completePoint(){
    const double hfd=median(samples_);std::vector<double> errors;for(double v:samples_)errors.push_back(std::abs(v-hfd));
    const double scatter=1.4826*median(errors);
    const auto [lo,hi]=std::minmax_element(samples_.begin(),samples_.end());
    if(scatter/hfd>options_.maxScatter||(*hi-*lo)/hfd>options_.maxScatter*3){fail("Unstable HFD samples; seeing, movement or flicker");return;}
    if(progress_.phase==StarFocusPhase::Baseline){progress_.baseline=hfd;travel(-options_.positions/2,StarFocusPhase::Sampling,true);return;}
    if(progress_.phase==StarFocusPhase::Verifying){
        progress_.verified=hfd;
        if(hfd>progress_.best*(1+options_.verificationTolerance)||hfd>progress_.baseline*(1+options_.verificationTolerance)){fail("Return did not reproduce the minimum; adjust backlash or use a smaller step");return;}
        progress_.phase=StarFocusPhase::Complete;progress_.detail="Verified stellar focus";return;
    }
    progress_.curve.push_back({progress_.position,hfd,scatter});
    if(progress_.curve.size()<static_cast<std::size_t>(options_.positions)){travel(progress_.position+1,StarFocusPhase::Sampling,false);return;}
    const auto& points=progress_.curve;
    const auto best=std::min_element(points.begin(),points.end(),[](auto a,auto b){return a.hfd<b.hfd;});
    const auto index=std::distance(points.begin(),best);progress_.best=best->hfd;
    if(index<2||index>static_cast<int>(points.size())-3){fail("Minimum outside the bracket; recenter manually or widen the sweep");return;}
    const double margin=std::max(best->hfd*.08,3*best->scatter);
    if(points.front().hfd<best->hfd+margin||points.back().hfd<best->hfd+margin){fail("Flat focus curve; motor response or sampling is insufficient");return;}
    for(std::size_t i=1;i<points.size();++i){const double tolerance=std::max(margin,3*std::max(points[i].scatter,points[i-1].scatter));
        if((i<=static_cast<std::size_t>(index)&&points[i].hfd>points[i-1].hfd+tolerance)||(i>static_cast<std::size_t>(index)&&points[i].hfd<points[i-1].hfd-tolerance)){fail("Irregular focus curve; no reliable minimum");return;}}
    // Validate a convex least-squares curve. Return to a measured point, never an
    // interpolated motor position: Canon steps are not a linear position encoder.
    double n=static_cast<double>(points.size()),s2{},s4{},sy{},sxy{},sx2y{};
    for(auto p:points){double x=p.position;s2+=x*x;s4+=x*x*x*x;sy+=p.hfd;sxy+=x*p.hfd;sx2y+=x*x*p.hfd;}
    double a=(n*sx2y-s2*sy)/(n*s4-s2*s2),b=sxy/s2,c=(sy-a*s2)/n;
    double residual{},total{};for(auto p:points){residual+=std::pow(p.hfd-(a*p.position*p.position+b*p.position+c),2);total+=std::pow(p.hfd-sy/n,2);}
    progress_.curveR2=total>0?1-residual/total:0;
    if(a<=0||progress_.curveR2<.7||std::abs(-b/(2*a)-best->position)>1.5){fail("Focus curve fit is inconsistent");return;}
    travel(best->position,StarFocusPhase::Verifying,true);
}
}
