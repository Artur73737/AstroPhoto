#include <astro/Infrastructure.hpp>
#include <astro/WorkerWait.hpp>
#include <astro/Localization.hpp>
#include <astro/ConfigManager.hpp>
#include <astro/StarFocusRun.hpp>
#include <astro/LiveStackRun.hpp>
#include <astro/PointFocusRun.hpp>
#include <astro/ConnectionPower.hpp>
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
CameraWorker::CameraWorker(Mailbox& mailbox,Config config,std::filesystem::path configPath,StarAnalyzer analyzer,CameraFactory factory,PreviewConsumer preview,std::shared_ptr<IStackProcessor> stacking) {
    mailbox.setLogLanguage(config.language);
    thread_=std::jthread([&mailbox,config=std::move(config),configPath=std::move(configPath),analyzer=std::move(analyzer),factory=std::move(factory),preview=std::move(preview),stacking=std::move(stacking)](std::stop_token stop) mutable {
        Snapshot state;state.backendAvailable=bool(factory)||canonBackendAvailable();
        state.coreThreadId=std::hash<std::thread::id>{}(std::this_thread::get_id());
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
            if(logfile) {const auto language=mailbox.logLanguage();logfile<<stamp.str()<<" ["<<localizeLogLevel(level,language)<<"] "<<localizeMessage(message,language)<<'\n';logfile.flush();}
        };
        try {
            WorkerWait timer;
            log("Info",state.backendStatus);
            if(!logfile) log("Warning","Impossibile aprire astro.log; log disponibile solo nella UI");
            SystemClock clock;DiskFileSystem files;
            ConnectionPower power(log);
            std::unique_ptr<ICamera> camera;std::unique_ptr<SequenceEngine> engine;
            if(state.backendAvailable) {
                try {camera=factory?factory(config,log):createCanonCamera(config,log);engine=std::make_unique<SequenceEngine>(*camera,files,clock,log);}
                catch(const std::exception& e) {camera.reset();state.backendAvailable=false;state.backendStatus=e.what();log("Error",e.what());}
            }
            std::unique_ptr<LiveStackRun> stack;if(camera&&stacking)stack=std::make_unique<LiveStackRun>(*camera,files,clock,*stacking,log);
            const auto recoveryPath=configPath.parent_path()/"stack-recovery.json";
            std::optional<Config> recovery;
            if(config.liveStack.autoRebuild&&std::filesystem::exists(recoveryPath))try{recovery=ConfigManager::load(recoveryPath);recovery->liveStack.fromSequence=false;recovery->liveStack.autoRebuild=false;}catch(const std::exception& e){log("Warning",e.what());}
            auto recoveryAt=clock.steadyNow()+std::chrono::seconds(2);bool recordedStack{};

            auto nextFrame=clock.steadyNow(),nextAnalysis=nextFrame;
            std::unique_ptr<StarFocusRun> autofocus;if(camera)autofocus=std::make_unique<StarFocusRun>(*camera,clock);
            std::unique_ptr<PointFocusRun> pointFocus;if(camera)pointFocus=std::make_unique<PointFocusRun>(*camera,clock);
            std::map<Property,std::uint32_t> focusProperties;auto nextFocusCheck=nextFrame;int focusZoom{};
            StarFocusOptions focusOptions=config.starFocus;
            bool reconnectPending{},connectionWanted{};int reconnectCount{};
            bool restoreLiveAfterCapture{};
            auto reconnectAt=nextFrame,warningAt=nextFrame;
            auto warn=[&](const std::string& message){if(clock.steadyNow()>=warningAt){log("Warning",message);warningAt=clock.steadyNow()+std::chrono::seconds(5);}};
            auto lostConnection=[&](const std::string& message){
                if(stack&&stack->status().active)stack->fail(message);
                if(pointFocus->status().active())pointFocus->fail(message);else pointFocus->reset();
                restoreLiveAfterCapture=false;
                if(autofocus->progress().active())autofocus->fail(message);
                if(engine->active())engine->reportError(message);else if(state.connected)log("Error",message);else warn(message);
                autofocus->invalidateLimits();state.liveView=false;state.frame={};state.stars={};
                try{camera->disconnect();}catch(const std::exception& cleanup){log("Warning",cleanup.what());}
                if(connectionWanted&&!reconnectPending){reconnectPending=config.reconnectAttempts>0;reconnectCount=0;reconnectAt=clock.steadyNow()+std::chrono::milliseconds(config.reconnectDelayMs);}
            };
            auto nextLens=nextFrame;
            auto refreshLens=[&] {
                try {state.lensFocus=camera->lensFocusStatus();state.manualFocusExposure=camera->manualExposureForFocus();}
                catch(const CameraError& e){state.manualFocusExposure=false;state.lensFocus={LensFocusState::Unknown,e.what()};if(e.disconnected)throw;warn(e.what());}
                catch(const std::exception& e){state.manualFocusExposure=false;state.lensFocus={LensFocusState::Unknown,e.what()};warn(e.what());}
                nextLens=clock.steadyNow()+std::chrono::milliseconds(config.lensPollMs);
            };
            auto refresh=[&] {
                state.cameraModule=camera->activeModule();
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
                        if(cmd.action==Action::StackStretch){if(!stacking)throw std::logic_error("Stacking unavailable");stacking->display({float(cmd.pointU),float(cmd.pointV),cmd.value/1000000.f,cmd.asinhStretch});mailbox.complete(cmd.id);continue;}
                        if(stack&&stack->status().active&&cmd.action!=Action::StopStack&&cmd.action!=Action::Stop&&cmd.action!=Action::PauseStack&&cmd.action!=Action::ResumeStack)throw std::logic_error("Stop live stacking and wait for final save before other camera actions");
                        if(pointFocus->status().active()&&cmd.action!=Action::StopPointFocus&&cmd.action!=Action::Stop&&cmd.action!=Action::Disconnect&&!(cmd.action==Action::LiveView&&cmd.value==0))throw std::logic_error("Stop point autofocus before changing camera settings");
                        if(autofocus->progress().active()&&cmd.action!=Action::StopStarFocus&&cmd.action!=Action::Stop&&cmd.action!=Action::Disconnect&&!(cmd.action==Action::LiveView&&cmd.value==0))throw std::logic_error("Stop stellar autofocus before changing camera settings");
                        if(engine->active() && cmd.action!=Action::Pause && cmd.action!=Action::Resume && cmd.action!=Action::Stop)
                            throw std::logic_error("Arrestare la sequenza prima di modificare la fotocamera");
                        if(cmd.action==Action::Focus||cmd.action==Action::Zoom||cmd.action==Action::SetProperty||cmd.action==Action::StartStarFocus||cmd.action==Action::Connect||cmd.action==Action::Disconnect||cmd.action==Action::Start||cmd.action==Action::CaptureSingle)pointFocus->reset();
                        switch(cmd.action) {
                        case Action::StartStack:
                            if(!stack)throw std::logic_error("RAW stacking backend unavailable");
                            validate(cmd.config);restoreLiveAfterCapture=state.liveView;
                            if(state.liveView){camera->setLiveView(false);state.liveView=false;}
                            try{auto requested=stackPlan(cmd.config);if(cmd.config.liveStack.fromSequence){for(auto [property,value]:requested.singleCapture.settings)camera->set(property,value);refresh();}auto metadata=requested;metadata.singleCapture.settings.clear();if(cmd.config.liveStack.sourceDirectory.empty())for(const auto& p:camera->properties()){metadata.singleCapture.settings[p.id]=p.value;metadata.labels[p.id][p.value]=p.currentLabel;}recovery.reset();stack->start(cmd.config,ConfigManager::toJson(metadata));recordedStack=true;if(requested.liveStack.sourceDirectory.empty())try{auto saved=requested;saved.liveStack.sourceDirectory=stack->status().directory+"/frames";saved.liveStack.fromSequence=false;ConfigManager::save(recoveryPath,saved);}catch(const std::exception& e){log("Warning",e.what());}}catch(...){if(restoreLiveAfterCapture&&camera->connected()){camera->setLiveView(true);state.liveView=true;}restoreLiveAfterCapture=false;throw;}
                            break;
                        case Action::StopStack:if(stack)stack->stop();break;
                        case Action::PauseStack:if(stack)stack->pause();break;
                        case Action::ResumeStack:if(stack)stack->resume();break;
                        case Action::StackStretch:break;
                        case Action::SetTurboLive:config.turboLiveStream=cmd.value!=0;nextFrame=clock.steadyNow();break;
                        case Action::StartPointFocus:
                            if(!state.liveView||state.frame.jpeg.empty()||state.frame.zoom!=1)throw CameraError("Point autofocus requires a fresh 1x Live View frame");
                            autofocus->invalidateLimits();pointFocus->start(cmd.pointU,cmd.pointV,cmd.pointTimeoutSeconds);log("Info","Point autofocus: focusing");break;
                        case Action::StopPointFocus:pointFocus->cancel();break;
                        case Action::Connect: camera->selectModule(cmd.config.cameraModule);connectionWanted=true;reconnectPending=false;state.model=camera->connect();refresh();log("Info","Connessa: "+state.model);break;
                        case Action::Disconnect: pointFocus->cancel();restoreLiveAfterCapture=false;connectionWanted=false;reconnectPending=false;autofocus->cancel();autofocus->invalidateLimits();camera->disconnect();state.properties.clear();state.liveView=false;state.frame={};break;
                        case Action::StartStarFocus:
                            if(!analyzer||!state.liveView)throw CameraError("Active Live View required for stellar autofocus");
                            refresh();focusProperties.clear();for(const auto& p:state.properties)focusProperties[p.id]=p.value;
                            focusZoom=state.frame.zoom;focusOptions=cmd.config.starFocus;autofocus->start(focusOptions);nextFocusCheck=clock.steadyNow();log("Info","Stellar autofocus started");break;
                        case Action::StopStarFocus:autofocus->cancel();break;
                        case Action::Refresh: refresh();break;
                        case Action::SetProperty:autofocus->invalidateLimits();camera->set(cmd.property,cmd.value);refresh();nextAnalysis=clock.steadyNow();break;
                        case Action::Start:
                        case Action::CaptureSingle:
                            validate(cmd.config);autofocus->invalidateLimits();
                            restoreLiveAfterCapture=state.liveView;
                            if(state.liveView) {camera->setLiveView(false);state.liveView=false;}
                            if(cmd.action==Action::CaptureSingle)engine->startSingle(cmd.config);else engine->start(cmd.config);break;
                        case Action::Pause:engine->pause();break;
                        case Action::Resume:engine->resume();break;
                        case Action::Stop:if(stack&&stack->status().active)stack->stop();else {pointFocus->cancel();autofocus->cancel();engine->stop();}break;
                        case Action::LiveView:if(!cmd.value){if(pointFocus->status().active())pointFocus->cancel();else pointFocus->reset();autofocus->cancel();autofocus->invalidateLimits();}camera->setLiveView(cmd.value!=0);state.liveView=cmd.value!=0;state.stars={};nextFrame=nextAnalysis=clock.steadyNow();break;
                        case Action::Focus:
                            if(cmd.value>6||cmd.value==3)throw std::invalid_argument("Passo focus non valido");
                            autofocus->invalidateLimits();refreshLens();camera->focus(static_cast<int>(cmd.value)-3);refreshLens();break;
                        case Action::Zoom:autofocus->invalidateLimits();camera->zoom(static_cast<int>(cmd.value));nextAnalysis=clock.steadyNow();break;
                        case Action::SaveConfig:break;
                        }
                        mailbox.complete(cmd.id);
                    } catch(const CameraError& e) {
                        if(stack&&stack->status().active&&!e.retryable)stack->fail(e.what());mailbox.complete(cmd.id,e.what());if(e.disconnected&&cmd.action!=Action::Disconnect)lostConnection(e.what());else log("Error",e.what());}
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
                        const auto pumpStart=clock.steadyNow();camera->pump();
                        state.livePerformance.pumpMs+=std::chrono::duration<double,std::milli>(clock.steadyNow()-pumpStart).count();
                        engine->tick();
                        // Restore only after the engine has finished exposure/download/stop.
                        // Never enable EVF during an active Bulb exposure or after USB loss.
                        if(restoreLiveAfterCapture&&!engine->active()&&(!stack||!stack->status().active)) {
                            if(camera->connected()&&engine->progress().state==SequenceState::Idle&&(!stack||stack->status().phase!="error")) {
                                camera->setLiveView(true);state.liveView=true;
                                nextFrame=nextAnalysis=clock.steadyNow();
                            }
                            restoreLiveAfterCapture=false;
                        }
                        if(autofocus->progress().active()&&clock.steadyNow()>=nextFocusCheck){
                            nextFocusCheck=clock.steadyNow()+std::chrono::seconds(1);
                            std::map<Property,std::uint32_t> current;for(const auto& p:camera->properties())current[p.id]=p.value;
                            if(current!=focusProperties)autofocus->fail("Camera settings changed during autofocus");
                            else if(camera->lensFocusStatus().state!=LensFocusState::Ready)autofocus->fail("Lens is no longer AF-ready");
                        }
                        autofocus->tick();
                        pointFocus->tick();
                        if(camera->connected()&&!engine->active()&&(!stack||!stack->status().active)&&clock.steadyNow()>=nextLens)refreshLens();
                        if(state.liveView && camera->connected() && clock.steadyNow()>=nextFrame) {
                            nextFrame=clock.steadyNow()+std::chrono::milliseconds(config.turboLiveStream?1:config.liveViewMs);
                            const auto downloadStart=clock.steadyNow();++state.livePerformance.polls;
                            auto frame=camera->liveFrame();
                            state.livePerformance.downloadMs+=std::chrono::duration<double,std::milli>(clock.steadyNow()-downloadStart).count();
                            if(frame.jpeg.empty())++state.livePerformance.emptyPolls;
                            if(!frame.jpeg.empty()) {
                                ++state.livePerformance.frames;
                                if(autofocus->progress().active()&&frame.zoom!=focusZoom)autofocus->fail("Live View zoom changed during autofocus");
                                // Keep full per-frame measurements during autofocus. Idle
                                // field statistics need not delay every displayed frame.
                                if(analyzer&&(autofocus->progress().active()||clock.steadyNow()>=nextAnalysis)){
                                    nextAnalysis=clock.steadyNow()+std::chrono::milliseconds(config.liveAnalysisMs);
                                    try{const auto analysisStart=clock.steadyNow();const std::vector<StarSample> empty;state.stars=analyzer(frame,focusOptions.detectionSnr,autofocus->progress().active()?autofocus->trackingTargets():empty,focusOptions.matchRadius);
                                        ++state.livePerformance.analyses;state.livePerformance.analysisMs+=std::chrono::duration<double,std::milli>(clock.steadyNow()-analysisStart).count();
                                        autofocus->tick(&state.stars,frame.revision);}
                                    catch(const std::exception& e){state.stars={};if(autofocus->progress().active())autofocus->fail(e.what());warn(std::string("Analisi Live View: ")+e.what());}}
                                state.frame=std::move(frame);
                            }
                        }
                    } catch(const CameraError& e) {
                        if(stack&&stack->status().active&&!e.retryable)stack->fail(e.what());
                        if(!e.retryable)pointFocus->fail(e.what());
                        if(e.disconnected)lostConnection(e.what());
                        else if(e.retryable){warn(e.what());nextFrame=clock.steadyNow()+std::chrono::milliseconds(config.retryDelayMs);}
                        else {
                            restoreLiveAfterCapture=false;
                            if(autofocus->progress().active())autofocus->fail(e.what());
                            if(engine->active())engine->reportError(e.what());else log("Error",e.what());
                            state.liveView=false;
                            if(camera->connected())try{camera->setLiveView(false);}catch(const std::exception& cleanup){log("Warning",cleanup.what());}
                        }
                    } catch(const std::exception& e) {
                        if(stack&&stack->status().active)stack->fail(e.what());
                        pointFocus->fail(e.what());
                        restoreLiveAfterCapture=false;
                        if(autofocus->progress().active())autofocus->fail(e.what());
                        engine->reportError(e.what());state.liveView=false;
                        try {camera->disconnect();} catch(const std::exception& cleanup){log("Error",cleanup.what());}
                    }
                    if(recovery&&stack&&!stack->status().active&&!engine->active()&&!autofocus->progress().active()&&!pointFocus->status().active()&&clock.steadyNow()>=recoveryAt){
                        auto requested=std::move(*recovery);recovery.reset();try{restoreLiveAfterCapture=state.liveView;if(state.liveView){camera->setLiveView(false);state.liveView=false;}stack->start(requested,ConfigManager::toJson(requested));recordedStack=true;log("Info","Rebuilding interrupted stack; no new exposures");}catch(const std::exception& e){log("Warning",e.what());}
                    }
                    if(stack)stack->tick();
                    if(recordedStack&&stack&&!stack->status().active){recordedStack=false;if(stack->status().phase=="complete"){std::error_code error;std::filesystem::remove(recoveryPath,error);if(error)log("Warning",error.message());}}

                    state.connected=camera->connected();state.progress=engine->progress();
                    const auto& diagnostic=autofocus->progress();
                    if(diagnostic.smallDiagnostic!=state.autofocus.smallDiagnostic&&!diagnostic.smallDiagnostic.empty())log("Info",diagnostic.smallDiagnostic);
                    if(diagnostic.surveyUsed&&(!state.autofocus.surveyUsed||diagnostic.surveyBestPosition!=state.autofocus.surveyBestPosition||diagnostic.surveyBestStars!=state.autofocus.surveyBestStars))
                        log("Info","Coarse survey: moves "+std::to_string(diagnostic.surveyMoves)+", stars "+std::to_string(diagnostic.surveyBestStars)+", best HFD "+std::to_string(diagnostic.surveyBestHfd));
                    if(diagnostic.observationRevision!=state.autofocus.observationRevision&&!diagnostic.observations.empty()){
                        const auto& o=diagnostic.observations.back();
                        log("Info","Focus measure: drive "+std::to_string(o.motorStep)+", commands "+std::to_string(o.commands)+", direction "+std::to_string(o.direction)+", HFD "+std::to_string(o.hfd)+", noise "+std::to_string(o.noise)+", delta "+std::to_string(o.delta)+", threshold "+std::to_string(o.threshold)+", frames "+std::to_string(o.frames)+", elapsed ms "+std::to_string(o.elapsedMs));
                    }
                    if(autofocus->progress().homed&&!state.autofocus.homed)
                        log("Info","Star AF: optical limits verified, reset near; estimated range "+std::to_string(autofocus->progress().coarseRange)+" large commands, calibration moves "+std::to_string(autofocus->progress().homingMoves));
                    for(std::size_t tier=0;tier<3;++tier)if(autofocus->progress().stageBest[tier]>0&&state.autofocus.stageBest[tier]==0)
                        log("Info","Star AF: verified Canon drive "+std::to_string(3-tier)+", HFD "+std::to_string(autofocus->progress().stageBest[tier])+" px");
                    if(autofocus->progress().pass!=state.autofocus.pass&&autofocus->progress().pass>0)
                        log("Info","Star AF: pass "+std::to_string(autofocus->progress().pass)+", Canon drive "+std::to_string(autofocus->progress().motorStep)+", spacing "+std::to_string(autofocus->progress().stride)+", search moves "+std::to_string(autofocus->progress().searchMoves));
                    if(autofocus->progress().curve.size()>state.autofocus.curve.size()){
                        const auto& point=autofocus->progress().curve.back();
                        log("Info","Star AF: step "+std::to_string(point.position)+", HFD "+std::to_string(point.hfd)+" px, scatter "+std::to_string(point.scatter));
                    }
                    if(state.autofocus.phase!=autofocus->progress().phase&&!autofocus->progress().active()&&autofocus->progress().phase!=StarFocusPhase::Idle)log(autofocus->progress().phase==StarFocusPhase::Failed?"Error":autofocus->progress().phase==StarFocusPhase::Warning?"Warning":"Info",autofocus->progress().detail);
                    state.autofocus=autofocus->progress();
                    if(state.pointAutofocus.phase!=pointFocus->status().phase&&!pointFocus->status().active()&&pointFocus->status().phase!=PointAfPhase::Idle)
                        log(pointFocus->status().phase==PointAfPhase::Failed?"Warning":"Info",pointFocus->status().detail);
                    state.pointAutofocus=pointFocus->status();
                    state.lastCapture=engine->lastCapture();if(stack)state.stack=stack->status();if(stacking)state.stack.preview=stacking->currentPreview();
                    if(!state.connected) {state.cameraModule={};state.liveView=false;state.properties.clear();state.frame={};state.lensFocus={};state.manualFocusExposure=false;}
                    if(!state.liveView)state.stars={};
                }
                state.reconnecting=reconnectPending;
                try{power.setConnected(state.connected);}catch(const std::exception& e){warn(e.what());}
                state.sleepInhibited=power.active();
                ++state.coreTick;mailbox.publish(state);if(preview)preview(state.lastCapture);
                auto wait=std::chrono::milliseconds(config.tickMs);
                if(state.connected&&state.liveView){
                    const auto remaining=std::chrono::ceil<std::chrono::milliseconds>(nextFrame-clock.steadyNow());
                    wait=std::clamp(remaining,std::chrono::milliseconds(1),wait);
                }
                timer.wait(stop,wait);
            }
            if(stack&&stack->status().active){stack->stop();while(stack->status().active){try{camera->pump();}catch(const std::exception& e){stack->fail(e.what());}stack->tick();std::this_thread::sleep_for(std::chrono::milliseconds(20));}}
            if(autofocus)autofocus->cancel();
            if(pointFocus)pointFocus->cancel();
            if(engine) engine->stop();
            if(camera) {try {camera->disconnect();} catch(const std::exception& e){log("Error",e.what());}}
            power.setConnected(false);state.sleepInhibited=false;state.connected=false;state.liveView=false;
        } catch(const std::exception& e) {state.backendAvailable=false;state.backendStatus=e.what();state.sleepInhibited=false;state.connected=false;state.liveView=false;log("Error",e.what());}
        mailbox.publish(state);
    });
}
CameraWorker::~CameraWorker() {thread_.request_stop();if(thread_.joinable()) thread_.join();}
}
