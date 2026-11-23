#pragma once
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
namespace astro::zwo {
struct Device {
    int id{}, width{}, height{}, bitDepth{};
    bool color{}, cooled{};
    std::string name;
    std::vector<int> bins, formats;
    std::string bayer;
};
struct Control {
    int id{};
    std::string name, description;
    long minimum{}, maximum{}, value{};
    bool automatic{}, autoSupported{}, writable{};
};
struct Frame {
    int width{}, height{}, format{};
    std::vector<std::uint8_t> bytes;
};
// All operations, including destruction, must use the constructing owner thread.
class Camera {
public:
    Camera();
    ~Camera();
    Camera(const Camera&) = delete;
    Camera& operator=(const Camera&) = delete;
    static std::vector<Device> enumerate();
    void open(int id);
    void close();
    bool connected() const;
    Device device() const;
    std::vector<Control> controls();
    void set(int control, long value, bool automatic);
    void roi(int width, int height, int bin, int format, int x = 0, int y = 0);
    void stream(bool);
    Frame frame(int timeoutMs = 50);
    double fixedExposureSeconds();
    void expose(bool dark = false);
    bool exposureReady();
    Frame download();
    void abort();
    void saveFits(const std::filesystem::path&, const Frame&);

private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
}  // namespace astro::zwo
