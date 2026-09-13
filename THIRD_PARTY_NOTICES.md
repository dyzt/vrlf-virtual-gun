# Third-party notices

## WinUHid

Source: https://github.com/cgutman/WinUHid (commit d6cebbef5c7909168d1f881185be8f607d6aefd4)

Used for: `driver/` (renamed fork) and `third_party/winuhid/` (user-mode library).

Changes to the driver: control device renamed to `VRLFVirtualGun`; hardware ID `Root\VRLFVirtualGun`; new trace GUID; WPP tracing compiled out; INF identities renamed; the control device ACL admits interactive users; only the compiled-in lightgun report descriptor and VID/PID are accepted; the last input report is resubmitted with its buttons cleared when a handle closes; private header renamed `Driver.h` and includes flattened.

Changes to the library: `Public.h` reached by a flat include.

MIT License, Copyright (c) 2025 Cameron Gutman. Full text: `third_party/winuhid/LICENSE`.
