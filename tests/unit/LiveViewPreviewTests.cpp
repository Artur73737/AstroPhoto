#include "LiveViewPreview.hpp"
#include <gtest/gtest.h>
TEST(LiveViewPreview,CanonTenXUsesMetadataToCropFiveXStream) {
    astro::PreviewPixels source{848,568,std::vector<std::uint8_t>(848*568*4),{}};
    for(int y=0;y<568;++y)for(int x=0;x<848;++x){auto i=(y*848+x)*4;source.rgba[i]=static_cast<std::uint8_t>(x);source.rgba[i+1]=static_cast<std::uint8_t>(y);source.rgba[i+3]=255;}
    astro::LiveFrame frame;frame.zoom=10;frame.coordinateWidth=4272;frame.coordinateHeight=2848;frame.imageX=1708;frame.imageY=1138;frame.zoomRect={1922,1281,428,286};
    auto cropped=astro::prepareLiveViewPreview(source,frame);
    EXPECT_EQ(cropped.width,424);EXPECT_EQ(cropped.height,284);EXPECT_EQ(cropped.rgba[0],212);EXPECT_EQ(cropped.rgba[1],142);
    frame.zoomRect.x=frame.imageX;frame.zoomRect.y=frame.imageY;
    auto corner=astro::prepareLiveViewPreview(source,frame);EXPECT_EQ(corner.rgba[0],0);EXPECT_EQ(corner.rgba[1],0);
    frame.zoom=5;EXPECT_EQ(astro::prepareLiveViewPreview(source,frame).rgba,source.rgba);
    frame.zoom=1;EXPECT_EQ(astro::prepareLiveViewPreview(source,frame).rgba,source.rgba);
    frame.zoom=10;frame.zoomRect.width=0;EXPECT_THROW(astro::prepareLiveViewPreview(source,frame),std::runtime_error);
}
