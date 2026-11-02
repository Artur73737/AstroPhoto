#include <astro/Core.hpp>
#include <astro/PointFocusRun.hpp>
#include <astro/LiveStackRun.hpp>
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
    std::vector<std::filesystem::path> sources;
    std::vector<std::filesystem::path> rawFiles(const std::filesystem::path&)const override{return sources;}
};
// Domain test double only: never linked into the desktop application or hardware tests.
class TestCamera final:public ICamera {
public:
    bool online{true},done{},busy{},failCapture{},failAbort{},emptyResult{};int starts{},aborts{},sets{},prepared{};std::filesystem::path path;
    SaveDestination destination{SaveDestination::PcOnly};
    std::string connect()override{online=true;return "unit-test-only";}
    bool manualExposureForFocus()override{return true;}
    void disconnect()override{online=false;}
    bool connected()const override{return online;}
    void pump()override{}
    std::vector<CameraProperty> props;std::vector<CameraProperty> properties()override{return props;}
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
    PointAfStatus point;int pointStarts{},pointStops{};bool failPointStop{};
    void startPointAutofocus(double u,double v)override{++pointStarts;point={PointAfPhase::Running,u,v,{}};}
    PointAfStatus pollPointAutofocus()override{return point;}
    void stopPointAutofocus()override{++pointStops;if(failPointStop)throw CameraError("AF stop failed");}
};
struct EngineTest:testing::Test {
    TestClock clock;MemoryFiles files;TestCamera camera;std::vector<std::string> log;
    SequenceEngine engine{camera,files,clock,[&](const std::string&,const std::string& s){log.push_back(s);}};
    Config config;
    void SetUp()override{config.sequence={SequenceItem{}};}
};
TEST(PointAutofocus,WaitsForConfirmationAndStopsBeforeReportingOk){
    TestClock clock;TestCamera camera;PointFocusRun run(camera,clock);run.start(.2,.7);
    run.tick();EXPECT_TRUE(run.status().active());EXPECT_EQ(camera.pointStarts,1);
    camera.point.phase=PointAfPhase::Complete;run.tick();EXPECT_EQ(run.status().phase,PointAfPhase::Complete);EXPECT_EQ(camera.pointStops,1);EXPECT_DOUBLE_EQ(run.status().u,.2);
}
TEST(PointAutofocus,TimeoutNeverReportsSuccess){
    TestClock clock;TestCamera camera;PointFocusRun run(camera,clock);run.start(.5,.5,3);clock.advance(3000);run.tick();
    EXPECT_EQ(run.status().phase,PointAfPhase::Failed);EXPECT_EQ(camera.pointStops,1);
}
TEST(PointAutofocus,CancelDisconnectAndStopFailureCannotReportOk){
    TestClock clock;TestCamera camera;PointFocusRun run(camera,clock);run.start(.5,.5);run.cancel();EXPECT_EQ(run.status().phase,PointAfPhase::Cancelled);
    run.start(.5,.5);camera.online=false;run.tick();EXPECT_EQ(run.status().phase,PointAfPhase::Failed);
    camera.online=true;run.start(.5,.5);camera.point.phase=PointAfPhase::Complete;camera.failPointStop=true;run.tick();EXPECT_EQ(run.status().phase,PointAfPhase::Failed);EXPECT_FALSE(camera.connected());
}
TEST(PointAutofocus,RejectsInvalidCoordinatesAndOverlappingRuns){
    TestClock clock;TestCamera camera;PointFocusRun run(camera,clock);
    EXPECT_THROW(run.start(NAN,.5),std::invalid_argument);
    EXPECT_THROW(run.start(1.1,.5),std::invalid_argument);
    EXPECT_THROW(run.start(.5,.5,61),std::invalid_argument);EXPECT_EQ(camera.pointStarts,0);
    run.start(.5,.5);
EXPECT_THROW(run.start(.2,.3),CameraError);EXPECT_EQ(camera.pointStarts,1);
}
TEST(FileOrganizer,SeparatesFrameTypesAndHasDeterministicUtcNames) {
    TestClock clock;MemoryFiles files;FileOrganizer organizer(files,clock);Config c;
    for(auto type:{FrameType::Light,FrameType::Dark,FrameType::Flat,FrameType::Bias}) {
        SequenceItem i;i.type=type;i.settings[Property::Iso]=72;
        auto p=organizer.stem(c,i,1);EXPECT_EQ(p.parent_path(),organizer.session(c)/frameName(type));
        EXPECT_NE(p.filename().string().find("20231114T221320"),std::string::npos);EXPECT_EQ(p.filename().string().find("0x"),std::string::npos);EXPECT_EQ(organizer.session(c).parent_path().filename(),"2023-11-14");
        EXPECT_NE(p,organizer.stem(c,i,2));
    }
}
TEST(FileOrganizer,RejectsTraversalAndWindowsReservedNames) {
    for(const auto* name:{"../escape","a/b","a\\b","CON","com1.raw","NUL","name.","name ",""}) {Config c;c.sessionName=name;
EXPECT_THROW(validate(c),std::invalid_argument)<<name;}
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
    files.bytes=0;
EXPECT_THROW(engine.start(config),std::runtime_error);EXPECT_EQ(camera.starts,0);
    files.bytes=1024ULL*1024*1024;config.sequence[0].count=2;camera.done=true;engine.start(config);engine.tick();files.bytes=0;engine.tick();EXPECT_EQ(camera.starts,1);EXPECT_EQ(engine.progress().state,SequenceState::Error);
}
TEST_F(EngineTest,DisconnectAbortsCapture) {engine.start(config);engine.tick();camera.online=false;engine.tick();EXPECT_EQ(engine.progress().state,SequenceState::Error);EXPECT_EQ(camera.aborts,1);}
TEST_F(EngineTest,EmptyDownloadIsAnError) {camera.done=true;camera.emptyResult=true;engine.start(config);engine.tick();EXPECT_EQ(engine.progress().state,SequenceState::Error);EXPECT_EQ(engine.progress().completed,0);}
TEST_F(EngineTest,RejectsInvalidOrEmptySequence) {
    config.sequence.clear();
EXPECT_THROW(engine.start(config),std::invalid_argument);
    config.sequence.push_back({});config.sequence[0].count=-1;
EXPECT_THROW(engine.start(config),std::invalid_argument);
}
TEST_F(EngineTest,DiskFailureIsReportedBeforeCapture) {files.fail=true;
EXPECT_THROW(engine.start(config),std::runtime_error);EXPECT_EQ(camera.starts,0);}
TEST(Mailbox,CommandsAreDrainedAndSnapshotsAreCopies) {
    Mailbox m;Command c;c.action=Action::Connect;m.send(c);EXPECT_EQ(m.take().size(),1);EXPECT_TRUE(m.take().empty());Snapshot s;s.model="Canon";m.publish(s);s.model="Changed";EXPECT_EQ(m.snapshot().model,"Canon");
}
class TestStackProcessor final:public IStackProcessor {
public:
    int submitted{},finished{};bool failBegin{};StackProcessResult result;
    void begin(const std::filesystem::path&,const StackOptions&,const std::string&)override{result={};result.ready=true;result.prepared=!failBegin;result.fatal=failBegin;}
    void submit(StackFrameJob)override{++submitted;}
    StackProcessResult poll()override{auto r=result;result={};return r;}
    void finish(const std::string&,bool)override{++finished;result={};result.ready=result.finalized=true;}
    void display(StackDisplay)override{}
    void ready(bool accepted=true){result={};result.ready=true;result.accepted=accepted;}
};
struct StackRunTest:testing::Test {
    TestClock clock;MemoryFiles files;TestCamera camera;TestStackProcessor processor;Config config;
    LiveStackRun run{camera,files,clock,processor,[](auto&,auto&){}};
    void SetUp()override{files.bytes=4ull*1024*1024*1024;config.liveStack.frames=2;camera.props={{Property::Iso,72,{},true,"100"},{Property::ExposureMode,3,{},false,"M - Manual"},{Property::ImageQuality,1,{},true,"RAW"},{Property::Shutter,1,{},true,"0.5 s"}};}
};
TEST_F(StackRunTest,SerializesExposureDownloadProcessingAndFixedDelay){
    run.start(config,"{}");run.tick();EXPECT_EQ(camera.starts,1);clock.advance(1000);run.tick();EXPECT_EQ(camera.starts,1);EXPECT_EQ(processor.submitted,0);
    camera.done=true;run.tick();EXPECT_EQ(processor.submitted,1);clock.advance(5000);run.tick();EXPECT_EQ(camera.starts,1);
    processor.ready();run.tick();EXPECT_EQ(run.status().accepted,1);clock.advance(2999);run.tick();EXPECT_EQ(camera.starts,1);clock.advance(1);run.tick();EXPECT_EQ(camera.starts,2);
    processor.ready();run.tick();run.tick();EXPECT_FALSE(run.status().active);EXPECT_EQ(run.status().accepted,2);EXPECT_EQ(camera.sets,0);
}
TEST_F(StackRunTest,StopWaitsForCurrentExposureAndDownloadWithoutAborting){
    run.start(config,"{}");run.tick();run.stop();run.tick();EXPECT_TRUE(run.status().active);EXPECT_EQ(camera.aborts,0);camera.done=true;run.tick();processor.ready();run.tick();run.tick();EXPECT_FALSE(run.status().active);EXPECT_EQ(camera.starts,1);EXPECT_EQ(camera.aborts,0);
}
TEST_F(StackRunTest,InfiniteCanStopDuringDelayAndRejectedFramesCountTowardLimit){
    config.liveStack.frames=0;run.start(config,"{}");run.tick();camera.done=true;run.tick();processor.ready();run.tick();run.stop();run.tick();run.tick();EXPECT_FALSE(run.status().active);EXPECT_EQ(camera.starts,1);
    config.liveStack.frames=1;run.start(config,"{}");run.tick();processor.ready(false);run.tick();run.tick();EXPECT_EQ(run.status().phase,"error");EXPECT_EQ(run.status().rejected,1);EXPECT_EQ(camera.starts,2);
}
TEST_F(StackRunTest,AmbiguousShutterErrorIsNeverRetried){camera.failCapture=true;run.start(config,"{}");run.tick();clock.advance(5000);run.tick();run.tick();EXPECT_EQ(camera.starts,1);EXPECT_EQ(camera.aborts,1);EXPECT_EQ(run.status().phase,"error");}
TEST_F(StackRunTest,RejectsChangedSettingsAndDiskExhaustionBeforeNextExposure){
    run.start(config,"{}");run.tick();camera.done=true;run.tick();processor.ready();run.tick();camera.props[0].value=80;clock.advance(3000);run.tick();run.tick();EXPECT_EQ(camera.starts,1);EXPECT_EQ(run.status().phase,"error");
    files.bytes=1;
EXPECT_THROW(run.start(config,"{}"),std::runtime_error);
}
TEST_F(StackRunTest,RequiresRawManualAndValidExposure){camera.props[2].currentLabel="JPEG";
EXPECT_THROW(run.start(config,"{}"),CameraError);camera.props[2].currentLabel="RAW";camera.props[3].currentLabel="Auto";
EXPECT_THROW(run.start(config,"{}"),CameraError);config.liveStack.delaySeconds=0;
EXPECT_THROW(run.start(config,"{}"),std::invalid_argument);EXPECT_EQ(camera.starts,0);}
TEST_F(StackRunTest,TimeoutAbortsOnceAndPersistsPreviousStack){run.start(config,"{}");run.tick();clock.advance(130000);run.tick();run.tick();EXPECT_EQ(camera.aborts,1);EXPECT_EQ(camera.starts,1);EXPECT_EQ(processor.finished,1);EXPECT_EQ(run.status().phase,"error");}
TEST_F(StackRunTest,PreparationFailureNeverReleasesShutter){processor.failBegin=true;run.start(config,"{}");run.tick();run.tick();EXPECT_EQ(camera.starts,0);EXPECT_EQ(run.status().phase,"error");}
TEST_F(StackRunTest,PauseCompletesCurrentFrameAndResumeNeverRepeatsIt){run.start(config,"{}");run.tick();run.pause();clock.advance(5000);run.tick();EXPECT_EQ(camera.starts,1);EXPECT_EQ(camera.aborts,0);camera.done=true;run.tick();processor.ready();run.tick();clock.advance(3000);run.tick();EXPECT_TRUE(run.status().paused);clock.advance(10000);run.tick();EXPECT_EQ(camera.starts,1);run.resume();run.tick();EXPECT_EQ(camera.starts,2);}
TEST_F(StackRunTest,StopWhilePausedSavesWithoutAnotherShutter){run.start(config,"{}");run.pause();run.tick();EXPECT_TRUE(run.status().paused);EXPECT_EQ(camera.starts,0);run.stop();run.tick();run.tick();EXPECT_FALSE(run.status().active);EXPECT_EQ(processor.finished,1);EXPECT_EQ(camera.starts,0);}
TEST_F(StackRunTest,RebuildNeedsNoCameraAndHonoursPauseWithoutShutter){
    camera.online=false;camera.props.clear();files.sources={"one.CR2","two.CR2"};config.liveStack.sourceDirectory="source";
    run.start(config,"{}");run.tick();EXPECT_EQ(processor.submitted,1);run.pause();processor.ready();run.tick();run.tick();EXPECT_TRUE(run.status().paused);EXPECT_EQ(camera.starts,0);run.resume();run.tick();EXPECT_EQ(processor.submitted,2);processor.ready();run.tick();run.tick();EXPECT_FALSE(run.status().active);EXPECT_EQ(run.status().accepted,2);EXPECT_EQ(camera.starts,0);
}
TEST_F(StackRunTest,HomogeneousPlanCountsRowsAndRejectsMixedSettingsBeforeCapture){
    config.liveStack.fromSequence=true;SequenceItem first;first.count=2;first.intervalSeconds=4;SequenceItem second=first;second.count=3;config.sequence={first,second};auto plan=stackPlan(config);EXPECT_EQ(plan.liveStack.frames,5);EXPECT_DOUBLE_EQ(plan.liveStack.delaySeconds,4);config.sequence[1].settings[Property::Iso]=99;
    EXPECT_THROW(stackPlan(config),std::invalid_argument);EXPECT_EQ(camera.starts,0);
}
}
