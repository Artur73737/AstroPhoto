#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <astro/cameras/zwo/adapter/ZwoAdapter.hpp>
#include <astro/cameras/zwo/sdk/ZwoCamera.hpp>
#include <algorithm>
#include <cmath>
namespace astro::zwo {
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT h) {
    if (FAILED(h)) throw CameraError("ZWO preview encoding failed: " + std::to_string(h));
}
std::vector<std::uint8_t> jpeg(const Frame& f, const Device& device) {
    auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct Com {
        HRESULT hr;
        ~Com() {
            if (SUCCEEDED(hr)) CoUninitialize();
        }
    } com{hr};
    const int w = std::min(f.width, 1280), h = std::max(1, int(double(f.height) * w / f.width));
    std::vector<BYTE> rgb(std::size_t(w) * h * 3);
    auto sample = [&](int x, int y) {
        auto i = (std::size_t(y) * f.width + x) * 2;
        return unsigned(f.bytes[i]) | (unsigned(f.bytes[i + 1]) << 8);
    };
    std::vector<unsigned> levels;
    for (int y = 0; y < f.height; y += std::max(1, f.height / 128))
        for (int x = 0; x < f.width; x += std::max(1, f.width / 128))
            levels.push_back(sample(x, y));
    std::sort(levels.begin(), levels.end());
    double low = levels[levels.size() / 100], high = std::max(low + 32., double(levels[levels.size() * 999 / 1000]));
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int sx = x * f.width / w, sy = y * f.height / h;
            for (int c = 0; c < 3; ++c) {
                double value = 0;
                int count = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        int xx = std::clamp(sx + dx, 0, f.width - 1), yy = std::clamp(sy + dy, 0, f.height - 1);
                        char channel = device.color ? device.bayer[(yy & 1) * 2 + (xx & 1)] : "RGB"[c];
                        if (channel == "RGB"[c]) {
                            value += sample(xx, yy);
                            ++count;
                        }
                    }
                double v = count ? value / count : sample(sx, sy);
                rgb[(std::size_t(y) * w + x) * 3 + c] = BYTE(std::clamp((v - low) / (high - low), 0., 1.) * 255);
            }
        }
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    ComPtr<IStream> stream;
    check(CreateStreamOnHGlobal(nullptr, TRUE, &stream));
    ComPtr<IWICBitmapEncoder> encoder;
    check(factory->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &encoder));
    check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;
    check(encoder->CreateNewFrame(&frame, nullptr));
    check(frame->Initialize(nullptr));
    check(frame->SetSize(w, h));
    auto format = GUID_WICPixelFormat24bppRGB;
    check(frame->SetPixelFormat(&format));
    if (format != GUID_WICPixelFormat24bppRGB) throw CameraError("JPEG RGB encoder unavailable");
    check(frame->WritePixels(h, w * 3, UINT(rgb.size()), rgb.data()));
    check(frame->Commit());
    check(encoder->Commit());
    STATSTG stat{};
    check(stream->Stat(&stat, STATFLAG_NONAME));
    if (stat.cbSize.QuadPart > 16 * 1024 * 1024) throw CameraError("JPEG exceeds transfer limit");
    std::vector<std::uint8_t> result(std::size_t(stat.cbSize.QuadPart));
    LARGE_INTEGER zero{};
    check(stream->Seek(zero, STREAM_SEEK_SET, nullptr));
    ULONG read{};
    check(stream->Read(result.data(), ULONG(result.size()), &read));
    if (read != result.size()) throw CameraError("Truncated encoded preview");
    return result;
}
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
        for (auto& c : camera.controls())
            out.push_back(
                {c.id, c.name, c.description, c.minimum, c.maximum, c.value, c.automatic, c.autoSupported, c.writable});
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
        out.jpeg = jpeg(f, camera.device());
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
