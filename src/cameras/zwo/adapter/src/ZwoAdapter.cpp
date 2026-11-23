#include <astro/cameras/zwo/adapter/ZwoAdapter.hpp>
#include <astro/cameras/zwo/sdk/ZwoCamera.hpp>
#include <astro/cameras/zwo/preview/SensorPreview.hpp>
#include <astro/cameras/zwo/recording/FrameRecorder.hpp>
#include <sstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <climits>
namespace astro::zwo {
namespace {
class Adapter final : public ICamera {
    Camera camera;
    bool live{}, pending{};
    std::filesystem::path file;
    std::uint64_t revision{};
    Config::ZwoCapture options;
    int exposureFormat{2};
    std::string outputFormat{"fits"};
    std::vector<Control> currentControls;
    std::unique_ptr<FrameRecorder> recorder;
    FrameRecordingOptions recordingOptions;
    FrameRecordingStatus lastRecording;
    SteadyTime recordingStarted{}, lastRecordingPreview{};
    std::uint64_t dropBaseline{}, recordingDrops{};
    StreamExposure exposureProgress;
    bool recordingActive() const { return recorder && recorder->status().active; }

public:
    explicit Adapter(const Config& config) : options(config.zwo) {}
    std::string connect() override {
        auto devices = Camera::enumerate();
        if (devices.empty()) throw CameraError("No ZWO ASI camera detected", false, true);
        camera.open(devices.front().id);
        auto d = camera.device();
        if (d.color && d.bayer.size() != 4) {
            camera.close();
            throw CameraError("Unsupported ZWO Bayer pattern");
        }
        configureCapture(options);
        currentControls.clear();
        return d.name;
    }
    CameraModuleInfo activeModule() const override { return {"zwo_asi", "ZWO ASI", {1}, {}, "FITS RAW16", false}; }
    void disconnect() override {
        if (recorder) {
            recorder->finish();
            lastRecording = recorder->status();
            recorder.reset();
        }
        camera.close();
        live = pending = false;
    }
    bool connected() const override { return camera.connected(); }
    std::optional<CaptureSettings> captureSettings() override {
        const auto device = camera.device();
        const auto geometry = camera.geometry();
        auto value = options;
        value.width = geometry.width;
        value.height = geometry.height;
        value.bin = geometry.bin;
        value.pixelFormat = geometry.format;
        value.x = geometry.x;
        value.y = geometry.y;
        CaptureSettings result{value, device.width, device.height, device.bins};
        const char* names[]{"RAW8", "RGB24", "RAW16", "MONO8"};
        for (int format : device.formats)
            if (format >= 0 && format < 4) result.pixelFormats.push_back({unsigned(format), names[format]});
        result.outputFormats = {"fits", "png", "tiff", "ser"};
        return result;
    }
    void configureCapture(const Config::ZwoCapture& requested) override {
        if (recordingActive()) throw CameraError("Stop continuous capture before changing camera settings");
        Config validated;
        validated.zwo = requested;
        validate(validated);
        if (pending) throw CameraError("Capture in progress");
        const auto old = camera.geometry();
        const auto previous = options;
        const bool resume = live;
        const auto oldControls = camera.controls();
        if (resume) camera.stream(false);
        try {
            const auto d = camera.device();
            const auto w = requested.width ? requested.width : (d.width / requested.bin) / 8 * 8;
            const auto h = requested.height ? requested.height : (d.height / requested.bin) / 2 * 2;
            camera.roi(w, h, requested.bin, requested.pixelFormat, requested.x, requested.y);
            for (const auto& c : camera.controls()) {
                if (c.id == 13 && c.writable) camera.set(c.id, requested.hardwareBin ? 1 : 0, false);
                if (c.id == 14 && c.writable) camera.set(c.id, requested.highSpeed ? 1 : 0, false);
            }
            options = requested;
            currentControls.clear();
            if (resume) {
                camera.stream(true);
                exposureProgress.started = std::chrono::steady_clock::now();
            }
        } catch (...) {
            try {
                camera.roi(old.width, old.height, old.bin, old.format, old.x, old.y);
                for (const auto& c : oldControls)
                    if ((c.id == 13 || c.id == 14) && c.writable) camera.set(c.id, c.value, c.automatic);
                options = previous;
                currentControls.clear();
                if (resume) {
                    camera.stream(true);
                    exposureProgress.started = std::chrono::steady_clock::now();
                }
            } catch (...) {
                live = false;
                throw CameraError("ZWO reconfiguration and rollback failed; reconnect camera");
            }
            throw;
        }
    }
    void pump() override {}
    std::vector<DeviceControl> deviceControls() override {
        std::vector<DeviceControl> out;
        currentControls = camera.controls();
        for (auto& c : currentControls) {
            if (c.id == 1) exposureProgress.seconds = c.value / 1000000.;
            out.push_back(
                {c.id, c.name, c.description, c.minimum, c.maximum, c.value, c.automatic, c.autoSupported, c.writable});
            auto& control = out.back();
            control.category = c.category;
            control.unit = c.unit;
            control.displayScale = c.displayScale;
            control.defaultValue = c.defaultValue;
            control.logarithmic = c.logarithmic;
            for (auto& [value, label] : c.choices)
                control.choices.push_back({std::uint32_t(value), label});
        }
        return out;
    }
    void setDeviceControl(int id, std::int64_t value, bool automatic) override {
        if (recordingActive()) throw CameraError("Stop continuous capture before changing camera settings");
        if (pending) throw CameraError("Capture in progress");
        if (value < LONG_MIN || value > LONG_MAX) throw CameraError("Native control exceeds SDK range");
        const bool restart = live && (id == 9 || id == 13 || id == 14 || id == 18);
        if (restart) camera.stream(false);
        try {
            camera.set(id, long(value), automatic);
            if (restart) {
                camera.stream(true);
                exposureProgress.started = std::chrono::steady_clock::now();
            }
        } catch (...) {
            if (restart) try {
                    camera.stream(true);
                } catch (...) {
                    live = false;
                }
            throw;
        }
        if (id == 13) options.hardwareBin = value != 0;
        if (id == 14) options.highSpeed = value != 0;
        if (id == 1) {
            exposureProgress.seconds = camera.controlValue(1).first / 1000000.;
            exposureProgress.started = std::chrono::steady_clock::now();
        }
        currentControls.clear();
    }
    std::vector<CameraProperty> properties() override {
        const auto format = camera.geometry().format;
        const char* names[]{"RAW8", "RGB24", "RAW16", "MONO8"};
        std::vector<CameraProperty> out{{Property::ExposureMode, 1, {}, false, "M - Manual"},
                                        {Property::ImageQuality, unsigned(format), {}, false, names[format]}};
        if (currentControls.empty()) currentControls = camera.controls();
        for (auto& c : currentControls)
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
        if (recordingActive()) throw CameraError("Continuous capture in progress");
        if (pending || live) throw CameraError("Stop video before still exposure");
        if (item.bulb) throw CameraError("ASI uses native timed exposure, not Canon Bulb");
        file = stem;
        outputFormat = item.scientificRaw ? "fits" : item.outputFormat;
        file += "." + outputFormat;
        exposureFormat = camera.geometry().format;
        if (item.scientificRaw && exposureFormat != 2) {
            const auto g = camera.geometry();
            camera.roi(g.width, g.height, g.bin, 2, g.x, g.y);
        }
        try {
            camera.fixedExposureSeconds();
            camera.expose(item.type == FrameType::Dark || item.type == FrameType::Bias);
        } catch (...) {
            const auto g = camera.geometry();
            if (g.format != exposureFormat) camera.roi(g.width, g.height, g.bin, exposureFormat, g.x, g.y);
            throw;
        }
        pending = true;
    }
    CaptureResult pollCapture() override {
        if (!pending) return {};
        if (!camera.exposureReady()) return {};
        auto frame = camera.download();
        if (outputFormat == "fits")
            camera.saveFits(file, frame);
        else {
            auto d = camera.device();
            sensor::save(file, frame, d.color ? d.bayer : "", d.name, outputFormat);
        }
        const auto g = camera.geometry();
        if (g.format != exposureFormat) camera.roi(g.width, g.height, g.bin, exposureFormat, g.x, g.y);
        pending = false;
        return {true, {file}};
    }
    void abortCapture() override {
        camera.abort();
        if (pending) {
            const auto g = camera.geometry();
            if (g.format != exposureFormat) camera.roi(g.width, g.height, g.bin, exposureFormat, g.x, g.y);
        }
        pending = false;
    }
    void setLiveView(bool enabled) override {
        if (!enabled && recordingActive()) throw CameraError("Stop continuous capture before stopping Live View");
        if (pending) throw CameraError("Still exposure in progress");
        camera.stream(enabled);
        if (enabled && !live) {
            exposureProgress.started = std::chrono::steady_clock::now();
            exposureProgress.seconds = camera.controlValue(1).first / 1000000.;
        }
        live = enabled;
    }
    LiveFrame liveFrame() override {
        if (!live) return {};
        auto f = camera.frame(0);
        if (f.bytes.empty()) return {};
        exposureProgress.started = std::chrono::steady_clock::now();
        ++exposureProgress.frame;
        if (recorder && recorder->status().active) {
            const auto dropped = camera.droppedFrames();
            recordingDrops = dropped - std::min(dropBaseline, dropped);
            recorder->dropped(recordingDrops);
            const auto exposure = camera.controlValue(1), gain = camera.controlValue(0);
            exposureProgress.seconds = exposure.first / 1000000.;
            recorder->push(f, std::chrono::system_clock::now(),
                           {exposure.first, gain.first, exposure.second, gain.second});
        }
        // Poll native frames even without saving; only JPEG delivery is capped.
        const double fps = options.previewFps > 0 ? options.previewFps : recordingActive() ? 60. : 0.;
        if (fps > 0 &&
            std::chrono::duration<double>(std::chrono::steady_clock::now() - lastRecordingPreview).count() < 1. / fps)
            return {};
        lastRecordingPreview = std::chrono::steady_clock::now();
        LiveFrame out;
        auto device = camera.device();
        if (!options.debayerPreview) device.color = false;
        out.jpeg = previewJpeg(f, device);
        out.revision = ++revision;
        out.coordinateWidth = f.width;
        out.coordinateHeight = f.height;
        out.zoomRect = {0, 0, f.width, f.height};
        return out;
    }
    void startFrameRecording(const FrameRecordingOptions& requested, const std::filesystem::path& directory,
                             std::uint64_t reserve) override {
        if (recordingActive() || pending) throw CameraError("Camera acquisition already active");
        if (!live) throw CameraError("Continuous capture requires running Live View");
        const auto g = camera.geometry();
        const auto d = camera.device();
        nlohmann::json controls = nlohmann::json::array();
        for (const auto& c : camera.controls()) {
            controls.push_back({{"id", c.id},
                                {"name", c.name},
                                {"value", c.value},
                                {"automatic", c.automatic},
                                {"minimum", c.minimum},
                                {"maximum", c.maximum}});
        }
        const auto meta =
            nlohmann::json{{"controls_snapshot_at_start", controls}, {"roi", {{"x", g.x}, {"y", g.y}, {"bin", g.bin}}}}
                .dump();
        recorder = std::make_unique<FrameRecorder>(requested, directory, sensor::Frame{g.width, g.height, g.format, {}},
                                                   d.color ? d.bayer : "", d.name, meta, reserve);
        recordingOptions = requested;
        recordingStarted = std::chrono::steady_clock::now();
        lastRecordingPreview = {};
        dropBaseline = camera.droppedFrames();
        recordingDrops = 0;
    }
    void stopFrameRecording() override {
        if (recorder) recorder->stop();
    }
    void failFrameRecording(const std::string& error) override {
        if (recordingActive()) recorder->fail(error);
    }
    FrameRecordingStatus frameRecordingStatus() override {
        if (!recorder) return lastRecording;
        auto status = recorder->status();
        if (status.active && recordingOptions.seconds > 0 &&
            std::chrono::duration<double>(std::chrono::steady_clock::now() - recordingStarted).count() >=
                recordingOptions.seconds)
            recorder->stop();
        status = recorder->status();
        status.sdkDropped = recordingDrops;
        return status;
    }
    StreamExposure streamExposure() override {
        auto status = exposureProgress;
        status.active = live && !pending;
        return status;
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
std::unique_ptr<ICamera> createAdapter(const Config& config) {
    return std::make_unique<Adapter>(config);
}
}  // namespace astro::zwo
