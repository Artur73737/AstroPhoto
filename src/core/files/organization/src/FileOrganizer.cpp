#include <astro/core/domain/Core.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace astro {
FileOrganizer::FileOrganizer(IFileSystem& f, IClock& c) : files_(f), clock_(c) {}
std::filesystem::path FileOrganizer::session(const Config& c) const {
    return sessionDirectory(c, clock_.utcNow());
}
std::filesystem::path nearestExistingDirectory(std::filesystem::path path) {
    std::error_code error;
    while (!path.empty()) {
        if (std::filesystem::is_directory(path, error)) return path;
        auto parent = path.parent_path();
        if (parent == path) break;
        path = std::move(parent);
    }
    return {};
}
std::filesystem::path sessionDirectory(const Config& c, std::chrono::system_clock::time_point now) {
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream date;
    date << std::put_time(&tm, "%Y-%m-%d");
    return c.outputDirectory / date.str() / pathFromUtf8(c.sessionName);
}
void FileOrganizer::prepare(const Config& c) {
    validate(c);
    files_.createDirectories(session(c));
}
std::filesystem::path FileOrganizer::stem(const Config& c, const SequenceItem& i, std::uint64_t counter) {
    validate(c);
    validateItem(i);
    auto dir = session(c) / frameName(i.type);
    files_.createDirectories(dir);
    auto now = clock_.utcNow();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time);
#else
    gmtime_r(&time, &tm);
#endif
    std::ostringstream name;
    name << std::put_time(&tm, "%Y%m%dT%H%M%S") << '_'
         << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000 << "Z_"
         << std::setfill('0') << std::setw(6) << counter;
    if (i.bulb) name << '_' << i.exposureSeconds << "s";
    name << '_' << frameName(i.type);
    return dir / name.str();
}
SequenceEngine::SequenceEngine(ICamera& c, IFileSystem& f, IClock& t, Log l)
    : camera_(c), files_(f), clock_(t), log_(std::move(l)), organizer_(f, t) {}

}
