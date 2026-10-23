#include <astro/Infrastructure.hpp>
#include <gtest/gtest.h>
#include <atomic>
using namespace astro;
namespace {
struct Control {std::atomic<int> connections{},closes{},frames{},captures{},pumpFault{},frameFault{};std::atomic<bool> unavailable{};};
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
    void beginCapture(const SequenceItem&,const std::filesystem::path&)override{++c->captures;}
    CaptureResult pollCapture()override{return {};}
    void abortCapture()override{disconnect();}
    void setLiveView(bool enabled)override{live=enabled;}
    LiveFrame liveFrame()override{if(c->frameFault.exchange(0))throw CameraError("Frame not ready",true);if(!live)return {};++c->frames;return {{1},++revision};}
    void focus(int)override{}
    LensFocusStatus lensFocusStatus()override{return {LensFocusState::Ready,{}};}
    bool manualExposureForFocus()override{return true;}
    void zoom(int)override{}
};
struct WorkerTest:testing::Test {
    Mailbox mailbox;std::shared_ptr<Control> control=std::make_shared<Control>();std::filesystem::path dir;std::unique_ptr<CameraWorker> worker;
    void start(CameraWorker::StarAnalyzer analyzer={}){dir=std::filesystem::temp_directory_path()/("astro-worker-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directories(dir);
        Config c;c.tickMs=2;c.liveViewMs=10;c.retryDelayMs=10;c.reconnectDelayMs=100;c.reconnectAttempts=3;
        worker=std::make_unique<CameraWorker>(mailbox,c,dir/"user.json",std::move(analyzer),[state=control](const Config&,Log){return std::make_unique<WorkerCamera>(state);});mailbox.send({Action::Connect});}
    bool wait(std::function<bool()> condition){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);do{if(condition())return true;std::this_thread::sleep_for(std::chrono::milliseconds(5));}while(std::chrono::steady_clock::now()<end);return false;}
    void TearDown()override{worker.reset();if(!dir.empty())std::filesystem::remove_all(dir);}
};
TEST_F(WorkerTest,TemporaryPumpAndFrameErrorsKeepTheSession){start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});ASSERT_TRUE(wait([&]{return control->frames>2;}));control->pumpFault=1;control->frameFault=1;const int before=control->frames;ASSERT_TRUE(wait([&]{return control->frames>before+3;}));EXPECT_TRUE(mailbox.snapshot().connected);EXPECT_TRUE(mailbox.snapshot().liveView);EXPECT_EQ(control->connections,1);EXPECT_EQ(control->closes,0);}
TEST_F(WorkerTest,AnalysisFailureDoesNotDisconnectCamera){start([](const LiveFrame&,double)->StarField{throw std::runtime_error("Invalid JPEG");});ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});ASSERT_TRUE(wait([&]{return control->frames>4;}));EXPECT_TRUE(mailbox.snapshot().connected);EXPECT_TRUE(mailbox.snapshot().liveView);EXPECT_EQ(control->closes,0);}
TEST_F(WorkerTest,TransportLossReconnectsWithoutRestartingCaptureOrLiveView){start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));mailbox.send({Action::LiveView,{},1});ASSERT_TRUE(wait([&]{return control->frames>2;}));control->pumpFault=2;ASSERT_TRUE(wait([&]{return control->connections==2&&mailbox.snapshot().connected;}));EXPECT_FALSE(mailbox.snapshot().liveView);EXPECT_EQ(control->captures,0);}
TEST_F(WorkerTest,ExplicitDisconnectCancelsPendingRecovery){start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));control->unavailable=true;control->pumpFault=2;ASSERT_TRUE(wait([&]{return !mailbox.snapshot().connected;}));mailbox.send({Action::Disconnect});std::this_thread::sleep_for(std::chrono::milliseconds(350));EXPECT_EQ(control->connections,1);}
TEST_F(WorkerTest,RecoveryAttemptsAreBoundedAndConnectCanRetry){start();ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));control->unavailable=true;control->pumpFault=2;ASSERT_TRUE(wait([&]{return control->connections==4;}));std::this_thread::sleep_for(std::chrono::milliseconds(450));EXPECT_EQ(control->connections,4);EXPECT_FALSE(mailbox.snapshot().connected);control->unavailable=false;mailbox.send({Action::Connect});ASSERT_TRUE(wait([&]{return mailbox.snapshot().connected;}));EXPECT_EQ(control->connections,5);}
}
