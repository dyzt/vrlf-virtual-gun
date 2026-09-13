#pragma once
#include <cstddef>
#include <string>

namespace setup {

inline constexpr const wchar_t* HARDWARE_ID = L"Root\\VRLFVirtualGun";

// Stages the package into the driver store with SetupCopyOEMInf first (so the published
// oemNN.inf name is known deterministically), then creates the root devnode and installs the
// staged package onto it. Removes any existing devnode first.
bool InstallDriver(const std::wstring& inf_path, std::wstring& published_inf, bool& reboot, std::wstring& error);
// Every devnode whose first hardware ID is HARDWARE_ID, present or not.
size_t RemoveDevices();
// RemoveDevices, then deletes the published package when one is named.
void UninstallDriver(const std::wstring& published_inf);

}  // namespace setup
