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
export PATH="$DEVKITA64/bin:$PATH"

set +e
trap 'echo "[diag] shell error near line $LINENO running: $BASH_COMMAND"' ERR

echo "### toolchain sanity"
cmake --version | head -1
ninja --version
aarch64-none-elf-gcc --version | head -1
aarch64-none-elf-nxlink --version 2>&1 | head -1 || aarch64-none-elf-nxlink 2>&1 | head -1
ls "$DEVKITA64/cmake/Switch.cmake"
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
    -DCMAKE_TOOLCHAIN_FILE="$DEVKITA64/cmake/Switch.cmake" \
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
ls -la th095/build-switch/th095.nro
aarch64-none-elf-nx-strings th095/build-switch/th095.nro 2>/dev/null | head -5 || true
exit 0
