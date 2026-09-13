#pragma once
#include <cstddef>
#include <initializer_list>
#include <string>

namespace setup {

inline std::wstring HexUpper(const unsigned char* data, size_t n) {
    static constexpr wchar_t digits[] = L"0123456789ABCDEF";
    std::wstring out;
    out.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        out.push_back(digits[data[i] >> 4]);
        out.push_back(digits[data[i] & 0x0F]);
    }
    return out;
}

// REG_MULTI_SZ: each item NUL-terminated, then one more NUL.
inline std::wstring MultiSz(std::initializer_list<const wchar_t*> items) {
    std::wstring out;
    for (const wchar_t* item : items) {
        out += item;
        out.push_back(L'\0');
    }
    out.push_back(L'\0');
    return out;
}

}  // namespace setup
