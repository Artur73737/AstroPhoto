#pragma once
#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace astro::update {
enum class Platform { Windows, Android };

struct UpdateInfo {
    std::string version;
    std::string currentVersion;
    std::string name;
    std::string releaseUrl;
    std::string downloadUrl;
    std::string notes;
    std::string publishedAt;
};

struct Result {
    bool succeeded{};
    std::optional<UpdateInfo> update;
    std::string error;
};

// Numeric dotted comparison: candidate must strictly exceed current.
bool isNewerVersion(std::string_view candidate, std::string_view current);

// Extracts the platform installer version from a published GitHub asset name.
std::optional<std::string> installerVersionFromAsset(std::string_view name, Platform platform);

// Network-free parsing used by the app and by deterministic unit tests.
Result parseReleaseList(std::string_view json, std::string_view currentVersion, Platform platform);

// Fetches the public GitHub release list on a background thread with bounded timeouts.
class Checker {
public:
    explicit Checker(std::string currentVersion, Platform platform = Platform::Windows);
    ~Checker();
    Checker(const Checker&) = delete;
    Checker& operator=(const Checker&) = delete;
    void start();
    bool running() const { return running_.load(); }
    std::optional<Result> poll();

private:
    void run();

    std::string currentVersion_;
    Platform platform_;
    std::jthread thread_;
    std::mutex mutex_;
    std::optional<Result> result_;
    std::atomic<bool> running_{};
};
}  // namespace astro::update
