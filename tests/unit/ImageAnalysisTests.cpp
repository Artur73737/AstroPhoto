#include "ImageAnalysis.hpp"
#include "FocusPeaking.hpp"
#include <gtest/gtest.h>
#include <numeric>
#include <cmath>
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
}
