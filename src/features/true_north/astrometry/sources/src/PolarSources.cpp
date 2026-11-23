#define NOMINMAX
#include <windows.h>
#include <astro/core/domain/Core.hpp>
#include <fstream>
#include <sstream>
#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <astro/features/true_north/astrometry/sources/PolarSources.hpp>
namespace astro::north::detail {
using stacking::Point;
using stacking::Transform;
std::wstring quote(const std::filesystem::path& p) {
    auto text = p.wstring();
    if (text.find(L'"') != text.npos) throw std::invalid_argument("Invalid extraction path");
    return L"\"" + text + L"\"";
}
std::vector<Point> extract(const std::filesystem::path& source, const StackOptions& options,
                           const std::function<bool()>& cancelled,
                           const std::function<void(const std::string&)>& progress) {
    static std::atomic<unsigned long long> serial{};
    auto folder =
        std::filesystem::temp_directory_path() / "AstroPhoto-polar" /
        (std::to_string(GetCurrentProcessId()) + "-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(++serial));
    std::filesystem::create_directories(folder);
    auto input = folder / ("input" + source.extension().string());
    if (std::filesystem::file_size(source) > 512ull * 1024 * 1024)
        throw std::runtime_error("Polar input exceeds transfer budget");
    std::filesystem::copy_file(source, input);
    auto executable = std::filesystem::absolute(pathFromUtf8(options.solverExecutable));
    std::wstring command = quote(executable) + L" -f " + quote(input) + L" -extract 20 -z 1";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (progress) progress("Polar recovery: ASTAP extracting source centroids from original pixels");
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        folder.c_str(), &startup, &process))
        throw std::runtime_error("Cannot launch ASTAP source extraction");
    struct Handles {
        HANDLE thread, process;
        ~Handles() {
            CloseHandle(thread);
            CloseHandle(process);
        }
    } handles{process.hThread, process.hProcess};
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(std::min(30, options.solverTimeoutSeconds));
    DWORD status;
    do {
        status = WaitForSingleObject(process.hProcess, 50);
        if ((cancelled && cancelled()) || std::chrono::steady_clock::now() > end) {
            TerminateProcess(process.hProcess, 1);
            WaitForSingleObject(process.hProcess, 5000);
            throw std::runtime_error("Polar extraction cancelled or timed out");
        }
    } while (status == WAIT_TIMEOUT);
    if (status != WAIT_OBJECT_0) throw std::runtime_error("Cannot wait for ASTAP extraction");
    auto csv = input;
    csv.replace_extension(".csv");
    std::ifstream in(csv);
    if (!in) throw std::runtime_error("ASTAP did not return source centroids");
    std::string line;
    std::getline(in, line);
    std::vector<Point> points;
    while (std::getline(in, line)) {
        if (line.size() > 4096) throw std::runtime_error("Invalid source record");
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream row(line);
        Point p;
        double hfd, snr;
        if (row >> p.x >> p.y >> hfd >> snr >> p.flux) {
            if (std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.flux) && p.flux > 0 && hfd > 0 && hfd < 30)
                points.push_back(p);
        }
        if (points.size() > 10000) throw std::runtime_error("Too many polar source candidates");
    }
    std::stable_sort(points.begin(), points.end(), [](auto a, auto b) { return a.flux > b.flux; });
    if (points.size() > 500) points.resize(500);
    return points;
}
}  // namespace astro::north::detail
