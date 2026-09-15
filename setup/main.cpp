#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cwchar>
#include <string>

#include "setup/install_flow.h"
#include "setup/log.h"
#include "setup/payload.h"
#include "setup/setup_lock.h"
#include "version.h"

namespace {

// Runs one install or uninstall with the machine-wide setup lock held.
template <typename Run>
int Locked(Run run) {
    setup::SetupLock lock;
    setup::LockResult got = lock.Acquire(0);
    if (got == setup::LockResult::Busy) {
        setup::Log(L"another install or uninstall is running; waiting for it to finish");
        got = lock.Acquire(setup::SETUP_LOCK_WAIT_MS);
    }
    if (got == setup::LockResult::Busy) {
        setup::Log(L"another install or uninstall is still running after 5 minutes; try again when it finishes");
        return setup::EXIT_SETUP_BUSY;
    }
    if (got == setup::LockResult::Unavailable) {
        setup::Log(L"setup lock unavailable (%lu); continuing without it", GetLastError());
    }
    return run();
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    if (argc >= 2 && _wcsicmp(argv[1], L"install") == 0) {
        const std::wstring payload = argc >= 3 ? argv[2] : setup::ExeDir();
        return Locked([&] { return setup::RunInstall(payload, VGUN_VERSION); });
    }
    if (argc >= 2 && _wcsicmp(argv[1], L"uninstall") == 0) {
        return Locked([] { return setup::RunUninstall(); });
    }
    fwprintf(stderr, L"usage: vrlf-virtual-gun-setup.exe install [payload dir] | uninstall\n");
    return 2;
}
