#!/usr/bin/env python3
"""Build vrlf-virtual-gun.

Usage: python build.py [tests|driver|cli|probe|setup|package|all] [--version X.Y.Z]
"""
import argparse
import hashlib
import os
import shutil
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
BUILD = os.path.join(ROOT, "build")
VCVARS = r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
WDK = r"C:\Program Files (x86)\Windows Kits\10"
WDK_VER = "10.0.26100.0"
UMDF_MINOR = "15"

CXX = "cl /nologo /std:c++17 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS"

TEST_EXES = {
    "gun_device_tests": [],
}


def run_msvc(cmd, cwd):
    """Run one command inside a vcvars64 environment; exit on failure."""
    full = f'"{VCVARS}" >nul 2>&1 && {cmd}'
    r = subprocess.run(full, shell=True, cwd=cwd, capture_output=True, text=True)
    sys.stdout.write(r.stdout[-8000:])
    sys.stderr.write(r.stderr[-4000:])
    if r.returncode != 0:
        sys.exit(f"build step failed ({r.returncode}): {cmd[:160]}")


def out_dir(*parts):
    d = os.path.join(BUILD, *parts)
    os.makedirs(d, exist_ok=True)
    return d


def quoted(paths):
    return " ".join(f'"{os.path.join(ROOT, p)}"' for p in paths)


def build_tests():
    d = out_dir("tests")
    for name, extra in TEST_EXES.items():
        srcs = quoted([f"tests/{name}.cpp"] + extra)
        run_msvc(f'{CXX} /I"{ROOT}" {srcs} /Fe:{name}.exe', d)
        r = subprocess.run([os.path.join(d, name + ".exe")], capture_output=True, text=True)
        print(r.stdout)
        if r.returncode != 0:
            sys.exit(f"{name} failed")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("target", nargs="?", default="tests")
    ap.add_argument("--version", default="1.0.0")
    args = ap.parse_args()
    targets = {
        "tests": lambda: build_tests(),
    }
    if args.target not in targets:
        sys.exit(f"unknown target {args.target}")
    targets[args.target]()


if __name__ == "__main__":
    main()
