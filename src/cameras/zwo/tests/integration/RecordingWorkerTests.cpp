#include <astro/infrastructure/camera_worker/Infrastructure.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <fstream>
#include <iostream>
#include <algorithm>
TEST(AsiHardware, WorkerRecordsOneHundredRaw16FramesIndependentOfTwoFpsPreview) {
    if (!std::getenv("ASTRO_ZWO_HARDWARE")) GTEST_SKIP() << "Physical ASI required";
    astro::Config config;
    config.cameraModule = "zwo_asi";
    config.language = "en";
    config.zwo.width = 640;
    config.zwo.height = 480;
    config.zwo.pixelFormat = 2;
    config.zwo.previewFps = 2;
    config.outputDirectory =
        std::filesystem::temp_directory_path() /
        ("asi-worker-recording-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(config.outputDirectory);
    config.recording.frames = 100;
    astro::Mailbox mailbox;
    astro::CameraWorker worker(mailbox, config, config.outputDirectory / "user.json");
    auto wait = [&](auto predicate) {
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (std::chrono::steady_clock::now() < until) {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    };
    auto submit = [&](astro::Action action, bool expectError = false) {
        astro::Command c;
        c.action = action;
        c.config = config;
        auto id = mailbox.submit(c);
        EXPECT_TRUE(wait([&] {
            auto r = mailbox.result(id);
            return r && r->finished;
        }));
        auto r = mailbox.result(id);
        if (r) {
            if (expectError)
                EXPECT_FALSE(r->error.empty());
            else
                EXPECT_TRUE(r->error.empty()) << r->error;
        }
        return id;
    };
    submit(astro::Action::Connect);
    ASSERT_TRUE(wait([&] { return mailbox.snapshot().connected; }));
    submit(astro::Action::CaptureSingle, true);
    submit(astro::Action::Start, true);
    const auto values = mailbox.snapshot().deviceControls;
    const auto exposure = *std::find_if(values.begin(), values.end(), [](const auto& c) { return c.id == 1; });
    astro::Command set;
    set.action = astro::Action::SetDeviceControl;
    set.controlId = 1;
    set.controlValue = 20000;
    auto setId = mailbox.submit(set);
    ASSERT_TRUE(wait([&] {
        auto r = mailbox.result(setId);
        return r && r->finished;
    }));
    submit(astro::Action::StartFrameRecording);
    ASSERT_TRUE(wait([&] { return mailbox.snapshot().recording.active; }));
    EXPECT_TRUE(mailbox.snapshot().liveView);
    submit(astro::Action::ConfigureCapture, true);
    submit(astro::Action::CaptureSingle, true);
    ASSERT_TRUE(wait([&] {
        auto s = mailbox.snapshot();
        return !s.recording.active && s.recording.received >= 100;
    }));
    auto result = mailbox.snapshot();
    EXPECT_TRUE(result.recording.complete) << result.recording.error;
    EXPECT_EQ(result.recording.written, 100);
    EXPECT_EQ(result.recording.queueDropped, 0);
    EXPECT_TRUE(result.liveView);
    EXPECT_GT(result.recording.fps, 2);
    EXPECT_GT(result.livePerformance.frames, 0);
    const auto file = astro::pathFromUtf8(result.recording.file);
    EXPECT_EQ(std::filesystem::file_size(file), 178 + 100ull * (640 * 480 * 2 + 8));
    std::cout << "Real worker: " << result.recording.written << " RAW16 frames at " << result.recording.fps
              << " saved FPS with 2 FPS preview; " << file << '\n';
    // A second unlimited run can be stopped without ending the live stream.
    config.recording.frames = 0;
    submit(astro::Action::StartFrameRecording);
    ASSERT_TRUE(wait([&] { return mailbox.snapshot().recording.received >= 5; }));
    submit(astro::Action::StopFrameRecording);
    ASSERT_TRUE(wait([&] { return !mailbox.snapshot().recording.active; }));
    EXPECT_TRUE(mailbox.snapshot().liveView);
    set.controlValue = exposure.value;
    set.controlAutomatic = exposure.automatic;
    setId = mailbox.submit(set);
    EXPECT_TRUE(wait([&] {
        auto r = mailbox.result(setId);
        return r && r->finished;
    }));
    submit(astro::Action::Disconnect);
}
