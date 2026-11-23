#pragma once
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <astro/core/imaging/model/PreviewAnalysis.hpp>
#include <memory>
namespace astro {
struct LiveProcessingRequest {
    LiveFrame frame;
    bool peaking{}, stars{};
    float threshold{};
    FocusTarget target;
    std::uint64_t context{};
};
struct ProcessedLive {
    PreviewPixels original, marked;
    Histogram histogram;
    FocusReading focus;
    std::uint64_t revision{};
    bool peaking{};
    double decodeMs{}, histogramMs{}, peakingMs{};
    std::string error;
};
// One running frame plus one replaceable pending frame; callers never wait for
// image processing, and old work cannot create an unbounded display queue.
class LiveProcessor {
public:
    LiveProcessor();
    ~LiveProcessor();
    void request(LiveProcessingRequest);
    std::unique_ptr<ProcessedLive> take();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace astro
