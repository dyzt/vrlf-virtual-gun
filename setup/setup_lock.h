#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace setup {

// One install or uninstall at a time, machine-wide. Two overlapping runs undo each other: an
// uninstall running from the install dir made a concurrent install's payload copy fail, then
// finished removing the driver, so the PC ended with nothing installed.
constexpr const wchar_t* SETUP_LOCK_NAME = L"Global\\VRLFVirtualGunSetup";
// How long a second run waits for the first before giving up.
constexpr DWORD SETUP_LOCK_WAIT_MS = 5 * 60 * 1000;
// ERROR_INSTALL_ALREADY_RUNNING: Windows' own "another installation is in progress" code.
constexpr int EXIT_SETUP_BUSY = 1618;

enum class LockResult {
    Held,         // this run owns the lock
    Busy,         // another run still holds it
    Unavailable,  // the lock could not be created; the run goes ahead without it
};

class SetupLock {
public:
    explicit SetupLock(const wchar_t* name = SETUP_LOCK_NAME) : _handle(CreateMutexW(nullptr, FALSE, name)) {}
    ~SetupLock() {
        if (_owned) ReleaseMutex(_handle);
        if (_handle != nullptr) CloseHandle(_handle);
    }
    SetupLock(const SetupLock&) = delete;
    SetupLock& operator=(const SetupLock&) = delete;

    // A lock left by a run that died holding it (WAIT_ABANDONED) is taken over.
    LockResult Acquire(DWORD timeout_ms) {
        if (_handle == nullptr) return LockResult::Unavailable;
        const DWORD r = WaitForSingleObject(_handle, timeout_ms);
        _owned = r == WAIT_OBJECT_0 || r == WAIT_ABANDONED;
        if (_owned) return LockResult::Held;
        return r == WAIT_TIMEOUT ? LockResult::Busy : LockResult::Unavailable;
    }

private:
    HANDLE _handle;
    bool _owned = false;
};

}  // namespace setup
