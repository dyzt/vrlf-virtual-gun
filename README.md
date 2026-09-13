# VRLF Virtual Lightgun

Virtual absolute-mouse lightguns for VR Lightgun Framework. Games that read Raw Input and ignore `SendInput` see each VRLF gun as a real device.

## Install

Through the mod manager: `vrlf-mods virtualgun install`.

By hand: unzip a release, run `vrlf-virtual-gun-setup.exe install` as administrator from the unzipped folder. Uninstall: `"%ProgramFiles%\VRLF Virtual Lightgun\vrlf-virtual-gun-setup.exe" uninstall`.

## What install does

- Generates a code-signing certificate on this PC, signs the driver, deletes the private key.
- Trusts the public certificate in `LocalMachine\Root` and `TrustedPublisher`.
- Installs the `Root\VRLFVirtualGun` driver.
- Records state under `HKLM\SOFTWARE\VRLF\VirtualGun`; uninstall removes all of it.

## What the driver does

A fork of [WinUHid](https://github.com/cgutman/WinUHid) that creates exactly one kind of device: a 5-button absolute mouse (`shared/gun_device.h`). Any other report descriptor or VID/PID is refused. Interactive users may open it. A device is destroyed when the handle that created it closes, after its buttons are released, so a crashed client never leaves a game holding fire.

## Security

- The signing key is deleted during install; only files signed then chain to the trusted certificate.
- Residual risk: any program already running as the logged-in user can create and drive virtual guns, and HID input is not limited by UIPI. The driver only ever creates the fixed 5-button absolute mouse.

## Build

Needs MSVC 2026, WDK 10.0.26100 and Python 3.

- `python build.py tests`
- `python build.py package --version 1.0.0`
- `python build.py cli` then `python tests/integration/smoke_cli.py` (no admin)
- `python tests/integration/test_installed_driver.py` (driver installed)

## Using the driver from your own program

Vendor `third_party/winuhid/*` and `driver/Public.h` into one directory, compile `WinUHid.cpp` with `WINUHID_STATIC`, and follow `tools/vgun_cli.cpp`: `WinUHidCreateDevice` with the descriptor from `shared/gun_device.h`, `WinUHidStartDevice(device, NULL, NULL)`, then `WinUHidSubmitInputReport` with 5-byte reports. Instance IDs are `VRLFGun<N>`; two programs creating the same N on one PC collide.

## Licence

MIT. Driver and user library derive from WinUHid (MIT); see `THIRD_PARTY_NOTICES.md`.
