// Proves setup/cert_sign.cpp without admin: a throwaway USER key signs copies
// of the driver package, then the key is deleted. Nothing is trusted.
// Usage: sign_probe.exe <package dir> <out dir>
#include <filesystem>
#include <string>

#include "setup/cert_sign.h"
#include "setup/log.h"

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        setup::Log(L"usage: sign_probe.exe <package dir> <out dir>");
        return 2;
    }
    const std::filesystem::path in = argv[1];
    const std::filesystem::path out = argv[2];
    std::error_code ec;
    std::filesystem::create_directories(out, ec);
    for (const wchar_t* name : {L"VRLFVirtualGun.dll", L"vrlfvirtualgun.cat"}) {
        std::filesystem::copy_file(in / name, out / name, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            setup::Log(L"copy %ls failed", name);
            return 1;
        }
    }
    setup::SigningCert cert;
    std::wstring error;
    if (!setup::CreateSigningCert(cert, error, false)) {
        setup::Log(L"probe failed: %ls", error.c_str());
        return 1;
    }
    bool ok = true;
    for (const wchar_t* name : {L"VRLFVirtualGun.dll", L"vrlfvirtualgun.cat"}) {
        if (!setup::SignFile(cert, (out / name).wstring(), error)) {
            setup::Log(L"probe failed: %ls", error.c_str());
            ok = false;
            break;
        }
    }
    setup::DestroySigningKey(cert);
    setup::Log(L"key deleted; thumbprint %ls", cert.thumbprint.c_str());
    return ok ? 0 : 1;
}
