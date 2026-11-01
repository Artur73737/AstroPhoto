#pragma once
#include <astro/Core.hpp>
#include <cmath>
namespace astro {
// Owned by the camera worker. Only a fresh camera AF confirmation means OK.
class PointFocusRun {
public:
    PointFocusRun(ICamera& camera,IClock& clock):camera_(camera),clock_(clock){}
    const PointAfStatus& status()const{return status_;}
    void reset(){if(!status_.active())status_={};}
    void start(double u,double v,int timeoutSeconds=30){
        if(status_.active())throw CameraError("Point autofocus is already running");
        if(!std::isfinite(u)||!std::isfinite(v)||u<0||u>1||v<0||v>1||timeoutSeconds<3||timeoutSeconds>60)
            throw std::invalid_argument("Invalid point autofocus coordinates or timeout");
        status_={PointAfPhase::Running,u,v,"Point autofocus: focusing"};
        deadline_=clock_.steadyNow()+std::chrono::seconds(timeoutSeconds);
        try{camera_.startPointAutofocus(u,v);}catch(const std::exception& e){finish(PointAfPhase::Failed,e.what());throw;}
    }
    void tick(){
        if(!status_.active())return;
        try{
            if(!camera_.connected())throw CameraError("Point autofocus: camera disconnected");
            if(clock_.steadyNow()>=deadline_){fail("Point autofocus: timed out, focus not confirmed");return;}
            auto result=camera_.pollPointAutofocus();
            if(result.active()){status_.u=result.u;status_.v=result.v;}
            if(result.phase==PointAfPhase::Complete||result.phase==PointAfPhase::Failed){camera_.stopPointAutofocus();status_=std::move(result);}
        }catch(const std::exception& e){fail(e.what());}
    }
    void cancel(){if(status_.active())finish(PointAfPhase::Cancelled,"Point autofocus: cancelled");}
    void fail(const std::string& reason){if(status_.active())finish(PointAfPhase::Failed,reason);}
private:
    void finish(PointAfPhase phase,const std::string& detail){
        status_.phase=phase;status_.detail=detail;
        try{camera_.stopPointAutofocus();}catch(const std::exception& e){
            status_.phase=PointAfPhase::Failed;status_.detail+="; ";status_.detail+=e.what();
            // Never unlock camera controls with a possibly running AF motor.
            try{camera_.disconnect();}catch(const std::exception& cleanup){status_.detail+="; ";status_.detail+=cleanup.what();}
        }
    }
    ICamera& camera_;IClock& clock_;PointAfStatus status_;SteadyTime deadline_{};
};
}
