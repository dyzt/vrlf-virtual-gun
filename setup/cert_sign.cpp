#include "setup/cert_sign.h"

#include <objbase.h>

#include "setup/log.h"
#include "setup/strings.h"

namespace setup {

namespace {

// mssign32.dll has no SDK header; these mirror the documented SignerSignEx2 types.
struct SIGNER_FILE_INFO {
    DWORD cbSize;
    LPCWSTR pwszFileName;
    HANDLE hFile;
};
struct SIGNER_SUBJECT_INFO {
    DWORD cbSize;
    DWORD* pdwIndex;
    DWORD dwSubjectChoice;
    union {
        SIGNER_FILE_INFO* pSignerFileInfo;
        PVOID pSignerBlobInfo;
    };
};
struct SIGNER_CERT_STORE_INFO {
    DWORD cbSize;
    PCCERT_CONTEXT pSigningCert;
    DWORD dwCertPolicy;
    HCERTSTORE hCertStore;
};
struct SIGNER_CERT {
    DWORD cbSize;
    DWORD dwCertChoice;
    union {
        LPCWSTR pwszSpcFile;
        SIGNER_CERT_STORE_INFO* pCertStoreInfo;
        PVOID pSpcChainInfo;
    };
    HWND hwnd;
};
struct SIGNER_SIGNATURE_INFO {
    DWORD cbSize;
    ALG_ID algidHash;
    DWORD dwAttrChoice;
    union {
        PVOID pAttrAuthcode;
    };
    PCRYPT_ATTRIBUTES psAuthenticated;
    PCRYPT_ATTRIBUTES psUnauthenticated;
};
struct SIGNER_CONTEXT {
    DWORD cbSize;
    DWORD cbBlob;
    BYTE* pbBlob;
};

constexpr DWORD SIGNER_SUBJECT_FILE = 0x01;
constexpr DWORD SIGNER_CERT_STORE = 0x02;
constexpr DWORD SIGNER_CERT_POLICY_CHAIN = 0x02;
constexpr DWORD SIGNER_NO_ATTR = 0x00;

using SignerSignEx2Fn = HRESULT(WINAPI*)(DWORD, SIGNER_SUBJECT_INFO*, SIGNER_CERT*, SIGNER_SIGNATURE_INFO*,
                                         PVOID, DWORD, PCSTR, PCWSTR, PCRYPT_ATTRIBUTES, PVOID,
                                         SIGNER_CONTEXT**, PVOID, PVOID);
using SignerFreeFn = HRESULT(WINAPI*)(SIGNER_CONTEXT*);

std::wstring Hex32(unsigned long v) {
    wchar_t buf[16];
    swprintf_s(buf, L"0x%08lX", v);
    return buf;
}

bool Encode(LPCSTR type, const void* value, BYTE*& out, DWORD& len) {
    return CryptEncodeObjectEx(X509_ASN_ENCODING, type, value, CRYPT_ENCODE_ALLOC_FLAG, nullptr, &out, &len) != FALSE;
}

}  // namespace

bool CreateSigningCert(SigningCert& out, std::wstring& error, bool machine_key) {
    const DWORD key_flags = machine_key ? NCRYPT_MACHINE_KEY_FLAG : 0;
    NCRYPT_PROV_HANDLE prov = 0;
    SECURITY_STATUS st = NCryptOpenStorageProvider(&prov, MS_KEY_STORAGE_PROVIDER, 0);
    if (st != ERROR_SUCCESS) {
        error = L"NCryptOpenStorageProvider " + Hex32(st);
        return false;
    }
    GUID guid;
    CoCreateGuid(&guid);
    wchar_t guid_text[64];
    StringFromGUID2(guid, guid_text, 64);
    out.key_name = std::wstring(L"VRLFVirtualGunSigning-") + guid_text;
    st = NCryptCreatePersistedKey(prov, &out.key, BCRYPT_RSA_ALGORITHM, out.key_name.c_str(), 0, key_flags);
    if (st == ERROR_SUCCESS) {
        DWORD bits = 3072;
        st = NCryptSetProperty(out.key, NCRYPT_LENGTH_PROPERTY, reinterpret_cast<PBYTE>(&bits), sizeof(bits), 0);
        if (st != ERROR_SUCCESS) {
            error = L"NCryptSetProperty length " + Hex32(st);
            NCryptFreeObject(prov);
            DestroySigningKey(out);
            return false;
        }
        // Export policy stays at its default of 0: the key can never leave the provider.
        st = NCryptFinalizeKey(out.key, 0);
    }
    NCryptFreeObject(prov);
    if (st != ERROR_SUCCESS) {
        error = L"key creation " + Hex32(st);
        DestroySigningKey(out);
        return false;
    }

    DWORD name_len = 0;
    CertStrToNameW(X509_ASN_ENCODING, CERT_SUBJECT, CERT_X500_NAME_STR, nullptr, nullptr, &name_len, nullptr);
    std::vector<BYTE> name(name_len);
    if (!CertStrToNameW(X509_ASN_ENCODING, CERT_SUBJECT, CERT_X500_NAME_STR, nullptr, name.data(), &name_len, nullptr)) {
        error = L"CertStrToNameW " + Hex32(GetLastError());
        DestroySigningKey(out);
        return false;
    }
    CERT_NAME_BLOB subject{name_len, name.data()};

    LPSTR usages[] = {const_cast<LPSTR>(szOID_PKIX_KP_CODE_SIGNING)};
    CERT_ENHKEY_USAGE eku{1, usages};
    BYTE key_usage_bits = CERT_DIGITAL_SIGNATURE_KEY_USAGE;
    CRYPT_BIT_BLOB key_usage{1, &key_usage_bits, 0};
    BYTE* eku_bytes = nullptr;
    DWORD eku_len = 0;
    BYTE* ku_bytes = nullptr;
    DWORD ku_len = 0;
    if (!Encode(X509_ENHANCED_KEY_USAGE, &eku, eku_bytes, eku_len) ||
        !Encode(X509_KEY_USAGE, &key_usage, ku_bytes, ku_len)) {
        error = L"extension encoding " + Hex32(GetLastError());
        LocalFree(eku_bytes);
        DestroySigningKey(out);
        return false;
    }
    CERT_EXTENSION extensions[2] = {
        {const_cast<LPSTR>(szOID_ENHANCED_KEY_USAGE), FALSE, {eku_len, eku_bytes}},
        {const_cast<LPSTR>(szOID_KEY_USAGE), TRUE, {ku_len, ku_bytes}},
    };
    CERT_EXTENSIONS extension_list{2, extensions};

    CRYPT_KEY_PROV_INFO prov_info{};
    prov_info.pwszContainerName = out.key_name.data();
    prov_info.pwszProvName = const_cast<LPWSTR>(MS_KEY_STORAGE_PROVIDER);
    prov_info.dwProvType = 0;
    prov_info.dwFlags = key_flags;

    CRYPT_ALGORITHM_IDENTIFIER algorithm{const_cast<LPSTR>(szOID_RSA_SHA256RSA), {0, nullptr}};

    SYSTEMTIME start;
    GetSystemTime(&start);
    FILETIME ft;
    SystemTimeToFileTime(&start, &ft);
    ULARGE_INTEGER t{};
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    t.QuadPart += 10ULL * 365ULL * 24ULL * 3600ULL * 10000000ULL;
    ft.dwLowDateTime = t.LowPart;
    ft.dwHighDateTime = t.HighPart;
    SYSTEMTIME end;
    FileTimeToSystemTime(&ft, &end);

    out.context = CertCreateSelfSignCertificate(out.key, &subject, 0, &prov_info, &algorithm, &start, &end,
                                                &extension_list);
    LocalFree(eku_bytes);
    LocalFree(ku_bytes);
    if (out.context == nullptr) {
        error = L"CertCreateSelfSignCertificate " + Hex32(GetLastError());
        DestroySigningKey(out);
        return false;
    }
    out.encoded.assign(out.context->pbCertEncoded, out.context->pbCertEncoded + out.context->cbCertEncoded);
    BYTE hash[20];
    DWORD hash_len = sizeof(hash);
    if (!CertGetCertificateContextProperty(out.context, CERT_SHA1_HASH_PROP_ID, hash, &hash_len)) {
        error = L"certificate thumbprint " + Hex32(GetLastError());
        DestroySigningKey(out);
        return false;
    }
    out.thumbprint = HexUpper(hash, hash_len);
    Log(L"signing certificate %ls created (%ls key)", out.thumbprint.c_str(), machine_key ? L"machine" : L"user");
    return true;
}

bool SignFile(const SigningCert& cert, const std::wstring& path, std::wstring& error) {
    HMODULE mssign = LoadLibraryW(L"mssign32.dll");
    if (mssign == nullptr) {
        error = L"LoadLibrary mssign32 " + Hex32(GetLastError());
        return false;
    }
    auto sign = reinterpret_cast<SignerSignEx2Fn>(GetProcAddress(mssign, "SignerSignEx2"));
    auto free_context = reinterpret_cast<SignerFreeFn>(GetProcAddress(mssign, "SignerFreeSignerContext"));
    if (sign == nullptr || free_context == nullptr) {
        error = L"mssign32 exports missing";
        FreeLibrary(mssign);
        return false;
    }
    SIGNER_FILE_INFO file{sizeof(SIGNER_FILE_INFO), path.c_str(), nullptr};
    DWORD index = 0;
    SIGNER_SUBJECT_INFO subject{};
    subject.cbSize = sizeof(subject);
    subject.pdwIndex = &index;
    subject.dwSubjectChoice = SIGNER_SUBJECT_FILE;
    subject.pSignerFileInfo = &file;
    SIGNER_CERT_STORE_INFO store{sizeof(SIGNER_CERT_STORE_INFO), cert.context, SIGNER_CERT_POLICY_CHAIN, nullptr};
    SIGNER_CERT signer{};
    signer.cbSize = sizeof(signer);
    signer.dwCertChoice = SIGNER_CERT_STORE;
    signer.pCertStoreInfo = &store;
    SIGNER_SIGNATURE_INFO signature{};
    signature.cbSize = sizeof(signature);
    signature.algidHash = CALG_SHA_256;
    signature.dwAttrChoice = SIGNER_NO_ATTR;
    SIGNER_CONTEXT* context = nullptr;
    const HRESULT hr = sign(0, &subject, &signer, &signature, nullptr, 0, nullptr, nullptr, nullptr, nullptr,
                            &context, nullptr, nullptr);
    if (context != nullptr) free_context(context);
    FreeLibrary(mssign);
    if (FAILED(hr)) {
        error = L"SignerSignEx2 " + path + L" " + Hex32(static_cast<unsigned long>(hr));
        return false;
    }
    Log(L"signed %ls", path.c_str());
    return true;
}

void DestroySigningKey(SigningCert& cert) {
    if (cert.key != 0) {
        // NCryptDeleteKey invalidates the handle whether or not it succeeds; never
        // fall back to NCryptFreeObject on it, or a failed delete double-frees.
        const SECURITY_STATUS st = NCryptDeleteKey(cert.key, 0);
        if (st != ERROR_SUCCESS) Log(L"NCryptDeleteKey 0x%08lX", st);
        cert.key = 0;
    }
    if (cert.context != nullptr) {
        CertFreeCertificateContext(cert.context);
        cert.context = nullptr;
    }
}

bool TrustCertificate(const std::vector<BYTE>& encoded, std::wstring& error) {
    for (const wchar_t* store_name : {L"Root", L"TrustedPublisher"}) {
        HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, store_name);
        if (store == nullptr) {
            error = std::wstring(L"CertOpenStore ") + store_name + L" " + Hex32(GetLastError());
            return false;
        }
        const BOOL ok = CertAddEncodedCertificateToStore(store, X509_ASN_ENCODING, encoded.data(),
                                                         static_cast<DWORD>(encoded.size()),
                                                         CERT_STORE_ADD_REPLACE_EXISTING, nullptr);
        const DWORD err = GetLastError();
        CertCloseStore(store, 0);
        if (!ok) {
            error = std::wstring(L"add to ") + store_name + L" " + Hex32(err);
            return false;
        }
        Log(L"certificate trusted in LocalMachine\\%ls", store_name);
    }
    return true;
}

void RemoveTrustedCertificate(const std::wstring& thumbprint) {
    if (thumbprint.size() != 40) return;
    BYTE hash[20];
    for (int i = 0; i < 20; ++i) {
        hash[i] = static_cast<BYTE>(std::stoul(thumbprint.substr(i * 2, 2), nullptr, 16));
    }
    CRYPT_HASH_BLOB blob{20, hash};
    for (const wchar_t* store_name : {L"Root", L"TrustedPublisher"}) {
        HCERTSTORE store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_LOCAL_MACHINE, store_name);
        if (store == nullptr) continue;
        PCCERT_CONTEXT found = nullptr;
        while ((found = CertFindCertificateInStore(store, X509_ASN_ENCODING, 0, CERT_FIND_SHA1_HASH, &blob,
                                                   nullptr)) != nullptr) {
            const BOOL deleted = CertDeleteCertificateFromStore(found);  // frees `found` either way
            if (!deleted) {
                Log(L"certificate delete from LocalMachine\\%ls failed: %lu", store_name, GetLastError());
                break;
            }
            Log(L"certificate removed from LocalMachine\\%ls", store_name);
        }
        CertCloseStore(store, 0);
    }
}

}  // namespace setup
