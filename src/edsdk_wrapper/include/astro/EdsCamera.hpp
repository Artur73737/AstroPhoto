#pragma once
#include <astro/Core.hpp>
namespace astro::edsdk {
// The public boundary contains no Canon types. All SDK/COM state belongs to Impl.
class EdsCamera final : public ICamera {
public:
    EdsCamera(const Config&,Log);
    ~EdsCamera() override;
    std::string connect() override;
    void disconnect() override;
    bool connected() const override;
    void pump() override;
    std::vector<CameraProperty> properties() override;
    void set(Property,std::uint32_t) override;
    void prepareHost(std::uint64_t,SaveDestination) override;
    void beginCapture(const SequenceItem&,const std::filesystem::path&) override;
    CaptureResult pollCapture() override;
    void abortCapture() override;
    void setLiveView(bool) override;
    LiveFrame liveFrame() override;
    void focus(int) override;
    void zoom(int) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
