/* Tests adapted in part from Hocus Focus WeightRegularizationTests and
 * AlglibHyperbolicFittingHuberTests. Copyright (c) 2021-2026 George Hilios.
 * Mozilla Public License 2.0; see licenses/hocus-focus/LICENSE.txt.
 */
#include <astro/HocusFocusMath.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
using namespace astro::hocus;
namespace {
std::vector<Point> curve(double center=.4){
    std::vector<Point> p;for(int i=-6;i<=6;++i)p.push_back({double(i),.5+2./1.8*std::hypot(i-center,1.8),.1});return p;
}
TEST(HocusFocusMath,RegularizesUnknownAndDegenerateSigmaWithoutChangingMeasurements){
    std::vector<Point> p{{0,2,.001},{1,3,.2},{2,4,.3},{3,5,.4},{4,6,.5}};
    auto r=regularizeWeights(p);EXPECT_NEAR(r[0].sigma,.06,1e-12);
    for(std::size_t i=0;i<p.size();++i){EXPECT_EQ(r[i].x,p[i].x);EXPECT_EQ(r[i].y,p[i].y);}EXPECT_EQ(p[0].sigma,.001);
    p[0].sigma=std::numeric_limits<double>::quiet_NaN();r=regularizeWeights(p);EXPECT_NEAR(r[0].sigma,.35,1e-12);
}
TEST(HocusFocusMath,RecoversHyperbolicMinimumAndLeaveOneOutStability){
    auto p=curve();auto f=fitHyperbola(p);ASSERT_TRUE(f.valid);EXPECT_NEAR(f.minimum,.4,.01);EXPECT_NEAR(f.value,2.5,.01);
    EXPECT_GT(f.rSquared,.999);EXPECT_EQ(f.leaveOneOutSolved,p.size());EXPECT_LT(f.leaveOneOutSigma,.02);
    for(auto v:p)EXPECT_NEAR(evaluate(f,v.x),v.y,.01);
}
TEST(HocusFocusMath,FitIsInvariantToInputOrderAndPositionOrigin){
    auto p=curve();for(auto& v:p)v.x+=100000;std::reverse(p.begin(),p.end());auto f=fitHyperbola(p);
    ASSERT_TRUE(f.valid);EXPECT_NEAR(f.minimum,100000.4,.01);
}
TEST(HocusFocusMath,HuberProtectsMinimumFromGrossWingOutliers){
    auto p=curve();p[1].y+=6;p[3].y+=4;auto f=fitHyperbola(p,false);ASSERT_TRUE(f.valid);EXPECT_NEAR(f.minimum,.4,.4);
    EXPECT_LT(f.rSquared,.95); // Robust location does not hide poor goodness of fit.
}
TEST(HocusFocusMath,FlatMonotoneInvalidAndDuplicatePositionsCannotInventMinimum){
    auto p=curve();for(auto& v:p)v.y=3;EXPECT_FALSE(fitHyperbola(p).valid);
    for(auto& v:p)v.y=10+v.x;auto f=fitHyperbola(p);EXPECT_TRUE(!f.valid||f.rSquared<.8);
    p=curve();p[1].x=p[0].x;EXPECT_FALSE(fitHyperbola(p).valid);
    p=curve();p[1].y=std::numeric_limits<double>::quiet_NaN();EXPECT_FALSE(fitHyperbola(p).valid);
}
TEST(HocusFocusMath,RobustBackgroundFitsGradientDespiteContaminatedAnnulus){
    std::vector<BackgroundPixel> p;for(int y=-14;y<=14;++y)for(int x=-14;x<=14;++x)if(x*x+y*y>=121&&x*x+y*y<=196)p.push_back({double(x),double(y),40+.3*x-.2*y});
    for(std::size_t i=0;i<p.size();i+=17)p[i].value+=100;
    auto b=fitBackground(p);ASSERT_TRUE(b.valid);EXPECT_NEAR(b.offset,40,.1);EXPECT_NEAR(b.dx,.3,.01);EXPECT_NEAR(b.dy,-.2,.01);
}
TEST(HocusFocusMath,DegenerateBackgroundDoesNotReportValidPlane){
    std::vector<BackgroundPixel> p(20,{0,0,40});EXPECT_FALSE(fitBackground(p).valid);
}
}
