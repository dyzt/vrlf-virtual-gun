#pragma once
#include <optional>
#include <string>

namespace setup {

inline constexpr const wchar_t* STATE_KEY = L"SOFTWARE\\VRLF\\VirtualGun";

struct InstallState {
    std::wstring version;
    std::wstring install_dir;
    std::wstring cert_thumbprint;  // uppercase hex SHA-1
    std::wstring driver_inf;       // published oemNN.inf
    // An in-place update records the new cert and package as Pending* before trusting and
    // staging them, and the old ones as Previous* before retiring them, so uninstall can undo
    // an update interrupted at any step.
    std::wstring pending_cert_thumbprint;
    std::wstring pending_driver_inf;
    std::wstring previous_cert_thumbprint;
    std::wstring previous_driver_inf;
};

std::optional<InstallState> ReadState();
bool WriteValue(const wchar_t* name, const std::wstring& value);
void DeleteValue(const wchar_t* name);
void DeleteState();

}  // namespace setup
