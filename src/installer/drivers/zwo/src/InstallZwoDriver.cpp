#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
// MSI invokes this synchronously after staging the signed driver files.
// No shell, no downloaded executable and no arbitrary executable path.
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return ERROR_INVALID_PARAMETER;
    auto inf = std::filesystem::absolute(argv[1]);
    if (inf.filename() != L"ASICAMUSB3.inf" || !std::filesystem::is_regular_file(inf)) return ERROR_FILE_NOT_FOUND;
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
