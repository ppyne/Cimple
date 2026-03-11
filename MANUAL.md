# Cimple Manual

Cimple is a small statically typed language with C-like syntax.

This manual describes the current reference implementation in this repository.

## 1. Build And Run

Requirements:

- `re2c`
- `cmake`
- a C11 compiler
- Lemon parser files fetched with `./tools/fetch_lemon.sh`

Build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Run a program:

```sh
./build/cimple run examples/hello.ci
```

Type-check without running:

```sh
./build/cimple check examples/hello.ci
```

## 2. Types

Built-in scalar types:

- `int`
- `float`
- `bool`
- `string`
- `void`

Array types:

- `int[]`
- `float[]`
- `bool[]`
- `string[]`

Opaque type:

- `ExecResult`

## 3. Entry Points

Valid entry points:

- `int main()`
- `void main()`
- `int main(string[] args)`
- `void main(string[] args)`

Example:

```ci
void main() {
    print("Hello, world!\n");
}
```

Covered by `/Users/avialle/dev/Cimple/tests/manual/readme_hello/input.ci`.

## 4. Variables And Control Flow

Example with `while`:

```ci
void main() {
    int i = 0;
    while (i < 3) {
        print(toString(i) + "\n");
        i++;
    }
}
```

Covered by `/Users/avialle/dev/Cimple/tests/manual/spec_while_basic/input.ci`.

Example with `for`, `break`, and `continue`:

```ci
void main() {
    for (int i = 0; i < 6; i++) {
        if (i == 1) { continue; }
        if (i == 4) { break; }
        print(toString(i) + "\n");
    }
}
```

Covered by `/Users/avialle/dev/Cimple/tests/manual/spec_loop_break_continue/input.ci`.

## 5. Functions And Bool

```ci
bool isAdult(int age) {
    return age >= 18;
}

void main() {
    print(toString(isAdult(20)) + "\n");
}
```

Covered by `/Users/avialle/dev/Cimple/tests/manual/spec_bool_function/input.ci`.

Bool conversions:

```ci
void main() {
    print(toString(toBool("true")) + "\n");
    print(toString(toBool("false")) + "\n");
}
```

Covered by `/Users/avialle/dev/Cimple/tests/manual/spec_bool_conversions/input.ci`.

## 6. Strings

Strings are UTF-8 byte strings. Concatenation uses `+`.

```ci
void main() {
    string s = "Hello";
    print(s + ", world!\n");
}
```

Covered by `/Users/avialle/dev/Cimple/tests/manual/spec_string_helpers/input.ci`.

Useful functions:

- `len`
- `glyphLen`
- `glyphAt`
- `byteAt`
- `substr`
- `indexOf`
- `contains`
- `startsWith`
- `endsWith`
- `replace`
- `format`
- `join`
- `split`
- `concat`

Examples:

```ci
void main() {
    string[] parts = split("a,b,c", ",");
    print(join(parts, "-") + "\n");
    print(replace("banana", "na", "X") + "\n");
    print(format("Name: {}", ["Alice"]) + "\n");
}
```

Covered by:

- `/Users/avialle/dev/Cimple/tests/manual/spec_join_split/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_replace_multiple_hits/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_format_single_marker/input.ci`

Unicode example:

```ci
void main() {
    string s = "hé!";
    print(toString(glyphLen(s)) + "\n");
    print(glyphAt(s, 1) + "\n");
}
```

Covered by:

- `/Users/avialle/dev/Cimple/tests/manual/spec_glyph_basic/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_glyph_at_utf8_middle/input.ci`

## 7. Numbers And Math

Float helpers:

- `abs`
- `min`
- `max`
- `clamp`
- `floor`
- `ceil`
- `round`
- `trunc`
- `fmod`
- `sqrt`
- `pow`
- `approxEqual`
- `sin`
- `cos`
- `tan`
- `asin`
- `acos`
- `atan`
- `atan2`
- `exp`
- `log`
- `log2`
- `log10`

Integer helpers:

- `absInt`
- `minInt`
- `maxInt`
- `clampInt`
- `isEven`
- `isOdd`
- `safeDivInt`
- `safeModInt`

Examples are covered by:

- `/Users/avialle/dev/Cimple/tests/manual/spec_math_conversions/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_float_trig_log/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_safe_int_math/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_bitwise_ops/input.ci`

## 8. Arrays

Arrays are homogeneous and mutable.

```ci
void main() {
    int[] xs = [1, 2, 3];
    arrayPush(xs, 4);
    arrayInsert(xs, 1, 9);
    arrayRemove(xs, 0);
    print(toString(count(xs)) + "\n");
}
```

Array helpers:

- `count`
- `arrayPush`
- `arrayPop`
- `arrayInsert`
- `arrayRemove`
- `arrayGet`
- `arraySet`

Covered by:

- `/Users/avialle/dev/Cimple/tests/manual/spec_array_mutators/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_array_ref/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_array_function_mutation_bool/input.ci`

## 9. Files

File helpers:

- `readFile`
- `writeFile`
- `appendFile`
- `fileExists`
- `readLines`
- `writeLines`

Example:

```ci
void main() {
    writeFile("hello.txt", "Bonjour\n");
    appendFile("hello.txt", "Cimple\n");
    print(readFile("hello.txt"));
}
```

Covered by:

- `/Users/avialle/dev/Cimple/tests/manual/spec_files_roundtrip/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_append_exists/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_write_read_lines_roundtrip/input.ci`

## 10. External Commands

`exec` runs an external command and returns an `ExecResult`.

Accessors:

- `execStatus`
- `execStdout`
- `execStderr`

Example:

```ci
void main() {
    ExecResult r = exec(["git", "status", "--short"], []);
    print(toString(execStatus(r)) + "\n");
}
```

Covered by:

- `/Users/avialle/dev/Cimple/tests/manual/spec_exec_status_check/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_exec_capture_processing/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_exec_env_basic/input.ci`

In WebAssembly builds, `exec` is not supported and dedicated wasm tests cover that behaviour.

## 11. Environment Variables

Use `getEnv(name, fallback)`.

```ci
void main() {
    print(getEnv("USER", "unknown") + "\n");
}
```

Covered by:

- `/Users/avialle/dev/Cimple/tests/manual/spec_getenv_fallback/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_getenv_default_missing/input.ci`

## 12. Time And Date

Time functions operate in UTC.

Available functions:

- `now`
- `epochToYear`
- `epochToMonth`
- `epochToDay`
- `epochToHour`
- `epochToMinute`
- `epochToSecond`
- `makeEpoch`
- `formatDate`

`formatDate` tokens supported by the current implementation:

- `YYYY`
- `MM`
- `DD`
- `HH`
- `mm`
- `ss`

Example:

```ci
void main() {
    int ts = makeEpoch(2025, 3, 11, 14, 32, 7);
    print(formatDate(ts, "YYYY-MM-DD") + "\n");
    print(formatDate(ts, "HH:mm:ss") + "\n");
}
```

Covered by:

- `/Users/avialle/dev/Cimple/tests/manual/spec_time_format/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_date_format_tokens/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_epoch_parts/input.ci`

## 13. Predefined Constants

Examples of predefined constants:

- `INT_MIN`
- `INT_MAX`
- `INT_SIZE`
- `FLOAT_EPSILON`
- `M_PI`
- `M_LN2`

Covered by:

- `/Users/avialle/dev/Cimple/tests/manual/spec_constants_basic/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_int_bounds_constants/input.ci`
- `/Users/avialle/dev/Cimple/tests/manual/spec_float_size_constants/input.ci`

## 14. Diagnostics

The implementation reports:

- lexical errors
- syntax errors
- semantic errors
- runtime errors

Negative coverage lives in `/Users/avialle/dev/Cimple/tests/negative/`.

## 15. Tested Examples Index

The manual examples in this file are intentionally tied to existing tests under `/Users/avialle/dev/Cimple/tests/manual/`.

This manual is an initial project manual. It now exists at the required repository root and is aligned with the current implementation, but it still needs expansion if you want full narrative coverage of the whole specification.
