#include <astro/cameras/zwo/adapter/ZwoAdapter.hpp>
#include <astro/cameras/zwo/sdk/ZwoCamera.hpp>
#include <astro/cameras/zwo/preview/SensorPreview.hpp>
#include <algorithm>
#include <climits>
namespace astro::zwo {
namespace {
class Adapter final : public ICamera {
    Camera camera;
    bool live{}, pending{};
    std::filesystem::path file;
    std::uint64_t revision{};

public:
    std::string connect() override {
        auto devices = Camera::enumerate();
        if (devices.empty()) throw CameraError("No ZWO ASI camera detected", false, true);
        camera.open(devices.front().id);
        auto d = camera.device();
        if (d.color && d.bayer.size() != 4) {
            camera.close();
            throw CameraError("Unsupported ZWO Bayer pattern");
        }
        camera.roi(d.width - d.width % 8, d.height - d.height % 2, 1, 2);
        return d.name;
    }
    CameraModuleInfo activeModule() const override { return {"zwo_asi", "ZWO ASI", {1}, {}, "FITS RAW16", false}; }
    void disconnect() override {
        camera.close();
        live = pending = false;
    }
    bool connected() const override { return camera.connected(); }
    void pump() override {}
    std::vector<DeviceControl> deviceControls() override {
        std::vector<DeviceControl> out;
        for (auto& c : camera.controls()) {
            out.push_back(
                {c.id, c.name, c.description, c.minimum, c.maximum, c.value, c.automatic, c.autoSupported, c.writable});
            auto& control = out.back();
            control.category = c.category;
            control.unit = c.unit;
            control.displayScale = c.displayScale;
            control.defaultValue = c.defaultValue;
            control.logarithmic = c.logarithmic;
            for (auto& [value, label] : c.choices) control.choices.push_back({std::uint32_t(value), label});
        }
        return out;
    }
    void setDeviceControl(int id, std::int64_t value, bool automatic) override {
        if (pending) throw CameraError("Capture in progress");
        if (value < LONG_MIN || value > LONG_MAX) throw CameraError("Native control exceeds SDK range");
        camera.set(id, long(value), automatic);
    }
    std::vector<CameraProperty> properties() override {
        std::vector<CameraProperty> out{{Property::ExposureMode, 1, {}, false, "M - Manual"},
                                        {Property::ImageQuality, 1, {}, false, "RAW16 FITS"}};
        for (auto& c : camera.controls())
            if (c.id == 0 || c.id == 1) {
                auto id = c.id == 0 ? Property::Gain : Property::ExposureMicroseconds;
                out.push_back({id, std::uint32_t(c.value), {}, false, c.automatic ? "Auto" : std::to_string(c.value)});
            }
        return out;
    }
    void set(Property p, std::uint32_t v) override {
        if (p == Property::Gain)
            setDeviceControl(0, v, false);
        else if (p == Property::ExposureMicroseconds)
            setDeviceControl(1, v, false);
        else
            throw CameraError("Unsupported ZWO property; use native controls");
    }
    void prepareHost(std::uint64_t available, SaveDestination destination) override {
        if (destination != SaveDestination::PcOnly) throw CameraError("ZWO ASI saves to PC only");
        auto d = camera.device();
        if (available < std::uint64_t(d.width) * d.height * 2) throw CameraError("Insufficient capture storage");
    }
    void beginCapture(const SequenceItem& item, const std::filesystem::path& stem) override {
        if (pending || live) throw CameraError("Stop video before still exposure");
        if (item.bulb) throw CameraError("ASI uses native timed exposure, not Canon Bulb");
        file = stem;
        file += ".fits";
        camera.fixedExposureSeconds();
        camera.expose(item.type == FrameType::Dark || item.type == FrameType::Bias);
        pending = true;
    }
    CaptureResult pollCapture() override {
        if (!pending) return {};
        if (!camera.exposureReady()) return {};
        auto frame = camera.download();
        camera.saveFits(file, frame);
        pending = false;
        return {true, {file}};
    }
    void abortCapture() override {
        camera.abort();
        pending = false;
    }
    void setLiveView(bool enabled) override {
        if (pending) throw CameraError("Still exposure in progress");
        camera.stream(enabled);
        live = enabled;
    }
    LiveFrame liveFrame() override {
        if (!live) return {};
        auto f = camera.frame(0);
        if (f.bytes.empty()) return {};
        LiveFrame out;
        out.jpeg = previewJpeg(f, camera.device());
        out.revision = ++revision;
        out.coordinateWidth = f.width;
        out.coordinateHeight = f.height;
        out.zoomRect = {0, 0, f.width, f.height};
        return out;
    }
    void focus(int) override { throw CameraError("ASI camera has no lens motor; external focuser is required"); }
    LensFocusStatus lensFocusStatus() override {
        return {LensFocusState::Unsupported, "ASI camera has no integrated lens motor"};
    }
    bool manualExposureForFocus() override {
        try {
            camera.fixedExposureSeconds();
            return true;
        } catch (...) {
            return false;
        }
    }
    void zoom(int ratio) override {
        if (ratio != 1) throw CameraError("Canon EVF zoom is unavailable on ZWO");
    }
};
}  // namespace
bool available() {
    return !Camera::enumerate().empty();
}
std::unique_ptr<ICamera> createAdapter() {
    return std::make_unique<Adapter>();
}
}  // namespace astro::zwo
