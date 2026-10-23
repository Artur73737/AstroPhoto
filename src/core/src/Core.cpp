#include <astro/Core.hpp>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace astro {
std::filesystem::path pathFromUtf8(const std::string& value) {return std::filesystem::path(std::u8string(value.begin(),value.end()));}
std::string propertyName(Property p) {
    switch(p) {
    case Property::Iso:return "ISO"; case Property::Aperture:return "Av"; case Property::Shutter:return "Tv";
    case Property::ExposureMode:return "AE"; case Property::WhiteBalance:return "WB";
    case Property::ColorTemperature:return "Kelvin"; case Property::PictureStyle:return "PictureStyle";
    case Property::ImageQuality:return "ImageQuality";
    } throw std::invalid_argument("Unknown property");
}
std::string frameName(FrameType t) {
    switch(t) {case FrameType::Light:return "Light";case FrameType::Dark:return "Dark";case FrameType::Flat:return "Flat";case FrameType::Bias:return "Bias";}
    throw std::invalid_argument("Unknown frame type");
}
std::string stateName(SequenceState s) {
    switch(s) {case SequenceState::Idle:return "Pronto";case SequenceState::Running:return "In esecuzione";case SequenceState::Paused:return "In pausa";case SequenceState::Stopping:return "Arresto";case SequenceState::Error:return "Errore";}
    return "?";
}
static void validateComponent(const std::string& s) {
    if(s.empty() || s=="." || s==".." || s.find_first_of("<>:\"/\\|?*")!=std::string::npos || s.back()=='.' || s.back()==' ' || std::any_of(s.begin(),s.end(),[](unsigned char c){return c<32;}))
        throw std::invalid_argument("Nome sessione non valido");
    auto name=s.substr(0,s.find('.')); std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
    if(name=="CON"||name=="PRN"||name=="AUX"||name=="NUL"||(name.size()==4&&(name.starts_with("COM")||name.starts_with("LPT"))&&name[3]>='1'&&name[3]<='9'))
        throw std::invalid_argument("Nome sessione riservato da Windows");
}
void validateItem(const SequenceItem& i) {
    (void)frameName(i.type);
    if(!std::isfinite(i.intervalSeconds)||i.intervalSeconds<0||i.intervalSeconds>86400)throw std::invalid_argument("Intervallo tra scatti non valido");
    if(i.count<=0 || !std::isfinite(i.exposureSeconds) || i.exposureSeconds<=0 || i.exposureSeconds>86400 || !std::isfinite(i.delaySeconds) || i.delaySeconds<0 || i.delaySeconds>86400)
        throw std::invalid_argument("Quantita/esposizione/ritardo non validi (durata massima 24h)");
    for(const auto& [p,v]:i.settings) { (void)propertyName(p); (void)v; }
}
void validate(const Config& c) {
    validateStarFocus(c.starFocus);
    if(c.saveDestination!=SaveDestination::PcOnly&&c.saveDestination!=SaveDestination::PcAndCamera)throw std::invalid_argument("Invalid save destination");
    if(c.crosshairShape!="cross"&&c.crosshairShape!="gap_cross"&&c.crosshairShape!="circle"&&c.crosshairShape!="target"&&c.crosshairShape!="thirds")throw std::invalid_argument("Invalid crosshair shape");
    if(c.language!="it"&&c.language!="en")throw std::invalid_argument("Lingua non supportata / Unsupported language");
    if(!std::isfinite(c.peakingThreshold)||c.peakingThreshold<10||c.peakingThreshold>100)throw std::invalid_argument("Soglia peaking fuori intervallo / Peaking threshold out of range");
    if(!std::isfinite(c.uiScale)||c.uiScale<0.75f||c.uiScale>2.0f)throw std::invalid_argument("Scala UI fuori intervallo (0.75 - 2.0)");
    validateComponent(c.sessionName);
    if(c.outputDirectory.empty()||c.tickMs<1||c.liveViewMs<1||c.downloadTimeoutSeconds<1||c.busyRetries<0||c.busyRetries>100||c.retryDelayMs<1||c.maxLogEntries<1||c.windowWidth<640||c.windowHeight<480)
        throw std::invalid_argument("Configurazione fuori intervallo");
    for(const auto& i:c.sequence) validateItem(i);
    validateItem(c.singleCapture);
}
FileOrganizer::FileOrganizer(IFileSystem& f,IClock& c):files_(f),clock_(c){}
void FileOrganizer::prepare(const Config& c) {validate(c); files_.createDirectories(c.outputDirectory/c.sessionName);}
std::filesystem::path FileOrganizer::stem(const Config& c,const SequenceItem& i,std::uint64_t counter) {
    validate(c); validateItem(i);
    auto dir=c.outputDirectory/c.sessionName/frameName(i.type); files_.createDirectories(dir);
    auto now=clock_.utcNow(); auto time=std::chrono::system_clock::to_time_t(now); std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm,&time);
#else
    gmtime_r(&time,&tm);
#endif
    std::ostringstream name;
    name<<std::put_time(&tm,"%Y%m%dT%H%M%S")<<'_'<<std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()%1000<<"Z_"<<std::setfill('0')<<std::setw(6)<<counter;
    if(i.bulb) name<<'_'<<i.exposureSeconds<<"s";
    for(auto p:{Property::Iso,Property::Aperture,Property::Shutter}) if(auto it=i.settings.find(p);it!=i.settings.end()) name<<'_'<<propertyName(p)<<"-0x"<<std::hex<<it->second<<std::dec;
    return dir/name.str();
}
SequenceEngine::SequenceEngine(ICamera& c,IFileSystem& f,IClock& t,Log l):camera_(c),files_(f),clock_(t),log_(std::move(l)),organizer_(f,t){}
bool SequenceEngine::active() const {return progress_.state==SequenceState::Running||progress_.state==SequenceState::Paused||progress_.state==SequenceState::Stopping;}
void SequenceEngine::start(const Config& c) {
    if(active()) throw std::logic_error("Sequenza gia attiva");
    validate(c); if(c.sequence.empty()) throw std::invalid_argument("Sequenza vuota");
    if(!camera_.connected()) throw CameraError("Fotocamera non connessa");
    organizer_.prepare(c);
    const auto space=files_.available(c.outputDirectory/c.sessionName);
    if(space<=c.reserveBytes) throw std::runtime_error("Spazio disco insufficiente");
    camera_.prepareHost(space-c.reserveBytes,c.saveDestination);
    config_=c; progress_={}; for(const auto& i:c.sequence) progress_.total+=static_cast<std::uint64_t>(i.count);
    progress_.state=SequenceState::Running; progress_.detail="Preparazione";
    shotInRow_=0; retries_=0; captureStarted_=false; phase_=Phase::Configure; retryAt_={};
    nextCaptureAt_=clock_.steadyNow();
    log_("Info","Sequenza avviata");
}
void SequenceEngine::startSingle(const Config& c) {
    auto single=c;single.sequence={c.singleCapture};single.sequence[0].count=1;
    start(single);log_("Info","Scatto singolo: "+frameName(c.singleCapture.type));
}
void SequenceEngine::pause() {if(progress_.state==SequenceState::Running) progress_.pauseRequested=true;}
void SequenceEngine::resume() {if(progress_.state==SequenceState::Paused) progress_.state=SequenceState::Running; progress_.pauseRequested=false;}
void SequenceEngine::fail(const std::string& message) {
    auto detail=message;
    if(captureStarted_) {try {camera_.abortCapture(); captureStarted_=false;} catch(const std::exception& e){detail+="; arresto: ";detail+=e.what();}}
    progress_.state=SequenceState::Error;progress_.detail=detail;log_("Error",detail);
}
void SequenceEngine::stop() {
    if(!active() && !captureStarted_) return;
    progress_.state=SequenceState::Stopping;
    try { if(captureStarted_) camera_.abortCapture(); captureStarted_=false;progress_.state=SequenceState::Idle;progress_.pauseRequested=false;progress_.detail="Sequenza interrotta";log_("Info",progress_.detail); }
    catch(const std::exception& e){fail(e.what());}
}
void SequenceEngine::tick() {
    if(progress_.state!=SequenceState::Running) return;
    try {
        if(!camera_.connected()) throw CameraError("Fotocamera disconnessa durante la sequenza");
        const auto now=clock_.steadyNow(); if(now<retryAt_) return;
        const auto& item=config_.sequence.at(progress_.row);
        if(phase_==Phase::Configure) {
            if(progress_.pauseRequested) {progress_.state=SequenceState::Paused;return;}
            const auto available=files_.available(config_.outputDirectory/config_.sessionName);
            if(available<=config_.reserveBytes) throw std::runtime_error("Spazio disco insufficiente");
            for(const auto& [p,v]:item.settings) camera_.set(p,v);
            deadline_=std::max(nextCaptureAt_,now+std::chrono::duration_cast<SteadyTime::duration>(std::chrono::duration<double>(shotInRow_==0?item.delaySeconds:0)));
            phase_=Phase::Delay; progress_.detail="Attesa prima dello scatto";retries_=0;
        }
        if(phase_==Phase::Delay && progress_.pauseRequested) {progress_.state=SequenceState::Paused;return;}
        if(phase_==Phase::Delay && now<deadline_)progress_.detail="Prossimo scatto tra "+std::to_string(static_cast<int>(std::ceil(std::chrono::duration<double>(deadline_-now).count())))+" s";
        if(phase_==Phase::Delay && now>=deadline_) phase_=Phase::Capture;
        if(phase_==Phase::Capture) {
            // Never automatically repeat a shutter command: completion can be ambiguous.
            captureStarted_=true;
            camera_.beginCapture(item,organizer_.stem(config_,item,++counter_));
            deadline_=now+std::chrono::duration_cast<SteadyTime::duration>(std::chrono::duration<double>(item.exposureSeconds+config_.downloadTimeoutSeconds));
            phase_=Phase::Download;progress_.detail="Esposizione / download";
        }
        if(phase_==Phase::Download) {
            auto result=camera_.pollCapture();
            if(result.complete) {
                if(result.files.empty()) throw CameraError("Trasferimento senza file");
                captureStarted_=false; ++progress_.completed; ++shotInRow_;
                ++lastCapture_.revision;lastCapture_.type=item.type;lastCapture_.files=result.files;
                nextCaptureAt_=now+std::chrono::duration_cast<SteadyTime::duration>(std::chrono::duration<double>(item.intervalSeconds));
                for(const auto& path:result.files) log_("Info","Salvato: "+path.string());
                if(shotInRow_>=item.count) {++progress_.row;shotInRow_=0;}
                phase_=Phase::Configure;
                if(progress_.completed==progress_.total) {progress_.state=SequenceState::Idle;progress_.pauseRequested=false;progress_.detail="Sequenza completata";log_("Info",progress_.detail);}
            } else if(now>=deadline_) throw CameraError("Timeout in attesa del download; verificare la fotocamera prima di riprovare");
        }
    } catch(const CameraError& e) {
        if(e.retryable && phase_==Phase::Configure && retries_<config_.busyRetries) {
            ++retries_; retryAt_=clock_.steadyNow()+std::chrono::milliseconds(static_cast<std::int64_t>(config_.retryDelayMs)*retries_);log_("Warning",e.what());
        } else fail(e.what());
    } catch(const std::exception& e) {fail(e.what());}
}
void Mailbox::send(Command c) {std::lock_guard lock(mutex_);commands_.push_back(std::move(c));}
std::vector<Command> Mailbox::take() {std::lock_guard lock(mutex_);std::vector<Command> out;out.swap(commands_);return out;}
Snapshot Mailbox::snapshot() const {std::lock_guard lock(mutex_);return snapshot_;}
void Mailbox::publish(const Snapshot& s) {std::lock_guard lock(mutex_);snapshot_=s;}
}
