#include "setup/payload.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <filesystem>

#include "setup/log.h"

namespace setup {

const std::vector<std::wstring>& PayloadFiles() {
    static const std::vector<std::wstring> files = {
        L"vrlf-virtual-gun-setup.exe",
        L"driver\\VRLFVirtualGun.dll",
        L"driver\\VRLFVirtualGun.inf",
        L"driver\\vrlfvirtualgun.cat",
        L"LICENSE",
        L"THIRD_PARTY_NOTICES.md",
    };
    return files;
}

std::wstring DefaultInstallDir() {
    wchar_t base[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"ProgramFiles", base, MAX_PATH);
    std::wstring dir = (n > 0 && n < MAX_PATH) ? std::wstring(base) : L"C:\\Program Files";
    return dir + L"\\VRLF Virtual Lightgun";
}

std::wstring ExeDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return std::filesystem::path(path).parent_path().wstring();
}

bool CopyPayload(const std::wstring& from, const std::wstring& to, std::wstring& error) {
    std::error_code ec;
    if (std::filesystem::equivalent(from, to, ec)) {
        error = L"install must run from the release folder, not from the install dir";
        return false;
    }
    std::filesystem::create_directories(std::filesystem::path(to) / L"driver", ec);
    if (ec) {
        error = L"create install dir failed";
        return false;
    }
    for (const auto& rel : PayloadFiles()) {
        const auto src = std::filesystem::path(from) / rel;
        const auto dst = std::filesystem::path(to) / rel;
        if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
            error = L"copy " + src.wstring() + L" failed: " + std::to_wstring(GetLastError());
            return false;
        }
    }
    Log(L"payload copied to %ls", to.c_str());
    return true;
}

void RemoveInstallDir(const std::wstring& dir) {
    std::error_code ec;
    // Collect first: deleting while a directory iterator is live is unreliable.
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (entry.is_regular_file(ec)) files.push_back(entry.path());
    }
    for (const auto& file : files) {
        if (!DeleteFileW(file.c_str())) MoveFileExW(file.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
    std::filesystem::remove(std::filesystem::path(dir) / L"driver", ec);
    if (!RemoveDirectoryW(dir.c_str())) MoveFileExW(dir.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    Log(L"install dir removed (anything in use goes at next reboot)");
}

}  // namespace setup
