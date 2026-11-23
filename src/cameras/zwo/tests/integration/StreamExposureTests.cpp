#include <astro/infrastructure/camera_worker/Infrastructure.hpp>
#include <gtest/gtest.h>
#include <thread>
#include <algorithm>
#include <iostream>
TEST(AsiHardware, AlwaysOnStreamReportsHalfSecondExposureAndOnlySavesAfterStart) {
    if (!std::getenv("ASTRO_ZWO_HARDWARE")) GTEST_SKIP() << "Physical ASI required";
    astro::Config config;
    config.cameraModule = "zwo_asi";
    config.language = "en";
    config.zwo.width = 640;
    config.zwo.height = 480;
    config.outputDirectory =
        std::filesystem::temp_directory_path() /
        ("asi-exposure-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(config.outputDirectory);
    config.recording.frames = 3;
    astro::Mailbox mailbox;
    astro::CameraWorker worker(mailbox, config, config.outputDirectory / "user.json");
    auto wait = [&](auto predicate) {
        auto end = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (std::chrono::steady_clock::now() < end) {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    };
    auto send = [&](astro::Command c, bool error = false) {
        c.config = config;
        auto id = mailbox.submit(c);
        EXPECT_TRUE(wait([&] {
            auto r = mailbox.result(id);
            return r && r->finished;
        }));
        auto r = mailbox.result(id);
        if (r) EXPECT_EQ(r->error.empty(), !error) << r->error;
    };
    astro::Command c;
    c.action = astro::Action::Connect;
    send(c);
    ASSERT_TRUE(wait([&] { return mailbox.snapshot().liveView && mailbox.snapshot().streamExposure.active; }));
    const auto controls = mailbox.snapshot().deviceControls;
    const auto original = *std::find_if(controls.begin(), controls.end(), [](const auto& v) { return v.id == 1; });
    c.action = astro::Action::SetDeviceControl;
    c.controlId = 1;
    c.controlValue = 500000;
    send(c);
    ASSERT_TRUE(wait([&] {
        auto s = mailbox.snapshot();
        auto p = s.streamExposure.fraction(std::chrono::steady_clock::now());
        return s.streamExposure.active && s.streamExposure.seconds == .5 && p > .05 && p < .95;
    }));
    ASSERT_TRUE(wait([&] { return mailbox.snapshot().streamExposure.frame >= 3; }));
    EXPECT_FALSE(mailbox.snapshot().recording.active);
    EXPECT_TRUE(mailbox.snapshot().recording.directory.empty());
    EXPECT_FALSE(
        std::filesystem::exists(astro::sessionDirectory(config, std::chrono::system_clock::now()) / "captures"));
    c.action = astro::Action::LiveView;
    c.value = 0;
    send(c, true);
    EXPECT_TRUE(mailbox.snapshot().liveView);
    c.action = astro::Action::StartFrameRecording;
    send(c);
    ASSERT_TRUE(wait([&] {
        auto s = mailbox.snapshot();
        return !s.recording.active && s.recording.written == 3;
    }));
    auto result = mailbox.snapshot();
    EXPECT_TRUE(result.recording.complete) << result.recording.error;
    EXPECT_TRUE(result.liveView);
    EXPECT_TRUE(result.streamExposure.active);
    const auto frame = result.streamExposure.frame;
    EXPECT_TRUE(wait([&] { return mailbox.snapshot().streamExposure.frame > frame + 1; }));
    c.action = astro::Action::SetDeviceControl;
    c.controlValue = original.value;
    c.controlAutomatic = original.automatic;
    send(c);
    c.action = astro::Action::Disconnect;
    send(c);
    std::cout << "Real 0.5 s preview progress, no preview files, three saved frames, and always-on stream passed\n";
}
