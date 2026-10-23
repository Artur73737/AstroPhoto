#pragma once
#include <astro/Core.hpp>
#include <thread>
namespace astro {
class SystemClock final : public IClock {
public:
    SteadyTime steadyNow() const override {return std::chrono::steady_clock::now();}
    std::chrono::system_clock::time_point utcNow() const override {return std::chrono::system_clock::now();}
};
class DiskFileSystem final : public IFileSystem {
public:
    void createDirectories(const std::filesystem::path& p) override {std::filesystem::create_directories(p);}
    std::uint64_t available(const std::filesystem::path& p) override {return std::filesystem::space(p).available;}
    bool exists(const std::filesystem::path& p) const override {return std::filesystem::exists(p);}
};
bool canonBackendAvailable();
std::unique_ptr<ICamera> createCanonCamera(const Config&,Log);
class CameraWorker {
public:
    CameraWorker(Mailbox&,Config,std::filesystem::path configPath);
    ~CameraWorker();
private:
    std::jthread thread_;
};
}
