#pragma once
#include <nlohmann/json.hpp>
#include <vector>
#include <cstdint>
namespace astro {
std::string base64Image(const std::vector<std::uint8_t>&);
nlohmann::json pngImage(std::vector<std::uint8_t> rgba,int width,int height,bool flip=false,void* nativeWindow=nullptr);
}
