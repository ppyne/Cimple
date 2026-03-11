#!/bin/sh
# Fetch lemon.c and lempar.c from the SQLite source tree (public domain).
# Run this script once from the tools/ directory before building.
set -e

BASE_URL="https://www.sqlite.org/src/raw"

echo "Downloading lemon.c ..."
curl -fsSL "${BASE_URL}/tool/lemon.c?name=lemon.c" -o lemon.c

echo "Downloading lempar.c ..."
curl -fsSL "${BASE_URL}/tool/lempar.c?name=lempar.c" -o lempar.c

echo "Done. lemon.c and lempar.c are now in tools/."
