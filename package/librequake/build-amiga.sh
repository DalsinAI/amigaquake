#!/bin/sh
# The LibreQuake archive: Quake 2.30 built as for the Quake drawer, with
# LibreQuake's free data (BSD-3-Clause, its QuakeC GPL-2) in place of id's.
# LIBREQUAKE_ZIP is lite.zip of LibreQuake's release v0.09-beta
# (https://github.com/lavenderdotpet/LibreQuake/releases); its SHA-256 is
# checked. tools/make_package.py then makes librequake-VERSION-68040.lha
# with its .sha256 and .readme, the LibreQuake drawer with its GlowIcons
# (package/librequake/icons/), the drawer as it unpacks and as
# LibreQuake.tar (for a lab), and package.json. The music (Ogg Vorbis) is
# left out: this Quake plays none. STOVE, NDK and ACGAME are the Makefile's;
# VERSION is package/librequake/VERSION. Nothing is published from here.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu

DATA_SHA256=428e736b2f01d953e09a08c60bee975bdc4a0ac2219e97fa095c8af41754da83

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$HERE/../.." && pwd)
OUT=${OUT:-"$ROOT/build-amiga-librequake"}
VERSION=$(cat "$HERE/VERSION")
: "${LIBREQUAKE_ZIP:?set LIBREQUAKE_ZIP to LibreQuake v0.09-beta lite.zip}"
mkdir -p "$OUT"
OUT=$(CDPATH= cd -- "$OUT" && pwd)
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/librequake-package.XXXXXX")
trap 'rm -rf "$BUILD"' EXIT INT TERM
cd "$ROOT"

if [ ! -x "$STOVE/bin/m68k-amigaos-gcc" ] && [ -x "$STOVE/prefix/bin/m68k-amigaos-gcc" ]; then
    STOVE="$STOVE/prefix"
fi
PATHS="STOVE=$STOVE"
for v in NDK ACGAME; do
    eval "val=\${$v:-}"
    if [ -n "$val" ]; then PATHS="$PATHS $v=$val"; fi
done
make -f amiga/Makefile.amigaos $PATHS -j4 > "$OUT/build-output.txt" 2>&1 || { tail -n 60 "$OUT/build-output.txt"; exit 1; }
cp build/amigaos/quake "$BUILD/quake"

# The data: id1's paks and settings, and LibreQuake's licences and credits
# in docs/ (one drawer, no drawers within, so each has its icon).
python3 - "$BUILD" "$LIBREQUAKE_ZIP" "$DATA_SHA256" "$STOVE/bin/m68k-amigaos-gcc" <<'PY'
import hashlib, io, json, subprocess, sys, tarfile, zipfile
from pathlib import Path
build, zpath, want, cc = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3], sys.argv[4]
blob = zpath.read_bytes()
got = hashlib.sha256(blob).hexdigest()
if got != want:
    raise SystemExit(f"{zpath}: SHA-256 {got}, not LibreQuake v0.09-beta lite.zip's {want}")
git = lambda *a: subprocess.run(["git", *a], capture_output=True, text=True).stdout.strip()
stamp = int(git("log", "-1", "--format=%ct") or 0)
z = zipfile.ZipFile(io.BytesIO(blob))
pick = {}
for n in z.namelist():
    if n.endswith("/"):
        continue
    rel = n.split("/", 1)[1]                      # "lite/id1/..." -> "id1/..."
    if rel.startswith("id1/docs/"):
        doc = rel[len("id1/docs/"):]
        if doc.startswith("branding/"):
            continue
        pick["docs/" + doc.replace("/", "-")] = n  # misc-docs/COPYING -> misc-docs-COPYING
    elif rel.startswith("id1/music/"):
        continue
    elif rel.count("/") == 1:
        pick[rel] = n
with tarfile.open(build / "data.tar", "w", format=tarfile.USTAR_FORMAT) as t:
    for name in sorted(pick):
        data = z.read(pick[name])
        info = tarfile.TarInfo(name)
        info.size, info.mtime, info.mode = len(data), stamp, 0o644
        t.addfile(info, io.BytesIO(data))
receipt = {
    "executable_sha256": hashlib.sha256((build / "quake").read_bytes()).hexdigest(),
    "data_sha256": got,
    "source_commit": git("rev-parse", "HEAD"),
    "compiler_release": subprocess.run([cc, "--version"], capture_output=True,
                                       text=True).stdout.splitlines()[0],
    "source_date_epoch": stamp,
}
(build / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
print("\n".join(sorted(pick)))
PY
cp "$BUILD/receipt.json" "$OUT/receipt.json"
python3 "$ROOT/tools/make_package.py" --build "$BUILD" --version "$VERSION" --out "$OUT" \
    --spec "$HERE/package.json"
