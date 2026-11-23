#include <astro/features/true_north/workspace/ZwoWorkspace.hpp>
#include <imgui.h>
#include <astro/features/true_north/astrometry/PolarSolve.hpp>
#include <astro/features/true_north/astronomy/TrueNorth.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <chrono>
#include <cstdlib>
TEST(ZwoWorkspace, RendersBothCompactAndWideLayoutsWithoutCameraOrOpenGlTexture) {
    auto* context = ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    for (auto size : {ImVec2(600, 900), ImVec2(1400, 900)}) {
        io.DisplaySize = size;
        io.DeltaTime = 1.f / 60;
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0, 0});
        ImGui::SetNextWindowSize(size);
        ImGui::Begin("ZWO integration");
        astro::ui::zwoNorthWorkspace();
        ImGui::End();
        ImGui::Render();
        ASSERT_GT(ImGui::GetDrawData()->TotalVtxCount, 0);
    }
    ImGui::DestroyContext(context);
}

TEST(ZwoWorkspace, FreshDssFileSolvesThroughControllerWithoutPhysicalCompassClaim) {
    auto file = std::getenv("ASTRO_NORTH_CONTROLLER_FILE");
    auto solver = std::getenv("ASTRO_ASTAP_EXE");
    auto catalog = std::getenv("ASTRO_ASTAP_DATABASE");
    if (!file || !solver || !catalog) GTEST_SKIP() << "Opt-in real survey file and actual ASTAP process";
    using J = nlohmann::json;
    auto state = J::parse(astro::ui::zwoNorthControl(J{
        {"action", "solve"},
        {"options",
         {{"file", file},
          {"solver", solver},
          {"catalog", catalog},
          {"epoch_utc", "2026-11-22T00:00:00Z"},
          {"plate_only", true},
          {"fov", 3}}}}.dump()));
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    while (state.at("busy").get<bool>() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        state = J::parse(astro::ui::zwoNorthControl("{\"action\":\"state\"}"));
    }
    ASSERT_FALSE(state.at("busy").get<bool>());
    ASSERT_TRUE(state.contains("result")) << state.dump();
    EXPECT_EQ(state["result"]["mode"], "plate_only");
    EXPECT_TRUE(state["result"]["geographic_bearing"].is_null());
    EXPECT_NEAR(state["result"]["fov_height_deg"].get<double>(), 3, .02);
    EXPECT_FALSE(state["result"]["pole"].is_null());
    std::cout << state.dump(2) << '\n';
}

TEST(PolarRecovery, SuppliedFieldsSolveFromFreshOriginalPixelsAndCatalogue) {
    auto root=std::getenv("ASTRO_USER_POLAR_DIRECTORY");auto solver=std::getenv("ASTRO_ASTAP_EXE");auto catalog=std::getenv("ASTRO_ASTAP_DATABASE");
    if(!root||!solver||!catalog)GTEST_SKIP()<<"Opt-in original user fields and real ASTAP/G05";
    const char* files[]={"Screenshot 2026-11-22 184047.png","Screenshot 2026-11-22 184052.png","Screenshot 2026-11-22 184056.png"};
    double previous=10;
    for(auto name:files){astro::StackOptions options;options.solverExecutable=solver;options.solverDatabase=catalog;options.solverTimeoutSeconds=60;
        auto solution=astro::north::solvePolarField(std::filesystem::path(root)/name,options,{},[](auto message){std::cout<<message<<'\n';});
        ASSERT_TRUE(solution.solved)<<name<<": "<<solution.detail;
        auto& m=solution.wcs;astro::north::Wcs w{m.at("CRPIX1"),m.at("CRPIX2"),m.at("CRVAL1"),m.at("CRVAL2"),{m.at("CD1_1"),m.at("CD1_2"),m.at("CD2_1"),m.at("CD2_2")},solution.width,solution.height};
        auto fov=astro::north::fieldOfView(w);EXPECT_GT(fov[1],1);EXPECT_LT(fov[1],3);EXPECT_LT(fov[1],previous);previous=fov[1];
        auto pole=astro::north::pixelAt(w,{0,90});ASSERT_TRUE(pole);EXPECT_GT(pole->x,1);EXPECT_LT(pole->x,solution.width);EXPECT_GT(pole->y,1);EXPECT_LT(pole->y,solution.height);
        std::cout<<name<<" FOV="<<fov[0]<<" x "<<fov[1]<<" J2000 pole="<<pole->x<<","<<pole->y<<" "<<solution.detail<<'\n';
    }
}
