#include <astro/Infrastructure.hpp>
#include <astro/ConfigManager.hpp>
#ifdef ASTRO_WITH_EDSDK
#include <astro/EdsCamera.hpp>
#endif
#include <fstream>
#include <iomanip>
#include <sstream>
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
CameraWorker::CameraWorker(Mailbox& mailbox,Config config,std::filesystem::path configPath) {
    thread_=std::jthread([&mailbox,config=std::move(config),configPath=std::move(configPath)](std::stop_token stop) mutable {
        Snapshot state;state.backendAvailable=canonBackendAvailable();
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
                try {camera=createCanonCamera(config,log);engine=std::make_unique<SequenceEngine>(*camera,files,clock,log);}
                catch(const std::exception& e) {camera.reset();state.backendAvailable=false;state.backendStatus=e.what();log("Error",e.what());}
            }
            auto nextFrame=clock.steadyNow();
            auto nextLens=nextFrame;
            auto refreshLens=[&] {
                try {state.lensFocus=camera->lensFocusStatus();}
                catch(const std::exception& e){if(state.lensFocus.detail!=e.what())log("Warning",e.what());state.lensFocus={LensFocusState::Unknown,e.what()};}
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
                        if(cmd.action==Action::SaveConfig) {ConfigManager::save(configPath,cmd.config);log("Info","Configurazione salvata: "+configPath.string());continue;}
                        if(!camera) throw std::runtime_error(state.backendStatus);
                        if(engine->active() && cmd.action!=Action::Pause && cmd.action!=Action::Resume && cmd.action!=Action::Stop)
                            throw std::logic_error("Arrestare la sequenza prima di modificare la fotocamera");
                        switch(cmd.action) {
                        case Action::Connect: state.model=camera->connect();refresh();log("Info","Connessa: "+state.model);break;
                        case Action::Disconnect: camera->disconnect();state.properties.clear();state.liveView=false;state.frame={};break;
                        case Action::Refresh: refresh();break;
                        case Action::SetProperty:camera->set(cmd.property,cmd.value);refresh();break;
                        case Action::Start:
                        case Action::CaptureSingle:
                            validate(cmd.config);
                            if(state.liveView) {camera->setLiveView(false);state.liveView=false;}
                            if(cmd.action==Action::CaptureSingle)engine->startSingle(cmd.config);else engine->start(cmd.config);break;
                        case Action::Pause:engine->pause();break;
                        case Action::Resume:engine->resume();break;
                        case Action::Stop:engine->stop();break;
                        case Action::LiveView:camera->setLiveView(cmd.value!=0);state.liveView=cmd.value!=0;break;
                        case Action::Focus:
                            if(cmd.value>6||cmd.value==3)throw std::invalid_argument("Passo focus non valido");
                            refreshLens();camera->focus(static_cast<int>(cmd.value)-3);refreshLens();break;
                        case Action::Zoom:camera->zoom(static_cast<int>(cmd.value));break;
                        case Action::SaveConfig:break;
                        }
                    } catch(const std::exception& e) {log("Error",e.what());}
                }
                if(camera) {
                    try {
                        camera->pump();engine->tick();
                        if(camera->connected()&&!engine->active()&&clock.steadyNow()>=nextLens)refreshLens();
                        if(state.liveView && clock.steadyNow()>=nextFrame) {
                            nextFrame=clock.steadyNow()+std::chrono::milliseconds(config.liveViewMs);
                            auto frame=camera->liveFrame();if(!frame.jpeg.empty()) state.frame=std::move(frame);
                        }
                    } catch(const std::exception& e) {
                        engine->reportError(e.what());state.liveView=false;
                        try {camera->disconnect();} catch(const std::exception& cleanup){log("Error",cleanup.what());}
                    }
                    state.connected=camera->connected();state.progress=engine->progress();
                    state.lastCapture=engine->lastCapture();
                    if(!state.connected) {state.liveView=false;state.properties.clear();state.frame={};state.lensFocus={};}
                }
                mailbox.publish(state);std::this_thread::sleep_for(std::chrono::milliseconds(config.tickMs));
            }
            if(engine) engine->stop();
            if(camera) {try {camera->disconnect();} catch(const std::exception& e){log("Error",e.what());}}
        } catch(const std::exception& e) {state.backendAvailable=false;state.backendStatus=e.what();log("Error",e.what());}
        mailbox.publish(state);
    });
}
CameraWorker::~CameraWorker() {thread_.request_stop();if(thread_.joinable()) thread_.join();}
}
