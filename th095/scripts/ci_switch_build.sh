#!/usr/bin/env bash
# TH095 Switch NRO build, run INSIDE the devkitpro/devkita64 container with
# the repository mounted at /work.
#
# The container provides the full devkitA64 toolchain (aarch64-none-elf-gcc,
# nxlink, nx-headers, CMake toolchain) plus the static switch portlibs
# (SDL2/SDL2_ttf/SDL2_image, harfbuzz/freetype, png/jpeg/webp/avif,
# EGL/GLESv2/glapi/drm_nouveau, zlib/bzip2) under /opt/devkitpro/portlibs.
#
# Diagnostics on failure are printed to stdout and posted as check-run
# annotations (the CI job's logs are not otherwise reachable).

set -u
cd /work

export DEVKITPRO=/opt/devkitpro
export DEVKITA64=/opt/devkitpro/devkitA64
export DEVKITARM=/opt/devkitpro/devkitARM
# devkitPro layout (dkp-pacman): toolchain under $DEVKITA64, libnx under
# $DEVKITPRO/libnx, devkit tools under $DEVKITPRO/tools, CMake toolchains
# under $DEVKITPRO/cmake, portlibs under $DEVKITPRO/portlibs/switch.
export PATH="$DEVKITPRO/tools/bin:$DEVKITA64/bin:$DEVKITPRO/portlibs/switch/bin:$PATH"

set +e
trap 'echo "[diag] shell error near line $LINENO running: $BASH_COMMAND"' ERR

echo "### devkitpro layout"
find /opt/devkitpro -maxdepth 2 -type d 2>/dev/null | sort | head -40

echo "### toolchain sanity"
cmake --version | head -1
ninja --version
aarch64-none-elf-gcc --version | head -1
ls "$DEVKITPRO/cmake/Switch.cmake"
echo "--- portlibs (selected)"
ls /opt/devkitpro/portlibs/switch/lib/ 2>/dev/null \
    | grep -E 'SDL2|harfbuzz|freetype|libpng|libjpeg|libwebp|libavif|GLESv2|EGL|glapi|drm_nouveau|libz\.|libbz2' \
    | head -30

echo "### fetch upstream"
python3 th095/scripts/fetch_upstream.py
fetch_status=$?

echo "### apply port source patches"
python3 th095/scripts/patch_upstream.py build-inputs/th095-src
patch_status=$?

echo "### configure"
cmake -S th095 -B th095/build-switch -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
    -DTH095_UPSTREAM_SRC=/work/build-inputs/th095-src \
    2>&1 | tee /tmp/ci-cfg.log
cfg_status=${PIPESTATUS[0]}

echo "### build"
cmake --build th095/build-switch -j"$(nproc)" 2>&1 | tee /tmp/ci-build.log
build_status=${PIPESTATUS[0]}

if [ "$fetch_status" -ne 0 ] || [ "$patch_status" -ne 0 ] \
   || [ "$cfg_status" -ne 0 ] || [ "$build_status" -ne 0 ]; then
    {
        echo "### statuses fetch/patch/configure/build: $fetch_status/$patch_status/$cfg_status/$build_status"
        echo
        echo "### configure log (last 60 lines)"
        tail -60 /tmp/ci-cfg.log 2>/dev/null
        echo
        echo "### build log (last 100 lines)"
        tail -100 /tmp/ci-build.log 2>/dev/null
        echo
        echo "### CMakeFiles/CMakeError.log (last 30 lines)"
        tail -30 th095/build-switch/CMakeFiles/CMakeError.log 2>/dev/null || true
        echo
        echo "### undefined symbol sample (link stage)"
        grep -m 20 "undefined reference" /tmp/ci-build.log 2>/dev/null || echo "(none)"
    } > /tmp/ci-diag.txt 2>&1
    echo "----- TH095 SWITCH CI DIAGNOSTICS -----"
    cat /tmp/ci-diag.txt
    # Check-run annotations (readable via the GitHub API).
    python3 -c 't=open("/tmp/ci-diag.txt").read(); [print("::error file=th095/CMakeLists.txt::" + c.replace(chr(10), " \\n ")) for c in [t[i:i+900] for i in range(0, len(t), 900)][:8]]'
    exit 1
fi

echo "### artifact"
ls -la th095/build-switch/ > /tmp/ci-ls.txt 2>&1
cat /tmp/ci-ls.txt
python3 -c 't=open("/tmp/ci-ls.txt").read(); [print("::notice file=th095/scripts/ci_switch_build.sh::build-dir: " + c.replace(chr(10), " \\n ")) for c in [t[i:i+900] for i in range(0, len(t), 900)][:4]]'
aarch64-none-elf-nx-strings th095/build-switch/th095.nro 2>/dev/null | head -5 || true

# Find the linked ELF (nx_create_nro converts it into th095.nro; keep a
# deterministic copy for symbol resolution and the th095-elf artifact).
ELF_FILE=""
for f in th095/build-switch/*; do
    [ -f "$f" ] || continue
    case "$f" in *.nro|*.nacp|*.nhdr|*.symelf|*.txt|*.cmake|*.ninja) continue ;; esac
    if aarch64-none-elf-readelf -h "$f" >/dev/null 2>&1; then ELF_FILE="$f"; break; fi
done
echo "### discovered ELF: $ELF_FILE"
if [ -n "$ELF_FILE" ] && [ -f "$ELF_FILE" ]; then
    cp "$ELF_FILE" th095/build-switch/th095.symelf
    ELF_FILE=th095/build-switch/th095.symelf
else
    echo "::error file=th095/scripts/ci_switch_build.sh::no ELF found in th095/build-switch"
fi

# Crash symbol resolution: if th095/scripts/crash_offsets.txt exists,
# resolve every offset against the ELF (built with -g) and post the result
# as check-run annotations (the only readable CI output channel).
if [ -f th095/scripts/crash_offsets.txt ]; then
    {
        echo "### crash offset resolution (ELF: th095/build-switch/th095)"
        [ -f "$ELF_FILE" ] && echo "### ELF for addr2line: $ELF_FILE ($(stat -c%s "$ELF_FILE") B)"
        for off in $(grep -oE "0x[0-9a-fA-F]+" th095/scripts/crash_offsets.txt); do
            res=$(aarch64-none-elf-addr2line -f -C -e "$ELF_FILE" "$off" 2>&1 | tr '\n' ' | ')
            echo "$off => $res"
        done
    } > /tmp/ci-syms.txt 2>&1
    cat /tmp/ci-syms.txt
    python3 -c 't=open("/tmp/ci-syms.txt").read(); [print("::notice file=th095/scripts/crash_offsets.txt::" + c.replace(chr(10), " \\n ")) for c in [t[i:i+900] for i in range(0, len(t), 900)][:12]]'
fi
exit 0
