#include <astro/cameras/zwo/adapter/ZwoAdapter.hpp>
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <astro/infrastructure/camera_worker/Infrastructure.hpp>
#include <gtest/gtest.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

// Opt in explicitly: these tests use the real, locally connected ASI SDK and
// camera. They never replace an SDK function or manufacture a camera frame.
TEST(AsiHardware, NativeControlsAndLiveViewSurviveStillCapture) {
    if (!std::getenv("ASTRO_ZWO_HARDWARE")) GTEST_SKIP() << "Set ASTRO_ZWO_HARDWARE=1 for a connected ASI camera";
    auto camera = astro::zwo::createAdapter();
    auto name = camera->connect();
    std::cout << "Connected: " << name << '\n';
    const auto controls = camera->deviceControls();
    ASSERT_FALSE(controls.empty());
    for (const auto& c : controls) {
        std::cout << c.id << " | " << c.name << " | " << c.value << " | " << c.minimum << ".." << c.maximum
                  << " | auto=" << c.automatic << " writable=" << c.writable << " | " << c.description << '\n';
        EXPECT_GE(c.value, c.minimum);
        EXPECT_LE(c.value, c.maximum);
    }
    const auto gain = std::find_if(controls.begin(), controls.end(), [](const auto& c) { return c.id == 0; });
    const auto exposure = std::find_if(controls.begin(), controls.end(), [](const auto& c) { return c.id == 1; });
    ASSERT_NE(gain, controls.end());
    ASSERT_NE(exposure, controls.end());
    EXPECT_EQ(exposure->unit, "us");
    EXPECT_TRUE(exposure->logarithmic);
    for (const auto& c : controls) {
        if (c.name == "Flip") EXPECT_EQ(c.choices.size(), 4);
        if (c.name == "HardwareBin" || c.name == "HighSpeedMode" || c.name == "MonoBin") EXPECT_EQ(c.choices.size(), 2);
        for (const auto& choice : c.choices) {
            EXPECT_GE(choice.value, c.minimum);
            EXPECT_LE(choice.value, c.maximum);
        }
    }
    // Restore the user's exact settings even when an assertion/SDK call fails.
    struct Restore {
        astro::ICamera& camera;
        astro::DeviceControl gain, exposure;
        ~Restore() {
            try {
                camera.abortCapture();
                camera.setLiveView(false);
                camera.setDeviceControl(gain.id, gain.value, gain.automatic);
                camera.setDeviceControl(exposure.id, exposure.value, exposure.automatic);
                camera.disconnect();
            } catch (const std::exception& e) { std::cerr << "Restore: " << e.what() << '\n'; }
        }
    } restore{*camera, *gain, *exposure};
    for (const auto& c : controls) {
        if (!c.writable) continue;
        camera->setDeviceControl(c.id, c.value, c.automatic);
        auto values = camera->deviceControls();
        auto value = std::find_if(values.begin(), values.end(), [&](const auto& v) { return v.id == c.id; });
        ASSERT_NE(value, values.end());
        EXPECT_EQ(value->automatic, c.automatic) << c.name;
        if (!c.automatic) EXPECT_EQ(value->value, c.value) << c.name;
    }
    std::cout << "All writable SDK controls: current-value write/readback passed\n";
    const auto testGain = gain->value < gain->maximum ? gain->value + 1 : gain->value - 1;
    camera->setDeviceControl(gain->id, testGain, false);
    auto actual = camera->deviceControls();
    auto readGain = std::find_if(actual.begin(), actual.end(), [&](const auto& c) { return c.id == gain->id; });
    ASSERT_NE(readGain, actual.end());
    EXPECT_EQ(readGain->value, testGain);
    camera->setDeviceControl(exposure->id, std::clamp<std::int64_t>(20000, exposure->minimum, exposure->maximum), false);
    auto frames = [&](int count) {
        camera->setLiveView(true);
        int received = 0;
        const auto start = std::chrono::steady_clock::now();
        while (received < count && std::chrono::steady_clock::now() - start < std::chrono::seconds(15)) {
            auto frame = camera->liveFrame();
            if (frame.jpeg.empty()) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }
            auto decoded = astro::decodeImage(frame.jpeg);
            EXPECT_GT(decoded.width, 0);
            EXPECT_GT(decoded.height, 0);
            EXPECT_EQ(decoded.rgba.size(), std::size_t(decoded.width) * decoded.height * 4);
            ++received;
        }
        EXPECT_EQ(received, count);
        std::cout << "Decoded real Live View JPEG frames: " << received << '\n';
    };
    frames(12);
    camera->setLiveView(false);
    auto directory = std::filesystem::temp_directory_path() /
        ("astro-asi-hardware-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    astro::SequenceItem item;
    camera->beginCapture(item, directory / "capture");
    astro::CaptureResult capture;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!capture.complete && std::chrono::steady_clock::now() < deadline) {
        capture = camera->pollCapture();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    ASSERT_TRUE(capture.complete);
    ASSERT_EQ(capture.files.size(), 1);
    EXPECT_GT(std::filesystem::file_size(capture.files[0]), 2880u);
    std::cout << "Real RAW16 FITS: " << capture.files[0] << '\n';
    frames(12);
}
TEST(AsiHardware, ApplicationWorkerPublishesNativeReadbackAndDecodedLiveFrames) {
    if (!std::getenv("ASTRO_ZWO_HARDWARE")) GTEST_SKIP() << "Set ASTRO_ZWO_HARDWARE=1 for a connected ASI camera";
    const auto directory = std::filesystem::temp_directory_path() /
        ("astro-asi-worker-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    astro::Mailbox mailbox;
    astro::Config config;
    config.cameraModule = "zwo_asi";
    config.language = "en";
    config.liveViewMs = 40;
    config.outputDirectory = directory;
    // Use the actual application worker, router, SDK, JPEG decoder and mailbox.
    astro::CameraWorker worker(mailbox, config, directory / "user.json",
        [](const astro::LiveFrame& frame, double, const std::vector<astro::StarSample>&, double) {
            auto pixels = astro::decodeImage(frame.jpeg);
            if (pixels.rgba.empty()) throw std::runtime_error("Empty actual Live View image");
            return astro::StarField{};
        });
    auto wait = [&](auto predicate) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        while (std::chrono::steady_clock::now() < deadline) {
            if (predicate()) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    };
    auto command = [&](astro::Action action, unsigned value) {
        astro::Command c;
        c.action = action;
        c.value = value;
        c.config = config;
        const auto id = mailbox.submit(c);
        EXPECT_TRUE(wait([&] { auto result = mailbox.result(id); return result && result->finished; }));
        const auto result = mailbox.result(id);
        if (result) EXPECT_TRUE(result->error.empty()) << result->error;
    };
    command(astro::Action::Connect, 0);
    ASSERT_TRUE(wait([&] { return mailbox.snapshot().connected; }));
    ASSERT_EQ(mailbox.snapshot().cameraModule.id, "zwo_asi");
    ASSERT_FALSE(mailbox.snapshot().deviceControls.empty());
    command(astro::Action::LiveView, 1);
    ASSERT_TRUE(wait([&] { return mailbox.snapshot().livePerformance.frames >= 12; }));
    const auto first = mailbox.snapshot();
    EXPECT_TRUE(first.liveView);
    EXPECT_FALSE(first.frame.jpeg.empty());
    ASSERT_TRUE(wait([&] { return mailbox.snapshot().livePerformance.analyses >= 2; }));
    for (const auto& entry : mailbox.snapshot().logs) EXPECT_NE(entry.level, "Error") << entry.message;
    // Stopping/restarting must leave the same core-owned session operational.
    command(astro::Action::LiveView, 0);
    command(astro::Action::LiveView, 1);
    EXPECT_TRUE(wait([&] { return mailbox.snapshot().frame.revision > first.frame.revision + 5; }));
    command(astro::Action::LiveView, 0);
    command(astro::Action::Disconnect, 0);
    std::cout << "Application worker: live frames, decoding, native control publication and stream restart passed\n";
}
