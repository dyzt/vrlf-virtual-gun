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
};

std::optional<InstallState> ReadState();
bool WriteValue(const wchar_t* name, const std::wstring& value);
void DeleteState();

}  // namespace setup
