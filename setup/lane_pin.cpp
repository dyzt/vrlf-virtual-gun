#include "setup/lane_pin.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>
#include <thread>

#include "WinUHid.h"
#include "setup/driver_install.h"
#include "setup/log.h"
#include "shared/gun_device.h"
#include "shared/lane_devnode.h"

namespace setup {

namespace {

constexpr const wchar_t* ENUM_KEY = L"SYSTEM\\CurrentControlSet\\Enum\\";
constexpr const wchar_t* VHF_KEY = L"SYSTEM\\CurrentControlSet\\Enum\\VHF\\HID_DEVICE_SYSTEM_VHF";
constexpr auto POLL = std::chrono::milliseconds(100);
constexpr auto MOUSE_TIMEOUT = std::chrono::seconds(15);
constexpr auto GONE_TIMEOUT = std::chrono::seconds(10);
constexpr int MAX_ROUNDS = 4;

std::wstring ReadString(const std::wstring& subkey, const wchar_t* value) {
    wchar_t buf[256] = {};
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, subkey.c_str(), value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) {
        return L"";
    }
    return buf;
}

LaneKey ReadLaneKey(const std::wstring& name) {
    LaneKey key;
    const std::wstring path = std::wstring(VHF_KEY) + L"\\" + name;
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ, &h) != ERROR_SUCCESS) return key;
    RegCloseKey(h);
    key.exists = true;
    key.prefix = ReadString(path, L"ParentIdPrefix");
    DWORD flags = 0;
    DWORD size = sizeof(flags);
    RegGetValueW(HKEY_LOCAL_MACHINE, path.c_str(), L"ConfigFlags", RRF_RT_REG_DWORD, nullptr, &flags, &size);
    key.installed = IsInstalledLaneKey(!ReadString(path, L"Driver").empty(), flags);
    return key;
}

std::vector<VhfKey> ReadVhfKeys() {
    std::vector<VhfKey> keys;
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, VHF_KEY, 0, KEY_READ, &h) != ERROR_SUCCESS) return keys;
    wchar_t name[256];
    for (DWORD i = 0;; ++i) {
        DWORD len = 256;
        if (RegEnumKeyExW(h, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        keys.push_back({name, ReadString(std::wstring(VHF_KEY) + L"\\" + name, L"ParentIdPrefix")});
    }
    RegCloseKey(h);
    return keys;
}

bool WritePrefix(const std::wstring& name, const std::wstring& prefix) {
    const std::wstring path = std::wstring(VHF_KEY) + L"\\" + name;
    const LSTATUS st = RegSetKeyValueW(HKEY_LOCAL_MACHINE, path.c_str(), L"ParentIdPrefix", REG_SZ, prefix.c_str(),
                                       static_cast<DWORD>((prefix.size() + 1) * sizeof(wchar_t)));
    if (st != ERROR_SUCCESS) Log(L"writing ParentIdPrefix on %ls failed: %ld", name.c_str(), st);
    return st == ERROR_SUCCESS;
}

bool WaitFor(std::chrono::milliseconds limit, bool (*done)(unsigned), unsigned lane) {
    const auto end = std::chrono::steady_clock::now() + limit;
    while (!done(lane)) {
        if (std::chrono::steady_clock::now() >= end) return false;
        std::this_thread::sleep_for(POLL);
    }
    return true;
}

// Creates the lane so PnP installs it (writing its key and a generated prefix), waits for its
// mouse, then takes it down again and waits until it is gone.
bool CreateOnce(unsigned lane) {
    const std::wstring instance = vgun::lane_instance_name(lane);
    WINUHID_DEVICE_CONFIG config = {};
    config.SupportedEvents = WINUHID_EVENT_NONE;
    config.VendorID = VGUN_VID;
    config.ProductID = VGUN_PID;
    config.VersionNumber = 1;
    config.ReportDescriptorLength = VGUN_REPORT_DESCRIPTOR_BYTES;
    config.ReportDescriptor = VGUN_REPORT_DESCRIPTOR;
    config.InstanceID = instance.c_str();
    PWINUHID_DEVICE device = WinUHidCreateDevice(&config);
    if (device == nullptr) {
        Log(L"lane %u: creating the device failed: %lu", lane, GetLastError());
        return false;
    }
    bool ok = WinUHidStartDevice(device, nullptr, nullptr) != FALSE;
    if (!ok) Log(L"lane %u: starting the device failed: %lu", lane, GetLastError());
    if (ok && !WaitFor(MOUSE_TIMEOUT, [](unsigned l) { return !vgun::lane_hid_instance_id(l).empty(); }, lane)) {
        Log(L"lane %u: its mouse did not start within 15 s", lane);
        ok = false;
    }
    if (ok) WinUHidStopDevice(device);
    WinUHidDestroyDevice(device);
    if (!WaitFor(GONE_TIMEOUT, [](unsigned l) { return vgun::lane_vhf_instances(l, true).empty(); }, lane)) {
        Log(L"lane %u: the device did not go away within 10 s", lane);
        return false;
    }
    return ok;
}

// True when the lane ends up pinned.
bool PinLane(const std::wstring& root_id, unsigned lane) {
    const std::wstring pinned = vgun::pinned_prefix(lane);
    for (int round = 0; round < MAX_ROUNDS; ++round) {
        const std::wstring root_prefix = ReadString(std::wstring(ENUM_KEY) + root_id, L"ParentIdPrefix");
        if (root_prefix.empty()) {
            // No lane was ever created under this root node; the first creation names the keys.
            Log(L"lane %u: no lane was ever created under %ls; creating it once", lane, root_id.c_str());
            if (!CreateOnce(lane)) return false;
            continue;
        }
        const std::wstring name = vgun::lane_key_name(root_prefix, lane);
        const LaneKey key = ReadLaneKey(name);
        switch (DecidePin(key, pinned, IsCollision(ReadVhfKeys(), name, lane, pinned))) {
            case PinAction::AlreadyPinned:
                Log(L"lane %u already pinned (%ls)", lane, pinned.c_str());
                return true;
            case PinAction::Write:
                if (!WritePrefix(name, pinned)) return false;
                Log(L"lane %u pinned: %ls -> %ls", lane, key.prefix.c_str(), pinned.c_str());
                return true;
            case PinAction::CreateFirst:
                Log(L"lane %u has no device key yet; creating it once", lane);
                if (!CreateOnce(lane)) return false;
                break;
            case PinAction::RemoveThenCreate:
                Log(L"lane %u key never finished installing; removing it", lane);
                if (!RemoveDeviceInstance(std::wstring(L"VHF\\") + VGUN_VHF_HARDWARE_ID + L"\\" + name)) return false;
                if (!CreateOnce(lane)) return false;
                break;
            case PinAction::Collision:
                Log(L"lane %u NOT pinned: another device already uses %ls", lane, pinned.c_str());
                return false;
        }
    }
    Log(L"lane %u NOT pinned: its key did not settle", lane);
    return false;
}

}  // namespace

PinSummary PinLanes() {
    PinSummary summary;
    if (WinUHidGetDriverInterfaceVersion() == 0) {
        Log(L"virtual gun driver not running (%lu); lanes not pinned", GetLastError());
        summary.driver_unavailable = true;
        return summary;
    }
    const std::wstring root_id = LiveRootInstanceId();
    if (root_id.empty()) {
        Log(L"no single live Root\\VRLFVirtualGun device; lanes not pinned");
        summary.failed = VGUN_MAX_LANES;
        return summary;
    }
    for (unsigned lane = 0; lane < VGUN_MAX_LANES; ++lane) {
        if (!PinLane(root_id, lane)) ++summary.failed;
    }
    return summary;
}

}  // namespace setup
