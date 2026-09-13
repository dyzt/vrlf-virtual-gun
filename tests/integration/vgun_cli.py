"""Python wrapper over build/tools/vgun_cli.exe (one reply line per command)."""
import os
import subprocess

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(ROOT, "build", "tools", "vgun_cli.exe")


class GunCli:
    def __init__(self, exe=EXE):
        self.p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)

    def _cmd(self, line):
        self.p.stdin.write(line + "\n")
        self.p.stdin.flush()
        reply = self.p.stdout.readline().strip()
        if not reply:
            raise RuntimeError(f"vgun_cli died on: {line}")
        return reply.split()

    def version(self):
        return int(self._cmd("version")[1])

    def _status(self, parts):
        return (True, 0) if parts[0] in ("ok", "sent") else (False, int(parts[2]))

    def create(self, lane):
        return self._status(self._cmd(f"create {lane}"))

    def state(self, lane, x, y, buttons):
        return self._status(self._cmd(f"state {lane} {x} {y} {buttons}"))

    def destroy(self, lane):
        self._cmd(f"destroy {lane}")

    def quit(self):
        self._cmd("quit")
        self.p.wait(5)

    def kill(self):
        """Simulates a crash: no destroy, no release."""
        self.p.kill()
        self.p.wait()
