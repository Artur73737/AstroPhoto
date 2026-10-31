#pragma once
#include <cmath>
#include <cstdint>
#include <deque>
namespace astro::ui {
// Count unique displayed frames, never UI redraws or skipped camera revisions.
class DisplayedFrameRate {
public:
    explicit DisplayedFrameRate(double windowSeconds=1):window_(windowSeconds){}
    void reset(){times_.clear();revision_=0;lastTime_=0;}
    double update(std::uint64_t revision,double now){
        if(!revision||!std::isfinite(now)){reset();return 0;}
        if(revision<revision_||now<lastTime_)reset();
        lastTime_=now;
        while(!times_.empty()&&now-times_.front()>window_)times_.pop_front();
        if(revision!=revision_){times_.push_back(now);revision_=revision;}
        if(times_.size()<2)return 0;
        const double duration=now-times_.front();
        return duration>0?(times_.size()-1)/duration:0;
    }
private:
    double window_,lastTime_{};
    std::uint64_t revision_{};
    std::deque<double> times_;
};
}
