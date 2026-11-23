#define NOMINMAX
#include <windows.h>
#include <setupapi.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
namespace {
bool sameFile(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::error_code error;
    const auto size = std::filesystem::file_size(a, error);
    if (error || std::filesystem::file_size(b, error) != size || error) return false;
    std::ifstream left(a, std::ios::binary), right(b, std::ios::binary);
    return left && right && std::equal(std::istreambuf_iterator<char>(left), {},
                                       std::istreambuf_iterator<char>(right), {});
}
bool alreadyStaged(const std::filesystem::path& source) {
    wchar_t windows[MAX_PATH]{};
    if (!GetWindowsDirectoryW(windows, MAX_PATH)) return false;
    std::error_code error;
    const auto directory = std::filesystem::path(windows) / L"INF";
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
        const auto& file = it->path();
        if (file.filename().wstring().rfind(L"oem", 0) != 0 || file.extension() != L".inf" || !sameFile(source, file))
            continue;
        wchar_t store[MAX_PATH]{};
        if (!SetupGetInfDriverStoreLocationW(file.c_str(), nullptr, nullptr, store, MAX_PATH, nullptr)) continue;
        const auto package = std::filesystem::path(store).parent_path();
        // Never skip installation based on a product name/version alone. The
        // protected DriverStore must contain all three identical package files.
        if (sameFile(source, store) && sameFile(source.parent_path() / L"ASICAMUSB3.sys", package / L"ASICAMUSB3.sys") &&
            sameFile(source.parent_path() / L"asicamusb3.cat", package / L"asicamusb3.cat")) return true;
    }
    return false;
}
}
// MSI invokes this synchronously after staging the signed driver files.
// No shell, no downloaded executable and no arbitrary executable path.
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return ERROR_INVALID_PARAMETER;
    auto inf = std::filesystem::absolute(argv[1]);
    if (inf.filename() != L"ASICAMUSB3.inf" || !std::filesystem::is_regular_file(inf)) return ERROR_FILE_NOT_FOUND;
    if (alreadyStaged(inf)) return ERROR_SUCCESS;
    wchar_t system[MAX_PATH];
    if (!GetSystemDirectoryW(system, MAX_PATH)) return GetLastError();
    auto exe = std::filesystem::path(system) / L"pnputil.exe";
    std::wstring command = L"\"" + exe.wstring() + L"\" /add-driver \"" + inf.wstring() + L"\" /install";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
                        &startup, &child))
        return GetLastError();
    CloseHandle(child.hThread);
    DWORD wait = WaitForSingleObject(child.hProcess, 180000), status = ERROR_TIMEOUT;
    if (wait == WAIT_OBJECT_0)
        GetExitCodeProcess(child.hProcess, &status);
    else {
        TerminateProcess(child.hProcess, ERROR_TIMEOUT);
        WaitForSingleObject(child.hProcess, 5000);
    }
    CloseHandle(child.hProcess);
    return status == ERROR_SUCCESS_REBOOT_REQUIRED ? ERROR_SUCCESS : int(status);
}
