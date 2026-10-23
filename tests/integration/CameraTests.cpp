#include <astro/Infrastructure.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <astro/StarAutofocus.hpp>
#ifdef ASTRO_PREVIEW_TESTS
#include "FocusPeaking.hpp"
#include "LiveViewPreview.hpp"
#include "ImageAnalysis.hpp"
#include "StarAnalysis.hpp"
#endif
using namespace astro;
TEST(Hardware,IdleKeepAliveAndLiveViewSoak) {
    if(!std::getenv("ASTRO_HARDWARE_CONNECTION_SOAK"))GTEST_SKIP()<<"Set ASTRO_HARDWARE_CONNECTION_SOAK=1 for a 3-minute no-shutter connection test";
    Config c;auto camera=createCanonCamera(c,[](const auto& level,const auto& message){std::cout<<level<<": "<<message<<std::endl;});
    ASSERT_FALSE(camera->connect().empty());
    const auto start=std::chrono::steady_clock::now();auto nextReport=start;bool live{};int frames{};
    while(std::chrono::steady_clock::now()-start<std::chrono::seconds(180)){
        ASSERT_NO_THROW(camera->pump());ASSERT_TRUE(camera->connected());
        auto elapsed=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start).count();
        if(elapsed>=150&&!live){camera->setLiveView(true);live=true;}
        if(live&&!camera->liveFrame().jpeg.empty())++frames;
        if(std::chrono::steady_clock::now()>=nextReport){std::cout<<"SOAK connected "<<elapsed<<" s, live frames "<<frames<<std::endl;nextReport=std::chrono::steady_clock::now()+std::chrono::seconds(15);}
        std::this_thread::sleep_for(std::chrono::milliseconds(c.liveViewMs));
    }
    EXPECT_GT(frames,0);camera->setLiveView(false);camera->disconnect();
    EXPECT_FALSE(camera->connect().empty());camera->disconnect();
    std::cout<<"SOAK PASS: 150 s idle + 30 s Live View, then reconnect in the same process; no shutter or motor commands"<<std::endl;
}
#ifdef ASTRO_PREVIEW_TESTS
TEST(Hardware,StellarFocusOnRealLiveView) {
    const auto mode=std::getenv("ASTRO_HARDWARE_STAR_FOCUS");if(!mode)GTEST_SKIP()<<"Set ASTRO_HARDWARE_STAR_FOCUS=analyze or run";
    Config c;auto camera=createCanonCamera(c,[](const auto&,const auto& message){std::cerr<<message<<std::endl;});
    ASSERT_FALSE(camera->connect().empty());
    std::optional<std::uint32_t> previousIso;
    struct RestoreIso{ICamera& camera;std::optional<std::uint32_t>& value;~RestoreIso(){if(value&&camera.connected())try{camera.set(Property::Iso,*value);}catch(const std::exception& e){std::cerr<<"ISO restore failed: "<<e.what()<<std::endl;}}} restore{*camera,previousIso};
    if(const auto iso=std::getenv("ASTRO_FOCUS_TEST_ISO")){
        for(const auto& p:camera->properties())if(p.id==Property::Iso){auto choice=std::find_if(p.choices.begin(),p.choices.end(),[&](const auto& v){return v.label==iso;});ASSERT_NE(choice,p.choices.end());previousIso=p.value;camera->set(Property::Iso,choice->value);std::cout<<"Temporary diagnostic ISO "<<iso<<"; restored at test end"<<std::endl;}
    }
    camera->setLiveView(true);SystemClock clock;StarField field;LiveFrame frame;
    const auto deadline=clock.steadyNow()+std::chrono::seconds(15);int received{};
    while(received<10&&clock.steadyNow()<deadline){camera->pump();auto next=camera->liveFrame();if(!next.jpeg.empty()){
        frame=std::move(next);auto start=clock.steadyNow();field=measureStars(prepareLiveViewPreview(decodeImage(frame.jpeg),frame));
        std::cout<<"STARS "<<field.stars.size()<<" broad "<<field.acquisitionCandidates<<" saturated "<<field.saturated<<" background "<<field.background<<" analysis_ms "<<std::chrono::duration<double,std::milli>(clock.steadyNow()-start).count()<<std::endl;++received;
    }std::this_thread::sleep_for(std::chrono::milliseconds(c.liveViewMs));}
    ASSERT_GT(received,0);
    if(const auto directory=std::getenv("ASTRO_FOCUS_DIAGNOSTICS_DIR")){std::filesystem::create_directories(directory);std::ofstream image(std::filesystem::path(directory)/"star-field.jpg",std::ios::binary);image.write(reinterpret_cast<const char*>(frame.jpeg.data()),static_cast<std::streamsize>(frame.jpeg.size()));}
    for(const auto& star:field.stars)std::cout<<"STAR "<<star.x<<","<<star.y<<" HFD "<<star.hfd<<" SNR "<<star.snr<<" flux "<<star.flux<<std::endl;
    if(std::string(mode)=="run"){
        // Zero detected stars is a valid acquisition starting point.
        StarAutofocus autofocus(*camera,clock);autofocus.start(c.starFocus);std::size_t points{};int pass{},moves{};
        while(autofocus.progress().active()){
            camera->pump();autofocus.tick();auto f=camera->liveFrame();if(!f.jpeg.empty()){auto measured=measureStars(prepareLiveViewPreview(decodeImage(f.jpeg),f));autofocus.tick(&measured,f.revision);
                if(const auto directory=std::getenv("ASTRO_FOCUS_DIAGNOSTICS_DIR")){if(autofocus.progress().moves!=moves){std::ofstream image(std::filesystem::path(directory)/("move-"+std::to_string(autofocus.progress().moves)+".jpg"),std::ios::binary);image.write(reinterpret_cast<const char*>(f.jpeg.data()),static_cast<std::streamsize>(f.jpeg.size()));std::cout<<"FIELD measured "<<measured.stars.size()<<" broad "<<measured.acquisitionCandidates<<" background "<<measured.background<<std::endl;}}
            }
            if(autofocus.progress().moves!=moves||autofocus.progress().pass!=pass){moves=autofocus.progress().moves;pass=autofocus.progress().pass;std::cout<<"AF_PROGRESS phase "<<int(autofocus.progress().phase)<<" pass "<<pass<<" moves "<<moves<<" search "<<autofocus.progress().searchMoves<<" position "<<autofocus.progress().position<<" step "<<autofocus.progress().motorStep<<" spacing "<<autofocus.progress().stride<<std::endl;}
            if(autofocus.progress().curve.size()!=points){points=autofocus.progress().curve.size();if(!points)continue;const auto& p=autofocus.progress().curve.back();std::cout<<"FOCUS_POINT "<<p.position<<" HFD "<<p.hfd<<" scatter "<<p.scatter<<std::endl;}
            std::this_thread::sleep_for(std::chrono::milliseconds(c.liveViewMs));
        }
        const auto& result=autofocus.progress();std::cout<<"FOCUS_RESULT "<<result.detail<<" baseline "<<result.baseline<<" best "<<result.best<<" verified "<<result.verified<<" moves "<<result.moves<<std::endl;
        EXPECT_EQ(result.phase,StarFocusPhase::Complete)<<result.detail;
    }
    camera->setLiveView(false);if(previousIso){camera->set(Property::Iso,*previousIso);previousIso.reset();}camera->disconnect();
}
#endif
TEST(Hardware,PowerCycleReconnectWithoutRestart) {
    if(!std::getenv("ASTRO_HARDWARE_POWER_CYCLE"))GTEST_SKIP()<<"Set ASTRO_HARDWARE_POWER_CYCLE=1 for an operator-assisted power cycle";
    Config c;auto camera=createCanonCamera(c,[](const auto&,const auto& message){std::cerr<<message<<std::endl;});
    ASSERT_FALSE(camera->connect().empty());
    std::cout<<"POWER CYCLE READY: switch the camera off, wait five seconds, then switch it on. Keep this process running."<<std::endl;
    bool disconnected=false;const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
    while(std::chrono::steady_clock::now()<deadline) {
        try{camera->pump();}catch(const CameraError& e){std::cout<<"Disconnect event: "<<e.what()<<std::endl;}
        if(!camera->connected()){disconnected=true;break;}
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    ASSERT_TRUE(disconnected)<<"No physical power-off observed";
    std::string model;
    while(std::chrono::steady_clock::now()<deadline) {
        try{camera->pump();model=camera->connect();break;}
        catch(const CameraError& e) {
            ASSERT_EQ(std::string(e.what()).find("Sessione gia aperta"),std::string::npos)<<e.what();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    ASSERT_FALSE(model.empty())<<"Camera did not return before timeout";
    EXPECT_TRUE(camera->connected());EXPECT_FALSE(camera->properties().empty());
    camera->disconnect();EXPECT_FALSE(camera->connected());
    EXPECT_FALSE(camera->connect().empty());camera->disconnect();
    std::cout<<"POWER CYCLE PASS: same camera object reconnected and read properties, then opened another clean session."<<std::endl;
}
TEST(Hardware,LensFocusStateAndOptionalDrive) {
    if(!std::getenv("ASTRO_HARDWARE_TESTS"))GTEST_SKIP()<<"Set ASTRO_HARDWARE_TESTS=1";
    Config c;auto camera=createCanonCamera(c,[](const auto&,const auto& message){std::cerr<<message<<'\n';});camera->connect();
    const auto status=camera->lensFocusStatus();
    std::cout<<"Lens focus state: "<<static_cast<int>(status.state)<<" (0 unknown, 1 no lens, 2 MF, 3 AF ready, 4 unsupported), "<<status.detail<<'\n';
    if(std::getenv("ASTRO_HARDWARE_FOCUS")) {
        camera->setLiveView(true);const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);LiveFrame frame;
        do {camera->pump();frame=camera->liveFrame();if(!frame.jpeg.empty())break;std::this_thread::sleep_for(std::chrono::milliseconds(c.liveViewMs));}while(std::chrono::steady_clock::now()<deadline);
        ASSERT_FALSE(frame.jpeg.empty());
        if(camera->lensFocusStatus().state==LensFocusState::Ready) {
            camera->focus(-1);
            for(int i=0;i<5;++i){camera->pump();std::this_thread::sleep_for(std::chrono::milliseconds(c.liveViewMs));}
            camera->focus(1);std::cout<<"Accepted one Near1 and one Far1 motor command; not a guarantee of restoring the exact optical focus.\n";
        } else {EXPECT_THROW(camera->focus(-1),CameraError);std::cout<<"Motor correctly blocked: lens is not reported AF-ready.\n";}
        camera->setLiveView(false);
    }
    camera->disconnect();
}
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
