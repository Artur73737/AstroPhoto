#include <astro/LiveStackRun.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace astro {
void validateStack(const StackOptions& o){
    if(!std::isfinite(o.bandingStrength)||o.bandingStrength<0||o.bandingStrength>1||!std::isfinite(o.bandingMaxOffset)||o.bandingMaxOffset<=0||o.bandingMaxOffset>.1||o.bandingRadius<2||o.bandingRadius>256)throw std::invalid_argument("Invalid horizontal banding settings");
    if(o.frames<0||o.frames>100000||!std::isfinite(o.delaySeconds)||o.delaySeconds<2||o.delaySeconds>3600||
       !std::isfinite(o.detectionSnr)||o.detectionSnr<3||o.detectionSnr>30||!std::isfinite(o.maximumRms)||o.maximumRms<.1||o.maximumRms>2||
       !std::isfinite(o.sigma)||o.sigma<3||o.sigma>10||o.minimumStars<8||o.minimumStars>100||o.memoryLimitMb<512||o.memoryLimitMb>8192)
        throw std::invalid_argument("Invalid live stack settings (delay must be at least 2 seconds)");
}
void validateStackDisplay(const StackDisplay& d){if(!std::isfinite(d.low)||!std::isfinite(d.mid)||!std::isfinite(d.high)||d.low<0||d.high>1||d.low>=d.high||d.mid<=.001f||d.mid>=.999f)throw std::invalid_argument("Invalid stack display levels");}
namespace {
double shutterSeconds(const std::string& s){
    auto end=s.find(" s");if(end==std::string::npos)throw CameraError("Select a timed shutter or Bulb before live stacking");
    auto value=s.substr(0,end);std::size_t used{};auto slash=value.find('/');double t{};
    if(slash!=std::string::npos){auto n=std::stod(value.substr(0,slash),&used);if(used!=slash)throw CameraError("Invalid shutter duration");auto den=std::stod(value.substr(slash+1),&used);if(used!=value.size()-slash-1||den<=0)throw CameraError("Invalid shutter duration");t=n/den;}
    else {t=std::stod(value,&used);if(used!=value.size())throw CameraError("Invalid shutter duration");}
    if(!std::isfinite(t)||t<=0||t>86400)throw CameraError("Invalid shutter duration");return t;
}
}
LiveStackRun::LiveStackRun(ICamera& c,IFileSystem& f,IClock& t,IStackProcessor& p,Log l):camera_(c),files_(f),clock_(t),processor_(p),log_(std::move(l)){}
void LiveStackRun::start(const Config& c,const std::string& metadata){
    if(status_.active)throw std::logic_error("Live stack already active");validate(c);
    if(!camera_.connected())throw CameraError("Camera is not connected");
    const auto properties=camera_.properties();bool raw=false,manual=false,iso=false,shutter=false;frozen_.clear();item_={};
    for(const auto& p:properties){
        frozen_[p.id]=p.value;
        if(p.id==Property::ImageQuality)raw=p.currentLabel.find("RAW")!=std::string::npos;
        if(p.id==Property::ExposureMode)manual=p.currentLabel.starts_with("M ")||p.currentLabel=="M"||p.currentLabel.find("Manual")!=std::string::npos;
        if(p.id==Property::Iso)iso=p.value!=0;
        if(p.id==Property::Shutter){shutter=true;item_.bulb=p.currentLabel=="Bulb";item_.exposureSeconds=item_.bulb?c.singleCapture.exposureSeconds:shutterSeconds(p.currentLabel);}
    }
    if(!raw||!manual||!iso||!shutter)throw CameraError("Live stacking requires RAW, manual exposure and fixed ISO; set them in Camera");
    FileOrganizer organizer(files_,clock_);organizer.prepare(c);
    auto now=clock_.utcNow();auto time=std::chrono::system_clock::to_time_t(now);std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm,&time);
#else
    gmtime_r(&time,&tm);
#endif
    std::ostringstream stamp;stamp<<std::put_time(&tm,"%Y%m%dT%H%M%S")<<'_'<<std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000<<'Z';
    auto base=organizer.session(c)/"live-stacked"/stamp.str(),dir=base;
    for(int suffix=1;files_.exists(dir);++suffix){dir=base;dir+="_"+std::to_string(suffix);}
    files_.createDirectories(dir/"frames");
    if(files_.available(dir)<=c.reserveBytes+512ull*1024*1024)throw std::runtime_error("Insufficient disk space for live stack");
    processor_.begin(dir,c.liveStack,metadata);
    config_=c;status_={};auto u=dir.u8string();status_.directory={u.begin(),u.end()};status_.target=c.liveStack.frames;status_.exposureSeconds=item_.exposureSeconds;
    status_.active=true;status_.phase="preparing";status_.detail="Live stack started";failed_=shutter_=false;started_=next_=clock_.steadyNow();log_("Info",status_.detail);
}
void LiveStackRun::stop(){if(status_.active){status_.stopping=true;status_.detail="Stopping after current exposure and processing";}}
void LiveStackRun::finalize(const std::string& reason){processor_.finish(reason,failed_);status_.phase="saving";status_.detail=reason;}
void LiveStackRun::fail(const std::string& why){
    if(!status_.active)return;failed_=true;status_.stopping=true;status_.detail=why;
    if(shutter_){try{camera_.abortCapture();}catch(const std::exception& e){status_.detail+="; ";status_.detail+=e.what();}shutter_=false;}
    log_("Error",status_.detail);
    if(status_.phase!="processing"&&status_.phase!="saving")finalize(status_.detail);
}
void LiveStackRun::tick(){
    if(!status_.active)return;
    try{
        const auto now=clock_.steadyNow();status_.elapsedSeconds=std::chrono::duration<double>(now-started_).count();
        if(status_.phase=="preparing"){
            auto r=processor_.poll();if(!r.ready)return;
            if(r.fatal||!r.prepared){fail(r.detail.empty()?"Stack initialization failed":r.detail);return;}
            status_.phase="waiting";
        }
        if(status_.phase=="processing"||status_.phase=="saving"){
            auto r=processor_.poll();if(r.preview)status_.preview=r.preview;if(!r.ready)return;
            if(r.preview)status_.preview=r.preview;
            if(r.finalized){status_.finalFile=r.finalFile;status_.active=false;status_.phase=(failed_||r.fatal||(status_.accepted==0&&status_.captured>0))?"error":"complete";if(!r.detail.empty())status_.detail=r.detail;if(status_.phase=="complete"&&status_.rejected>0)status_.detail="Live stack finished with rejected frames: "+std::to_string(status_.accepted)+" / "+std::to_string(status_.captured);log_(status_.phase=="error"?"Error":status_.rejected?"Warning":"Info",status_.detail);return;}
            status_.stars=r.stars;status_.matches=r.matches;status_.rms=r.rms;status_.overlap=r.overlap;
            if(r.accepted){++status_.accepted;status_.integratedSeconds+=item_.exposureSeconds;}else ++status_.rejected;
            log_(r.accepted?"Info":"Warning","Live stack frame "+std::to_string(status_.captured)+": "+r.detail);
            if(r.fatal){failed_=true;status_.stopping=true;status_.detail=r.detail;}
            if(status_.stopping||(status_.target>0&&status_.captured>=status_.target)){finalize(failed_?status_.detail:status_.stopping?"Live stack stopped by user":"Live stack finished");return;}
            status_.phase="waiting";status_.detail=r.detail;next_=now+std::chrono::duration_cast<SteadyTime::duration>(std::chrono::duration<double>(config_.liveStack.delaySeconds));
        }
        if(status_.phase=="waiting"){
            status_.delayRemaining=std::max(0.0,std::chrono::duration<double>(next_-now).count());
            if(status_.stopping){finalize("Live stack stopped by user");return;}if(now<next_)return;
            if(!camera_.connected())throw CameraError("Camera disconnected during live stack");
            const auto dir=pathFromUtf8(status_.directory);auto space=files_.available(dir);
            if(space<=config_.reserveBytes+512ull*1024*1024)throw std::runtime_error("Insufficient disk space for the next RAW and final stack");
            std::map<Property,std::uint32_t> current;for(const auto& p:camera_.properties())current[p.id]=p.value;
            if(current!=frozen_)throw CameraError("Camera settings changed during live stack; start a new stack");
            camera_.prepareHost(space-config_.reserveBytes,config_.saveDestination);
            std::ostringstream name;name<<"Light_"<<std::setfill('0')<<std::setw(6)<<status_.captured+1;
            shutter_=true; // never retry an ambiguous shutter command
            camera_.beginCapture(item_,dir/"frames"/name.str());exposureStart_=now;deadline_=now+std::chrono::duration_cast<SteadyTime::duration>(std::chrono::duration<double>(item_.exposureSeconds+config_.downloadTimeoutSeconds));
            status_.phase="exposing";status_.detail="Exposure / RAW download";status_.delayRemaining=0;
        }
        if(status_.phase=="exposing"){
            status_.exposureElapsed=std::min(item_.exposureSeconds,std::chrono::duration<double>(now-exposureStart_).count());
            if(now>deadline_)throw CameraError("Live stack capture/download timed out");
            auto capture=camera_.pollCapture();if(!capture.complete)return;shutter_=false;
            if(capture.files.empty())throw CameraError("Capture finished without a downloaded file");
            ++status_.captured;processor_.submit({capture.files,item_.exposureSeconds,status_.captured});status_.phase="processing";status_.detail="Decoding RAW / aligning / integrating";
        }
    }catch(const std::exception& e){if(status_.phase=="saving"){status_.active=false;status_.phase="error";status_.detail=e.what();log_("Error",e.what());}else fail(e.what());}
}
}
