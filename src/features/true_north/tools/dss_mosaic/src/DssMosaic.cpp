#include <astro/features/true_north/astronomy/TrueNorth.hpp>
#include <fstream>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include <cmath>
using namespace astro::north;
struct Tile {
    Wcs w;
    double zero{}, scale{1};
    std::vector<short> pixels;
};
Tile read(const std::string& file) {
    std::ifstream in(file, std::ios::binary);
    Tile t;
    std::map<std::string, double> h;
    std::size_t cards = 0;
    for (;;) {
        char card[81]{};
        if (!in.read(card, 80)) throw std::runtime_error("Truncated DSS FITS header: " + file);
        ++cards;
        std::string s(card, 80);
        auto key = s.substr(0, 8);
        key.erase(key.find_last_not_of(' ') + 1);
        if (key == "END") break;
        if (s[8] == '=') {
            try {
                h[key] = std::stod(s.substr(10));
            } catch (...) {
            }
        }
        if (cards > 4096) throw std::runtime_error("FITS header too large");
    }
    if (h.at("BITPIX") != 16) throw std::runtime_error("DSS tile must be signed 16-bit");
    t.w = {h.at("CRPIX1"),
           h.at("CRPIX2"),
           h.at("CRVAL1"),
           h.at("CRVAL2"),
           {h.at("CD1_1"), h.at("CD1_2"), h.at("CD2_1"), h.at("CD2_2")},
           int(h.at("NAXIS1")),
           int(h.at("NAXIS2"))};
    if (t.w.width < 1 || t.w.height < 1 || std::uint64_t(t.w.width) * t.w.height > 100000000)
        throw std::runtime_error("Invalid FITS dimensions");
    if (h.contains("BZERO")) t.zero = h.at("BZERO");
    if (h.contains("BSCALE")) t.scale = h.at("BSCALE");
    in.seekg((cards * 80 + 2879) / 2880 * 2880);
    t.pixels.resize(std::size_t(t.w.width) * t.w.height);
    for (auto& p : t.pixels) {
        unsigned char bytes[2];
        if (!in.read(reinterpret_cast<char*>(bytes), 2)) throw std::runtime_error("Truncated DSS tile data");
        p = short((unsigned(bytes[0]) << 8) | bytes[1]);
    }
    return t;
}
int main(int argc, char** argv) {
    try {
        if (argc != 6) throw std::runtime_error("Usage: dss_mosaic output.fits tile0 tile1 tile2 tile3");
        std::vector<Tile> tiles;
        for (int i = 2; i < 6; ++i)
            tiles.push_back(read(argv[i]));
        Wcs output{800.5, 800.5, 40, 88.7, {-.001875, 0, 0, .001875}, 1600, 1600};
        std::ofstream out(argv[1], std::ios::binary);
        std::string header;
        auto card = [&](std::string key, std::string value) {
            key.resize(8, ' ');
            auto s = key + "= " + value;
            s.resize(80, ' ');
            header += s;
        };
        card("SIMPLE", "T");
        card("BITPIX", "16");
        card("NAXIS", "2");
        card("NAXIS1", "1600");
        card("NAXIS2", "1600");
        card("EXTEND", "T");
        card("CRPIX1", "800.5");
        card("CRPIX2", "800.5");
        card("CRVAL1", "40");
        card("CRVAL2", "88.7");
        card("CTYPE1", "'RA---TAN'");
        card("CTYPE2", "'DEC--TAN'");
        card("CD1_1", "-0.001875");
        card("CD1_2", "0");
        card("CD2_1", "0");
        card("CD2_2", "0.001875");
        std::string end = "END";
        end.resize(80, ' ');
        header += end;
        header.resize((header.size() + 2879) / 2880 * 2880, ' ');
        out.write(header.data(), header.size());
        std::vector<char> coverage(1600 * 1600);
        int gaps = 0;
        for (int y = 1; y <= 1600; ++y)
            for (int x = 1; x <= 1600; ++x) {
                auto sky = skyAt(output, {double(x), double(y)});
                double value = 0;
                int n = 0;
                for (auto& t : tiles) {
                    auto p = pixelAt(t.w, sky);
                    if (!p) continue;
                    int ix = int(std::lround(p->x)) - 1, iy = int(std::lround(p->y)) - 1;
                    if (ix >= 0 && iy >= 0 && ix < t.w.width && iy < t.w.height) {
                        value += t.pixels[std::size_t(iy) * t.w.width + ix] * t.scale + t.zero;
                        ++n;
                    }
                }
                coverage[(y - 1) * 1600 + x - 1] = n ? 1 : 0;
                if (!n) ++gaps;
                short v = short(std::clamp(n ? value / n : 0., -32768., 32767.));
                out.put(char(std::uint16_t(v) >> 8));
                out.put(char(v));
            }
        std::string padding((2880 - (1600 * 1600 * 2) % 2880) % 2880, 0);
        out.write(padding.data(), padding.size());
        header.clear();
        card("XTENSION", "'IMAGE   '");
        card("BITPIX", "8");
        card("NAXIS", "2");
        card("NAXIS1", "1600");
        card("NAXIS2", "1600");
        card("PCOUNT", "0");
        card("GCOUNT", "1");
        card("EXTNAME", "'COVERAGE'");
        header += end;
        header.resize((header.size() + 2879) / 2880 * 2880, ' ');
        out.write(header.data(), header.size());
        out.write(coverage.data(), coverage.size());
        std::string maskPadding((2880 - coverage.size() % 2880) % 2880, 0);
        out.write(maskPadding.data(), maskPadding.size());
        if (!out) throw std::runtime_error("Mosaic write failed");
        std::cout << "DSS2 real-tile reprojection: 1600x1600, 3 degree TAN, missing pixels " << gaps << '\n';
        return gaps > 1600 * 1600 / 100 ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
