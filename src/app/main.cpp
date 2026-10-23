#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <astro/ConfigManager.hpp>
#include <astro/Infrastructure.hpp>
#include <astro/MainWindow.hpp>
#include "PhotoPreview.hpp"
#include "FocusPeaking.hpp"
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
void check(HRESULT result) {if(FAILED(result))throw std::runtime_error("Decodifica JPEG WIC fallita: "+std::to_string(result));}
struct Texture {
    GLuint id{};int width{},height{};std::uint64_t revision{};
    astro::PreviewPixels livePixels;bool peaking{};float threshold{};
    ~Texture(){if(id)glDeleteTextures(1,&id);}
    void update(const astro::LiveFrame& frame,bool enabled,float level) {
        if(frame.jpeg.empty())return;
        const bool changed=frame.revision!=revision;
        if(!changed&&peaking==enabled&&threshold==level)return;
        if(changed)livePixels=astro::decodeImage(frame.jpeg);
        upload(enabled?astro::focusPeaking(livePixels,level):livePixels);
        revision=frame.revision;peaking=enabled;threshold=level;
    }
    void upload(const astro::PreviewPixels& image) {
        const auto w=image.width,h=image.height;const auto& pixels=image.rgba;
        if(!id)glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,static_cast<GLsizei>(w),static_cast<GLsizei>(h),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());width=static_cast<int>(w);height=static_cast<int>(h);
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
    bool smoke{},light{},connect{};std::filesystem::path previewFile;
    std::string language;
    float dpi{};
    int width{},height{};
};
class DpiFonts {
public:
    explicit DpiFonts(const std::filesystem::path& path) {
        std::ifstream file(path,std::ios::binary);bytes_={std::istreambuf_iterator<char>(file),{}};
        if(bytes_.empty())throw std::runtime_error("Font Cousine-Regular.ttf assente");
    }
    float update(GLFWwindow* window,float userScale,float overrideDpi,bool light) {
        float x=1,y=1;glfwGetWindowContentScale(window,&x,&y);
        const float monitor=overrideDpi>0?overrideDpi:std::max(x,y);
        const float scale=monitor*userScale;
        if(std::abs(scale-scale_)<0.001f&&light==light_)return monitor;
        // Rebuild the atlas before NewFrame: never magnify an existing bitmap font.
        ImGui_ImplOpenGL3_DestroyFontsTexture();auto& io=ImGui::GetIO();io.Fonts->Clear();
        ImFontConfig font;font.FontDataOwnedByAtlas=false;
        io.FontDefault=io.Fonts->AddFontFromMemoryTTF(bytes_.data(),static_cast<int>(bytes_.size()),std::max(9.0f,std::floor(13*scale)),&font);
        if(!io.FontDefault||!io.Fonts->Build()||!ImGui_ImplOpenGL3_CreateFontsTexture())throw std::runtime_error("Creazione font DPI fallita");
        astro::ui::applyTheme(light);ImGui::GetStyle().ScaleAllSizes(scale);io.FontGlobalScale=1.0f;
        if(monitor_>0&&std::abs(monitor-monitor_)>0.001f)
            ImGui::ScaleWindowsInViewport(static_cast<ImGuiViewportP*>(ImGui::GetMainViewport()),monitor/monitor_);
        scale_=scale;monitor_=monitor;light_=light;return monitor;
    }
private:
    std::vector<char> bytes_;float scale_{},monitor_{};bool light_{};
};
void saveLayout(const std::filesystem::path& path) {
    std::size_t size{};const char* settings=ImGui::SaveIniSettingsToMemory(&size);
    auto temporary=path;temporary+=".tmp";
    {std::ofstream out(temporary,std::ios::binary);out.write(settings,static_cast<std::streamsize>(size));out.flush();if(!out)throw std::runtime_error("Salvataggio layout ImGui fallito");}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Sostituzione layout ImGui fallita");
    ImGui::GetIO().WantSaveIniSettings=false;
}
int run(const RunOptions& options) {
    wchar_t module[32768]{};auto length=GetModuleFileNameW(nullptr,module,static_cast<DWORD>(std::size(module)));if(!length||length==std::size(module))throw std::runtime_error("Percorso eseguibile non disponibile");
    auto folder=std::filesystem::path(module).parent_path();auto configPath=folder/"user.json";
    auto config=astro::ConfigManager::load(std::filesystem::exists(configPath)?configPath:folder/"defaults.json");
    if(options.light)config.lightTheme=true;
    if(!options.language.empty())config.language=options.language;
    if(config.outputDirectory.is_relative())config.outputDirectory=folder/config.outputDirectory;
    if(!glfwInit())throw std::runtime_error("Inizializzazione GLFW fallita");
    struct GlfwGuard{~GlfwGuard(){glfwTerminate();}} glfwGuard;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,0);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR,GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED,GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    float primaryX=1,primaryY=1;auto* monitor=glfwGetPrimaryMonitor();
    int areaX{},areaY{},areaWidth=config.windowWidth,areaHeight=config.windowHeight;
    if(monitor){glfwGetMonitorContentScale(monitor,&primaryX,&primaryY);glfwGetMonitorWorkarea(monitor,&areaX,&areaY,&areaWidth,&areaHeight);}
    GLFWwindow* window=glfwCreateWindow(1,1,"AstroCanon Controller",nullptr,nullptr);if(!window)throw std::runtime_error("Creazione finestra/OpenGL fallita");
    struct WindowGuard{GLFWwindow* w;~WindowGuard(){glfwDestroyWindow(w);}} windowGuard{window};
    glfwSetWindowPos(window,areaX,areaY);
    glfwMakeContextCurrent(window);glfwSwapInterval(options.smoke?0:1);
    IMGUI_CHECKVERSION();ImGui::CreateContext();
    struct ImGuiGuard{bool renderer{},platform{};~ImGuiGuard(){if(renderer)ImGui_ImplOpenGL3_Shutdown();if(platform)ImGui_ImplGlfw_Shutdown();ImGui::DestroyContext();}} imguiGuard;
    ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard|ImGuiConfigFlags_DockingEnable|ImGuiConfigFlags_ViewportsEnable;
    ImGui::GetIO().ConfigViewportsNoAutoMerge=true;
    ImGui::GetIO().ConfigViewportsNoDecoration=true;
    ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly=true;
    const auto layoutPath=folder/"desktop-layout.ini";std::string layout;
    if(!options.smoke){std::ifstream layoutFile(layoutPath,std::ios::binary);layout={std::istreambuf_iterator<char>(layoutFile),{}};}
    if(!layout.empty())ImGui::LoadIniSettingsFromMemory(layout.data(),layout.size());
    imguiGuard.platform=ImGui_ImplGlfw_InitForOpenGL(window,true);
    if(!imguiGuard.platform)throw std::runtime_error("Inizializzazione piattaforma ImGui fallita");
    imguiGuard.renderer=ImGui_ImplOpenGL3_Init("#version 130");
    if(!imguiGuard.renderer)throw std::runtime_error("Inizializzazione rendering ImGui fallita");
    if(options.smoke)ImGui::GetPlatformIO().Platform_ShowWindow=[](ImGuiViewport*){};
    std::map<HWND,std::pair<int,int>> roundedSizes;
    DpiFonts fonts(folder/"Cousine-Regular.ttf");
    Texture texture,capturedTexture;astro::PhotoPreview photos;astro::Mailbox mailbox;astro::CameraWorker worker(mailbox,config,configPath);astro::ui::MainWindow ui(mailbox,config,!layout.empty());
    if(options.connect) {astro::Command command;command.action=astro::Action::Connect;mailbox.send(std::move(command));}
    int frames=0;std::string frameError;std::uint64_t previewRevision{};std::string testPreviewStatus;
    if(!options.previewFile.empty()){auto pixels=astro::decodePreview(options.previewFile);capturedTexture.upload(pixels);testPreviewStatus="File di collaudo | "+pixels.description;}
    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();const auto state=mailbox.snapshot();
        int framebufferWidth{},framebufferHeight{};glfwGetFramebufferSize(window,&framebufferWidth,&framebufferHeight);
        if(framebufferWidth==0||framebufferHeight==0){glfwWaitEventsTimeout(0.1);continue;}
        const float monitorScale=fonts.update(window,ui.uiScale(),options.dpi,ui.lightTheme());
        try {texture.update(state.frame,ui.focusPeaking(),ui.peakingThreshold());}catch(const std::exception& e){frameError=e.what();}
        if(state.lastCapture.revision!=previewRevision){previewRevision=state.lastCapture.revision;capturedTexture.width=capturedTexture.height=0;}
        photos.request(state.lastCapture);if(auto pixels=photos.poll())capturedTexture.upload(*pixels);
        glfwMakeContextCurrent(window);
        ImGui_ImplOpenGL3_NewFrame();ImGui_ImplGlfw_NewFrame();ImGui::NewFrame();ui.render(state,{texture.id,texture.width,texture.height},monitorScale,{capturedTexture.id,capturedTexture.width,capturedTexture.height,testPreviewStatus.empty()?photos.status():testPreviewStatus},{static_cast<float>(areaX),static_cast<float>(areaY),static_cast<float>(options.width?options.width:areaWidth),static_cast<float>(options.height?options.height:areaHeight)});
        if(ui.exitRequested())glfwSetWindowShouldClose(window,GLFW_TRUE);
        if(!frameError.empty()){ImGui::Begin("Errore anteprima");ImGui::TextWrapped("%s",frameError.c_str());if(ImGui::Button("Chiudi"))frameError.clear();ImGui::End();}
        ImGui::Render();ImGui::UpdatePlatformWindows();
        for(auto* vp:ImGui::GetPlatformIO().Viewports) {
            if(vp==ImGui::GetMainViewport()||!vp->PlatformHandleRaw)continue;
            auto hwnd=static_cast<HWND>(vp->PlatformHandleRaw);RECT rect{};GetWindowRect(hwnd,&rect);
            auto size=std::make_pair(static_cast<int>(rect.right-rect.left),static_cast<int>(rect.bottom-rect.top));
            if(roundedSizes[hwnd]!=size){
                const int radius=std::max(8,static_cast<int>(ImGui::GetStyle().WindowRounding*2));
                auto region=CreateRoundRectRgn(0,0,size.first+1,size.second+1,radius,radius);
                if(!region||!SetWindowRgn(hwnd,region,TRUE)){if(region)DeleteObject(region);throw std::runtime_error("Arrotondamento finestra nativa fallito");}
                roundedSizes[hwnd]=size;
            }
        }
        ImGui::RenderPlatformWindowsDefault();
        if(options.smoke&&++frames>=30){
            std::ofstream report(folder/"desktop-smoke.txt");int count=0;
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
                report<<"rounded="<<rounded<<" "<<vp->ID<<" hwnd="<<vp->PlatformHandleRaw<<" pos="<<vp->Pos.x<<","<<vp->Pos.y<<" size="<<w<<","<<h<<" decorated="<<glfwGetWindowAttrib(native,GLFW_DECORATED)<<"\n";
            }
            report<<"hidden_host_visible="<<glfwGetWindowAttrib(window,GLFW_VISIBLE)<<" panels="<<count<<"\n";
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
    auto hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(hr)){MessageBoxA(nullptr,"Inizializzazione COM fallita","AstroCanon",MB_ICONERROR);return 1;}
    int result=1;RunOptions options;
    try {
        std::istringstream args(command);std::string arg;
        while(args>>arg) {
            if(arg=="--smoke-test")options.smoke=true;
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
    }catch(const std::exception& e){if(options.smoke){std::ofstream("smoke-error.log")<<e.what();}else MessageBoxA(nullptr,e.what(),"AstroCanon - Errore",MB_ICONERROR);}
    CoUninitialize();return result;
}
