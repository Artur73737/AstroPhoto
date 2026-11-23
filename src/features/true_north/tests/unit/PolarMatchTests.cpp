#include <astro/features/true_north/astrometry/matching/PolarMatch.hpp>
#include <gtest/gtest.h>
#include <random>
TEST(PolarMatch, RecoversScaleRotationParityAndRejectsDuplicateOwnership){
    using astro::stacking::Point;std::vector<Point> reference,sources;std::mt19937 random(3751);std::uniform_real_distribution<double> x(-1.6,1.6),y(-1.1,1.1),noise(-.03,.03);
    for(int i=0;i<120;++i){Point p{x(random),y(random),double(1000-i)};reference.push_back(p);
        sources.push_back({280*p.x+110*p.y+600,110*p.x-280*p.y+400,double(1000-i)});}
    for(auto& p:sources){p.x+=noise(random);p.y+=noise(random);}
    auto result=astro::north::detail::recover(reference,sources,1200,800,5,{},{});
    EXPECT_GE(result.stars,115);EXPECT_LT(result.rms,.1);EXPECT_NEAR(result.transform.a,280,.05);EXPECT_NEAR(result.transform.b,110,.05);EXPECT_NEAR(result.transform.tx,600,.05);
}
TEST(PolarMatch, CancellationNeverReturnsAnUnverifiedSolution){
    std::vector<astro::stacking::Point> points;for(int i=0;i<40;++i)points.push_back({double(i%7),double((i*i)%11),double(100-i)});
    EXPECT_THROW(astro::north::detail::recover(points,points,1200,800,2,[]{return true;},{}),std::runtime_error);
}
TEST(PolarMatch, UnrelatedFieldsDoNotPassConfidenceGate){
    std::mt19937 random(19527);std::uniform_real_distribution<double> r(-2,2),x(20,1180),y(20,780);
    std::vector<astro::stacking::Point> reference,sources;for(int i=0;i<70;++i){reference.push_back({r(random),r(random),double(i)});sources.push_back({x(random),y(random),double(i)});}
    EXPECT_THROW(astro::north::detail::recover(reference,sources,1200,800,2,{},{}),std::runtime_error);
}
