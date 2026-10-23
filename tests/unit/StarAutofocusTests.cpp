#include <astro/StarAutofocus.hpp>
#include <gtest/gtest.h>
#include <cmath>
using namespace astro;
namespace {
class Clock final:public IClock{public:SteadyTime now{};SteadyTime steadyNow()const override{return now;}std::chrono::system_clock::time_point utcNow()const override{return {};}void advance(){now+=std::chrono::milliseconds(100);}};
// Domain-only motor test double; no SDK emulation or production fallback.
class Motor final:public ICamera{
public:
    int position{},commands{};bool online{true},af{true},manual{true},ambiguous{};std::vector<int> directions;
    std::string connect()override{online=true;return "domain-test";}void disconnect()override{online=false;}bool connected()const override{return online;}void pump()override{}
    std::vector<CameraProperty> properties()override{return {};}
    void set(Property,std::uint32_t)override{throw std::logic_error("unexpected property write");}
    void prepareHost(std::uint64_t,SaveDestination)override{throw std::logic_error("unexpected storage write");}
    void beginCapture(const SequenceItem&,const std::filesystem::path&)override{throw std::logic_error("unexpected shutter");}
    CaptureResult pollCapture()override{return {};}void abortCapture()override{}void setLiveView(bool)override{}LiveFrame liveFrame()override{return {};}
    void focus(int step)override{++commands;position+=step;directions.push_back(step);if(ambiguous)throw CameraError("ambiguous move",true);}
    LensFocusStatus lensFocusStatus()override{return {af?LensFocusState::Ready:LensFocusState::ManualFocus,{}};}
    bool manualExposureForFocus()override{return manual;}void zoom(int)override{throw std::logic_error("unexpected zoom");}
};
StarField field(double hfd){StarField f;f.width=500;f.height=200;for(int i=0;i<5;++i)f.stars.push_back({40.+i*80,80,hfd,1000,50,.9,120});return f;}
struct FocusTest:testing::Test{
    Clock clock;Motor motor;StarAutofocus focus{motor,clock};std::uint64_t frame{};
    void start(){StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);}
    void tick(double hfd){auto f=field(hfd);clock.advance();focus.tick(&f,++frame);}
    void run(bool flat=false,bool badReturn=false){for(int i=0;i<2000&&focus.progress().active();++i){double hfd=flat?3:2+.12*motor.position*motor.position;if(badReturn&&focus.progress().phase==StarFocusPhase::Verifying)hfd*=2;tick(hfd);}}
};
TEST_F(FocusTest,BracketsSameStarsReturnsFromNearAndVerifies){start();run();const auto& p=focus.progress();ASSERT_EQ(p.phase,StarFocusPhase::Complete)<<p.detail;EXPECT_EQ(p.curve.size(),9);EXPECT_NEAR(p.best,2,1e-6);EXPECT_NEAR(p.verified,2,1e-6);EXPECT_EQ(motor.position,0);EXPECT_EQ(motor.directions.back(),1);EXPECT_GT(p.curveR2,.99);}
TEST_F(FocusTest,FlatCurveNeverDeclaresSuccess){start();run(true);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(focus.progress().verified,0);}
TEST_F(FocusTest,ReturnMustReproduceTheMinimum){start();run(false,true);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_GT(focus.progress().verified,focus.progress().best);}
TEST_F(FocusTest,AmbiguousMovementIsNeverRetried){start();motor.ambiguous=true;run();EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,1);}
TEST_F(FocusTest,CancelStopsFurtherMotorCommands){start();while(motor.commands<2)tick(3);focus.cancel();const auto count=motor.commands;for(int i=0;i<50;++i)tick(3);EXPECT_EQ(motor.commands,count);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Cancelled);}
TEST_F(FocusTest,RejectsMissingLensAfAndAutomaticExposure){motor.af=false;
    EXPECT_THROW(start(),CameraError);motor.af=true;motor.manual=false;
    EXPECT_THROW(start(),CameraError);}
TEST_F(FocusTest,RepeatedOrMissingFramesCannotCompleteBaseline){start();auto f=field(3);for(int i=0;i<150;++i){clock.advance();focus.tick(&f,1);}EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,0);}
TEST_F(FocusTest,LostCohortCannotBeReplacedBySharperOtherStars){start();while(focus.progress().frames<1)tick(3);auto f=field(1);for(auto& s:f.stars)s.y+=30;for(int i=0;i<150;++i){clock.advance();focus.tick(&f,++frame);}EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,0);}
TEST_F(FocusTest,ClippedOrLostFieldNeverMovesLens){start();StarField empty;empty.width=500;empty.height=200;for(int i=0;i<150;++i){clock.advance();focus.tick(&empty,++frame);}EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,0);}
TEST_F(FocusTest,MinimumOnBoundaryIsRejected){start();for(int i=0;i<2000&&focus.progress().active();++i)tick(2+.1*std::pow(motor.position-5,2));EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(focus.progress().verified,0);}
TEST_F(FocusTest,VariableFramesAreRejected){start();for(int i=0;i<300&&focus.progress().active();++i)tick(i%2?2:5);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,0);}
TEST_F(FocusTest,TransientBrightDetectionDoesNotPoisonCohort){start();for(int i=0;i<2000&&focus.progress().active();++i){auto f=field(2+.12*motor.position*motor.position);if(i%3==0)f.stars.push_back({30,30,1,1000,1000,.9,120});clock.advance();focus.tick(&f,++frame);}EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;}
TEST_F(FocusTest,MovementBudgetStopsBeforeExcessCommands){StarFocusOptions o;o.positions=21;o.settleMs=200;o.framesPerPoint=3;o.maxMoves=20;focus.start(o);run();EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,20);}
TEST_F(FocusTest,ManualSwitchAndDisconnectStopSweep){start();while(motor.commands<1)tick(3);motor.af=false;run();EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,1);motor.af=true;start();motor.online=false;tick(3);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);}
}
