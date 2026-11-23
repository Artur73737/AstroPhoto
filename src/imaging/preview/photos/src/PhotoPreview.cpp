#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <libraw/libraw.h>
#include <astro/imaging/preview/photos/PhotoPreview.hpp>
#include <astro/imaging/fits/FitsImage.hpp>
#include <astro/imaging/sensor/SensorImage.hpp>
#include <nlohmann/json.hpp>
#include <astro/stacking/calibration/Calibration.hpp>
#include <algorithm>
#include <fstream>
#include <limits>

namespace astro {
using Microsoft::WRL::ComPtr;
namespace {
void check(HRESULT result) {
    if (FAILED(result)) throw std::runtime_error("Lettura anteprima WIC fallita: " + std::to_string(result));
}
void rawCheck(int result) {
    if (result != LIBRAW_SUCCESS) throw std::runtime_error(std::string("Anteprima RAW: ") + libraw_strerror(result));
}
PreviewPixels decodeSource(IWICBitmapSource* source, IWICImagingFactory* factory) {
    UINT width{}, height{};
    check(source->GetSize(&width, &height));
    if (!width || !height || width > 16384 || height > 16384)
        throw std::runtime_error("Dimensioni anteprima non valide");
    ComPtr<IWICFormatConverter> converter;
    check(factory->CreateFormatConverter(&converter));
    check(converter->Initialize(source, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                                WICBitmapPaletteTypeCustom));
    PreviewPixels result;
    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    result.rgba.resize(static_cast<std::size_t>(width) * height * 4);
    check(converter->CopyPixels(nullptr, width * 4, static_cast<UINT>(result.rgba.size()), result.rgba.data()));
    return result;
}
}  // namespace
PreviewPixels decodeImage(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty() || bytes.size() > std::numeric_limits<DWORD>::max())
        throw std::runtime_error("Immagine vuota o troppo grande");
    // ZWO's owner thread has no Canon COM session. Decoding must initialize
    // its own apartment and must also accept an existing STA apartment.
    const auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) check(hr);
    struct ComGuard {
        bool owns;
        ~ComGuard() {
            if (owns) CoUninitialize();
        }
    } guard{SUCCEEDED(hr)};
    ComPtr<IWICImagingFactory> factory;
    check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;
    check(factory->CreateStream(&stream));
    check(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size())));
    ComPtr<IWICBitmapDecoder> decoder;
    check(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder));
    ComPtr<IWICBitmapFrameDecode> frame;
    check(decoder->GetFrame(0, &frame));
    return decodeSource(frame.Get(), factory.Get());
}
PreviewPixels decodePreview(const std::filesystem::path& path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (extension == ".ser") {
        auto ser = sensor::readSer(path);
        auto pixels = sensor::display(ser.frame, ser.bayer, 4096);
        PreviewPixels out;
        out.width = pixels.width;
        out.height = pixels.height;
        out.description = "SER first frame";
        out.rgba.resize(std::size_t(out.width) * out.height * 4);
        for (std::size_t i = 0; i < pixels.bgr.size() / 3; ++i) {
            for (int c = 0; c < 3; ++c)
                out.rgba[i * 4 + c] = pixels.bgr[i * 3 + 2 - c];
            out.rgba[i * 4 + 3] = 255;
        }
        return out;
    }
    if (fits::isFits(path)) {
        auto image = fits::read(path, 512ull * 1024 * 1024);
        if (image.channels == 1 && image.bitpix == 16 && image.metadata.contains("BAYERPAT") &&
            image.metadata.at("BAYERPAT") != "MONO") {
            // Display demosaicing needs pixels/CFA, not calibration metadata.
            // Generic captured FITS may legitimately omit Gain or Exposure.
            stacking::SensorFrame sensor;
            sensor.width = image.width;
            sensor.height = image.height;
            sensor.wb = {1, 1, 1, 1};
            sensor.matrix = {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
            sensor.pixels = image.samples;
            const auto pattern = image.metadata.at("BAYERPAT");
            if (pattern.size() != 4) throw std::runtime_error("Invalid FITS Bayer pattern");
            for (int i = 0; i < 4; ++i) {
                auto c = std::string("RGB").find(pattern[i]);
                if (c == std::string::npos) throw std::runtime_error("Invalid FITS Bayer pattern");
                sensor.cfa[i] = int(c);
            }
            for (auto& value : sensor.pixels)
                value /= 65535.f;
            auto color = stacking::demosaicPixels(sensor, {});
            image.samples.resize(std::size_t(image.width) * image.height * 3);
            image.channels = 3;
            for (std::size_t i = 0; i < color.rgb.size() / 3; ++i)
                for (int c = 0; c < 3; ++c)
                    image.samples[std::size_t(c) * image.width * image.height + i] = color.rgb[i * 3 + c];
        }
        PreviewPixels out;
        out.width = image.width;
        out.height = image.height;
        out.description = "FITS display stretch (scientific samples unchanged)";
        std::vector<float> levels;
        for (std::size_t i = 0; i < image.samples.size(); i += std::max(std::size_t(1), image.samples.size() / 32768))
            levels.push_back(image.samples[i]);
        std::sort(levels.begin(), levels.end());
        auto low = levels[levels.size() / 100], high = std::max(low + 1e-6f, levels[levels.size() * 999 / 1000]);
        out.rgba.resize(std::size_t(out.width) * out.height * 4);
        for (std::size_t i = 0; i < out.rgba.size() / 4; ++i) {
            for (int c = 0; c < 3; ++c) {
                auto v = image.samples[i + (image.channels == 3 ? std::size_t(c) * out.width * out.height : 0)];
                out.rgba[i * 4 + c] = std::uint8_t(std::clamp((v - low) / (high - low), 0.f, 1.f) * 255);
            }
            out.rgba[i * 4 + 3] = 255;
        }
        return out;
    }
    const auto hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) check(hr);
    struct ComGuard {
        bool owns;
        ~ComGuard() {
            if (owns) CoUninitialize();
        }
    } guard{SUCCEEDED(hr)};
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    PreviewPixels result;
    if (ext == ".cr2") {
        auto raw = std::make_unique<LibRaw>();
        rawCheck(raw->open_file(path.c_str()));
        rawCheck(raw->unpack_thumb());
        int error{};
        auto* thumbnail = raw->dcraw_make_mem_thumb(&error);
        std::unique_ptr<libraw_processed_image_t, decltype(&LibRaw::dcraw_clear_mem)> image(thumbnail,
                                                                                            &LibRaw::dcraw_clear_mem);
        rawCheck(error);
        if (!image) throw std::runtime_error("Anteprima CR2 assente");
        if (image->type == LIBRAW_IMAGE_JPEG)
            result = decodeImage({image->data, image->data + image->data_size});
        else if (image->type == LIBRAW_IMAGE_BITMAP && image->bits == 8 && image->colors == 3) {
            result.width = image->width;
            result.height = image->height;
            const auto count = static_cast<std::size_t>(result.width) * result.height;
            if (!count || image->data_size < count * 3) throw std::runtime_error("Anteprima CR2 incompleta");
            result.rgba.resize(count * 4);
            for (std::size_t i = 0; i < count; ++i) {
                std::copy_n(image->data + i * 3, 3, result.rgba.data() + i * 4);
                result.rgba[i * 4 + 3] = 255;
            }
        } else
            throw std::runtime_error("Formato anteprima CR2 non supportato");
        result.description = "Anteprima incorporata CR2";
    } else {
        ComPtr<IWICImagingFactory> factory;
        check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)));
        ComPtr<IWICBitmapDecoder> decoder;
        check(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad,
                                                 &decoder));
        ComPtr<IWICBitmapFrameDecode> frame;
        check(decoder->GetFrame(0, &frame));
        result = decodeSource(frame.Get(), factory.Get());
        result.description = "Immagine scaricata";
        auto metadata = path;
        metadata += ".json";
        if (std::filesystem::exists(metadata) && std::filesystem::file_size(metadata) < 65536) {
            std::ifstream input(metadata);
            auto info = nlohmann::json::parse(input);
            const auto bayer = info.value("bayer", std::string{});
            if (bayer.size() == 4 && info.value("width", 0) == result.width &&
                info.value("height", 0) == result.height) {
                sensor::Frame raw{result.width, result.height, sensor::Raw8,
                                  std::vector<std::uint8_t>(result.rgba.size() / 4)};
                for (std::size_t i = 0; i < raw.bytes.size(); ++i)
                    raw.bytes[i] = result.rgba[i * 4];
                auto color = sensor::display(raw, bayer, 0);
                for (std::size_t i = 0; i < raw.bytes.size(); ++i)
                    for (int c = 0; c < 3; ++c)
                        result.rgba[i * 4 + c] = color.bgr[i * 3 + 2 - c];
            }
        }
    }
    const auto name = path.filename().u8string();
    result.description += " | " + std::string(name.begin(), name.end());
    return result;
}
void PhotoPreview::request(const CapturedPhoto& photo) {
    if (!photo.revision || photo.revision == requested_) return;
    requested_ = photo.revision;
    {
        std::lock_guard lock(mutex_);
        ready_.reset();
        status_ = "Caricamento dello scatto...";
    }
    if (photo.files.empty()) {
        std::lock_guard lock(mutex_);
        status_ = "Nessun file scaricato";
        return;
    }
    auto file = photo.files.front();
    for (const auto& candidate : photo.files) {
        auto ext = candidate.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".jpg" || ext == ".jpeg") {
            file = candidate;
            break;
        }
    }
    try {
        auto pixels = decodePreview(file);
        pixels.revision = photo.revision;
        std::lock_guard lock(mutex_);
        status_ = pixels.description;
        ready_ = std::move(pixels);
    } catch (const std::exception& e) {
        std::lock_guard lock(mutex_);
        status_ = e.what();
    }
}
std::optional<PreviewPixels> PhotoPreview::poll() {
    std::lock_guard lock(mutex_);
    auto result = std::move(ready_);
    ready_.reset();
    return result;
}
std::string PhotoPreview::status() const {
    std::lock_guard lock(mutex_);
    return status_;
}
}  // namespace astro
