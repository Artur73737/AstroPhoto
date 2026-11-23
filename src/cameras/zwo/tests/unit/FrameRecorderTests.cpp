#include <astro/cameras/zwo/recording/FrameRecorder.hpp>
#include <astro/imaging/fits/FitsImage.hpp>
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>
namespace {
struct Directory {
    std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("astro-recording-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Directory() { std::filesystem::create_directory(path); }
    ~Directory() {
        std::error_code e;
        const auto target = std::filesystem::weakly_canonical(path, e);
        if (!e && target.parent_path() == std::filesystem::weakly_canonical(std::filesystem::temp_directory_path()) &&
            target.filename().string().starts_with("astro-recording-"))
            std::filesystem::remove_all(target, e);
    }
};
std::uint32_t little(const std::vector<unsigned char>& b, std::size_t p) {
    return unsigned(b[p]) | (unsigned(b[p + 1]) << 8) | (unsigned(b[p + 2]) << 16) | (unsigned(b[p + 3]) << 24);
}
auto bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(in)), {});
}
astro::sensor::Frame frame() {
    astro::sensor::Frame f{8, 8, 2, {}};
    for (int i = 0; i < 64; ++i) {
        f.bytes.push_back(std::uint8_t(i));
        f.bytes.push_back(std::uint8_t(i + 7));
    }
    return f;
}
}  // namespace
TEST(FrameRecorder, SerContainsExactlyFiveOrderedNativeFramesAndUtcTrailer) {
    Directory dir;
    astro::FrameRecordingOptions options;
    options.frames = 5;
    const auto f = frame();
    astro::zwo::FrameRecorder recorder(options, dir.path, {8, 8, 2, {}}, "RGGB", "Unit data", "{}", 0);
    const auto t = std::chrono::system_clock::now();
    for (int i = 0; i < 5; ++i) {
        auto sample = f;
        sample.bytes[0] = std::uint8_t(i);
        ASSERT_TRUE(recorder.push(sample, t + std::chrono::milliseconds(i)));
    }
    EXPECT_FALSE(recorder.push(f, t));
    recorder.finish();
    auto state = recorder.status();
    EXPECT_FALSE(state.active);
    EXPECT_TRUE(state.complete) << state.error;
    EXPECT_EQ(state.written, 5);
    EXPECT_EQ(state.received, 5);
    auto data = bytes(dir.path / "capture.ser");
    ASSERT_EQ(data.size(), 178 + f.bytes.size() * 5 + 8 * 5);
    EXPECT_EQ(little(data, 38), 5);
    EXPECT_EQ(little(data, 18), 8);
    EXPECT_EQ(little(data, 34), 16);
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(data[178 + i * f.bytes.size()], i);
    EXPECT_TRUE(std::equal(data.begin() + 170, data.begin() + 178, data.begin() + 178 + f.bytes.size() * 5));
    std::ifstream info(dir.path / "capture.json");
    auto json = nlohmann::json::parse(info);
    EXPECT_EQ(json["frames_written"], 5);
    EXPECT_EQ(json["complete"], true);
}
TEST(FrameRecorder, StillFileSequencesDrainAndPreserveSamplePrecision) {
    for (const std::string format : {"fits", "png", "tiff"}) {
        Directory dir;
        astro::FrameRecordingOptions options;
        options.format = format;
        options.frames = 0;
        auto f = frame();
        astro::zwo::FrameRecorder recorder(options, dir.path, {8, 8, 2, {}}, "RGGB", "Device", "{}", 0);
        for (int i = 0; i < 5; ++i)
            ASSERT_TRUE(recorder.push(f, std::chrono::system_clock::now()));
        recorder.finish();
        EXPECT_EQ(recorder.status().written, 5);
        EXPECT_TRUE(recorder.status().complete) << recorder.status().error;
        const auto file = dir.path / "frames" / ("frame_00000001." + format);
        EXPECT_TRUE(std::filesystem::exists(file));
        EXPECT_EQ(astro::decodePreview(file).width, 8);
        if (format == "fits") {
            auto image = astro::fits::read(file, 1024 * 1024);
            ASSERT_EQ(image.samples.size(), 64);
            EXPECT_EQ(image.bitpix, 16);
        }
    }
}
TEST(FrameRecorder, QueueExhaustionStopsExplicitlyAndPreservesExistingFiles) {
    Directory dir;
    astro::FrameRecordingOptions options;
    options.frames = 0;
    options.memoryMiB = 16;
    astro::zwo::FrameRecorder recorder(options, dir.path, {5000, 2000, 2, {}}, "RGGB", "Device", "{}", 0);
    astro::sensor::Frame f{5000, 2000, 2, std::vector<std::uint8_t>(20000000)};
    EXPECT_FALSE(recorder.push(f, std::chrono::system_clock::now()));
    recorder.finish();
    auto state = recorder.status();
    EXPECT_FALSE(state.active);
    EXPECT_FALSE(state.complete);
    EXPECT_EQ(state.queueDropped, 1);
    EXPECT_FALSE(state.error.empty());
    const auto original = bytes(dir.path / "capture.ser");
    EXPECT_THROW((astro::zwo::FrameRecorder(options, dir.path, {8, 8, 2, {}}, "RGGB", "Device", "{}", 0)),
                 std::runtime_error);
    EXPECT_EQ(bytes(dir.path / "capture.ser"), original);
}
TEST(FrameRecorder, DurationRejectsLateFramesAndGracefulStopRejectsNewFrames) {
    Directory dir;
    astro::FrameRecordingOptions options;
    options.frames = 0;
    options.seconds = .02;
    auto f = frame();
    astro::zwo::FrameRecorder recorder(options, dir.path, {8, 8, 2, {}}, "RGGB", "Device", "{}", 0);
    ASSERT_TRUE(recorder.push(f, std::chrono::system_clock::now()));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_FALSE(recorder.push(f, std::chrono::system_clock::now()));
    recorder.finish();
    EXPECT_EQ(recorder.status().written, 1);
    EXPECT_TRUE(recorder.status().complete);
}
