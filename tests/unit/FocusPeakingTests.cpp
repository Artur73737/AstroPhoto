#include "FocusPeaking.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <cmath>
namespace {
astro::PreviewPixels edge(bool blurred) {
    astro::PreviewPixels p{80,60,std::vector<std::uint8_t>(80*60*4,255),{}};
    for(int y=0;y<p.height;++y)for(int x=0;x<p.width;++x) {
        auto value=static_cast<std::uint8_t>(blurred?std::clamp((x-20)*255/40,0,255):(x<40?0:255));
        auto i=(y*p.width+x)*4;for(int c=0;c<3;++c)p.rgba[i+c]=value;
    }return p;
}
int marked(const astro::PreviewPixels& p) {
    int count{};for(std::size_t i=0;i<p.rgba.size();i+=4)if(p.rgba[i]==255&&p.rgba[i+1]==0&&p.rgba[i+2]==255)++count;return count;
}
TEST(FocusPeaking,HighlightsSharpEdgesAndLeavesBlurredEdgesUnmarked) {
    auto sharp=edge(false),soft=edge(true);auto result=astro::focusPeaking(sharp,35);
    EXPECT_GT(marked(result),40);EXPECT_LT(marked(result),120);EXPECT_EQ(marked(astro::focusPeaking(soft,35)),0);
    EXPECT_EQ(marked(sharp),0); // source is not modified
    for(std::size_t i=3;i<result.rgba.size();i+=4)EXPECT_EQ(result.rgba[i],sharp.rgba[i]);
}
TEST(FocusPeaking,RejectsInvalidInputsAndDoesNotMarkFlatOrTinyImages) {
    auto p=edge(false);std::fill(p.rgba.begin(),p.rgba.end(),128);
    EXPECT_EQ(astro::focusPeaking(p,35).rgba,p.rgba);
    astro::PreviewPixels tiny{1,1,{20,30,40,255},{}};EXPECT_EQ(astro::focusPeaking(tiny,35).rgba,tiny.rgba);
    EXPECT_THROW(astro::focusPeaking(p,std::numeric_limits<float>::quiet_NaN()),std::invalid_argument);
    EXPECT_THROW(astro::focusPeaking(p,0),std::invalid_argument);
    p.rgba.pop_back();EXPECT_THROW(astro::focusPeaking(p,35),std::invalid_argument);
}
TEST(FocusPeaking,ThresholdFiltersLowContrastEdges) {
    auto p=edge(false);for(std::size_t i=0;i<p.rgba.size();i+=4)for(int c=0;c<3;++c)p.rgba[i+c]/=4;
    EXPECT_GT(marked(astro::focusPeaking(p,10)),0);
    EXPECT_EQ(marked(astro::focusPeaking(p,100)),0);
}
astro::PreviewPixels star(double sigma,double peak) {
    astro::PreviewPixels p{64,64,std::vector<std::uint8_t>(64*64*4,255),{}};
    for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
        const double radius=(x-32)*(x-32)+(y-32)*(y-32);
        const auto value=static_cast<std::uint8_t>(std::clamp(8+peak*std::exp(-radius/(2*sigma*sigma)),0.0,255.0));
        for(int c=0;c<3;++c)p.rgba[(y*64+x)*4+c]=value;
    }return p;
}
TEST(StarPeaking,FindsCompactSourcesButRejectsDefocusHotPixelsAndClipping) {
    auto sharp=star(1.2,180);auto result=astro::focusPeaking(sharp,35,true);
    EXPECT_GT(marked(result),0);EXPECT_EQ(result.rgba[(32*64+32)*4],sharp.rgba[(32*64+32)*4]);
    EXPECT_EQ(marked(astro::focusPeaking(star(4,180),35,true)),0);
    EXPECT_EQ(marked(astro::focusPeaking(star(0.2,180),35,true)),0);
    EXPECT_EQ(marked(astro::focusPeaking(star(1.2,255),35,true)),0);
    EXPECT_EQ(marked(astro::focusPeaking(edge(false),35,true)),0);
    EXPECT_EQ(marked(sharp),0);
}
TEST(StarPeaking,ThresholdControlsFaintStarDetection) {
    const auto faint=star(1.2,50);
    EXPECT_GT(marked(astro::focusPeaking(faint,10,true)),0);
    EXPECT_EQ(marked(astro::focusPeaking(faint,100,true)),0);
}
}
