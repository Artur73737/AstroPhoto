#pragma once
#include <astro/Core.hpp>
#include <future>

namespace astro {
struct PreviewPixels { int width{},height{};std::vector<std::uint8_t> rgba;std::string description; };
PreviewPixels decodePreview(const std::filesystem::path&);
PreviewPixels decodeImage(const std::vector<std::uint8_t>&);
class PhotoPreview {
public:
    void request(const CapturedPhoto&);
    std::optional<PreviewPixels> poll();
    const std::string& status() const {return status_;}
private:
    void launch();
    CapturedPhoto pending_;
    std::uint64_t requested_{},loading_{};
    std::future<PreviewPixels> task_;
    std::string status_;
};
}
