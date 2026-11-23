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
    if (device.color && device.bayer.size() != 4) throw std::invalid_argument("Invalid Bayer pattern");
    auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) check(hr);
    struct Com {
        HRESULT hr;
        ~Com() {
            if (SUCCEEDED(hr)) CoUninitialize();
        }
    } com{hr};
    auto pixels = sensor::display(f, device.color ? device.bayer : "");
    const int w = pixels.width, h = pixels.height;
    auto& rgb = pixels.bgr;
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
