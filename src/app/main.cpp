#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <astro/ConfigManager.hpp>
#include <astro/Infrastructure.hpp>
#include <astro/MainWindow.hpp>
#include "PhotoPreview.hpp"
#include "FocusPeaking.hpp"
#include "LiveViewPreview.hpp"
#include "ImageAnalysis.hpp"
#include "StarAnalysis.hpp"
#include "TaskbarGroup.hpp"
#include "ControlQueue.hpp"
#include "ControlApi.hpp"
#include "ControlImage.hpp"
#include "McpTransport.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <map>
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
    GLuint id{};int width{},height{};std::uint64_t revision{};
    astro::PreviewPixels livePixels;bool peaking{},starPeaking{};float threshold{};
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
        if(changed){livePixels=astro::prepareLiveViewPreview(astro::decodeImage(frame.jpeg),frame);histogram=astro::imageHistogram(livePixels);}
        const auto display=enabled?astro::focusPeaking(livePixels,level,stars):livePixels;
        focus=focusTracker.update(astro::measureFocus(livePixels,target,enabled?&display:nullptr),frame.revision,context);
        focusRevision=target.revision;lastContext=context;upload(display);
        revision=frame.revision;peaking=enabled;threshold=level;starPeaking=stars;
    }
    void uploadPhoto(const astro::PreviewPixels& image){histogram=astro::imageHistogram(image);upload(image);}
    void upload(const astro::PreviewPixels& image) {
        const auto w=image.width,h=image.height;const auto& pixels=image.rgba;
        if(!id)glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(w),static_cast<GLsizei>(h),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());width=static_cast<int>(w);height=static_cast<int>(h);
    }
};
void settleDesktopPanels(float userScale) {
    if(ImGui::GetIO().MouseDown[0]||GImGui->MovingWindow)return;
    const char* names[]{"AstroCanon###Toolbar","Connessione###Connection","Fotocamera###Camera","Immagine###LiveView","Sequenze###Sequence","File e sessione###Output","Registro###Log"};
    const auto& monitors=ImGui::GetPlatformIO().Monitors;
    std::map<int,std::vector<std::pair<int,ImGuiWindow*>>> groups;
    for(int i=0;i<7;++i)if(auto* panel=ImGui::FindWindowByName(names[i]);panel&&panel->Active&&!panel->Hidden&&!panel->DockIsActive) {
        int monitor=-1;float bestArea=-1,bestDistance=std::numeric_limits<float>::max();
        const auto bounds=panel->Rect();const auto center=bounds.GetCenter();
        for(int m=0;m<monitors.Size;++m){const auto& screen=monitors[m];const ImRect work(screen.WorkPos,{screen.WorkPos.x+screen.WorkSize.x,screen.WorkPos.y+screen.WorkSize.y});
            auto overlap=bounds;overlap.ClipWith(work);const float area=std::max(0.f,overlap.GetWidth())*std::max(0.f,overlap.GetHeight());
            const auto c=work.GetCenter();const float distance=(center.x-c.x)*(center.x-c.x)+(center.y-c.y)*(center.y-c.y);
            if(area>bestArea||(area==bestArea&&distance<bestDistance)){monitor=m;bestArea=area;bestDistance=distance;}}
        if(monitor>=0&&monitor<monitors.Size)groups[monitor].push_back({i,panel});
    }
    for(const auto& [index,panels]:groups) {
        const auto& m=monitors[index];const ImRect work(m.WorkPos,{m.WorkPos.x+m.WorkSize.x,m.WorkPos.y+m.WorkSize.y});bool invalid=false;
        for(std::size_t i=0;i<panels.size();++i){const auto* a=panels[i].second;if(!work.Contains(a->Rect()))invalid=true;for(std::size_t j=0;j<i;++j)if(a->Rect().Overlaps(panels[j].second->Rect()))invalid=true;}
        if(!invalid)continue;
        const auto tiles=astro::ui::desktopLayout({m.WorkPos.x,m.WorkPos.y,m.WorkSize.x,m.WorkSize.y},13*userScale*m.DpiScale);
        for(const auto& [slot,panel]:panels){const auto& t=tiles[slot];ImGui::SetWindowPos(panel,{t.x,t.y});ImGui::SetWindowSize(panel,{t.width,t.height});}
    }
}
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
    bool smoke{},light{},connect{};std::filesystem::path previewFile;
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
    float update(GLFWwindow* window,float userScale,float overrideDpi,bool light) {
        float x=1,y=1;glfwGetWindowContentScale(window,&x,&y);
        const float monitor=overrideDpi>0?overrideDpi:std::max(x,y);
        std::map<int,ImFont*> wanted;wanted[key(monitor)]=nullptr;
        int count{};auto** monitors=glfwGetMonitors(&count);
        for(int i=0;i<count;++i){float mx{},my{};glfwGetMonitorContentScale(monitors[i],&mx,&my);wanted[key(overrideDpi>0?overrideDpi:std::max(mx,my))]=nullptr;}
        bool rebuild=userScale!=scale_||overrideDpi!=override_||wanted.size()!=fonts_.size();
        for(const auto& [dpi,font]:wanted){(void)font;if(!fonts_.contains(dpi))rebuild=true;}
        scale_=userScale;override_=overrideDpi;light_=light;
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
        astro::ui::applyTheme(light_);ImGui::GetStyle().ScaleAllSizes(scale_*entry->first/96.f);
        ImGui::SetCurrentFont(entry->second);
    }
    inline static DpiFonts* current_{};
    std::vector<char> bytes_;std::map<int,ImFont*> fonts_;float scale_{},override_{};bool light_{};
};
void saveLayout(const std::filesystem::path& path) {
    std::size_t size{};const char* settings=ImGui::SaveIniSettingsToMemory(&size);
    auto temporary=path;temporary+=".tmp";
    {std::ofstream out(temporary,std::ios::binary);out.write(settings,static_cast<std::streamsize>(size));out.flush();if(!out)throw std::runtime_error("Salvataggio layout ImGui fallito");}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Sostituzione layout ImGui fallita");
    ImGui::GetIO().WantSaveIniSettings=false;
}
int run(const RunOptions& options) {
    std::unique_ptr<astro::mcp::InstanceGuard> instance;if(!options.smoke)instance=std::make_unique<astro::mcp::InstanceGuard>();
    std::unique_ptr<astro::ControlQueue> control;if(!options.smoke)control=std::make_unique<astro::ControlQueue>();
    wchar_t module[32768]{};auto length=GetModuleFileNameW(nullptr,module,static_cast<DWORD>(std::size(module)));if(!length||length==std::size(module))throw std::runtime_error("Percorso eseguibile non disponibile");
    auto folder=std::filesystem::path(module).parent_path();auto configPath=folder/"user.json";
    auto config=astro::ConfigManager::load(std::filesystem::exists(configPath)?configPath:folder/"defaults.json");
    if(options.light)config.lightTheme=true;
    if(!options.language.empty())config.language=options.language;
    if(config.outputDirectory.is_relative())config.outputDirectory=folder/config.outputDirectory;
    if(!glfwInit())throw std::runtime_error("Inizializzazione GLFW fallita");
    struct GlfwGuard{~GlfwGuard(){glfwTerminate();}} glfwGuard;
    WindowIcons windowIcons;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR,GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED,GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    float primaryX=1,primaryY=1;auto* monitor=glfwGetPrimaryMonitor();
    int areaX{},areaY{},areaWidth=config.windowWidth,areaHeight=config.windowHeight;
    if(monitor){glfwGetMonitorContentScale(monitor,&primaryX,&primaryY);glfwGetMonitorWorkarea(monitor,&areaX,&areaY,&areaWidth,&areaHeight);}
    GLFWwindow* window=glfwCreateWindow(1,1,"AstroCanon Controller",nullptr,nullptr);if(!window)throw std::runtime_error("Creazione finestra/OpenGL fallita");
    struct WindowGuard{GLFWwindow* w;~WindowGuard(){glfwDestroyWindow(w);}} windowGuard{window};
    windowIcons.apply(glfwGetWin32Window(window));
    glfwSetWindowPos(window,areaX,areaY);
    glfwMakeContextCurrent(window);glfwSwapInterval(options.smoke?0:1);
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    struct ImGuiGuard{bool renderer{},platform{};~ImGuiGuard(){if(renderer)ImGui_ImplOpenGL3_Shutdown();if(platform)ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();}} imguiGuard;
    ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_ViewportsEnable;
    ImGui::GetIO().ConfigViewportsNoAutoMerge=true;
    ImGui::GetIO().ConfigViewportsNoDecoration=true;
    ImGui::GetIO().ConfigViewportsNoTaskBarIcon=true;
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly=true;
    const auto layoutPath=folder/"desktop-layout.ini";std::string layout;
    if(!options.smoke){std::ifstream layoutFile(layoutPath,std::ios::binary);layout={std::istreambuf_iterator<char>(layoutFile),{}};}
    if(!layout.empty())ImGui::LoadIniSettingsFromMemory(layout.data(),layout.size());
    imguiGuard.platform=ImGui_ImplGlfw_InitForOpenGL(window,true);
    if(!imguiGuard.platform)throw std::runtime_error("Inizializzazione piattaforma ImGui fallita");
    imguiGuard.renderer=ImGui_ImplOpenGL3_Init("#version 130");
    if(!imguiGuard.renderer)throw std::runtime_error("Inizializzazione rendering ImGui fallita");
    if(options.smoke)ImGui::GetPlatformIO().Platform_ShowWindow=[](ImGuiViewport*){};
    TaskbarGroup taskbarGroup;
    std::map<HWND,std::pair<int,int>> roundedSizes;
    DpiFonts fonts(folder/"Cousine-Regular.ttf");
    Texture texture,capturedTexture;astro::PhotoPreview photos;astro::Mailbox mailbox;
    astro::CameraWorker worker(mailbox,config,configPath,[](const astro::LiveFrame& frame,double snr,const std::vector<astro::StarSample>& tracked,double radius){return astro::measureStars(astro::prepareLiveViewPreview(astro::decodeImage(frame.jpeg),frame),snr,tracked,radius);},{},[&photos](const astro::CapturedPhoto& photo){photos.request(photo);});
    astro::ui::MainWindow ui(mailbox,config,!layout.empty());
    astro::ControlApi controlApi(mailbox,ui);
    std::unique_ptr<astro::mcp::Server> mcpServer;if(!options.smoke)mcpServer=std::make_unique<astro::mcp::Server>(*control,options.mcpTransport.empty()?astro::mcp::configuredTransport():options.mcpTransport);
    if(options.connect) {astro::Command command;command.action=astro::Action::Connect;mailbox.send(std::move(command));}
    int frames=0;std::string frameError;std::uint64_t previewRevision{};std::string testPreviewStatus;
    if(!options.previewFile.empty()){auto pixels=astro::decodePreview(options.previewFile);capturedTexture.uploadPhoto(pixels);testPreviewStatus="File di collaudo | "+pixels.description;}
    std::string monitorSignature;
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();const auto state=mailbox.snapshot();
        // Refresh after hot-plug, resolution/taskbar or DPI changes. The primary monitor
        // need not start at (0,0), and other monitors may have negative coordinates.
        std::ostringstream topology;int monitorCount{};auto** monitors=glfwGetMonitors(&monitorCount);
        for(int i=0;i<monitorCount;++i){int x{},y{},w{},h{};float sx{},sy{};glfwGetMonitorWorkarea(monitors[i],&x,&y,&w,&h);glfwGetMonitorContentScale(monitors[i],&sx,&sy);topology<<monitors[i]<<':'<<x<<','<<y<<','<<w<<','<<h<<','<<sx<<','<<sy<<';';}
        topology<<glfwGetPrimaryMonitor();
        if(topology.str()!=monitorSignature){monitorSignature=topology.str();monitor=glfwGetPrimaryMonitor();if(monitor)glfwGetMonitorWorkarea(monitor,&areaX,&areaY,&areaWidth,&areaHeight);glfwSetWindowPos(window,areaX,areaY);ui.resetLayout();}
        int framebufferWidth{},framebufferHeight{};glfwGetFramebufferSize(window,&framebufferWidth,&framebufferHeight);
        if(framebufferWidth==0||framebufferHeight==0){glfwWaitEventsTimeout(0.1);continue;}
        const float monitorScale=fonts.update(window,ui.uiScale(),options.dpi,ui.lightTheme());
        std::uint64_t focusContext=1469598103934665603ull;
        for(const auto& property:state.properties)focusContext=(focusContext^property.value)*1099511628211ull;
        if(!state.connected||!state.liveView){texture.focusTracker.reset();texture.focus={};}
        else try {texture.update(state.frame,ui.focusPeaking(),ui.peakingThreshold(),ui.starPeaking(),ui.focusTarget(),focusContext);}catch(const std::exception& e){frameError=e.what();}
        if(state.lastCapture.revision!=previewRevision){previewRevision=state.lastCapture.revision;capturedTexture.width=capturedTexture.height=0;}
        if(auto pixels=photos.poll();pixels&&pixels->revision>=state.lastCapture.revision)capturedTexture.uploadPhoto(*pixels);
        glfwMakeContextCurrent(window);
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ui.render(state,{texture.id,texture.width,texture.height,{},&texture.histogram,texture.focus},monitorScale,{capturedTexture.id,capturedTexture.width,capturedTexture.height,testPreviewStatus.empty()?photos.status():testPreviewStatus,&capturedTexture.histogram},{static_cast<float>(areaX),static_cast<float>(areaY),static_cast<float>(options.width?options.width:areaWidth),static_cast<float>(options.height?options.height:areaHeight)});
        if(ui.exitRequested())glfwSetWindowShouldClose(window,GLFW_TRUE);
        if(!options.smoke&&(!taskbarGroup.owner()||!IsIconic(taskbarGroup.owner())))settleDesktopPanels(ui.uiScale());
        if(!frameError.empty()){ImGui::Begin("Errore anteprima");ImGui::TextWrapped("%s",frameError.c_str());if(ImGui::Button("Chiudi"))frameError.clear();ImGui::End();}
        ImGui::Render();ImGui::UpdatePlatformWindows();
        for(auto* vp:ImGui::GetPlatformIO().Viewports) {
            if(vp==ImGui::GetMainViewport()||!vp->PlatformHandleRaw)continue;
            auto hwnd=static_cast<HWND>(vp->PlatformHandleRaw);RECT rect{};GetWindowRect(hwnd,&rect);
            taskbarGroup.apply(vp);
            windowIcons.apply(hwnd);
            auto size=std::make_pair(static_cast<int>(rect.right-rect.left),static_cast<int>(rect.bottom-rect.top));
            if(roundedSizes[hwnd]!=size){
                const int radius=std::max(8,static_cast<int>(ImGui::GetStyle().WindowRounding*2));
                auto region=CreateRoundRectRgn(0,0,size.first+1,size.second+1,radius,radius);
                if(!region||!SetWindowRgn(hwnd,region,TRUE)){if(region)DeleteObject(region);throw std::runtime_error("Arrotondamento finestra nativa fallito");}
                roundedSizes[hwnd]=size;
            }
        }
        ImGui::RenderPlatformWindowsDefault();
        if(control)control->pump([&](const std::string& text){
            auto result=controlApi.call(astro::ControlJson::parse(text),[&](const astro::ControlJson& request){
                const auto p=request.value("params",astro::ControlJson::object());
                const auto source=p.value("source",std::string("panel"));const auto stateNow=mailbox.snapshot();
                if(request.at("method")=="analysis"){
                    if(source!="live"&&source!="capture")throw std::invalid_argument("Analysis source must be live or capture");
                    const auto& t=source=="live"?texture:capturedTexture;auto r=astro::previewTelemetry(t.histogram,t.focus);r["ok"]=true;r["revision"]=source=="live"?texture.revision:previewRevision;r["available"]=t.width>0&&(source!="live"||stateNow.liveView);return r;
                }
                if(source=="raw_live"){
                    if(!stateNow.liveView||stateNow.frame.jpeg.empty())throw std::runtime_error("No active Live View frame");
                    return astro::ControlJson{{"ok",true},{"mime_type","image/jpeg"},{"revision",stateNow.frame.revision},{"data",astro::base64Image(stateNow.frame.jpeg)}};
                }
                if(source=="live"||source=="capture"){
                    auto& t=source=="live"?texture:capturedTexture;
                    if(!t.id||t.width<1||t.height<1||(source=="live"&&!stateNow.liveView))throw std::runtime_error("No preview available");
                    if(static_cast<std::uint64_t>(t.width)*t.height>16000000)throw std::runtime_error("Preview exceeds 16 megapixels");
                    glfwMakeContextCurrent(window);glBindTexture(GL_TEXTURE_2D,t.id);std::vector<std::uint8_t> pixels(static_cast<std::size_t>(t.width)*t.height*4);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                    auto r=astro::pngImage(std::move(pixels),t.width,t.height);r["revision"]=source=="live"?texture.revision:previewRevision;return r;
                }
                if(source!="panel")throw std::invalid_argument("Unknown image source");
                const auto panel=p.value("panel",std::string("LiveView"));auto* w=ImGui::FindWindowByName(("###"+panel).c_str());
                if(!w||!w->Active||w->Hidden||!w->Viewport||!w->Viewport->PlatformHandle)throw std::runtime_error("Panel is not visible");
                auto* vp=w->Viewport;auto* native=static_cast<GLFWwindow*>(vp->PlatformHandle);glfwMakeContextCurrent(native);int width{},height{};glfwGetFramebufferSize(native,&width,&height);
                if(width<1||height<1||static_cast<std::uint64_t>(width)*height>16000000)throw std::runtime_error("Panel is minimized or too large");
                glViewport(0,0,width,height);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(vp->DrawData);
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width)*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
                auto r=astro::pngImage(std::move(pixels),width,height,true,vp->PlatformHandleRaw);r["panel"]=panel;r["live_revision"]=texture.revision;r["capture_revision"]=previewRevision;return r;
            });return result.dump();
        });
        const auto windowAction=ui.takeWindowAction();
        if(mcpServer&&mcpServer->finished())glfwSetWindowShouldClose(window,GLFW_TRUE);
        if(windowAction&&taskbarGroup.owner())ShowWindow(taskbarGroup.owner(),windowAction==1?SW_MINIMIZE:SW_RESTORE);
        if(options.smoke&&++frames>=30){
            std::ofstream report(folder/"desktop-smoke.txt");int count=0,taskbarEntries=0;
            std::vector<ImRect> panelRects;
            const ImRect work({static_cast<float>(areaX),static_cast<float>(areaY)},{static_cast<float>(areaX+(options.width?options.width:areaWidth)),static_cast<float>(areaY+(options.height?options.height:areaHeight))});
            for(auto* vp:ImGui::GetPlatformIO().Viewports){
                if(vp==ImGui::GetMainViewport()||!vp->PlatformHandle)continue;
                auto* native=static_cast<GLFWwindow*>(vp->PlatformHandle);glfwMakeContextCurrent(native);
                int w{},h{};glfwGetFramebufferSize(native,&w,&h);
                // Render again into the back buffer for an exact per-window artifact.
                glViewport(0,0,w,h);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);ImGui_ImplOpenGL3_RenderDrawData(vp->DrawData);
                screenshot(folder/("desktop-panel-"+std::to_string(++count)+".png"),w,h,static_cast<HWND>(vp->PlatformHandleRaw));
                auto region=CreateRectRgn(0,0,0,0);const auto kind=GetWindowRgn(static_cast<HWND>(vp->PlatformHandleRaw),region);
                const bool rounded=kind!=ERROR&&!PtInRegion(region,0,0)&&PtInRegion(region,w/2,h/2);DeleteObject(region);
                if(!rounded)throw std::runtime_error("Regione nativa non arrotondata");
                const auto hwnd=static_cast<HWND>(vp->PlatformHandleRaw);
                const auto exStyle=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
                const bool entry=(exStyle&WS_EX_APPWINDOW)&&!(exStyle&WS_EX_TOOLWINDOW)&&!GetWindow(hwnd,GW_OWNER);
                if(entry)++taskbarEntries;
                else if(GetWindow(hwnd,GW_OWNER)!=taskbarGroup.owner()||!(exStyle&WS_EX_TOOLWINDOW)||(exStyle&WS_EX_APPWINDOW))throw std::runtime_error("Panel is not an owned taskbar-free tool window");
                if(!SendMessageW(hwnd,WM_GETICON,ICON_SMALL,0)||!SendMessageW(hwnd,WM_GETICON,ICON_BIG,0))throw std::runtime_error("Missing DPI-aware window icons");
                const ImRect bounds(vp->Pos,{vp->Pos.x+vp->Size.x,vp->Pos.y+vp->Size.y});
                if(!work.Contains(bounds))throw std::runtime_error("Panel outside primary work area");
                for(const auto& other:panelRects)if(bounds.Overlaps(other))throw std::runtime_error("Overlapping desktop panels");
                panelRects.push_back(bounds);
                report<<"rounded="<<rounded<<" "<<vp->ID<<" hwnd="<<vp->PlatformHandleRaw<<" pos="<<vp->Pos.x<<","<<vp->Pos.y<<" size="<<w<<","<<h<<" decorated="<<glfwGetWindowAttrib(native,GLFW_DECORATED)<<"\n";
            }
            report<<"hidden_host_visible="<<glfwGetWindowAttrib(window,GLFW_VISIBLE)<<" panels="<<count<<"\n";
            report<<"taskbar_entries="<<taskbarEntries<<"\n";
            if(taskbarEntries!=1)throw std::runtime_error("Expected exactly one Windows taskbar entry");
            if(count<7)throw std::runtime_error("Mancano finestre native nel collaudo desktop");
            glfwSetWindowShouldClose(window,GLFW_TRUE);
        }
        glfwMakeContextCurrent(window);
        if(!options.smoke&&ImGui::GetIO().WantSaveIniSettings){try{saveLayout(layoutPath);}catch(const std::exception& e){frameError=e.what();}}
        glfwWaitEventsTimeout(0.01);
    }
    if(!options.smoke)saveLayout(layoutPath);
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
            else if(arg=="--smoke-light")options.light=true;
            else if(arg.starts_with("--smoke-language="))options.language=arg.substr(17);
            else if(arg.starts_with("--smoke-preview="))options.previewFile=astro::pathFromUtf8(arg.substr(16));
            else if(arg.starts_with("--smoke-dpi="))options.dpi=std::stof(arg.substr(12));
            else if(arg.starts_with("--smoke-width="))options.width=std::stoi(arg.substr(14));
            else if(arg.starts_with("--smoke-height="))options.height=std::stoi(arg.substr(15));
            else throw std::invalid_argument("Argomento non riconosciuto: "+arg);
        }
        if(!std::isfinite(options.dpi)||options.dpi<0||options.dpi>4||options.width<0||options.height<0||options.width>7680||options.height>4320)throw std::invalid_argument("Parametri smoke non validi");
        if(!options.smoke&&(options.dpi||options.width||options.height||options.light||!options.previewFile.empty()||!options.language.empty()))throw std::invalid_argument("Le opzioni di test richiedono --smoke-test");
        if(!options.language.empty()&&options.language!="it"&&options.language!="en")throw std::invalid_argument("Unsupported smoke language");
        if(options.smoke&&options.connect)throw std::invalid_argument("Lo smoke test non apre sessioni sulla fotocamera");
        result=run(options);
    }catch(const std::exception& e){if(options.smoke){std::ofstream("smoke-error.log")<<e.what();}else if(options.mcpTransport=="stdio"){DWORD written{};const auto message=std::string(e.what())+"\n";WriteFile(GetStdHandle(STD_ERROR_HANDLE),message.data(),static_cast<DWORD>(message.size()),&written,nullptr);}else MessageBoxA(nullptr,e.what(),"AstroCanon - Errore",MB_ICONERROR);}
    CoUninitialize();return result;
}
