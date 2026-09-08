#!/usr/bin/env python3
"""Fetch the pinned TH095 reconstruction source.

Clones N0zoM1z0/th095 at the exact commit recorded in upstream.json into
build-inputs/th095-src (repository root). Refuses to overwrite an existing
fetch unless its HEAD matches the pinned commit.

    python3 th095/scripts/fetch_upstream.py [--repo-root DIR] [--force]

No game data is fetched: only the MIT-licensed reconstruction source.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=None, help="repository root (default: two levels above this script)")
    parser.add_argument("--force", action="store_true", help="re-clone even if the source tree already matches")
    args = parser.parse_args()

    here = Path(__file__).resolve()
    repo_root = Path(args.repo_root).resolve() if args.repo_root else here.parents[2]
    manifest = json.loads((repo_root / "th095" / "upstream.json").read_text(encoding="utf-8"))
    url = manifest["source"]
    commit = manifest["commit"]

    dest = repo_root / "build-inputs" / "th095-src"
    git_dir = dest / ".git"
    if git_dir.is_dir() and not args.force:
        head = subprocess.run(
            ["git", "rev-parse", "HEAD"], cwd=dest, capture_output=True, text=True, check=True
        ).stdout.strip()
        if head == commit:
            print(f"[fetch_upstream] {dest} already at {commit}; nothing to do.")
            return
        print(f"[fetch_upstream] {dest} is at {head}, not {commit}; re-cloning (use --force to force).", file=sys.stderr)
        subprocess.run(["rm", "-rf", str(dest)], check=True)

    dest.parent.mkdir(parents=True, exist_ok=True)
    print(f"[fetch_upstream] cloning {url} at {commit} ...")
    run(["git", "init", "-q", str(dest)])
    run(["git", "-C", str(dest), "remote", "add", "origin", url])
    run(["git", "-C", str(dest), "fetch", "-q", "--depth", "1", "origin", commit])
    run(["git", "-C", str(dest), "checkout", "-q", "FETCH_HEAD"])
    head = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=dest, capture_output=True, text=True, check=True
    ).stdout.strip()
    if head != commit:
        raise SystemExit(f"[fetch_upstream] HEAD is {head}, expected {commit}")
    print(f"[fetch_upstream] ready: {dest} ({commit})")


if __name__ == "__main__":
    main()
