#pragma once
#include <astro/Core.hpp>
#include <span>
#include <string_view>

namespace astro::camera_models {
// Model policy only: no SDK handles, threads, UI or hardware calls.
// EDSDK remains the sole owner of sessions, callbacks and command execution.
struct CapturePolicy {
    std::uint32_t bulbTv{}, takePictureCommand{}, bulbStartCommand{}, bulbEndCommand{};
    int expectedFiles{};
};
class CameraModule {
public:
    virtual ~CameraModule()=default;
    virtual CameraModuleInfo info() const=0;
    virtual std::span<const std::string_view> modelNames() const=0;
    virtual std::span<const Property> properties() const=0;
    virtual CapturePolicy capturePolicy(std::uint32_t imageQuality) const=0;
    virtual LensFocusState lensState(std::uint32_t attached,std::uint32_t afMode) const=0;
    virtual bool manualExposureForFocus(std::uint32_t ae,std::uint32_t iso,std::uint32_t tv) const=0;
    virtual std::uint32_t focusDrive(int signedStep) const=0;
    virtual PreviewRegion previewRegion(const LiveFrame&) const=0;
};
std::span<const CameraModule* const> modules();
const CameraModule& find(std::string_view id);
// Exact names, normalized only for case/whitespace. No substring/family fallback.
const CameraModule& resolve(std::string_view selection,std::string_view detectedModel);
void validateSelection(std::string_view selection);
}
