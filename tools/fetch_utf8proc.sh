#!/bin/sh
set -e
VER="v2.9.0"
BASE="https://raw.githubusercontent.com/JuliaStrings/utf8proc/$VER"
SCRIPT_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
echo "Downloading utf8proc.h ..."
curl -fsSL "$BASE/utf8proc.h" -o "$SCRIPT_DIR/utf8proc.h"
echo "Downloading utf8proc.c ..."
curl -fsSL "$BASE/utf8proc.c" -o "$SCRIPT_DIR/utf8proc.c"
echo "Downloading utf8proc_data.c ..."
curl -fsSL "$BASE/utf8proc_data.c" -o "$SCRIPT_DIR/utf8proc_data.c"
echo "Done. utf8proc $VER is now in tools/."
