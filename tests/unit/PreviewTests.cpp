#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <gtest/gtest.h>
#include <astro/imaging/fits/FitsImage.hpp>
#include <cstdlib>
#include <thread>

TEST(PhotoPreview, MissingImageReportsAnError) {
    EXPECT_THROW(astro::decodePreview("not-an-existing-capture.jpg"), std::runtime_error);
    EXPECT_THROW(astro::decodePreview("not-an-existing-capture.CR2"), std::runtime_error);
}
TEST(PhotoPreview, RealCr2EmbeddedImageAndWorkerDelivery) {
    char* sample{};
    std::size_t size{};
    _dupenv_s(&sample, &size, "ASTRO_PREVIEW_CR2");
    std::unique_ptr<char, decltype(&std::free)> owner(sample, &std::free);
    if (!sample) GTEST_SKIP() << "Set ASTRO_PREVIEW_CR2 to an existing CR2 for the real-file test";
    auto file = astro::pathFromUtf8(sample);
    auto pixels = astro::decodePreview(file);
    EXPECT_GT(pixels.width, 100);
    EXPECT_GT(pixels.height, 100);
    EXPECT_EQ(pixels.rgba.size(), static_cast<std::size_t>(pixels.width) * pixels.height * 4);
    EXPECT_NE(pixels.description.find("CR2"), std::string::npos);
    astro::PhotoPreview loader;
    loader.request({1, astro::FrameType::Light, {file}});
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::optional<astro::PreviewPixels> result;
    while (!result && std::chrono::steady_clock::now() < deadline) {
        result = loader.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(result.has_value()) << loader.status();
    EXPECT_EQ(result->rgba, pixels.rgba);
}

TEST(PhotoPreview, Raw16BayerFitsIsDemosaicedInsteadOfDisplayingGreenMosaic) {
    auto file = std::filesystem::temp_directory_path() /
                ("astro-bayer-preview-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                 ".fits");
    std::vector<std::uint8_t> bytes(16 * 16 * 2);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) {
            unsigned value = (!(y & 1) && !(x & 1)) ? 10000 : ((y & 1) && (x & 1)) ? 30000 : 20000;
            auto i = (y * 16 + x) * 2;
            bytes[i] = value & 255;
            bytes[i + 1] = value >> 8;
        }
    astro::fits::writeRaw16(
        file, 16, 16, bytes,
        {{"INSTRUME", "'Numerical Bayer fixture'"}, {"GAIN", "0"}, {"EXPTIME", "1"}, {"BAYERPAT", "'RGGB'"}});
    auto image = astro::decodePreview(file);
    ASSERT_EQ(image.rgba.size(), 16 * 16 * 4u);
    auto i = (8 * 16 + 8) * 4;
    EXPECT_LT(image.rgba[i], image.rgba[i + 1]);
    EXPECT_LT(image.rgba[i + 1], image.rgba[i + 2]);
    EXPECT_EQ(image.rgba[i], image.rgba[i + 4]);
    EXPECT_EQ(image.rgba[i + 1], image.rgba[i + 5]);
    std::filesystem::remove(file);
}
