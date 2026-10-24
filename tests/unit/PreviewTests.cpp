#include "PhotoPreview.hpp"
#include <gtest/gtest.h>
#include <cstdlib>
#include <thread>

TEST(PhotoPreview,MissingImageReportsAnError) {
    EXPECT_THROW(astro::decodePreview("not-an-existing-capture.jpg"),std::runtime_error);
    EXPECT_THROW(astro::decodePreview("not-an-existing-capture.CR2"),std::runtime_error);
}
TEST(PhotoPreview,RealCr2EmbeddedImageAndWorkerDelivery) {
    char* sample{};std::size_t size{};_dupenv_s(&sample,&size,"ASTRO_PREVIEW_CR2");
    std::unique_ptr<char,decltype(&std::free)> owner(sample,&std::free);
    if(!sample)GTEST_SKIP()<<"Set ASTRO_PREVIEW_CR2 to an existing CR2 for the real-file test";
    auto file=astro::pathFromUtf8(sample);auto pixels=astro::decodePreview(file);
    EXPECT_GT(pixels.width,100);EXPECT_GT(pixels.height,100);
    EXPECT_EQ(pixels.rgba.size(),static_cast<std::size_t>(pixels.width)*pixels.height*4);
    EXPECT_NE(pixels.description.find("CR2"),std::string::npos);
    astro::PhotoPreview loader;loader.request({1,astro::FrameType::Light,{file}});
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    std::optional<astro::PreviewPixels> result;
    while(!result&&std::chrono::steady_clock::now()<deadline){result=loader.poll();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
    ASSERT_TRUE(result.has_value())<<loader.status();EXPECT_EQ(result->rgba,pixels.rgba);
}
