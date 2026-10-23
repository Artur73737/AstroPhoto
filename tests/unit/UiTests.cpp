#include <astro/MainWindow.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <gtest/gtest.h>
#include <string>
#include "../../src/ui/src/Widgets.hpp"
#include "../../src/ui/src/AnalysisOverlay.hpp"

namespace {
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
}
TEST(ImGuiDesktop,PanelsFloatWithoutContainerAndCanMoveIndependently) {
    UiContext context(1600,900);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui);
    EXPECT_EQ(panel("Workspace"),nullptr);EXPECT_NE(panel("AstroCanon###Toolbar"),nullptr);
    auto* camera=panel("Fotocamera###Camera");auto* image=panel("Immagine###LiveView");auto* sequence=panel("Sequenze###Sequence");
    ASSERT_NE(camera,nullptr);ASSERT_NE(image,nullptr);ASSERT_NE(sequence,nullptr);
    EXPECT_FALSE(camera->DockIsActive);EXPECT_FALSE(image->DockIsActive);EXPECT_FALSE(sequence->DockIsActive);
    EXPECT_LT(camera->Pos.x+camera->Size.x,image->Pos.x);EXPECT_LT(image->Pos.x+image->Size.x,sequence->Pos.x);
    const auto oldImagePos=image->Pos;ImGui::SetWindowPos(camera,{70,190});ImGui::SetWindowSize(camera,{310,450});context.render(ui);
    EXPECT_FLOAT_EQ(camera->Pos.x,70);EXPECT_FLOAT_EQ(camera->Size.x,310);EXPECT_FLOAT_EQ(image->Pos.x,oldImagePos.x);
}
TEST(ImGuiDesktop,RestartReplacesOldOverlappingLayoutWithPrimaryTiles) {
    std::string settings;
    {
        UiContext context(1600,900);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui);
        ImGui::SetWindowPos(panel("Fotocamera###Camera"),{75,150});ImGui::SetWindowSize(panel("Fotocamera###Camera"),{333,444});context.render(ui);
        std::size_t size{};const char* data=ImGui::SaveIniSettingsToMemory(&size);settings.assign(data,size);
    }
    {
        UiContext context(1600,900);ImGui::LoadIniSettingsFromMemory(settings.data(),settings.size());astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{},true);context.render(ui);
        auto* camera=panel("Fotocamera###Camera");ASSERT_NE(camera,nullptr);
        const auto tile=astro::ui::desktopLayout({0,0,1600,900},13)[2];
        EXPECT_NEAR(camera->Pos.x,tile.x,1);EXPECT_NEAR(camera->Pos.y,tile.y,1);
        EXPECT_NEAR(camera->Size.x,tile.width,1);EXPECT_NEAR(camera->Size.y,tile.height,1);EXPECT_FALSE(camera->DockIsActive);
    }
}
TEST(ImGuiDesktop,AllPanelsStayInsideWorkAreaWithoutOverlapAtDifferentDpi) {
    for(const auto area: {astro::ui::DesktopArea{0,0,1280,720},{-1920,80,1920,1040},{2560,-900,2560,1400}})
    for(float dpi:{.75f,1.f,1.5f,2.f}) {
        UiContext context(area.width,area.height,dpi);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});
        for(int i=0;i<4;++i){ImGui::NewFrame();ui.render({}, {},dpi,{},area);ImGui::Render();}
        const char* names[]{"AstroCanon###Toolbar","Connessione###Connection","Fotocamera###Camera","Immagine###LiveView","Sequenze###Sequence","File e sessione###Output","Registro###Log"};
        for(int i=0;i<7;++i) {
            const auto* a=panel(names[i]);ASSERT_NE(a,nullptr);SCOPED_TRACE(names[i]);
            // No platform backend in this test: ImGui clamps absolute positions to its
            // simulated display. The layout math below independently covers signed origins.
            if(area.x==0&&area.y==0){EXPECT_GE(a->Pos.x,area.x);EXPECT_GE(a->Pos.y,area.y);EXPECT_LE(a->Pos.x+a->Size.x,area.x+area.width);EXPECT_LE(a->Pos.y+a->Size.y,area.y+area.height);
                for(int j=0;j<i;++j){const auto* b=panel(names[j]);EXPECT_FALSE(a->Rect().Overlaps(b->Rect()));}}
        }
        const auto tiles=astro::ui::desktopLayout(area,13*dpi);
        for(std::size_t i=0;i<tiles.size();++i){const auto a=tiles[i];EXPECT_GE(a.x,area.x);EXPECT_GE(a.y,area.y);EXPECT_LE(a.x+a.width,area.x+area.width);EXPECT_LE(a.y+a.height,area.y+area.height);
            for(std::size_t j=0;j<i;++j){const auto b=tiles[j];EXPECT_FALSE(a.x<b.x+b.width&&a.x+a.width>b.x&&a.y<b.y+b.height&&a.y+a.height>b.y);}}
    }
}
TEST(ImGuiDesktop,RoundedStyleSurvivesThemeChange) {
    UiContext context(1600,900);EXPECT_GT(ImGui::GetStyle().FrameRounding,0);EXPECT_GT(ImGui::GetStyle().WindowRounding,0);
    astro::ui::applyTheme(true);EXPECT_GT(ImGui::GetStyle().FrameRounding,0);EXPECT_GT(ImGui::GetStyle().WindowRounding,0);
}
}
