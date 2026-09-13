#include "setup/registry_state.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace setup {

namespace {

std::wstring ReadString(HKEY key, const wchar_t* name) {
    DWORD bytes = 0;
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS) {
        return L"";
    }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) != ERROR_SUCCESS) {
        return L"";
    }
    value.resize(wcsnlen(value.c_str(), value.size()));
    return value;
}

}  // namespace

std::optional<InstallState> ReadState() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, STATE_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    InstallState s;
    s.version = ReadString(key, L"Version");
    s.install_dir = ReadString(key, L"InstallDir");
    s.cert_thumbprint = ReadString(key, L"CertThumbprint");
    s.driver_inf = ReadString(key, L"DriverInf");
    RegCloseKey(key);
    return s;
}

bool WriteValue(const wchar_t* name, const std::wstring& value) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, STATE_KEY, 0, nullptr, 0, KEY_WRITE | KEY_WOW64_64KEY,
                        nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const LSTATUS st = RegSetValueExW(key, name, 0, REG_SZ,
                                      reinterpret_cast<const BYTE*>(value.c_str()),
                                      static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return st == ERROR_SUCCESS;
}

void DeleteState() {
    RegDeleteKeyExW(HKEY_LOCAL_MACHINE, STATE_KEY, KEY_WOW64_64KEY, 0);
}

}  // namespace setup
