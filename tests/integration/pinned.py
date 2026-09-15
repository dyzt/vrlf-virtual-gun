"""The pinned lane identity from shared/gun_device.h, so Python never restates it."""
import os
import re

HEADER = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                      "shared", "gun_device.h")


def _define(name):
    with open(HEADER, encoding="utf-8") as f:
        m = re.search(rf'#define {name} L"([^"]+)"', f.read())
    if not m:
        raise RuntimeError(f"{name} not found in {HEADER}")
    return m.group(1)


def pinned_prefix(lane):
    return _define("VGUN_PINNED_PREFIX_FORMAT").replace("%x", format(lane, "x"))


def pinned_instance_id(lane):
    return f"HID\\{_define('VGUN_VHF_HARDWARE_ID')}\\{pinned_prefix(lane)}&0000"


def pinned_device_path(lane):
    hwid, guid = _define("VGUN_VHF_HARDWARE_ID"), _define("VGUN_MOUSE_INTERFACE_GUID")
    return f"\\\\?\\HID#{hwid}#{pinned_prefix(lane)}&0000#{guid}"
