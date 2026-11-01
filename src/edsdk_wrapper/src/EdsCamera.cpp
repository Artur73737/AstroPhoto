#define NOMINMAX
#include <windows.h>
#include <EDSDK.h>
#include <EDSDKErrors.h>
#include <astro/EdsCamera.hpp>
#include <astro/PropertyLabels.hpp>
#include <astro/CameraModule.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>

namespace astro::edsdk {
namespace {
std::string hex(EdsUInt32 n) {std::ostringstream s;s<<"0x"<<std::uppercase<<std::hex<<n;return s.str();}
std::string errorText(EdsError e) {
    switch(e) {
    case EDS_ERR_OK:return "OK";
    case EDS_ERR_DEVICE_BUSY:return "Fotocamera occupata";
    case EDS_ERR_NOT_SUPPORTED:return "Operazione non supportata dalla fotocamera/SDK";
    case EDS_ERR_OBJECT_NOTREADY:return "Oggetto non ancora pronto";
    case EDS_ERR_COMM_DISCONNECTED:return "Connessione USB interrotta";
    case EDS_ERR_DEVICE_NOT_FOUND:return "Fotocamera USB non trovata";
    case EDS_ERR_INVALID_HANDLE:return "Riferimento alla fotocamera non piu valido";
    case EDS_ERR_COMM_USB_BUS_ERR:return "Errore del bus USB";
    case EDS_ERR_SESSION_ALREADY_OPEN:return "Sessione gia aperta; chiudere EOS Utility e riconnettere";
    case EDS_ERR_COMM_PORT_IS_IN_USE:return "USB in uso da un altro programma (es. EOS Utility)";
    case EDS_ERR_TAKE_PICTURE_CARD_NG:return "Errore della scheda; verificare presenza, spazio e stato";
    default:return "Errore Canon EDSDK";
    }
}
void check(EdsError e,const std::string& context) {if(e!=EDS_ERR_OK) throw CameraError(context+": "+errorText(e)+" ("+hex(e)+")",e==EDS_ERR_DEVICE_BUSY||e==EDS_ERR_OBJECT_NOTREADY,
    e==EDS_ERR_COMM_DISCONNECTED||e==EDS_ERR_DEVICE_NOT_FOUND||e==EDS_ERR_INVALID_HANDLE||e==EDS_ERR_COMM_USB_BUS_ERR);}
template<class T> struct Ref {
    T value{};
    Ref()=default; explicit Ref(T v):value(v){}
    Ref(const Ref&)=delete;Ref& operator=(const Ref&)=delete;
    ~Ref(){reset();}
    void reset(){if(value){EdsRelease(value);value=nullptr;}} // EdsRelease returns a reference count, not EdsError.
};
EdsPropertyID sdkProperty(Property p) {
    switch(p) {
    case Property::Iso:return kEdsPropID_ISOSpeed;case Property::Aperture:return kEdsPropID_Av;
    case Property::Shutter:return kEdsPropID_Tv;case Property::ExposureMode:return kEdsPropID_AEMode;
    case Property::WhiteBalance:return kEdsPropID_WhiteBalance;case Property::ColorTemperature:return kEdsPropID_ColorTemperature;
    case Property::PictureStyle:return kEdsPropID_PictureStyle;case Property::ImageQuality:return kEdsPropID_ImageQuality;
    } throw std::invalid_argument("Proprieta non supportata");
}
}
struct EdsCamera::Impl {
    Config config;Log log;Ref<EdsCameraRef> camera;
    const camera_models::CameraModule* module{};
    camera_models::CapturePolicy capturePolicy{};
    std::thread::id owner=std::this_thread::get_id();
    bool com{},sdk{},session{},shutdown{},live{},capturing{},bulb{};
    bool focusUnsupported{};
    bool pointAfRunning{},pointAfPendingSeen{};
    std::optional<EdsUInt32> previousAfMode;
    PointAfStatus pointAf;
    int frameZoom{},coordinateWidth{},coordinateHeight{};
    std::uint64_t pointAfRevision{};
    bool extendPending{},keepAliveUnsupported{};
    EdsUInt32 shutdownSeconds{};
    int keepAliveRetries{};
    SteadyTime nextKeepAlive{};
    EdsUInt32 previousEvf{},previousMode{};
    std::optional<EdsUInt32> previousSaveTo;
    std::uint64_t revision{};
    int expectedFiles{1};
    SteadyTime bulbEnd{};
    std::filesystem::path stem;
    CaptureResult capture;
    std::exception_ptr callbackError;
    Impl(const Config& c,Log l):config(c),log(std::move(l)) {
        auto hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        if(FAILED(hr)) throw std::runtime_error("CoInitializeEx fallita: "+hex(static_cast<EdsUInt32>(hr)));
        com=true;
        try {check(EdsInitializeSDK(),"Inizializzazione SDK");sdk=true;} catch(...) {CoUninitialize();com=false;throw;}
    }
    ~Impl() {
        try {close();} catch(const std::exception& e){safeLog(e.what());}
        if(sdk) {auto e=EdsTerminateSDK();if(e!=EDS_ERR_OK) safeLog("TerminateSDK: "+errorText(e));}
        if(com) CoUninitialize();
    }
    void safeLog(const std::string& s) noexcept {try {log("Error",s);}catch(...) {OutputDebugStringA(s.c_str());}}
    void assertThread() const {if(std::this_thread::get_id()!=owner) throw std::logic_error("EDSDK deve essere chiamato dal suo worker STA");}
    void require() const {assertThread();if(!session||shutdown) throw CameraError("Fotocamera non connessa",false,true);}
    EdsUInt32 get(EdsPropertyID p) {require();EdsUInt32 v{};check(EdsGetPropertyData(camera.value,p,0,sizeof(v),&v),"Lettura "+hex(p));return v;}
    void put(EdsPropertyID p,EdsUInt32 v) {require();check(EdsSetPropertyData(camera.value,p,0,sizeof(v),&v),"Impostazione "+hex(p));}
    void endBulb() {if(bulb){check(EdsSendCommand(camera.value,capturePolicy.bulbEndCommand,0),"Fine Bulb");bulb=false;}}
    void stopPointAf(){
        if(pointAfRunning){require();check(EdsSendCommand(camera.value,kEdsCameraCommand_DoEvfAf,kEdsCameraCommand_EvfAf_OFF),"Point autofocus stop");pointAfRunning=false;}
        if(previousAfMode){require();put(kEdsPropID_Evf_AFMode,*previousAfMode);previousAfMode.reset();}
    }
    EdsFocusInfo focusInfo(){
        EdsFocusInfo info{};check(EdsGetPropertyData(camera.value,kEdsPropID_FocusInfo,0,sizeof(info),&info),"Point autofocus result");
        if(info.pointNumber>std::size(info.focusPoint))throw CameraError("Invalid camera AF point count");return info;
    }
    void close() {
        assertThread();std::exception_ptr first;
        auto cleanup=[&](auto fn){try{fn();}catch(...){if(!first)first=std::current_exception();}};
        if(session) {
            if(!shutdown) cleanup([&]{stopPointAf();});
            if(!shutdown) cleanup([&]{endBulb();});
            if(live&&!shutdown) cleanup([&]{put(kEdsPropID_Evf_OutputDevice,previousEvf);put(kEdsPropID_Evf_Mode,previousMode);});
            if(previousSaveTo&&!shutdown)cleanup([&]{put(kEdsPropID_SaveTo,*previousSaveTo);});
            cleanup([&]{check(EdsSetObjectEventHandler(camera.value,kEdsObjectEvent_All,nullptr,nullptr),"Rimozione handler oggetti");});
            cleanup([&]{check(EdsSetCameraStateEventHandler(camera.value,kEdsStateEvent_All,nullptr,nullptr),"Rimozione handler stato");});
            cleanup([&]{check(EdsCloseSession(camera.value),"Chiusura sessione");});
        }
        session=false;live=false;capturing=false;bulb=false;extendPending=false;previousSaveTo.reset();camera.reset();
        pointAfRunning=false;previousAfMode.reset();frameZoom=coordinateWidth=coordinateHeight=0;
        capture={};stem.clear();callbackError=nullptr;module=nullptr;
        if(first) std::rethrow_exception(first);
    }
    void releaseShutdownSession() {
        // State callbacks only mark shutdown. Release SDK references outside the callback,
        // even if the powered-off device rejects handler removal or CloseSession.
        if(shutdown&&session) {try {close();}catch(const std::exception& e){safeLog(e.what());}}
    }
    void renewShutdownTimer() {
        if(!session||shutdown||keepAliveUnsupported)return;
        const auto now=std::chrono::steady_clock::now();
        if(extendPending){log("Info","Canon: spegnimento automatico tra "+std::to_string(shutdownSeconds)+" s; rinnovo richiesto");extendPending=false;nextKeepAlive=now;}
        if(now<nextKeepAlive)return;
        const auto err=EdsSendCommand(camera.value,kEdsCameraCommand_ExtendShutDownTimer,0);
        if(err==EDS_ERR_DEVICE_BUSY||err==EDS_ERR_OBJECT_NOTREADY){
            if(keepAliveRetries<config.busyRetries){++keepAliveRetries;nextKeepAlive=now+std::chrono::milliseconds(static_cast<std::int64_t>(config.retryDelayMs)*keepAliveRetries);return;}
            log("Warning","Canon: rinnovo timer temporaneamente occupato; nuovo tentativo al prossimo intervallo");
        } else if(err==EDS_ERR_NOT_SUPPORTED){keepAliveUnsupported=true;log("Warning","Canon: rinnovo timer non supportato; disabilitare Auto power off sulla fotocamera");}
        else check(err,"Rinnovo timer spegnimento Canon");
        keepAliveRetries=0;nextKeepAlive=now+std::chrono::seconds(config.keepAliveSeconds);
    }
    void restartSdk() {
        // No camera/list/stream reference may outlive TerminateSDK.
        assertThread();if(session||camera.value)throw std::logic_error("SDK reset requires a closed session");
        if(sdk){check(EdsTerminateSDK(),"Riavvio SDK: terminazione");sdk=false;}
        check(EdsInitializeSDK(),"Riavvio SDK: inizializzazione");sdk=true;
        log("Info","Canon: SDK reinizializzato per aggiornare il rilevamento USB");
    }
    static EdsError EDSCALLBACK onState(EdsStateEvent event,EdsUInt32 parameter,EdsVoid* context) noexcept {
        auto& self=*static_cast<Impl*>(context);
        if(event==kEdsStateEvent_Shutdown) self.shutdown=true;
        if(event==kEdsStateEvent_WillSoonShutDown){self.extendPending=true;self.shutdownSeconds=parameter;}
        return EDS_ERR_OK;
    }
    static EdsError EDSCALLBACK onObject(EdsObjectEvent event,EdsBaseRef object,EdsVoid* context) noexcept {
        auto& self=*static_cast<Impl*>(context);Ref<EdsBaseRef> ref(object);
        if(event!=kEdsObjectEvent_DirItemRequestTransfer || !object) return EDS_ERR_OK;
        bool finalized=false;
        try {
            if(!self.capturing) {check(EdsDownloadCancel(object),"Annullamento trasferimento fuori sequenza");finalized=true;return EDS_ERR_OK;}
            EdsDirectoryItemInfo info{};check(EdsGetDirectoryItemInfo(object,&info),"Informazioni file");
            auto extension=std::filesystem::path(info.szFileName).extension().wstring();
            if(extension.empty() || extension.find_first_not_of(L".abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789")!=std::wstring::npos) throw std::runtime_error("Estensione file camera non valida");
            Ref<EdsStreamRef> stream;check(EdsCreateMemoryStream(0,&stream.value),"Stream download");
            check(EdsDownload(object,info.size,stream.value),"Download");
            EdsVoid* data{};EdsUInt64 size{};
            check(EdsGetPointer(stream.value,&data),"Puntatore download");check(EdsGetLength(stream.value,&size),"Dimensione download");
            if(size!=info.size||size==0||size>std::numeric_limits<DWORD>::max()) throw std::runtime_error("Dimensione download non valida");
            std::filesystem::path path;HANDLE file=INVALID_HANDLE_VALUE;
            // CREATE_NEW prevents overwriting existing data, including concurrent writers.
            for(unsigned suffix=0;file==INVALID_HANDLE_VALUE;++suffix) {
                path=self.stem; if(suffix) path+=L"_"+std::to_wstring(suffix);path+=extension;
                file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(file==INVALID_HANDLE_VALUE && GetLastError()!=ERROR_FILE_EXISTS) throw std::runtime_error("Creazione file fallita: "+std::to_string(GetLastError()));
                if(suffix==UINT_MAX) throw std::runtime_error("Nomi file esauriti");
            }
            DWORD written{};const bool ok=WriteFile(file,data,static_cast<DWORD>(size),&written,nullptr)!=0 && written==size;
            const bool flushed=FlushFileBuffers(file)!=0; const bool closed=CloseHandle(file)!=0;
            if(!ok||!flushed||!closed) {auto partial=path;partial+=L".partial";std::error_code ec;std::filesystem::rename(path,partial,ec);throw std::runtime_error("Scrittura download incompleta: "+path.string());}
            check(EdsDownloadComplete(object),"Conferma download");finalized=true;
            self.capture.files.push_back(path);
            self.capture.complete=self.capture.files.size()>=static_cast<std::size_t>(self.expectedFiles);
        } catch(...) {
            self.callbackError=std::current_exception();
            if(!finalized) {const auto e=EdsDownloadCancel(object);if(e!=EDS_ERR_OK) self.safeLog("DownloadCancel: "+errorText(e));}
        }
        return EDS_ERR_OK;
    }
};
EdsCamera::EdsCamera(const Config& c,Log l):impl_(std::make_unique<Impl>(c,std::move(l))){}
EdsCamera::~EdsCamera()=default;
void EdsCamera::selectModule(const std::string& selection){
    auto& s=*impl_;s.assertThread();s.releaseShutdownSession();if(s.session)throw CameraError("Disconnect before selecting a camera module");
    camera_models::validateSelection(selection);s.config.cameraModule=selection;
}
CameraModuleInfo EdsCamera::activeModule() const {impl_->assertThread();return impl_->module?impl_->module->info():CameraModuleInfo{};}
std::string EdsCamera::connect() {
    auto& s=*impl_;s.assertThread();s.releaseShutdownSession();
    if(s.session) throw CameraError("Sessione gia aperta");s.shutdown=false;s.callbackError=nullptr;s.focusUnsupported=false;
    if(!s.sdk)s.restartSdk();
    for(int attempt=0;attempt<2;++attempt){
        bool staleSession{};
        try {
            Ref<EdsCameraListRef> list;EdsUInt32 count{};
            check(EdsGetCameraList(&list.value),"Elenco fotocamere");check(EdsGetChildCount(list.value,&count),"Numero fotocamere");
            if(count!=1)throw CameraError(count==0?"Nessuna fotocamera Canon rilevata":"Collegare una sola fotocamera Canon",false,count==0);
            check(EdsGetChildAtIndex(list.value,0,&s.camera.value),"Selezione fotocamera");
            const auto opened=EdsOpenSession(s.camera.value);staleSession=opened==EDS_ERR_SESSION_ALREADY_OPEN;
            check(opened,"Apertura sessione");s.session=true;
            std::array<char,EDS_MAX_NAME> name{};check(EdsGetPropertyData(s.camera.value,kEdsPropID_ProductName,0,static_cast<EdsUInt32>(name.size()),name.data()),"Nome fotocamera");
            name.back()='\0';s.module=&camera_models::resolve(s.config.cameraModule,name.data());
            s.log("Info","Camera module: "+s.module->info().id+" ("+s.config.cameraModule+")");
            check(EdsSetObjectEventHandler(s.camera.value,kEdsObjectEvent_All,Impl::onObject,&s),"Registrazione eventi file");
            check(EdsSetCameraStateEventHandler(s.camera.value,kEdsStateEvent_All,Impl::onState,&s),"Registrazione eventi stato");
            s.keepAliveUnsupported=false;s.keepAliveRetries=0;s.nextKeepAlive={};s.renewShutdownTimer();
            s.log("Info","Canon: sessione aperta, rinnovo timer ogni "+std::to_string(s.config.keepAliveSeconds)+" s");
            return name.data();
        }catch(const CameraError& e){
            try{s.close();}catch(const std::exception& cleanup){s.safeLog(cleanup.what());}
            if(attempt==0&&(e.disconnected||staleSession)){s.restartSdk();s.shutdown=false;continue;}
            throw;
        }catch(...){try{s.close();}catch(const std::exception& cleanup){s.safeLog(cleanup.what());}throw;}
    }
    throw CameraError("Riconnessione Canon non riuscita",false,true);
}
void EdsCamera::disconnect(){impl_->close();}
bool EdsCamera::manualExposureForFocus(){auto& s=*impl_;const auto ae=s.get(kEdsPropID_AEMode),iso=s.get(kEdsPropID_ISOSpeed),tv=s.get(kEdsPropID_Tv);return s.module->manualExposureForFocus(ae,iso,tv);}
bool EdsCamera::connected() const {impl_->assertThread();return impl_->session&&!impl_->shutdown;}
void EdsCamera::pump() {
    auto& s=*impl_;s.assertThread();MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    if(!s.sdk)return;
    const auto eventError=EdsGetEvent();
    if(s.shutdown&&s.session) {
        s.releaseShutdownSession();
        if(eventError!=EDS_ERR_OK)s.safeLog("Eventi SDK durante disconnessione: "+errorText(eventError));
        throw CameraError("Canon ha notificato Shutdown (spegnimento, standby o perdita USB)",false,true);
    }
    check(eventError,"Eventi SDK");
    if(s.callbackError){auto e=s.callbackError;s.callbackError=nullptr;std::rethrow_exception(e);}
    if(s.bulb && std::chrono::steady_clock::now()>=s.bulbEnd) s.endBulb();
    s.renewShutdownTimer();
}
std::vector<CameraProperty> EdsCamera::properties() {
    auto& s=*impl_;s.require();std::vector<CameraProperty> result;
    for(auto p:s.module->properties()) {
        EdsUInt32 value{};auto err=EdsGetPropertyData(s.camera.value,sdkProperty(p),0,sizeof(value),&value);
        if(err==EDS_ERR_NOT_SUPPORTED) continue;check(err,"Lettura "+propertyName(p));
        CameraProperty prop{p,value,{},false,propertyLabel(p,value)};EdsPropertyDesc desc{};err=EdsGetPropertyDesc(s.camera.value,sdkProperty(p),&desc);
        if(err!=EDS_ERR_NOT_SUPPORTED) {
            check(err,"Descrizione "+propertyName(p));
            if(desc.numElements<0||desc.numElements>static_cast<EdsInt32>(std::size(desc.propDesc))) throw CameraError("Descrittore proprieta non valido");
            for(EdsInt32 i=0;i<desc.numElements;++i) {auto v=static_cast<EdsUInt32>(desc.propDesc[i]);prop.choices.push_back({v,propertyLabel(p,v)});}
            prop.writable=!prop.choices.empty();
        }
        result.push_back(std::move(prop));
    } return result;
}
void EdsCamera::set(Property p,std::uint32_t value) {
    auto& s=*impl_;s.require();const auto supported=s.module->properties();
    if(std::find(supported.begin(),supported.end(),p)==supported.end())throw CameraError("Property is not supported by the selected camera module");
    EdsPropertyDesc desc{};check(EdsGetPropertyDesc(s.camera.value,sdkProperty(p),&desc),"Valori ammessi "+propertyName(p));
    if(desc.numElements<0||desc.numElements>static_cast<EdsInt32>(std::size(desc.propDesc))) throw CameraError("Descrittore proprieta non valido");
    bool found=false;for(EdsInt32 i=0;i<desc.numElements;++i) if(static_cast<EdsUInt32>(desc.propDesc[i])==value) found=true;
    if(!found) throw CameraError("Valore non disponibile: "+propertyName(p));
    if(s.get(sdkProperty(p))!=value) s.put(sdkProperty(p),value);
}
void EdsCamera::prepareHost(std::uint64_t available,SaveDestination destination) {
    auto& s=*impl_;s.require();
    if(destination!=SaveDestination::PcOnly&&destination!=SaveDestination::PcAndCamera)throw CameraError("Invalid save destination");
    const EdsUInt32 target=destination==SaveDestination::PcOnly?kEdsSaveTo_Host:kEdsSaveTo_Both;
    if(!s.previousSaveTo)s.previousSaveTo=s.get(kEdsPropID_SaveTo);
    s.put(kEdsPropID_SaveTo,target);
    if(s.get(kEdsPropID_SaveTo)!=target)throw CameraError("La fotocamera non ha applicato la destinazione di salvataggio / Save destination not applied");
    EdsCapacity cap{};cap.bytesPerSector=4096;cap.numberOfFreeClusters=static_cast<EdsInt32>(std::min<std::uint64_t>(available/4096,INT_MAX));cap.reset=1;
    check(EdsSetCapacity(s.camera.value,cap),"Capacita host");
    s.log("Info",destination==SaveDestination::PcOnly?"Salvataggio: solo PC (nessuna copia sulla scheda)":"Salvataggio: PC + scheda fotocamera");
}
void EdsCamera::beginCapture(const SequenceItem& item,const std::filesystem::path& stem) {
    auto& s=*impl_;s.require();if(s.capturing) throw CameraError("Scatto precedente ancora attivo");
    s.capturePolicy=s.module->capturePolicy(s.get(kEdsPropID_ImageQuality));s.expectedFiles=s.capturePolicy.expectedFiles;
    auto tv=s.get(kEdsPropID_Tv);
    if((tv==s.capturePolicy.bulbTv)!=item.bulb) throw CameraError("Il Tv corrente non corrisponde alla scelta Bulb; impostarlo sulla fotocamera o nel menu Tv");
    s.stem=stem;s.capture={};s.capturing=true;
    if(item.bulb) {
        s.bulb=true; // A failed start may still have reached the camera: cleanup must attempt BulbEnd.
        check(EdsSendCommand(s.camera.value,s.capturePolicy.bulbStartCommand,0),"Inizio Bulb");
        s.bulbEnd=std::chrono::steady_clock::now()+std::chrono::duration_cast<SteadyTime::duration>(std::chrono::duration<double>(item.exposureSeconds));
    } else check(EdsSendCommand(s.camera.value,s.capturePolicy.takePictureCommand,0),"Scatto");
}
CaptureResult EdsCamera::pollCapture() {auto& s=*impl_;s.require();if(s.capture.complete)s.capturing=false;return s.capture;}
void EdsCamera::abortCapture() {
    // Reconnect is mandatory after cancellation: late transfer events must not enter a new capture.
    impl_->close();
}
void EdsCamera::setLiveView(bool enabled) {
    auto& s=*impl_;s.require();if(enabled==s.live)return;
    if(enabled) {
        s.previousEvf=s.get(kEdsPropID_Evf_OutputDevice);s.previousMode=s.get(kEdsPropID_Evf_Mode);s.frameZoom=0;s.live=true;
        try {s.put(kEdsPropID_Evf_Mode,1);s.put(kEdsPropID_Evf_OutputDevice,s.previousEvf|kEdsEvfOutputDevice_PC);}
        catch(...) {try {s.put(kEdsPropID_Evf_OutputDevice,s.previousEvf);s.put(kEdsPropID_Evf_Mode,s.previousMode);s.live=false;}catch(const std::exception& e){s.safeLog(e.what());}throw;}
    } else {s.stopPointAf();s.put(kEdsPropID_Evf_OutputDevice,s.previousEvf);s.put(kEdsPropID_Evf_Mode,s.previousMode);s.live=false;}
}
LiveFrame EdsCamera::liveFrame() {
    auto& s=*impl_;s.require();if(!s.live)return {};
    Ref<EdsStreamRef> stream;check(EdsCreateMemoryStream(0,&stream.value),"Stream Live View");
    Ref<EdsEvfImageRef> frame;check(EdsCreateEvfImageRef(stream.value,&frame.value),"Frame Live View");
    auto err=EdsDownloadEvfImage(s.camera.value,frame.value);if(err==EDS_ERR_OBJECT_NOTREADY||err==EDS_ERR_DEVICE_BUSY)return {};check(err,"Download Live View");
    EdsVoid* data{};EdsUInt64 length{};check(EdsGetPointer(stream.value,&data),"Dati Live View");check(EdsGetLength(stream.value,&length),"Dimensione Live View");
    if(length>std::numeric_limits<std::size_t>::max()) throw CameraError("Frame Live View troppo grande");
    auto* bytes=static_cast<std::uint8_t*>(data);LiveFrame result{{bytes,bytes+static_cast<std::size_t>(length)},++s.revision};
    EdsUInt32 zoom{};check(EdsGetPropertyData(frame.value,kEdsPropID_Evf_Zoom,0,sizeof(zoom),&zoom),"Zoom del frame Live View");result.zoom=static_cast<int>(zoom);
    if(zoom>1||s.frameZoom!=static_cast<int>(zoom)||!s.coordinateWidth) {
        EdsSize coordinates{};EdsPoint position{};EdsRect rect{};
        check(EdsGetPropertyData(frame.value,kEdsPropID_Evf_CoordinateSystem,0,sizeof(coordinates),&coordinates),"Coordinate Live View");
        check(EdsGetPropertyData(frame.value,kEdsPropID_Evf_ImagePosition,0,sizeof(position),&position),"Origine Live View");
        check(EdsGetPropertyData(frame.value,kEdsPropID_Evf_ZoomRect,0,sizeof(rect),&rect),"Rettangolo zoom Live View");
        result.coordinateWidth=coordinates.width;result.coordinateHeight=coordinates.height;result.imageX=position.x;result.imageY=position.y;
        result.zoomRect={rect.point.x,rect.point.y,rect.size.width,rect.size.height};
        s.coordinateWidth=coordinates.width;s.coordinateHeight=coordinates.height;
    }
    if(zoom==1){result.coordinateWidth=s.coordinateWidth;result.coordinateHeight=s.coordinateHeight;}
    s.frameZoom=static_cast<int>(zoom);
    result.previewRegion=s.module->previewRegion(result);
    return result;
}
LensFocusStatus EdsCamera::lensFocusStatus() {
    auto& s=*impl_;s.require();EdsUInt32 lens{},af{};
    auto result=EdsGetPropertyData(s.camera.value,kEdsPropID_LensStatus,0,sizeof(lens),&lens);
    if(result==EDS_ERR_NOT_SUPPORTED)return {LensFocusState::Unknown,"Stato obiettivo non disponibile"};
    check(result,"Lettura presenza obiettivo");
    const auto attached=s.module->lensState(lens,0);
    if(attached==LensFocusState::NoLens){s.focusUnsupported=false;return {attached,{}};}
    if(attached==LensFocusState::Unknown)return {attached,"Stato obiettivo non riconosciuto"};
    result=EdsGetPropertyData(s.camera.value,kEdsPropID_AFMode,0,sizeof(af),&af);
    if(result==EDS_ERR_NOT_SUPPORTED)return {LensFocusState::Unknown,"Stato AF/MF non disponibile"};
    check(result,"Lettura AF/MF obiettivo");
    const auto state=s.module->lensState(lens,af);
    if(state!=LensFocusState::Ready)return {state,state==LensFocusState::Unknown?"Stato AF/MF non riconosciuto":""};
    if(s.focusUnsupported)return {LensFocusState::Unsupported,{}};
    return {LensFocusState::Ready,{}};
}
void EdsCamera::focus(int step) {
    auto& s=*impl_;s.require();if(!s.live)throw CameraError("Attivare Live View per la messa a fuoco");
    const auto command=static_cast<EdsInt32>(s.module->focusDrive(step));
    if(lensFocusStatus().state!=LensFocusState::Ready)throw CameraError("Movimento focus non disponibile: collegare un obiettivo compatibile e impostarlo su AF");
    const auto result=EdsSendCommand(s.camera.value,kEdsCameraCommand_DriveLensEvf,command);
    if(result==EDS_ERR_NOT_SUPPORTED)s.focusUnsupported=true;
    check(result,"Messa a fuoco"); // No retry: a relative movement must not be repeated ambiguously.
}
void EdsCamera::startPointAutofocus(double u,double v){
    auto& s=*impl_;s.require();
    if(!std::isfinite(u)||!std::isfinite(v)||u<0||u>1||v<0||v>1)throw CameraError("Invalid point autofocus coordinates or timeout");
    if(!s.live||s.frameZoom!=1||s.coordinateWidth<=0||s.coordinateHeight<=0)throw CameraError("Point autofocus requires a fresh 1x Live View frame");
    if(s.capturing||s.pointAfRunning)throw CameraError("Camera is busy");
    if(lensFocusStatus().state!=LensFocusState::Ready)throw CameraError("Point autofocus requires an AF-ready lens");
    const auto mode=s.module->pointAutofocusMode();
    EdsPropertyDesc modes{};check(EdsGetPropertyDesc(s.camera.value,kEdsPropID_Evf_AFMode,&modes),"Available Live AF modes");
    if(modes.numElements<0||modes.numElements>128||std::find(modes.propDesc,modes.propDesc+modes.numElements,static_cast<EdsInt32>(mode))==modes.propDesc+modes.numElements)
        throw CameraError("Camera does not offer single-point Live AF");
    s.pointAf={PointAfPhase::Running,u,v,"Point autofocus: focusing"};
    s.previousAfMode=s.get(kEdsPropID_Evf_AFMode);
    try{
        s.put(kEdsPropID_Evf_AFMode,mode);
        check(EdsSendCommand(s.camera.value,kEdsCameraCommand_DoEvfAf,kEdsCameraCommand_EvfAf_OFF),"Reset point autofocus");
        // Legacy EOS uses the upper-left of the 1/5-size AF/zoom frame.
        // EdsSetFramePoint is only documented for newer cameras, not the 1100D.
        const auto fraction=s.module->pointAutofocusFrameFraction();
        if(!std::isfinite(fraction)||fraction<=0||fraction>1)throw CameraError("Invalid camera AF frame geometry");
        EdsPoint position{static_cast<EdsInt32>(std::lround(std::clamp(u-fraction/2,0.,1-fraction)*s.coordinateWidth)),static_cast<EdsInt32>(std::lround(std::clamp(v-fraction/2,0.,1-fraction)*s.coordinateHeight))};
        check(EdsSetPropertyData(s.camera.value,kEdsPropID_Evf_ZoomPosition,0,sizeof(position),&position),"Point autofocus position");
        s.pointAf.u=double(position.x)/s.coordinateWidth+fraction/2;s.pointAf.v=double(position.y)/s.coordinateHeight+fraction/2;
        const auto info=s.focusInfo();s.pointAfPendingSeen=true;
        for(EdsUInt32 i=0;i<info.pointNumber;++i)if(info.focusPoint[i].valid&&(info.focusPoint[i].selected==1||info.focusPoint[i].selected==3)&&((info.focusPoint[i].justFocus&15)==1||(info.focusPoint[i].justFocus&15)==2))s.pointAfPendingSeen=false;
        s.pointAfRevision=s.revision;s.pointAfRunning=true;
        check(EdsSendCommand(s.camera.value,kEdsCameraCommand_DoEvfAf,kEdsCameraCommand_EvfAf_ON),"Start point autofocus");
    }catch(...){try{s.stopPointAf();}catch(const std::exception& e){s.safeLog(e.what());}throw;}
}
PointAfStatus EdsCamera::pollPointAutofocus(){
    auto& s=*impl_;s.require();if(!s.pointAfRunning)return s.pointAf;
    if(lensFocusStatus().state!=LensFocusState::Ready)throw CameraError("Point autofocus requires an AF-ready lens");
    const auto info=s.focusInfo();int result=-1;
    for(EdsUInt32 i=0;i<info.pointNumber;++i){const auto& p=info.focusPoint[i];if(p.valid&&(p.selected==1||p.selected==3)){result=static_cast<int>(p.justFocus&15);break;}}
    if(result==0||result==4)s.pointAfPendingSeen=true;
    if(s.pointAfPendingSeen&&s.revision>s.pointAfRevision+1){
        if(result==1){s.pointAf.phase=PointAfPhase::Complete;s.pointAf.detail="Point autofocus: OK, confirmed by camera";}
        else if(result==2){s.pointAf.phase=PointAfPhase::Failed;s.pointAf.detail="Point autofocus: camera could not focus";}
    }
    return s.pointAf;
}
void EdsCamera::stopPointAutofocus(){impl_->stopPointAf();}
void EdsCamera::zoom(int ratio) {
    impl_->require();if(!impl_->live)throw CameraError("Attivare Live View per lo zoom");
    const auto choices=impl_->module->info().zoomRatios;
    if(std::find(choices.begin(),choices.end(),ratio)==choices.end())throw std::invalid_argument("Zoom non valido per il modulo camera");
    if(ratio>1&&(impl_->coordinateWidth<=0||impl_->coordinateHeight<=0))throw CameraError("Wait for a Live View frame before zooming");
    impl_->put(kEdsPropID_Evf_Zoom,static_cast<EdsUInt32>(ratio));
    if(ratio>1){
        // ZoomPosition is the upper-left corner, not the centre. Set AFTER
        // the ratio: the native border is 1/5 at 5x and 1/10 at 10x.
        const double origin=(1.-1./ratio)/2;
        EdsPoint position{static_cast<EdsInt32>(std::lround(impl_->coordinateWidth*origin)),static_cast<EdsInt32>(std::lround(impl_->coordinateHeight*origin))};
        check(EdsSetPropertyData(impl_->camera.value,kEdsPropID_Evf_ZoomPosition,0,sizeof(position),&position),"Center Live View zoom");
    }
    impl_->frameZoom=0;
    // Zoom is write-only on the camera; the returned EVF image confirms the actual ratio.
}
}
