#include <astro/cameras/zwo/adapter/ZwoAdapter.hpp>
#include <astro/imaging/sensor/SensorImage.hpp>
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <astro/infrastructure/camera_worker/Infrastructure.hpp>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <thread>
#include <iostream>
#include <algorithm>
TEST(AsiHardware, ContinuousFeedRecordsNativeSerAndStillSequencesWithoutRestartingVideo) {
    if (!std::getenv("ASTRO_ZWO_HARDWARE")) GTEST_SKIP() << "Physical ASI required";
    auto camera = astro::zwo::createAdapter();
    std::cout << camera->connect() << '\n';
    const auto original = *camera->captureSettings();
    const auto controls = camera->deviceControls();
    auto exposure = *std::find_if(controls.begin(), controls.end(), [](const auto& c) { return c.id == 1; });
    struct Restore {
        astro::ICamera& c;
        astro::CaptureSettings geometry;
        astro::DeviceControl exposure;
        ~Restore() {
            try {
                c.stopFrameRecording();
                while (c.frameRecordingStatus().active)
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                c.setLiveView(false);
                c.configureCapture(geometry.value);
                c.setDeviceControl(1, exposure.value, exposure.automatic);
                c.disconnect();
            } catch (...) {
            }
        }
    } restore{*camera, original, exposure};
    auto settings = original.value;
    settings.width = 64;
    settings.height = 64;
    settings.x = settings.y = 0;
    settings.pixelFormat = 0;
    camera->configureCapture(settings);
    camera->setDeviceControl(1, 1000, false);
    camera->setLiveView(true);
    const auto root = std::filesystem::temp_directory_path() /
                      ("asi-continuous-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    auto run = [&](astro::FrameRecordingOptions options, const std::string& name, int stopAfter = 0) {
        auto path = root / name;
        std::filesystem::create_directory(path);
        camera->startFrameRecording(options, path, 0);
        EXPECT_THROW(camera->setDeviceControl(0, 10, false), std::exception);
        EXPECT_THROW(camera->configureCapture(settings), std::exception);
        EXPECT_THROW(camera->setLiveView(false), std::exception);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(35);
        unsigned previews{};
        while (camera->frameRecordingStatus().active && std::chrono::steady_clock::now() < deadline) {
            if (!camera->liveFrame().jpeg.empty()) ++previews;
            if (stopAfter && camera->frameRecordingStatus().received >= unsigned(stopAfter))
                camera->stopFrameRecording();
            std::this_thread::yield();  // Polling test must not inherit Windows' 15.6 ms Sleep granularity.
        }
        auto state = camera->frameRecordingStatus();
        EXPECT_FALSE(state.active);
        EXPECT_TRUE(state.complete) << state.error;
        EXPECT_EQ(state.queueDropped, 0);
        EXPECT_GT(previews, 0);
        if (options.frames)
            EXPECT_EQ(state.written, options.frames);
        else
            EXPECT_GE(state.written, 1);
        std::ifstream metadata(path / "capture.json");
        auto json = nlohmann::json::parse(metadata);
        EXPECT_EQ(json["frames_written"], state.written);
        if (options.format == "ser") {
            std::ifstream ser(path / "capture.ser", std::ios::binary);
            ser.seekg(38);
            unsigned char count[4]{};
            ser.read(reinterpret_cast<char*>(count), 4);
            auto n = unsigned(count[0]) | (unsigned(count[1]) << 8) | (unsigned(count[2]) << 16) |
                     (unsigned(count[3]) << 24);
            EXPECT_EQ(n, state.written);
            EXPECT_EQ(std::filesystem::file_size(path / "capture.ser"), 178 + state.written * (64 * 64 + 8));
            EXPECT_EQ(astro::sensor::readSer(path / "capture.ser").frame.width, 64);
        } else {
            EXPECT_EQ(astro::decodePreview(path / "frames" / ("frame_00000001." + options.format)).width, 64);
        }
        std::cout << name << ": " << state.written << " native frames, " << previews << " previews, "
                  << state.sdkDropped << " SDK drops\n";
    };
    for (unsigned n : {100, 200, 500, 1000}) {
        astro::FrameRecordingOptions o;
        o.frames = n;
        run(o, "ser-" + std::to_string(n));
    }
    for (const char* format : {"fits", "png", "tiff"}) {
        astro::FrameRecordingOptions o;
        o.format = format;
        o.frames = 5;
        run(o, format);
    }
    astro::FrameRecordingOptions unlimited;
    unlimited.frames = 0;
    run(unlimited, "until-stopped", 5);
    auto timed = unlimited;
    timed.seconds = .4;
    run(timed, "duration");
    camera->setLiveView(false);
    camera->configureCapture(original.value);
    camera->setDeviceControl(1, exposure.value, exposure.automatic);
    camera->setLiveView(true);
    bool resumed{};
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!resumed && std::chrono::steady_clock::now() < until) {
        resumed = !camera->liveFrame().jpeg.empty();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    EXPECT_TRUE(resumed);
    std::cout << "Physical continuous capture outputs: " << root << '\n';
}
