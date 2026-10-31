#include <astro/StarAutofocus.hpp>
#include <astro/StarFocusRun.hpp>
#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
using namespace astro;
namespace {
class Clock final:public IClock{public:SteadyTime now{};SteadyTime steadyNow()const override{return now;}std::chrono::system_clock::time_point utcNow()const override{return {};}void advance(){now+=std::chrono::milliseconds(100);}};
// Domain-only motor test double; no SDK emulation or production fallback.
class Motor final:public ICamera{
public:
    int position{},commands{};std::array<int,4> stepSizes{0,1,2,3};bool online{true},af{true},manual{true},ambiguous{},ignoreSmall{};std::vector<int> directions;
    bool bounded{},ignoreAll{};
    std::string connect()override{online=true;return "domain-test";}void disconnect()override{online=false;}bool connected()const override{return online;}void pump()override{}
    std::vector<CameraProperty> properties()override{return {};}
    void set(Property,std::uint32_t)override{throw std::logic_error("unexpected property write");}
    void prepareHost(std::uint64_t,SaveDestination)override{throw std::logic_error("unexpected storage write");}
    void beginCapture(const SequenceItem&,const std::filesystem::path&)override{throw std::logic_error("unexpected shutter");}
    CaptureResult pollCapture()override{return {};}void abortCapture()override{}void setLiveView(bool)override{}LiveFrame liveFrame()override{return {};}
    void focus(int step)override{++commands;if(!ignoreAll&&(!ignoreSmall||std::abs(step)>1))position+=(step>0?1:-1)*stepSizes[std::abs(step)];if(bounded)position=std::clamp(position,-9,9);directions.push_back(step);if(ambiguous)throw CameraError("ambiguous move",true);}
    LensFocusStatus lensFocusStatus()override{return {af?LensFocusState::Ready:LensFocusState::ManualFocus,{}};}
    bool manualExposureForFocus()override{return manual;}void zoom(int)override{throw std::logic_error("unexpected zoom");}
};
StarField field(double hfd){StarField f;f.width=500;f.height=200;for(int i=0;i<5;++i)f.stars.push_back({40.+i*80,80,hfd,1000,50,.9,120});return f;}
struct FocusTest:testing::Test{
    Clock clock;Motor motor;StarAutofocus focus{motor,clock};std::uint64_t frame{};
    void start(){StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;o.searchEnabled=false;o.searchStep=1;o.maxPasses=1;focus.start(o);}
    void tick(double hfd){auto f=field(hfd);clock.advance();focus.tick(&f,++frame);}
    void run(bool flat=false,bool badReturn=false){for(int i=0;i<2000&&focus.progress().active();++i){double hfd=flat?3:2+.12*motor.position*motor.position;if(badReturn&&focus.progress().phase==StarFocusPhase::Verifying)hfd*=2;tick(hfd);}}
};
TEST_F(FocusTest,BracketsSameStarsReturnsFromNearAndVerifies){start();run();const auto& p=focus.progress();ASSERT_EQ(p.phase,StarFocusPhase::Complete)<<p.detail;EXPECT_GE(p.curve.size(),3);EXPECT_NEAR(p.best,2,1e-6);EXPECT_NEAR(p.verified,2,1e-6);EXPECT_EQ(motor.position,0);EXPECT_EQ(motor.directions.back(),1);}
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
TEST_F(FocusTest,MinimumOnBoundaryIsRejected){start();for(int i=0;i<2000&&focus.progress().active();++i)tick(2+.1*std::pow(motor.position-50,2));EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(focus.progress().verified,0);}
TEST_F(FocusTest,VariableFramesAreRejected){start();for(int i=0;i<300&&focus.progress().active();++i)tick(i%2?2:5);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,0);}
TEST_F(FocusTest,TransientBrightDetectionDoesNotPoisonCohort){start();for(int i=0;i<2000&&focus.progress().active();++i){auto f=field(2+.12*motor.position*motor.position);if(i%3==0)f.stars.push_back({30,30,1,1000,1000,.9,120});clock.advance();focus.tick(&f,++frame);}EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;}
TEST_F(FocusTest,MovementBudgetStopsBeforeExcessCommands){StarFocusOptions o;o.positions=21;o.settleMs=200;o.framesPerPoint=3;o.maxMoves=20;focus.start(o);run(true);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_LE(motor.commands,20);EXPECT_GT(motor.commands,0);}
TEST_F(FocusTest,ManualSwitchAndDisconnectStopSweep){start();while(motor.commands<1)tick(3);motor.af=false;run();EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,1);motor.af=true;start();motor.online=false;tick(3);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);}
TEST_F(FocusTest,AcquiresStarsFromZeroAndFocusesTowardInfinity){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=field(2+.08*std::pow(motor.position-18,2));if(motor.position<12||motor.position>30)f.stars.clear();clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_GT(focus.progress().searchMoves,0);EXPECT_NEAR(motor.position,18,1);EXPECT_LE(motor.commands,o.maxMoves);
}
TEST_F(FocusTest,SearchReversesAndFindsStarsOnTheNearSide){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=field(2+.08*std::pow(motor.position+18,2));if(motor.position>-12||motor.position<-32)f.stars.clear();clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_GT(focus.progress().searchMoves,o.searchSpan);EXPECT_NEAR(motor.position,-18,1);
}
TEST_F(FocusTest,EmptyFieldSearchIsBoundedAndNeverSucceeds){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;o.searchSpan=4;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=field(3);f.stars.clear();clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,3*o.searchSpan);EXPECT_EQ(focus.progress().verified,0);
}
TEST_F(FocusTest,SearchStopsIfFramesStopArriving){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    while(motor.commands<1){auto f=field(3);f.stars.clear();clock.advance();focus.tick(&f,++frame);}
    for(int i=0;i<200;++i){clock.advance();focus.tick();}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,1);
}
TEST_F(FocusTest,ShallowCurveWidensThenRefinesAndVerifies){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i)tick(4+.003*motor.position*motor.position);
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_GE(focus.progress().pass,3);EXPECT_EQ(focus.progress().motorStep,1);EXPECT_NEAR(motor.position,0,1);EXPECT_NEAR(focus.progress().verified,4,.01);
}
TEST_F(FocusTest,AdaptiveRecenteringBracketsAnOffCenterMinimum){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i)tick(2+.1*std::pow(motor.position-5,2));
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_GE(focus.progress().pass,2);EXPECT_EQ(motor.position,5);
}
TEST_F(FocusTest,FlatCurveAfterAllAdaptationsStillFails){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i)tick(3);
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_GT(focus.progress().stride,1);EXPECT_EQ(focus.progress().verified,0);EXPECT_LE(motor.commands,o.maxMoves);
}
TEST_F(FocusTest,SearchCancellationAndMfPreventFurtherMovement){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    while(motor.commands<1){auto f=field(3);f.stars.clear();clock.advance();focus.tick(&f,++frame);}
    focus.cancel();for(int i=0;i<100;++i)tick(3);EXPECT_EQ(motor.commands,1);
    focus.start(o);motor.af=false;for(int i=0;i<200&&focus.progress().active();++i){auto f=field(3);f.stars.clear();clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,1);
}

TEST_F(FocusTest,ReturnPositionErrorIsRemeasuredAndRecovered){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);bool displaced=false;
    for(int i=0;i<6000&&focus.progress().active();++i){if(!displaced&&focus.progress().phase==StarFocusPhase::Verifying){motor.position+=3;displaced=true;}tick(2+.15*motor.position*motor.position);}
    EXPECT_TRUE(displaced);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_GT(focus.progress().pass,1);EXPECT_EQ(motor.position,0);EXPECT_NEAR(focus.progress().verified,2,1e-6);
}
TEST_F(FocusTest,ReportedFlatMonitorCurveRequestsAWiderScan){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    const double measured[]{4.088819,4.013418,3.983893,4.072849,4.078939,4.180406,4.099408,4.163500,4.122802};
    for(int i=0;i<2000&&focus.progress().active()&&focus.progress().pass<2;++i)tick(measured[std::clamp(motor.position+4,0,8)]);
    EXPECT_TRUE(focus.progress().active());EXPECT_EQ(focus.progress().pass,2);EXPECT_EQ(focus.progress().motorStep,3);EXPECT_EQ(focus.progress().verified,0);
}

TEST_F(FocusTest,CoarseDetectionsDoNotBypassMeasuredLargeStage){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=field(2+.08*std::pow(motor.position-18,2));if(motor.position<12||motor.position>30)f.stars.clear();if(motor.position>=3&&motor.position<12)f.acquisitionCandidates=5;clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_NE(std::find(motor.directions.begin(),motor.directions.end(),2),motor.directions.end());EXPECT_NEAR(motor.position,18,1);
}

TEST_F(FocusTest,UnresponsiveSmallestDriveCannotClaimVerifiedFineFocus){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;motor.position=5;motor.ignoreSmall=true;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i)tick(2+.15*motor.position*motor.position);
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed)<<focus.progress().detail;EXPECT_GT(focus.progress().pass,1);EXPECT_GT(focus.progress().stageBest[1],0);EXPECT_EQ(focus.progress().stageBest[2],0);EXPECT_LE(motor.commands,o.maxMoves);
}
TEST_F(FocusTest,SingleTemporalOutlierWaitsForCleanSamples){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);bool spike=false;
    for(int i=0;i<6000&&focus.progress().active();++i){double hfd=2+.12*motor.position*motor.position;if(!spike&&focus.progress().frames==2){hfd=10;spike=true;}tick(hfd);}
    EXPECT_TRUE(spike);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;
}

TEST_F(FocusTest,DefaultCommandsStartLargeThenDescendThroughMediumToFine){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);run();
    ASSERT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;
    ASSERT_FALSE(motor.directions.empty());EXPECT_EQ(std::abs(motor.directions.front()),3);
    int previous=3;bool medium=false,small=false;for(int command:motor.directions){EXPECT_LE(std::abs(command),previous);previous=std::abs(command);medium|=previous==2;small|=previous==1;}
    EXPECT_TRUE(medium);EXPECT_TRUE(small);EXPECT_EQ(motor.position,0);
    for(double best:focus.progress().stageBest)EXPECT_NEAR(best,2,1e-6);
}
TEST_F(FocusTest,LegacyStepSettingsCannotSkipAnyTier){
    StarFocusOptions o;o.searchStep=1;o.step=3;o.settleMs=200;o.framesPerPoint=3;focus.start(o);run();
    ASSERT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;
    int previous=3;bool medium=false,small=false;
    for(int command:motor.directions){EXPECT_LE(std::abs(command),previous);previous=std::abs(command);medium|=previous==2;small|=previous==1;}
    EXPECT_TRUE(medium);EXPECT_TRUE(small);for(double best:focus.progress().stageBest)EXPECT_GT(best,0);
}
TEST_F(FocusTest,EachTierReturnsToItsMeasuredBestBeforeStartingTheNext){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);int previous=3;
    for(int i=0;i<6000&&focus.progress().active();++i){
        tick(2+.12*std::pow(motor.position-5,2));
        const auto& p=focus.progress();
        if(p.motorStep!=previous){
            EXPECT_EQ(p.motorStep,previous-1);EXPECT_LE(std::abs(motor.position-5),previous);
            EXPECT_NEAR(p.stageBest[3-previous],2+.12*std::pow(motor.position-5,2),1e-6);previous=p.motorStep;
        }
    }
    ASSERT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;
    EXPECT_EQ(motor.position,5);EXPECT_NEAR(focus.progress().stageBest[2],2,1e-6);
}
TEST_F(FocusTest,FailedLargeStageReturnNeverPromotesToMedium){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){
        double hfd=2+.12*motor.position*motor.position;
        if(focus.progress().phase==StarFocusPhase::Reanchoring)hfd*=2;
        tick(hfd);
    }
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);
    EXPECT_EQ(focus.progress().stageBest[0],0);EXPECT_EQ(focus.progress().stageBest[1],0);
    for(int command:motor.directions)EXPECT_EQ(std::abs(command),3);
}
TEST_F(FocusTest,ConsecutiveFarSideSamplesDoNotReverseTheLens){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    int pass=-1,position=0;std::size_t commands=0;bool checked=false;
    for(int i=0;i<6000&&focus.progress().active();++i){
        tick(2+.004*motor.position*motor.position);const auto& p=focus.progress();
        if(p.phase!=StarFocusPhase::Sampling||(p.pass==pass&&p.position==position))continue;
        if(p.pass==pass&&position>0&&p.position>position){
            for(std::size_t j=commands;j<motor.directions.size();++j)EXPECT_GT(motor.directions[j],0);checked=true;
        }
        pass=p.pass;position=p.position;commands=motor.directions.size();
    }
    EXPECT_TRUE(checked);
}
TEST_F(FocusTest,RetryCannotForgetABetterMinimumFromAnEarlierTier){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){
        const double deterioration=focus.progress().motorStep==1?.7:0;
        tick(2+.12*std::pow(motor.position-5,2)+deterioration);
    }
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);
    EXPECT_GT(focus.progress().stageBest[1],0);EXPECT_EQ(focus.progress().stageBest[2],0);
}
TEST_F(FocusTest,LargeDriveLosesStarsButImmediatelyReturnsAndRefines){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=field(2+.15*motor.position*motor.position);if(std::abs(motor.position)>3)f.stars.clear();clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_EQ(motor.position,0);EXPECT_LE(motor.commands,65);
}
TEST_F(FocusTest,FiveFinePointsCannotConstrainAFourParameterMinimumReliably){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=field(2+.15*motor.position*motor.position);if(std::abs(motor.position)>2)f.stars.clear();clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(focus.progress().stageBest[2],0);EXPECT_TRUE(focus.progress().restored);EXPECT_EQ(motor.position,0);
}
TEST_F(FocusTest,FittedCurveContainsOnlyMeasurementsApproachedTowardInfinity){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);bool checked=false;
    for(int i=0;i<6000&&focus.progress().active();++i){
        const bool near=!motor.directions.empty()&&motor.directions.back()<0;
        tick(2+.12*motor.position*motor.position+(near?.6:0));
        const auto& p=focus.progress();
        if(p.fitValid&&p.motorStep==1){
            const int origin=motor.position-p.position;
            for(auto point:p.curve)EXPECT_NEAR(point.hfd,2+.12*std::pow(point.position+origin,2),1e-6);
            checked=true;
        }
    }
    EXPECT_TRUE(checked);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;
}
TEST_F(FocusTest,NoiseLimitedFineResponseWidensSpacingWithoutReturningToLargeDrive){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;o.maxPasses=8;focus.start(o);bool fine=false,widened=false;
    for(int i=0;i<6000&&focus.progress().active();++i){
        const bool small=focus.progress().motorStep==1;
        tick(small?2+.0001*motor.position*motor.position+(i%3-1)*.04:2+.12*motor.position*motor.position);
        if(small){fine=true;if(focus.progress().stride>=4){widened=true;break;}}
    }
    EXPECT_TRUE(fine);EXPECT_TRUE(widened);EXPECT_EQ(focus.progress().motorStep,1);EXPECT_EQ(focus.progress().stageBest[2],0);
    focus.cancel();
}
StarField redundantField(double blur){auto f=field(blur);for(int i=0;i<5;++i)f.stars.push_back({40.+i*80,130,blur+.15*i,1000,45,.9,120});return f;}
TEST_F(FocusTest,MissingOneOfTheBrightestStarsDoesNotDiscardTheEntireFrame){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=redundantField(2+.15*motor.position*motor.position);if(focus.progress().pass>0)f.stars.erase(f.stars.begin()+i%5);clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_EQ(motor.position,0);EXPECT_GE(focus.progress().stars,5);
}
TEST_F(FocusTest,DifferentIntrinsicStarSizesCannotCreateAFakeFocusMinimum){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=redundantField(3);for(std::size_t j=0;j<f.stars.size();++j)f.stars[j].hfd=2+.4*j;
        if(focus.progress().pass>0){if(motor.position==0)f.stars.resize(6);else f.stars.erase(f.stars.begin(),f.stars.begin()+4);}
        clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(focus.progress().verified,0);
}
TEST_F(FocusTest,UnmeasurableNeighborsRestoreBestWithoutClaimingSuccess){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=field(2);if(motor.position!=0)f.stars.clear();clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(focus.progress().verified,0);EXPECT_EQ(motor.position,0);EXPECT_TRUE(focus.progress().restored);
}
TEST_F(FocusTest,WaitsForCommonStarsInsteadOfAcceptingTheFirstCompletedSubset){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=redundantField(2+.15*motor.position*motor.position);
        if(focus.progress().pass==0&&focus.progress().frames>0)f.stars.resize(5);
        if(focus.progress().phase==StarFocusPhase::Sampling&&i%2)f.stars.erase(f.stars.begin(),f.stars.begin()+5);
        clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_EQ(motor.position,0);
}
TEST_F(FocusTest,RegistersCoherentLensBreathingAndCommonBrightnessChanges){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=redundantField(2+.15*motor.position*motor.position);
        for(auto& s:f.stars){s.x=250+(s.x-250)*(1+.006*motor.position)+1.8*motor.position;s.y=100+(s.y-100)*(1+.006*motor.position);s.flux*=motor.position==0?1:1.7;}
        clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;EXPECT_EQ(motor.position,0);
}
TEST_F(FocusTest,AcquisitionHintsCannotSkipTheLargeStage){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i){auto f=field(2+.08*std::pow(motor.position-18,2));if(motor.position<12||motor.position>30)f.stars.clear();if(motor.position>=3&&motor.position<12)f.acquisitionCandidates=2;clock.advance();focus.tick(&f,++frame);}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;
    ASSERT_GE(motor.directions.size(),2);EXPECT_EQ(motor.directions[0],3);EXPECT_EQ(motor.directions[1],3);EXPECT_NEAR(motor.position,18,1);
}

TEST_F(FocusTest,LostStarsAtAnInexactCoarseReturnAreRecoveredBeforeRefining){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);bool displaced=false;
    for(int i=0;i<6000&&focus.progress().active();++i){
        if(!displaced&&focus.progress().phase==StarFocusPhase::Reanchoring){motor.position+=6;displaced=true;}
        auto f=field(2+.15*motor.position*motor.position);if(std::abs(motor.position)>3)f.stars.clear();
        clock.advance();focus.tick(&f,++frame);
    }
    EXPECT_TRUE(displaced);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;
    EXPECT_EQ(motor.position,0);for(auto value:focus.progress().stageBest)EXPECT_GT(value,0);
    EXPECT_LE(motor.commands,o.maxMoves);
}
TEST_F(FocusTest,MissingFramesAtReturnNeverCauseBlindRecoveryMoves){
    start();while(focus.progress().active()&&focus.progress().phase!=StarFocusPhase::Reanchoring)tick(2+.12*motor.position*motor.position);
    ASSERT_EQ(focus.progress().phase,StarFocusPhase::Reanchoring);const auto count=motor.commands;
    for(int i=0;i<150;++i){clock.advance();focus.tick();}
    EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,count);
}
TEST_F(FocusTest,UnrecoverableReturnIsBoundedAndNeverPromoted){
    StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);bool lost=false;
    for(int i=0;i<6000&&focus.progress().active();++i){
        lost|=focus.progress().phase==StarFocusPhase::Reanchoring;
        auto f=field(2+.12*motor.position*motor.position);if(lost)f.stars.clear();clock.advance();focus.tick(&f,++frame);
    }
    EXPECT_TRUE(lost);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);EXPECT_LE(motor.commands,o.maxMoves);
    for(auto value:focus.progress().stageBest)EXPECT_EQ(value,0);
}
TEST_F(FocusTest,DriveMagnitudesNeedNotHaveAThreeTwoOnePhysicalRatio){
    motor.stepSizes={0,1,10,50};StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    for(int i=0;i<6000&&focus.progress().active();++i)tick(2+.0005*motor.position*motor.position);
    ASSERT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;
    EXPECT_LE(std::abs(motor.position),1);for(auto value:focus.progress().stageBest)EXPECT_GT(value,0);
}
StarField travelField(int position){
    auto f=field(2+.12*position*position);f.focusContrast=12;
    for(int i=0;i<64;++i)f.focusSignature.push_back(std::sin(i*.2+position*.07));return f;
}
TEST(FocusHoming,CalibratesBothEndsAndAlwaysResetsNearFromDifferentStarts){
    for(int initial:{-9,0,9}){
        Clock clock;Motor motor;motor.bounded=true;motor.position=initial;FocusHoming homing(motor,clock);
        StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;homing.start(o);
        for(int i=0;i<4000&&homing.progress().active();++i){auto f=travelField(motor.position);clock.advance();homing.tick(&f,i+1);}
        ASSERT_EQ(homing.progress().phase,StarFocusPhase::Complete)<<homing.progress().detail;
        EXPECT_TRUE(homing.progress().nearLimitKnown);EXPECT_TRUE(homing.progress().farLimitKnown);EXPECT_TRUE(homing.progress().homed);
        EXPECT_EQ(homing.progress().coarseRange,6);EXPECT_EQ(motor.position,-9);EXPECT_EQ(motor.directions.front(),-3);
    }
}
TEST(FocusHoming,StationaryLensCannotMasqueradeAsAnEndpoint){
    Clock clock;Motor motor;motor.ignoreAll=true;FocusHoming homing(motor,clock);StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;homing.start(o);
    for(int i=0;i<2000&&homing.progress().active();++i){auto f=travelField(0);clock.advance();homing.tick(&f,i+1);}
    EXPECT_EQ(homing.progress().phase,StarFocusPhase::Failed);EXPECT_FALSE(homing.progress().nearLimitKnown);EXPECT_FALSE(homing.progress().homed);
    EXPECT_LE(motor.commands,o.homingStablePoints+o.homingProbeMoves);
}
TEST(FocusHoming,BlankFieldAndMissingFramesNeverCalibrateOrMoveBlindly){
    Clock clock;Motor motor;FocusHoming homing(motor,clock);StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;homing.start(o);
    for(int i=0;i<150;++i){auto f=travelField(0);f.focusContrast=0;clock.advance();homing.tick(&f,i+1);}
    EXPECT_EQ(homing.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,0);
    homing.start(o);for(int i=0;i<150;++i){clock.advance();homing.tick();}
    EXPECT_EQ(homing.progress().phase,StarFocusPhase::Failed);EXPECT_EQ(motor.commands,0);
}
TEST(FocusHoming,CancelAndBudgetBoundEveryMotorCommand){
    Clock clock;Motor motor;FocusHoming homing(motor,clock);StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;o.homingMaxMoves=20;homing.start(o);
    for(int i=0;i<3000&&homing.progress().active();++i){auto f=travelField(motor.position);clock.advance();homing.tick(&f,i+1);}
    EXPECT_EQ(homing.progress().phase,StarFocusPhase::Failed);EXPECT_LE(motor.commands,20);
    homing.start(o);homing.cancel();const int before=motor.commands;
    for(int i=0;i<100;++i){auto f=travelField(motor.position);clock.advance();homing.tick(&f,i+1);}
    EXPECT_EQ(motor.commands,before);EXPECT_FALSE(homing.progress().homed);
}
TEST(FocusHoming,SmallCumulativeMovementIsNotAnEndpointPlateau){
    Clock clock;Motor motor;FocusHoming homing(motor,clock);StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;homing.start(o);
    for(int i=0;i<3000&&homing.progress().active()&&motor.commands<20;++i){
        auto f=travelField(0);for(int j=0;j<64;++j)f.focusSignature[j]=std::sin(j*.2+motor.position*.0003);
        clock.advance();homing.tick(&f,i+1);
    }
    EXPECT_TRUE(homing.progress().active())<<homing.progress().detail;EXPECT_EQ(motor.commands,20);
    EXPECT_FALSE(homing.progress().nearLimitKnown);homing.cancel();
}
TEST(FocusHoming,QueuedReverseProbeRequiresNewFramesBetweenCommands){
    Clock clock;Motor motor;motor.bounded=true;FocusHoming homing(motor,clock);StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;homing.start(o);
    std::uint64_t revision=0;
    while(motor.directions.empty()||motor.directions.back()<0){auto f=travelField(motor.position);clock.advance();homing.tick(&f,++revision);ASSERT_TRUE(homing.progress().active());}
    const int before=motor.commands;
    for(int i=0;i<150;++i){auto f=travelField(motor.position);clock.advance();homing.tick(&f,revision);}
    EXPECT_EQ(motor.commands,before);EXPECT_EQ(homing.progress().phase,StarFocusPhase::Failed);EXPECT_FALSE(homing.progress().homed);
}
TEST_F(FocusTest,QueuedTravelDoesNotDrainWithoutFreshFrames){
    start();while(motor.commands<1)tick(2+.12*motor.position*motor.position);
    const int before=motor.commands;
    for(int i=0;i<150;++i){auto f=field(3);clock.advance();focus.tick(&f,frame);}
    EXPECT_EQ(motor.commands,before);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);
}
TEST(StarFocusRun,EveryRunCalibratesThenSweepsFromNearWithSharedBudgets){
    Clock clock;Motor motor;motor.bounded=true;StarFocusRun focus(motor,clock);StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;
    for(int run=0;run<2;++run){motor.position=run?6:-3;focus.start(o);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Homing);bool started=false;
        for(int i=0;i<6000&&focus.progress().active();++i){auto f=travelField(motor.position);clock.advance();focus.tick(&f,i+1);
            if(!started&&focus.progress().phase!=StarFocusPhase::Homing){started=true;EXPECT_EQ(motor.position,-9);EXPECT_TRUE(focus.progress().homed);}}
        ASSERT_TRUE(started);ASSERT_EQ(focus.progress().phase,StarFocusPhase::Complete)<<focus.progress().detail;
        EXPECT_LE(focus.progress().moves,o.maxMoves);EXPECT_GT(focus.progress().homingMoves,0);EXPECT_EQ(motor.position,0);
        focus.invalidateLimits();EXPECT_FALSE(focus.progress().homed);EXPECT_FALSE(focus.progress().nearLimitKnown);EXPECT_FALSE(focus.progress().farLimitKnown);
    }
}
TEST(StarFocusRun,ObservedEndpointStopsRefinementAndInvalidatesLimits){
    Clock clock;Motor motor;motor.bounded=true;StarFocusRun focus(motor,clock);StarFocusOptions o;o.settleMs=200;o.framesPerPoint=3;focus.start(o);
    bool reachedRefinement=false;
    for(int i=0;i<6000&&focus.progress().active();++i){
        auto f=travelField(motor.position);
        if(focus.progress().homed&&focus.progress().motorStep==2){reachedRefinement=true;f.focusSignature=travelField(-9).focusSignature;}
        clock.advance();focus.tick(&f,i+1);
    }
    ASSERT_TRUE(reachedRefinement);EXPECT_EQ(focus.progress().phase,StarFocusPhase::Failed);
    EXPECT_EQ(focus.progress().detail,"Calibrated optical travel boundary reached during refinement");
    EXPECT_FALSE(focus.progress().nearLimitKnown);EXPECT_FALSE(focus.progress().farLimitKnown);EXPECT_EQ(focus.progress().stageBest[2],0);
}



}
