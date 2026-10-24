#include <astro/Infrastructure.hpp>
#include <astro/ConfigManager.hpp>
#include <astro/StarAutofocus.hpp>
#ifdef ASTRO_WITH_EDSDK
#include <astro/EdsCamera.hpp>
#endif
#include <fstream>
#include <iomanip>
#include <sstream>
#include <condition_variable>
namespace astro {
bool canonBackendAvailable() {
#ifdef ASTRO_WITH_EDSDK
    return true;
#else
    return false;
#endif
}
std::unique_ptr<ICamera> createCanonCamera(const Config& c,Log log) {
#ifdef ASTRO_WITH_EDSDK
    return std::make_unique<edsdk::EdsCamera>(c,std::move(log));
#else
    (void)c;(void)log;throw std::runtime_error("EDSDK non installato: ricompilare con ASTRO_WITH_EDSDK=ON");
#endif
}
CameraWorker::CameraWorker(Mailbox& mailbox,Config config,std::filesystem::path configPath,StarAnalyzer analyzer,CameraFactory factory,PreviewConsumer preview) {
    thread_=std::jthread([&mailbox,config=std::move(config),configPath=std::move(configPath),analyzer=std::move(analyzer),factory=std::move(factory),preview=std::move(preview)](std::stop_token stop) mutable {
        Snapshot state;state.backendAvailable=bool(factory)||canonBackendAvailable();
        state.coreThreadId=std::hash<std::thread::id>{}(std::this_thread::get_id());
        std::mutex timerMutex;std::condition_variable_any timer;
        state.backendStatus=state.backendAvailable?"Canon EDSDK disponibile":"EDSDK assente: editor disponibile, controllo hardware disabilitato";
        std::ofstream logfile(configPath.parent_path()/"astro.log",std::ios::app);
        auto log=[&](const std::string& level,const std::string& message) {
            auto time=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); std::tm tm{};
#ifdef _WIN32
            localtime_s(&tm,&time);
#else
            localtime_r(&time,&tm);
#endif
            std::ostringstream stamp;stamp<<std::put_time(&tm,"%Y-%m-%d %H:%M:%S");
            state.logs.push_back({stamp.str(),level,message});
            while(state.logs.size()>static_cast<std::size_t>(config.maxLogEntries)) state.logs.pop_front();
            if(logfile) {logfile<<stamp.str()<<" ["<<level<<"] "<<message<<'\n';logfile.flush();}
        };
        try {
            log("Info",state.backendStatus);
            if(!logfile) log("Warning","Impossibile aprire astro.log; log disponibile solo nella UI");
            SystemClock clock;DiskFileSystem files;
            std::unique_ptr<ICamera> camera;std::unique_ptr<SequenceEngine> engine;
            if(state.backendAvailable) {
                try {camera=factory?factory(config,log):createCanonCamera(config,log);engine=std::make_unique<SequenceEngine>(*camera,files,clock,log);}
                catch(const std::exception& e) {camera.reset();state.backendAvailable=false;state.backendStatus=e.what();log("Error",e.what());}
            }
            auto nextFrame=clock.steadyNow();
            std::unique_ptr<StarAutofocus> autofocus;if(camera)autofocus=std::make_unique<StarAutofocus>(*camera,clock);
            std::map<Property,std::uint32_t> focusProperties;auto nextFocusCheck=nextFrame;int focusZoom{};
            StarFocusOptions focusOptions=config.starFocus;
            bool reconnectPending{},connectionWanted{};int reconnectCount{};
            auto reconnectAt=nextFrame,warningAt=nextFrame;
            auto warn=[&](const std::string& message){if(clock.steadyNow()>=warningAt){log("Warning",message);warningAt=clock.steadyNow()+std::chrono::seconds(5);}};
            auto lostConnection=[&](const std::string& message){
                if(autofocus->progress().active())autofocus->fail(message);
                if(engine->active())engine->reportError(message);else if(state.connected)log("Error",message);else warn(message);
                state.liveView=false;state.frame={};state.stars={};
                try{camera->disconnect();}catch(const std::exception& cleanup){log("Warning",cleanup.what());}
                if(connectionWanted&&!reconnectPending){reconnectPending=config.reconnectAttempts>0;reconnectCount=0;reconnectAt=clock.steadyNow()+std::chrono::milliseconds(config.reconnectDelayMs);}
            };
            auto nextLens=nextFrame;
            auto refreshLens=[&] {
                try {state.lensFocus=camera->lensFocusStatus();state.manualFocusExposure=camera->manualExposureForFocus();}
                catch(const CameraError& e){state.manualFocusExposure=false;state.lensFocus={LensFocusState::Unknown,e.what()};if(e.disconnected)throw;warn(e.what());}
                catch(const std::exception& e){state.manualFocusExposure=false;state.lensFocus={LensFocusState::Unknown,e.what()};warn(e.what());}
                nextLens=clock.steadyNow()+std::chrono::milliseconds(config.liveViewMs*5);
            };
            auto refresh=[&] {
                refreshLens();
                state.properties=camera->properties();
                for(auto& p:state.properties) if(auto labels=config.labels.find(p.id);labels!=config.labels.end()) {
                    for(auto& choice:p.choices) if(auto name=labels->second.find(choice.value);name!=labels->second.end()) choice.label=name->second;
                    if(auto name=labels->second.find(p.value);name!=labels->second.end())p.currentLabel=name->second;
                }
            };
            while(!stop.stop_requested()) {
                for(auto& cmd:mailbox.take()) {
                    try {
                        if(cmd.action==Action::SaveConfig) {ConfigManager::save(configPath,cmd.config);if(autofocus&&!autofocus->progress().active())focusOptions=cmd.config.starFocus;log("Info","Configurazione salvata: "+configPath.string());mailbox.complete(cmd.id);continue;}
                        if(!camera) throw std::runtime_error(state.backendStatus);
                        if(autofocus->progress().active()&&cmd.action!=Action::StopStarFocus&&cmd.action!=Action::Stop&&cmd.action!=Action::Disconnect&&!(cmd.action==Action::LiveView&&cmd.value==0))throw std::logic_error("Stop stellar autofocus before changing camera settings");
                        if(engine->active() && cmd.action!=Action::Pause && cmd.action!=Action::Resume && cmd.action!=Action::Stop)
                            throw std::logic_error("Arrestare la sequenza prima di modificare la fotocamera");
                        switch(cmd.action) {
                        case Action::Connect: connectionWanted=true;reconnectPending=false;state.model=camera->connect();refresh();log("Info","Connessa: "+state.model);break;
                        case Action::Disconnect: connectionWanted=false;reconnectPending=false;autofocus->cancel();camera->disconnect();state.properties.clear();state.liveView=false;state.frame={};break;
                        case Action::StartStarFocus:
                            if(!analyzer||!state.liveView)throw CameraError("Active Live View required for stellar autofocus");
                            refresh();focusProperties.clear();for(const auto& p:state.properties)focusProperties[p.id]=p.value;
                            focusZoom=state.frame.zoom;focusOptions=cmd.config.starFocus;autofocus->start(focusOptions);nextFocusCheck=clock.steadyNow();log("Info","Stellar autofocus started");break;
                        case Action::StopStarFocus:autofocus->cancel();break;
                        case Action::Refresh: refresh();break;
                        case Action::SetProperty:camera->set(cmd.property,cmd.value);refresh();break;
                        case Action::Start:
                        case Action::CaptureSingle:
                            validate(cmd.config);
                            if(state.liveView) {camera->setLiveView(false);state.liveView=false;}
                            if(cmd.action==Action::CaptureSingle)engine->startSingle(cmd.config);else engine->start(cmd.config);break;
                        case Action::Pause:engine->pause();break;
                        case Action::Resume:engine->resume();break;
                        case Action::Stop:autofocus->cancel();engine->stop();break;
                        case Action::LiveView:if(!cmd.value)autofocus->cancel();camera->setLiveView(cmd.value!=0);state.liveView=cmd.value!=0;break;
                        case Action::Focus:
                            if(cmd.value>6||cmd.value==3)throw std::invalid_argument("Passo focus non valido");
                            refreshLens();camera->focus(static_cast<int>(cmd.value)-3);refreshLens();break;
                        case Action::Zoom:camera->zoom(static_cast<int>(cmd.value));break;
                        case Action::SaveConfig:break;
                        }
                        mailbox.complete(cmd.id);
                    } catch(const CameraError& e) {mailbox.complete(cmd.id,e.what());if(e.disconnected&&cmd.action!=Action::Disconnect)lostConnection(e.what());else log("Error",e.what());}
                    catch(const std::exception& e) {mailbox.complete(cmd.id,e.what());log("Error",e.what());}
                }
                if(camera) {
                    if(reconnectPending&&!camera->connected()&&clock.steadyNow()>=reconnectAt){
                        ++reconnectCount;
                        try{
                            state.model=camera->connect();reconnectPending=false;
                            log("Info","Canon riconnessa; scatti, autofocus e Live View restano fermi");
                            refresh();
                        }catch(const std::exception& e){
                            log("Warning","Riconnessione "+std::to_string(reconnectCount)+"/"+std::to_string(config.reconnectAttempts)+": "+e.what());
                            if(camera->connected()) {try{camera->disconnect();}catch(const std::exception& cleanup){log("Warning",cleanup.what());}}
                            reconnectPending=reconnectCount<config.reconnectAttempts;
                            reconnectAt=clock.steadyNow()+std::chrono::milliseconds(static_cast<std::int64_t>(config.reconnectDelayMs)*reconnectCount);
                            if(!reconnectPending){connectionWanted=false;log("Warning","Riconnessione sospesa: verificare alimentazione/USB e premere Connetti");}
                        }
                    }
                    try {
                        camera->pump();engine->tick();
                        if(autofocus->progress().active()&&clock.steadyNow()>=nextFocusCheck){
                            nextFocusCheck=clock.steadyNow()+std::chrono::seconds(1);
                            std::map<Property,std::uint32_t> current;for(const auto& p:camera->properties())current[p.id]=p.value;
                            if(current!=focusProperties)autofocus->fail("Camera settings changed during autofocus");
                            else if(camera->lensFocusStatus().state!=LensFocusState::Ready)autofocus->fail("Lens is no longer AF-ready");
                        }
                        autofocus->tick();
                        if(camera->connected()&&!engine->active()&&clock.steadyNow()>=nextLens)refreshLens();
                        if(state.liveView && camera->connected() && clock.steadyNow()>=nextFrame) {
                            nextFrame=clock.steadyNow()+std::chrono::milliseconds(config.liveViewMs);
                            auto frame=camera->liveFrame();if(!frame.jpeg.empty()) {
                                if(autofocus->progress().active()&&frame.zoom!=focusZoom)autofocus->fail("Live View zoom changed during autofocus");
                                if(analyzer){try{const std::vector<StarSample> empty;state.stars=analyzer(frame,focusOptions.detectionSnr,autofocus->progress().active()?autofocus->trackingTargets():empty,focusOptions.matchRadius);autofocus->tick(&state.stars,frame.revision);}
                                    catch(const std::exception& e){state.stars={};if(autofocus->progress().active())autofocus->fail(e.what());warn(std::string("Analisi Live View: ")+e.what());}}
                                state.frame=std::move(frame);
                            }
                        }
                    } catch(const CameraError& e) {
                        if(e.disconnected)lostConnection(e.what());
                        else if(e.retryable){warn(e.what());nextFrame=clock.steadyNow()+std::chrono::milliseconds(config.retryDelayMs);}
                        else {
                            if(autofocus->progress().active())autofocus->fail(e.what());
                            if(engine->active())engine->reportError(e.what());else log("Error",e.what());
                            state.liveView=false;
                            if(camera->connected())try{camera->setLiveView(false);}catch(const std::exception& cleanup){log("Warning",cleanup.what());}
                        }
                    } catch(const std::exception& e) {
                        if(autofocus->progress().active())autofocus->fail(e.what());
                        engine->reportError(e.what());state.liveView=false;
                        try {camera->disconnect();} catch(const std::exception& cleanup){log("Error",cleanup.what());}
                    }
                    state.connected=camera->connected();state.progress=engine->progress();
                    for(std::size_t tier=0;tier<3;++tier)if(autofocus->progress().stageBest[tier]>0&&state.autofocus.stageBest[tier]==0)
                        log("Info","Star AF: verified Canon drive "+std::to_string(3-tier)+", HFD "+std::to_string(autofocus->progress().stageBest[tier])+" px");
                    if(autofocus->progress().pass!=state.autofocus.pass&&autofocus->progress().pass>0)
                        log("Info","Star AF: pass "+std::to_string(autofocus->progress().pass)+", Canon drive "+std::to_string(autofocus->progress().motorStep)+", spacing "+std::to_string(autofocus->progress().stride)+", search moves "+std::to_string(autofocus->progress().searchMoves));
                    if(autofocus->progress().curve.size()>state.autofocus.curve.size()){
                        const auto& point=autofocus->progress().curve.back();
                        log("Info","Star AF: step "+std::to_string(point.position)+", HFD "+std::to_string(point.hfd)+" px, scatter "+std::to_string(point.scatter));
                    }
                    if(state.autofocus.phase!=autofocus->progress().phase&&!autofocus->progress().active()&&autofocus->progress().phase!=StarFocusPhase::Idle)log(autofocus->progress().phase==StarFocusPhase::Failed?"Error":"Info",autofocus->progress().detail);
                    state.autofocus=autofocus->progress();
                    state.lastCapture=engine->lastCapture();
                    if(!state.connected) {state.liveView=false;state.properties.clear();state.frame={};state.lensFocus={};state.manualFocusExposure=false;}
                    if(!state.liveView)state.stars={};
                }
                state.reconnecting=reconnectPending;
                ++state.coreTick;mailbox.publish(state);if(preview)preview(state.lastCapture);
                std::unique_lock timerLock(timerMutex);timer.wait_for(timerLock,stop,std::chrono::milliseconds(config.tickMs),[]{return false;});
            }
            if(autofocus)autofocus->cancel();
            if(engine) engine->stop();
            if(camera) {try {camera->disconnect();} catch(const std::exception& e){log("Error",e.what());}}
        } catch(const std::exception& e) {state.backendAvailable=false;state.backendStatus=e.what();log("Error",e.what());}
        mailbox.publish(state);
    });
}
CameraWorker::~CameraWorker() {thread_.request_stop();if(thread_.joinable()) thread_.join();}
}
