#include "StarAnalysis.hpp"
#include <gtest/gtest.h>
#include <cmath>
#include <random>
#include <algorithm>
namespace {
astro::PreviewPixels gaussian(double sigma,double amplitude=120,double dx=0,double dy=0,double elongation=1){
    astro::PreviewPixels p{96,96,std::vector<std::uint8_t>(96*96*4,255),{}};
    for(int y=0;y<96;++y)for(int x=0;x<96;++x){double r=std::pow((x-48-dx)/elongation,2)+std::pow(y-48-dy,2);auto v=static_cast<std::uint8_t>(std::clamp(std::lround(12+amplitude*std::exp(-r/(2*sigma*sigma))),0L,255L));for(int c=0;c<3;++c)p.rgba[(y*96+x)*4+c]=v;}return p;
}
TEST(StarAnalysis,HfdMatchesGaussianHalfFluxDiameter){for(double sigma:{1.,1.5,2.,3.}){auto f=astro::measureStars(gaussian(sigma));ASSERT_EQ(f.stars.size(),1)<<sigma;EXPECT_NEAR(f.stars[0].hfd,2*std::sqrt(2*std::log(2.))*sigma,.35);}}
TEST(StarAnalysis,ConstantFluxDefocusIncreasesHfd){double last=0;for(double sigma:{1.,1.5,2.,2.5,3.}){auto f=astro::measureStars(gaussian(sigma,180/(sigma*sigma)));ASSERT_EQ(f.stars.size(),1)<<sigma;EXPECT_GT(f.stars[0].hfd,last);last=f.stars[0].hfd;}}
TEST(StarAnalysis,RejectsClippingHotPixelsTrailsAndNoise){EXPECT_TRUE(astro::measureStars(gaussian(.15,180)).stars.empty());auto saturated=astro::measureStars(gaussian(1.5,400));EXPECT_TRUE(saturated.stars.empty());EXPECT_GT(saturated.saturated,0);EXPECT_TRUE(astro::measureStars(gaussian(1.5,100,0,0,4)).stars.empty());auto p=gaussian(1);std::mt19937 rng(421);std::normal_distribution<double> noise(0,4);for(std::size_t i=0;i<p.rgba.size();i+=4){auto v=static_cast<std::uint8_t>(std::clamp(std::lround(50+noise(rng)),0L,255L));for(int c=0;c<3;++c)p.rgba[i+c]=v;}EXPECT_TRUE(astro::measureStars(p).stars.empty());}
TEST(StarAnalysis,SubpixelTranslationAndBrightnessDoNotInventSharperStars){auto ref=astro::measureStars(gaussian(2,140));ASSERT_EQ(ref.stars.size(),1);for(double shift:{.2,.5,.8}){auto f=astro::measureStars(gaussian(2,70,shift,-shift));ASSERT_EQ(f.stars.size(),1);EXPECT_NEAR(f.stars[0].hfd,ref.stars[0].hfd,.3);EXPECT_NEAR(f.stars[0].x,48+shift,.15);}}
TEST(StarAnalysis,SourcePixelsAreUnmodified){auto p=gaussian(2);auto copy=p.rgba;astro::measureStars(p);EXPECT_EQ(p.rgba,copy);}
}
