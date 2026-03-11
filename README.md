# Cimple

**Cimple** is a small, statically-typed, imperative language with C-like syntax.
This repository contains the reference implementation: a lexer, parser, semantic analyser, and AST interpreter written in C11.

## Features

- Statically typed: `int`, `float`, `bool`, `string`, `void`
- Dynamic homogeneous arrays: `int[]`, `float[]`, `bool[]`, `string[]`
- Opaque `ExecResult` type for external command execution
- Minimal standard library (I/O, strings, maths, file, time, environment)
- Predefined constants: `INT_MAX`, `M_PI`, `FLOAT_EPSILON`, …
- Portable: macOS, Linux, Windows, WebAssembly (Emscripten)

## Quick start

### Prerequisites

| Tool | Purpose |
|------|---------|
| `re2c` ≥ 3.0 | Lexer generation |
| `lemon` (SQLite) | Parser generation |
| CMake ≥ 3.15 | Build system |
| C11 compiler | GCC, Clang, MSVC |

**Get Lemon:**
```sh
cd tools/
./fetch_lemon.sh
```

**Install re2c** (macOS: `sudo port install re2c`, Ubuntu: `apt install re2c`).

### Build

```sh
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Run

```sh
./cimple run examples/hello.ci
./cimple run examples/fibonacci.ci 15
./cimple check myprogram.ci
```

## Documentation

- [Language Manual](MANUAL.md) — complete reference: types, operators, control flow, all 68 standard-library functions, scoping rules, error diagnostics, and worked examples.
- [Language Specification](SPECIFICATION.md) — formal specification.

## Language overview

```c
// Variables
int x = 42;
float pi = M_PI;
string name = "Alice";
bool ok = true;

// Arrays
int[] nums = [1, 2, 3];
arrayPush(nums, 4);
print(toString(count(nums)) + "\n");  // 4

// Functions
int factorial(int n) {
    if (n <= 1) { return 1; }
    return n * factorial(n - 1);
}

// Control flow
for (int i = 0; i < 10; i++) {
    if (isEven(i)) { continue; }
    print(toString(i) + "\n");
}

// Entry point
void main(string[] args) {
    print("Hello, " + args[0] + "!\n");
}
```

## Project structure

```
cimple/
├── CMakeLists.txt
├── toolchain-emscripten.cmake
├── src/
│   ├── main.c                   # CLI entry point
│   ├── common/                  # error.h/c, memory.h/c
│   ├── ast/                     # ast.h/c — AST node definitions
│   ├── lexer/                   # lexer.re (re2c input), lexer.h
│   ├── parser/                  # parser.y (Lemon grammar), parser_helper.h/c
│   ├── semantic/                # semantic.h/c — type checking
│   ├── runtime/                 # value.h/c, scope.h/c
│   └── interpreter/             # interpreter.h/c, builtins.h/c
├── tools/
│   ├── fetch_lemon.sh           # Download lemon.c + lempar.c
│   ├── lemon.c                  # (to be fetched — not committed)
│   └── lempar.c                 # (to be fetched — not committed)
├── examples/                    # Example .ci programs
└── tests/                       # Test suite
```

## Pipeline

```
Source (.ci)
    │
    ▼ re2c
Lexer → Tokens
    │
    ▼ Lemon
Parser → AST
    │
    ▼
Semantic analyser (type checking, scope)
    │
    ▼
AST Interpreter → Output
```

## WebAssembly

```sh
cd tools/ && ./fetch_lemon.sh
mkdir build-wasm && cd build-wasm
cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain-emscripten.cmake
emmake cmake --build .
node cimple.js run ../examples/hello.ci
```

## License

See [LICENSE](LICENSE).
