#include <astro/Infrastructure.hpp>
#include <astro/ConnectionPower.hpp>
#include <astro/WorkerWait.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <fstream>
using namespace astro;
namespace {
TEST(WorkerWait,CancelsAnOutstandingWaitAndCanBeReused){
    WorkerWait timer;std::stop_source source;std::atomic<bool> entered{},completed{},result{true};
    std::jthread waiter([&]{entered=true;result=timer.wait(source.get_token(),std::chrono::seconds(10));completed=true;});
    while(!entered)std::this_thread::yield();
    source.request_stop();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!completed&&std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
    EXPECT_TRUE(completed);waiter.join();EXPECT_FALSE(result);
    std::stop_source next;EXPECT_TRUE(timer.wait(next.get_token(),std::chrono::milliseconds(2)));
    EXPECT_FALSE(timer.wait(source.get_token(),std::chrono::milliseconds(2)));
}
TEST(WorkerWait,DoesNotCompleteBeforeItsDeadline){
    WorkerWait timer;std::stop_source source;const auto start=std::chrono::steady_clock::now();
    EXPECT_TRUE(timer.wait(source.get_token(),std::chrono::milliseconds(15)));
    EXPECT_GE(std::chrono::steady_clock::now()-start,std::chrono::milliseconds(14));
}
struct Control {std::atomic<int> connections{},closes{},frames{},captures{},pumpFault{},frameFault{},pointStarts{},pointStops{};std::atomic<bool> unavailable{}, captureComplete{}, captureWhileLive{},pointDone{};std::atomic<std::uint64_t> pointThread{};};
// Domain interface double only: no SDK symbols or replacement Canon library.
class WorkerCamera final:public ICamera {
    std::shared_ptr<Control> c;bool open{},live{};std::uint64_t revision{};
public:
    explicit WorkerCamera(std::shared_ptr<Control> state):c(std::move(state)){}
    std::string connect()override{++c->connections;if(c->unavailable)throw CameraError("USB missing",false,true);open=true;return "Domain test";}
    void disconnect()override{if(open)++c->closes;open=live=false;}
    bool connected()const override{return open;}
    void pump()override{const int fault=c->pumpFault.exchange(0);if(fault==1)throw CameraError("Busy",true);if(fault==2){open=false;throw CameraError("USB lost",false,true);}}
    std::vector<CameraProperty> properties()override{return {};}
    void set(Property,std::uint32_t)override{}
    void prepareHost(std::uint64_t,SaveDestination)override{}
    void beginCapture(const SequenceItem&,const std::filesystem::path&)override{c->captureWhileLive=live;++c->captures;}
    CaptureResult pollCapture()override{return c->captureComplete?CaptureResult{true,{"domain-frame.CR2"}}:CaptureResult{};}
    void abortCapture()override{disconnect();}
    void setLiveView(bool enabled)override{live=enabled;}
    LiveFrame liveFrame()override{if(c->frameFault.exchange(0))throw CameraError("Frame not ready",true);if(!live)return {};++c->frames;return {{1},++revision,1};}
    void focus(int)override{}
    LensFocusStatus lensFocusStatus()override{return {LensFocusState::Ready,{}};}
    bool manualExposureForFocus()override{return true;}
    void zoom(int)override{}
    void startPointAutofocus(double,double)override{++c->pointStarts;c->pointThread=std::hash<std::thread::id>{}(std::this_thread::get_id());}
    PointAfStatus pollPointAutofocus()override{return {c->pointDone?PointAfPhase::Complete:PointAfPhase::Running,.5,.5,{}};}
    void stopPointAutofocus()override{++c->pointStops;}
};
struct WorkerTest:testing::Test {
    Mailbox mailbox;std::shared_ptr<Control> control=std::make_shared<Control>();std::filesystem::path dir;std::unique_ptr<CameraWorker> worker;
    void start(CameraWorker::StarAnalyzer analyzer={}){dir=std::filesystem::temp_directory_path()/("astro-worker-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
        Config c;c.tickMs=2;c.liveViewMs=10;c.retryDelayMs=10;c.reconnectDelayMs=100;c.reconnectAttempts=3;
        worker=std::make_unique<CameraWorker>(mailbox,c,dir/"user.json",std::move(analyzer),[state=control](const Config&,Log){return std::make_unique<WorkerCamera>(state);});mailbox.send({Action::Connect});}
    bool wait(std::function<bool()> condition){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);do{if(condition())return true;std::this_thread::sleep_for(std::chrono::milliseconds(5));}while(std::chrono::steady_clock::now()<end);return false;}
    void TearDown()override{worker.reset();if(!dir.empty())std::filesystem::remove_all(dir);}
};
TEST_F(WorkerTest,LiveViewStopsBeforeShutterAndResumesOnlyAfterDownload){
    start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));
    mailbox.send({Action::LiveView,{},1});ASSERT_TRUE(wait([&]{return mailbox.snapshot().liveView;}));
    Command capture;capture.action=Action::CaptureSingle;capture.config.outputDirectory=dir;capture.config.singleCapture.delaySeconds=0;
    mailbox.send(capture);ASSERT_TRUE(wait([&]{return control->captures==1;}));
    EXPECT_FALSE(control->captureWhileLive);EXPECT_FALSE(mailbox.snapshot().liveView);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));EXPECT_FALSE(mailbox.snapshot().liveView);
    control->captureComplete=true;
    ASSERT_TRUE(wait([&]{auto s=mailbox.snapshot();return s.progress.completed==1&&s.liveView;}));
    EXPECT_TRUE(mailbox.snapshot().connected);
}
TEST_F(WorkerTest,PointAfRunsOnCoreThreadAndRejectsCaptureUntilStopped){
    start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().frame.revision>0;}));mailbox.send({Action::StartPointFocus});
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().pointAutofocus.active();}));
    EXPECT_EQ(control->pointThread,mailbox.snapshot().coreThreadId);
    const auto capture=mailbox.submit({Action::CaptureSingle});ASSERT_TRUE(wait([&]{auto r=mailbox.result(capture);return r&&r->finished;}));EXPECT_FALSE(mailbox.result(capture)->error.empty());EXPECT_EQ(control->captures,0);
    mailbox.send({Action::StopPointFocus});ASSERT_TRUE(wait([&]{return mailbox.snapshot().pointAutofocus.phase==PointAfPhase::Cancelled;}));EXPECT_EQ(control->pointStops,1);EXPECT_TRUE(mailbox.snapshot().liveView);
}
TEST_F(WorkerTest,PointAfConfirmationIsClearedByLensMovement){
    start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().frame.revision>0;}));mailbox.send({Action::StartPointFocus});control->pointDone=true;
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().pointAutofocus.phase==PointAfPhase::Complete;}));
    mailbox.send({Action::Focus,{},4});ASSERT_TRUE(wait([&]{return mailbox.snapshot().pointAutofocus.phase==PointAfPhase::Idle;}));
}
TEST_F(WorkerTest,QueuedPointAfRejectsEveryConflictingCameraAction){
    start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().frame.revision>0;}));
    const auto first=mailbox.submit({Action::StartPointFocus});
    std::vector<std::uint64_t> rejected;
    for(auto action:{Action::StartPointFocus,Action::StartStarFocus,Action::Focus,Action::Zoom,Action::SetProperty,Action::Start,Action::CaptureSingle})rejected.push_back(mailbox.submit({action}));
    ASSERT_TRUE(wait([&]{auto r=mailbox.result(rejected.back());return r&&r->finished;}));
    EXPECT_TRUE(mailbox.result(first)->error.empty());
    for(auto id:rejected)EXPECT_FALSE(mailbox.result(id)->error.empty());
    EXPECT_EQ(control->captures,0);EXPECT_TRUE(mailbox.snapshot().pointAutofocus.active());
    mailbox.send({Action::Stop});ASSERT_TRUE(wait([&]{return !mailbox.snapshot().pointAutofocus.active();}));
    EXPECT_TRUE(mailbox.snapshot().liveView);
}
TEST_F(WorkerTest,CaptureWithoutLiveViewDoesNotEnableItAfterDownload){
    start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));control->captureComplete=true;
    Command capture;capture.action=Action::CaptureSingle;capture.config.outputDirectory=dir;capture.config.singleCapture.delaySeconds=0;
    mailbox.send(capture);ASSERT_TRUE(wait([&]{return mailbox.snapshot().progress.completed==1;}));
    EXPECT_FALSE(mailbox.snapshot().liveView);EXPECT_FALSE(control->captureWhileLive);
}
TEST_F(WorkerTest,TemporaryPumpAndFrameErrorsKeepTheSession){start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});ASSERT_TRUE(wait([&]{return control->frames>2;}));control->pumpFault=1;control->frameFault=1;const int before=control->frames;ASSERT_TRUE(wait([&]{return control->frames>before+3;}));EXPECT_TRUE(mailbox.snapshot().connected);EXPECT_TRUE(mailbox.snapshot().liveView);EXPECT_EQ(control->connections,1);EXPECT_EQ(control->closes,0);}
TEST_F(WorkerTest,LogFileUsesCurrentLanguageWithoutWorkerRestart){
    start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));
    mailbox.setLogLanguage("en");Command en;en.action=Action::SaveConfig;en.config.language="en";
    const auto first=mailbox.submit(en);ASSERT_TRUE(wait([&]{return mailbox.result(first)->finished;}));
    mailbox.setLogLanguage("it");Command it;it.action=Action::SaveConfig;
    const auto second=mailbox.submit(it);ASSERT_TRUE(wait([&]{return mailbox.result(second)->finished;}));
    std::ifstream file(dir/"astro.log");const std::string text{std::istreambuf_iterator<char>(file),{}};
    EXPECT_NE(text.find("[Info] Configuration saved: "),std::string::npos);
    EXPECT_NE(text.find("[Informazioni] Configurazione salvata: "),std::string::npos);
    EXPECT_EQ(control->connections,1);
    // Keep original messages in memory so old rows can be rendered in either language.
    EXPECT_TRUE(mailbox.snapshot().logs.back().message.starts_with("Configurazione salvata: "));
}
TEST_F(WorkerTest,IdleStarStatisticsDoNotAnalyzeEveryPreviewFrame){
    std::atomic<int> analyses{};
    start([&](const LiveFrame&,double,const std::vector<StarSample>&,double){++analyses;return StarField{};});
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});
    ASSERT_TRUE(wait([&]{return control->frames>=20;}));
    EXPECT_GE(analyses.load(),1);EXPECT_LT(analyses.load(),control->frames.load()/2);
    const auto before=analyses.load();mailbox.send({Action::Zoom,{},5});
    ASSERT_TRUE(wait([&]{return analyses.load()>before;}));
}
TEST_F(WorkerTest,ActiveAutofocusStillAnalyzesEveryFrame){
    std::atomic<int> analyses{};
    start([&](const LiveFrame&,double,const std::vector<StarSample>&,double){++analyses;StarField field;field.width=64;field.height=48;field.focusContrast=10;field.focusSignature.resize(64*48);return field;});
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().frame.revision>0;}));mailbox.send({Action::StartStarFocus});
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().autofocus.active();}));
    const int firstFrame=control->frames.load(),firstAnalysis=analyses.load();
    ASSERT_TRUE(wait([&]{return control->frames>=firstFrame+8;}));
    EXPECT_TRUE(mailbox.snapshot().autofocus.active());EXPECT_GE(analyses.load()-firstAnalysis,7);
}
TEST_F(WorkerTest,AnalysisFailureDoesNotDisconnectCamera){start([](const LiveFrame&,double,const std::vector<StarSample>&,double)->StarField{throw std::runtime_error("Invalid JPEG");});ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});ASSERT_TRUE(wait([&]{return control->frames>4;}));EXPECT_TRUE(mailbox.snapshot().connected);EXPECT_TRUE(mailbox.snapshot().liveView);EXPECT_EQ(control->closes,0);}
TEST_F(WorkerTest,TransportLossReconnectsWithoutRestartingCaptureOrLiveView){start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});ASSERT_TRUE(wait([&]{return control->frames>2;}));control->pumpFault=2;ASSERT_TRUE(wait([&]{return control->connections==2&&mailbox.snapshot().connected;}));EXPECT_FALSE(mailbox.snapshot().liveView);EXPECT_EQ(control->captures,0);}
TEST_F(WorkerTest,ExplicitDisconnectCancelsPendingRecovery){start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));control->unavailable=true;control->pumpFault=2;ASSERT_TRUE(wait([&]{return !mailbox.snapshot().connected;}));mailbox.send({Action::Disconnect});std::this_thread::sleep_for(std::chrono::milliseconds(350));EXPECT_EQ(control->connections,1);}
TEST_F(WorkerTest,RecoveryAttemptsAreBoundedAndConnectCanRetry){start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));control->unavailable=true;control->pumpFault=2;ASSERT_TRUE(wait([&]{return control->connections==4;}));std::this_thread::sleep_for(std::chrono::milliseconds(450));EXPECT_EQ(control->connections,4);EXPECT_FALSE(mailbox.snapshot().connected);control->unavailable=false;mailbox.send({Action::Connect});ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));EXPECT_EQ(control->connections,5);}
TEST_F(WorkerTest,RemoteCommandFailureIsAcknowledgedOnTheCoreThread){
    start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));
    EXPECT_NE(mailbox.snapshot().coreThreadId,std::hash<std::thread::id>{}(std::this_thread::get_id()));
    const auto id=mailbox.submit({Action::StartStarFocus});
    ASSERT_TRUE(wait([&]{return mailbox.result(id)->finished;}));EXPECT_FALSE(mailbox.result(id)->error.empty());
    const auto refreshId=mailbox.submit({Action::Refresh});ASSERT_TRUE(wait([&]{return mailbox.result(refreshId)->finished;}));EXPECT_TRUE(mailbox.result(refreshId)->error.empty());
}
TEST_F(WorkerTest,ShutdownInterruptsLongConfiguredPollingWait){
    dir=std::filesystem::temp_directory_path()/("astro-worker-stop-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
    Config c;c.tickMs=10000;
    worker=std::make_unique<CameraWorker>(mailbox,c,dir/"user.json",CameraWorker::StarAnalyzer{},[state=control](const Config&,Log){return std::make_unique<WorkerCamera>(state);});
    ASSERT_TRUE(wait([&]{return mailbox.snapshot().coreTick>0;}));const auto before=std::chrono::steady_clock::now();worker.reset();
    EXPECT_LT(std::chrono::steady_clock::now()-before,std::chrono::seconds(2));
}
TEST_F(WorkerTest,ConnectedIdleCameraInhibitsSleepAndDisconnectReleasesIt){
    start();ASSERT_TRUE(wait([&]{auto s=mailbox.snapshot();return s.connected&&s.sleepInhibited;}));
    EXPECT_FALSE(mailbox.snapshot().liveView);
    mailbox.send({Action::Disconnect});ASSERT_TRUE(wait([&]{auto s=mailbox.snapshot();return !s.connected&&!s.sleepInhibited;}));
    mailbox.send({Action::Connect});ASSERT_TRUE(wait([&]{return mailbox.snapshot().sleepInhibited;}));
    worker.reset();EXPECT_FALSE(mailbox.snapshot().sleepInhibited);EXPECT_FALSE(mailbox.snapshot().connected);
}
TEST_F(WorkerTest,LostConnectionReleasesPowerRequestWhileWaitingForUsb){
    start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().sleepInhibited;}));
    control->unavailable=true;control->pumpFault=2;
    ASSERT_TRUE(wait([&]{auto s=mailbox.snapshot();return s.reconnecting&&!s.connected&&!s.sleepInhibited;}));
}
TEST(ConnectionPower,NativeRequestIsIdempotentAndReusable){
    ConnectionPower power([](const std::string&,const std::string&){});
    EXPECT_FALSE(power.active());power.setConnected(false);
    for(int i=0;i<20;++i){power.setConnected(true);power.setConnected(true);EXPECT_TRUE(power.active());power.setConnected(false);power.setConnected(false);EXPECT_FALSE(power.active());}
}
}
