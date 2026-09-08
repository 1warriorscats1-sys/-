#!/usr/bin/env bash
# Generic Switch port builder.
#
#   TITLE_ID=... CONTROL_XML=... ICON=... OUTPUT=... scripts/build.sh [data.win]
#
# Defaults build the SANAE's Sylphid Breeze port; witchs-night-market/build.sh
# overrides the variables for the second port.
#
# Produces <OUTPUT>.nsp (runner only, no game data) and, when a data file is
# given, the converted <OUTPUT dir>/game.win.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${WORK:-$HOME/work}"
RUNTIME="${RUNTIME:-2024.14.3.260}"          # 2024.14.4.268 has broken saving
RTDIR="$REPO/build-inputs/runtime-$RUNTIME/bin"

TITLE_ID="${TITLE_ID:-010000000005a1e1}"
CONTROL_XML="${CONTROL_XML:-$REPO/scripts/control.xml}"
ICON="${ICON:-$REPO/build-inputs/steam_library.jpg}"
ICON_CROP="${ICON_CROP:-0,300,600,900}"
OUTPUT="${OUTPUT:-$REPO/SANAEs_Sylphid_Breeze.nsp}"
DATA_OUT="${DATA_OUT:-$REPO/game.win}"
STEAM_PATCH="${STEAM_PATCH:-1}"              # disarm steam_* calls in the data
DATA_IN="${1:-}"
KEYS="$WORK/keys.dat"                        # prod.keys, never committed

mkdir -p "$WORK"; cd "$WORK"

# ---------------------------------------------------------------- toolchain
[ -d hacPack ] || git clone --depth 1 https://github.com/DarkMatterCore/hacPack.git
[ -x hacPack/hacpack ] || (cd hacPack && cp -n config.mk.template config.mk && make -j"$(nproc)")
[ -x hacPack/hacPack-Tools/hacPackTools-NACP/hptnacp ] || \
  (cd hacPack/hacPack-Tools/hacPackTools-NACP && make)
if [ ! -x hactool/hactool ]; then
  [ -d hactool ] || git clone --depth 1 https://github.com/SciresM/hactool.git
  (cd hactool && printf 'CC = gcc\nCFLAGS = -O2 -std=gnu11 -fPIC -I ../hacPack/mbedtls/include -D_FILE_OFFSET_BITS=64\nLDFLAGS = -L ../hacPack/mbedtls/library -lmbedtls -lmbedx509 -lmbedcrypto\n' > config.mk && make -j"$(nproc)")
fi
python3 -c "import lz4.block, capstone, PIL" 2>/dev/null || \
  pip3 install --quiet --break-system-packages lz4 capstone pillow

# ------------------------------------------------------------------- title
BUILD="build-$TITLE_ID"
rm -rf "$BUILD" && mkdir -p "$BUILD"/nsp/{exefs,romfs,control,logo} "$BUILD"/nca
cp "$RTDIR"/* "$BUILD"/nsp/exefs/

# runner fixes: NULL audio-group path, fatal "deprecated" builtins, fatal remount
python3 "$REPO/scripts/patch_nso.py" "$RTDIR/main" "$BUILD/nsp/exefs/main"

python3 - "$BUILD" "$TITLE_ID" <<'PY'
import struct, sys
build, tid = sys.argv[1], int(sys.argv[2], 16)
p = "%s/nsp/exefs/main.npdm" % build
d = bytearray(open(p, "rb").read()); assert d[:4] == b"META"
aci0, _, acid, _ = struct.unpack_from("<IIII", d, 0x70)
struct.pack_into("<Q", d, aci0 + 0x10, tid)
struct.pack_into("<QQ", d, acid + 0x210, tid, tid)
open(p, "wb").write(bytes(d))
PY

python3 - "$BUILD" "$ICON" "$ICON_CROP" <<'PY'
import sys
from PIL import Image
build, icon, crop = sys.argv[1], sys.argv[2], tuple(int(x) for x in sys.argv[3].split(","))
Image.new("RGBA", (160, 40), (0, 0, 0, 0)).save("%s/nsp/logo/NintendoLogo.png" % build)
gif = Image.new("P", (160, 40)); gif.putpalette([0, 0, 0] + [0] * 765)
gif.save("%s/nsp/logo/StartupMovie.gif" % build, transparency=0)
im = Image.open(icon).convert("RGB").crop(crop).resize((256, 256), Image.LANCZOS)
for lang in ("AmericanEnglish", "Japanese", "TraditionalChinese", "SimplifiedChinese"):
    im.save("%s/nsp/control/icon_%s.dat" % (build, lang), "JPEG", quality=95, subsampling=0)
PY

printf '[LLVM-Switch]\nSDKDir=C:\\Nintendo\\NXSDK\\NintendoSDK\nUseNEX=0\nUseNPLN=0\nnMeta=C:\\Users\\ZeusNX\\Project\\options\\switch\\application.nmeta\n' \
  > "$BUILD/nsp/romfs/options.ini"
printf 'True' > "$BUILD/nsp/romfs/preselected_user"

hacPack/hacPack-Tools/hacPackTools-NACP/hptnacp -i "$CONTROL_XML" \
  -o "$BUILD/nsp/control/control.nacp" -a createnacp >/dev/null
truncate -s 16384 "$BUILD/nsp/control/control.nacp"   # hptnacp writes 4 bytes too many
# keep only the icons for languages the NACP actually declares
python3 - "$BUILD" "$CONTROL_XML" <<'PY'
import glob, os, re, sys
build, xml = sys.argv[1], sys.argv[2]
langs = set(re.findall(r"<SupportedLanguage>(\w+)</SupportedLanguage>", open(xml, encoding="utf-8").read()))
for f in glob.glob("%s/nsp/control/icon_*.dat" % build):
    if os.path.basename(f)[5:-4] not in langs:
        os.remove(f)
PY

HP="hacPack/hacpack --keyset $KEYS --titleid $TITLE_ID"
$HP --type nca --ncatype program --exefsdir "$BUILD/nsp/exefs" --romfsdir "$BUILD/nsp/romfs" \
    --logodir "$BUILD/nsp/logo" --outdir "$BUILD/nca" >/dev/null
$HP --type nca --ncatype control --romfsdir "$BUILD/nsp/control" --outdir "$BUILD/nca" >/dev/null
PROGRAM=$(ls -S "$BUILD"/nca/*.nca | head -1)
CONTROL=$(ls -S "$BUILD"/nca/*.nca | tail -1)
$HP --type nca --ncatype meta --titletype application \
    --programnca "$PROGRAM" --controlnca "$CONTROL" --outdir "$BUILD/nca" >/dev/null
$HP --type nsp --ncadir "$BUILD/nca" --outdir "$BUILD" >/dev/null
cp "$BUILD/$TITLE_ID.nsp" "$OUTPUT"

# --------------------------------------------------------------- game data
if [ -n "$DATA_IN" ] && [ -f "$DATA_IN" ]; then
  python3 "$REPO/scripts/inspect_data.py" "$DATA_IN"
  python3 "$REPO/scripts/convert_data.py" "$DATA_IN" "$BUILD/game.step1.win"
  if [ "$STEAM_PATCH" = "1" ]; then
    python3 "$REPO/scripts/patch_steam.py" "$BUILD/game.step1.win" "$BUILD/game.win" || \
      cp "$BUILD/game.step1.win" "$BUILD/game.win"
  else
    cp "$BUILD/game.step1.win" "$BUILD/game.win"
  fi
  python3 "$REPO/scripts/verify_data.py" "$BUILD/game.win"
  cp "$BUILD/game.win" "$DATA_OUT"
fi

echo
echo "runtime : $RUNTIME    title id: $TITLE_ID"
sha256sum "$OUTPUT" ${DATA_IN:+"$DATA_OUT"} 2>/dev/null || true
