#include <astro/imaging/preview/processing/LiveProcessor.hpp>
#include <astro/imaging/preview/live/LiveViewPreview.hpp>
#include <astro/imaging/analysis/histogram/ImageAnalysis.hpp>
#include <astro/imaging/peaking/edges/FocusPeaking.hpp>
#include <condition_variable>
#include <thread>
namespace astro {
struct LiveProcessor::Impl {
    std::mutex mutex;
    std::condition_variable_any ready;
    std::optional<LiveProcessingRequest> pending;
    std::unique_ptr<ProcessedLive> result;
    std::jthread thread;
    Impl()
        : thread([this](std::stop_token stop) {
              FocusTracker tracker;
              while (!stop.stop_requested()) {
                  LiveProcessingRequest request;
                  {
                      std::unique_lock lock(mutex);
                      if (!ready.wait(lock, stop, [&] { return pending.has_value(); })) break;
                      request = std::move(*pending);
                      pending.reset();
                  }
                  auto out = std::make_unique<ProcessedLive>();
                  out->revision = request.frame.revision;
                  out->peaking = request.peaking;
                  try {
                      auto start = std::chrono::steady_clock::now();
                      auto elapsed = [&] {
                          auto now = std::chrono::steady_clock::now();
                          auto ms = std::chrono::duration<double, std::milli>(now - start).count();
                          start = now;
                          return ms;
                      };
                      out->original = prepareLiveViewPreview(decodeImage(request.frame.jpeg), request.frame);
                      out->decodeMs = elapsed();
                      out->histogram = imageHistogram(out->original);
                      out->histogramMs = elapsed();
                      if (request.peaking) out->marked = focusPeaking(out->original, request.threshold, request.stars);
                      out->focus = tracker.update(
                          measureFocus(out->original, request.target, request.peaking ? &out->marked : nullptr),
                          request.frame.revision, request.context);
                      out->peakingMs = elapsed();
                  } catch (const std::exception& e) {
                      out->error = e.what();
                  }
                  {
                      std::lock_guard lock(mutex);
                      result = std::move(out);
                  }
              }
          }) {}
    ~Impl() {
        thread.request_stop();
        ready.notify_all();
    }
};
LiveProcessor::LiveProcessor() : impl_(std::make_unique<Impl>()) {}
LiveProcessor::~LiveProcessor() = default;
void LiveProcessor::request(LiveProcessingRequest r) {
    {
        std::lock_guard lock(impl_->mutex);
        impl_->pending = std::move(r);
    }
    impl_->ready.notify_one();
}
std::unique_ptr<ProcessedLive> LiveProcessor::take() {
    std::lock_guard lock(impl_->mutex);
    return std::move(impl_->result);
}
}  // namespace astro
