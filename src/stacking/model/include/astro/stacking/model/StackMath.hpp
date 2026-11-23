#pragma once
#include <optional>
#include <astro/core/stacking/model/LiveStack.hpp>
#include <span>
namespace astro::stacking {
// Online sigma rejection starts only once a pixel has this many samples: with fewer,
// the per-pixel variance estimate is too unreliable and rejects valid data.
inline constexpr int kMinimumRejectionSamples = 8;
struct Point {
    double x{}, y{}, flux{}, hfd{}, fwhm{};
};
struct Transform {
    double a{1}, b{}, tx{}, c{}, d{1}, ty{};
    Point apply(Point p) const { return {a * p.x + b * p.y + tx, c * p.x + d * p.y + ty}; }
};
bool plausible(const Transform&);
double overlap(const Transform&, int width, int height);
bool coverage(const std::vector<Point>&, int width, int height);
Transform fitSimilarity(const std::vector<std::pair<Point, Point>>&);
Transform fitAffine(const std::vector<std::pair<Point, Point>>&);
struct Alignment {
    bool accepted{};
    Transform transform;
    int matches{};
    double rms{}, overlap{};
    std::string reason;
};
struct LinearImage {
    int width{}, height{};
    std::vector<float> rgb;
    std::array<float, 3> previewGains{1, 1, 1};
    std::array<float, 4> whiteBalance{};
    std::uint64_t correctedPixels{};
};
struct BandingResult {
    double rms{};
    int limitedRows{};
};
BandingResult reduceHorizontalBanding(LinearImage&, const StackOptions&);
std::array<float, 3> normalizeBackground(LinearImage&, const std::array<float, 3>* reference, bool removeGradient);
struct DrizzleFrame {
    LinearImage image;
    std::vector<float> weights;
};
DrizzleFrame drizzle(const LinearImage&, const Transform&, double scale, double pixfrac);
// Estimated peak memory for a stack at the given drizzle scale.
std::uint64_t stackMemoryBytes(int width, int height, double drizzleScale);
// Effective memory budget: the configured MB, raised up to 75% of physical RAM.
std::uint64_t stackMemoryBudgetBytes(int configuredMb);
std::vector<Point> detectStars(const LinearImage&, double snr);
// Building blocks of detectStars, exposed for testing.
std::vector<float> luminance(const LinearImage&);
// Measures a candidate peak at (x,row); nullopt when clipped, unsupported or not star-shaped.
std::optional<Point> measureStar(const LinearImage&, const std::vector<float>& luminance, int x, int row,
                                 float background, float noise);
std::vector<Point> selectDistributedStars(std::vector<Point> stars, int width, int height);
Alignment align(const std::vector<Point>& reference, const std::vector<Point>& current, int width, int height,
                const StackOptions&);
// WB is frozen from the first RAW, before demosaic and camera-to-linear-sRGB conversion.
LinearImage decodeRaw(const std::filesystem::path&, int memoryLimitMb, const std::array<float, 4>& whiteBalance = {},
                      bool cosmetic = true);
std::size_t correctIsolatedBayerPixels(std::span<std::uint16_t> pixels, int width, int height, int stride, float black,
                                       float white);
class Accumulator {
public:
    void add(const LinearImage&, const Transform&, const StackOptions&);
    float value(std::size_t pixel, int channel) const;
    int sourceWidth() const { return sourceWidth_; }
    int sourceHeight() const { return sourceHeight_; }
    int width{}, height{}, frames{};
    double pixelScale{1}, pixfrac{1};
    std::uint64_t clipped{};            // rejected + winsorized pixels (kept for report compatibility)
    std::uint64_t rejectedSamples{};    // pixels dropped by sigma rejection
    std::uint64_t winsorizedSamples{};  // pixels clamped by winsorized rejection
    std::vector<std::uint32_t> count;
    std::vector<double> weights;
    std::array<float, 3> previewGains{1, 1, 1};

private:
    std::vector<double> mean_;
    std::vector<float> m2_;
    int sourceWidth_{}, sourceHeight_{};
};
StackPreview render(const Accumulator&, StackDisplay, std::uint64_t revision);
void writeFits(const std::filesystem::path&, const Accumulator&, double exposureSeconds);
void writeTiff(const std::filesystem::path&, const Accumulator&);
void writeXisf(const std::filesystem::path&, const Accumulator&, double exposureSeconds);
std::shared_ptr<IStackProcessor> makeStackProcessor();
}  // namespace astro::stacking
