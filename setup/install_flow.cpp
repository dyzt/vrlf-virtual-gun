#include "setup/install_flow.h"

#include "setup/cert_sign.h"
#include "setup/driver_install.h"
#include "setup/log.h"
#include "setup/payload.h"
#include "setup/registry_state.h"

namespace setup {

namespace {

int Fail(const std::wstring& error) {
    Log(L"install failed: %ls; rolling back", error.c_str());
    RunUninstall();
    return 1;
}

}  // namespace

int RunInstall(const std::wstring& payload_dir, const std::wstring& version) {
    if (ReadState()) {
        Log(L"existing install found; removing it first");
        RunUninstall();
    }
    const std::wstring dir = DefaultInstallDir();
    // Every value is written BEFORE the step it describes, so a rollback can undo a half-done step.
    WriteValue(L"InstallDir", dir);
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

    WriteValue(L"CertThumbprint", cert.thumbprint);
    if (!TrustCertificate(cert.encoded, error)) return Fail(error);

    std::wstring published;
    bool reboot = false;
    if (!InstallDriver(driver_dir + L"\\VRLFVirtualGun.inf", published, reboot, error)) return Fail(error);
    WriteValue(L"DriverInf", published);

    WriteValue(L"Version", version);
    Log(L"VRLF Virtual Lightgun %ls installed", version.c_str());
    return reboot ? 3010 : 0;
}

int RunUninstall() {
    const auto state = ReadState();
    UninstallDriver(state ? state->driver_inf : L"");
    if (state && !state->cert_thumbprint.empty()) RemoveTrustedCertificate(state->cert_thumbprint);
    if (state && !state->install_dir.empty()) RemoveInstallDir(state->install_dir);
    DeleteState();
    Log(L"uninstall complete");
    return 0;
}

}  // namespace setup
