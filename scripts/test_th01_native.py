#!/usr/bin/env python3
"""Compile and run the th01 native tests (tests/native/th01_*.c).

    python3 scripts/test_th01_native.py
    python3 scripts/test_th01_native.py --sanitize   # ASan + UBSan + leak check

Thin wrapper around `scripts/build_th01.py --target native` so CI and local runs
use exactly the same compile flags as the real build.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--sanitize", action="store_true",
                        help="build with -fsanitize=address,undefined")
    args = parser.parse_args()

    command = [sys.executable, str(ROOT / "scripts" / "build_th01.py"), "--target", "native"]
    if args.sanitize or os.environ.get("TH01_SANITIZE"):
        command.append("--sanitize")
        os.environ.setdefault("ASAN_OPTIONS", "detect_leaks=1")
        os.environ.setdefault("UBSAN_OPTIONS", "print_stacktrace=1:halt_on_error=1")

    print("$", " ".join(command))
    return subprocess.run(command, cwd=str(ROOT), env=os.environ).returncode


if __name__ == "__main__":
    sys.exit(main())
