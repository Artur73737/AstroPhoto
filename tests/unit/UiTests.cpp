#include <astro/MainWindow.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <gtest/gtest.h>
#include <string>
#include "../../src/ui/src/Widgets.hpp"
#include "../../src/ui/src/AnalysisOverlay.hpp"

namespace {
TEST(DisplayedFrameRate,CountsDisplayedFramesInsteadOfUiRedrawsOrCameraSequenceGaps){
    astro::ui::DisplayedFrameRate fps;
    EXPECT_EQ(fps.update(10,0),0);
    EXPECT_EQ(fps.update(10,.01),0);
    EXPECT_NEAR(fps.update(20,.1),10,1e-9);
    EXPECT_NEAR(fps.update(40,.2),10,1e-9);
    EXPECT_NEAR(fps.update(40,.25),8,1e-9);
    EXPECT_EQ(fps.update(40,1.3),0);
    EXPECT_EQ(fps.update(50,1.4),0);
    EXPECT_NEAR(fps.update(60,1.5),10,1e-9);
}
TEST(DisplayedFrameRate,ResetsAcrossDisconnectClockResetAndRevisionRestart){
    astro::ui::DisplayedFrameRate fps;
    fps.update(100,5);EXPECT_NEAR(fps.update(101,5.05),20,1e-8);
    EXPECT_EQ(fps.update(1,5.1),0);
    EXPECT_NEAR(fps.update(2,5.15),20,1e-8);
    EXPECT_EQ(fps.update(3,0),0);
    fps.reset();EXPECT_EQ(fps.update(4,.05),0);
    EXPECT_EQ(fps.update(0,.1),0);
}
TEST(StellarFocusControls,CanStartWithZeroStarsButStillRequiresAfLiveViewAndManualExposure) {
    astro::Snapshot s;s.connected=true;s.liveView=true;s.manualFocusExposure=true;s.lensFocus.state=astro::LensFocusState::Ready;
    ASSERT_TRUE(s.stars.stars.empty());EXPECT_TRUE(astro::ui::widgets::canStartStarFocus(s));
    s.manualFocusExposure=false;EXPECT_FALSE(astro::ui::widgets::canStartStarFocus(s));s.manualFocusExposure=true;
    s.liveView=false;EXPECT_FALSE(astro::ui::widgets::canStartStarFocus(s));s.liveView=true;
    s.lensFocus.state=astro::LensFocusState::ManualFocus;EXPECT_FALSE(astro::ui::widgets::canStartStarFocus(s));
}
TEST(LensFocusControls,OnlyEnableForConnectedIdleLiveViewWithAfLens) {
    astro::Snapshot s;EXPECT_FALSE(astro::ui::widgets::canDriveFocus(s));
    s.connected=true;s.liveView=true;
    for(auto state:{astro::LensFocusState::Unknown,astro::LensFocusState::NoLens,astro::LensFocusState::ManualFocus,astro::LensFocusState::Unsupported}){s.lensFocus.state=state;EXPECT_FALSE(astro::ui::widgets::canDriveFocus(s));}
    s.lensFocus.state=astro::LensFocusState::Ready;EXPECT_TRUE(astro::ui::widgets::canDriveFocus(s));
    s.liveView=false;EXPECT_FALSE(astro::ui::widgets::canDriveFocus(s));s.liveView=true;
    for(auto state:{astro::SequenceState::Running,astro::SequenceState::Paused,astro::SequenceState::Stopping}){s.progress.state=state;EXPECT_FALSE(astro::ui::widgets::canDriveFocus(s));}
    s.progress.state=astro::SequenceState::Idle;s.connected=false;EXPECT_FALSE(astro::ui::widgets::canDriveFocus(s));
    s.connected=true;s.autofocus.phase=astro::StarFocusPhase::Sampling;EXPECT_FALSE(astro::ui::widgets::canDriveFocus(s));
}
TEST(PointFocusOverlay,DistinguishesOffEmptyAndSparseCoverage) {
    astro::ui::setLanguage("en");astro::FocusReading f;f.peakingMeasured=true;
    EXPECT_NE(astro::ui::focusMeasurementText(f,false).find("Peaking: off"),std::string::npos);
    EXPECT_NE(astro::ui::focusMeasurementText(f,true).find("0 pixels - no signal"),std::string::npos);
    f.peakingPixels=1;f.peakingFraction=1./4096;
    EXPECT_NE(astro::ui::focusMeasurementText(f,true).find("0.02% (1 px)"),std::string::npos);
    astro::ui::setLanguage("it");EXPECT_NE(astro::ui::focusMeasurementText(f,false).find("disattivato"),std::string::npos);
}
TEST(ImGuiProperties,ReadOnlyCurrentValueUsesItsLabelWithoutAddingChoices) {
    astro::ui::setLanguage("it");
    astro::CameraProperty p{astro::Property::ExposureMode,3,{},false,"M - Manuale"};
    EXPECT_EQ(astro::ui::widgets::label(p,3),"M - Manuale");
    EXPECT_TRUE(p.choices.empty());EXPECT_FALSE(p.writable);
    EXPECT_EQ(astro::ui::widgets::label(p,99),"Non disponibile");
}
// Exercise the real ImGui layout/docking engine, without a camera or GPU backend.
struct UiContext {
    UiContext(float width,float height,float scale=1) {
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_DockingEnable;
        io.DisplaySize={width,height};io.DeltaTime=1.0f/60;
        ImFontConfig font;font.SizePixels=13*scale;io.Fonts->AddFontDefault(&font);io.Fonts->Build();
        astro::ui::applyTheme();ImGui::GetStyle().ScaleAllSizes(scale);
    }
    ~UiContext(){ImGui::DestroyContext();}
    void render(astro::ui::MainWindow& ui,float scale=1) {
        for(int i=0;i<4;++i){ImGui::NewFrame();ui.render({}, {},scale);ImGui::Render();}
    }
};
ImGuiWindow* panel(const char* name) {return ImGui::FindWindowByName(name);}
TEST(ImGuiDesktop,EnglishLabelsKeepWindowIdentityAndLayout) {
    UiContext context(1600,900);astro::Mailbox mailbox;astro::Config config;config.language="en";
    astro::ui::MainWindow ui(mailbox,config);context.render(ui);
    auto* camera=panel("Camera###Camera");ASSERT_NE(camera,nullptr);
    EXPECT_EQ(camera,panel("Fotocamera###Camera"));
    EXPECT_STREQ(astro::ui::tr("Impostazioni"),"Settings");
    EXPECT_STREQ(astro::ui::tr("Soglia"),"Threshold");
    EXPECT_STREQ(astro::ui::tr("M - Manuale"),"M - Manual");
    EXPECT_STREQ(astro::ui::tr("RAW + JPEG L Normale"),"RAW + JPEG L Normal");
    EXPECT_EQ(astro::ui::translatedMessage("Prossimo scatto tra 12 s"),"Next capture in 12 s");
    EXPECT_EQ(astro::ui::translatedMessage("Anteprima incorporata CR2 | foto.CR2"),"Embedded CR2 preview | foto.CR2");
    astro::ui::setLanguage("it");EXPECT_STREQ(astro::ui::tr("Impostazioni"),"Impostazioni");
    EXPECT_EQ(astro::ui::translatedMessage("Connessa: Canon EOS 1100D"),"Connessa: Canon EOS 1100D");
    astro::ui::setLanguage("en");EXPECT_EQ(astro::ui::translatedMessage("Connessa: Canon EOS 1100D"),"Connected: Canon EOS 1100D");
    config.language="it";ui.controlConfig(config);EXPECT_EQ(mailbox.logLanguage(),"it");
    EXPECT_EQ(astro::ui::translatedMessage("Connessa: Canon EOS 1100D"),"Connessa: Canon EOS 1100D");
}
TEST(ImGuiWorkspace,AllPanelsDockInsideOneContainer) {
    UiContext context(1920,1080);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui);
    ASSERT_NE(panel("###Toolbar"),nullptr);
    for(const auto* name:{"###Connection","###Camera","###LiveView","###Sequence","###Output","###Log"}){
        const auto* p=panel(name);ASSERT_NE(p,nullptr);EXPECT_NE(p->DockId,0u);EXPECT_EQ(p->Viewport,ImGui::GetMainViewport());
    }
    EXPECT_EQ(ImGui::GetPlatformIO().Viewports.Size,1);
    EXPECT_FALSE(ImGui::GetIO().ConfigFlags&ImGuiConfigFlags_ViewportsEnable);
}
TEST(ImGuiDesktop,FocusControlsStayToTheRightOfThePreview){
    UiContext context(1600,900);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui);
    ImGuiWindow* preview=nullptr;ImGuiWindow* controls=nullptr;
    for(auto* window:GImGui->Windows){std::string name=window->Name;if(name.find("/Preview_")!=std::string::npos)preview=window;if(name.find("/Focus controls_")!=std::string::npos)controls=window;}
    ASSERT_NE(preview,nullptr);ASSERT_NE(controls,nullptr);
    EXPECT_LE(preview->Pos.x+preview->Size.x,controls->Pos.x);EXPECT_NEAR(preview->Pos.y,controls->Pos.y,1);EXPECT_GT(preview->Size.x,controls->Size.x);
}
TEST(ImGuiWorkspace,CustomizedTabsSurviveRestartAndResize) {
    std::string settings;
    {
        UiContext context(1920,1080);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui);
        ui.automaticLayout(false);
        ImGui::DockBuilderDockWindow("###Camera",panel("###LiveView")->DockId);context.render(ui);
        ASSERT_EQ(panel("###Camera")->DockId,panel("###LiveView")->DockId);
        std::size_t size{};const char* data=ImGui::SaveIniSettingsToMemory(&size);settings.assign(data,size);
    }
    {
        UiContext context(1280,800);ImGui::LoadIniSettingsFromMemory(settings.data(),settings.size());
        astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{},true);ui.automaticLayout(false);context.render(ui);
        EXPECT_EQ(panel("###Camera")->DockId,panel("###LiveView")->DockId);
        ImGui::GetIO().DisplaySize={1100,720};context.render(ui);
        EXPECT_EQ(panel("###Camera")->DockId,panel("###LiveView")->DockId);
        ui.resetLayout();context.render(ui);
        EXPECT_NE(panel("###Camera")->DockId,panel("###LiveView")->DockId);
    }
}
TEST(ImGuiWorkspace,AutomaticLayoutRegroupsAtNarrowWidthsAndRestoresColumns) {
    UiContext context(1920,1080);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui);
    EXPECT_NE(panel("###Camera")->DockId,panel("###LiveView")->DockId);
    ImGui::GetIO().DisplaySize={800,600};context.render(ui);
    for(const auto* name:{"###Connection","###Camera","###Sequence","###Output","###Log"})EXPECT_EQ(panel(name)->DockId,panel("###LiveView")->DockId);
    EXPECT_FALSE(panel("###LiveView")->Hidden);
    ImGui::GetIO().DisplaySize={1920,1080};context.render(ui);
    EXPECT_NE(panel("###Camera")->DockId,panel("###LiveView")->DockId);
    EXPECT_NE(panel("###Sequence")->DockId,panel("###LiveView")->DockId);
}
TEST(ImGuiWorkspace,ClosedPanelsReopenAndUndockedPanelsReturnToContainer) {
    UiContext context(1600,900);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui);
    auto state=ui.controlState();state.panels[1]=false;ui.controlState(state);context.render(ui);
    state.panels[1]=true;ui.controlState(state);context.render(ui);EXPECT_NE(panel("###Camera")->DockId,0u);
    ImGui::DockBuilderDockWindow("###Camera",0);context.render(ui);
    EXPECT_NE(panel("###Camera")->DockId,0u);EXPECT_EQ(panel("###Camera")->Viewport,ImGui::GetMainViewport());
}
TEST(ImGuiWorkspace,VisiblePanelsStayInsideClientAreaWithoutOverlapAtDifferentDpi) {
    for(const auto size:{ImVec2{640,480},ImVec2{800,600},ImVec2{1280,720},ImVec2{1920,1080},ImVec2{2560,1440}})
    for(float dpi:{.75f,1.f,1.5f,2.f}) {
        UiContext context(size.x,size.y,dpi);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui,dpi);
        const ImRect work({0,0},size);std::vector<ImRect> visible;
        for(const auto* name:{"###Connection","###Camera","###LiveView","###Sequence","###Output","###Log"}){
            const auto* p=panel(name);ASSERT_NE(p,nullptr);SCOPED_TRACE(std::string(name)+" "+std::to_string(size.x)+" dpi="+std::to_string(dpi));
            EXPECT_NE(p->DockId,0u);EXPECT_EQ(p->Viewport,ImGui::GetMainViewport());
            if(p->Hidden)continue;
            EXPECT_TRUE(work.Contains(p->Rect()));
            for(const auto& other:visible)EXPECT_FALSE(p->Rect().Overlaps(other));visible.push_back(p->Rect());
        }
    }
}
TEST(ImGuiWorkspace,NarrowPreviewPlacesFocusBelowImage) {
    UiContext context(640,600);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui);
    ImGuiWindow *preview=nullptr,*controls=nullptr;
    for(auto* w:GImGui->Windows){std::string name=w->Name;if(name.find("/Preview_")!=std::string::npos)preview=w;if(name.find("/Focus controls_")!=std::string::npos)controls=w;}
    ASSERT_NE(preview,nullptr);ASSERT_NE(controls,nullptr);
    EXPECT_LE(preview->Pos.y+preview->Size.y,controls->Pos.y);EXPECT_NEAR(preview->Pos.x,controls->Pos.x,1);
}
TEST(ImGuiDesktop,RoundedStyleSurvivesThemeChange) {
    UiContext context(1600,900);EXPECT_GT(ImGui::GetStyle().FrameRounding,0);EXPECT_GT(ImGui::GetStyle().WindowRounding,0);
    astro::ui::applyTheme(true);EXPECT_GT(ImGui::GetStyle().FrameRounding,0);EXPECT_GT(ImGui::GetStyle().WindowRounding,0);
}
TEST(ImGuiWorkspace,LiveStackIsAModeInsideImageWithoutAnotherDockPanel) {
    for(const auto size:{ImVec2{1920,1080},ImVec2{640,600}}){
        UiContext context(size.x,size.y);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});
        auto state=ui.controlState();state.viewStack=true;ui.controlState(state);context.render(ui);
        EXPECT_EQ(panel("###LiveStack"),nullptr);
        auto* image=panel("###LiveView");ASSERT_NE(image,nullptr);
        ImGuiWindow *preview=nullptr,*controls=nullptr;
        for(auto* w:GImGui->Windows){std::string name=w->Name;if(w->ParentWindow==image&&name.find("/Preview_")!=std::string::npos)preview=w;if(w->ParentWindow==image&&name.find("/Stack controls_")!=std::string::npos)controls=w;}
        ASSERT_NE(preview,nullptr);ASSERT_NE(controls,nullptr);EXPECT_EQ(preview->ParentWindow,image);EXPECT_EQ(controls->ParentWindow,image);
        if(size.x>1000)EXPECT_LE(controls->Pos.x+controls->Size.x,preview->Pos.x);else EXPECT_LE(preview->Pos.y+preview->Size.y,controls->Pos.y);
    }
}
}
