#define NOMINMAX
#include <windows.h>
#include <EDSDK.h>
#include <EDSDKErrors.h>
#include <astro/EdsCamera.hpp>
#include <astro/PropertyLabels.hpp>
#include <algorithm>
#include <array>
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
    case EDS_ERR_SESSION_ALREADY_OPEN:return "Sessione gia aperta; chiudere EOS Utility e riconnettere";
    case EDS_ERR_COMM_PORT_IS_IN_USE:return "USB in uso da un altro programma (es. EOS Utility)";
    case EDS_ERR_TAKE_PICTURE_CARD_NG:return "Errore della scheda; verificare presenza, spazio e stato";
    default:return "Errore Canon EDSDK";
    }
}
void check(EdsError e,const std::string& context) {if(e!=EDS_ERR_OK) throw CameraError(context+": "+errorText(e)+" ("+hex(e)+")",e==EDS_ERR_DEVICE_BUSY);}
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
    std::thread::id owner=std::this_thread::get_id();
    bool com{},sdk{},session{},shutdown{},live{},capturing{},bulb{};
    bool focusUnsupported{};
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
    void require() const {assertThread();if(!session||shutdown) throw CameraError("Fotocamera non connessa");}
    EdsUInt32 get(EdsPropertyID p) {require();EdsUInt32 v{};check(EdsGetPropertyData(camera.value,p,0,sizeof(v),&v),"Lettura "+hex(p));return v;}
    void put(EdsPropertyID p,EdsUInt32 v) {require();check(EdsSetPropertyData(camera.value,p,0,sizeof(v),&v),"Impostazione "+hex(p));}
    void endBulb() {if(bulb){check(EdsSendCommand(camera.value,kEdsCameraCommand_BulbEnd,0),"Fine Bulb");bulb=false;}}
    void close() {
        assertThread();std::exception_ptr first;
        auto cleanup=[&](auto fn){try{fn();}catch(...){if(!first)first=std::current_exception();}};
        if(session) {
            cleanup([&]{endBulb();});
            if(live&&!shutdown) cleanup([&]{put(kEdsPropID_Evf_OutputDevice,previousEvf);put(kEdsPropID_Evf_Mode,previousMode);});
            if(previousSaveTo&&!shutdown)cleanup([&]{put(kEdsPropID_SaveTo,*previousSaveTo);});
            cleanup([&]{check(EdsSetObjectEventHandler(camera.value,kEdsObjectEvent_All,nullptr,nullptr),"Rimozione handler oggetti");});
            cleanup([&]{check(EdsSetCameraStateEventHandler(camera.value,kEdsStateEvent_All,nullptr,nullptr),"Rimozione handler stato");});
            cleanup([&]{check(EdsCloseSession(camera.value),"Chiusura sessione");});
        }
        session=false;live=false;capturing=false;bulb=false;previousSaveTo.reset();camera.reset();
        if(first) std::rethrow_exception(first);
    }
    static EdsError EDSCALLBACK onState(EdsStateEvent event,EdsUInt32,EdsVoid* context) noexcept {
        auto& self=*static_cast<Impl*>(context);
        if(event==kEdsStateEvent_Shutdown) self.shutdown=true;
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
std::string EdsCamera::connect() {
    auto& s=*impl_;s.assertThread();if(s.session) throw CameraError("Sessione gia aperta");s.shutdown=false;s.callbackError=nullptr;s.focusUnsupported=false;
    Ref<EdsCameraListRef> list;check(EdsGetCameraList(&list.value),"Elenco fotocamere");EdsUInt32 count{};
    check(EdsGetChildCount(list.value,&count),"Numero fotocamere");if(count!=1) throw CameraError(count==0?"Nessuna fotocamera Canon rilevata":"Collegare una sola fotocamera Canon");
    check(EdsGetChildAtIndex(list.value,0,&s.camera.value),"Selezione fotocamera");
    try {
        check(EdsOpenSession(s.camera.value),"Apertura sessione");s.session=true;
        check(EdsSetObjectEventHandler(s.camera.value,kEdsObjectEvent_All,Impl::onObject,&s),"Registrazione eventi file");
        check(EdsSetCameraStateEventHandler(s.camera.value,kEdsStateEvent_All,Impl::onState,&s),"Registrazione eventi stato");
        std::array<char,EDS_MAX_NAME> name{};check(EdsGetPropertyData(s.camera.value,kEdsPropID_ProductName,0,static_cast<EdsUInt32>(name.size()),name.data()),"Nome fotocamera");return name.data();
    } catch(...) {try{s.close();}catch(const std::exception& e){s.safeLog(e.what());}throw;}
}
void EdsCamera::disconnect(){impl_->close();}
bool EdsCamera::connected() const {impl_->assertThread();return impl_->session&&!impl_->shutdown;}
void EdsCamera::pump() {
    auto& s=*impl_;s.assertThread();MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    check(EdsGetEvent(),"Eventi SDK");
    if(s.callbackError){auto e=s.callbackError;s.callbackError=nullptr;std::rethrow_exception(e);}
    if(s.bulb && std::chrono::steady_clock::now()>=s.bulbEnd) s.endBulb();
}
std::vector<CameraProperty> EdsCamera::properties() {
    auto& s=*impl_;s.require();std::vector<CameraProperty> result;
    for(auto p:{Property::Iso,Property::Aperture,Property::Shutter,Property::ExposureMode,Property::WhiteBalance,Property::ColorTemperature,Property::PictureStyle,Property::ImageQuality}) {
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
    auto& s=*impl_;s.require();EdsPropertyDesc desc{};check(EdsGetPropertyDesc(s.camera.value,sdkProperty(p),&desc),"Valori ammessi "+propertyName(p));
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
    // Canon's quality encoding has 0xff0f in the secondary slot when only one file is produced.
    auto quality=s.get(kEdsPropID_ImageQuality);s.expectedFiles=(quality&0xffff)==0xff0f?1:2;
    auto tv=s.get(kEdsPropID_Tv);constexpr EdsUInt32 bulbTv=0x0c; // SDK Tv protocol value, not a menu of supported choices.
    if((tv==bulbTv)!=item.bulb) throw CameraError("Il Tv corrente non corrisponde alla scelta Bulb; impostarlo sulla fotocamera o nel menu Tv");
    s.stem=stem;s.capture={};s.capturing=true;
    if(item.bulb) {
        s.bulb=true; // A failed start may still have reached the camera: cleanup must attempt BulbEnd.
        check(EdsSendCommand(s.camera.value,kEdsCameraCommand_BulbStart,0),"Inizio Bulb");
        s.bulbEnd=std::chrono::steady_clock::now()+std::chrono::duration_cast<SteadyTime::duration>(std::chrono::duration<double>(item.exposureSeconds));
    } else check(EdsSendCommand(s.camera.value,kEdsCameraCommand_TakePicture,0),"Scatto");
}
CaptureResult EdsCamera::pollCapture() {auto& s=*impl_;s.require();if(s.capture.complete)s.capturing=false;return s.capture;}
void EdsCamera::abortCapture() {
    // Reconnect is mandatory after cancellation: late transfer events must not enter a new capture.
    impl_->close();
}
void EdsCamera::setLiveView(bool enabled) {
    auto& s=*impl_;s.require();if(enabled==s.live)return;
    if(enabled) {
        s.previousEvf=s.get(kEdsPropID_Evf_OutputDevice);s.previousMode=s.get(kEdsPropID_Evf_Mode);s.live=true;
        try {s.put(kEdsPropID_Evf_Mode,1);s.put(kEdsPropID_Evf_OutputDevice,s.previousEvf|kEdsEvfOutputDevice_PC);}
        catch(...) {try {s.put(kEdsPropID_Evf_OutputDevice,s.previousEvf);s.put(kEdsPropID_Evf_Mode,s.previousMode);s.live=false;}catch(const std::exception& e){s.safeLog(e.what());}throw;}
    } else {s.put(kEdsPropID_Evf_OutputDevice,s.previousEvf);s.put(kEdsPropID_Evf_Mode,s.previousMode);s.live=false;}
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
    if(zoom>1) {
        EdsSize coordinates{};EdsPoint position{};EdsRect rect{};
        check(EdsGetPropertyData(frame.value,kEdsPropID_Evf_CoordinateSystem,0,sizeof(coordinates),&coordinates),"Coordinate Live View");
        check(EdsGetPropertyData(frame.value,kEdsPropID_Evf_ImagePosition,0,sizeof(position),&position),"Origine Live View");
        check(EdsGetPropertyData(frame.value,kEdsPropID_Evf_ZoomRect,0,sizeof(rect),&rect),"Rettangolo zoom Live View");
        result.coordinateWidth=coordinates.width;result.coordinateHeight=coordinates.height;result.imageX=position.x;result.imageY=position.y;
        result.zoomRect={rect.point.x,rect.point.y,rect.size.width,rect.size.height};
    }
    return result;
}
LensFocusStatus EdsCamera::lensFocusStatus() {
    auto& s=*impl_;s.require();EdsUInt32 lens{},af{};
    auto result=EdsGetPropertyData(s.camera.value,kEdsPropID_LensStatus,0,sizeof(lens),&lens);
    if(result==EDS_ERR_NOT_SUPPORTED)return {LensFocusState::Unknown,"Stato obiettivo non disponibile"};
    check(result,"Lettura presenza obiettivo");
    if(lens==0){s.focusUnsupported=false;return {LensFocusState::NoLens,{}};}
    if(lens!=1)return {LensFocusState::Unknown,"Stato obiettivo non riconosciuto"};
    result=EdsGetPropertyData(s.camera.value,kEdsPropID_AFMode,0,sizeof(af),&af);
    if(result==EDS_ERR_NOT_SUPPORTED)return {LensFocusState::Unknown,"Stato AF/MF non disponibile"};
    check(result,"Lettura AF/MF obiettivo");
    // Canon API 5.2.24: 0/1/2 are AF; 3 is the read-only manual-focus state.
    if(af==3)return {LensFocusState::ManualFocus,{}};
    if(af>2)return {LensFocusState::Unknown,"Stato AF/MF non riconosciuto"};
    if(s.focusUnsupported)return {LensFocusState::Unsupported,{}};
    return {LensFocusState::Ready,{}};
}
void EdsCamera::focus(int step) {
    auto& s=*impl_;s.require();if(!s.live)throw CameraError("Attivare Live View per la messa a fuoco");
    EdsInt32 command{};
    switch(step){case -3:command=kEdsEvfDriveLens_Near3;break;case -2:command=kEdsEvfDriveLens_Near2;break;case -1:command=kEdsEvfDriveLens_Near1;break;case 1:command=kEdsEvfDriveLens_Far1;break;case 2:command=kEdsEvfDriveLens_Far2;break;case 3:command=kEdsEvfDriveLens_Far3;break;default:throw std::invalid_argument("Passo focus non valido");}
    if(lensFocusStatus().state!=LensFocusState::Ready)throw CameraError("Movimento focus non disponibile: collegare un obiettivo compatibile e impostarlo su AF");
    const auto result=EdsSendCommand(s.camera.value,kEdsCameraCommand_DriveLensEvf,command);
    if(result==EDS_ERR_NOT_SUPPORTED)s.focusUnsupported=true;
    check(result,"Messa a fuoco"); // No retry: a relative movement must not be repeated ambiguously.
}
void EdsCamera::zoom(int ratio) {
    if(ratio!=1&&ratio!=5&&ratio!=10)throw std::invalid_argument("Zoom non valido");
    impl_->require();if(!impl_->live)throw CameraError("Attivare Live View per lo zoom");
    impl_->put(kEdsPropID_Evf_Zoom,static_cast<EdsUInt32>(ratio));
    // Zoom is write-only on the camera; the returned EVF image confirms the actual ratio.
}
}
