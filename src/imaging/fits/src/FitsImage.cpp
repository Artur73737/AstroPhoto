#include <astro/imaging/fits/FitsImage.hpp>
#include <fstream>
#include <bit>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <atomic>
#include <cctype>
namespace astro::fits {
namespace {
std::string trim(std::string s) {
    auto a = s.find_first_not_of(" \r\n"), b = s.find_last_not_of(" \r\n");
    return a == s.npos ? "" : s.substr(a, b - a + 1);
}
std::uint32_t word(const unsigned char* b) {
    return (unsigned(b[0]) << 24) | (unsigned(b[1]) << 16) | (unsigned(b[2]) << 8) | b[3];
}
}  // namespace
bool isFits(const std::filesystem::path& file) {
    auto e = file.extension().string();
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return e == ".fits" || e == ".fit" || e == ".fts";
}
Image read(const std::filesystem::path& file, std::size_t budget) {
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open FITS");
    Image out;
    std::size_t cards = 0;
    for (;;) {
        char b[80];
        if (!in.read(b, 80)) throw std::runtime_error("Truncated FITS header");
        ++cards;
        if (cards > 8192) throw std::runtime_error("FITS header exceeds limit");
        std::string line(b, 80), key = trim(line.substr(0, 8));
        if (cards == 1 && key != "SIMPLE") throw std::runtime_error("Not a primary FITS image");
        if (key == "END") break;
        if (line[8] != '=') continue;
        auto value = trim(line.substr(10));
        if (!value.empty() && value.front() == '\'') {
            auto end = value.find('\'', 1);
            if (end == value.npos) throw std::runtime_error("Invalid FITS string");
            value = trim(value.substr(1, end - 1));
        } else {
            auto slash = value.find('/');
            if (slash != value.npos) value = trim(value.substr(0, slash));
        }
        out.metadata[key] = value;
    }
    auto integer = [&](const char* key) { return std::stoi(out.metadata.at(key)); };
    int axes = integer("NAXIS");
    out.width = integer("NAXIS1");
    out.height = integer("NAXIS2");
    out.channels = axes == 3 ? integer("NAXIS3") : 1;
    out.bitpix = integer("BITPIX");
    if ((axes != 2 && axes != 3) || (out.channels != 1 && out.channels != 3) || out.width < 1 || out.height < 1 ||
        (out.bitpix != 8 && out.bitpix != 16 && out.bitpix != -32))
        throw std::runtime_error("Unsupported FITS primary image layout");
    auto count = std::uint64_t(out.width) * out.height * out.channels;
    if (count > budget / sizeof(float) || count > std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("FITS exceeds configured memory budget");
    double zero = out.metadata.contains("BZERO") ? std::stod(out.metadata.at("BZERO")) : 0,
           scale = out.metadata.contains("BSCALE") ? std::stod(out.metadata.at("BSCALE")) : 1;
    if (!std::isfinite(zero) || !std::isfinite(scale) || scale == 0) throw std::runtime_error("Invalid FITS scaling");
    in.seekg((cards * 80 + 2879) / 2880 * 2880);
    out.samples.resize(std::size_t(count));
    const int bytes = std::abs(out.bitpix) / 8;
    for (auto& sample : out.samples) {
        unsigned char b[4]{};
        if (!in.read(reinterpret_cast<char*>(b), bytes)) throw std::runtime_error("Truncated FITS pixels");
        double value = out.bitpix == 8    ? b[0]
                       : out.bitpix == 16 ? double(std::int16_t((unsigned(b[0]) << 8) | b[1]))
                                          : double(std::bit_cast<float>(word(b)));
        sample = float(value * scale + zero);
        if (!std::isfinite(sample)) throw std::runtime_error("Non-finite FITS sample");
    }
    return out;
}
void writeRaw16(const std::filesystem::path& file, int width, int height, const std::vector<std::uint8_t>& bytes,
                const std::map<std::string, std::string>& metadata, int channels) {
    if (width < 1 || height < 1 || std::uint64_t(width) * height > 256ull * 1024 * 1024 ||
        (channels != 1 && channels != 3) || bytes.size() != std::size_t(width) * height * 2 * channels)
        throw std::invalid_argument("Invalid RAW16 FITS payload");
    if (std::filesystem::exists(file)) throw std::runtime_error("Refusing to overwrite FITS");
    auto tmp = file;
    static std::atomic<unsigned long long> serial{};
    tmp += ".tmp-" + std::to_string(++serial);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } cleanup{tmp};
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    std::string header;
    auto card = [&](std::string key, std::string value) {
        if (key.size() > 8 || value.find_first_of("\r\n") != value.npos)
            throw std::invalid_argument("Invalid FITS card");
        key.resize(8, ' ');
        auto text = key + "= " + value;
        if (text.size() > 80) throw std::invalid_argument("FITS card exceeds 80 columns");
        text.resize(80, ' ');
        header += text;
    };
    card("SIMPLE", "T");
    card("BITPIX", "16");
    card("NAXIS", channels == 3 ? "3" : "2");
    card("NAXIS1", std::to_string(width));
    card("NAXIS2", std::to_string(height));
    if (channels == 3) card("NAXIS3", "3");
    card("BZERO", "32768");
    card("BSCALE", "1");
    for (auto& [key, value] : metadata) {
        if (key == "SIMPLE" || key == "BITPIX" || key == "NAXIS" || key == "NAXIS1" || key == "NAXIS2" ||
            key == "NAXIS3" || key == "BZERO" || key == "BSCALE")
            throw std::invalid_argument("Reserved FITS metadata key");
        card(key, value);
    }
    std::string end = "END";
    end.resize(80, ' ');
    header += end;
    header.resize((header.size() + 2879) / 2880 * 2880, ' ');
    out.write(header.data(), header.size());
    for (int c = 0; c < channels; ++c)
        for (std::size_t sample = 0; sample < std::size_t(width) * height; ++sample) {
            const auto i = (sample * channels + c) * 2;
            unsigned v = (unsigned(bytes[i]) | (unsigned(bytes[i + 1]) << 8)) ^ 0x8000;
            out.put(char(v >> 8));
            out.put(char(v));
        }
    std::string pad((2880 - bytes.size() % 2880) % 2880, 0);
    out.write(pad.data(), pad.size());
    out.flush();
    if (!out) throw std::runtime_error("FITS write failed");
    out.close();
    std::filesystem::rename(tmp, file);
}
}  // namespace astro::fits
