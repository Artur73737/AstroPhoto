#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <astro/imaging/sensor/SensorImage.hpp>
#include <fstream>
#include <array>
#include <chrono>
#include <stdexcept>
#include <algorithm>
#include <nlohmann/json.hpp>
namespace astro::sensor {
namespace {
using Microsoft::WRL::ComPtr;
void check(HRESULT r) {
    if (FAILED(r)) throw std::runtime_error("Sensor image encoder: " + std::to_string(r));
}
void put(std::ostream& out, std::uint64_t value, int bytes) {
    for (int i = 0; i < bytes; ++i)
        out.put(char(value >> (8 * i)));
}
std::uint32_t word(const std::array<unsigned char, 178>& h, int pos) {
    return unsigned(h[pos]) | (unsigned(h[pos + 1]) << 8) | (unsigned(h[pos + 2]) << 16) | (unsigned(h[pos + 3]) << 24);
}
}  // namespace
void save(const std::filesystem::path& file, const Frame& f, const std::string& bayer, const std::string& instrument,
          const std::string& format) {
    validate(f);
    if (std::filesystem::exists(file)) throw std::runtime_error("Refusing to overwrite capture");
    auto temporary = file;
    temporary += ".tmp";
    auto metadataFile = file;
    metadataFile += ".json";
    if (std::filesystem::exists(metadataFile)) throw std::runtime_error("Refusing to overwrite capture metadata");
    if (std::filesystem::exists(temporary)) throw std::runtime_error("Capture temporary file already exists");
    struct Cleanup {
        std::filesystem::path p;
        ~Cleanup() {
            std::error_code e;
            std::filesystem::remove(p, e);
        }
    } cleanup{temporary};
    if (format == "ser") {
        int color = f.format == Bgr24 ? 101 : 0;
        const std::string patterns[]{"RGGB", "GRBG", "GBRG", "BGGR"};
        if (f.format != Bgr24 && f.format != Mono8 && !bayer.empty()) {
            auto it = std::find(std::begin(patterns), std::end(patterns), bayer);
            if (it == std::end(patterns)) throw std::invalid_argument("Unsupported SER CFA");
            color = 8 + int(it - std::begin(patterns));
        }
        std::ofstream out(temporary, std::ios::binary);
        out.write("LUCAM-RECORDER", 14);
        put(out, 0, 4);
        put(out, color, 4);
        // SER Player/PIPP interoperability: 0 denotes little-endian samples.
        put(out, 0, 4);
        put(out, f.width, 4);
        put(out, f.height, 4);
        put(out, f.format == Raw16 ? 16 : 8, 4);
        put(out, 1, 4);
        std::string empty(40, 0), name = instrument.substr(0, 40);
        for (auto& c : name)
            if (c < 32 || c > 126) c = '?';
        name.resize(40, 0);
        out.write(empty.data(), 40);
        out.write(name.data(), 40);
        out.write(empty.data(), 40);
        // No timestamp claim without an acquisition timestamp supplied by the caller.
        put(out, 0, 8);
        put(out, 0, 8);
        out.write(reinterpret_cast<const char*>(f.bytes.data()), f.bytes.size());
        out.flush();
        if (!out) throw std::runtime_error("SER write failed");
        out.close();
    } else {
        if (format != "png" && format != "tiff") throw std::invalid_argument("Unsupported image export");
        const auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) check(hr);
        struct Com {
            bool own;
            ~Com() {
                if (own) CoUninitialize();
            }
        } com{SUCCEEDED(hr)};
        ComPtr<IWICImagingFactory> factory;
        check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
        ComPtr<IWICStream> stream;
        check(factory->CreateStream(&stream));
        check(stream->InitializeFromFilename(temporary.c_str(), GENERIC_WRITE));
        ComPtr<IWICBitmapEncoder> encoder;
        check(factory->CreateEncoder(format == "png" ? GUID_ContainerFormatPng : GUID_ContainerFormatTiff, nullptr,
                                     &encoder));
        check(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache));
        ComPtr<IWICBitmapFrameEncode> image;
        check(encoder->CreateNewFrame(&image, nullptr));
        check(image->Initialize(nullptr));
        check(image->SetSize(f.width, f.height));
        auto pixel = f.format == Raw16   ? GUID_WICPixelFormat16bppGray
                     : f.format == Bgr24 ? GUID_WICPixelFormat24bppBGR
                                         : GUID_WICPixelFormat8bppGray;
        const auto requested = pixel;
        check(image->SetPixelFormat(&pixel));
        if (!IsEqualGUID(pixel, requested)) throw std::runtime_error("Encoder cannot preserve native sensor precision");
        ComPtr<IWICBitmap> bitmap;
        const UINT stride = f.width * (f.format == Raw16 ? 2 : f.format == Bgr24 ? 3 : 1);
        check(factory->CreateBitmapFromMemory(f.width, f.height, requested, stride, UINT(f.bytes.size()),
                                              const_cast<BYTE*>(f.bytes.data()), &bitmap));
        ComPtr<IWICFormatConverter> converter;
        check(factory->CreateFormatConverter(&converter));
        check(converter->Initialize(bitmap.Get(), pixel, WICBitmapDitherTypeNone, nullptr, 0,
                                    WICBitmapPaletteTypeCustom));
        check(image->WriteSource(converter.Get(), nullptr));
        check(image->Commit());
        check(encoder->Commit());
        image.Reset();
        encoder.Reset();
        stream.Reset();
    }
    std::filesystem::rename(temporary, file);
    std::ofstream metadata(metadataFile);
    metadata << nlohmann::json{{"width", f.width},
                               {"height", f.height},
                               {"pixel_format", f.format},
                               {"bayer", f.format == Mono8 || f.format == Bgr24 ? "" : bayer},
                               {"instrument", instrument},
                               {"sample_bits", f.format == Raw16 ? 16 : 8}}
                    .dump(2);
    metadata.flush();
    if (!metadata) throw std::runtime_error("Capture metadata write failed");
}
SerFrame readSer(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::array<unsigned char, 178> h{};
    if (!in.read(reinterpret_cast<char*>(h.data()), h.size()) ||
        std::string(reinterpret_cast<char*>(h.data()), 14) != "LUCAM-RECORDER")
        throw std::runtime_error("Invalid SER header");
    const auto width = word(h, 26), height = word(h, 30), depth = word(h, 34), count = word(h, 38), color = word(h, 18);
    if (width == 0 || height == 0 || width > 32768 || height > 32768 || !count || !depth || depth > 16 ||
        std::uint64_t(width) * height * (color >= 100 ? 3 : 1) * (depth > 8 ? 2 : 1) > 512ull * 1024 * 1024)
        throw std::runtime_error("SER frame exceeds preview bounds");
    if (color != 0 && (color < 8 || color > 11) && color != 100 && color != 101)
        throw std::runtime_error("Unsupported SER color mode");
    if (color >= 100 && depth > 8) throw std::runtime_error("16-bit RGB SER preview is not supported");
    SerFrame result;
    auto& f = result.frame;
    f.width = int(width);
    f.height = int(height);
    f.format = color >= 100 ? Bgr24 : depth > 8 ? Raw16 : Raw8;
    const std::string patterns[]{"RGGB", "GRBG", "GBRG", "BGGR"};
    if (color >= 8 && color <= 11) result.bayer = patterns[color - 8];
    f.bytes.resize(std::size_t(width) * height * (color >= 100 ? 3 : depth > 8 ? 2 : 1));
    if (!in.read(reinterpret_cast<char*>(f.bytes.data()), f.bytes.size()))
        throw std::runtime_error("Truncated SER frame");
    if (f.format == Raw16 && word(h, 22) == 1)
        for (std::size_t i = 0; i < f.bytes.size(); i += 2)
            std::swap(f.bytes[i], f.bytes[i + 1]);
    if (color == 100)
        for (std::size_t i = 0; i < f.bytes.size(); i += 3)
            std::swap(f.bytes[i], f.bytes[i + 2]);
    return result;
}
}  // namespace astro::sensor
