#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <astro/cameras/zwo/preview/SensorPreview.hpp>
#include <astro/cameras/zwo/sdk/ZwoCamera.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::zwo {
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT h) {
    if (FAILED(h)) throw std::runtime_error("ZWO preview encoding failed: " + std::to_string(h));
}
}  // namespace
std::vector<std::uint8_t> previewJpeg(const Frame& f, const Device& device) {
    if (f.width <= 0 || f.height <= 0 || f.width > 32768 || f.height > 32768 || f.format != 2 ||
        f.bytes.size() != std::size_t(f.width) * f.height * 2 || (device.color && device.bayer.size() != 4))
        throw std::invalid_argument("Invalid RAW16 preview frame / Bayer pattern");
    auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) check(hr);
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
                // WIC's JPEG encoder negotiates BGR, not RGB. Keep the Bayer
                // channels correct while packing the byte order it actually accepts.
                rgb[(std::size_t(y) * w + x) * 3 + (2 - c)] = BYTE(std::clamp((v - low) / (high - low), 0., 1.) * 255);
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
    auto format = GUID_WICPixelFormat24bppBGR;
    check(frame->SetPixelFormat(&format));
    if (format != GUID_WICPixelFormat24bppBGR) throw std::runtime_error("JPEG BGR encoder unavailable");
    check(frame->WritePixels(h, w * 3, UINT(rgb.size()), rgb.data()));
    check(frame->Commit());
    check(encoder->Commit());
    STATSTG stat{};
    check(stream->Stat(&stat, STATFLAG_NONAME));
    if (stat.cbSize.QuadPart > 16 * 1024 * 1024) throw std::runtime_error("JPEG exceeds transfer limit");
    std::vector<std::uint8_t> result(std::size_t(stat.cbSize.QuadPart));
    LARGE_INTEGER zero{};
    check(stream->Seek(zero, STREAM_SEEK_SET, nullptr));
    ULONG read{};
    check(stream->Read(result.data(), ULONG(result.size()), &read));
    if (read != result.size()) throw std::runtime_error("Truncated encoded preview");
    return result;
}
}  // namespace astro::zwo
