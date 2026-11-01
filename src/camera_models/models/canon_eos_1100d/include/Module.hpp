#pragma once
#include <astro/CameraModule.hpp>
namespace astro::camera_models {
class CanonEos1100d final:public CameraModule {
public:
    CameraModuleInfo info() const override;
    std::span<const std::string_view> modelNames() const override;
    std::span<const Property> properties() const override;
    CapturePolicy capturePolicy(std::uint32_t) const override;
    LensFocusState lensState(std::uint32_t,std::uint32_t) const override;
    bool manualExposureForFocus(std::uint32_t,std::uint32_t,std::uint32_t) const override;
    std::uint32_t focusDrive(int) const override;
    std::uint32_t pointAutofocusMode() const override {return 1;} // Canon Live single-point contrast AF.
    double pointAutofocusFrameFraction() const override {return .2;} // Legacy EVF AF/zoom frame.
    PreviewRegion previewRegion(const LiveFrame&) const override;
};
}
