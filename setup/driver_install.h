#pragma once
#include <cstddef>
#include <string>

namespace setup {

inline constexpr const wchar_t* HARDWARE_ID = L"Root\\VRLFVirtualGun";

// Stages the package into the driver store with SetupCopyOEMInf first (so the published
// oemNN.inf name is known deterministically), then creates the root devnode and installs the
// staged package onto it. Removes any existing devnode first.
bool InstallDriver(const std::wstring& inf_path, std::wstring& published_inf, bool& reboot, std::wstring& error);
// Copies the package into the driver store and returns its published oemNN.inf name.
bool StagePackage(const std::wstring& inf_path, std::wstring& published_inf, std::wstring& error);
// Installs the package onto the existing root devnode without removing it. Keeping the node
// keeps its ParentIdPrefix, so the virtual guns' Raw Input device paths survive the update.
bool UpdateDevice(const std::wstring& inf_path, bool& reboot, std::wstring& error);
// Deletes a staged package from the driver store; true for an empty name. Fails while a
// loaded driver still runs from the package (an update that needs a reboot).
bool UnstagePackage(const std::wstring& published_inf);
// Devnodes whose first hardware ID is HARDWARE_ID: all of them, or only present ones.
size_t CountDevices(bool present_only);
// Every devnode whose first hardware ID is HARDWARE_ID, present or not.
size_t RemoveDevices();
// RemoveDevices, then deletes the published package when one is named.
void UninstallDriver(const std::wstring& published_inf);

}  // namespace setup
