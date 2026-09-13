#pragma once
#include <string>
#include <vector>

namespace setup {

// Files the release zip carries, relative to its root. Install copies exactly these.
const std::vector<std::wstring>& PayloadFiles();
std::wstring DefaultInstallDir();  // %ProgramFiles%\VRLF Virtual Lightgun
std::wstring ExeDir();
bool CopyPayload(const std::wstring& from, const std::wstring& to, std::wstring& error);
// Deletes the folder; files still in use (this exe) are scheduled for reboot.
void RemoveInstallDir(const std::wstring& dir);

}  // namespace setup
