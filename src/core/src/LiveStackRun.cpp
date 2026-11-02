#include <astro/LiveStackRun.hpp>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace astro {
Config stackPlan(const Config& input){
    if(!input.liveStack.fromSequence)return input;
    if(!input.liveStack.sourceDirectory.empty()||input.liveStack.frameType!="Light")throw std::invalid_argument("Sequence stacking requires live Light acquisition");
    if(input.sequence.empty())throw std::invalid_argument("Stack sequence plan is empty");
    auto c=input;const auto& first=c.sequence.front();std::int64_t total=0;
    for(std::size_t n=0;n<c.sequence.size();++n){const auto& row=c.sequence[n];validateItem(row);if(row.type!=FrameType::Light||row.settings!=first.settings||row.bulb!=first.bulb||std::abs(row.exposureSeconds-first.exposureSeconds)>1e-6||(n&&row.delaySeconds!=0))throw std::invalid_argument("A stack sequence must use identical Light settings; split heterogeneous rows into separate stacks");total+=row.count;c.liveStack.delaySeconds=std::max(c.liveStack.delaySeconds,row.intervalSeconds);}
    if(total<1||total>100000)throw std::invalid_argument("Invalid stack sequence frame count");
    c.liveStack.frames=int(total);c.singleCapture=first;c.liveStack.fromSequence=false;return c;
}
void validateStack(const StackOptions& o){
    if(o.frameType!="Light"&&o.frameType!="Dark"&&o.frameType!="Flat"&&o.frameType!="Bias"&&o.frameType!="Dark-flat")throw std::invalid_argument("Invalid stack frame type");
    if(o.frameType!="Light"&&(o.alignment||o.drizzleScale!=1||o.backgroundNormalization||o.gradientRemoval||!o.biasDirectory.empty()||!o.darkDirectory.empty()||!o.flatDirectory.empty()||!o.darkFlatDirectory.empty()))throw std::invalid_argument("Master acquisition requires alignment, drizzle, background processing and calibration disabled");
    if((o.drizzleScale!=1&&o.drizzleScale!=1.5&&o.drizzleScale!=2&&o.drizzleScale!=3)||!std::isfinite(o.drizzlePixfrac)||o.drizzlePixfrac<.1||o.drizzlePixfrac>1)throw std::invalid_argument("Invalid drizzle scale or drop fraction");
    if(o.solverExecutable.find('"')!=std::string::npos||o.solverDatabase.find('"')!=std::string::npos)throw std::invalid_argument("Invalid solver path");
    if(!std::isfinite(o.solverFov)||o.solverFov<0||o.solverFov>180||o.solverTimeoutSeconds<5||o.solverTimeoutSeconds>300)throw std::invalid_argument("Invalid plate solver settings");
    if(o.checkpointFrames<0||o.checkpointFrames>1000)throw std::invalid_argument("Invalid checkpoint interval");
    if(!o.flatDirectory.empty()&&o.biasDirectory.empty()&&o.darkFlatDirectory.empty())throw std::invalid_argument("Flat calibration requires bias or dark-flat frames");
    if(o.flatDirectory.empty()&&!o.darkFlatDirectory.empty())throw std::invalid_argument("Dark-flat frames require a flat directory");
    if(!std::isfinite(o.bandingStrength)||o.bandingStrength<0||o.bandingStrength>1||!std::isfinite(o.bandingMaxOffset)||o.bandingMaxOffset<=0||o.bandingMaxOffset>.1||o.bandingRadius<2||o.bandingRadius>256)throw std::invalid_argument("Invalid horizontal banding settings");
    if(o.frames<0||o.frames>100000||!std::isfinite(o.delaySeconds)||o.delaySeconds<2||o.delaySeconds>3600||
       !std::isfinite(o.detectionSnr)||o.detectionSnr<3||o.detectionSnr>30||!std::isfinite(o.maximumRms)||o.maximumRms<.1||o.maximumRms>2||
       !std::isfinite(o.sigma)||o.sigma<3||o.sigma>10||o.minimumStars<8||o.minimumStars>100||o.memoryLimitMb<512||o.memoryLimitMb>16384)
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
void LiveStackRun::start(const Config& input,const std::string& metadata){
    const Config c=stackPlan(input);
    if(status_.active)throw std::logic_error("Live stack already active");validate(c);
    sources_.clear();const bool replay=!c.liveStack.sourceDirectory.empty();
    if(replay){sources_=files_.rawFiles(pathFromUtf8(c.liveStack.sourceDirectory));if(sources_.empty())throw std::invalid_argument("Source folder has no CR2 files");}
    if(!replay&&!camera_.connected())throw CameraError("Camera is not connected");
    const auto properties=replay?std::vector<CameraProperty>{}:camera_.properties();bool raw=false,manual=false,iso=false,shutter=false;frozen_.clear();item_={};
    for(const auto& p:properties){
        frozen_[p.id]=p.value;
        if(p.id==Property::ImageQuality)raw=p.currentLabel.find("RAW")!=std::string::npos;
        if(p.id==Property::ExposureMode)manual=p.currentLabel.starts_with("M ")||p.currentLabel=="M"||p.currentLabel.find("Manual")!=std::string::npos;
        if(p.id==Property::Iso)iso=p.value!=0;
        if(p.id==Property::Shutter){shutter=true;item_.bulb=p.currentLabel=="Bulb";item_.exposureSeconds=item_.bulb?c.singleCapture.exposureSeconds:shutterSeconds(p.currentLabel);}
    }
    if(c.liveStack.frameType=="Dark"||c.liveStack.frameType=="Dark-flat")item_.type=FrameType::Dark;else if(c.liveStack.frameType=="Flat")item_.type=FrameType::Flat;else if(c.liveStack.frameType=="Bias")item_.type=FrameType::Bias;
    if(!replay&&(!raw||!manual||!iso||!shutter))throw CameraError("Live stacking requires RAW, manual exposure and fixed ISO; set them in Camera");
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
    if(replay){status_.target=int(sources_.size());status_.exposureSeconds=0;}
    status_.active=true;status_.phase="preparing";status_.detail="Live stack started";failed_=shutter_=false;started_=next_=clock_.steadyNow();log_("Info",status_.detail);
    if(input.liveStack.fromSequence)next_+=std::chrono::duration_cast<SteadyTime::duration>(std::chrono::duration<double>(input.sequence.front().delaySeconds));
}
void LiveStackRun::stop(){if(status_.active){status_.stopping=true;status_.detail="Stopping after current exposure and processing";if(status_.phase=="saving"||status_.phase=="preparing")processor_.cancelOptionalProcessing();}}
void LiveStackRun::pause(){if(status_.active&&!status_.stopping){status_.pauseRequested=true;status_.detail="Pausing after current exposure and processing";}}
void LiveStackRun::resume(){if(status_.active&&!status_.stopping){status_.pauseRequested=false;if(status_.paused){status_.paused=false;status_.phase="waiting";status_.detail="Live stack resumed";}}}
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
        if(status_.phase=="paused"){if(status_.stopping){status_.paused=false;finalize("Live stack stopped by user");}return;}
        if(status_.phase=="preparing"){
            auto r=processor_.poll();if(!r.ready)return;
            if(r.fatal||!r.prepared){fail(r.detail.empty()?"Stack initialization failed":r.detail);return;}
            status_.phase="waiting";
        }
        if(status_.phase=="processing"||status_.phase=="saving"){
            auto r=processor_.poll();if(r.preview)status_.preview=r.preview;if(!r.ready)return;
            if(r.preview)status_.preview=r.preview;
            if(r.finalized){status_.astrometry=r.astrometry;status_.finalFile=r.finalFile;status_.active=false;status_.phase=(failed_||r.fatal||(status_.accepted==0&&status_.captured>0))?"error":"complete";if(!r.detail.empty())status_.detail=r.detail;if(status_.phase=="complete"&&status_.rejected>0)status_.detail="Live stack finished with rejected frames: "+std::to_string(status_.accepted)+" / "+std::to_string(status_.captured);log_(status_.phase=="error"?"Error":(status_.rejected||r.warning)?"Warning":"Info",status_.detail);return;}
            status_.hfd=r.hfd;status_.fwhm=r.fwhm;status_.stackHfd=r.stackHfd;status_.stackFwhm=r.stackFwhm;status_.stars=r.stars;status_.matches=r.matches;status_.rms=r.rms;status_.overlap=r.overlap;
            if(r.accepted){++status_.accepted;status_.integratedSeconds+=r.exposureSeconds>0?r.exposureSeconds:item_.exposureSeconds;}else ++status_.rejected;
            log_(r.accepted?"Info":"Warning","Live stack frame "+std::to_string(status_.captured)+": "+r.detail);
            if(r.fatal){failed_=true;status_.stopping=true;status_.detail=r.detail;}
            if(status_.stopping||(status_.target>0&&status_.captured>=status_.target)){finalize(failed_?status_.detail:status_.stopping?"Live stack stopped by user":"Live stack finished");return;}
            status_.phase="waiting";status_.detail=r.detail;next_=now+std::chrono::duration_cast<SteadyTime::duration>(std::chrono::duration<double>(sources_.empty()?config_.liveStack.delaySeconds:0));
        }
        if(status_.phase=="waiting"){
            status_.delayRemaining=std::max(0.0,std::chrono::duration<double>(next_-now).count());
            if(status_.stopping){finalize("Live stack stopped by user");return;}if(now<next_)return;
            if(status_.pauseRequested){status_.paused=true;status_.phase="paused";status_.detail="Live stack paused";return;}
            if(!sources_.empty()){
                const auto path=sources_.at(status_.captured);processor_.submit({{path},0,status_.captured+1});++status_.captured;status_.phase="processing";status_.detail="Rebuilding from original CR2 files";return;
            }
            if(!camera_.connected())throw CameraError("Camera disconnected during live stack");
            const auto dir=pathFromUtf8(status_.directory);auto space=files_.available(dir);
            if(space<=config_.reserveBytes+512ull*1024*1024)throw std::runtime_error("Insufficient disk space for the next RAW and final stack");
            std::map<Property,std::uint32_t> current;for(const auto& p:camera_.properties())current[p.id]=p.value;
            if(current!=frozen_)throw CameraError("Camera settings changed during live stack; start a new stack");
            camera_.prepareHost(space-config_.reserveBytes,config_.saveDestination);
            std::ostringstream name;name<<config_.liveStack.frameType<<'_'<<std::setfill('0')<<std::setw(6)<<status_.captured+1;
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
