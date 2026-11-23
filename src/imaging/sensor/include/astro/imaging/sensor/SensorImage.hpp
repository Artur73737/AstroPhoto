#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <filesystem>
namespace astro::sensor {
enum Format { Raw8 = 0, Bgr24 = 1, Raw16 = 2, Mono8 = 3 };
struct Frame {
    int width{}, height{}, format{};
    std::vector<std::uint8_t> bytes;
};
struct Display {
    int width{}, height{};
    std::vector<std::uint8_t> bgr;
};
void validate(const Frame&);
Display display(const Frame&, const std::string& bayer, int maxWidth = 1280);
void save(const std::filesystem::path&, const Frame&, const std::string& bayer, const std::string& instrument,
          const std::string& format);
struct SerFrame {
    Frame frame;
    std::string bayer;
};
SerFrame readSer(const std::filesystem::path&);
}  // namespace astro::sensor
