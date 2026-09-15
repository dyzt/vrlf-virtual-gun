#pragma once
/*
 * The one device type VRLFVirtualGun creates (spec D10). Included by the driver
 * (C), the tools, the installer and VRLF (C++). Everything outside the
 * __cplusplus block must stay valid C.
 */

#define VGUN_VID 0x1209            /* pid.codes open-source VID */
#define VGUN_PID 0x5647            /* project PID requested from pid.codes; see README */
#define VGUN_MAX_COORD 32767
#define VGUN_BUTTON_MASK 0x1F      /* 5 buttons */
#define VGUN_REPORT_BYTES 5
#define VGUN_MAX_LANES 8
#define VGUN_INSTANCE_ID_FORMAT L"VRLFGun%u"
/* Setup pins every lane's VHF devnode ParentIdPrefix to this value, so lane N's Raw Input
 * path is identical on every PC ("56524c" is "VRL"; the last hex digit is the lane). */
#define VGUN_PINNED_PREFIX_FORMAT L"2&56524c3%x&0"
#define VGUN_VHF_HARDWARE_ID L"HID_DEVICE_SYSTEM_VHF"
#define VGUN_MOUSE_INTERFACE_GUID L"{378de44c-56ef-11d1-bc8c-00a0c91405dd}"

/* Absolute mouse: 5 buttons, 3 padding bits, X and Y 16-bit 0..32767, no report ID. */
static const unsigned char VGUN_REPORT_DESCRIPTOR[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01,        /* Generic Desktop, Mouse, Collection (Application) */
    0x09, 0x01, 0xA1, 0x00,                    /*   Pointer, Collection (Physical) */
    0x05, 0x09, 0x19, 0x01, 0x29, 0x05,        /*     Buttons 1..5 */
    0x15, 0x00, 0x25, 0x01, 0x95, 0x05, 0x75, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x03, 0x81, 0x03,        /*     3 bits padding */
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31,        /*     X, Y */
    0x16, 0x00, 0x00, 0x26, 0xFF, 0x7F,        /*     Logical 0..32767 */
    0x75, 0x10, 0x95, 0x02, 0x81, 0x02,        /*     Input (Data, Var, Abs) */
    0xC0, 0xC0,
};
#define VGUN_REPORT_DESCRIPTOR_BYTES ((unsigned short)sizeof(VGUN_REPORT_DESCRIPTOR))

#ifdef __cplusplus
#include <cstdint>
#include <cwchar>
#include <string>

namespace vgun {

#pragma pack(push, 1)
struct GunReport {
    uint8_t buttons;
    uint16_t x;
    uint16_t y;
};
#pragma pack(pop)

inline constexpr GunReport GUN_CENTRE{0, 16384, 16384};

inline uint16_t clamp_coord(int v) {
    return static_cast<uint16_t>(v < 0 ? 0 : (v > VGUN_MAX_COORD ? VGUN_MAX_COORD : v));
}

inline GunReport make_report(int x, int y, int buttons) {
    GunReport r{};
    r.buttons = static_cast<uint8_t>(buttons & VGUN_BUTTON_MASK);
    r.x = clamp_coord(x);
    r.y = clamp_coord(y);
    return r;
}

inline bool same_id(const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }

inline std::wstring lane_instance_name(unsigned lane) {
    wchar_t buf[16];
    swprintf(buf, 16, VGUN_INSTANCE_ID_FORMAT, lane);
    return buf;
}

inline std::wstring pinned_prefix(unsigned lane) {
    wchar_t buf[16];
    swprintf(buf, 16, VGUN_PINNED_PREFIX_FORMAT, lane);
    return buf;
}

inline std::wstring pinned_hid_instance_id(unsigned lane) {
    return std::wstring(L"HID\\") + VGUN_VHF_HARDWARE_ID + L"\\" + pinned_prefix(lane) + L"&0000";
}

inline std::wstring pinned_device_path(unsigned lane) {
    return std::wstring(L"\\\\?\\HID#") + VGUN_VHF_HARDWARE_ID + L"#" + pinned_prefix(lane) + L"&0000#" +
           VGUN_MOUSE_INTERFACE_GUID;
}

/* A lane's VHF devnode key under Enum\VHF\HID_DEVICE_SYSTEM_VHF. */
inline std::wstring lane_key_name(const std::wstring& root_prefix, unsigned lane) {
    return root_prefix + L"&" + lane_instance_name(lane);
}

/* Does a lane key name or VHF instance ID end in "&VRLFGun<lane>" (any case)? */
inline bool is_lane_key_of(const std::wstring& name, unsigned lane) {
    const std::wstring suffix = L"&" + lane_instance_name(lane);
    return name.size() >= suffix.size() && same_id(name.substr(name.size() - suffix.size()), suffix);
}

}  // namespace vgun
#endif
