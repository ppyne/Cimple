<img src="logo_cimple_full.svg" height="150" title="Cimple" alt="Cimple" />

---

# Cimple — Language Manual

Cimple is a small, statically typed, imperative language with C-like syntax. It is designed
to be easy to learn, predictable to use, and free of the historical pitfalls of C.

This document is the authoritative user-facing manual for the current reference implementation.

---

## Table of Contents

1. [Build and Run](#1-build-and-run)
2. [Language Overview](#2-language-overview)
3. [Types](#3-types)
4. [Variables](#4-variables)
5. [Expressions and Operators](#5-expressions-and-operators)
6. [Control Flow](#6-control-flow)
7. [Functions](#7-functions)
8. [Standard Library](#8-standard-library)
9. [Predefined Constants](#9-predefined-constants)
10. [Scoping Rules](#10-scoping-rules)
11. [Keywords](#11-keywords)
12. [Diagnostics](#12-diagnostics)
13. [Worked Examples](#13-worked-examples)

---

## 1. Build and Run

### Requirements

| Tool | Minimum version | How to install |
|------|-----------------|----------------|
| `re2c` | 3.0 | `brew install re2c` / `apt install re2c` |
| `cmake` | 3.15 | bundled on most systems |
| C11 compiler | any | GCC, Clang, or MSVC |
| Lemon | from SQLite | `cd tools && ./fetch_lemon.sh` |

### Building

```sh
cd tools && ./fetch_lemon.sh && cd ..   # download lemon.c + lempar.c (once)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The resulting binary is `build/cimple`.

### Running a program

```sh
./build/cimple run hello.ci
```

### Type-checking without running

```sh
./build/cimple check hello.ci
```

This validates lexical, syntactic and semantic correctness but does not execute anything.

### Passing arguments

```sh
./build/cimple run myprog.ci arg1 arg2
```

Arguments after the filename are passed to the program as `args[0]`, `args[1]`, etc.
The program name itself is **not** included in `args`.

### WebAssembly

A WebAssembly build is available via the Emscripten toolchain:

```sh
cmake -S . -B build-wasm -DCMAKE_TOOLCHAIN_FILE=toolchain-emscripten.cmake
cmake --build build-wasm
```

The `exec` built-in is unavailable on WebAssembly and produces a runtime error if called.
`getEnv` always returns the fallback value on WebAssembly.

---

## 2. Language Overview

A Cimple program is a single source file. It must define a `main` function that acts as the
program entry point. Every other construct — type annotations, function signatures, variable
declarations — must be explicit. There is no type inference, no implicit conversion, no
preprocessor, and no pointers.

### Hello, world

```ci
void main() {
    print("Hello, world!\n");
}
```

### Reading a command-line argument

```ci
void main(string[] args) {
    if (count(args) == 0) {
        print("No argument given.\n");
        return;
    }
    print("Hello, " + args[0] + "!\n");
}
```

### Using an integer return code

```ci
int main(string[] args) {
    if (count(args) == 0) {
        print("usage: prog <name>\n");
        return 1;
    }
    print("Hi, " + args[0] + "\n");
    return 0;
}
```

---

## 3. Types

### 3.1 Scalar types

| Type | Description | Default value |
|------|-------------|---------------|
| `int` | Signed 64-bit integer (`int64_t`) | `0` |
| `float` | 64-bit IEEE 754 double | `0.0` |
| `bool` | Boolean: `true` or `false` | `false` |
| `string` | Immutable UTF-8 byte string | `""` |
| `byte` | Unsigned 8-bit integer (`0`–`255`) | `0` |
| `void` | No value; only valid as a function return type | — |

### 3.2 Array types

Arrays are ordered, dynamic, and homogeneous. Each element type has its own array type:

| Array type | Element type |
|------------|-------------|
| `int[]` | `int` |
| `float[]` | `float` |
| `bool[]` | `bool` |
| `string[]` | `string` |
| `byte[]` | `byte` |

Mixed-type arrays do not exist. There are no two-dimensional arrays.

### 3.3 Opaque type: `ExecResult`

`ExecResult` is the type returned by `exec()`. It is a first-class value that can be stored
in a variable or passed to a function, but its internals are opaque. It can only be inspected
via `execStatus`, `execStdout`, and `execStderr`.

There is no `ExecResult` literal. A variable of type `ExecResult` must be initialised by an
`exec()` call; declaring one without initialisation is a semantic error.

### 3.4 Type strictness

Cimple does not perform implicit conversions. The following is a **semantic error**:

```ci
string s = "age=" + 42;        // ERROR: cannot concatenate string and int
```

Use an explicit conversion instead:

```ci
string s = "age=" + toString(42);   // OK
```

The four conversion intrinsics (`toString`, `toInt`, `toFloat`, `toBool`) are the only
polymorphic functions in the language. Their overloads are resolved statically by the
semantic analyser based on the declared type of the argument.

`byte` is intentionally strict:

- integer literals in `[0, 255]` may be assigned directly to `byte`
- assigning an `int` variable or expression to `byte` is a semantic error
- use `intToByte()` when you want an explicit clamp

```ci
byte ok = 255;
byte alsoOk = intToByte(300);   // 255

int n = 10;
// byte bad = n;                // semantic error
byte good = intToByte(n);
```

---

## 4. Variables

### 4.1 Scalar declarations

```ci
int x;                 // x is 0 (default)
int y = 10;
float pi = 3.14159;
bool ok = true;
string name = "Alice";
```

All variables must have an explicit type. There is no `var` or `auto`.

Uninitialized variables receive default values: `0` for `int`, `0.0` for `float`,
`""` for `string`, `false` for `bool`.

### 4.2 Array declarations

```ci
int[] scores = [10, 20, 30];
float[] temps = [0.1, 0.2, 0.3];
bool[] flags = [true, false, true];
string[] names = ["Alice", "Bob", "Charlie"];

int[] empty;           // empty array (count == 0)
string[] words;        // empty array
string[] init = [];    // also empty; type inferred from declaration
```

An array declared without an initialiser is empty (length 0).

Arrays **cannot** be reassigned as a whole after declaration:

```ci
int[] xs = [1, 2, 3];
xs = [4, 5, 6];        // ERROR: array reassignment is forbidden
```

To modify an array, use the array mutation functions (`arrayPush`, `arraySet`, etc.).

### 4.3 Assignment

Assignment uses `=` and must be type-compatible:

```ci
x = 42;
name = "Bob";
scores[1] = 99;        // element assignment is a standalone statement
```

### 4.4 Increment and decrement

`++` and `--` are **postfix-only** and **standalone statements only**:

```ci
i++;    // equivalent to i = i + 1
i--;    // equivalent to i = i - 1
```

The prefix forms `++i` and `--i` are syntactically forbidden.
Using `i++` inside an expression (e.g. `x = i++`) is a semantic error.

---

## 5. Expressions and Operators

### 5.1 Integer literals

| Base | Prefix | Example | Value |
|------|--------|---------|-------|
| Decimal | — | `42` | 42 |
| Hexadecimal | `0x` / `0X` | `0x2A` | 42 |
| Binary | `0b` / `0B` | `0b101010` | 42 |
| Octal | leading `0` | `052` | 42 |

The literal `0` alone is decimal zero. The `-` sign is the unary minus operator, not part
of a literal.

### 5.2 Float literals

```ci
float f1 = 1.5;
float f2 = 1e-3;      // 0.001
float f3 = 2.5e4;     // 25000.0
float f4 = 1.;        // 1.0  (trailing point is valid)
```

A float literal must contain either a decimal point or an exponent (or both). The bare
integer `1` is always an `int`.

### 5.3 String literals

Strings are enclosed in double quotes. Supported escape sequences:

| Sequence | Character |
|----------|-----------|
| `\"` | double quote |
| `\\` | backslash |
| `\n` | newline (LF) |
| `\t` | horizontal tab |
| `\r` | carriage return |
| `\b` | backspace |
| `\f` | form feed |
| `\uXXXX` | Unicode code point (exactly 4 hex digits) |

```ci
string greeting = "Hello!\n";
string path     = "C:\\Users\\Alice";
string heart    = "\u2665";
string accented = "\u00E9l\u00E8ve";   // "élève"
```

A raw newline inside a string literal is a lexical error; use `\n` instead.

### 5.4 Boolean literals

```ci
bool t = true;
bool f = false;
```

### 5.5 Array literals

```ci
int[]    xs = [1, 2, 3];
string[] ys = ["a", "b"];
int[]    empty = [];
```

All elements of a literal must share the same type.

### 5.6 Operators

#### Arithmetic

| Operator | Types | Description |
|----------|-------|-------------|
| `+` | `int`, `float`, or `string + string` | addition / concatenation |
| `-` | `int`, `float` | subtraction |
| `*` | `int`, `float` | multiplication |
| `/` | `int`, `float` | division (integer division truncates toward zero) |
| `%` | `int` | remainder |

Division by zero is a runtime error.

#### Comparison

| Operator | Types |
|----------|-------|
| `==` `!=` | any matching type pair; strings are compared byte-for-byte |
| `<` `<=` `>` `>=` | `int`, `float`, `string` (lexicographic) |

`float` comparisons are exact. Use `approxEqual(a, b, epsilon)` for fuzzy equality.

#### Logical

| Operator | Description |
|----------|-------------|
| `&&` | logical AND (short-circuits) |
| `\|\|` | logical OR (short-circuits) |
| `!` | logical NOT |

#### Bitwise (integers only)

| Operator | Description |
|----------|-------------|
| `&` | bitwise AND |
| `\|` | bitwise OR |
| `^` | bitwise XOR |
| `~` | bitwise NOT (unary) |
| `<<` | left shift |
| `>>` | right shift |

```ci
print(toString(6 & 3)   + "\n");   // 2
print(toString(6 | 3)   + "\n");   // 7
print(toString(6 ^ 3)   + "\n");   // 5
print(toString(~0)      + "\n");   // -1
print(toString(1 << 3)  + "\n");   // 8
print(toString(20 >> 2) + "\n");   // 5
```

#### Unary

| Operator | Types | Description |
|----------|-------|-------------|
| `-` | `int`, `float` | numeric negation |
| `!` | `bool` | logical NOT |
| `~` | `int` | bitwise NOT |

### 5.7 Operator precedence

From highest to lowest:

| Level | Operators |
|-------|-----------|
| 1 | function call, indexing `[]`, parentheses |
| 2 | unary `!`, unary `-`, unary `~` |
| 3 | `*` `/` `%` |
| 4 | `+` `-` |
| 5 | `<<` `>>` |
| 6 | `<` `<=` `>` `>=` |
| 7 | `==` `!=` |
| 8 | `&` |
| 9 | `^` |
| 10 | `\|` |
| 11 | `&&` |
| 12 | `\|\|` |

All binary operators are left-associative. Assignment is a statement, not an expression.

```ci
print(toString(2 + 3 * 4)              + "\n");  // 14  (* before +)
print(toString((2 + 3) * 4)            + "\n");  // 20
print(toString(1 + 2 << 3)             + "\n");  // 24  (+ before <<)
print(toString(true || false && false)  + "\n");  // true (&& before ||)
```

### 5.8 Restrictions

- `i++` and `i--` may only appear as standalone statements, not inside expressions.
- There is no comma operator.
- Assignment is a statement and cannot appear inside an expression.
- Array element assignment `a[i] = v` is a standalone statement, not an expression.
- There is no `char` type; use `string` (byte indexing) or `byteAt`.
- Strings are immutable; `s[i] = ...` is a semantic error.

---

## 6. Control Flow

### 6.1 `if` / `else`

```ci
if (x > 0) {
    print("positive\n");
} else if (x < 0) {
    print("negative\n");
} else {
    print("zero\n");
}
```

The body of `if` or `else` may be a block `{ ... }` or a single statement without braces.
A dangling `else` binds to the nearest unmatched `if`.

```ci
if (ok) print("yes\n");
if (ok) x = 1; else x = 2;
```

### 6.2 `while`

```ci
int i = 0;
while (i < 5) {
    print(toString(i) + "\n");
    i++;
}
```

A single-statement body without braces is allowed:

```ci
while (x > 0) x--;
```

### 6.3 `for`

```ci
for (int i = 0; i < 10; i++) {
    print(toString(i) + " ");
}
print("\n");
```

Rules:
- The **init** part must declare a single `int` variable with an initialiser.
- The **condition** must be a `bool` expression.
- The **update** must be a simple assignment or `i++` / `i--`.
- All three parts are mandatory and separated by `;`.
- The loop variable is scoped to the loop body.

A single-statement body without braces is allowed:

```ci
for (int j = 0; j < 3; j++) print(toString(j) + "\n");
```

### 6.4 `break` and `continue`

```ci
for (int i = 0; i < 10; i++) {
    if (i == 2) { continue; }
    if (i == 6) { break; }
    print(toString(i) + "\n");
}
```

- `break` exits the innermost enclosing loop immediately.
- `continue` skips to the next iteration of the innermost enclosing loop.
- Using `break` or `continue` outside a loop is a semantic error.

### 6.5 `return`

```ci
int add(int a, int b) {
    return a + b;
}

void greet(string name) {
    if (name == "") { return; }
    print("Hello, " + name + "\n");
}
```

- A non-`void` function must `return` a value on **every** execution path; the semantic
  analyser checks this statically.
- A `void` function may use bare `return;` or simply fall off the end.
- `return expr;` inside a `void` function is a semantic error.

---

## 7. Functions

### 7.1 Declaration

```ci
int add(int a, int b) {
    return a + b;
}

string greet(string name) {
    return "Hello, " + name + "!";
}

void printAll(int[] xs) {
    for (int i = 0; i < count(xs); i++) {
        print(toString(xs[i]) + "\n");
    }
}
```

Every function declares its return type and the type and name of each parameter explicitly.

### 7.2 Rules

- **No overloading**: two user-defined functions may not share a name.
- No default parameters.
- No variadic functions.
- **Scalar parameters** are passed by value.
- **Array parameters** are passed by reference: modifications inside the function are visible
  in the caller.

```ci
void addToAll(int[] xs, int delta) {
    for (int i = 0; i < count(xs); i++) {
        xs[i] = xs[i] + delta;
    }
}

void main() {
    int[] values = [1, 2, 3];
    addToAll(values, 10);
    // values is now [11, 12, 13]
}
```

### 7.3 Recursion

```ci
int fibonacci(int n) {
    if (n <= 1) { return n; }
    return fibonacci(n - 1) + fibonacci(n - 2);
}
```

### 7.4 Entry point signatures

`main` must match exactly one of these four signatures:

```ci
int  main()
void main()
int  main(string[] args)
void main(string[] args)
```

Any other signature for `main` is a semantic error. When `args` is present, `args[0]` is
the first user-supplied argument (the binary name is excluded). When no argument is given,
`count(args) == 0`.

---

## 8. Standard Library

Built-in functions are ordinary names known to the runtime — they are not keywords.

---

### 8.1 I/O

#### `print`

```ci
void print(string value)
```

Writes `value` to standard output. Only accepts `string`; use `toString` for other types.

```ci
print("x = " + toString(42) + "\n");
```

#### `input`

```ci
string input()
```

Reads one line from standard input. The trailing newline is stripped. Returns `""` on EOF or
read error. Takes no arguments.

```ci
print("Enter your name: ");
string name = input();
print("Hello, " + name + "\n");
```

---

### 8.2 String functions

#### `len`

```ci
int len(string s)
```

Returns the number of **bytes** in `s`. For ASCII strings, byte count equals character count.
For multi-byte UTF-8 strings, `len(s) >= glyphLen(s)`.

#### `glyphLen`

```ci
int glyphLen(string s)
```

Returns the number of Unicode code points after NFC normalisation.

```ci
string s = "élève";
print(toString(len(s))      + "\n");  // 7  (bytes)
print(toString(glyphLen(s)) + "\n");  // 5  (code points after NFC)
```

#### `glyphAt`

```ci
string glyphAt(string s, int index)
```

Returns the Unicode code point at position `index` (zero-based NFC code-point index) as a
single-code-point `string`. Out-of-bounds access is a runtime error.

```ci
string s = "hé!";
print(glyphAt(s, 0) + "\n");  // h
print(glyphAt(s, 1) + "\n");  // é
print(glyphAt(s, 2) + "\n");  // !
```

#### `byteAt`

```ci
int byteAt(string s, int index)
```

Returns the byte value (0–255) at byte position `index`. Out-of-bounds access is a runtime error.

```ci
print(toString(byteAt("A", 0))  + "\n");  // 65
print(toString(byteAt("é", 0))  + "\n");  // 195  (0xC3, first byte of U+00E9)
print(toString(byteAt("é", 1))  + "\n");  // 169  (0xA9, second byte)
```

#### `s[i]` — byte indexing

`s[i]` returns the single byte at position `i` as a one-byte `string`. It is the string
equivalent of `byteAt`, with `string` rather than `int` as the return type.

```ci
string s = "hello";
for (int i = 0; i < len(s); i++) {
    print(s[i]);
}
print("\n");
```

Assignment through indexing (`s[i] = ...`) is forbidden; strings are immutable.

**String indexing summary:**

| Expression | Unit | NFC | Return type |
|------------|------|-----|-------------|
| `len(s)` | bytes | no | `int` |
| `s[i]` | bytes | no | `string` (1 byte) |
| `byteAt(s, i)` | bytes | no | `int` (0–255) |
| `glyphLen(s)` | code points | yes | `int` |
| `glyphAt(s, i)` | code points | yes | `string` |

#### `substr`

```ci
string substr(string s, int start, int length)
```

Returns at most `length` **bytes** starting at byte offset `start`. If the slice would
exceed the string length it is clamped silently.

```ci
print(substr("Hello, world", 7, 5) + "\n");  // world
```

#### `indexOf`

```ci
int indexOf(string s, string needle)
```

Returns the byte offset of the first occurrence of `needle` in `s`, or `-1` if not found.
An empty `needle` always returns `0`.

```ci
print(toString(indexOf("banana", "na")) + "\n");  // 2
print(toString(indexOf("banana", "xy")) + "\n");  // -1
```

#### `contains` / `startsWith` / `endsWith`

```ci
bool contains(string s, string needle)
bool startsWith(string s, string prefix)
bool endsWith(string s, string suffix)
```

All three are case-sensitive.

```ci
print(toString(contains("foobar", "oba"))    + "\n");  // true
print(toString(startsWith("foobar", "foo"))  + "\n");  // true
print(toString(endsWith("foobar", "bar"))    + "\n");  // true
```

#### `replace`

```ci
string replace(string s, string old, string new)
```

Replaces **all** occurrences of `old` with `new`. Returns `s` unchanged if `old` is absent.
An empty `old` is a runtime error.

```ci
print(replace("banana", "na", "X")          + "\n");  // baXX
print(replace("hello world", "world", "Cimple") + "\n");  // hello Cimple
print(replace("hello", "xyz", "abc")        + "\n");  // hello  (unchanged)
```

#### `format`

```ci
string format(string template, string[] args)
```

Substitutes `{}` markers in order with elements of `args`. The number of markers must equal
`count(args)` exactly; a mismatch is a runtime error. All arguments must be `string`; convert
with `toString` beforehand.

```ci
string msg = format("Name: {}, age: {}.", ["Alice", toString(30)]);
print(msg + "\n");  // Name: Alice, age: 30.
```

#### `join`

```ci
string join(string[] array, string separator)
```

Concatenates all elements of `array` with `separator` between consecutive elements.
Returns `""` for an empty array. A single-element array returns that element.

```ci
string[] parts = ["one", "two", "three"];
print(join(parts, ", ")  + "\n");  // one, two, three
print(join(parts, "")    + "\n");  // onetwothree
print(join([], "-")      + "\n");  // (empty)
```

#### `split`

```ci
string[] split(string value, string separator)
```

Splits `value` on each occurrence of `separator`. Consecutive separators produce empty
strings. An empty `separator` is a runtime error. An empty `value` returns `[""]`.

```ci
string[] parts = split("a,b,,c", ",");
// ["a", "b", "", "c"]

string[] words = split("hello world", " ");
// ["hello", "world"]
```

#### `concat`

```ci
string concat(string[] array)
```

Concatenates all elements with no separator. Equivalent to `join(array, "")`.

```ci
print(concat(["x", "=", "42"]) + "\n");  // x=42
```

---

### 8.3 Type conversion intrinsics

These four are the only polymorphic functions in Cimple; overload resolution is entirely
static.

#### `toString`

```ci
string toString(int value)
string toString(float value)
string toString(bool value)
```

```ci
print(toString(42)    + "\n");  // 42
print(toString(3.14)  + "\n");  // 3.14
print(toString(true)  + "\n");  // true
```

#### `toInt`

```ci
int toInt(float value)
int toInt(string value)
```

- `toInt(float)` truncates toward zero: `toInt(3.9)` → `3`, `toInt(-2.7)` → `-2`.
- `toInt(string)` returns `0` for unparseable input.

```ci
print(toString(toInt(3.9))    + "\n");  // 3
print(toString(toInt("-12"))  + "\n");  // -12
print(toString(toInt("bad"))  + "\n");  // 0
```

#### `toFloat`

```ci
float toFloat(int value)
float toFloat(string value)
```

`toFloat(string)` returns `NaN` for unparseable input.

```ci
print(toString(toFloat(5))      + "\n");  // 5
print(toString(toFloat("1.5"))  + "\n");  // 1.5
```

#### `toBool`

```ci
bool toBool(string value)
```

Accepts `"true"`, `"false"`, `"1"`, `"0"`. Returns `false` for unrecognised input.

```ci
print(toString(toBool("true"))  + "\n");  // true
print(toString(toBool("0"))     + "\n");  // false
```

---

### 8.4 String validation predicates

```ci
bool isIntString(string value)
bool isFloatString(string value)
bool isBoolString(string value)
```

Return `true` if `value` is a valid representation of the respective type. Useful for safe
parsing before calling `toInt` / `toFloat` / `toBool`.

```ci
if (isIntString(s)) {
    int n = toInt(s);
    // use n
}
```

---

### 8.5 Float predicates

```ci
bool isNaN(float value)
bool isInfinite(float value)
bool isFinite(float value)
bool isPositiveInfinity(float value)
bool isNegativeInfinity(float value)
```

```ci
float x = toFloat("not-a-number");
if (isNaN(x)) {
    print("parse failed\n");
}
```

---

### 8.6 Float math

All trigonometric functions take and return angles in **radians**.

| Function | Signature | Description |
|----------|-----------|-------------|
| `abs` | `float abs(float x)` | absolute value |
| `min` | `float min(float a, float b)` | minimum |
| `max` | `float max(float a, float b)` | maximum |
| `clamp` | `float clamp(float v, float lo, float hi)` | clamp to `[lo, hi]` |
| `floor` | `float floor(float x)` | round toward −∞ |
| `ceil` | `float ceil(float x)` | round toward +∞ |
| `round` | `float round(float x)` | round to nearest (half away from zero) |
| `trunc` | `float trunc(float x)` | truncate toward zero |
| `fmod` | `float fmod(float x, float y)` | floating-point remainder |
| `sqrt` | `float sqrt(float x)` | square root |
| `pow` | `float pow(float base, float exp)` | exponentiation |
| `approxEqual` | `bool approxEqual(float a, float b, float eps)` | `\|a−b\| < eps` |
| `exp` | `float exp(float x)` | eˣ |
| `log` | `float log(float x)` | natural logarithm |
| `log2` | `float log2(float x)` | base-2 logarithm |
| `log10` | `float log10(float x)` | base-10 logarithm |
| `sin` | `float sin(float x)` | sine |
| `cos` | `float cos(float x)` | cosine |
| `tan` | `float tan(float x)` | tangent |
| `asin` | `float asin(float x)` | arc sine |
| `acos` | `float acos(float x)` | arc cosine |
| `atan` | `float atan(float x)` | arc tangent |
| `atan2` | `float atan2(float y, float x)` | two-argument arc tangent |

```ci
void main() {
    float angle = M_PI / 4.0;
    print(toString(sin(angle))      + "\n");  // ~0.7071
    print(toString(sqrt(2.0))       + "\n");  // ~1.4142
    print(toString(pow(2.0, 10.0))  + "\n");  // 1024
    print(toString(log(M_E))        + "\n");  // 1
    print(toString(log2(8.0))       + "\n");  // 3
    print(toString(floor(-1.3))     + "\n");  // -2
    print(toString(ceil(-1.3))      + "\n");  // -1
}
```

---

### 8.7 Integer math

| Function | Signature | Description |
|----------|-----------|-------------|
| `absInt` | `int absInt(int x)` | absolute value |
| `minInt` | `int minInt(int a, int b)` | minimum |
| `maxInt` | `int maxInt(int a, int b)` | maximum |
| `clampInt` | `int clampInt(int v, int lo, int hi)` | clamp to `[lo, hi]` |
| `isEven` | `bool isEven(int value)` | true if even |
| `isOdd` | `bool isOdd(int value)` | true if odd |
| `safeDivInt` | `int safeDivInt(int a, int b, int fallback)` | `a/b`, or `fallback` when `b==0` |
| `safeModInt` | `int safeModInt(int a, int b, int fallback)` | `a%b`, or `fallback` when `b==0` |

```ci
print(toString(absInt(-7))           + "\n");  // 7
print(toString(clampInt(15, 0, 10))  + "\n");  // 10
print(toString(safeDivInt(10, 0, -1))+ "\n");  // -1
print(toString(isEven(4))            + "\n");  // true
print(toString(isOdd(7))             + "\n");  // true
```

---

### 8.8 Array functions

These polymorphic intrinsics work with any element type (`T` stands for `int`, `float`,
`bool`, or `string`):

| Function | Signature | Description |
|----------|-----------|-------------|
| `count` | `int count(T[] array)` | number of elements |
| `arrayPush` | `void arrayPush(T[] array, T value)` | append element |
| `arrayPop` | `T arrayPop(T[] array)` | remove and return last element |
| `arrayInsert` | `void arrayInsert(T[] array, int index, T value)` | insert before `index` |
| `arrayRemove` | `void arrayRemove(T[] array, int index)` | remove element at `index` |
| `arrayGet` | `T arrayGet(T[] array, int index)` | read element (same as `a[i]`) |
| `arraySet` | `void arraySet(T[] array, int index, T value)` | write element (same as `a[i] = v`) |

`a[i]` and `arrayGet(a, i)` are interchangeable for reads.
`a[i] = v` and `arraySet(a, i, v)` are interchangeable for writes.

```ci
void main() {
    int[] xs = [1, 2, 3];
    arrayPush(xs, 4);           // [1, 2, 3, 4]
    arrayInsert(xs, 1, 9);      // [1, 9, 2, 3, 4]
    arrayRemove(xs, 0);         // [9, 2, 3, 4]
    int last = arrayPop(xs);    // last = 4, xs = [9, 2, 3]
    print(toString(count(xs)) + "\n");   // 3
}
```

Out-of-bounds access at runtime produces an error message showing the index requested, the
current length, and the source location.

---

### 8.9 File I/O

```ci
string   readFile(string path)
void     writeFile(string path, string content)
void     appendFile(string path, string content)
bool     fileExists(string path)
string[] readLines(string path)
void     writeLines(string path, string[] lines)
```

- `readFile` returns the entire file as a `string`. The file must exist; otherwise a runtime
  error is raised.
- `writeFile` creates or truncates the file.
- `appendFile` opens in append mode, creating the file if necessary.
- `fileExists` returns `true` if `path` names an existing regular file.
- `readLines` returns each line as a `string[]` element (newlines stripped).
- `writeLines` writes each element followed by `\n`.

```ci
void main() {
    writeFile("note.txt", "First line\n");
    appendFile("note.txt", "Second line\n");
    print(readFile("note.txt"));

    string[] lines = readLines("note.txt");
    print(toString(count(lines)) + " lines\n");   // 2 lines
}
```

---

### 8.10 External commands

```ci
ExecResult exec(string[] command, string[] env)
int        execStatus(ExecResult result)
string     execStdout(ExecResult result)
string     execStderr(ExecResult result)
```

`exec` runs an external process **synchronously**. stdout and stderr are captured separately.
The child process inherits the current environment; `env` supplies additional or overriding
variables in `"NAME=VALUE"` format.

A non-zero exit code is a valid result, not a Cimple runtime error. A runtime error is raised
only if the executable cannot be found or launched.

```ci
void main() {
    ExecResult r = exec(["git", "log", "--oneline", "-5"], []);
    if (execStatus(r) == 0) {
        string[] commits = split(execStdout(r), "\n");
        for (int i = 0; i < count(commits); i++) {
            if (len(commits[i]) > 0) {
                print(commits[i] + "\n");
            }
        }
    } else {
        print("git error: " + execStderr(r) + "\n");
    }
}
```

Passing environment variables to the child process:

```ci
ExecResult r = exec(["python3", "script.py"], ["DEBUG=1", "LANG=en_US.UTF-8"]);
```

`exec` is unavailable on WebAssembly and raises a runtime error if called.

---

### 8.11 Environment variables

```ci
string getEnv(string name, string fallback)
```

Returns the value of environment variable `name`, or `fallback` if it is not set.
A variable set to an empty string returns `""`, not the fallback — the variable exists,
it is just empty.

```ci
string home  = getEnv("HOME", "/tmp");
string port  = getEnv("PORT", "8080");
string debug = getEnv("DEBUG", "0");

if (debug == "1") {
    print("Debug mode on\n");
}
print("Listening on port " + port + "\n");
```

On WebAssembly, `getEnv` always returns `fallback`.

---

### 8.12 Time and date

All functions operate in **UTC**. Time is represented as **epoch milliseconds** (`int64_t`):
milliseconds elapsed since 1970-01-01 00:00:00 UTC.

```ci
int now()
int epochToYear(int epochMs)
int epochToMonth(int epochMs)        // 1 = January, 12 = December
int epochToDay(int epochMs)          // 1 – 31
int epochToHour(int epochMs)         // 0 – 23
int epochToMinute(int epochMs)       // 0 – 59
int epochToSecond(int epochMs)       // 0 – 59
int makeEpoch(int year, int month, int day, int hour, int minute, int second)
string formatDate(int epochMs, string fmt)
```

`formatDate` tokens:

| Token | Field | Example |
|-------|-------|---------|
| `YYYY` | 4-digit year | `2025` |
| `MM` | 2-digit month | `03` |
| `DD` | 2-digit day | `11` |
| `HH` | 2-digit hour | `14` |
| `mm` | 2-digit minute | `32` |
| `ss` | 2-digit second | `07` |
| `w` | weekday (`0` = Sunday, `6` = Saturday) | `2` |
| `yday` | day of year, base 0 | `69` |
| `WW` | ISO week number, zero-padded | `11` |
| `ISO` | full UTC ISO 8601 datetime | `2025-03-11T14:32:07Z` |

```ci
void main() {
    int ts = makeEpoch(2025, 3, 11, 14, 32, 7);
    print(formatDate(ts, "YYYY-MM-DD") + "\n");   // 2025-03-11
    print(formatDate(ts, "HH:mm:ss")   + "\n");   // 14:32:07
    print(formatDate(ts, "w")          + "\n");   // 2
    print(formatDate(ts, "yday")       + "\n");   // 69
    print(formatDate(ts, "WW")         + "\n");   // 11
    print(formatDate(ts, "ISO")        + "\n");   // 2025-03-11T14:32:07Z

    int t0 = now();
    // ... some work ...
    int t1 = now();
    print("Elapsed: " + toString(t1 - t0) + " ms\n");
}
```

To get the current time in seconds: `now() / 1000`.

---

### 8.13 File system helpers

```ci
string tempPath()
void remove(string path)
void chmod(string path, int mode)
string cwd()
void copy(string src, string dst)
void move(string src, string dst)
bool isReadable(string path)
bool isWritable(string path)
bool isExecutable(string path)
bool isDirectory(string path)
string dirname(string path)
string basename(string path)
string filename(string path)
string extension(string path)
```

Notes:

- `tempPath()` returns a unique path that does not currently exist; it does not create the file
- `remove()` removes files only; removing a directory is a runtime error
- `chmod()` is available on POSIX platforms and raises a runtime error on WebAssembly
- `copy()` and `move()` overwrite the destination if it already exists
- `dirname`, `basename`, `filename`, `extension`, and `cwd()` are pure path helpers

```ci
void main(string[] args) {
    string src = tempPath();
    string dst = tempPath();

    writeFile(src, "hello");
    copy(src, dst);
    print(readFile(dst) + "\n");

    move(dst, "moved.txt");
    print(toString(fileExists("moved.txt")) + "\n");
    print(dirname("/tmp/demo.txt") + "\n");     // /tmp
    print(filename("archive.tar.gz") + "\n");   // archive.tar

    remove(src);
    remove("moved.txt");
}
```

---

### 8.14 Binary I/O and `byte`

```ci
int byteToInt(byte b)
byte intToByte(int n)
byte[] stringToBytes(string s)
string bytesToString(byte[] data)
byte[] readFileBytes(string path)
void writeFileBytes(string path, byte[] data)
void appendFileBytes(string path, byte[] data)
byte[] intToBytes(int n)
byte[] floatToBytes(float f)
int bytesToInt(byte[] data)
float bytesToFloat(byte[] data)
```

Rules:

- `intToByte()` clamps silently to `[0, 255]`
- `byte + byte` and `byte + int` produce `int`
- `byte & byte`, `byte | byte`, `byte ^ byte`, and `~byte` produce `byte`
- `bytesToString()` replaces invalid UTF-8 byte sequences with `U+FFFD`
- `bytesToInt()` requires exactly `INT_SIZE` bytes
- `bytesToFloat()` requires exactly `FLOAT_SIZE` bytes

```ci
void main(string[] args) {
    byte[] raw = [0x48, 0x69];
    print(bytesToString(raw) + "\n");           // Hi

    byte mask = 0xF0;
    byte low  = 0x0F;
    byte both = mask | low;
    print(toString(byteToInt(both)) + "\n");    // 255

    byte[] dump = intToBytes(42);
    print(toString(bytesToInt(dump)) + "\n");   // 42
}
```

---

## 9. Imports

Use top-level imports to split a program across multiple files:

```ci
import "math/add.ci";
import "utils/strings.ci";
```

Rules:

- every `import` must appear before any declaration in the current file
- the path must end with `.ci`
- imports are resolved recursively before semantic analysis
- each imported file is processed once, even if multiple files import it
- imported files may not define `main`
- circular imports are semantic errors

```ci
// main.ci
import "math/add.ci";

void main(string[] args) {
    print(toString(addThree(4)) + "\n");
}
```

```ci
// math/add.ci
import "more.ci";

int addThree(int n) {
    return addOne(addOne(addOne(n)));
}
```

```ci
// math/more.ci
int addOne(int n) { return n + 1; }
```

---

## 10. Predefined Constants

These identifiers are reserved; they cannot be redeclared or assigned.

### Integer constants

| Constant | Description | Typical value (64-bit) |
|----------|-------------|------------------------|
| `INT_MAX` | largest `int` | 9 223 372 036 854 775 807 |
| `INT_MIN` | smallest `int` | −9 223 372 036 854 775 808 |
| `INT_SIZE` | size of `int` in bytes | 8 |
| `FLOAT_SIZE` | size of `float` in bytes | 8 |
| `FLOAT_DIG` | significant decimal digits in a `float` | 15 |

### Float constants

| Constant | Description | Typical value |
|----------|-------------|---------------|
| `FLOAT_EPSILON` | smallest `x` such that `1.0 + x != 1.0` | 2.220446e−16 |
| `FLOAT_MIN` | smallest positive normalised `float` | 2.225074e−308 |
| `FLOAT_MAX` | largest positive `float` | 1.797693e+308 |

### Mathematical constants

| Constant | Value | Description |
|----------|-------|-------------|
| `M_PI` | 3.141592653589793 | π |
| `M_E` | 2.718281828459045 | e |
| `M_TAU` | 6.283185307179586 | τ = 2π |
| `M_SQRT2` | 1.4142135623730951 | √2 |
| `M_LN2` | 0.6931471805599453 | ln 2 |
| `M_LN10` | 2.302585092994046 | ln 10 |

```ci
void main() {
    print("π = " + toString(M_PI) + "\n");

    // Circumference of a circle of radius 5 using τ
    float c = M_TAU * 5.0;
    print("circumference = " + toString(c) + "\n");

    // Approximate float equality
    if (approxEqual(0.1 + 0.2, 0.3, FLOAT_EPSILON * 10.0)) {
        print("approximately equal\n");
    }
}
```

---

## 11. Scoping Rules

Cimple uses **lexical (block) scoping**.

- A variable declared inside `{ }` is local to that block and invisible outside it.
- Inner blocks may shadow outer variables by declaring a new variable of the same name.
- A variable must be declared before first use.
- Function parameters are scoped to the function body.
- The `for` loop variable is scoped to the loop body.
- All function declarations are globally visible; a function may be called before its textual
  definition.

```ci
int x = 1;      // global

void demo() {
    int x = 2;      // local — shadows global
    {
        int x = 3;  // inner block — shadows local
        print(toString(x) + "\n");  // 3
    }
    print(toString(x) + "\n");      // 2
}

void main() {
    demo();
    print(toString(x) + "\n");      // 1
}
```

---

## 12. Keywords

The following identifiers are reserved and may not be used as variable or function names:

```
bool      break     continue  else      ExecResult
false     float     for       if        import
int       return    string    true      void
while     byte
```

The predefined constants are also reserved (see §9):

```
FLOAT_DIG  FLOAT_EPSILON  FLOAT_MAX  FLOAT_MIN  FLOAT_SIZE
INT_MAX    INT_MIN        INT_SIZE
M_E        M_LN10         M_LN2      M_PI       M_SQRT2  M_TAU
```

---

## 13. Diagnostics

The implementation reports four categories of error, each with a source location:

| Category | When raised | Example |
|----------|-------------|---------|
| **Lexical error** | during tokenisation | unknown escape `\q`, unterminated string |
| **Syntax error** | during parsing | missing `;`, unmatched `}` |
| **Semantic error** | during analysis | type mismatch, undeclared variable, missing `return` |
| **Runtime error** | during execution | division by zero, out-of-bounds index, file not found |

Semantic errors prevent execution entirely. Runtime errors abort the program immediately with
a descriptive message including the source location.

Common semantic errors to watch for:

- Using a variable before declaring it.
- Calling a function with wrong argument count or wrong types.
- Missing `return` on some path of a non-`void` function.
- `break` or `continue` outside a loop.
- Assigning to a string index (`s[i] = ...`).
- Reassigning an array variable as a whole (`xs = [...]` after its declaration).
- Declaring a variable whose name matches a reserved constant (`INT_MAX = 5` is forbidden).
- Calling `toInt(bool)` or any conversion with an unsupported source type.

---

## 14. Worked Examples

### 13.1 FizzBuzz

```ci
void main() {
    for (int i = 1; i <= 30; i++) {
        if (i % 15 == 0) {
            print("FizzBuzz\n");
        } else if (i % 3 == 0) {
            print("Fizz\n");
        } else if (i % 5 == 0) {
            print("Buzz\n");
        } else {
            print(toString(i) + "\n");
        }
    }
}
```

### 13.2 Recursive Fibonacci

```ci
int fibonacci(int n) {
    if (n <= 1) { return n; }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

void main(string[] args) {
    int n = 10;
    if (count(args) > 0) { n = toInt(args[0]); }
    print(toString(fibonacci(n)) + "\n");
}
```

### 13.3 Count words in a line

```ci
void main() {
    print("Enter a sentence: ");
    string line = input();
    string[] words = split(line, " ");
    print("Word count: " + toString(count(words)) + "\n");
}
```

### 13.4 Sum an array with a helper function

```ci
int sumArray(int[] xs) {
    int total = 0;
    for (int i = 0; i < count(xs); i++) {
        total = total + xs[i];
    }
    return total;
}

void main() {
    int[] data = [3, 1, 4, 1, 5, 9, 2, 6];
    print("Sum: " + toString(sumArray(data)) + "\n");   // 31
}
```

### 13.5 File processing

```ci
void main(string[] args) {
    if (count(args) < 1) {
        print("usage: prog <file>\n");
        return;
    }
    string path = args[0];
    if (!fileExists(path)) {
        print("File not found: " + path + "\n");
        return;
    }
    string[] lines = readLines(path);
    print("Lines: " + toString(count(lines)) + "\n");
    for (int i = 0; i < count(lines); i++) {
        print(toString(i + 1) + ": " + lines[i] + "\n");
    }
}
```

### 13.6 Running an external command

```ci
void main() {
    ExecResult r = exec(["git", "rev-parse", "--short", "HEAD"], []);
    if (execStatus(r) != 0) {
        print("Not a git repository\n");
        return;
    }
    string[] lines = split(execStdout(r), "\n");
    print("Current commit: " + lines[0] + "\n");
}
```

### 13.7 Formatted timestamp

```ci
void main() {
    int ts = now();
    string stamp = formatDate(ts, "YYYY-MM-DD HH:mm:ss");
    print("[" + stamp + " UTC] Program started\n");
}
```

### 13.8 Bitwise permission flags

```ci
int FLAG_READ    = 0x1;
int FLAG_WRITE   = 0x2;
int FLAG_EXECUTE = 0x4;

bool hasPermission(int flags, int perm) {
    return (flags & perm) != 0;
}

void main() {
    int perms = FLAG_READ | FLAG_WRITE;
    print(toString(hasPermission(perms, FLAG_READ))    + "\n");  // true
    print(toString(hasPermission(perms, FLAG_EXECUTE)) + "\n");  // false
}
```

### 13.9 CSV line parser

```ci
string[] parseCSV(string line) {
    string[] fields = split(line, ",");
    for (int i = 0; i < count(fields); i++) {
        // trim leading/trailing spaces (simple version)
        string f = fields[i];
        while (len(f) > 0 && f[0] == " ") {
            f = substr(f, 1, len(f) - 1);
        }
        fields[i] = f;
    }
    return fields;
}

void main() {
    string line = "Alice, 30, engineer";
    string[] cols = parseCSV(line);
    for (int i = 0; i < count(cols); i++) {
        print(cols[i] + "\n");
    }
}
```

---

*End of manual.*
