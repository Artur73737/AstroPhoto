#pragma once
#include <astro/core/domain/Core.hpp>
#include <astro/imaging/sensor/SensorImage.hpp>
namespace astro::zwo {
struct FrameReadback {
    std::int64_t exposureUs{}, gain{};
    bool autoExposure{}, autoGain{};
};
// Disk I/O has its own bounded owner. No SDK or UI calls occur on this thread.
class FrameRecorder {
public:
    FrameRecorder(FrameRecordingOptions, std::filesystem::path directory, sensor::Frame geometry, std::string bayer,
                  std::string device, std::string metadata, std::uint64_t reserveBytes);
    ~FrameRecorder();
    bool push(const sensor::Frame&, std::chrono::system_clock::time_point, FrameReadback = {});
    void stop();
    void finish();
    void fail(const std::string&);
    void dropped(std::uint64_t);
    FrameRecordingStatus status() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace astro::zwo
