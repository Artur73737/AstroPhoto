#include <astro/Infrastructure.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <iostream>
#ifdef ASTRO_PREVIEW_TESTS
#include "FocusPeaking.hpp"
#include "LiveViewPreview.hpp"
#include "ImageAnalysis.hpp"
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
            const auto histogram=imageHistogram(pixels);EXPECT_EQ(histogram.pixels,static_cast<std::uint64_t>(pixels.width)*pixels.height);
            const auto focus=measureFocus(pixels,{true,.5f,.5f,1},&overlay);
            EXPECT_GT(focus.width,0);EXPECT_GT(focus.height,0);EXPECT_GE(focus.score,0);EXPECT_FALSE(focus.nearBest);
            std::cout<<"Real Live View + peaking: "<<pixels.width<<"x"<<pixels.height<<", "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" ms\n";
#endif
            ++frames;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(c.liveViewMs));
    }
    camera->setLiveView(false);camera->disconnect();EXPECT_EQ(frames,10);
}
TEST(Hardware,LiveViewZoomMetadata) {
    if(!std::getenv("ASTRO_HARDWARE_LIVEVIEW"))GTEST_SKIP()<<"Set ASTRO_HARDWARE_LIVEVIEW=1";
    Config c;auto camera=createCanonCamera(c,[](const auto&,const auto& message){std::cerr<<message<<'\n';});camera->connect();camera->setLiveView(true);
    for(int ratio:{1,5,10,1}) {
        camera->zoom(ratio);LiveFrame frame;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
        do {camera->pump();frame=camera->liveFrame();if(!frame.jpeg.empty()&&frame.zoom==ratio)break;std::this_thread::sleep_for(std::chrono::milliseconds(100));}while(std::chrono::steady_clock::now()<deadline);
        EXPECT_FALSE(frame.jpeg.empty());EXPECT_EQ(frame.zoom,ratio);
        std::cout<<"Zoom "<<frame.zoom<<": coordinate system "<<frame.coordinateWidth<<"x"<<frame.coordinateHeight<<", image origin "<<frame.imageX<<","<<frame.imageY<<", rect "<<frame.zoomRect.x<<","<<frame.zoomRect.y<<" "<<frame.zoomRect.width<<"x"<<frame.zoomRect.height<<'\n';
#ifdef ASTRO_PREVIEW_TESTS
        if(!frame.jpeg.empty()){
            auto pixels=decodeImage(frame.jpeg);auto display=prepareLiveViewPreview(pixels,frame);
            std::cout<<"JPEG "<<pixels.width<<"x"<<pixels.height<<" -> display "<<display.width<<"x"<<display.height<<'\n';
            EXPECT_EQ(display.width,ratio==10?pixels.width/2:pixels.width);EXPECT_EQ(display.height,ratio==10?pixels.height/2:pixels.height);
            EXPECT_EQ(focusPeaking(display,35,true).rgba.size(),display.rgba.size());
        }
#endif
    }
    camera->setLiveView(false);camera->disconnect();
}
