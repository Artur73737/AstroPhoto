#include "LiveViewPreview.hpp"
#include <astro/CameraModule.hpp>
#include <gtest/gtest.h>
TEST(LiveViewPreview,UncroppedDecodedFrameTransfersItsBufferWithoutAnotherCopy){
    astro::PreviewPixels source{4,4,std::vector<std::uint8_t>(64,77),{}};
    const auto* pixels=source.rgba.data();
    auto display=astro::prepareLiveViewPreview(std::move(source),{});
    EXPECT_EQ(display.rgba.data(),pixels);EXPECT_EQ(display.rgba.size(),64);
}
TEST(LiveViewPreview,CanonTenXUsesMetadataToCropFiveXStream) {
    astro::PreviewPixels source{848,568,std::vector<std::uint8_t>(848*568*4),{}};
    for(int y=0;y<568;++y)for(int x=0;x<848;++x){auto i=(y*848+x)*4;source.rgba[i]=static_cast<std::uint8_t>(x);source.rgba[i+1]=static_cast<std::uint8_t>(y);source.rgba[i+3]=255;}
    astro::LiveFrame frame;frame.zoom=10;frame.coordinateWidth=4272;frame.coordinateHeight=2848;frame.imageX=1708;frame.imageY=1138;frame.zoomRect={1922,1281,428,286};
    const auto& module=astro::camera_models::find("canon_eos_1100d");
    frame.previewRegion=module.previewRegion(frame);
    auto cropped=astro::prepareLiveViewPreview(source,frame);
    EXPECT_EQ(cropped.width,424);EXPECT_EQ(cropped.height,284);EXPECT_EQ(cropped.rgba[0],212);EXPECT_EQ(cropped.rgba[1],142);
    frame.zoomRect.x=frame.imageX;frame.zoomRect.y=frame.imageY;
    frame.previewRegion=module.previewRegion(frame);
    auto corner=astro::prepareLiveViewPreview(source,frame);EXPECT_EQ(corner.rgba[0],0);EXPECT_EQ(corner.rgba[1],0);
    frame.zoom=5;frame.previewRegion=module.previewRegion(frame);EXPECT_EQ(astro::prepareLiveViewPreview(source,frame).rgba,source.rgba);
    frame.zoom=1;frame.previewRegion=module.previewRegion(frame);EXPECT_EQ(astro::prepareLiveViewPreview(source,frame).rgba,source.rgba);
    frame.zoom=10;frame.zoomRect.width=0;EXPECT_THROW(module.previewRegion(frame),std::runtime_error);
}
TEST(LiveViewPreview,UncroppedModuleDoesNotInheritCanonTenXWorkaround){
    astro::PreviewPixels source{4,4,std::vector<std::uint8_t>(64,77),{}};
    astro::LiveFrame frame;frame.zoom=10;EXPECT_EQ(astro::prepareLiveViewPreview(source,frame).rgba,source.rgba);
    frame.previewRegion={0,0,2,.5,{}};EXPECT_THROW(astro::prepareLiveViewPreview(source,frame),std::runtime_error);
}
