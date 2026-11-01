#!/usr/bin/env bash
set -euo pipefail

HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
make -C "$HERE" -f Makefile.first-pass -j"${JOBS:-2}"
make -C "$HERE" -f Makefile.first-pass verify
sha256sum "$HERE/build/quake-null"

