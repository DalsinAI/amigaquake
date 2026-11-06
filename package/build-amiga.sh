#!/bin/sh
# The release archive, for Cradle's stove_build (target package): the game
# built with amiga/Makefile.amigaos, then tools/make_package.py makes
# quake-VERSION-68040.lha with its .sha256 and .readme, the Quake drawer with
# its GlowIcons (icons/), the drawer as it unpacks and as Quake.tar (for a
# lab), and package.json. The game data (id1/pak0.pak) is not in it: see
# package/data-ReadMe.txt.
# STOVE, NDK and the other paths below are the Makefile's; VERSION is
# package/VERSION. Nothing is published from here.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
OUT=${OUT:-"$ROOT/build-amiga-package"}
VERSION=$(cat "$HERE/VERSION")
mkdir -p "$OUT"
OUT=$(CDPATH= cd -- "$OUT" && pwd)
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/quake-package.XXXXXX")
trap 'rm -rf "$BUILD"' EXIT INT TERM
cd "$ROOT"

# NDK, ACGAME and OPENGPU are passed on when set; otherwise the Makefiles'
# own defaults are used (Cradle's stove_build sets only STOVE and OUT).
# A stove's compiler is in its prefix/.
if [ ! -x "$STOVE/bin/m68k-amigaos-gcc" ] && [ -x "$STOVE/prefix/bin/m68k-amigaos-gcc" ]; then
    STOVE="$STOVE/prefix"
fi
# Timing only: ACGame's backend linked in, from DalsinAI/acgame a3ea99f.
git init -q "$BUILD/acgame"
git -C "$BUILD/acgame" fetch -q --depth 1 https://github.com/DalsinAI/acgame a3ea99f2a6d7376c8060e695e5d9c2878dbedc76
git -C "$BUILD/acgame" checkout -q FETCH_HEAD
ACGAME="$BUILD/acgame"
echo "Cradle's guest checkout: $(git -C "$ROOT/../DalsinAI__amigachrome-guest" log -1 --format='%h %ci' 2>&1)" 
PATHS="STOVE=$STOVE"
for v in NDK ACGAME OPENGPU; do
    eval "val=\${$v:-}"
    if [ -n "$val" ]; then PATHS="$PATHS $v=$val"; fi
done
make -f amiga/Makefile.amigaos $PATHS -j4 > "$OUT/build-output.txt" 2>&1 || { tail -n 60 "$OUT/build-output.txt"; exit 1; }
cp build/amigaos/quake "$BUILD/quake"
cp build/amigaos/quake "$OUT/quake"
# OpenUpQuake: id's GLQuake on OpenGPU's MiniGL (OPENGPU is the OpenGPU kit).
make -f amiga/openup/Makefile.amigaos $PATHS -j4 >> "$OUT/build-output.txt" 2>&1 || { tail -n 60 "$OUT/build-output.txt"; exit 1; }
cp build/openup/openupquake "$BUILD/openupquake"

# What the ReadMe and the check name: the program's SHA-256, the commit, the
# compiler and the commit's date (the archive's dates, so the same inputs
# make the same bytes).
python3 - "$BUILD" "$STOVE/bin/m68k-amigaos-gcc" "quake" <<'PY'
import hashlib, json, subprocess, sys
from pathlib import Path
build, cc, exe = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
git = lambda *a: subprocess.run(["git", *a], capture_output=True, text=True).stdout.strip()
receipt = {
    "executable_sha256": hashlib.sha256((build / exe).read_bytes()).hexdigest(),
    "source_commit": git("rev-parse", "HEAD"),
    "compiler_release": subprocess.run([cc, "--version"], capture_output=True,
                                       text=True).stdout.splitlines()[0],
    "source_date_epoch": int(git("log", "-1", "--format=%ct") or 0),
}
(build / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
PY
cp "$BUILD/receipt.json" "$OUT/receipt.json"
python3 "$ROOT/tools/make_package.py" --build "$BUILD" --version "$VERSION" --out "$OUT"
