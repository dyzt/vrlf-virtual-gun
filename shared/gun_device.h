#pragma once
/*
 * The one device type VRLFVirtualGun creates (spec D10). Included by the driver
 * (C), the tools, the installer and VRLF (C++). Everything outside the
 * __cplusplus block must stay valid C.
 */

#define VGUN_VID 0x1209            /* pid.codes open-source VID */
#define VGUN_PID 0x0001            /* pid.codes test PID; swap for the assigned PID before a public release */
#define VGUN_MAX_COORD 32767
#define VGUN_BUTTON_MASK 0x1F      /* 5 buttons */
#define VGUN_REPORT_BYTES 5
#define VGUN_MAX_LANES 8
#define VGUN_INSTANCE_ID_FORMAT L"VRLFGun%u"

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

}  // namespace vgun
#endif
