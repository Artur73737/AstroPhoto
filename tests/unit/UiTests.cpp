#include <astro/MainWindow.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <gtest/gtest.h>
#include <string>
#include "../../src/ui/src/Widgets.hpp"

namespace {
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
TEST(ImGuiDesktop,UserPositionAndSizeSurviveRestart) {
    std::string settings;
    {
        UiContext context(1600,900);astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{});context.render(ui);
        ImGui::SetWindowPos(panel("Fotocamera###Camera"),{75,150});ImGui::SetWindowSize(panel("Fotocamera###Camera"),{333,444});context.render(ui);
        std::size_t size{};const char* data=ImGui::SaveIniSettingsToMemory(&size);settings.assign(data,size);
    }
    {
        UiContext context(1600,900);ImGui::LoadIniSettingsFromMemory(settings.data(),settings.size());astro::Mailbox mailbox;astro::ui::MainWindow ui(mailbox,{},true);context.render(ui);
        auto* camera=panel("Fotocamera###Camera");ASSERT_NE(camera,nullptr);EXPECT_FLOAT_EQ(camera->Pos.x,75);EXPECT_FLOAT_EQ(camera->Pos.y,150);
        EXPECT_FLOAT_EQ(camera->Size.x,333);EXPECT_FLOAT_EQ(camera->Size.y,444);EXPECT_FALSE(camera->DockIsActive);
    }
}
TEST(ImGuiDesktop,RoundedStyleSurvivesThemeChange) {
    UiContext context(1600,900);EXPECT_GT(ImGui::GetStyle().FrameRounding,0);EXPECT_GT(ImGui::GetStyle().WindowRounding,0);
    astro::ui::applyTheme(true);EXPECT_GT(ImGui::GetStyle().FrameRounding,0);EXPECT_GT(ImGui::GetStyle().WindowRounding,0);
}
}
