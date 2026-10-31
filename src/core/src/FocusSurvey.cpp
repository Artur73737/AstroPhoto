#include <astro/FocusSurvey.hpp>
#include <algorithm>
#include <cmath>
namespace astro {
namespace {
double median(std::vector<double> v){std::sort(v.begin(),v.end());return v[v.size()/2];}
StarField stable(const std::vector<StarField>& frames,const StarFocusOptions& options){
    StarField result;if(frames.empty())return result;result.width=frames.back().width;result.height=frames.back().height;
    std::vector<std::vector<bool>> used;for(const auto& frame:frames)used.emplace_back(frame.stars.size(),false);
    for(auto star:frames.back().stars){
        if(!std::isfinite(star.hfd)||star.hfd<=0||!std::isfinite(star.flux)||star.flux<=0)continue;
        std::vector<double> values;std::vector<std::size_t> matches;
        for(std::size_t frameIndex=0;frameIndex<frames.size();++frameIndex){const auto& frame=frames[frameIndex];std::size_t found=frame.stars.size();double distance=options.matchRadius;
            for(std::size_t i=0;i<frame.stars.size();++i){const auto& s=frame.stars[i];if(!used[frameIndex][i]&&std::isfinite(s.hfd)&&s.hfd>0&&s.flux>star.flux*.5&&s.flux<star.flux*2){const double d=std::hypot(s.x-star.x,s.y-star.y);if(d<distance){distance=d;found=i;}}}
            if(found==frame.stars.size())break;
            values.push_back(frame.stars[found].hfd);matches.push_back(found);
        }
        if(values.size()!=frames.size())continue;
        star.hfd=median(values);std::vector<double> deviations;for(double v:values)deviations.push_back(std::abs(v-star.hfd));
        if(1.4826*median(deviations)>star.hfd*options.maxScatter)continue;
        for(std::size_t i=0;i<matches.size();++i)used[i][matches[i]]=true;
        result.stars.push_back(star);
    }
    if(result.stars.size()<static_cast<std::size_t>(options.minStars))result.stars.clear();return result;
}
double hfd(const StarField& f){std::vector<double> v;for(auto s:f.stars)v.push_back(s.hfd);return v.empty()?0:median(v);}
std::vector<double> paired(const StarField& current,const StarField& reference,double radius){
    std::vector<double> delta;std::vector<bool> used(current.stars.size());
    for(auto s:reference.stars){std::size_t found=current.stars.size();double distance=radius;
        for(std::size_t i=0;i<current.stars.size();++i)if(!used[i]){const auto t=current.stars[i];const double d=std::hypot(t.x-s.x,t.y-s.y);if(d<distance&&t.flux>s.flux*.5&&t.flux<s.flux*2){distance=d;found=i;}}
        if(found<current.stars.size()){used[found]=true;delta.push_back(current.stars[found].hfd-s.hfd);}
    }return delta;
}
}
void FocusSurvey::start(const StarFocusOptions& o){
    validateStarFocus(o);options_=o;progress_={};progress_.phase=StarFocusPhase::Homing;progress_.motorStep=3;progress_.surveyUsed=true;
    progress_.detail="Coarse survey: seeking near side; travel limit unconfirmed";
    leg_=Leg::Near;legMoves_=discard_=pendingMove_=returnMoves_=width_=height_=0;moving_=false;revision_=moveRevision_=0;samples_.clear();best_={};
    deadline_=clock_.steadyNow()+std::chrono::seconds(o.timeoutSeconds);lastFrameAt_=ready_=clock_.steadyNow();
}
void FocusSurvey::fail(const std::string& reason){moving_=false;progress_.phase=StarFocusPhase::Failed;progress_.detail=reason;}
void FocusSurvey::cancel(){if(progress_.active()){moving_=false;progress_.phase=StarFocusPhase::Cancelled;progress_.detail="Coarse survey cancelled; lens left at current position";}}
void FocusSurvey::move(int direction){
    if(progress_.moves>=options_.maxMoves){fail("Coarse survey movement budget exhausted");return;}
    pendingMove_=direction;moving_=true;samples_.clear();
}
void FocusSurvey::tick(const StarField* field,std::uint64_t revision){
    if(!progress_.active())return;
    try{
        const auto now=clock_.steadyNow();
        if(now>=deadline_){fail("Coarse survey time limit reached");return;}
        if(!camera_.connected()){fail("Camera disconnected during coarse survey");return;}
        const bool fresh=field&&revision!=revision_;if(fresh){revision_=revision;lastFrameAt_=now;}
        if(now-lastFrameAt_>=std::chrono::seconds(12)){fail("No fresh frames during coarse survey");return;}
        if(now<ready_)return;
        if(moving_){
            if(moveRevision_==revision_)return;
            if(camera_.lensFocusStatus().state!=LensFocusState::Ready||!camera_.manualExposureForFocus()){fail("Coarse survey requires a connected AF lens and fixed manual exposure");return;}
            camera_.focus(pendingMove_*3);moveRevision_=revision_;moving_=false;++progress_.moves;++legMoves_;progress_.surveyPosition+=pendingMove_;
            progress_.surveyMoves=progress_.moves;discard_=2;progress_.frames=0;ready_=now+std::chrono::milliseconds(options_.settleMs);return;
        }
        if(!fresh)return;if(discard_){--discard_;return;}sample(*field);
    }catch(const std::exception& e){fail(e.what());}
}
void FocusSurvey::sample(const StarField& f){
    if(f.width<=0||f.height<=0)return;
    if(width_&&(f.width!=width_||f.height!=height_)){fail("Live View geometry changed during coarse survey");return;}
    width_=f.width;height_=f.height;
    samples_.push_back(f);progress_.frames=static_cast<int>(samples_.size());progress_.requiredFrames=options_.framesPerPoint;
    if(progress_.frames>=options_.framesPerPoint)point();
}
void FocusSurvey::point(){
    const auto current=stable(samples_,options_);samples_.clear();progress_.current=hfd(current);progress_.stars=static_cast<int>(current.stars.size());
    if(leg_==Leg::Near){
        // Every large command gets a fresh observation, even when zero stars
        // are visible. Bounded near seeking never asserts a blind endpoint.
        if(legMoves_<options_.acquisitionNearMoves){move(-1);return;}
        leg_=Leg::Far;legMoves_=0;progress_.detail="Coarse survey: measuring every large step toward infinity";
    }
    if(leg_==Leg::Far){
        const auto differences=paired(current,best_,options_.matchRadius);
        const bool better=best_.stars.empty()||
            (differences.size()>=static_cast<std::size_t>(options_.minStars)?median(differences)<-hfd(best_)*.01:current.stars.size()>best_.stars.size());
        if(!current.stars.empty()&&better){best_=current;progress_.surveyBestPosition=progress_.surveyPosition;progress_.surveyBestHfd=progress_.current;progress_.surveyBestStars=progress_.stars;}
        const int reserve=std::abs(progress_.surveyPosition-progress_.surveyBestPosition)+4;
        if(legMoves_<options_.acquisitionFarMoves&&progress_.moves+reserve+1<options_.maxMoves){move(1);return;}
        if(best_.stars.empty()){fail("Coarse survey found no stable stars; check illumination and search range");return;}
        leg_=Leg::Return;progress_.detail="Coarse survey: returning to the best measured star field";
    }
    const auto differences=paired(current,best_,options_.matchRadius);
    const bool reproduced=differences.size()>=static_cast<std::size_t>(options_.minStars)&&median(differences)<=hfd(best_)*options_.verificationTolerance;
    if(leg_==Leg::Return){
        // At a physical endpoint, successful commands may have no motion.
        // Recover the optical field, never a nominal command coordinate.
        if(++returnMoves_>options_.acquisitionFarMoves||progress_.moves+2>=options_.maxMoves){fail("Coarse survey return not reproduced; stellar focus unconfirmed");return;}
        if(reproduced)leg_=Leg::Approach;
        move(-1);return;
    }
    if(leg_==Leg::Approach){leg_=Leg::Verify;move(1);return;}
    if(!reproduced){leg_=Leg::Return;move(-1);return;}
    progress_.surveyReturnVerified=true;progress_.phase=StarFocusPhase::Complete;
    progress_.detail="Coarse survey return verified; optical travel limits remain unknown";
}
}
