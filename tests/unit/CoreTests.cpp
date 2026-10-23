#include <astro/Core.hpp>
#include <gtest/gtest.h>
#include <set>
using namespace astro;
namespace {
class TestClock final:public IClock {
public:
    SteadyTime now{};
    SteadyTime steadyNow()const override{return now;}
    std::chrono::system_clock::time_point utcNow()const override{return std::chrono::system_clock::time_point{std::chrono::seconds(1700000000)};}
    void advance(int ms){now+=std::chrono::milliseconds(ms);}
};
class MemoryFiles final:public IFileSystem {
public:
    std::set<std::filesystem::path> paths;std::uint64_t bytes=1024ULL*1024*1024;bool fail{};
    void createDirectories(const std::filesystem::path& p)override{if(fail)throw std::runtime_error("disk unavailable");paths.insert(p);}
    std::uint64_t available(const std::filesystem::path&)override{return bytes;}
    bool exists(const std::filesystem::path& p)const override{return paths.contains(p);}
};
// Domain test double only: never linked into the desktop application or hardware tests.
class TestCamera final:public ICamera {
public:
    bool online{true},done{},busy{},failCapture{},failAbort{},emptyResult{};int starts{},aborts{},sets{},prepared{};std::filesystem::path path;
    SaveDestination destination{SaveDestination::PcOnly};
    std::string connect()override{online=true;return "unit-test-only";}
    void disconnect()override{online=false;}
    bool connected()const override{return online;}
    void pump()override{}
    std::vector<CameraProperty> properties()override{return {};}
    void set(Property,std::uint32_t)override{++sets;if(busy)throw CameraError("busy",true);}
    void prepareHost(std::uint64_t,SaveDestination target)override{++prepared;destination=target;}
    void beginCapture(const SequenceItem&,const std::filesystem::path& p)override{++starts;path=p;if(failCapture)throw CameraError("ambiguous shutter error",true);}
    CaptureResult pollCapture()override{return {done,done&&!emptyResult?std::vector<std::filesystem::path>{path.string()+".CR2"}:std::vector<std::filesystem::path>{}};}
    void abortCapture()override{++aborts;if(failAbort)throw CameraError("abort failed");}
    void setLiveView(bool)override{}
    LiveFrame liveFrame()override{return {};}
    void focus(int)override{}
    LensFocusStatus lensFocusStatus()override{return {};}
    void zoom(int)override{}
};
struct EngineTest:testing::Test {
    TestClock clock;MemoryFiles files;TestCamera camera;std::vector<std::string> log;
    SequenceEngine engine{camera,files,clock,[&](const std::string&,const std::string& s){log.push_back(s);}};
    Config config;
    void SetUp()override{config.sequence={SequenceItem{}};}
};
TEST(FileOrganizer,SeparatesFrameTypesAndHasDeterministicUtcNames) {
    TestClock clock;MemoryFiles files;FileOrganizer organizer(files,clock);Config c;
    for(auto type:{FrameType::Light,FrameType::Dark,FrameType::Flat,FrameType::Bias}) {
        SequenceItem i;i.type=type;i.settings[Property::Iso]=72;
        auto p=organizer.stem(c,i,1);EXPECT_EQ(p.parent_path(),c.outputDirectory/c.sessionName/frameName(type));
        EXPECT_NE(p.filename().string().find("20231114T221320"),std::string::npos);EXPECT_NE(p.filename().string().find("ISO-0x48"),std::string::npos);
        EXPECT_NE(p,organizer.stem(c,i,2));
    }
}
TEST(FileOrganizer,RejectsTraversalAndWindowsReservedNames) {
    for(const auto* name:{"../escape","a/b","a\\b","CON","com1.raw","NUL","name.","name ",""}) {Config c;c.sessionName=name;EXPECT_THROW(validate(c),std::invalid_argument)<<name;}
}
TEST_F(EngineTest,CompletesOnlyAfterDownloadAndCountsAllRows) {
    config.sequence[0].count=2;auto dark=SequenceItem{};dark.type=FrameType::Dark;config.sequence.push_back(dark);
    engine.start(config);engine.tick();EXPECT_EQ(engine.progress().completed,0);EXPECT_EQ(camera.starts,1);
    camera.done=true;engine.tick();EXPECT_EQ(engine.progress().completed,1);
    engine.tick();engine.tick();EXPECT_EQ(engine.progress().completed,3);EXPECT_EQ(camera.starts,3);EXPECT_EQ(engine.progress().state,SequenceState::Idle);
}
TEST_F(EngineTest,DestinationAppliesToSequencesAndSingleShots) {
    config.saveDestination=SaveDestination::PcAndCamera;engine.start(config);
    EXPECT_EQ(camera.destination,SaveDestination::PcAndCamera);engine.stop();
    config.saveDestination=SaveDestination::PcOnly;engine.startSingle(config);
    EXPECT_EQ(camera.destination,SaveDestination::PcOnly);EXPECT_EQ(camera.prepared,2);
}
TEST_F(EngineTest,PauseWaitsForActiveDownloadThenResumes) {
    config.sequence[0].count=2;engine.start(config);engine.tick();engine.pause();engine.tick();EXPECT_EQ(engine.progress().state,SequenceState::Running);
    camera.done=true;engine.tick();engine.tick();EXPECT_EQ(engine.progress().state,SequenceState::Paused);EXPECT_EQ(camera.starts,1);
    engine.resume();engine.tick();EXPECT_EQ(camera.starts,2);EXPECT_EQ(engine.progress().state,SequenceState::Idle);
}
TEST_F(EngineTest,DelayUsesMonotonicClock) {
    config.sequence[0].delaySeconds=2;engine.start(config);engine.tick();EXPECT_EQ(camera.starts,0);clock.advance(1999);engine.tick();EXPECT_EQ(camera.starts,0);clock.advance(1);engine.tick();EXPECT_EQ(camera.starts,1);
}
TEST_F(EngineTest,IntervalStartsAfterDownloadAndAppliesAcrossRows) {
    config.sequence[0].intervalSeconds=2;config.sequence.push_back({});
    engine.start(config);engine.tick();EXPECT_EQ(camera.starts,1);
    clock.advance(10000);engine.tick();EXPECT_EQ(camera.starts,1);
    camera.done=true;engine.tick();EXPECT_EQ(engine.lastCapture().revision,1);
    engine.tick();EXPECT_EQ(camera.starts,1);clock.advance(1999);engine.tick();EXPECT_EQ(camera.starts,1);
    clock.advance(1);engine.tick();EXPECT_EQ(camera.starts,2);EXPECT_EQ(engine.progress().state,SequenceState::Idle);
}
TEST_F(EngineTest,InitialDelayIsNotRepeatedWithinRow) {
    config.sequence[0].count=2;config.sequence[0].delaySeconds=2;config.sequence[0].intervalSeconds=1;
    engine.start(config);engine.tick();clock.advance(2000);camera.done=true;engine.tick();EXPECT_EQ(camera.starts,1);
    clock.advance(1000);engine.tick();EXPECT_EQ(camera.starts,2);
}
TEST_F(EngineTest,SingleCaptureDoesNotRunOrModifySequencePlan) {
    config.sequence[0].count=10;config.singleCapture.type=FrameType::Dark;config.singleCapture.count=7;
    engine.startSingle(config);camera.done=true;engine.tick();engine.tick();
    EXPECT_EQ(camera.starts,1);EXPECT_EQ(engine.progress().total,1);EXPECT_EQ(config.sequence[0].count,10);
    EXPECT_EQ(engine.lastCapture().type,FrameType::Dark);ASSERT_EQ(engine.lastCapture().files.size(),1);
    EXPECT_EQ(engine.lastCapture().files[0].parent_path().filename(),"Dark");
    engine.startSingle(config);engine.tick();EXPECT_EQ(engine.lastCapture().revision,2);
}
TEST_F(EngineTest,StopDuringIntervalPreventsNextShot) {
    config.sequence[0].count=2;config.sequence[0].intervalSeconds=10;camera.done=true;
    engine.start(config);engine.tick();engine.tick();engine.stop();clock.advance(10000);engine.tick();EXPECT_EQ(camera.starts,1);EXPECT_EQ(camera.aborts,0);
}
TEST_F(EngineTest,PauseDuringDelayDoesNotFireShutter) {
    config.sequence[0].delaySeconds=2;engine.start(config);engine.tick();engine.pause();clock.advance(2000);engine.tick();
    EXPECT_EQ(camera.starts,0);EXPECT_EQ(engine.progress().state,SequenceState::Paused);
    engine.resume();engine.tick();EXPECT_EQ(camera.starts,1);
}
TEST_F(EngineTest,TransportFailureRemainsErrorAndAborts) {
    engine.start(config);engine.tick();engine.reportError("transfer failed");
    EXPECT_EQ(engine.progress().state,SequenceState::Error);EXPECT_EQ(camera.aborts,1);EXPECT_EQ(engine.progress().detail,"transfer failed");
}
TEST_F(EngineTest,StopAbortsBulbAndDoesNotStartNextShot) {
    config.sequence[0].bulb=true;config.sequence[0].count=2;engine.start(config);engine.tick();engine.stop();engine.tick();EXPECT_EQ(camera.aborts,1);EXPECT_EQ(camera.starts,1);EXPECT_EQ(engine.progress().state,SequenceState::Idle);
}
TEST_F(EngineTest,StopBeforeExposureDoesNotAbort) {engine.start(config);engine.stop();EXPECT_EQ(camera.aborts,0);}
TEST_F(EngineTest,TimeoutAbortsAndDoesNotCountFrame) {
    config.downloadTimeoutSeconds=1;engine.start(config);engine.tick();clock.advance(2001);engine.tick();EXPECT_EQ(camera.aborts,1);EXPECT_EQ(engine.progress().state,SequenceState::Error);EXPECT_EQ(engine.progress().completed,0);
}
TEST_F(EngineTest,RetryOnlyIdempotentConfigurationWithBackoff) {
    config.sequence[0].settings[Property::Iso]=72;camera.busy=true;engine.start(config);engine.tick();EXPECT_EQ(camera.sets,1);engine.tick();EXPECT_EQ(camera.sets,1);
    clock.advance(config.retryDelayMs);camera.busy=false;engine.tick();EXPECT_EQ(camera.sets,2);EXPECT_EQ(camera.starts,1);
}
TEST_F(EngineTest,BusyRetriesAreBounded) {
    config.sequence[0].settings[Property::Iso]=72;config.busyRetries=2;camera.busy=true;engine.start(config);
    for(int i=0;i<4;++i){engine.tick();clock.advance(1000);}EXPECT_EQ(camera.sets,3);EXPECT_EQ(engine.progress().state,SequenceState::Error);EXPECT_EQ(camera.starts,0);
}
TEST_F(EngineTest,NeverRetriesAmbiguousShutterCommand) {
    camera.failCapture=true;engine.start(config);engine.tick();clock.advance(10000);engine.tick();EXPECT_EQ(camera.starts,1);EXPECT_EQ(camera.aborts,1);EXPECT_EQ(engine.progress().state,SequenceState::Error);
}
TEST_F(EngineTest,AbortFailureRemainsVisible) {
    camera.failAbort=true;engine.start(config);engine.tick();engine.stop();EXPECT_EQ(engine.progress().state,SequenceState::Error);EXPECT_NE(engine.progress().detail.find("abort failed"),std::string::npos);
}
TEST_F(EngineTest,RejectsLowDiskBeforeStartingAndBetweenFrames) {
    files.bytes=0;EXPECT_THROW(engine.start(config),std::runtime_error);EXPECT_EQ(camera.starts,0);
    files.bytes=1024ULL*1024*1024;config.sequence[0].count=2;camera.done=true;engine.start(config);engine.tick();files.bytes=0;engine.tick();EXPECT_EQ(camera.starts,1);EXPECT_EQ(engine.progress().state,SequenceState::Error);
}
TEST_F(EngineTest,DisconnectAbortsCapture) {engine.start(config);engine.tick();camera.online=false;engine.tick();EXPECT_EQ(engine.progress().state,SequenceState::Error);EXPECT_EQ(camera.aborts,1);}
TEST_F(EngineTest,EmptyDownloadIsAnError) {camera.done=true;camera.emptyResult=true;engine.start(config);engine.tick();EXPECT_EQ(engine.progress().state,SequenceState::Error);EXPECT_EQ(engine.progress().completed,0);}
TEST_F(EngineTest,RejectsInvalidOrEmptySequence) {
    config.sequence.clear();EXPECT_THROW(engine.start(config),std::invalid_argument);
    config.sequence.push_back({});config.sequence[0].count=-1;EXPECT_THROW(engine.start(config),std::invalid_argument);
}
TEST_F(EngineTest,DiskFailureIsReportedBeforeCapture) {files.fail=true;EXPECT_THROW(engine.start(config),std::runtime_error);EXPECT_EQ(camera.starts,0);}
TEST(Mailbox,CommandsAreDrainedAndSnapshotsAreCopies) {
    Mailbox m;Command c;c.action=Action::Connect;m.send(c);EXPECT_EQ(m.take().size(),1);EXPECT_TRUE(m.take().empty());Snapshot s;s.model="Canon";m.publish(s);s.model="Changed";EXPECT_EQ(m.snapshot().model,"Canon");
}
}
