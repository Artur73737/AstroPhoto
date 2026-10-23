#include "ImageAnalysis.hpp"
#include "FocusPeaking.hpp"
#include <gtest/gtest.h>
#include <numeric>
#include <cmath>
#include <random>
namespace {
astro::PreviewPixels detail(double blur=1) {
    astro::PreviewPixels p{96,96,std::vector<std::uint8_t>(96*96*4,255),{}};
    for(int y=0;y<96;++y)for(int x=0;x<96;++x){const auto v=static_cast<std::uint8_t>(80+100/(1+std::exp(-(x-48)/blur)));for(int c=0;c<3;++c)p.rgba[(y*96+x)*4+c]=v;}return p;
}
TEST(ImageHistogram,CountsEveryChannelWithoutChangingPixels) {
    const astro::PreviewPixels p{2,1,{255,0,0,255,0,255,0,255},{}};const auto h=astro::imageHistogram(p);
    EXPECT_EQ(h.pixels,2);EXPECT_EQ(h.bins[0][255],1);EXPECT_EQ(h.bins[1][255],1);EXPECT_EQ(h.bins[2][0],2);
    EXPECT_EQ(h.bins[3][54],1);EXPECT_EQ(h.bins[3][182],1);
    for(const auto& bins:h.bins)EXPECT_EQ(std::accumulate(bins.begin(),bins.end(),0ull),2);
    EXPECT_THROW(astro::imageHistogram({}),std::invalid_argument);
}
TEST(PointFocus,SharpnessFallsWithDefocusAndRoiStaysInsideEdges) {
    const astro::FocusTarget target{true,.5f,.5f,1};const auto sharp=detail(.8),soft=detail(6);
    const auto overlay=astro::focusPeaking(sharp,10);const auto a=astro::measureFocus(sharp,target,&overlay),b=astro::measureFocus(soft,target);
    EXPECT_TRUE(a.valid);EXPECT_TRUE(b.valid);EXPECT_GT(a.score,b.score);EXPECT_GT(a.peakingFraction,0);
    for(float u:{0.f,1.f})for(float v:{0.f,1.f}){const auto r=astro::measureFocus(sharp,{true,u,v,1});EXPECT_GE(r.x,1);EXPECT_GE(r.y,1);EXPECT_LE(r.x+r.width,sharp.width-1);EXPECT_LE(r.y+r.height,sharp.height-1);}
    auto flat=sharp;std::fill(flat.rgba.begin(),flat.rgba.end(),128);EXPECT_FALSE(astro::measureFocus(flat,target).valid);
    std::fill(flat.rgba.begin(),flat.rgba.end(),255);EXPECT_FALSE(astro::measureFocus(flat,target).valid);
}
TEST(PointFocus,RequiresSweepAndReturnNeverAcceptsStationaryImage) {
    astro::FocusTracker tracker;astro::FocusReading r;r.valid=true;r.score=10;
    for(int i=1;i<=40;++i)EXPECT_FALSE(tracker.update(r,i,1).nearBest);
    tracker.reset();std::uint64_t frame{};astro::FocusReading out;
    for(double score:{2.,4.,7.,10.,12.,12.,12.,12.,12.,12.,10.,8.,5.,3.,3.,3.,3.,3.}){r.score=score;out=tracker.update(r,++frame,1);}
    EXPECT_TRUE(out.bracketed);EXPECT_FALSE(out.nearBest);
    r.score=12;for(int i=0;i<18;++i)out=tracker.update(r,++frame,1);
    EXPECT_TRUE(out.nearBest);
    const auto samples=out.samples;EXPECT_EQ(tracker.update(r,frame,1).samples,samples);
    out=tracker.update(r,++frame,2);EXPECT_FALSE(out.nearBest);EXPECT_EQ(out.samples,1);
    r.valid=false;out=tracker.update(r,++frame,2);EXPECT_FALSE(out.valid);EXPECT_FALSE(out.nearBest);
}
TEST(PointFocus,CoverageIsIndependentOfFocusScoreAndUpdatesOnSameFrame) {
    const auto image=detail();const astro::FocusTarget target{true,.5f,.5f,1};
    const auto off=astro::measureFocus(image,target);EXPECT_FALSE(off.peakingMeasured);
    auto overlay=image;const auto i=(48*image.width+48)*4;overlay.rgba[i]=255;overlay.rgba[i+1]=0;overlay.rgba[i+2]=255;
    const auto sparse=astro::measureFocus(image,target,&overlay);EXPECT_TRUE(sparse.peakingMeasured);EXPECT_EQ(sparse.peakingPixels,1);EXPECT_DOUBLE_EQ(sparse.peakingFraction,1./4096);EXPECT_DOUBLE_EQ(sparse.score,off.score);
    astro::FocusTracker tracker;const auto before=tracker.update(off,10,1),after=tracker.update(sparse,10,1);
    EXPECT_EQ(after.samples,before.samples);EXPECT_DOUBLE_EQ(after.score,before.score);EXPECT_EQ(after.peakingPixels,1);
    const auto empty=astro::measureFocus(image,target,&image);EXPECT_TRUE(empty.peakingMeasured);EXPECT_EQ(empty.peakingPixels,0);
}
TEST(PointFocus,DefocusedNoiseMustNotBeatSharpDetail) {
    const astro::FocusTarget target{true,.5f,.5f,1};const auto sharp=astro::measureFocus(detail(.8),target);
    auto flat=detail();std::mt19937 rng(417);std::normal_distribution<double> noise(0,3);
    for(std::size_t i=0;i<flat.rgba.size();i+=4){const auto value=static_cast<std::uint8_t>(std::clamp(std::lround(128+noise(rng)),0L,255L));for(int c=0;c<3;++c)flat.rgba[i+c]=value;}
    const auto blurred=astro::measureFocus(flat,target);
    EXPECT_FALSE(blurred.valid);EXPECT_LT(blurred.score,sharp.score);
    astro::FocusTracker tracker;astro::FocusReading out;
    for(int i=1;i<=12;++i)out=tracker.update(sharp,i,1);
    const auto best=out.best;
    for(int i=13;i<=40;++i){out=tracker.update(blurred,i,1);EXPECT_LE(out.best,best);EXPECT_FALSE(out.nearBest);}
}
TEST(PointFocus,SingleSharpnessSpikeMustNotChangeConfirmedBest) {
    astro::FocusTracker tracker;astro::FocusReading r;r.valid=true;r.score=10;astro::FocusReading out;
    for(int i=1;i<=10;++i)out=tracker.update(r,i,1);
    const auto best=out.best;r.score=1000;out=tracker.update(r,11,1);
    EXPECT_DOUBLE_EQ(out.best,best);r.score=10;
    for(int i=12;i<=20;++i){out=tracker.update(r,i,1);EXPECT_DOUBLE_EQ(out.best,best);EXPECT_FALSE(out.nearBest);}
}
TEST(PointFocus,ProgressiveDefocusLowersDetailAndDoesNotRaiseStoredPeak) {
    const astro::FocusTarget target{true,.5f,.5f,1};astro::FocusTracker tracker;std::uint64_t frame{};
    auto previous=astro::measureFocus(detail(.8),target);astro::FocusReading out;
    for(int i=0;i<8;++i)out=tracker.update(previous,++frame,1);
    const double best=out.best;
    for(double blur:{1.5,3.,6.,12.,24.,48.}) {
        auto current=astro::measureFocus(detail(blur),target);
        EXPECT_LT(current.score,previous.score);previous=current;
        for(int i=0;i<6;++i){out=tracker.update(current,++frame,1);EXPECT_DOUBLE_EQ(out.best,best);EXPECT_FALSE(out.nearBest);}
    }
    auto flat=detail();std::fill(flat.rgba.begin(),flat.rgba.end(),128);out=tracker.update(astro::measureFocus(flat,target),++frame,1);
    EXPECT_FALSE(out.valid);EXPECT_DOUBLE_EQ(out.best,best);EXPECT_FALSE(out.nearBest);
}
TEST(PointFocus,ConstantFluxStarLosesSharpnessWhenItsLightSpreads) {
    const astro::FocusTarget target{true,.5f,.5f,1};double previous=1e9;
    for(double sigma:{1.,1.5,2.,3.,4.,6.}) {
        astro::PreviewPixels p{96,96,std::vector<std::uint8_t>(96*96*4,255),{}};
        for(int y=0;y<96;++y)for(int x=0;x<96;++x){double radius=(x-48)*(x-48)+(y-48)*(y-48);auto light=static_cast<std::uint8_t>(20+180/(sigma*sigma)*std::exp(-radius/(2*sigma*sigma)));for(int c=0;c<3;++c)p.rgba[(y*96+x)*4+c]=light;}
        const auto r=astro::measureFocus(p,target);EXPECT_LE(r.score,previous);previous=r.score;
    }
}
}
