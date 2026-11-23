#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <cstdint>
namespace astro::fits {
struct Image {
    int width{}, height{}, channels{}, bitpix{};
    std::map<std::string, std::string> metadata;
    std::vector<float> samples;
};
bool isFits(const std::filesystem::path&);
Image read(const std::filesystem::path&, std::size_t memoryBytes);
void writeRaw16(const std::filesystem::path&, int width, int height, const std::vector<std::uint8_t>& littleEndian,
                const std::map<std::string, std::string>& metadata, int channels = 1);
}  // namespace astro::fits
