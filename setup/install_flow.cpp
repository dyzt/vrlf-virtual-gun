#include "setup/install_flow.h"

#include <cwchar>

#include "setup/cert_sign.h"
#include "setup/driver_install.h"
#include "setup/install_mode.h"
#include "setup/log.h"
#include "setup/payload.h"
#include "setup/registry_state.h"

namespace setup {

namespace {

bool SameName(const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }

int Fail(const std::wstring& error) {
    Log(L"install failed: %ls; rolling back", error.c_str());
    RunUninstall();
    return 1;
}

// The device never took the new package: drop the pending cert and package and leave the
// recorded install exactly as it was. `pending_inf` is skipped when the store handed back the
// package the device already runs.
int FailUpdate(const std::wstring& error, const InstallState& old, const std::wstring& pending_cert,
               const std::wstring& pending_inf) {
    Log(L"update failed: %ls; keeping the existing install", error.c_str());
    if (!SameName(pending_inf, old.driver_inf)) UnstagePackage(pending_inf);
    RemoveTrustedCertificate(pending_cert);
    DeleteValue(L"PendingDriverInf");
    DeleteValue(L"PendingCertThumbprint");
    return 1;
}

// Retires the package and cert an earlier update replaced. A package still in use by a driver
// waiting on a reboot stays recorded and is retried by the next install or uninstall.
void RetirePrevious(const InstallState& state) {
    if (!state.previous_cert_thumbprint.empty()) {
        RemoveTrustedCertificate(state.previous_cert_thumbprint);
        DeleteValue(L"PreviousCertThumbprint");
    }
    if (!state.previous_driver_inf.empty() && UnstagePackage(state.previous_driver_inf)) {
        DeleteValue(L"PreviousDriverInf");
    }
}

// Keeps the root device node and swaps the driver on it, so Windows keeps each virtual gun's
// ParentIdPrefix and the Raw Input device paths games have bound stay the same.
int RunUpdate(const InstallState& old, const std::wstring& payload_dir, const std::wstring& version) {
    Log(L"existing install %ls found; updating in place (device node kept)", old.version.c_str());
    std::wstring error;
    if (!CopyPayload(payload_dir, old.install_dir, error)) return FailUpdate(error, old, L"", L"");

    const std::wstring driver_dir = old.install_dir + L"\\driver";
    SigningCert cert;
    if (!CreateSigningCert(cert, error, true)) return FailUpdate(error, old, L"", L"");
    const bool signed_ok = SignFile(cert, driver_dir + L"\\VRLFVirtualGun.dll", error) &&
                           SignFile(cert, driver_dir + L"\\vrlfvirtualgun.cat", error);
    DestroySigningKey(cert);
    Log(L"signing key deleted");
    if (!signed_ok) return FailUpdate(error, old, L"", L"");

    // Pending* is written BEFORE the step it describes, as the fresh install does.
    if (!WriteValue(L"PendingCertThumbprint", cert.thumbprint)) {
        return FailUpdate(L"registry write PendingCertThumbprint failed", old, L"", L"");
    }
    if (!TrustCertificate(cert.encoded, error)) return FailUpdate(error, old, cert.thumbprint, L"");

    const std::wstring inf = driver_dir + L"\\VRLFVirtualGun.inf";
    std::wstring published;
    if (!StagePackage(inf, published, error)) return FailUpdate(error, old, cert.thumbprint, L"");
    if (!WriteValue(L"PendingDriverInf", published)) {
        return FailUpdate(L"registry write PendingDriverInf failed", old, cert.thumbprint, published);
    }
    bool reboot = false;
    if (!UpdateDevice(inf, reboot, error)) return FailUpdate(error, old, cert.thumbprint, published);

    if (SameName(published, old.driver_inf)) {
        // An identical INF (a same-version reinstall): the store handed back the package it
        // already holds and kept that package's catalog, which the OLD cert signed. Retiring
        // the old cert would leave the running driver signed by an untrusted certificate, so
        // keep it and discard the new one instead.
        RemoveTrustedCertificate(cert.thumbprint);
        DeleteValue(L"PendingCertThumbprint");
        DeleteValue(L"PendingDriverInf");
        Log(L"driver store already held this package (%ls); kept its certificate", published.c_str());
        if (!WriteValue(L"Version", version)) {
            Log(L"update failed: registry write Version failed; run install again to repair");
            return 1;
        }
        Log(L"VRLF Virtual Lightgun %ls reinstalled in place", version.c_str());
        return reboot ? 3010 : 0;
    }

    // The device runs the new package. Record the old pair as Previous* before promoting the
    // new one, so an interruption from here on leaves every cert and package recorded.
    if (!WriteValue(L"PreviousCertThumbprint", old.cert_thumbprint) ||
        !WriteValue(L"PreviousDriverInf", old.driver_inf) ||
        !WriteValue(L"CertThumbprint", cert.thumbprint) || !WriteValue(L"DriverInf", published)) {
        Log(L"update failed: registry write while promoting the new driver; run install again to repair");
        return 1;
    }
    DeleteValue(L"PendingCertThumbprint");
    DeleteValue(L"PendingDriverInf");
    const auto promoted = ReadState();
    if (promoted) RetirePrevious(*promoted);

    if (!WriteValue(L"Version", version)) {
        Log(L"update failed: registry write Version failed; run install again to repair");
        return 1;
    }
    Log(L"VRLF Virtual Lightgun updated %ls -> %ls", old.version.c_str(), version.c_str());
    return reboot ? 3010 : 0;
}

}  // namespace

int RunInstall(const std::wstring& payload_dir, const std::wstring& version) {
    auto state = ReadState();
    if (state && state->pending_cert_thumbprint.empty() && state->pending_driver_inf.empty() &&
        (!state->previous_cert_thumbprint.empty() || !state->previous_driver_inf.empty())) {
        RetirePrevious(*state);
        state = ReadState();
    }
    const std::wstring dir = DefaultInstallDir();
    InstallMode mode = ChooseInstallMode(state, CountDevices(false), CountDevices(true));
    if (mode == InstallMode::Update && !SameName(state->install_dir, dir)) mode = InstallMode::Reinstall;
    if (mode == InstallMode::Update) return RunUpdate(*state, payload_dir, version);
    if (mode == InstallMode::Reinstall) {
        Log(L"existing install found; removing it first (virtual gun device paths will change)");
        RunUninstall();
    }

    // Every value is written BEFORE the step it describes, so a rollback can undo a half-done
    // step. A failed WRITE is itself a Fail(): a value RunUninstall never sees is a step
    // RunUninstall can never undo.
    if (!WriteValue(L"InstallDir", dir)) return Fail(L"registry write InstallDir failed");
    std::wstring error;
    if (!CopyPayload(payload_dir, dir, error)) return Fail(error);

    const std::wstring driver_dir = dir + L"\\driver";
    SigningCert cert;
    if (!CreateSigningCert(cert, error, true)) return Fail(error);
    const bool signed_ok = SignFile(cert, driver_dir + L"\\VRLFVirtualGun.dll", error) &&
                           SignFile(cert, driver_dir + L"\\vrlfvirtualgun.cat", error);
    DestroySigningKey(cert);
    Log(L"signing key deleted");
    if (!signed_ok) return Fail(error);

    if (!WriteValue(L"CertThumbprint", cert.thumbprint)) return Fail(L"registry write CertThumbprint failed");
    if (!TrustCertificate(cert.encoded, error)) return Fail(error);

    std::wstring published;
    bool reboot = false;
    if (!InstallDriver(driver_dir + L"\\VRLFVirtualGun.inf", published, reboot, error)) return Fail(error);
    if (!WriteValue(L"DriverInf", published)) {
        // RunUninstall (inside Fail) can't learn `published` from the registry once this write
        // has failed, so undo the driver install directly with the name InstallDriver just
        // handed back, before the generic rollback runs.
        UninstallDriver(published);
        return Fail(L"registry write DriverInf failed");
    }

    if (!WriteValue(L"Version", version)) return Fail(L"registry write Version failed");
    Log(L"VRLF Virtual Lightgun %ls installed", version.c_str());
    return reboot ? 3010 : 0;
}

int RunUninstall() {
    const auto state = ReadState();
    UninstallDriver(state ? state->driver_inf : L"");
    if (state) {
        // An interrupted update can leave a second and third package and cert recorded.
        for (const std::wstring* inf : {&state->pending_driver_inf, &state->previous_driver_inf}) {
            if (!SameName(*inf, state->driver_inf)) UnstagePackage(*inf);
        }
        for (const std::wstring* thumb :
             {&state->cert_thumbprint, &state->pending_cert_thumbprint, &state->previous_cert_thumbprint}) {
            RemoveTrustedCertificate(*thumb);
        }
        if (!state->install_dir.empty()) RemoveInstallDir(state->install_dir);
    }
    DeleteState();
    Log(L"uninstall complete");
    return 0;
}

}  // namespace setup
