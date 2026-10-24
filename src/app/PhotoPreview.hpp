#pragma once
#include <astro/Core.hpp>
#include <mutex>

namespace astro {
struct PreviewPixels { int width{},height{};std::vector<std::uint8_t> rgba;std::string description;std::uint64_t revision{}; };
PreviewPixels decodePreview(const std::filesystem::path&);
PreviewPixels decodeImage(const std::vector<std::uint8_t>&);
class PhotoPreview {
public:
    void request(const CapturedPhoto&);
    std::optional<PreviewPixels> poll();
    std::string status() const;
private:
    // request() runs on the core worker. UI only polls the completed image.
    std::uint64_t requested_{};
    mutable std::mutex mutex_;
    std::optional<PreviewPixels> ready_;
    std::string status_;
};
}
