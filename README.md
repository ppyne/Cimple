<img src="logo_cimple_full.svg" height="150" title="Cimple" alt="Cimple" />

---

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

**Get Lemon and utf8proc:**
```sh
cd tools/
./fetch_lemon.sh
./fetch_utf8proc.sh
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
- [Language Specification](SPECIFICATION.md) — formal specification (in french).

## Language overview

```c
int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

void main(string[] args) {
    // Variables
    int x = 42;
    float pi = M_PI;
    string name = "Alice";
    bool ok = true;

    // Arrays
    int[] nums = [1, 2, 3];
    arrayPush(nums, 4);
    print(toString(count(nums)) + "\n");  // 4

    // Control flow
    for (int i = 0; i < 10; i++) {
        if (isEven(i)) continue;
        print(toString(i) + "\n");
    }

    print(toString(factorial(5)) + "\n");
    if (count(args) == 0) {
        print("Usage: pass a name as the first argument.\n");
        return;
    }
    print("Hello, " + args[0] + "!\n");
}
```

Prints :

```text
$ build/cimple run ./test.ci world
4
1
3
5
7
9
120
Hello, world!
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

BSD 3-Clause License

See [LICENSE](LICENSE).
