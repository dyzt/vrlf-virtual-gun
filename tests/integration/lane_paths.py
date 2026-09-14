"""Records or compares the Raw Input device path of each virtual gun lane.

Games that bind by device path (TeknoParrot's RawInput API) lose their bindings when a
path changes, so a driver update must keep them. Run `save` before installing an update
and `compare` after it. Exit 0 = same paths.

  python tests/integration/lane_paths.py save
  python tests/integration/lane_paths.py compare
"""
import json
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(__file__))
from raw_input import Watcher  # noqa: E402
from vgun_cli import GunCli  # noqa: E402

LANES = (0, 1)
RECORD = os.path.join(tempfile.gettempdir(), "vrlf-lane-paths.json")


def read_paths():
    watcher, cli, paths = Watcher(), GunCli(), {}
    try:
        for lane in LANES:
            ok, err = cli.create(lane)
            if not ok:
                sys.exit(f"lane {lane} create failed: Win32 error {err}")
        watcher.pump(2.0)
        for lane in LANES:
            watcher.events.clear()
            for _ in range(5):
                cli.state(lane, 16384, 16384, 1)
                watcher.pump(0.03)
            cli.state(lane, 16384, 16384, 0)
            watcher.pump(0.3)
            names = {e[1] for e in watcher.ours()}
            if len(names) != 1:
                sys.exit(f"lane {lane}: expected one device path, saw {sorted(names)}")
            paths[str(lane)] = names.pop()
    finally:
        cli.quit()
    return paths


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in ("save", "compare"):
        sys.exit(__doc__)
    paths = read_paths()
    for lane, path in paths.items():
        print(f"lane {lane}: {path}")
    if sys.argv[1] == "save":
        with open(RECORD, "w", encoding="utf-8") as f:
            json.dump(paths, f)
        print(f"saved to {RECORD}")
        return
    with open(RECORD, encoding="utf-8") as f:
        saved = json.load(f)
    changed = [lane for lane in paths if saved.get(lane) != paths[lane]]
    if changed:
        for lane in changed:
            print(f"lane {lane} CHANGED from {saved.get(lane)}")
        sys.exit("FAIL: device paths changed")
    print("PASS: device paths unchanged")


if __name__ == "__main__":
    main()
