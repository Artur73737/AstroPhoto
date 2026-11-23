#include <astro/cameras/zwo/preview/SensorPreview.hpp>
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <gtest/gtest.h>
#include <thread>

TEST(AsiPreview, Raw16JpegPreservesRedAndBlueAcrossBayerPatternsWithoutCallerCom) {
    for (const std::string pattern : {"RGGB", "BGGR", "GRBG", "GBRG"}) {
        for (char dominant : {'R', 'B'}) {
            astro::zwo::Device device;
            device.color = true;
            device.bayer = pattern;
            astro::zwo::Frame frame{64, 64, 2, std::vector<std::uint8_t>(64 * 64 * 2)};
            for (int y = 0; y < 64; ++y)
                for (int x = 0; x < 64; ++x) {
                    const unsigned value = pattern[(y & 1) * 2 + (x & 1)] == dominant ? 40000 : 100;
                    auto index = (y * 64 + x) * 2;
                    frame.bytes[index] = std::uint8_t(value);
                    frame.bytes[index + 1] = std::uint8_t(value >> 8);
                }
            // A fresh thread deliberately has no COM apartment: both encoder
            // and decoder must work without a Canon session or caller setup.
            std::exception_ptr error;
            astro::PreviewPixels decoded;
            std::thread thread([&] {
                try {
                    decoded = astro::decodeImage(astro::zwo::previewJpeg(frame, device));
                } catch (...) {
                    error = std::current_exception();
                }
            });
            thread.join();
            if (error) std::rethrow_exception(error);
            ASSERT_EQ(decoded.width, 64);
            const auto pixel = (32 * 64 + 32) * 4;
            EXPECT_GT(decoded.rgba[pixel + (dominant == 'R' ? 0 : 2)], 140);
            EXPECT_LT(decoded.rgba[pixel + (dominant == 'R' ? 0 : 2)], 175);
            EXPECT_LT(decoded.rgba[pixel + (dominant == 'R' ? 2 : 0)], 20);
            EXPECT_LT(decoded.rgba[pixel + 1], 20);
        }
    }
}
TEST(AsiPreview, RejectsInvalidBuffersBeforeEncoding) {
    astro::zwo::Device device;
    EXPECT_THROW(astro::zwo::previewJpeg({}, device), std::invalid_argument);
    astro::zwo::Frame frame{64, 64, 2, {}};
    EXPECT_THROW(astro::zwo::previewJpeg(frame, device), std::invalid_argument);
    frame.bytes.resize(64 * 64 * 2);
    device.color = true;
    EXPECT_THROW(astro::zwo::previewJpeg(frame, device), std::invalid_argument);
}
