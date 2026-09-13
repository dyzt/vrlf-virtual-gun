#include "setup/driver_install.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cfgmgr32.h>  // MAX_CLASS_NAME_LEN; not pulled in transitively by setupapi.h
#include <newdev.h>
#include <setupapi.h>
#define INITGUID
#include <devpkey.h>

#include "setup/log.h"
#include "setup/strings.h"

namespace setup {

namespace {

std::wstring Err(const wchar_t* what) {
    wchar_t buf[128];
    swprintf_s(buf, L"%ls failed: %lu", what, GetLastError());
    return buf;
}

}  // namespace

size_t RemoveDevices() {
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return 0;
    size_t removed = 0;
    SP_DEVINFO_DATA dev{sizeof(SP_DEVINFO_DATA)};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dev); ++i) {
        wchar_t ids[1024] = {};
        if (!SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID, nullptr,
                                               reinterpret_cast<PBYTE>(ids), sizeof(ids) - sizeof(wchar_t), nullptr)) {
            continue;
        }
        if (_wcsicmp(ids, HARDWARE_ID) != 0) continue;
        if (DiUninstallDevice(nullptr, set, &dev, 0, nullptr)) {
            ++removed;
        } else {
            Log(L"DiUninstallDevice failed: %lu", GetLastError());
        }
    }
    SetupDiDestroyDeviceInfoList(set);
    if (removed > 0) Log(L"removed %zu device node(s)", removed);
    return removed;
}

bool InstallDriver(const std::wstring& inf_path, std::wstring& published_inf, bool& reboot, std::wstring& error) {
    RemoveDevices();
    GUID class_guid;
    wchar_t class_name[MAX_CLASS_NAME_LEN];
    if (!SetupDiGetINFClassW(inf_path.c_str(), &class_guid, class_name, MAX_CLASS_NAME_LEN, nullptr)) {
        error = Err(L"SetupDiGetINFClassW");
        return false;
    }
    HDEVINFO set = SetupDiCreateDeviceInfoList(&class_guid, nullptr);
    if (set == INVALID_HANDLE_VALUE) {
        error = Err(L"SetupDiCreateDeviceInfoList");
        return false;
    }
    SP_DEVINFO_DATA dev{sizeof(SP_DEVINFO_DATA)};
    const std::wstring ids = MultiSz({HARDWARE_ID});
    bool ok = SetupDiCreateDeviceInfoW(set, class_name, &class_guid, nullptr, nullptr, DICD_GENERATE_ID, &dev) &&
              SetupDiSetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID,
                                                reinterpret_cast<const BYTE*>(ids.c_str()),
                                                static_cast<DWORD>(ids.size() * sizeof(wchar_t))) &&
              SetupDiCallClassInstaller(DIF_REGISTERDEVICE, set, &dev);
    if (!ok) {
        error = Err(L"create root device");
        SetupDiDestroyDeviceInfoList(set);
        return false;
    }
    BOOL need_reboot = FALSE;
    if (!UpdateDriverForPlugAndPlayDevicesW(nullptr, HARDWARE_ID, inf_path.c_str(), INSTALLFLAG_FORCE, &need_reboot)) {
        error = Err(L"UpdateDriverForPlugAndPlayDevicesW");
        SetupDiCallClassInstaller(DIF_REMOVE, set, &dev);
        SetupDiDestroyDeviceInfoList(set);
        return false;
    }
    reboot = need_reboot != FALSE;
    wchar_t inf_name[MAX_PATH] = {};
    DEVPROPTYPE type = 0;
    if (SetupDiGetDevicePropertyW(set, &dev, &DEVPKEY_Device_DriverInfPath, &type,
                                  reinterpret_cast<PBYTE>(inf_name), sizeof(inf_name), nullptr, 0)) {
        published_inf = inf_name;
    }
    SetupDiDestroyDeviceInfoList(set);
    Log(L"driver installed (%ls)%ls", published_inf.c_str(), reboot ? L", reboot required" : L"");
    return true;
}

void UninstallDriver(const std::wstring& published_inf) {
    RemoveDevices();
    if (published_inf.empty()) return;
    if (SetupUninstallOEMInfW(published_inf.c_str(), SUOI_FORCEDELETE, nullptr)) {
        Log(L"driver package %ls deleted", published_inf.c_str());
    } else {
        Log(L"SetupUninstallOEMInfW %ls failed: %lu", published_inf.c_str(), GetLastError());
    }
}

}  // namespace setup
