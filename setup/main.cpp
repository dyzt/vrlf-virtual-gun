#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cwchar>

#include "setup/install_flow.h"
#include "setup/payload.h"
#include "version.h"

int wmain(int argc, wchar_t** argv) {
    if (argc >= 2 && _wcsicmp(argv[1], L"install") == 0) {
        return setup::RunInstall(argc >= 3 ? argv[2] : setup::ExeDir(), VGUN_VERSION);
    }
    if (argc >= 2 && _wcsicmp(argv[1], L"uninstall") == 0) {
        return setup::RunUninstall();
    }
    fwprintf(stderr, L"usage: vrlf-virtual-gun-setup.exe install [payload dir] | uninstall\n");
    return 2;
}
