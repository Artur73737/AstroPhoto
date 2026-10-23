#include <astro/Infrastructure.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <iostream>
#ifdef ASTRO_PREVIEW_TESTS
#include "FocusPeaking.hpp"
#endif
using namespace astro;
// Hardware tests are never replaced by emulated SDK calls. Explicit environment opt-in is required.
TEST(Hardware,EnumerateOpenReadPropertiesAndClose) {
    if(!std::getenv("ASTRO_HARDWARE_TESTS"))GTEST_SKIP()<<"Set ASTRO_HARDWARE_TESTS=1 with a physical Canon attached";
    Config c;auto camera=createCanonCamera(c,[](const auto& level,const auto& message){std::cerr<<level<<": "<<message<<'\n';});
    for(int attempt=0;attempt<3;++attempt) {
        auto model=camera->connect();EXPECT_FALSE(model.empty());ASSERT_TRUE(camera->connected());
        auto properties=camera->properties();EXPECT_FALSE(properties.empty());
        std::cout<<"Session "<<attempt+1<<": "<<model<<", "<<properties.size()<<" properties\n";
        for(const auto& p:properties) {
            std::cout<<"  "<<propertyName(p.id)<<"="<<p.currentLabel<<", "<<p.choices.size()<<" available values\n";
            EXPECT_FALSE(p.currentLabel.empty());EXPECT_NE(p.currentLabel,"Non riconosciuto");
            EXPECT_EQ(p.currentLabel.find("0x"),std::string::npos);
            for(const auto& choice:p.choices) {EXPECT_FALSE(choice.label.empty());EXPECT_NE(choice.label,"Non riconosciuto");EXPECT_EQ(choice.label.find("0x"),std::string::npos);}
        }
        camera->prepareHost(512ULL*1024*1024,SaveDestination::PcOnly); // SDK readback is checked before accepting this setting.
        camera->pump();camera->disconnect();EXPECT_FALSE(camera->connected());
    }
}
TEST(Hardware,DownloadTenRealLiveViewFrames) {
    if(!std::getenv("ASTRO_HARDWARE_LIVEVIEW"))GTEST_SKIP()<<"Set ASTRO_HARDWARE_LIVEVIEW=1 to activate Live View";
    Config c;auto camera=createCanonCamera(c,[](const auto&,const auto& message){std::cerr<<message<<'\n';});camera->connect();camera->setLiveView(true);
    int frames=0;auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(c.downloadTimeoutSeconds);
    while(frames<10&&std::chrono::steady_clock::now()<deadline) {
        camera->pump();auto f=camera->liveFrame();
        if(!f.jpeg.empty()) {
            EXPECT_GT(f.jpeg.size(),2);EXPECT_EQ(f.jpeg[0],0xff);EXPECT_EQ(f.jpeg[1],0xd8);
#ifdef ASTRO_PREVIEW_TESTS
            auto pixels=decodeImage(f.jpeg);auto start=std::chrono::steady_clock::now();auto overlay=focusPeaking(pixels,35);
            EXPECT_EQ(overlay.width,pixels.width);EXPECT_EQ(overlay.height,pixels.height);EXPECT_EQ(overlay.rgba.size(),pixels.rgba.size());
            std::cout<<"Real Live View + peaking: "<<pixels.width<<"x"<<pixels.height<<", "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" ms\n";
#endif
            ++frames;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(c.liveViewMs));
    }
    camera->setLiveView(false);camera->disconnect();EXPECT_EQ(frames,10);
}
