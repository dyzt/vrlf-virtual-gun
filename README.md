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
- Over an existing install, updates in place: the device node is kept and only the driver, certificate and package are replaced, so the Raw Input device paths games have bound (TeknoParrot's RawInput API) stay the same. An incomplete or damaged install is removed and reinstalled instead, which changes those paths.
- Uninstall renames a still-running installer aside and removes it at the next reboot, so a reinstall before that reboot is safe.

## What the driver does

A fork of [WinUHid](https://github.com/cgutman/WinUHid) that creates exactly one kind of device: a 5-button absolute mouse (`shared/gun_device.h`). Any other report descriptor or VID/PID is refused. Interactive users may open it. A device is destroyed when the handle that created it closes, after its buttons are released, so a crashed client never leaves a game holding fire.

The device reports VID `0x1209` (pid.codes) with PID `0x5647`, requested for this project in [pidcodes/pidcodes.github.com#1277](https://github.com/pidcodes/pidcodes.github.com/pull/1277) and pending approval. v1.0.0 used the pid.codes test PID `0x0001`; the driver accepts only the ID it was built with, so a VRLF build and a driver version must agree.

## Security

- The signing key is deleted during install; only files signed then chain to the trusted certificate.
- Residual risk: any program already running as the logged-in user can create and drive virtual guns, and HID input is not limited by UIPI. The driver only ever creates the fixed 5-button absolute mouse.

## Build

Needs MSVC 2026, WDK 10.0.26100 and Python 3.

- `python build.py tests`
- `python build.py package --version 1.0.2`
- `python build.py cli` then `python tests/integration/smoke_cli.py` (no admin)
- `python tests/integration/test_installed_driver.py` (driver installed)
- `python tests/integration/lane_paths.py save`, install an update, then `python tests/integration/lane_paths.py compare`
- `python tests/integration/check_store_signature.py` (the driver store copy is signed by the one trusted certificate)

## Using the driver from your own program

Vendor `third_party/winuhid/*` and `driver/Public.h` into one directory, compile `WinUHid.cpp` with `WINUHID_STATIC`, and follow `tools/vgun_cli.cpp`: `WinUHidCreateDevice` with the descriptor from `shared/gun_device.h`, `WinUHidStartDevice(device, NULL, NULL)`, then `WinUHidSubmitInputReport` with 5-byte reports. Instance IDs are `VRLFGun<N>`; two programs creating the same N on one PC collide.

## Licence

MIT. Driver and user library derive from WinUHid (MIT); see `THIRD_PARTY_NOTICES.md`.

## Verification 2026-09-13

Dev PC, Windows 11 Pro 26200, Secure Boot on, test signing off, Smart App Control off.

- `install` exit code 0; device `ROOT\SYSTEM\0008` Started under `oem88.inf`. A second `install` over the live install took the "existing install found" path and came back clean.
- `signtool verify /pa` on `VRLFVirtualGun.dll` and `vrlfvirtualgun.cat`: both `Successfully verified` against the per-machine certificate, no private key left behind.
- `tests/integration/smoke_cli.py`: `PASS`.
- `tests/integration/test_installed_driver.py`: two lanes gave two VHF Raw Input mice, 22 events, two distinct handles, absolute moves and both buttons seen. `NOTES: duplicate VRLFGun0 from a second process -> ok=False err=87` (a second program asking for an instance ID in use is refused with ERROR_INVALID_PARAMETER). `PASS`.
- `uninstall` exit code 0; certs 0, packages 0, devices 0, state key gone. The running installer was renamed aside and scheduled for delete at reboot.
