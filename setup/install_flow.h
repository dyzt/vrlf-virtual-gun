#pragma once
#include <string>

namespace setup {

// 0 ok, 1 failed (rolled back), 3010 ok but reboot required.
int RunInstall(const std::wstring& payload_dir, const std::wstring& version);
int RunUninstall();

}  // namespace setup
