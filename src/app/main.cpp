#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <astro/ConfigManager.hpp>
#include <astro/Infrastructure.hpp>
#include <astro/StackMath.hpp>
#include <astro/MainWindow.hpp>
#include "PhotoPreview.hpp"
#include "NightDisplay.hpp"
#include "FocusPeaking.hpp"
#include "LiveViewPreview.hpp"
#include "ImageAnalysis.hpp"
#include "StarAnalysis.hpp"
#include <astro/BuildVersion.hpp>
#include "ControlQueue.hpp"
#include "ControlApi.hpp"
#include "ControlImage.hpp"
#include "McpTransport.hpp"
#include <astro/RemoteServer.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <map>
#include <tuple>
#include <fstream>
#include <limits>
#include <cmath>
#include <sstream>

namespace {
using Microsoft::WRL::ComPtr;
class WindowIcons {
public:
    ~WindowIcons(){for(auto& [dpi,pair]:icons_){(void)dpi;DestroyIcon(pair.first);DestroyIcon(pair.second);}}
    void apply(HWND window) {
        const UINT dpi=GetDpiForWindow(window);
        auto [entry,created]=icons_.try_emplace(dpi);
        if(created) {
            auto load=[&](int metric){return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,GetSystemMetricsForDpi(metric,dpi),GetSystemMetricsForDpi(metric+1,dpi),LR_DEFAULTCOLOR));};
            entry->second={load(SM_CXSMICON),load(SM_CXICON)};
            if(!entry->second.first||!entry->second.second)throw std::runtime_error("Unable to load application icon resource");
        }
        if(reinterpret_cast<HICON>(SendMessageW(window,WM_GETICON,ICON_SMALL,0))!=entry->second.first)SendMessageW(window,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(entry->second.first));
        if(reinterpret_cast<HICON>(SendMessageW(window,WM_GETICON,ICON_BIG,0))!=entry->second.second)SendMessageW(window,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(entry->second.second));
    }
private:
    std::map<UINT,std::pair<HICON,HICON>> icons_;
};
void check(HRESULT result) {if(FAILED(result))throw std::runtime_error("Decodifica JPEG WIC fallita: "+std::to_string(result));}
struct Texture {
    std::uint64_t decodedFrames{},processedFrames{};
    double decodeMs{},histogramMs{},peakingMs{},uploadMs{};
    GLuint id{};int width{},height{};std::uint64_t revision{};
    astro::PreviewPixels livePixels,markedPixels;bool peaking{},starPeaking{};float threshold{};
    astro::Histogram histogram;astro::FocusTracker focusTracker;astro::FocusReading focus;
    std::uint64_t focusRevision{},lastContext{};
    std::chrono::steady_clock::time_point lastFrameTime{};
    ~Texture(){if(id)glDeleteTextures(1,&id);}
    void update(const astro::LiveFrame& frame,bool enabled,float level,bool stars,astro::FocusTarget target,std::uint64_t context) {
        if(frame.jpeg.empty())return;
        const bool changed=frame.revision!=revision;
        if(changed)lastFrameTime=std::chrono::steady_clock::now();
        if(std::chrono::steady_clock::now()-lastFrameTime>std::chrono::seconds(2)){focus={};focusTracker.reset();return;}
        context^=target.revision*1099511628211ull;
        for(int value:{frame.zoom,frame.zoomRect.x,frame.zoomRect.y,frame.zoomRect.width,frame.zoomRect.height})context=(context^static_cast<unsigned>(value))*1099511628211ull;
        if(!changed&&peaking==enabled&&threshold==level&&starPeaking==stars&&focusRevision==target.revision&&lastContext==context)return;
        auto stage=std::chrono::steady_clock::now();
        const auto elapsed=[&]{const auto now=std::chrono::steady_clock::now();const auto ms=std::chrono::duration<double,std::milli>(now-stage).count();stage=now;return ms;};
        if(changed){livePixels=astro::prepareLiveViewPreview(astro::decodeImage(frame.jpeg),frame);decodeMs+=elapsed();++decodedFrames;histogram=astro::imageHistogram(livePixels);histogramMs+=elapsed();}
        auto marked=enabled?astro::focusPeaking(livePixels,level,stars):astro::PreviewPixels{};
        const auto& display=enabled?marked:livePixels;
        focus=focusTracker.update(astro::measureFocus(livePixels,target,enabled?&display:nullptr),frame.revision,context);
        peakingMs+=elapsed();++processedFrames;
        focusRevision=target.revision;lastContext=context;upload(display);uploadMs+=elapsed();
        markedPixels=std::move(marked);
        revision=frame.revision;peaking=enabled;threshold=level;starPeaking=stars;
    }
    void uploadStack(const astro::StackPreview& image){
        if(image.rgba16.size()!=std::size_t(image.width)*image.height*4)throw std::runtime_error("Invalid 16-bit stack preview");
        if(!id){glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}else glBindTexture(GL_TEXTURE_2D,id);
        if(width==image.width&&height==image.height)glTexSubImage2D(GL_TEXTURE_2D,0,0,0,image.width,image.height,GL_RGBA,GL_UNSIGNED_SHORT,image.rgba16.data());else glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16,image.width,image.height,0,GL_RGBA,GL_UNSIGNED_SHORT,image.rgba16.data());
        width=image.width;height=image.height;revision=image.revision;
    }
    void uploadPhoto(const astro::PreviewPixels& image){histogram=astro::imageHistogram(image);upload(image);}
    void upload(const astro::PreviewPixels& image) {
        const auto w=image.width,h=image.height;const auto& pixels=image.rgba;
        if(!id){glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}
        else glBindTexture(GL_TEXTURE_2D,id);
        if(width==w&&height==h)glTexSubImage2D(GL_TEXTURE_2D,0,0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        else glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        width=w;height=h;
    }
};

void screenshot(const std::filesystem::path& path,int width,int height,HWND nativeWindow) {
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width)*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    for(std::size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    const auto stride=static_cast<std::size_t>(width)*4;
    for(int y=0;y<height/2;++y)std::swap_ranges(pixels.begin()+y*stride,pixels.begin()+(y+1)*stride,pixels.begin()+(height-y-1)*stride);
    auto region=CreateRectRgn(0,0,0,0);
    if(GetWindowRgn(nativeWindow,region)!=ERROR)for(int y=0;y<height;++y)for(int x=0;x<width;++x)
        if(!PtInRegion(region,x,y))pixels[(static_cast<std::size_t>(y)*width+x)*4+3]=0;
    DeleteObject(region);
    ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;check(factory->CreateStream(&stream));check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;check(encoder->CreateNewFrame(&frame,nullptr));check(frame->Initialize(nullptr));check(frame->SetSize(width,height));
    auto format=GUID_WICPixelFormat32bppBGRA;check(frame->SetPixelFormat(&format));
    if(format!=GUID_WICPixelFormat32bppBGRA)throw std::runtime_error("Formato PNG non supportato");
    check(frame->WritePixels(height,static_cast<UINT>(stride),static_cast<UINT>(pixels.size()),pixels.data()));check(frame->Commit());check(encoder->Commit());
}
struct RunOptions {
    bool smoke{},light{},night{},connect{},startLive{},smokeStack{};int remotePort{};std::filesystem::path previewFile,pairingTestFile;
    std::string language;
    std::string mcpTransport;
    float dpi{};
    int width{},height{};
};
class DpiFonts {
public:
    explicit DpiFonts(const std::filesystem::path& path) {
        std::ifstream file(path,std::ios::binary);bytes_={std::istreambuf_iterator<char>(file),{}};
        if(bytes_.empty())throw std::runtime_error("Font Cousine-Regular.ttf assente");
        current_=this;
        ImGui::GetPlatformIO().Platform_OnChangedViewport=[](ImGuiViewport* vp){if(current_)current_->select(vp->DpiScale);};
    }
    ~DpiFonts(){ImGui::GetPlatformIO().Platform_OnChangedViewport=nullptr;current_=nullptr;}
    float update(GLFWwindow* window,float userScale,float overrideDpi,bool light,bool night) {
        float x=1,y=1;glfwGetWindowContentScale(window,&x,&y);
        const float monitor=overrideDpi>0?overrideDpi:std::max(x,y);
        std::map<int,ImFont*> wanted;wanted[key(monitor)]=nullptr;
        int count{};auto** monitors=glfwGetMonitors(&count);
        for(int i=0;i<count;++i){float mx{},my{};glfwGetMonitorContentScale(monitors[i],&mx,&my);wanted[key(overrideDpi>0?overrideDpi:std::max(mx,my))]=nullptr;}
        bool rebuild=userScale!=scale_||overrideDpi!=override_||wanted.size()!=fonts_.size();
        for(const auto& [dpi,font]:wanted){(void)font;if(!fonts_.contains(dpi))rebuild=true;}
        scale_=userScale;override_=overrideDpi;light_=light;night_=night;
        auto& io=ImGui::GetIO();
        if(rebuild) {
            // One crisp font per monitor DPI, all in the same atlas. Rebuild only outside a frame.
            ImGui_ImplOpenGL3_DestroyFontsTexture();io.Fonts->Clear();fonts_=wanted;
            for(auto& [dpi,entry]:fonts_){ImFontConfig font;font.FontDataOwnedByAtlas=false;entry=io.Fonts->AddFontFromMemoryTTF(bytes_.data(),static_cast<int>(bytes_.size()),std::max(9.f,std::floor(13*scale_*dpi/96.f)),&font);if(!entry)throw std::runtime_error("Creazione font DPI fallita");}
            if(!io.Fonts->Build()||!ImGui_ImplOpenGL3_CreateFontsTexture())throw std::runtime_error("Creazione atlante DPI fallita");
        }
        io.FontDefault=fonts_.at(key(monitor));io.FontGlobalScale=1;select(monitor);return monitor;
    }
private:
    static int key(float dpi){return static_cast<int>(std::lround(dpi*96));}
    void select(float dpi) {
        if(fonts_.empty())return;const auto wanted=key(override_>0?override_:dpi);
        auto entry=fonts_.find(wanted);if(entry==fonts_.end())entry=fonts_.begin();
        astro::ui::applyTheme(light_,night_);ImGui::GetStyle().ScaleAllSizes(scale_*entry->first/96.f);
        ImGui::SetCurrentFont(entry->second);
    }
    inline static DpiFonts* current_{};
    std::vector<char> bytes_;std::map<int,ImFont*> fonts_;float scale_{},override_{};bool light_{},night_{};
};
void saveLayout(const std::filesystem::path& path,bool automatic) {
    std::size_t size{};const char* settings=ImGui::SaveIniSettingsToMemory(&size);
    auto temporary=path;temporary+=".tmp";
    {std::ofstream out(temporary,std::ios::binary);out<<"; astrophoto-auto-layout="<<(automatic?1:0)<<"\n";out.write(settings,static_cast<std::streamsize>(size));out.flush();if(!out)throw std::runtime_error("Salvataggio layout ImGui fallito");}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Sostituzione layout ImGui fallita");
    ImGui::GetIO().WantSaveIniSettings=false;
}
int run(const RunOptions& options) {
    std::unique_ptr<astro::mcp::InstanceGuard> instance;if(!options.smoke)instance=std::make_unique<astro::mcp::InstanceGuard>();
    std::unique_ptr<astro::ControlQueue> control;if(!options.smoke)control=std::make_unique<astro::ControlQueue>();
    wchar_t module[32768]{};auto length=GetModuleFileNameW(nullptr,module,static_cast<DWORD>(std::size(module)));if(!length||length==std::size(module))throw std::runtime_error("Percorso eseguibile non disponibile");
    auto folder=std::filesystem::path(module).parent_path();auto configPath=folder/"user.json";
    auto config=astro::ConfigManager::load(std::filesystem::exists(configPath)?configPath:folder/"defaults.json");
    if(options.light){config.lightTheme=true;config.nightTheme=false;}if(options.night){config.nightTheme=true;config.lightTheme=false;}
    if(!options.language.empty())config.language=options.language;
    if(config.outputDirectory.is_relative())config.outputDirectory=folder/config.outputDirectory;
    if(!glfwInit())throw std::runtime_error("Inizializzazione GLFW fallita");
    struct GlfwGuard{~GlfwGuard(){glfwTerminate();}} glfwGuard;
    WindowIcons windowIcons;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR,GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED,GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    float primaryX=1,primaryY=1;auto* monitor=glfwGetPrimaryMonitor();
    int areaX{},areaY{},areaWidth=config.windowWidth,areaHeight=config.windowHeight;
    if(monitor){glfwGetMonitorContentScale(monitor,&primaryX,&primaryY);glfwGetMonitorWorkarea(monitor,&areaX,&areaY,&areaWidth,&areaHeight);}
    const std::string windowTitle=std::string("AstroPhoto v")+astro::ApplicationVersion;
    GLFWwindow* window=glfwCreateWindow(std::max(640,std::min(config.windowWidth,areaWidth)),std::max(480,std::min(config.windowHeight,areaHeight)),windowTitle.c_str(),nullptr,nullptr);if(!window)throw std::runtime_error("Creazione finestra/OpenGL fallita");
    struct WindowGuard{GLFWwindow* w;~WindowGuard(){glfwDestroyWindow(w);}} windowGuard{window};
    windowIcons.apply(glfwGetWin32Window(window));
    glfwSetWindowPos(window,areaX,areaY);
    glfwSetWindowSizeLimits(window,640,480,GLFW_DONT_CARE,GLFW_DONT_CARE);
    if(options.smoke)glfwSetWindowSize(window,options.width?options.width:1600,options.height?options.height:900);
    else {glfwShowWindow(window);glfwMaximizeWindow(window);}
    // Windows composes the single native surface. Keep the render loop free of
    // a driver VSync wait; its cancellable idle budget already limits CPU use.
    glfwMakeContextCurrent(window);glfwSwapInterval(0);
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    struct ImGuiGuard{bool renderer{},platform{};~ImGuiGuard(){if(renderer)ImGui_ImplOpenGL3_Shutdown();if(platform)ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();}} imguiGuard;
    ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly=true;
    const auto layoutPath=folder/"workspace-layout.ini";std::string layout;
    if(!options.smoke){std::ifstream layoutFile(layoutPath,std::ios::binary);layout={std::istreambuf_iterator<char>(layoutFile),{}};}
    if(!layout.empty())ImGui::LoadIniSettingsFromMemory(layout.data(),layout.size());
    imguiGuard.platform=ImGui_ImplGlfw_InitForOpenGL(window,true);
    if(!imguiGuard.platform)throw std::runtime_error("Inizializzazione piattaforma ImGui fallita");
    imguiGuard.renderer=ImGui_ImplOpenGL3_Init("#version 130");
    if(!imguiGuard.renderer)throw std::runtime_error("Inizializzazione rendering ImGui fallita");

    DpiFonts fonts(folder/"Cousine-Regular.ttf");
    astro::NightDisplay nightDisplay;Texture texture,capturedTexture,stackTexture;auto stacking=astro::stacking::makeStackProcessor();astro::PhotoPreview photos;astro::Mailbox mailbox;
    astro::CameraWorker worker(mailbox,config,configPath,[](const astro::LiveFrame& frame,double snr,const std::vector<astro::StarSample>& tracked,double radius){return astro::measureStars(astro::prepareLiveViewPreview(astro::decodeImage(frame.jpeg),frame),snr,tracked,radius);},{},[&photos](const astro::CapturedPhoto& photo){photos.request(photo);},stacking);
    astro::ui::MainWindow ui(mailbox,config,!layout.empty());
    if(options.smokeStack){auto view=ui.controlState();view.viewStack=true;ui.controlState(view);}
    ui.automaticLayout(layout.find("; astrophoto-auto-layout=0")==std::string::npos);
    astro::ControlApi controlApi(mailbox,ui);
    std::unique_ptr<astro::remote::Server> remoteServer;
    if(!options.smoke)remoteServer=std::make_unique<astro::remote::Server>(*control,folder/"remote.json");
    if(remoteServer&&options.remotePort)remoteServer->start(options.remotePort);
    // Explicit, opt-in fixture for physical Android instrumentation. Never log
    // the one-time secret; remove it on authentication, expiry or shutdown.
    struct PairingFixture {std::filesystem::path path;bool written{};~PairingFixture(){if(written){std::error_code error;std::filesystem::remove(path,error);}}} pairingFixture{options.pairingTestFile};
    struct CloseControl {astro::ControlQueue* queue;~CloseControl(){if(queue)queue->close();}} closeControl{control.get()};
    std::unique_ptr<astro::mcp::Server> mcpServer;
    if(!options.smoke){try{mcpServer=std::make_unique<astro::mcp::Server>(*control,options.mcpTransport.empty()?astro::mcp::configuredTransport():options.mcpTransport);ui.mcpStatus(mcpServer->endpoint());}
        catch(const std::exception& e){if(options.mcpTransport=="stdio")throw;ui.mcpStatus(std::string("MCP unavailable: ")+e.what());auto view=ui.controlState();view.error=e.what();ui.controlState(view);}}
    if(options.connect) {astro::Command command;command.action=astro::Action::Connect;command.config=config;mailbox.send(std::move(command));}
    if(options.startLive){astro::Command command;command.action=astro::Action::LiveView;command.value=1;command.config=config;mailbox.send(std::move(command));auto view=ui.controlState();view.viewLive=true;ui.controlState(view);}
    int frames=0;std::string frameError;std::uint64_t previewRevision{};std::string testPreviewStatus;
    if(!options.previewFile.empty()){auto pixels=astro::decodePreview(options.previewFile);capturedTexture.uploadPhoto(pixels);testPreviewStatus="File di collaudo | "+pixels.description;}
    bool appliedTurbo=config.turboLiveStream, savedAutomatic=ui.automaticLayout();
    ImVec2 lastClientSize{static_cast<float>(config.windowWidth),static_cast<float>(config.windowHeight)};
    while(!glfwWindowShouldClose(window)) {
        if(remoteServer){
            const auto remote=remoteServer->status();
            if(!pairingFixture.path.empty()){
                if(!pairingFixture.written&&remote.running&&!remote.token.empty()&&!remote.addresses.empty()){
                    if(std::filesystem::exists(pairingFixture.path))throw std::runtime_error("Pairing test fixture already exists");
                    std::ofstream file(pairingFixture.path,std::ios::binary);file<<"astrophoto-pair:1|"<<remote.addresses.front()<<"|"<<remote.token;file.close();if(!file)throw std::runtime_error("Cannot write pairing test fixture");pairingFixture.written=true;
                }else if(pairingFixture.written&&remote.token.empty()){std::error_code error;std::filesystem::remove(pairingFixture.path,error);}
            }
            ui.remoteStatus(remote);switch(ui.takeRemoteAction()){case 1:remoteServer->start(ui.remotePort());break;case 2:remoteServer->stop();break;case 3:remoteServer->rotateSecret();break;}}
        const auto uiFrameStarted=std::chrono::steady_clock::now();
        glfwPollEvents();const auto state=mailbox.snapshot();
        if(ui.controlConfig().turboLiveStream!=appliedTurbo){appliedTurbo=ui.controlConfig().turboLiveStream;astro::Command command;command.action=astro::Action::SetTurboLive;command.value=appliedTurbo;mailbox.send(std::move(command));}
        windowIcons.apply(glfwGetWin32Window(window));
        // GLFW/Windows retain native maximize, resize and monitor/DPI behavior.
        // Recover a window left on a monitor that has been disconnected.
        RECT nativeBounds{};GetWindowRect(glfwGetWin32Window(window),&nativeBounds);
        if(!MonitorFromRect(&nativeBounds,MONITOR_DEFAULTTONULL)){
            if(auto* primary=glfwGetPrimaryMonitor()){int x{},y{},w{},h{};glfwGetMonitorWorkarea(primary,&x,&y,&w,&h);glfwSetWindowPos(window,x,y);}
        }
        int framebufferWidth{},framebufferHeight{};glfwGetFramebufferSize(window,&framebufferWidth,&framebufferHeight);
        framebufferWidth=std::max(1,framebufferWidth);framebufferHeight=std::max(1,framebufferHeight);
        const float monitorScale=fonts.update(window,ui.uiScale(),options.dpi,ui.lightTheme(),ui.nightTheme());nightDisplay.caption(glfwGetWin32Window(window),ui.lightTheme(),ui.nightTheme());
        std::uint64_t focusContext=1469598103934665603ull;
        for(const auto& property:state.properties)focusContext=(focusContext^property.value)*1099511628211ull;
        if(!state.connected||!state.liveView){texture.focusTracker.reset();texture.focus={};}
        else try {texture.update(state.frame,ui.focusPeaking(),ui.peakingThreshold(),ui.starPeaking(),ui.focusTarget(),focusContext);}catch(const std::exception& e){frameError=e.what();}
        if(state.lastCapture.revision!=previewRevision){previewRevision=state.lastCapture.revision;capturedTexture.width=capturedTexture.height=0;}
        if(auto pixels=photos.poll();pixels&&pixels->revision>=state.lastCapture.revision)capturedTexture.uploadPhoto(*pixels);
        controlApi.previewPerformance({{"decoded_frames",texture.decodedFrames},{"processed_frames",texture.processedFrames},{"displayed_revision",texture.revision},{"decode_ms",texture.decodeMs},{"histogram_ms",texture.histogramMs},{"peaking_ms",texture.peakingMs},{"upload_ms",texture.uploadMs}});
        glfwMakeContextCurrent(window);
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();
        // Some Windows drivers report a zero-size client while minimized.
        // Keep the workspace and remote control queue alive without rebuilding
        // a zero-size dock tree or stopping the remote preview.
        auto& io=ImGui::GetIO();
        if(io.DisplaySize.x>0&&io.DisplaySize.y>0)lastClientSize=io.DisplaySize;else io.DisplaySize=lastClientSize;
        if(state.stack.preview&&state.stack.preview->revision!=stackTexture.revision){const auto& p=*state.stack.preview;stackTexture.uploadStack(p);stackTexture.revision=p.revision;}
        ImGui::NewFrame();ui.render(state,{texture.id,texture.width,texture.height,{},&texture.histogram,texture.focus,texture.revision},monitorScale,{capturedTexture.id,capturedTexture.width,capturedTexture.height,testPreviewStatus.empty()?photos.status():testPreviewStatus,&capturedTexture.histogram},{},{stackTexture.id,stackTexture.width,stackTexture.height});
        if(ui.exitRequested())glfwSetWindowShouldClose(window,GLFW_TRUE);
        if(!frameError.empty()){ImGui::Begin("Errore anteprima");ImGui::TextWrapped("%s",frameError.c_str());if(ImGui::Button("Chiudi"))frameError.clear();ImGui::End();}
        ImGui::Render();
        glfwMakeContextCurrent(window);glViewport(0,0,framebufferWidth,framebufferHeight);
        glClearColor(.10f,.12f,.13f,1);glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());nightDisplay.render(framebufferWidth,framebufferHeight,ui.nightTheme());glfwSwapBuffers(window);
        if(control)control->pump([&](const std::string& text){
            const auto remoteRequest=astro::ControlJson::parse(text);
            if(remoteRequest.value("method",std::string{})=="remote_preview") {
                const auto source=remoteRequest.at("params").value("source",std::string("raw_live"));const auto current=mailbox.snapshot();
                if(source=="raw_live"){
                    if(!current.liveView||current.frame.jpeg.empty())throw std::runtime_error("No active Live View frame");
                    const auto& crop=current.frame.previewRegion;
                    auto header=astro::ControlJson{{"format","jpeg"},{"turbo",ui.controlConfig().turboLiveStream},{"revision",current.frame.revision},{"crop",{crop.x,crop.y,crop.width,crop.height}}}.dump()+"\n";
                    header.append(reinterpret_cast<const char*>(current.frame.jpeg.data()),current.frame.jpeg.size());return header;
                }
                if(source!="capture"&&source!="live"&&source!="stack")throw std::invalid_argument("Invalid remote preview source");
                if(source=="stack"&&!mailbox.snapshot().stack.preview)throw std::runtime_error("No preview available");
                auto& t=source=="stack"?stackTexture:source=="live"?texture:capturedTexture;
                if(!t.id||t.width<1||t.height<1||(source=="live"&&!current.liveView))throw std::runtime_error("No preview available");
                if(static_cast<std::uint64_t>(t.width)*t.height>16000000)throw std::runtime_error("Preview exceeds 16 megapixels");
                std::string bytes;
                if(source=="live"){
                    const auto& pixels=t.peaking?t.markedPixels:t.livePixels;
                    bytes.assign(reinterpret_cast<const char*>(pixels.rgba.data()),pixels.rgba.size());
                }else {glfwMakeContextCurrent(window);glBindTexture(GL_TEXTURE_2D,t.id);bytes.resize(std::size_t(t.width)*t.height*4);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,bytes.data());}
                return astro::ControlJson{{"format","rgba"},{"turbo",ui.controlConfig().turboLiveStream},{"width",t.width},{"height",t.height},{"revision",source=="stack"?stackTexture.revision:source=="live"?t.revision:previewRevision}}.dump()+"\n"+bytes;
            }
            auto result=controlApi.call(astro::ControlJson::parse(text),[&](const astro::ControlJson& request){
                const auto p=request.value("params",astro::ControlJson::object());
                const auto source=p.value("source",std::string("panel"));const auto stateNow=mailbox.snapshot();
                if(request.at("method")=="analysis"){
                    if(source!="live"&&source!="capture")throw std::invalid_argument("Analysis source must be live or capture");
                    if(source=="stack"&&!mailbox.snapshot().stack.preview)throw std::runtime_error("No preview available");
                auto& t=source=="stack"?stackTexture:source=="live"?texture:capturedTexture;auto r=astro::previewTelemetry(t.histogram,t.focus);r["ok"]=true;r["revision"]=source=="stack"?stackTexture.revision:source=="live"?texture.revision:previewRevision;r["available"]=t.width>0&&(source!="live"||stateNow.liveView);return r;
                }
                if(source=="raw_live"){
                    if(!stateNow.liveView||stateNow.frame.jpeg.empty())throw std::runtime_error("No active Live View frame");
                    return astro::ControlJson{{"ok",true},{"mime_type","image/jpeg"},{"revision",stateNow.frame.revision},{"data",astro::base64Image(stateNow.frame.jpeg)}};
                }
                if(source=="live"||source=="capture"||source=="stack"){
                    if(source=="stack"&&!mailbox.snapshot().stack.preview)throw std::runtime_error("No preview available");
                auto& t=source=="stack"?stackTexture:source=="live"?texture:capturedTexture;
                    if(!t.id||t.width<1||t.height<1||(source=="live"&&!stateNow.liveView))throw std::runtime_error("No preview available");
                    if(static_cast<std::uint64_t>(t.width)*t.height>16000000)throw std::runtime_error("Preview exceeds 16 megapixels");
                    glfwMakeContextCurrent(window);glBindTexture(GL_TEXTURE_2D,t.id);std::vector<std::uint8_t> pixels(static_cast<std::size_t>(t.width)*t.height*4);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                    auto r=astro::pngImage(std::move(pixels),t.width,t.height);r["revision"]=source=="stack"?stackTexture.revision:source=="live"?texture.revision:previewRevision;return r;
                }
                if(source!="panel")throw std::invalid_argument("Unknown image source");
                const auto panel=p.value("panel",std::string("LiveView"));auto* w=ImGui::FindWindowByName(("###"+panel).c_str());
                if(!w||!w->Active||w->Hidden||!w->Viewport||!w->Viewport->PlatformHandle)throw std::runtime_error("Panel is not visible");
                auto* vp=w->Viewport;auto* native=static_cast<GLFWwindow*>(vp->PlatformHandle);glfwMakeContextCurrent(native);int width{},height{};glfwGetFramebufferSize(native,&width,&height);
                if(width<1||height<1||static_cast<std::uint64_t>(width)*height>16000000)throw std::runtime_error("Panel is minimized or too large");
                glViewport(0,0,width,height);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(vp->DrawData);nightDisplay.render(width,height,ui.nightTheme());
                const float sx=width/vp->Size.x,sy=height/vp->Size.y;
                const int x=std::clamp(static_cast<int>((w->Pos.x-vp->Pos.x)*sx),0,width-1);
                const int top=std::clamp(static_cast<int>((w->Pos.y-vp->Pos.y)*sy),0,height-1);
                const int cropWidth=std::clamp(static_cast<int>(w->Size.x*sx),1,width-x);
                const int cropHeight=std::clamp(static_cast<int>(w->Size.y*sy),1,height-top);
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(cropWidth)*cropHeight*4);
                glReadPixels(x,height-top-cropHeight,cropWidth,cropHeight,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                auto r=astro::pngImage(std::move(pixels),cropWidth,cropHeight,true);r["panel"]=panel;r["live_revision"]=texture.revision;r["capture_revision"]=previewRevision;return r;
            });return result.dump();
        });
        const auto windowAction=ui.takeWindowAction();
        if(mcpServer&&mcpServer->finished()&&mcpServer->endpoint()=="stdio")glfwSetWindowShouldClose(window,GLFW_TRUE);
        if(windowAction)ShowWindow(glfwGetWin32Window(window),windowAction==1?SW_MINIMIZE:SW_RESTORE);
        if(options.smoke&&++frames>=30){
            std::ofstream report(folder/"desktop-smoke.txt");
            const auto* vp=ImGui::GetMainViewport();
            const ImRect work(vp->Pos,{vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y});
            const char* names[]{"###Connection","###Camera","###LiveView","###Sequence","###Output","###Log"};
            int docked=0,visible=0;std::vector<ImRect> bounds;
            for(const auto* name:names){
                const auto* panel=ImGui::FindWindowByName(name);
                if(!panel||!panel->DockId)throw std::runtime_error("Panel missing from the workspace");
                ++docked;if(panel->Hidden)continue;++visible;
                if(!work.Contains(panel->Rect()))throw std::runtime_error("Panel outside container");
                for(const auto& other:bounds)if(panel->Rect().Overlaps(other))throw std::runtime_error("Overlapping visible workspace panels");
                bounds.push_back(panel->Rect());
                report<<name<<" dock="<<panel->DockId<<" pos="<<panel->Pos.x<<","<<panel->Pos.y<<" size="<<panel->Size.x<<","<<panel->Size.y<<"\n";
            }
            if(ImGui::GetPlatformIO().Viewports.Size!=1)throw std::runtime_error("Unexpected detached native viewport");
            const auto hwnd=glfwGetWin32Window(window);const auto exStyle=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
            if((exStyle&WS_EX_TOOLWINDOW)||GetWindow(hwnd,GW_OWNER))throw std::runtime_error("Main window is not taskbar eligible");
            report<<"native_windows=1 docked_panels="<<docked<<" visible_panels="<<visible<<" taskbar_entries=1\n";
            glfwMakeContextCurrent(window);glViewport(0,0,framebufferWidth,framebufferHeight);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            nightDisplay.render(framebufferWidth,framebufferHeight,ui.nightTheme());screenshot(folder/"workspace-smoke.png",framebufferWidth,framebufferHeight,hwnd);
            glfwSetWindowShouldClose(window,GLFW_TRUE);
        }
        glfwMakeContextCurrent(window);
        if(!options.smoke&&(ImGui::GetIO().WantSaveIniSettings||savedAutomatic!=ui.automaticLayout())){try{saveLayout(layoutPath,ui.automaticLayout());savedAutomatic=ui.automaticLayout();}catch(const std::exception& e){frameError=e.what();}}
        // The idle budget includes image processing/rendering time. Adding a
        // fixed sleep after a costly peaking frame needlessly reduces Live View FPS.
        const double idle=0.01-std::chrono::duration<double>(std::chrono::steady_clock::now()-uiFrameStarted).count();
        if(idle>0)glfwWaitEventsTimeout(idle);
    }
    if(!options.smoke)saveLayout(layoutPath,ui.automaticLayout());
    return 0;
}
}
int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR command,int) {
    if(std::string(command)=="--mcp-init"){try{astro::mcp::initializeSettings();return 0;}catch(...){return 1;}}
    auto hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(hr)){MessageBoxA(nullptr,"Inizializzazione COM fallita","AstroCanon",MB_ICONERROR);return 1;}
    int result=1;RunOptions options;
    try {
        std::istringstream args(command);std::string arg;
        while(args>>arg) {
            if(arg=="--smoke-test")options.smoke=true;
            else if(arg=="--mcp-stdio"||arg=="--mcp-http"){if(!options.mcpTransport.empty())throw std::invalid_argument("Choose only one MCP transport");options.mcpTransport=arg=="--mcp-stdio"?"stdio":"http";}
            else if(arg=="--connect")options.connect=true;
            else if(arg=="--live-view"){options.connect=true;options.startLive=true;}
            else if(arg.starts_with("--remote-server=")){options.remotePort=std::stoi(arg.substr(16));if(options.remotePort<1024||options.remotePort>65535)throw std::invalid_argument("Remote port must be 1024..65535");}
            else if(arg.starts_with("--pairing-test-file="))options.pairingTestFile=astro::pathFromUtf8(arg.substr(20));
            else if(arg=="--smoke-stack")options.smokeStack=true;
            else if(arg=="--smoke-light")options.light=true;
            else if(arg=="--smoke-night")options.night=true;
            else if(arg.starts_with("--smoke-language="))options.language=arg.substr(17);
            else if(arg.starts_with("--smoke-preview="))options.previewFile=astro::pathFromUtf8(arg.substr(16));
            else if(arg.starts_with("--smoke-dpi="))options.dpi=std::stof(arg.substr(12));
            else if(arg.starts_with("--smoke-width="))options.width=std::stoi(arg.substr(14));
            else if(arg.starts_with("--smoke-height="))options.height=std::stoi(arg.substr(15));
            else throw std::invalid_argument("Argomento non riconosciuto: "+arg);
        }
        if(!std::isfinite(options.dpi)||options.dpi<0||options.dpi>4||options.width<0||options.height<0||options.width>7680||options.height>4320)throw std::invalid_argument("Parametri smoke non validi");
        if(!options.smoke&&(options.smokeStack||options.dpi||options.width||options.height||options.light||options.night||!options.previewFile.empty()||!options.language.empty()))throw std::invalid_argument("Le opzioni di test richiedono --smoke-test");
        if(!options.language.empty()&&options.language!="it"&&options.language!="en")throw std::invalid_argument("Unsupported smoke language");
        if(options.smoke&&options.connect)throw std::invalid_argument("Lo smoke test non apre sessioni sulla fotocamera");
        result=run(options);
    }catch(const std::exception& e){if(options.smoke){std::ofstream("smoke-error.log")<<e.what();}else if(options.mcpTransport=="stdio"){DWORD written{};const auto message=std::string(e.what())+"\n";WriteFile(GetStdHandle(STD_ERROR_HANDLE),message.data(),static_cast<DWORD>(message.size()),&written,nullptr);}else MessageBoxA(nullptr,e.what(),"AstroCanon - Errore",MB_ICONERROR);}
    CoUninitialize();return result;
}
