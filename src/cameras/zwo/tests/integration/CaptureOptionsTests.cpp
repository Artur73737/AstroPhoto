#include <astro/cameras/zwo/adapter/ZwoAdapter.hpp>
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <astro/imaging/fits/FitsImage.hpp>
#include <gtest/gtest.h>
#include <cstdlib>
#include <iostream>
#include <thread>
TEST(AsiHardware, GeometryPixelFormatsOutputsAndAcquisitionLocks) {
    if (!std::getenv("ASTRO_ZWO_HARDWARE")) GTEST_SKIP() << "Physical ASI required";
    auto camera = astro::zwo::createAdapter();
    std::cout << camera->connect() << '\n';
    const auto original = *camera->captureSettings();
    const auto before = camera->deviceControls();
    auto find = [&](int id) {
        return *std::find_if(before.begin(), before.end(), [&](const auto& c) { return c.id == id; });
    };
    const auto gain = find(0), exposure = find(1);
    struct Restore {
        astro::ICamera& c;
        astro::CaptureSettings original;
        astro::DeviceControl gain, exposure;
        ~Restore() {
            try {
                c.abortCapture();
                c.setLiveView(false);
                c.configureCapture(original.value);
                c.setDeviceControl(0, gain.value, gain.automatic);
                c.setDeviceControl(1, exposure.value, exposure.automatic);
                c.disconnect();
            } catch (const std::exception& e) {
                std::cerr << e.what() << '\n';
            }
        }
    } restore{*camera, original, gain, exposure};
    EXPECT_FALSE(original.value.hardwareBin);
    EXPECT_TRUE(original.value.highSpeed);
    for (const auto& c : before) {
        if (c.id == 13) EXPECT_EQ(c.value, 0);
        if (c.id == 14) EXPECT_EQ(c.value, 1);
    }
    camera->setDeviceControl(0, 0, false);
    camera->setDeviceControl(1, 1000, false);
    auto frame = [&] {
        astro::LiveFrame result;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
        while (result.jpeg.empty() && std::chrono::steady_clock::now() < deadline) {
            result = camera->liveFrame();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        if (result.jpeg.empty()) throw std::runtime_error("No real video frame");
        return result;
    };
    auto directory =
        std::filesystem::temp_directory_path() /
        ("asi-capture-options-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    // Every exposed writable native option is round-tripped using its actual
    // current value; no guessed range or camera profile is injected.
    for (const auto& control : camera->deviceControls()) {
        if (!control.writable) continue;
        camera->setDeviceControl(control.id, control.value, control.automatic);
        auto actual = camera->deviceControls();
        auto found = std::find_if(actual.begin(), actual.end(), [&](const auto& c) { return c.id == control.id; });
        ASSERT_NE(found, actual.end());
        if (!control.automatic) EXPECT_EQ(found->value, control.value) << control.name;
        EXPECT_EQ(found->automatic, control.automatic) << control.name;
        std::cout << "Native control read/write: " << control.name << '\n';
    }
    int shot = 0;
    for (const auto& pixel : original.pixelFormats) {
        auto settings = original.value;
        settings.width = std::min(640, original.sensorWidth / 8 * 8);
        settings.height = std::min(480, original.sensorHeight / 2 * 2);
        settings.x = (original.sensorWidth - settings.width) / 2;
        settings.y = (original.sensorHeight - settings.height) / 2;
        settings.pixelFormat = int(pixel.value);
        camera->configureCapture(settings);
        camera->setLiveView(true);
        auto live = frame();
        auto preview = astro::decodeImage(live.jpeg);
        EXPECT_EQ(preview.width, settings.width);
        EXPECT_EQ(preview.height, settings.height);
        EXPECT_EQ(camera->captureSettings()->value.pixelFormat, pixel.value);
        auto invalid = settings;
        invalid.width = 13;
        EXPECT_THROW(camera->configureCapture(invalid), std::exception);
        EXPECT_FALSE(frame().jpeg.empty());
        std::cout << "Real pixel mode / rollback: " << pixel.label << '\n';
        for (const auto& format : original.outputFormats) {
            camera->setLiveView(false);
            astro::SequenceItem item;
            item.outputFormat = format;
            camera->beginCapture(item, directory / ('s' + std::to_string(++shot)));
            EXPECT_THROW(camera->setDeviceControl(0, 1, false), std::exception);
            EXPECT_THROW(camera->configureCapture(settings), std::exception);
            EXPECT_THROW(camera->setLiveView(true), std::exception);
            astro::CaptureResult capture;
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(8);
            while (!capture.complete && std::chrono::steady_clock::now() < deadline) {
                capture = camera->pollCapture();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            ASSERT_TRUE(capture.complete);
            ASSERT_EQ(capture.files.size(), 1);
            const auto picture = astro::decodePreview(capture.files.front());
            EXPECT_EQ(picture.width, settings.width);
            EXPECT_EQ(picture.height, settings.height);
            if (format == "fits") {
                auto fits = astro::fits::read(capture.files.front(), 32 * 1024 * 1024);
                EXPECT_EQ(fits.channels, pixel.value == 1 ? 3 : 1);
            }
            camera->setLiveView(true);
            EXPECT_FALSE(frame().jpeg.empty());
            std::cout << pixel.label << " -> " << format << " captured, decoded, video resumed\n";
        }
        camera->setLiveView(false);
    }
    for (int bin : original.bins) {
        auto settings = original.value;
        settings.bin = bin;
        settings.width = (original.sensorWidth / bin) / 8 * 8;
        settings.height = (original.sensorHeight / bin) / 2 * 2;
        settings.x = settings.y = 0;
        camera->configureCapture(settings);
        camera->setLiveView(true);
        auto actual = camera->captureSettings()->value;
        EXPECT_EQ(actual.bin, bin);
        EXPECT_EQ(frame().coordinateWidth, settings.width);
        camera->setLiveView(false);
        std::cout << "Real bin " << bin << " passed\n";
    }
    std::cout << "Captured " << shot << " physical frames into " << directory << '\n';
}
