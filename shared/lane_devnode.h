#pragma once
/*
 * Finds a virtual gun lane's devnodes through cfgmgr32. Windows-only C++, header-only; include
 * gun_device.h (or VRLF's virtual_gun_device.h) first, and link cfgmgr32.lib.
 * Devnode tree: Root\VRLFVirtualGun -> VHF\HID_DEVICE_SYSTEM_VHF\<prefix>&VRLFGun<N> ->
 * HID\HID_DEVICE_SYSTEM_VHF\<lane ParentIdPrefix>&0000 (the mouse Raw Input sees).
 */
#include <windows.h>

#include <cfgmgr32.h>

#include <cwchar>
#include <string>
#include <vector>

namespace vgun {

// Instance IDs of lane N's VHF devnodes: present ones, or every one Windows remembers.
inline std::vector<std::wstring> lane_vhf_instances(unsigned lane, bool present_only) {
    std::vector<std::wstring> out;
    const ULONG flags = CM_GETIDLIST_FILTER_ENUMERATOR | (present_only ? CM_GETIDLIST_FILTER_PRESENT : 0);
    for (int attempt = 0; attempt < 3; ++attempt) {
        ULONG len = 0;
        if (CM_Get_Device_ID_List_SizeW(&len, L"VHF", flags) != CR_SUCCESS || len == 0) return out;
        std::vector<wchar_t> buf(len);
        const CONFIGRET cr = CM_Get_Device_ID_ListW(L"VHF", buf.data(), len, flags);
        if (cr == CR_BUFFER_SMALL) continue;  // a device arrived between the two calls
        if (cr != CR_SUCCESS) return out;
        for (const wchar_t* p = buf.data(); *p != L'\0'; p += wcslen(p) + 1) {
            if (is_lane_key_of(p, lane)) out.emplace_back(p);
        }
        return out;
    }
    return out;
}

// The started HID mouse under lane N's present VHF devnode, or empty (lane down, or PnP has
// not started the mouse yet).
inline std::wstring lane_hid_instance_id(unsigned lane) {
    for (const std::wstring& vhf : lane_vhf_instances(lane, true)) {
        std::wstring id_copy = vhf;
        DEVINST parent = 0;
        DEVINST child = 0;
        if (CM_Locate_DevNodeW(&parent, id_copy.data(), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) continue;
        if (CM_Get_Child(&child, parent, 0) != CR_SUCCESS) continue;
        ULONG status = 0;
        ULONG problem = 0;
        if (CM_Get_DevNode_Status(&status, &problem, child, 0) != CR_SUCCESS || (status & DN_STARTED) == 0) continue;
        wchar_t id[MAX_DEVICE_ID_LEN] = {};
        if (CM_Get_Device_IDW(child, id, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) continue;
        return id;
    }
    return {};
}

}  // namespace vgun
