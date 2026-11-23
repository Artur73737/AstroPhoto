#define NOMINMAX
#include <windows.h>
#include <astro/stacking/astrometry/PlateSolve.hpp>
#include <astro/core/domain/Core.hpp>
#include <cmath>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstdio>
#include <atomic>
#include <mutex>
namespace astro::stacking {
PlateSolution parsePlateSolution(const std::string& text) {
    PlateSolution result;
    bool declared = false;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        auto key = line.substr(0, pos), value = line.substr(pos + 1);
        auto trim = [](std::string s) {
            auto a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
            return a == std::string::npos ? std::string{} : s.substr(a, b - a + 1);
        };
        key = trim(key);
        value = trim(value);
        if (key == "DIMENSIONS") std::sscanf(value.c_str(), "%d x %d", &result.width, &result.height);
        if (key == "PLTSOLVD") declared = value == "T";
        if (key == "WARNING" || key == "ERROR") result.detail += value + " ";
        for (auto field : {"CRPIX1", "CRPIX2", "CRVAL1", "CRVAL2", "CD1_1", "CD1_2", "CD2_1", "CD2_2"})
            if (key == field) {
                std::size_t used{};
                try {
                    double v = std::stod(value, &used);
                    if (used != value.size() || !std::isfinite(v)) return {false, "Invalid astrometric output", {}};
                    result.wcs[key] = v;
                } catch (...) {
                    return {false, "Invalid astrometric output", {}};
                }
            }
    }
    if (!declared || result.wcs.size() != 8) {
        result.detail = "No complete astrometric solution. " + result.detail;
        return result;
    }
    const auto& m = result.wcs;
    double det = m.at("CD1_1") * m.at("CD2_2") - m.at("CD1_2") * m.at("CD2_1");
    result.solved =
        std::abs(det) > 1e-15 && m.at("CRVAL1") >= 0 && m.at("CRVAL1") < 360 && std::abs(m.at("CRVAL2")) <= 90;
    if (!result.solved) result.detail = "Invalid astrometric coordinates/matrix";
    return result;
}
PlateSolution solvePlate(const std::filesystem::path& fits, const StackOptions& options,
                         const std::function<bool()>& cancelled,
                         const std::function<void(const std::string&)>& progress) {
    if (progress) progress("Checking input and solver configuration");
    static std::timed_mutex solverMutex;
    std::unique_lock lock(solverMutex, std::defer_lock);
    if (progress) progress("Waiting for exclusive ASTAP process slot");
    auto queueDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.solverTimeoutSeconds);
    while (!lock.try_lock_for(std::chrono::milliseconds(50))) {
        if ((cancelled && cancelled()) || std::chrono::steady_clock::now() > queueDeadline)
            return {false, "Solver queue cancelled or timed out", {}};
    }
    if (options.solverExecutable.empty()) return {false, "Plate solving disabled", {}};
    static std::atomic<std::uint64_t> serial{};
    const auto executable = std::filesystem::absolute(pathFromUtf8(options.solverExecutable)),
               image = std::filesystem::absolute(fits),
               base = std::filesystem::temp_directory_path() / "AstroPhoto-plate-solves" /
                      ("plate-solution-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(++serial));
    std::filesystem::create_directories(base.parent_path());
    auto quote = [](const std::wstring& value) {
        if (value.find(L'"') != std::wstring::npos) throw std::invalid_argument("Invalid solver path");
        auto end = value.find_last_not_of(L'\\');
        auto trailing = end == std::wstring::npos ? value.size() : value.size() - end - 1;
        return L"\"" + value + std::wstring(trailing, L'\\') + L"\"";
    };
    std::wstring command = quote(executable.wstring()) + L" -f " + quote(image.wstring()) + L" -o " +
                           quote(base.wstring()) + L" -fov " + std::to_wstring(options.solverFov) +
                           L" -r 180 -z 0 -wcs -log";
    if (!options.solverDatabase.empty())
        command += L" -d " + quote(std::filesystem::absolute(pathFromUtf8(options.solverDatabase)).wstring());
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                        executable.parent_path().c_str(), &startup, &process))
        return {false, "Cannot start ASTAP (Windows " + std::to_string(GetLastError()) + ")", {}};
    struct Handles {
        HANDLE thread, process;
        ~Handles() {
            CloseHandle(thread);
            CloseHandle(process);
        }
    } handles{process.hThread, process.hProcess};
    if (progress) progress("ASTAP running: extracting stars and matching catalog");
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.solverTimeoutSeconds);
    DWORD wait{};
    auto nextProgress = std::chrono::steady_clock::now();
    std::string previousLine;
    do {
        wait = WaitForSingleObject(process.hProcess, 50);
        if (progress && std::chrono::steady_clock::now() >= nextProgress) {
            nextProgress = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
            auto logPath = base;
            logPath += ".log";
            std::ifstream log(logPath, std::ios::binary | std::ios::ate);
            if (log) {
                auto end = log.tellg();
                if (end > 4096)
                    log.seekg(end - std::streamoff(4096));
                else
                    log.seekg(0);
                std::string line, last;
                while (std::getline(log, line))
                    if (!line.empty() && line != "\r") last = line;
                if (!last.empty() && last != previousLine) {
                    previousLine = last;
                    progress("ASTAP: " + last);
                }
            }
        }
        if ((cancelled && cancelled()) || std::chrono::steady_clock::now() >= deadline) {
            TerminateProcess(process.hProcess, 1);
            WaitForSingleObject(process.hProcess, 5000);
            return {false, "ASTAP cancelled or timed out", {}};
        }
    } while (wait == WAIT_TIMEOUT);
    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 5000);
        return {false, "ASTAP process wait failure", {}};
    }
    DWORD code{};
    if (!GetExitCodeProcess(process.hProcess, &code)) return {false, "ASTAP process status unavailable", {}};
    auto ini = base;
    ini += ".ini";
    std::ifstream file(ini, std::ios::binary);
    if (!file) return {false, "ASTAP solution file missing", {}};
    std::string text;
    char buffer[4096];
    while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
        text.append(buffer, std::size_t(file.gcount()));
        if (text.size() > 65536) return {false, "ASTAP output exceeds limit", {}};
    }
    if (progress) progress("Reading and validating newly generated WCS");
    auto result = parsePlateSolution(text);
    if (code != 0 && !result.solved) {
        result.detail = "ASTAP exit code " + std::to_string(code) + ": " + result.detail;
    }
    return result;
}
}  // namespace astro::stacking
