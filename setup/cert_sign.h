#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincrypt.h>
#include <ncrypt.h>

#include <string>
#include <vector>

namespace setup {

inline constexpr const wchar_t* CERT_SUBJECT = L"CN=VRLF Virtual Lightgun (this PC)";

struct SigningCert {
    PCCERT_CONTEXT context = nullptr;
    NCRYPT_KEY_HANDLE key = 0;
    std::wstring key_name;
    std::vector<BYTE> encoded;  // public certificate, kept after the key is gone
    std::wstring thumbprint;    // uppercase hex SHA-1
};

// A new RSA-3072 key (never exportable; machine-wide when machine_key, else the
// current user's) and a 10-year self-signed code-signing certificate over it.
bool CreateSigningCert(SigningCert& out, std::wstring& error, bool machine_key);
// Authenticode-sign a PE or catalog file with SHA-256 through mssign32.dll.
bool SignFile(const SigningCert& cert, const std::wstring& path, std::wstring& error);
// Deletes the private key and frees the context. Safe to call twice.
void DestroySigningKey(SigningCert& cert);
// Adds the public certificate to LocalMachine Root and TrustedPublisher.
bool TrustCertificate(const std::vector<BYTE>& encoded, std::wstring& error);
// Removes a thumbprint from LocalMachine Root and TrustedPublisher.
void RemoveTrustedCertificate(const std::wstring& thumbprint);

}  // namespace setup
