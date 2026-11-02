#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace astro {
struct StackOptions {
    int frames{50}; // zero means until stopped; counts exposures, including rejected frames
    double delaySeconds{3}, detectionSnr{5}, maximumRms{1}, sigma{4};
    int minimumStars{8}, memoryLimitMb{2048};
    bool alignment{true}, rejection{true}, cosmeticCorrection{true};
    bool horizontalBanding{false};
    double bandingStrength{1}, bandingMaxOffset{.02};
    int bandingRadius{32};
    std::string biasDirectory,darkDirectory,flatDirectory,darkFlatDirectory;
    bool backgroundNormalization{},gradientRemoval{},affineAlignment{};
    int checkpointFrames{10};
    std::string sourceDirectory; // empty = camera; otherwise rebuild without shutter commands
    double drizzleScale{1},drizzlePixfrac{.8};
    bool winsorizedRejection{},exportTiff{},exportXisf{};
    std::string solverExecutable,solverDatabase;
    double solverFov{};
    int solverTimeoutSeconds{120};
    std::string frameType{"Light"};
    bool fromSequence{},autoRebuild{};
};
struct StackDisplay {float low{}, mid{.25f}, high{1};bool asinh{};};
struct StackPreview {
    StackDisplay display;
    int width{},height{};std::uint64_t revision{};
    std::vector<std::uint8_t> rgba;
    std::vector<std::uint16_t> rgba16;
    std::array<float,256> histogram{};
};
struct StackStatus {
    std::string phase{"idle"},detail,directory,finalFile,astrometry;
    bool active{},stopping{},pauseRequested{},paused{};
    int captured{},accepted{},rejected{},target{},stars{},matches{};
    double elapsedSeconds{},exposureSeconds{},exposureElapsed{},delayRemaining{},rms{},overlap{1},integratedSeconds{};
    double hfd{},fwhm{},stackHfd{},stackFwhm{};
    std::shared_ptr<const StackPreview> preview;
};
struct StackFrameJob {
    std::vector<std::filesystem::path> files;
    double exposureSeconds{};int number{};
};
struct StackProcessResult {
    bool ready{},accepted{},fatal{},finalized{},prepared{},warning{};
    int stars{},matches{};double rms{},overlap{1};
    double exposureSeconds{},hfd{},fwhm{},stackHfd{},stackFwhm{};
    std::string detail,finalFile,astrometry;
    std::shared_ptr<const StackPreview> preview;
};
// Implementations own their processing thread. Submit is bounded to one job;
// poll never blocks the camera thread. No SDK objects cross this boundary.
class IStackProcessor {
public:
    virtual ~IStackProcessor()=default;
    virtual void begin(const std::filesystem::path&,const StackOptions&,const std::string& metadata)=0;
    virtual void submit(StackFrameJob)=0;
    virtual StackProcessResult poll()=0;
    virtual std::shared_ptr<const StackPreview> currentPreview() const {return {};}
    virtual void finish(const std::string& reason,bool failed=false)=0;
    virtual void display(StackDisplay)=0;
    virtual void cancelOptionalProcessing() {}
};
void validateStack(const StackOptions&);
void validateStackDisplay(const StackDisplay&);
}
