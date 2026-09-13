"""CLI smoke test. No admin needed. Exit 0 = pass."""
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
from vgun_cli import GunCli  # noqa: E402


def main():
    failures = []
    cli = GunCli()
    try:
        ver = cli.version()
        print("version ->", ver)
        ok, err = cli.create(0)
        print("create(0) ->", ok, err)
        if ver == 0:
            if ok:
                failures.append("create succeeded without a usable driver")
            elif err == 0:
                failures.append("failed create reported error 0")
        else:
            if not ok:
                failures.append(f"create failed with the driver present: {err}")
            ok, err = cli.state(0, 16384, 16384, 0)
            print("state(0) ->", ok, err)
            cli.destroy(0)
        ok, err = cli.create(99)
        if ok or err != 87:
            failures.append(f"lane 99 should fail with 87, got {ok},{err}")
        cli.quit()
    except Exception as e:  # noqa: BLE001
        failures.append(repr(e))
        cli.kill()
    print("PASS" if not failures else "FAIL: " + "; ".join(failures))
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
