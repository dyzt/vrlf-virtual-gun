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

bool IsOurDevice(HDEVINFO set, SP_DEVINFO_DATA& dev) {
    wchar_t ids[1024] = {};
    if (!SetupDiGetDeviceRegistryPropertyW(set, &dev, SPDRP_HARDWAREID, nullptr,
                                           reinterpret_cast<PBYTE>(ids), sizeof(ids) - sizeof(wchar_t), nullptr)) {
        return false;
    }
    return _wcsicmp(ids, HARDWARE_ID) == 0;
}

}  // namespace

// Shared by every InstallDriver failure branch that runs after SetupCopyOEMInfW has already
// staged one, by the in-place update, and by UninstallDriver.
bool UnstagePackage(const std::wstring& published_inf) {
    if (published_inf.empty()) return true;
    if (SetupUninstallOEMInfW(published_inf.c_str(), SUOI_FORCEDELETE, nullptr)) {
        Log(L"driver package %ls deleted", published_inf.c_str());
        return true;
    }
    Log(L"SetupUninstallOEMInfW %ls failed: %lu", published_inf.c_str(), GetLastError());
    return false;
}

size_t CountDevices(bool present_only) {
    const DWORD flags = DIGCF_ALLCLASSES | (present_only ? DIGCF_PRESENT : 0);
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, flags);
    if (set == INVALID_HANDLE_VALUE) return 0;
    size_t count = 0;
    SP_DEVINFO_DATA dev{sizeof(SP_DEVINFO_DATA)};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dev); ++i) {
        if (IsOurDevice(set, dev)) ++count;
    }
    SetupDiDestroyDeviceInfoList(set);
    return count;
}

std::wstring LiveRootInstanceId() {
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (set == INVALID_HANDLE_VALUE) return L"";
    std::wstring found;
    size_t count = 0;
    SP_DEVINFO_DATA dev{sizeof(SP_DEVINFO_DATA)};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dev); ++i) {
        if (!IsOurDevice(set, dev)) continue;
        wchar_t id[MAX_DEVICE_ID_LEN] = {};
        if (SetupDiGetDeviceInstanceIdW(set, &dev, id, MAX_DEVICE_ID_LEN, nullptr)) found = id;
        ++count;
    }
    SetupDiDestroyDeviceInfoList(set);
    return count == 1 ? found : L"";
}

bool RemoveDeviceInstance(const std::wstring& instance_id) {
    HDEVINFO set = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (set == INVALID_HANDLE_VALUE) return false;
    SP_DEVINFO_DATA dev{sizeof(SP_DEVINFO_DATA)};
    const bool ok = SetupDiOpenDeviceInfoW(set, instance_id.c_str(), nullptr, 0, &dev) &&
                    DiUninstallDevice(nullptr, set, &dev, 0, nullptr);
    if (!ok) {
        Log(L"removing %ls failed: %lu", instance_id.c_str(), GetLastError());
    } else {
        Log(L"removed %ls", instance_id.c_str());
    }
    SetupDiDestroyDeviceInfoList(set);
    return ok;
}

size_t RemoveDevices() {
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return 0;
    size_t removed = 0;
    SP_DEVINFO_DATA dev{sizeof(SP_DEVINFO_DATA)};
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dev); ++i) {
        if (!IsOurDevice(set, dev)) continue;
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

// Stage the package into the driver store FIRST and learn its published oemNN.inf name
// deterministically from the call that creates it, rather than from a devnode property read
// after the fact (which can fail and leave `published_inf` empty, orphaning the staged
// package: UninstallDriver only calls SetupUninstallOEMInfW when it has a name).
bool StagePackage(const std::wstring& inf_path, std::wstring& published_inf, std::wstring& error) {
    wchar_t dest_inf[MAX_PATH] = {};
    PWSTR dest_name = nullptr;
    if (!SetupCopyOEMInfW(inf_path.c_str(), nullptr, SPOST_PATH, 0, dest_inf, MAX_PATH, nullptr, &dest_name)) {
        error = Err(L"SetupCopyOEMInfW");
        return false;
    }
    published_inf = dest_name;
    return true;
}

bool UpdateDevice(const std::wstring& inf_path, bool& reboot, std::wstring& error) {
    BOOL need_reboot = FALSE;
    if (!UpdateDriverForPlugAndPlayDevicesW(nullptr, HARDWARE_ID, inf_path.c_str(), INSTALLFLAG_FORCE, &need_reboot)) {
        error = Err(L"UpdateDriverForPlugAndPlayDevicesW");
        return false;
    }
    reboot = need_reboot != FALSE;
    Log(L"driver updated on the existing device node%ls", reboot ? L", reboot required" : L"");
    return true;
}

bool InstallDriver(const std::wstring& inf_path, std::wstring& published_inf, bool& reboot, std::wstring& error) {
    RemoveDevices();

    if (!StagePackage(inf_path, published_inf, error)) return false;  // nothing staged, nothing to roll back

    GUID class_guid;
    wchar_t class_name[MAX_CLASS_NAME_LEN];
    if (!SetupDiGetINFClassW(inf_path.c_str(), &class_guid, class_name, MAX_CLASS_NAME_LEN, nullptr)) {
        error = Err(L"SetupDiGetINFClassW");
        UnstagePackage(published_inf);
        return false;
    }
    HDEVINFO set = SetupDiCreateDeviceInfoList(&class_guid, nullptr);
    if (set == INVALID_HANDLE_VALUE) {
        error = Err(L"SetupDiCreateDeviceInfoList");
        UnstagePackage(published_inf);
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
        UnstagePackage(published_inf);
        return false;
    }
    // The store already holds the package (staged above), so the original inf_path resolves
    // to it whether or not the caller's staging folder still exists afterwards.
    BOOL need_reboot = FALSE;
    if (!UpdateDriverForPlugAndPlayDevicesW(nullptr, HARDWARE_ID, inf_path.c_str(), INSTALLFLAG_FORCE, &need_reboot)) {
        error = Err(L"UpdateDriverForPlugAndPlayDevicesW");
        SetupDiCallClassInstaller(DIF_REMOVE, set, &dev);
        SetupDiDestroyDeviceInfoList(set);
        UnstagePackage(published_inf);  // don't leak the staged package
        return false;
    }
    reboot = need_reboot != FALSE;

    // Cross-check only: DEVPKEY_Device_DriverInfPath should agree with the name SetupCopyOEMInfW
    // handed back above. A mismatch or a failed read here never changes `published_inf` and
    // never fails the install; it's a log line for the odd case, not a source of truth.
    wchar_t inf_name[MAX_PATH] = {};
    DEVPROPTYPE type = 0;
    if (SetupDiGetDevicePropertyW(set, &dev, &DEVPKEY_Device_DriverInfPath, &type,
                                  reinterpret_cast<PBYTE>(inf_name), sizeof(inf_name), nullptr, 0) &&
        _wcsicmp(inf_name, published_inf.c_str()) != 0) {
        Log(L"driver inf path reported as %ls (staged %ls)", inf_name, published_inf.c_str());
    }
    SetupDiDestroyDeviceInfoList(set);
    Log(L"driver installed (%ls)%ls", published_inf.c_str(), reboot ? L", reboot required" : L"");
    return true;
}

void UninstallDriver(const std::wstring& published_inf) {
    RemoveDevices();
    UnstagePackage(published_inf);
}

}  // namespace setup
