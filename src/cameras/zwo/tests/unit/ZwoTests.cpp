#include <astro/cameras/zwo/sdk/ZwoCamera.hpp>
#include <gtest/gtest.h>
#include <astro/imaging/fits/FitsImage.hpp>
#include <atomic>
#include <thread>
#include <fstream>
#include <chrono>
TEST(Zwo, OwnerThreadAndDisconnectedCommandsAreEnforced) {
    astro::zwo::Camera c;
    EXPECT_FALSE(c.connected());
    EXPECT_THROW(c.stream(true), std::logic_error);
    EXPECT_THROW(c.set(0, 1, false), std::logic_error);
    std::atomic<bool> rejected{};
    std::thread other([&] {
        try {
            c.connected();
        } catch (const std::logic_error&) {
            rejected = true;
        }
    });
    other.join();
    EXPECT_TRUE(rejected);
}
TEST(Fits, Raw16PreservesUnsignedEndpointsAndNeverOverwrites) {
    astro::zwo::Camera c;
    astro::zwo::Frame f{2, 2, 2, {0, 0, 255, 127, 0, 128, 255, 255}};
    auto file = std::filesystem::temp_directory_path() /
                ("astro-zwo-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".fits");
    astro::fits::writeRaw16(file,f.width,f.height,f.bytes,{});
    std::ifstream in(file, std::ios::binary);
    std::string bytes{std::istreambuf_iterator<char>(in), {}};
    ASSERT_EQ(bytes.size(), 5760);
    EXPECT_EQ(bytes.substr(0, 6), "SIMPLE");
    EXPECT_NE(bytes.find("BZERO"), std::string::npos);
    EXPECT_EQ(static_cast<unsigned char>(bytes[2880]), 128);
    EXPECT_EQ(static_cast<unsigned char>(bytes[2882]), 255);
    EXPECT_EQ(static_cast<unsigned char>(bytes[2884]), 0);
    EXPECT_EQ(static_cast<unsigned char>(bytes[2886]), 127);
    auto decoded=astro::fits::read(file,1024);
    EXPECT_EQ(decoded.samples,(std::vector<float>{0,32767,32768,65535}));
    EXPECT_THROW(astro::fits::read(file,4),std::runtime_error);
    in.close();
    EXPECT_THROW(astro::fits::writeRaw16(file,f.width,f.height,f.bytes,{}), std::runtime_error);
    std::filesystem::remove(file);
    auto tmp = file;
    tmp += ".tmp";
    std::filesystem::remove(tmp);
}
