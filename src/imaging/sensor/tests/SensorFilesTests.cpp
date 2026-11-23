#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <astro/imaging/sensor/SensorImage.hpp>
#include <astro/imaging/preview/processing/LiveProcessor.hpp>
#include <gtest/gtest.h>
#include <chrono>
#include <fstream>
#include <thread>

namespace {
struct Files {
    std::vector<std::filesystem::path> paths;
    std::filesystem::path next(const char* ext) {
        auto path =
            std::filesystem::temp_directory_path() /
            ("astro-sensor-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ext);
        paths.push_back(path);
        auto side = path;
        side += ".json";
        paths.push_back(side);
        return path;
    }
    ~Files() {
        for (const auto& p : paths) {
            std::error_code error;
            std::filesystem::remove(p, error);
        }
    }
};
astro::sensor::Frame samples() {
    astro::sensor::Frame frame{8, 8, astro::sensor::Raw16, {}};
    for (unsigned i = 0; i < 64; ++i) {
        const unsigned v = i * 1024 + 17;
        frame.bytes.push_back(v & 255);
        frame.bytes.push_back(v >> 8);
    }
    return frame;
}
}  // namespace
TEST(SensorFiles, SerPreservesEveryNativeBitAndBayerHeaderWithoutOverwrite) {
    Files files;
    auto path = files.next(".ser");
    auto frame = samples();
    astro::sensor::save(path, frame, "GRBG", "Real device name", "ser");
    auto decoded = astro::sensor::readSer(path);
    EXPECT_EQ(decoded.frame.bytes, frame.bytes);
    EXPECT_EQ(decoded.bayer, "GRBG");
    EXPECT_EQ(std::filesystem::file_size(path), 178 + frame.bytes.size());
    EXPECT_THROW(astro::sensor::save(path, frame, "GRBG", "Device", "ser"), std::runtime_error);
}
TEST(SensorFiles, PngAndTiffPreserveSixteenBitSensorSamples) {
    const auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct Com {
        HRESULT hr;
        ~Com() {
            if (SUCCEEDED(hr)) CoUninitialize();
        }
    } com{hr};
    Files files;
    auto frame = samples();
    for (const char* format : {"png", "tiff"}) {
        auto path = files.next(format);
        astro::sensor::save(path, frame, "RGGB", "Device", format);
        Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
        ASSERT_TRUE(SUCCEEDED(
            CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))));
        Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
        ASSERT_TRUE(SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                                 WICDecodeMetadataCacheOnLoad, &decoder)));
        Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> image;
        ASSERT_TRUE(SUCCEEDED(decoder->GetFrame(0, &image)));
        GUID pixel{};
        ASSERT_TRUE(SUCCEEDED(image->GetPixelFormat(&pixel)));
        EXPECT_TRUE(IsEqualGUID(pixel, GUID_WICPixelFormat16bppGray));
        std::vector<std::uint8_t> bytes(frame.bytes.size());
        ASSERT_TRUE(SUCCEEDED(image->CopyPixels(nullptr, 16, UINT(bytes.size()), bytes.data())));
        EXPECT_EQ(bytes, frame.bytes);
    }
}
TEST(LiveProcessor, LatestPendingFrameFinishesWithRealCodecAndNoUnboundedQueue) {
    Files files;
    auto path = files.next(".png");
    astro::sensor::save(path, samples(), "RGGB", "Device", "png");
    std::ifstream in(path, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    astro::LiveProcessor processor;
    for (unsigned i = 1; i <= 100; ++i) {
        astro::LiveProcessingRequest request;
        request.frame.jpeg = bytes;
        request.frame.revision = i;
        processor.request(std::move(request));
    }
    std::unique_ptr<astro::ProcessedLive> last;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto result = processor.take()) last = std::move(result);
        if (last && last->revision == 100) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(last->revision, 100);
    EXPECT_TRUE(last->error.empty()) << last->error;
    EXPECT_EQ(last->original.width, 8);
    EXPECT_EQ(last->original.height, 8);
}
