#pragma once
#include <string>
#include <vector>

namespace setup {

// Files the release zip carries, relative to its root. Install copies exactly these.
const std::vector<std::wstring>& PayloadFiles();
std::wstring DefaultInstallDir();  // %ProgramFiles%\VRLF Virtual Lightgun
std::wstring ExeDir();
bool CopyPayload(const std::wstring& from, const std::wstring& to, std::wstring& error);
// Deletes the folder; a file still in use (this exe) is renamed aside and the renamed copy
// is scheduled for reboot deletion, so a reinstall's fresh copy at the original path is never
// the one deleted.
void RemoveInstallDir(const std::wstring& dir);

}  // namespace setup
