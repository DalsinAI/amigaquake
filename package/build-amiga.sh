#!/bin/sh
# The release archive, for Cradle's stove_build (target package): the game
# built with amiga/Makefile.amigaos against acgame.library (DalsinAI/acgame
# at ACGAME_REF), then tools/make_package.py makes quake-VERSION-68040.lha
# with its .sha256 and .readme: the Quake drawer with its GlowIcons (icons/)
# and the acgame.library it was built against in Libs/, its icon, and beside
# them the Amiga Installer script Install-Quake and its icon
# (package/installer/, made by amicradle scripts/port_installer.py from
# package/installer.json). OUT also gets the drawer as it unpacks, as
# Quake.tar (for a lab), and package.json. The game data (id1/pak0.pak) is not in it: see
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
# ACGame is DalsinAI/acgame at ACGAME_REF (the acgame.library this game is
# built against and ships), unless ACGAME names a checkout.
ACGAME_REF=${ACGAME_REF:-a3ea99f2a6d7376c8060e695e5d9c2878dbedc76}
DEPS="$BUILD/deps"
mkdir -p "$DEPS"
if [ -z "${ACGAME:-}" ]; then
    git init -q "$DEPS/acgame"
    git -C "$DEPS/acgame" fetch -q --depth 1 https://github.com/DalsinAI/acgame "$ACGAME_REF"
    git -C "$DEPS/acgame" checkout -q FETCH_HEAD
    ACGAME="$DEPS/acgame"
fi
ACGAMELIB="$DEPS/acgame-out"
CC="$STOVE/bin/m68k-amigaos-gcc" AR="$STOVE/bin/m68k-amigaos-ar" NM="$STOVE/bin/m68k-amigaos-nm" \
    OUT="$ACGAMELIB" sh "$ACGAME/build-amiga.sh" > "$OUT/acgame-output.txt" 2>&1 || { tail -n 60 "$OUT/acgame-output.txt"; exit 1; }
ACGAME_COMMIT=$(git -C "$ACGAME" rev-parse HEAD)
cp "$ACGAMELIB/acgame.library" "$BUILD/acgame.library"
PATHS="STOVE=$STOVE"
for v in NDK ACGAME ACGAMELIB OPENGPU; do
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
python3 - "$BUILD" "$STOVE/bin/m68k-amigaos-gcc" "quake" "$ACGAME_COMMIT" <<'PY'
import hashlib, json, subprocess, sys
from pathlib import Path
build, cc, exe, acgame = Path(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
git = lambda *a: subprocess.run(["git", *a], capture_output=True, text=True).stdout.strip()
receipt = {
    "executable_sha256": hashlib.sha256((build / exe).read_bytes()).hexdigest(),
    "source_commit": git("rev-parse", "HEAD"),
    "compiler_release": subprocess.run([cc, "--version"], capture_output=True,
                                       text=True).stdout.splitlines()[0],
    "source_date_epoch": int(git("log", "-1", "--format=%ct") or 0),
    "acgame_commit": acgame,
    "acgame_library_sha256": hashlib.sha256((build / "acgame.library").read_bytes()).hexdigest(),
}
(build / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
PY
cp "$BUILD/receipt.json" "$OUT/receipt.json"
python3 "$ROOT/tools/make_package.py" --build "$BUILD" --version "$VERSION" --out "$OUT"
