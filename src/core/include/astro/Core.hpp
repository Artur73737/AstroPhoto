#pragma once
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace astro {
using SteadyTime = std::chrono::steady_clock::time_point;
enum class Property { Iso, Aperture, Shutter, ExposureMode, WhiteBalance, ColorTemperature, PictureStyle, ImageQuality };
std::string propertyName(Property);
std::filesystem::path pathFromUtf8(const std::string&);
struct Choice { std::uint32_t value{}; std::string label; };
struct CameraProperty { Property id{}; std::uint32_t value{}; std::vector<Choice> choices; bool writable{}; std::string currentLabel; };
enum class FrameType { Light, Dark, Flat, Bias };
std::string frameName(FrameType);
enum class SequenceState { Idle, Running, Paused, Stopping, Error };
std::string stateName(SequenceState);
struct SequenceItem {
    FrameType type{FrameType::Light};
    int count{1};
    double exposureSeconds{1};
    double delaySeconds{};
    double intervalSeconds{};
    bool bulb{};
    std::map<Property, std::uint32_t> settings;
};
struct Config {
    std::filesystem::path outputDirectory{"output"};
    std::string sessionName{"session"};
    int tickMs{20}, liveViewMs{100}, downloadTimeoutSeconds{120}, busyRetries{3}, retryDelayMs{250};
    int windowWidth{1400}, windowHeight{900}, maxLogEntries{1000};
    float uiScale{1.0f};
    bool lightTheme{};
    std::string language{"it"};
    bool focusPeaking{};
    float peakingThreshold{35.0f};
    SequenceItem singleCapture;
    std::uint64_t reserveBytes{268435456};
    std::vector<SequenceItem> sequence;
    std::map<Property, std::map<std::uint32_t, std::string>> labels;
};
void validate(const Config&);
void validateItem(const SequenceItem&);
class CameraError : public std::runtime_error {
public:
    CameraError(std::string message, bool retryable = false) : std::runtime_error(std::move(message)), retryable(retryable) {}
    bool retryable;
};
struct CaptureResult { bool complete{}; std::vector<std::filesystem::path> files; };
struct CapturedPhoto { std::uint64_t revision{}; FrameType type{FrameType::Light}; std::vector<std::filesystem::path> files; };
struct LiveFrame { std::vector<std::uint8_t> jpeg; std::uint64_t revision{}; };
class ICamera {
public:
    virtual ~ICamera() = default;
    virtual std::string connect() = 0;
    virtual void disconnect() = 0;
    virtual bool connected() const = 0;
    virtual void pump() = 0;
    virtual std::vector<CameraProperty> properties() = 0;
    virtual void set(Property, std::uint32_t) = 0;
    virtual void prepareHost(std::uint64_t availableBytes) = 0;
    virtual void beginCapture(const SequenceItem&, const std::filesystem::path& stem) = 0;
    virtual CaptureResult pollCapture() = 0;
    virtual void abortCapture() = 0;
    virtual void setLiveView(bool) = 0;
    virtual LiveFrame liveFrame() = 0;
    virtual void focus(int step) = 0;
    virtual void zoom(int ratio) = 0;
};
class IClock {
public:
    virtual ~IClock() = default;
    virtual SteadyTime steadyNow() const = 0;
    virtual std::chrono::system_clock::time_point utcNow() const = 0;
};
class IFileSystem {
public:
    virtual ~IFileSystem() = default;
    virtual void createDirectories(const std::filesystem::path&) = 0;
    virtual std::uint64_t available(const std::filesystem::path&) = 0;
    virtual bool exists(const std::filesystem::path&) const = 0;
};
class FileOrganizer {
public:
    FileOrganizer(IFileSystem&, IClock&);
    std::filesystem::path stem(const Config&, const SequenceItem&, std::uint64_t counter);
    void prepare(const Config&);
private:
    IFileSystem& files_; IClock& clock_;
};
struct Progress {
    SequenceState state{SequenceState::Idle};
    std::size_t row{};
    std::uint64_t completed{}, total{};
    bool pauseRequested{};
    std::string detail;
};
using Log = std::function<void(const std::string&, const std::string&)>;
class SequenceEngine {
public:
    SequenceEngine(ICamera&, IFileSystem&, IClock&, Log);
    void start(const Config&);
    void startSingle(const Config&);
    void tick();
    void pause();
    void resume();
    void stop();
    void reportError(const std::string& message) { fail(message); }
    const Progress& progress() const { return progress_; }
    const CapturedPhoto& lastCapture() const { return lastCapture_; }
    bool active() const;
private:
    enum class Phase { Configure, Delay, Capture, Download };
    void fail(const std::string&);
    ICamera& camera_; IFileSystem& files_; IClock& clock_; Log log_;
    FileOrganizer organizer_; Config config_; Progress progress_;
    Phase phase_{Phase::Configure};
    SteadyTime deadline_{}, retryAt_{};
    SteadyTime nextCaptureAt_{};
    CapturedPhoto lastCapture_;
    std::uint64_t counter_{};
    int shotInRow_{}, retries_{};
    bool captureStarted_{};
};
struct LogEntry { std::string time, level, message; };
struct Snapshot {
    bool backendAvailable{}, connected{}, liveView{};
    std::string model, backendStatus;
    std::vector<CameraProperty> properties;
    Progress progress;
    LiveFrame frame;
    CapturedPhoto lastCapture;
    std::deque<LogEntry> logs;
};
enum class Action { Connect, Disconnect, Refresh, SetProperty, Start, CaptureSingle, Pause, Resume, Stop, LiveView, Focus, Zoom, SaveConfig };
struct Command { Action action{}; Property property{}; std::uint32_t value{}; Config config; };
class Mailbox {
public:
    void send(Command);
    std::vector<Command> take();
    Snapshot snapshot() const;
    void publish(const Snapshot&);
private:
    mutable std::mutex mutex_; std::vector<Command> commands_; Snapshot snapshot_;
};
}
