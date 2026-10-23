#include <astro/MainWindow.hpp>
#include "Widgets.hpp"
#include <imgui_internal.h>
namespace astro::ui {
using namespace widgets;
namespace {
constexpr const char* connectionTitle="Connessione###Connection";
constexpr const char* cameraTitle="Fotocamera###Camera";
constexpr const char* liveTitle="Immagine###LiveView";
constexpr const char* sequenceTitle="Sequenze###Sequence";
constexpr const char* outputTitle="File e sessione###Output";
constexpr const char* logTitle="Registro###Log";
}
void applyTheme(bool light) {
    auto& s=ImGui::GetStyle();s=ImGuiStyle{};ImGui::StyleColorsDark();
    s.WindowPadding={5,5};s.FramePadding={4,2};s.ItemSpacing={5,3};s.ItemInnerSpacing={4,3};s.CellPadding={3,2};
    s.WindowRounding=8;s.ChildRounding=5;s.FrameRounding=4;s.GrabRounding=4;s.TabRounding=4;s.ScrollbarRounding=6;
    s.WindowBorderSize=1;s.ChildBorderSize=1;s.FrameBorderSize=0;s.TabBorderSize=0;s.DockingSeparatorSize=3;
    s.WindowMinSize={160,100};s.ScrollbarSize=13;
    auto* c=s.Colors;
    c[ImGuiCol_Text]={0.87f,0.88f,0.89f,1};c[ImGuiCol_TextDisabled]={0.51f,0.56f,0.59f,1};
    c[ImGuiCol_WindowBg]={0.12f,0.14f,0.15f,1};c[ImGuiCol_ChildBg]={0.10f,0.12f,0.13f,1};
    c[ImGuiCol_PopupBg]={0.10f,0.12f,0.13f,1};c[ImGuiCol_Border]={0.31f,0.35f,0.37f,0.7f};
    c[ImGuiCol_TitleBg]={0.035f,0.04f,0.045f,1};c[ImGuiCol_TitleBgActive]={0.12f,0.23f,0.36f,1};c[ImGuiCol_TitleBgCollapsed]={0.07f,0.08f,0.09f,1};
    c[ImGuiCol_MenuBarBg]={0.12f,0.13f,0.14f,1};
    c[ImGuiCol_FrameBg]={0.13f,0.20f,0.29f,1};c[ImGuiCol_FrameBgHovered]={0.20f,0.31f,0.43f,1};c[ImGuiCol_FrameBgActive]={0.25f,0.40f,0.58f,1};
    c[ImGuiCol_Button]={0.18f,0.31f,0.46f,1};c[ImGuiCol_ButtonHovered]={0.25f,0.43f,0.65f,1};c[ImGuiCol_ButtonActive]={0.23f,0.48f,0.77f,1};
    c[ImGuiCol_Header]={0.15f,0.25f,0.37f,1};c[ImGuiCol_HeaderHovered]={0.22f,0.36f,0.52f,1};c[ImGuiCol_HeaderActive]={0.25f,0.43f,0.65f,1};
    c[ImGuiCol_Tab]={0.09f,0.12f,0.16f,1};c[ImGuiCol_TabSelected]={0.16f,0.28f,0.42f,1};c[ImGuiCol_TabHovered]=c[ImGuiCol_ButtonHovered];
    c[ImGuiCol_CheckMark]={0.35f,0.64f,1,1};c[ImGuiCol_SliderGrab]=c[ImGuiCol_CheckMark];c[ImGuiCol_PlotHistogram]={0.29f,0.55f,0.82f,1};
    c[ImGuiCol_DockingEmptyBg]={0.28f,0.35f,0.38f,1};c[ImGuiCol_DockingPreview]={0.27f,0.50f,0.78f,0.7f};
    if(light)ImGui::StyleColorsLight();
}
MainWindow::MainWindow(Mailbox& m,Config c,bool loaded):mailbox_(m),config_(std::move(c)),resetLayout_(!loaded) {
    auto utf8=config_.outputDirectory.u8string();output_={utf8.begin(),utf8.end()};if(!config_.sequence.empty())selectedRow_=0;
}
void MainWindow::send(Action action,std::uint32_t value) {Command c;c.action=action;c.value=value;mailbox_.send(std::move(c));}
void MainWindow::save() {
    try {config_.outputDirectory=pathFromUtf8(output_);validate(config_);Command c;c.action=Action::SaveConfig;c.config=config_;mailbox_.send(std::move(c));error_.clear();}
    catch(const std::exception& e){error_=e.what();}
}
void MainWindow::start(const Snapshot& s) {
    try {
        config_.outputDirectory=pathFromUtf8(output_);validate(config_);Command c;c.action=Action::Start;c.config=config_;
        for(auto& item:c.config.sequence)for(const auto& p:s.properties)
            if((p.id==Property::Iso||p.id==Property::Aperture||p.id==Property::Shutter)&&!item.settings.contains(p.id))item.settings[p.id]=p.value;
        mailbox_.send(std::move(c));error_.clear();
    }catch(const std::exception& e){error_=e.what();}
}
void MainWindow::single(const Snapshot& s) {
    try {
        config_.outputDirectory=pathFromUtf8(output_);
        Command command;command.action=Action::CaptureSingle;command.config=config_;command.config.sequence.clear();validate(command.config);
        for(const auto& p:s.properties)if(p.id==Property::Iso||p.id==Property::Aperture||p.id==Property::Shutter)
            command.config.singleCapture.settings[p.id]=p.value;
        mailbox_.send(std::move(command));viewLive_=false;showLive_=true;error_.clear();
    }catch(const std::exception& e){error_=e.what();}
}
void MainWindow::render(const Snapshot& s,Preview preview,float monitorScale,Preview captured,DesktopArea desktop) {
    setLanguage(config_.language);
    const bool running=active(s);
    if(desktop.width<=0){auto* vp=ImGui::GetMainViewport();desktop={vp->Pos.x,vp->Pos.y,vp->Size.x,vp->Size.y};}
    const auto condition=resetLayout_?ImGuiCond_Always:ImGuiCond_FirstUseEver;
    ImGui::SetNextWindowPos({desktop.x+10,desktop.y+8},condition);
    ImGui::SetNextWindowSize({std::min(desktop.width-20,em(90)),0},condition);
    ImGuiWindowClass toolbarClass;toolbarClass.ParentViewportId=0;toolbarClass.ViewportFlagsOverrideClear=ImGuiViewportFlags_NoTaskBarIcon;
    ImGui::SetNextWindowClass(&toolbarClass);bool toolbarOpen=true;
    ImGui::Begin("AstroCanon###Toolbar",&toolbarOpen,ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize);
    if(!toolbarOpen)exitRequested_=true;
    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem(tr("Salva piano"),"Ctrl+S"))save();
            ImGui::Separator();if(ImGui::MenuItem(tr("Esci")))exitRequested_=true;ImGui::EndMenu();
        }
        if(ImGui::BeginMenu(tr("Fotocamera"))) {
            if(ImGui::MenuItem(tr("Scatto singolo"),nullptr,false,s.connected&&!running))single(s);
            if(ImGui::MenuItem(s.connected?tr("Disconnetti"):tr("Connetti"),nullptr,false,s.backendAvailable&&!running))send(s.connected?Action::Disconnect:Action::Connect);
            if(ImGui::MenuItem(tr("Aggiorna proprieta"),nullptr,false,s.connected&&!running))send(Action::Refresh);
            if(ImGui::MenuItem("Live View",nullptr,s.liveView,s.connected&&!running))send(Action::LiveView,!s.liveView);ImGui::EndMenu();
        }
        if(ImGui::BeginMenu(tr("Sequenza"))) {
            if(ImGui::MenuItem(tr("Avvia"),nullptr,false,s.connected&&!running&&!config_.sequence.empty()))start(s);
            if(ImGui::MenuItem(tr("Pausa"),nullptr,false,s.progress.state==SequenceState::Running))send(Action::Pause);
            if(ImGui::MenuItem(tr("Riprendi"),nullptr,false,s.progress.state==SequenceState::Paused))send(Action::Resume);
            if(ImGui::MenuItem("Stop",nullptr,false,running))send(Action::Stop);ImGui::EndMenu();
        }
        if(ImGui::BeginMenu(tr("Finestre"))) {
            ImGui::MenuItem(tr("Connessione"),nullptr,&showConnection_);ImGui::MenuItem(tr("Fotocamera"),nullptr,&showSettings_);
            ImGui::MenuItem("Live View",nullptr,&showLive_);ImGui::MenuItem(tr("Sequenze"),nullptr,&showSequence_);
            ImGui::MenuItem(tr("File e sessione"),nullptr,&showOutput_);ImGui::MenuItem(tr("Registro"),nullptr,&showLog_);
            ImGui::Separator();if(ImGui::MenuItem(tr("Ripristina disposizione"))){resetLayout_=true;ImGui::SetWindowPos("AstroCanon###Toolbar",{desktop.x+10,desktop.y+8});showConnection_=showSettings_=showLive_=showSequence_=showOutput_=showLog_=true;}
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu(tr("Impostazioni"))) {
            if(ImGui::BeginMenu(tr("Lingua"))) {
                if(ImGui::MenuItem("Italiano",nullptr,config_.language=="it"))config_.language="it";
                if(ImGui::MenuItem("English",nullptr,config_.language=="en"))config_.language="en";
                ImGui::EndMenu();
            }
            if(ImGui::MenuItem(tr("Tema Dark"),nullptr,!config_.lightTheme))config_.lightTheme=false;
            if(ImGui::MenuItem(tr("Tema Light"),nullptr,config_.lightTheme))config_.lightTheme=true;
            ImGui::Text(tr("DPI monitor: %.0f%%"),monitorScale*100);ImGui::Separator();
            for(float scale:{0.75f,0.85f,1.0f,1.15f,1.25f,1.5f}) {char title[48];std::snprintf(title,sizeof(title),tr("Scala UI %.0f%%%s"),scale*100,scale==1?tr(" (automatica)"):"");if(ImGui::MenuItem(title,nullptr,config_.uiScale==scale))config_.uiScale=scale;}
            ImGui::Separator();if(ImGui::MenuItem(tr("Salva preferenze")))save();ImGui::EndMenu();
        }
        if(ImGui::MenuItem(tr("Esci")))exitRequested_=true;
        ImGui::EndMenuBar();
    }
    ImGui::Text("%s | %s | %llu/%llu",s.connected?s.model.c_str():tr("Disconnessa"),tr(stateName(s.progress.state).c_str()),static_cast<unsigned long long>(s.progress.completed),static_cast<unsigned long long>(s.progress.total));
    ImGui::End();
    if(ImGui::Shortcut(ImGuiMod_Ctrl|ImGuiKey_S))save();
    const float gap=10, top=em(5.6f), width=desktop.width, height=std::max(em(25),desktop.height-top-gap);
    const float left=width*0.22f, right=width*0.24f, center=width-left-right-4*gap;
    auto window=[&](const char* title,bool& visible,float x,float y,float w,float h,auto draw) {
        if(!visible)return;
        ImGui::SetNextWindowPos({desktop.x+x,desktop.y+y},resetLayout_?ImGuiCond_Always:ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({std::max(em(15),w),std::max(em(7),h)},resetLayout_?ImGuiCond_Always:ImGuiCond_FirstUseEver);
        if(resetLayout_)ImGui::SetNextWindowDockID(0,ImGuiCond_Always);
        if(ImGui::Begin(tr(title),&visible))draw();ImGui::End();
    };
    window(connectionTitle,showConnection_,gap,top,left,height*0.24f,[&]{connection(s);});
    window(cameraTitle,showSettings_,gap,top+height*0.24f+gap,left,height*0.55f-gap,[&]{settings(s);});
    window(liveTitle,showLive_,left+2*gap,top,center,height*0.75f,[&]{liveView(s,preview,captured);});
    window(sequenceTitle,showSequence_,width-right-gap,top,right,height*0.96f,[&]{sequence(s);});
    window(outputTitle,showOutput_,width*0.38f,top+height*0.79f,width-right-width*0.38f-2*gap,height*0.21f,[&]{output(s);});
    window(logTitle,showLog_,gap,top+height*0.79f+gap,width*0.38f-2*gap,height*0.21f-gap,[&]{logs(s);});
    resetLayout_=false;
}
}
