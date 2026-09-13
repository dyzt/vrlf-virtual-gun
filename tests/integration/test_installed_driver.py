"""End-to-end check against the INSTALLED driver. Exit 0 = pass."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from raw_input import (LEFT_DOWN, LEFT_UP, MOUSE_MOVE_ABSOLUTE, RAW_PER_DEVICE_UNIT,  # noqa: E402
                       RIGHT_DOWN, Watcher, vhf_mice)
from vgun_cli import GunCli  # noqa: E402

LANE0_X = 8192
LANE0_RAW_X = round(LANE0_X * RAW_PER_DEVICE_UNIT)  # 16384


def main():
    failures = []
    notes = []
    before = len(vhf_mice())
    print("VHF mice before:", before)
    watcher = Watcher()
    cli = None
    second = None
    try:
        cli = GunCli()
        ver = cli.version()
        print("version ->", ver)
        if ver == 0:
            failures.append("driver interface version 0: not installed or not openable")
        for lane in (0, 1):
            ok, err = cli.create(lane)
            print(f"create({lane}) ->", ok, err)
            if not ok:
                failures.append(f"create {lane} err {err}")
        watcher.pump(2.0)
        during = len(vhf_mice())
        print("VHF mice with two lanes:", during)
        if during != before + 2:
            failures.append(f"expected {before + 2} VHF mice, saw {during}")

        # Reports right after creation can fail with ERROR_NOT_READY (21) until PnP
        # has started the device; the pump above gives it two seconds.
        watcher.events.clear()
        for _ in range(10):
            cli.state(0, LANE0_X, LANE0_X, 1)
            cli.state(1, 24575, 24575, 2)
            watcher.pump(0.03)
        cli.state(0, LANE0_X, LANE0_X, 0)
        cli.state(1, 24575, 24575, 0)
        watcher.pump(0.5)
        ours = watcher.ours()
        handles = {e[0] for e in ours}
        print("events from VHF devices:", len(ours), "distinct handles:", len(handles))
        if len(handles) < 2:
            failures.append("fewer than two distinct device handles produced input")
        if not any(e[2] & MOUSE_MOVE_ABSOLUTE for e in ours):
            failures.append("no MOUSE_MOVE_ABSOLUTE events")
        if not any(e[3] & LEFT_DOWN for e in ours):
            failures.append("no left-button-down from lane 0")
        if not any(e[3] & RIGHT_DOWN for e in ours):
            failures.append("no right-button-down from lane 1")
        if not any(e[4] == LANE0_RAW_X and e[5] == LANE0_RAW_X for e in ours):
            failures.append(f"lane 0 device {LANE0_X} did not arrive as raw {LANE0_RAW_X}")

        # Spec D11: a second program creating the same instance ID. Recorded, not asserted.
        second = GunCli()
        ok, err = second.create(0)
        notes.append(f"duplicate VRLFGun0 from a second process -> ok={ok} err={err}")
        print(notes[-1])
        second.quit()

        # Crash with lane 0's trigger held: the driver must release it and remove both devices.
        cli.state(0, LANE0_X, LANE0_X, 1)
        watcher.pump(0.3)
        watcher.events.clear()
        cli.kill()
        watcher.pump(2.0)
        after_crash = watcher.ours()
        if not any(e[3] & LEFT_UP for e in after_crash):
            failures.append("no left-button-up after the client died with the trigger held")
        after = len(vhf_mice())
        print("VHF mice after the client died:", after)
        if after != before:
            failures.append(f"devices left behind: {after - before}")
    except Exception as e:  # noqa: BLE001
        failures.append(repr(e))
    finally:
        for proc in (second, cli):
            if proc is not None and proc.p.poll() is None:
                proc.kill()

    print("NOTES: " + "; ".join(notes))
    print("PASS" if not failures else "FAIL: " + "; ".join(failures))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
