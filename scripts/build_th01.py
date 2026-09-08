#!/usr/bin/env python3
"""Build the experimental open th01 port for Linux (host/headless) and Switch.

    python3 scripts/build_th01.py --target native          # native unit tests
    python3 scripts/build_th01.py --target headless        # simulation runner
    python3 scripts/build_th01.py --target host            # renders frames + audio
    python3 scripts/build_th01.py --target switch          # th01.nro (devkitPro)

Nothing is downloaded and no game data is bundled: the Switch build reads an
optional open data package from sdmc:/switch/th01/ and otherwise plays the
built-in procedural content set.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import tarfile
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT = ROOT / "highly-responsive-prayers"
SRC = PORT / "th01"
DIST = ROOT / "dist" / "th01"

TITLE = "TH01 open port"
AUTHOR = "1warriorscats1-sys/-"
VERSION = "01.01"

CORE_SOURCES = ["hrp_game.c", "hrp_pkg.c", "hrp_demo_data.c", "hrp_render.c",
                "hrp_audio.c", "hrp_autoplay.c"]


class BuildError(RuntimeError):
    pass


def run(command: list[str], cwd: Path | None = None, env: dict | None = None) -> subprocess.CompletedProcess:
    printable = " ".join(command)
    print(f"  $ {printable}")
    result = subprocess.run(command, cwd=str(cwd or ROOT), env=env,
                            capture_output=True, text=True)
    if result.returncode != 0:
        sys.stdout.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise BuildError(f"command failed ({result.returncode}): {printable}")
    return result


def core_paths() -> list[Path]:
    paths = [SRC / name for name in CORE_SOURCES]
    missing = [str(path) for path in paths if not path.exists()]
    if missing:
        raise BuildError(f"missing engine sources: {', '.join(missing)}")
    return paths


# --------------------------------------------------------------- native test


def target_native(sanitize: bool) -> dict:
    tests = sorted((ROOT / "tests" / "native").glob("th01_*.c"))
    if not tests:
        raise BuildError("no native tests found in tests/native/")

    out_dir = DIST / "native"
    out_dir.mkdir(parents=True, exist_ok=True)
    common = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O1", "-g", f"-I{SRC}",
              "-D_POSIX_C_SOURCE=200809L"]
    if sanitize:
        common += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]

    results = []
    for test in tests:
        binary = out_dir / test.stem
        cmd = [os.environ.get("CC", "gcc"), *common,
               *[str(path) for path in core_paths()], str(test), "-o", str(binary), "-lm"]
        run(cmd)
        completed = run([str(binary)])
        print(completed.stdout.strip())
        results.append({"test": test.name, "status": "pass"})
    return {"target": "native", "sanitize": sanitize, "results": results}


# ------------------------------------------------------------------- host


def target_headless(frames: int) -> dict:
    out_dir = DIST / "host"
    out_dir.mkdir(parents=True, exist_ok=True)
    binary = out_dir / "hrp_headless"
    cmd = [os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
           f"-I{SRC}", "-D_POSIX_C_SOURCE=200809L", *[str(path) for path in core_paths()], str(SRC / "main_headless.c"),
           "-o", str(binary), "-lm"]
    run(cmd)

    summary = {}
    for scenario in ["determinism", "boot", "card", "boss", "death"]:
        completed = run([str(binary), "--scenario", scenario, "--frames", str(frames), "--json"])
        summary[scenario] = json.loads(completed.stdout.strip())
        if not summary[scenario].get("ok", True):
            raise BuildError(f"headless scenario {scenario} failed")
        print(f"  {scenario}: pass")
    return {"target": "headless", "frames": frames, "scenarios": summary}


def target_host(frames: int, every: int, preview: bool = True) -> dict:
    out_dir = DIST / "host"
    frames_dir = out_dir / "frames"
    if frames_dir.exists():
        shutil.rmtree(frames_dir)
    frames_dir.mkdir(parents=True, exist_ok=True)

    binary = out_dir / "hrp_host"
    cmd = [os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-O2",
           f"-I{SRC}", "-D_POSIX_C_SOURCE=200809L", *[str(path) for path in core_paths()], str(SRC / "main_host.c"),
           "-o", str(binary), "-lm"]
    run(cmd)

    wav = out_dir / "th01.wav"
    run([str(binary), "--frames", str(frames), "--every", str(every),
         "--idle-frames", "60", "--dump", str(frames_dir), "--wav", str(wav)])

    info = {"target": "host", "frames": frames, "every": every,
            "frames_dumped": len(list(frames_dir.glob("frame_*.ppm"))),
            "wav": str(wav.relative_to(ROOT)) if wav.exists() else None}

    if preview and info["frames_dumped"]:
        gif = out_dir / "th01-preview.gif"
        run([sys.executable, str(ROOT / "scripts" / "render_th01_preview.py"),
             "--frames", str(frames_dir), "--gif", str(gif), "--step", "3", "--delay", "10"])
        info["preview"] = str(gif.relative_to(ROOT))

    (out_dir / "BUILD.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    return info


# ----------------------------------------------------------------- switch


def switch_toolchain() -> tuple[str, Path]:
    devkitpro = os.environ.get("DEVKITPRO")
    if not devkitpro:
        raise BuildError("DEVKITPRO is not set: use the devkitpro/devkita64 container")
    prefix = Path(devkitpro) / "devkitA64" / "bin" / "aarch64-none-elf-"
    if not (Path(f"{prefix}gcc").exists() or shutil.which(f"{prefix}gcc")):
        raise BuildError(f"devkitA64 compiler not found at {prefix}gcc")
    return str(prefix), Path(devkitpro)


def target_switch() -> dict:
    prefix, devkitpro = switch_toolchain()
    out_dir = DIST / "switch"
    build_dir = DIST / "switch" / "obj"
    if build_dir.exists():
        shutil.rmtree(build_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    build_dir.mkdir(parents=True, exist_ok=True)

    gcc = f"{prefix}gcc"
    arch = "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec"
    cflags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-O2", "-ffunction-sections",
              "-fdata-sections", "-D__SWITCH__", "-D_POSIX_C_SOURCE=200809L", f"-I{devkitpro / 'libnx' / 'include'}",
              *arch.split()]
    ldflags = [f"-specs={devkitpro / 'libnx' / 'switch.specs'}", *arch.split(),
               f"-L{devkitpro / 'libnx' / 'lib'}", "-lnx", "-Wl,--gc-sections"]

    objects = []
    for source in [*core_paths(), SRC / "main_switch.c"]:
        obj = build_dir / (source.stem + ".o")
        run([gcc, *cflags, "-c", str(source), "-o", str(obj)])
        objects.append(str(obj))

    elf = out_dir / "th01.elf"
    run([gcc, *objects, *ldflags, f"-Wl,-Map,{out_dir / 'th01.map'}", "-o", str(elf)])

    nacp = out_dir / "th01.nacp"
    run(["nacptool", "--create", TITLE, AUTHOR, VERSION, str(nacp)])

    icon = out_dir / "th01-icon.jpg"
    if not icon.exists():
        run([sys.executable, str(ROOT / "scripts" / "create_th01_icon.py"), str(icon)])

    nro = out_dir / "th01.nro"
    run(["elf2nro", str(elf), str(nro), f"--icon={icon}", f"--nacp={nacp}"])

    completed = run([sys.executable, str(ROOT / "scripts" / "verify_th01_nro.py"), str(nro), "--json"])
    nacp_info = json.loads(completed.stdout.strip())
    print("  verified:", nacp_info)

    install = PORT / "TH01_INSTALL.txt"
    zip_path = out_dir / "TH01-experimental-switch.zip"
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(nro, "switch/th01/th01.nro")
        if install.exists():
            archive.write(install, "TH01_INSTALL.txt")
        archive.writestr("README.txt",
                         "Experimental open th01 homebrew. No game data is included:\n"
                         "drop your own package at sdmc:/switch/th01/th01open.dat or play the\n"
                         "built-in procedural set. See TH01_INSTALL.txt.\n")
    print(f"  packaged {zip_path.relative_to(ROOT)}")

    source_tar = package_sources(out_dir)
    symbols = out_dir / "th01-symbols.zip"
    with zipfile.ZipFile(symbols, "w", zipfile.ZIP_DEFLATED) as archive:
        archive.write(elf, "th01.elf")
        archive.write(out_dir / "th01.map", "th01.map")

    info = {
        "target": "switch",
        "title": TITLE,
        "author": AUTHOR,
        "version": VERSION,
        "nro": str(nro.relative_to(ROOT)),
        "nro_bytes": nro.stat().st_size,
        "icon_bytes": icon.stat().st_size if icon.exists() else 0,
        "zip": str(zip_path.relative_to(ROOT)),
        "source_archive": str(source_tar.relative_to(ROOT)),
        "symbols": str(symbols.relative_to(ROOT)),
        "verified": nacp_info,
        "built_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    (out_dir / "BUILD.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")
    return info


def package_sources(out_dir: Path) -> Path:
    """Ship the exact sources that produced the NRO (MIT-licensed additions)."""
    tar_path = out_dir / "th01-source.tar.gz"
    members = [SRC, PORT / "TH01_INSTALL.txt", PORT / "README.md"]
    members += [ROOT / "scripts" / name for name in
                ("build_th01.py", "create_th01_icon.py", "verify_th01_nro.py",
                 "pack_th01_data.py", "test_th01_native.py", "gen_th01_font.py",
                 "render_th01_preview.py")]
    members.append(ROOT / "tests" / "test_th01.py")
    members += sorted((ROOT / "tests" / "native").glob("th01_*.c"))
    members += sorted((ROOT / "docs").glob("TH01_*.md"))
    members.append(ROOT / ".github" / "workflows" / "th01-switch.yml")

    with tarfile.open(tar_path, "w:gz") as archive:
        for member in members:
            if member.exists():
                archive.add(str(member), arcname=str(member.relative_to(ROOT)))
    return tar_path


# -------------------------------------------------------------------- main


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--target", choices=["native", "headless", "host", "switch"],
                        required=True)
    parser.add_argument("--frames", type=int, default=1200)
    parser.add_argument("--every", type=int, default=5)
    parser.add_argument("--sanitize", action="store_true",
                        help="build the native tests with ASan+UBSan")
    parser.add_argument("--no-preview", action="store_true")
    args = parser.parse_args()

    if args.target == "native":
        info = target_native(args.sanitize or bool(os.environ.get("TH01_SANITIZE")))
    elif args.target == "headless":
        info = target_headless(args.frames)
    elif args.target == "host":
        info = target_host(args.frames, args.every, preview=not args.no_preview)
    else:
        info = target_switch()

    print(json.dumps({key: value for key, value in info.items() if key != "results"}, indent=2)
          if args.target != "native" else "native: all tests passed")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BuildError as error:
        print(f"build error: {error}", file=sys.stderr)
        sys.exit(1)
