# tools/

This directory contains third-party build tools.

## Required files

Two files must be obtained before building:

| File | Description |
|------|-------------|
| `lemon.c` | The Lemon parser generator from SQLite (public domain) |
| `lempar.c` | The Lemon parser template (public domain) |

### Automatic download

```sh
cd tools/
./fetch_lemon.sh
```

### Manual download

Both files are from the SQLite source tree and are in the public domain:

- https://www.sqlite.org/src/file/tool/lemon.c
- https://www.sqlite.org/src/file/tool/lempar.c

## re2c

The lexer requires `re2c`. Install it via your package manager:

```sh
# macOS
brew install re2c

# Ubuntu / Debian
sudo apt install re2c

# Fedora
sudo dnf install re2c

# Arch
sudo pacman -S re2c

# Windows (via Scoop)
scoop install re2c
```

## Building

After obtaining the tools, build from the repository root:

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

The `cimple` executable will be in `build/`.
