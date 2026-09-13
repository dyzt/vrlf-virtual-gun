#include "setup/log.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <string>

namespace setup {

void Log(const wchar_t* fmt, ...) {
    wchar_t msg[2048];
    va_list args;
    va_start(args, fmt);
    _vsnwprintf_s(msg, _TRUNCATE, fmt, args);
    va_end(args);
    wprintf(L"%ls\n", msg);
    wchar_t base[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"ProgramData", base, MAX_PATH);
    std::wstring dir = (n > 0 && n < MAX_PATH) ? std::wstring(base) : L"C:\\ProgramData";
    dir += L"\\VRLF";
    CreateDirectoryW(dir.c_str(), nullptr);
    dir += L"\\VirtualGun";
    CreateDirectoryW(dir.c_str(), nullptr);
    FILE* f = nullptr;
    if (_wfopen_s(&f, (dir + L"\\setup.log").c_str(), L"a, ccs=UTF-8") == 0 && f) {
        SYSTEMTIME t;
        GetLocalTime(&t);
        fwprintf(f, L"%04u-%02u-%02u %02u:%02u:%02u %ls\n", t.wYear, t.wMonth, t.wDay,
                 t.wHour, t.wMinute, t.wSecond, msg);
        fclose(f);
    }
}

}  // namespace setup
