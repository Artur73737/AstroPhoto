#pragma once
#include <astro/StackMath.hpp>
#include <memory>
#include <functional>
namespace astro::stacking {
// Black-subtracted, normalized sensor samples. CFA positions are in active-area coordinates.
struct SensorFrame {
    int width{},height{},iso{};double exposure{};std::string model;
    std::array<int,4> cfa{};
    std::array<float,4> wb{};
    std::array<std::array<float,3>,3> matrix{};
    std::vector<float> pixels;
};
SensorFrame readSensor(const std::filesystem::path&,int memoryMb);
void validateSensorMatch(const SensorFrame&,const SensorFrame&,bool exposure);
SensorFrame meanMaster(const std::vector<SensorFrame>&); // deterministic numerical validation
void addToMaster(SensorFrame&,const SensorFrame&,int count);
void writeSensorMaster(const std::filesystem::path&,const SensorFrame&,int frames,const std::string& type);
std::vector<std::uint8_t> calibrationDefects(const SensorFrame&);
std::size_t repairCalibrationDefects(SensorFrame&,std::span<const std::uint8_t>);
void calibrateSensor(SensorFrame&,const SensorFrame* bias,const SensorFrame* dark,const SensorFrame* flat,const SensorFrame* darkFlat);
LinearImage demosaicCalibrated(const SensorFrame&,const std::array<float,4>& wb);
class Calibration {
public:
    explicit Calibration(const StackOptions&,const std::function<bool()>& cancelled={});
    bool enabled()const;
    LinearImage decode(const std::filesystem::path&,const std::array<float,4>& wb)const;
    std::string report()const;
private:
    StackOptions options_;
    std::unique_ptr<SensorFrame> bias_,dark_,flat_,darkFlat_;
    std::string report_;
    std::vector<std::uint8_t> defects_;
};
}
