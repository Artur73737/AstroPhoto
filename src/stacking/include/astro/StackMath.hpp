#pragma once
#include <astro/LiveStack.hpp>
#include <span>
namespace astro::stacking {
struct Point {double x{},y{},flux{};};
struct Transform {double a{1},b{},tx{},c{},d{1},ty{};Point apply(Point p)const{return {a*p.x+b*p.y+tx,c*p.x+d*p.y+ty};}};
bool plausible(const Transform&);
double overlap(const Transform&,int width,int height);
bool coverage(const std::vector<Point>&,int width,int height);
Transform fitSimilarity(const std::vector<std::pair<Point,Point>>&);
struct Alignment {bool accepted{};Transform transform;int matches{};double rms{},overlap{};std::string reason;};
struct LinearImage {int width{},height{};std::vector<float> rgb;std::array<float,3> previewGains{1,1,1};std::array<float,4> whiteBalance{};std::uint64_t correctedPixels{};};
struct BandingResult {double rms{};int limitedRows{};};
BandingResult reduceHorizontalBanding(LinearImage&,const StackOptions&);
std::vector<Point> detectStars(const LinearImage&,double snr);
Alignment align(const std::vector<Point>& reference,const std::vector<Point>& current,int width,int height,const StackOptions&);
// WB is frozen from the first RAW, before demosaic and camera-to-linear-sRGB conversion.
LinearImage decodeRaw(const std::filesystem::path&,int memoryLimitMb,const std::array<float,4>& whiteBalance={},bool cosmetic=true);
std::size_t correctIsolatedBayerPixels(std::span<std::uint16_t> pixels,int width,int height,int stride,float black,float white);
class Accumulator {
public:
    void add(const LinearImage&,const Transform&,const StackOptions&);
    float value(std::size_t pixel,int channel)const;
    int width{},height{},frames{};
    std::uint64_t clipped{};
    std::vector<std::uint32_t> count;
    std::array<float,3> previewGains{1,1,1};
private:
    std::vector<double> mean_;
    std::vector<float> m2_;
};
StackPreview render(const Accumulator&,StackDisplay,std::uint64_t revision);
void writeFits(const std::filesystem::path&,const Accumulator&,double exposureSeconds);
std::shared_ptr<IStackProcessor> makeStackProcessor();
}
