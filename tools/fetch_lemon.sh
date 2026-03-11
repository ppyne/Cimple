#!/bin/sh
# Fetch lemon.c and lempar.c from the SQLite source tree (public domain).
# Run this script once from the tools/ directory before building.
set -e

BASE_URL="https://raw.githubusercontent.com/sqlite/sqlite/master/tool"

echo "Downloading lemon.c ..."
curl -fsSL "${BASE_URL}/lemon.c" -o lemon.c

echo "Downloading lempar.c ..."
curl -fsSL "${BASE_URL}/lempar.c" -o lempar.c

echo "Done. lemon.c and lempar.c are now in tools/."
