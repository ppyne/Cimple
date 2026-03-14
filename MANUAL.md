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
9. [Imports](#9-imports)
10. [Structures](#10-structures)
11. [Predefined Constants](#11-predefined-constants)
12. [Scoping Rules](#12-scoping-rules)
13. [Keywords](#13-keywords)
14. [Diagnostics](#14-diagnostics)
15. [Worked Examples](#15-worked-examples)

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

### Comments

`//` starts a line comment; everything from `//` to the end of the line is ignored:

```c
// This is a comment.
int x = 42;    // inline comment
```

There are no block comments.

### Hello, world

```c
void main() {
    print("Hello, world!\n");
}
```

### Reading a command-line argument

```c
void main(string[] args) {
    if (count(args) == 0) {
        print("No argument given.\n");
        return;
    }
    print("Hello, " + args[0] + "!\n");
}
```

### Using an integer return code

```c
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
| `StructureName[]` | instances of that structure type |

Mixed-type arrays do not exist. Two-dimensional (jagged) arrays are also available — see §3.2a below.

### 3.2a Two-dimensional arrays

Cimple supports **jagged 2D arrays** where each row is an independent 1D array. Rows may have different lengths.

**Type syntax:** append `[][]` to any scalar or struct/union type.

```c
int[][] matrix;                          // empty 2D array
int[][] grid = [[1,2,3],[4,5,6]];        // 2D literal

int[] row = [7,8,9];
arrayPush(grid, row);                    // add a row

int val = grid[1][2];                    // read: 6
grid[0][1] = 99;                         // write 2D element

print(toString(count(grid)) + "\n");     // number of rows
print(toString(count(grid[0])) + "\n"); // columns in row 0
```

**Nested iteration:**
```c
for (int i = 0; i < count(grid); i++) {
    for (int j = 0; j < count(grid[i]); j++) {
        print(toString(grid[i][j]) + " ");
    }
    print("\n");
}
```

All standard array functions (`arrayPush`, `arrayPop`, `arrayInsert`, `arrayRemove`,
`count`) work on the outer array. The element type of the outer array is the corresponding
1D type (e.g. `int[]` for `int[][]`).

### 3.3 Opaque type: `ExecResult`

`ExecResult` is the type returned by `exec()`. It is a first-class value that can be stored
in a variable or passed to a function, but its internals are opaque. It can only be inspected
via `execStatus`, `execStdout`, and `execStderr`.

There is no `ExecResult` literal. A variable of type `ExecResult` must be initialised by an
`exec()` call; declaring one without initialisation is a semantic error.

### 3.4 Type strictness

Cimple does not perform implicit conversions. The following is a **semantic error**:

```c
string s = "age=" + 42;        // ERROR: cannot concatenate string and int
```

Use an explicit conversion instead:

```c
string s = "age=" + toString(42);   // OK
```

The four conversion intrinsics (`toString`, `toInt`, `toFloat`, `toBool`) are the only
polymorphic functions in the language. Their overloads are resolved statically by the
semantic analyser based on the declared type of the argument.

`byte` is intentionally strict:

- integer literals in `[0, 255]` may be assigned directly to `byte`
- assigning an `int` variable or expression to `byte` is a semantic error
- use `intToByte()` when you want an explicit clamp

```c
byte ok = 255;
byte alsoOk = intToByte(300);   // 255

int n = 10;
// byte bad = n;                // semantic error
byte good = intToByte(n);
```

The logical operators `&&`, `||`, and `!` are **not defined on `byte`**; using them on a
`byte` value is a semantic error. Convert to `bool` explicitly if needed.

### 3.5 Union types

A `union` is a type that holds exactly one value at a time, chosen from a fixed set of
typed members. The compiler automatically tracks which member is active via a `.kind`
field, and generates a named constant for each member.

#### Declaration

```c
union Valeur {
    int    i;
    float  f;
    string s;
}
```

This declares a type `Valeur` with three possible members. The compiler generates:
`Valeur.I`, `Valeur.F`, `Valeur.S` (integer constants, one per member, in declaration order).

#### Writing a member

Assign directly to the desired field. The `.kind` is updated automatically.

```c
Valeur v;
v.i = 42;          // v.kind == Valeur.I
v.s = "hello";     // v.kind == Valeur.S  (overwrites previous)
```

#### Reading — use `switch` on `.kind`

Reading a member whose `.kind` does not match is a runtime error. The canonical pattern:

```c
switch (v.kind) {
    case Valeur.I: print("int:    " + toString(v.i)); break;
    case Valeur.F: print("float:  " + toString(v.f)); break;
    case Valeur.S: print("string: " + v.s);           break;
    default: break;
}
```

A `switch` on `.kind` that does not cover all members produces a **compiler warning**.

#### Heterogeneous collections

The primary use case — a single array that holds values of different types:

```c
union Json {
    int    number;
    float  real;
    string text;
    bool   flag;
}

Json[] row;

Json name; name.text   = "Alice"; arrayPush(row, name);
Json age;  age.number  = 30;      arrayPush(row, age);
Json ratio; ratio.real = 0.85;    arrayPush(row, ratio);
Json active; active.flag = true;  arrayPush(row, active);

for (int i = 0; i < count(row); i++) {
    switch (row[i].kind) {
        case Json.NUMBER: print(toString(row[i].number)); break;
        case Json.REAL:   print(toString(row[i].real));   break;
        case Json.TEXT:   print(row[i].text);             break;
        case Json.FLAG:   print(toString(row[i].flag));   break;
    }
    print("\n");
}
```

> `.kind` is read-only. Assigning to it directly is a semantic error.
> Union members may be any scalar, array, or `void` type. Structures as union members are not
> supported in this version.

#### Void members — the Option / nullable pattern

A member declared with `void` carries no payload. It represents an "absent" or "empty" state,
giving Cimple a clean nullable pattern without null pointers.

```c
union Option {
    void none;    // absent — no payload
    int  some;    // present — carries an int
}
```

**Activating a void member** — write the member access as a statement (no assignment):

```c
Option x;
x.none;           // x.kind == Option.none  (absent)
x.some = 42;      // x.kind == Option.some  (present, value = 42)
x.none;           // back to absent
```

**Branching** — void members participate in `switch` like any other member:

```c
switch (x.kind) {
    case Option.none: print("nothing\n");              break;
    case Option.some: print(toString(x.some) + "\n"); break;
}
```

**Practical example** — a function that may return a result or nothing:

```c
union MaybeInt {
    void nothing;
    int  value;
}

MaybeInt find(int[] arr, int target) {
    MaybeInt r;
    for (int i = 0; i < count(arr); i++) {
        if (arr[i] == target) {
            r.value = i;
            return r;
        }
    }
    r.nothing;
    return r;
}

void main() {
    int[] nums = [3, 7, 2, 9];
    MaybeInt result = find(nums, 7);
    switch (result.kind) {
        case MaybeInt.nothing: print("not found\n"); break;
        case MaybeInt.value:   print("found at index " + toString(result.value) + "\n"); break;
    }
}
```

---

## 4. Variables

### 4.1 Scalar declarations

```c
int x;                 // x is 0 (default)
int y = 10;
float pi = 3.14159;
bool ok = true;
string name = "Alice";
```

All variables must have an explicit type. There is no `var` or `auto`.

Uninitialized variables receive default values: `0` for `int` and `byte`, `0.0` for `float`,
`""` for `string`, `false` for `bool`.

### 4.2 Array declarations

```c
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

```c
int[] xs = [1, 2, 3];
xs = [4, 5, 6];        // ERROR: array reassignment is forbidden
```

To modify an array, use the array mutation functions (`arrayPush`, `arraySet`, etc.).

### 4.3 Assignment

Assignment uses `=` and must be type-compatible:

```c
x = 42;
name = "Bob";
scores[1] = 99;        // element assignment is a standalone statement
```

### 4.4 Increment and decrement

`++` and `--` are **postfix-only** and **standalone statements only**:

```c
i++;    // equivalent to i = i + 1
i--;    // equivalent to i = i - 1
```

The prefix forms `++i` and `--i` are syntactically forbidden.
Using `i++` inside an expression (e.g. `x = i++`) is a semantic error.

`++` and `--` only apply to `int` variables. Applying them to a `byte` variable is a
semantic error — use `x = intToByte(byteToInt(x) + 1)` instead.

### 4.5 Compound assignment operators

`+=`, `-=`, `*=`, `/=` and `%=` combine an arithmetic operation with an assignment.
They work on `int` and `float` variables:

```c
int x = 10;
x += 5;     // x = 15
x -= 3;     // x = 12
x *= 2;     // x = 24
x /= 4;     // x = 6
x %= 4;     // x = 2

float f = 1.0;
f += 0.5;   // f = 1.5
```

Compound operators can also appear in the update clause of a `for` loop:

```c
for (int i = 1; i <= 100; i += 3) { ... }
```

Division or modulo by zero with `/=` or `%=` is a runtime error.

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

```c
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
| `\X` (other) | preserved literally as `\` + `X` |

The last row means regex shortcuts such as `\s`, `\d`, `\w`, `\S`, `\D`, `\W`, `\b`, `\B`
can be written directly in string literals **without doubling the backslash**:

```c
string greeting = "Hello!\n";
string path     = "C:\\Users\\Alice";
string heart    = "\u2665";
string accented = "\u00E9l\u00E8ve";   // "élève"

// Regex patterns — no double-backslash needed
RegExp words = regexCompile("\w+", "");
RegExp blank = regexCompile("\s+", "");
RegExp date  = regexCompile("(\d{2})/(\d{2})/(\d{4})", "");
```

A raw newline inside a string literal is a lexical error; use `\n` instead.

### 5.4 Boolean literals

```c
bool t = true;
bool f = false;
```

### 5.5 Array literals

```c
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

#### Bitwise (`int` and `byte`)

| Operator | Description |
|----------|-------------|
| `&` | bitwise AND |
| `\|` | bitwise OR |
| `^` | bitwise XOR |
| `~` | bitwise NOT (unary) |
| `<<` | left shift |
| `>>` | right shift |

The result type depends on the operand types:

| Operands | Result |
|----------|--------|
| `int` op `int` | `int` |
| `byte` op `byte` (binary `&` `\|` `^` only) | `byte` |
| `byte` op `int` or `int` op `byte` | `int` |
| `~byte` | `byte` |
| `~int` | `int` |
| `byte << int`, `byte >> int` | `int` |

```c
// int bitwise
print(toString(6 & 3)   + "\n");   // 2
print(toString(6 | 3)   + "\n");   // 7
print(toString(6 ^ 3)   + "\n");   // 5
print(toString(~0)      + "\n");   // -1
print(toString(1 << 3)  + "\n");   // 8
print(toString(20 >> 2) + "\n");   // 5

// byte bitwise — result is byte when both operands are byte
byte mask = 0xF0;
byte lo   = 0x0F;
byte both = mask | lo;             // byte (0xFF = 255)
int  wide = mask | 256;            // int  (byte | int → int)
```

#### Unary

| Operator | Types | Description |
|----------|-------|-------------|
| `-` | `int`, `float` | numeric negation |
| `!` | `bool` | logical NOT |
| `~` | `int` | bitwise NOT (result `int`) |
| `~` | `byte` | bitwise NOT (result `byte`) |

#### Ternary operator `?:`

The ternary operator provides an inline conditional expression:

```
condition ? value_if_true : value_if_false
```

Both branches must produce the same type. The operator short-circuits: only one branch
is evaluated at runtime.

```c
int x = 10;
string s = (x > 5) ? "big" : "small";   // "big"
int abs = (x >= 0) ? x : -x;            // 10

// Nested ternary
int n = 0;
string sign = (n > 0) ? "positive" : (n < 0) ? "negative" : "zero";
```

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
| 13 | `?:` (right-associative) |
| 14 | `=` `+=` `-=` `*=` `/=` `%=` (right-associative, statement only) |

All binary operators are left-associative unless noted. Assignment operators are
statements, not expressions.

```c
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

```c
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

```c
if (ok) print("yes\n");
if (ok) x = 1; else x = 2;
```

### 6.2 `while`

```c
int i = 0;
while (i < 5) {
    print(toString(i) + "\n");
    i++;
}
```

A single-statement body without braces is allowed:

```c
while (x > 0) x--;
```

### 6.3 `for`

```c
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

```c
for (int j = 0; j < 3; j++) print(toString(j) + "\n");
```

### 6.4 `break` and `continue`

```c
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

```c
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

```c
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

```c
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

### 7.3 Function parameters (callbacks)

A function can be passed as an argument to another function. The parameter type is written
as a function signature — return type, local name, parameter types — exactly like a
function declaration without a body:

```c
void forEach(int[] arr, void action(int)) {
    for (int i = 0; i < count(arr); i++) {
        action(arr[i]);
    }
}

void printVal(int x) { print(toString(x) + "\n"); }

void main() {
    int[] nums = [1, 2, 3, 4, 5];
    forEach(nums, printVal);   // prints each element
}
```

The type of a function parameter is fully described by its return type and the types of
its own parameters. The check is **static**: passing a function with the wrong signature
is a semantic error.

```c
// Generic sort — works for any comparison criterion
void bubbleSort(int[] arr, bool cmp(int, int)) {
    for (int i = 0; i < count(arr) - 1; i++) {
        for (int j = 0; j < count(arr) - i - 1; j++) {
            if (!cmp(arr[j], arr[j+1])) {
                int tmp  = arr[j];
                arr[j]   = arr[j+1];
                arr[j+1] = tmp;
            }
        }
    }
}

bool ascending(int a, int b)  { return a < b; }
bool descending(int a, int b) { return a > b; }

void main() {
    int[] scores = [5, 2, 8, 1, 9];
    bubbleSort(scores, ascending);    // [1, 2, 5, 8, 9]
    bubbleSort(scores, descending);   // [9, 8, 5, 2, 1]
}
```

A function reference can also be stored in a variable using the same declaration syntax:

```c
bool compare(int, int) = ascending;   // variable holding a function
compare(3, 7);                         // called like a regular function
compare = descending;                  // reassignable
```

Standard library functions can be passed as callbacks when their signature matches:

```c
void apply(string[] items, void fn(string)) {
    for (int i = 0; i < count(items); i++) { fn(items[i]); }
}

string[] names = ["Alice", "Bob", "Carol"];
apply(names, print);   // print is void(string): compatible
```

> Anonymous functions (lambdas) are not supported. Only named, globally-declared
> functions can be passed as callbacks.

### 7.4 Recursion

```c
int fibonacci(int n) {
    if (n <= 1) { return n; }
    return fibonacci(n - 1) + fibonacci(n - 2);
}
```

### 7.5 Entry point signatures

`main` must match exactly one of these four signatures:

```c
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

```c
void print(string value)
```

Writes `value` to standard output. Only accepts `string`; use `toString` for other types.

```c
print("x = " + toString(42) + "\n");
```

#### `input`

```c
string input()
```

Reads one line from standard input. The trailing newline is stripped. Returns `""` on EOF or
read error. Takes no arguments.

```c
print("Enter your name: ");
string name = input();
print("Hello, " + name + "\n");
```

---

### 8.2 String functions

#### `len`

```c
int len(string s)
```

Returns the number of **bytes** in `s`. For ASCII strings, byte count equals character count.
For multi-byte UTF-8 strings, `len(s) >= glyphLen(s)`.

#### `glyphLen`

```c
int glyphLen(string s)
```

Returns the number of Unicode code points after NFC normalisation.

```c
string s = "élève";
print(toString(len(s))      + "\n");  // 7  (bytes)
print(toString(glyphLen(s)) + "\n");  // 5  (code points after NFC)
```

#### `glyphAt`

```c
string glyphAt(string s, int index)
```

Returns the Unicode code point at position `index` (zero-based NFC code-point index) as a
single-code-point `string`. Out-of-bounds access is a runtime error.

```c
string s = "hé!";
print(glyphAt(s, 0) + "\n");  // h
print(glyphAt(s, 1) + "\n");  // é
print(glyphAt(s, 2) + "\n");  // !
```

#### `byteAt`

```c
int byteAt(string s, int index)
```

Returns the byte value (0–255) at byte position `index`. Out-of-bounds access is a runtime error.

```c
print(toString(byteAt("A", 0))  + "\n");  // 65
print(toString(byteAt("é", 0))  + "\n");  // 195  (0xC3, first byte of U+00E9)
print(toString(byteAt("é", 1))  + "\n");  // 169  (0xA9, second byte)
```

#### `s[i]` — byte indexing

`s[i]` returns the single byte at position `i` as a one-byte `string`. It is the string
equivalent of `byteAt`, with `string` rather than `int` as the return type.

```c
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

```c
string substr(string s, int start, int length)
```

Returns at most `length` **bytes** starting at byte offset `start`. If the slice would
exceed the string length it is clamped silently.

```c
print(substr("Hello, world", 7, 5) + "\n");  // world
```

#### `indexOf`

```c
int indexOf(string s, string needle)
```

Returns the byte offset of the first occurrence of `needle` in `s`, or `-1` if not found.
An empty `needle` always returns `0`.

```c
print(toString(indexOf("banana", "na")) + "\n");  // 2
print(toString(indexOf("banana", "xy")) + "\n");  // -1
```

#### `contains` / `startsWith` / `endsWith`

```c
bool contains(string s, string needle)
bool startsWith(string s, string prefix)
bool endsWith(string s, string suffix)
```

All three are case-sensitive.

```c
print(toString(contains("foobar", "oba"))    + "\n");  // true
print(toString(startsWith("foobar", "foo"))  + "\n");  // true
print(toString(endsWith("foobar", "bar"))    + "\n");  // true
```

#### `replace`

```c
string replace(string s, string old, string new)
```

Replaces **all** occurrences of `old` with `new`. Returns `s` unchanged if `old` is absent.
An empty `old` is a runtime error.

```c
print(replace("banana", "na", "X")          + "\n");  // baXX
print(replace("hello world", "world", "Cimple") + "\n");  // hello Cimple
print(replace("hello", "xyz", "abc")        + "\n");  // hello  (unchanged)
```

#### `format`

```c
string format(string template, string[] args)
```

Substitutes `{}` markers in order with elements of `args`. The number of markers must equal
`count(args)` exactly; a mismatch is a runtime error. All arguments must be `string`; convert
with `toString` beforehand.

```c
string msg = format("Name: {}, age: {}.", ["Alice", toString(30)]);
print(msg + "\n");  // Name: Alice, age: 30.
```

#### `join`

```c
string join(string[] array, string separator)
```

Concatenates all elements of `array` with `separator` between consecutive elements.
Returns `""` for an empty array. A single-element array returns that element.

```c
string[] parts = ["one", "two", "three"];
print(join(parts, ", ")  + "\n");  // one, two, three
print(join(parts, "")    + "\n");  // onetwothree
print(join([], "-")      + "\n");  // (empty)
```

#### `split`

```c
string[] split(string value, string separator)
```

Splits `value` on each occurrence of `separator`. Consecutive separators produce empty
strings. An empty `separator` is a runtime error. An empty `value` returns `[""]`.

```c
string[] parts = split("a,b,,c", ",");
// ["a", "b", "", "c"]

string[] words = split("hello world", " ");
// ["hello", "world"]
```

#### `concat`

```c
string concat(string[] array)
```

Concatenates all elements with no separator. Equivalent to `join(array, "")`.

```c
print(concat(["x", "=", "42"]) + "\n");  // x=42
```

#### Whitespace helpers

```c
string trim(string s)
string trimLeft(string s)
string trimRight(string s)
bool isBlank(string s)
```

These helpers use the ASCII whitespace set: space, tab, carriage return, newline,
vertical tab, and form feed.

```c
print("[" + trim("  bonjour  ") + "]\n");      // [bonjour]
print("[" + trimLeft("\t hello\n") + "]\n");   // [hello\n]
print("[" + trimRight("\t hello\n") + "]\n");  // [\t hello]
print(toString(isBlank("\t\n")) + "\n");       // true
```

#### Case helpers

```c
string toUpper(string s)
string toLower(string s)
string capitalize(string s)
```

- `toUpper` uppercases every Unicode code point
- `toLower` lowercases every Unicode code point
- `capitalize` uppercases only the first code point and leaves the remainder unchanged

```c
print(toUpper("straße") + "\n");    // STRASSE
print(toLower("HÉLLO") + "\n");     // héllo
print(capitalize("été") + "\n");    // Été
```

#### Padding and repetition

```c
string padLeft(string s, int width, string pad)
string padRight(string s, int width, string pad)
string repeat(string s, int n)
```

- widths are measured in glyphs
- if `glyphLen(s) >= width`, padding returns `s` unchanged
- if `pad == ""`, padding returns `s` unchanged
- negative `width` or negative repeat counts are runtime errors

```c
print("[" + padLeft("42", 6, " ") + "]\n");   // [    42]
print("[" + padRight("hi", 5, "+-") + "]\n"); // [hi+-+]
print(repeat("-", 5) + "\n");                 // -----
```

#### Reverse search and counting

```c
int lastIndexOf(string s, string needle)
int countOccurrences(string s, string needle)
```

- `lastIndexOf` returns the byte offset of the last occurrence, or `-1`
- `countOccurrences` counts non-overlapping matches
- `countOccurrences(..., "")` is a runtime error

```c
print(toString(lastIndexOf("abcabc", "abc")) + "\n");     // 3
print(toString(countOccurrences("aaaa", "aa")) + "\n");   // 2
```

---

### 8.3 Type conversion intrinsics

`toString`, `toInt`, `toFloat`, and `toBool` are polymorphic: they accept several source
types and the correct overload is resolved statically by the semantic analyser.

#### `toString`

```c
string toString(int value)
string toString(float value)
string toString(bool value)
string toString(byte value)
```

```c
print(toString(42)              + "\n");  // 42
print(toString(3.14)            + "\n");  // 3.14
print(toString(true)            + "\n");  // true
print(toString(intToByte(65))   + "\n");  // 65   (decimal, 0–255)
```

#### `toInt`

```c
int toInt(float value)
int toInt(string value)
```

- `toInt(float)` truncates toward zero: `toInt(3.9)` → `3`, `toInt(-2.7)` → `-2`.
- `toInt(string)` returns `0` for unparseable input.

```c
print(toString(toInt(3.9))    + "\n");  // 3
print(toString(toInt("-12"))  + "\n");  // -12
print(toString(toInt("bad"))  + "\n");  // 0
```

#### `toFloat`

```c
float toFloat(int value)
float toFloat(string value)
```

`toFloat(string)` returns `NaN` for unparseable input.

```c
print(toString(toFloat(5))      + "\n");  // 5
print(toString(toFloat("1.5"))  + "\n");  // 1.5
```

#### `toBool`

```c
bool toBool(string value)
```

Accepts `"true"`, `"false"`, `"1"`, `"0"`. Returns `false` for unrecognised input.

```c
print(toString(toBool("true"))  + "\n");  // true
print(toString(toBool("0"))     + "\n");  // false
```

---

### 8.4 String validation predicates

```c
bool isIntString(string value)
bool isFloatString(string value)
bool isBoolString(string value)
```

Return `true` if `value` is a valid representation of the respective type. Useful for safe
parsing before calling `toInt` / `toFloat` / `toBool`.

```c
if (isIntString(s)) {
    int n = toInt(s);
    // use n
}
```

---

### 8.5 Float predicates

```c
bool isNaN(float value)
bool isInfinite(float value)
bool isFinite(float value)
bool isPositiveInfinity(float value)
bool isNegativeInfinity(float value)
```

```c
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

```c
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

```c
print(toString(absInt(-7))           + "\n");  // 7
print(toString(clampInt(15, 0, 10))  + "\n");  // 10
print(toString(safeDivInt(10, 0, -1))+ "\n");  // -1
print(toString(isEven(4))            + "\n");  // true
print(toString(isOdd(7))             + "\n");  // true
```

---

### 8.8 Array functions

These polymorphic intrinsics work with any element type (`T` stands for `int`, `float`,
`bool`, `string`, `byte`, or a structure type):

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

```c
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

```c
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

```c
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

```c
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

```c
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

```c
ExecResult r = exec(["python3", "script.py"], ["DEBUG=1", "LANG=en_US.UTF-8"]);
```

`exec` is unavailable on WebAssembly and raises a runtime error if called.

---

### 8.11 Environment variables

```c
string getEnv(string name, string fallback)
```

Returns the value of environment variable `name`, or `fallback` if it is not set.
A variable set to an empty string returns `""`, not the fallback — the variable exists,
it is just empty.

```c
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

```c
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

`formatDate` substitutes single-letter tokens in `fmt` left-to-right (PHP-compatible).
Any character that does not match a token is copied verbatim. Use `\` to escape a letter
that should appear literally (e.g. `\Y` → `"Y"`).

| Token | Field | Range / format | Example |
|-------|-------|----------------|---------|
| `Y` | 4-digit year, zero-padded | `0000`–`9999` | `2025` |
| `m` | 2-digit month | `01`–`12` | `03` |
| `d` | 2-digit day | `01`–`31` | `11` |
| `H` | 2-digit hour (24 h) | `00`–`23` | `14` |
| `i` | 2-digit minute | `00`–`59` | `32` |
| `s` | 2-digit second | `00`–`59` | `07` |
| `w` | weekday, no padding (`0`=Sunday … `6`=Saturday) | `0`–`6` | `2` |
| `z` | day of year, base 0, no padding | `0`–`365` | `69` |
| `W` | ISO 8601 week number, zero-padded | `01`–`53` | `11` |
| `c` | full UTC date-time | `Y-m-dTH:i:sZ` | `2025-03-11T14:32:07Z` |

**Notes on specific tokens:**

- **`z`** — January 1 is `"0"`, December 31 of a non-leap year is `"364"`, December 31 of
  a leap year is `"365"`. The value is not padded.
- **`W`** — uses ISO 8601 week numbering: weeks start on **Monday**; the week containing
  the first Thursday of the year is week `01`. Days at the very start or end of a year can
  belong to the previous or next year's week.
- **`c`** — returns `"invalid"` if the year of the epoch falls outside the range `0`–`9999`.

```c
void main() {
    int ts = makeEpoch(2025, 3, 11, 14, 32, 7);
    print(formatDate(ts, "Y-m-d")          + "\n");  // 2025-03-11
    print(formatDate(ts, "H:i:s")          + "\n");  // 14:32:07
    print(formatDate(ts, "[Y-m-d]")        + "\n");  // [2025-03-11]  ([ ] copied verbatim)
    print(formatDate(ts, "d/m/Y")          + "\n");  // 11/03/2025
    print(formatDate(ts, "w")              + "\n");  // 2             (Tuesday)
    print(formatDate(ts, "z")              + "\n");  // 69            (70th day, base 0)
    print(formatDate(ts, "W")              + "\n");  // 11            (ISO week 11)
    print(formatDate(ts, "Y-W-w")          + "\n");  // 2025-11-2     (ISO week date)
    print(formatDate(ts, "c")              + "\n");  // 2025-03-11T14:32:07Z
    print(formatDate(ts, "\Y\e\a\r: Y")   + "\n");  // Year: 2025    (backslash escape)

    int t0 = now();
    // ... some work ...
    int t1 = now();
    print("Elapsed: " + toString(t1 - t0) + " ms\n");
}
```

To get the current time in seconds: `now() / 1000`.

---

### 8.13 File system helpers

#### `cwd`

```c
string cwd()
```

Returns the current working directory as an absolute path.

#### `tempPath`

```c
string tempPath()
```

Returns a unique path string that does not currently exist on disk. It does **not** create
the file. Two successive calls always return different paths. The path is located in the
system's temporary directory (`$TMPDIR` on POSIX, `%TEMP%` on Windows).

```c
string tmp = tempPath();
writeFile(tmp, "scratch data");
// ... use tmp ...
remove(tmp);
```

#### `remove`

```c
void remove(string path)
```

Deletes the file at `path`. Both of the following are **runtime errors**:
- `path` does not exist
- `path` names a directory (use a shell command via `exec` to remove directories)

#### `chmod`

```c
void chmod(string path, int mode)
```

Changes the permission bits of `path` to `mode` (octal integer, e.g. `0644`). Available
on **POSIX platforms only** (macOS, Linux). Raises a runtime error on Windows and
WebAssembly with the message `chmod: not supported on this platform`.

```c
chmod("script.sh", 0x1ED);  // 0755 octal — owner rwx, group rx, other rx
```

#### `copy`

```c
void copy(string src, string dst)
```

Copies the regular file at `src` to `dst`. If `dst` already exists it is overwritten.
Runtime errors:
- `src` does not exist
- `src` is a directory (only regular files may be copied)
- the parent directory of `dst` does not exist

#### `move`

```c
void move(string src, string dst)
```

Moves (renames) the regular file at `src` to `dst`. If `dst` already exists it is
overwritten. Runtime errors:
- `src` does not exist
- `src` is a directory
- the parent directory of `dst` does not exist

#### `isReadable` / `isWritable` / `isExecutable`

```c
bool isReadable(string path)
bool isWritable(string path)
bool isExecutable(string path)
```

Return `true` if the current process has the corresponding permission on `path`. Return
`false` if `path` does not exist.

#### `isDirectory`

```c
bool isDirectory(string path)
```

Returns `true` if `path` names an existing directory, `false` otherwise (including when the
path does not exist or names a regular file).

#### Path decomposition helpers

```c
string dirname(string path)    // parent directory
string basename(string path)   // file name with extension
string filename(string path)   // file name without extension
string extension(string path)  // extension without leading dot
```

| Call | Result | Notes |
|------|--------|-------|
| `dirname("/tmp/demo.txt")` | `"/tmp"` | |
| `dirname("file.txt")` | `""` | no directory component |
| `basename("/tmp/demo.txt")` | `"demo.txt"` | |
| `filename("archive.tar.gz")` | `"archive.tar"` | strips last extension only |
| `filename(".gitignore")` | `".gitignore"` | leading-dot file has no extension |
| `extension("archive.tar.gz")` | `"gz"` | |
| `extension(".gitignore")` | `""` | leading-dot file has no extension |
| `extension("makefile")` | `""` | no extension |

```c
void main() {
    string src = tempPath();
    string dst = tempPath();

    writeFile(src, "hello");
    copy(src, dst);
    print(readFile(dst) + "\n");       // hello

    move(dst, "moved.txt");
    print(toString(fileExists("moved.txt")) + "\n");   // true
    print(toString(isDirectory(cwd()))      + "\n");   // true
    print(dirname("/tmp/demo.txt")          + "\n");   // /tmp
    print(filename("archive.tar.gz")        + "\n");   // archive.tar
    print(extension("archive.tar.gz")       + "\n");   // gz

    remove(src);
    remove("moved.txt");
}
```

---

### 8.14 Binary I/O and `byte`

```c
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
- **Arithmetic** (`+`, `-`, `*`, `/`, `%`): `byte op byte` and `byte op int` always produce `int`
- **Bitwise binary** (`&`, `|`, `^`): `byte op byte` → `byte`; `byte op int` or `int op byte` → `int`
- **Bitwise unary** (`~`): `~byte` → `byte`; `~int` → `int`
- **Shifts** (`<<`, `>>`): `byte << int` and `byte >> int` → `int`; the result can exceed 255
- **Comparisons** (`==`, `!=`, `<`, `<=`, `>`, `>=`): a `byte` may be compared with another `byte`
  or with an `int` (the byte is promoted to int for the comparison)
- **Logical operators** (`&&`, `||`, `!`): **not defined on `byte`** — semantic error
- **Increment / decrement** (`++`, `--`): **not defined on `byte`** — semantic error
- `bytesToString()` replaces invalid UTF-8 byte sequences with `U+FFFD`
- `bytesToInt()` requires exactly `INT_SIZE` bytes
- `bytesToFloat()` requires exactly `FLOAT_SIZE` bytes

```c
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

### 8.14 Utility

#### `assert`

```c
void assert(bool condition)
void assert(bool condition, string message)
```

Terminates the program immediately if `condition` is `false`.
Prints `[ASSERTION FAILED]` to stderr, with the optional custom `message` and line number.

```c
assert(x > 0);
assert(count(items) > 0, "items list must not be empty");
```

Use `assert` to validate invariants and catch programming errors early.

#### `randInt`

```c
int randInt(int min, int max)
```

Returns a random integer uniformly distributed in the closed range `[min, max]`.
Passing `min > max` is a runtime error. Passing `min == max` always returns `min`.

```c
int dice = randInt(1, 6);
int coin = randInt(0, 1);   // 0 = heads, 1 = tails
```

#### `randFloat`

```c
float randFloat()
```

Returns a random `float` uniformly distributed in the half-open range `[0.0, 1.0)`.

```c
float r = randFloat();
float scaled = 10.0 * randFloat();   // in [0.0, 10.0)
```

#### `sleep`

```c
void sleep(int ms)
```

Pauses execution for `ms` milliseconds. Useful for rate limiting, simple animations,
or delay loops.

```c
sleep(500);   // wait 500 ms
sleep(1000);  // wait 1 second
```

> `sleep()` is not available on WebAssembly targets (runtime error).

---

### 8.15 Regular expressions (`RegExp`)

Cimple provides a built-in regex engine through two opaque types and a set of global
functions, all prefixed with `regex`.

#### Compilation

```c
RegExp re = regexCompile(string pattern, string flags)
```

`flags` is a string of zero or more characters (order irrelevant, duplicates ignored):

| Flag | Meaning |
|------|---------|
| `"i"` | case-insensitive (ASCII A–Z ↔ a–z in V1) |
| `"m"` | multiline — `^` and `$` match per line |
| `"s"` | dotall — `.` matches `\n` too |

A syntax error in the pattern raises a runtime error `RegExpSyntax`.

```c
RegExp ws   = regexCompile("\s+", "");
RegExp word = regexCompile("\w+", "i");
RegExp date = regexCompile("(\d{2})/(\d{2})/(\d{4})", "");
```

> **No double-backslash needed.** Cimple preserves unknown escape sequences (`\s`, `\d`,
> `\w`, etc.) literally, so regex shortcuts work directly in string literals.

#### Test and search

```c
bool          regexTest(RegExp re, string input, int start)
RegExpMatch   regexFind(RegExp re, string input, int start)
RegExpMatch[] regexFindAll(RegExp re, string input, int start, int max)
```

`start` is a **glyph index** (0-based). `max = -1` means unlimited; `max = 0` returns `[]`.

```c
RegExp digits = regexCompile("\d+", "");
bool ok = regexTest(digits, "price: 42", 0);    // true

RegExpMatch m = regexFind(digits, "price: 42", 0);
if (regexOk(m)) {
    print(regexGroups(m)[0] + "\n");   // "42"
}
```

#### Inspecting a match

```c
bool     regexOk(RegExpMatch m)
int      regexStart(RegExpMatch m)    // glyph index of first character
int      regexEnd(RegExpMatch m)      // glyph index past last character
string[] regexGroups(RegExpMatch m)   // [0]=full match, [1..n]=capturing groups
```

`regexGroups(m)[0]` is always the full match when `ok=true`. An optional group that did
not participate in the match produces `""`.

#### Replace

```c
string regexReplace(RegExp re, string input, string replacement, int start)
string regexReplaceAll(RegExp re, string input, string replacement, int start, int max)
```

In `replacement`: `$0` = full match, `$1`–`$99` = capturing groups, `$$` = literal `$`.

```c
RegExp swap = regexCompile("(\w+)-(\w+)", "");
string s = regexReplaceAll(swap, "alpha-beta gamma-delta", "$2:$1", 0, -1);
print(s + "\n");   // "beta:alpha delta:gamma"
```

#### Split

```c
string[] regexSplit(RegExp re, string input, int start, int maxParts)
```

`maxParts = -1` means unlimited; `maxParts = 0` returns `[]`.

```c
RegExp sep = regexCompile(",\s*", "");
string[] parts = regexSplit(sep, "a, b,  c", 0, -1);
// parts = ["a", "b", "c"]
```

#### Metadata

```c
string regexPattern(RegExp re)   // original pattern string
string regexFlags(RegExp re)     // canonical flags, e.g. "ims"
```

#### Pattern syntax reference (V1)

| Category | Syntax |
|----------|--------|
| Any glyph | `.` (excludes `\n` unless flag `s`) |
| Anchors | `^` `$` (per line with flag `m`) |
| Character class | `[abc]` `[a-z]` `[^...]` |
| Capturing group | `(…)` |
| Non-capturing group | `(?:…)` |
| Alternation | `A\|B\|C` |
| Greedy quantifiers | `*` `+` `?` `{m}` `{m,}` `{m,n}` |
| Non-greedy | `*?` `+?` `??` `{m}?` `{m,}?` `{m,n}?` |
| Digit shortcut | `\d` `\D` |
| Word shortcut | `\w` `\W` |
| Whitespace shortcut | `\s` `\S` |

Backreferences in patterns (`\1`), lookahead/lookbehind (`(?=…)` etc.) are **not supported**.

#### Full example — date extraction

```c
void main() {
    RegExp re = regexCompile("(\d{4})-(\d{2})-(\d{2})", "");
    string text = "Starts: 2026-12-31, ends: 2027-01-15";

    RegExpMatch[] ms = regexFindAll(re, text, 0, -1);
    for (int i = 0; i < count(ms); i++) {
        string[] g = regexGroups(ms[i]);
        print("year=" + g[1] + " month=" + g[2] + " day=" + g[3] + "\n");
    }
    // year=2026 month=12 day=31
    // year=2027 month=01 day=15
}
```

---

## 9. Imports

Use top-level imports to split a program across multiple files:

```c
import "math/add.ci";
import "utils/strings.ci";
```

Rules:

- every `import` must appear **before any declaration** in the current file — including
  global variable declarations; an `import` that follows a declaration is a syntax error
- `import` inside a function or block is a **syntax error**
- the path must end with `.ci`; any other extension is a semantic error
- paths are resolved **relative to the directory of the file containing the `import`**,
  not relative to the working directory where `cimple run` is invoked; absolute paths are
  also accepted; both `/` and `\` are valid separators
- imports are resolved recursively (depth-first) before semantic analysis
- each imported file is processed **exactly once**, even if multiple files import it;
  re-importing the same file is silently ignored, not an error
- the maximum import nesting depth is **32 levels**; a chain deeper than that is a
  semantic error
- imported files may not define `main`
- circular imports are semantic errors

```c
// main.ci
import "math/add.ci";

void main(string[] args) {
    print(toString(addThree(4)) + "\n");
}
```

```c
// math/add.ci
import "more.ci";      // resolved relative to math/, not to the project root

int addThree(int n) {
    return addOne(addOne(addOne(n)));
}
```

```c
// math/more.ci
int addOne(int n) { return n + 1; }
```

---

## 10. Structures

Structures group related data and behaviour into a single named type. They are the only
abstraction mechanism in Cimple — there are no classes, interfaces, or generics.

```c
// Without a structure: scattered parallel variables.
float px = 0.0;
float py = 0.0;

// With a structure: one named value that carries both fields and operations.
structure Point {
    float x;          // implicit default: 0.0
    float y;          // implicit default: 0.0

    void move(float dx, float dy) {
        self.x = self.x + dx;
        self.y = self.y + dy;
    }

    float distanceTo(Point other) {
        float dx = self.x - other.x;
        float dy = self.y - other.y;
        return sqrt(dx * dx + dy * dy);
    }
}
```

### Declaration rules

- Fields and methods may appear in **any order** inside the body.
- A structure must be **declared before use** (textual order); forward references are a semantic error.
- **Field types**: any scalar (`int`, `float`, `bool`, `string`, `byte`), any array type, or a previously declared structure type. `ExecResult` and `void` are not valid field types.
- **Implicit defaults**: scalar and array fields do not require an explicit initializer. When omitted, the field receives the type's zero value automatically: `0` for `int`/`byte`, `0.0` for `float`, `false` for `bool`, `""` for `string`, `[]` for any array type. `int x;` and `int x = 0;` are strictly equivalent inside a structure.
- A field of **structure type** is the only exception: it **must** be initialized with `clone StructureName`. There is no sensible zero value for a composite type, so the initializer is always required.
- **Recursive fields** (a structure that directly or indirectly contains a field of its own type) are a semantic error.

```c
structure Config {
    string host = "localhost";  // explicit default overrides implicit ""
    int    port = 8080;         // explicit default overrides implicit 0
    bool   debug;               // implicit default: false
    string[] tags;              // implicit default: []
    // Config self = clone Config;   ← semantic error: recursive field
}
```

### `self` — the current instance

Inside any method, `self` refers to the current instance. You must always write `self.field`
to read or write a field; bare `field` without `self.` is a compile error inside a method.

```c
structure Counter {
    int value;    // implicit default: 0

    void increment() {
        self.value = self.value + 1;
    }

    int get() {
        return self.value;
    }
}
```

`self` cannot be reassigned (`self = clone Other` is a semantic error).

### `clone` — creating instances

`clone StructureName` creates a new, independent instance with all fields set to their
declared defaults (or implicit defaults for scalar and array fields).

```c
Counter a = clone Counter;
Counter b = clone Counter;

a.increment();
a.increment();
// a.value == 2, b.value == 0 — each clone is independent
```

`clone` takes a **type name**, not a variable. `clone a` (where `a` is a variable) is a
semantic error; use `clone Counter` instead.

### Inheritance

A structure may inherit from one base using `:`:

```c
structure Animal {
    string name = "";

    void speak() {
        print(self.name + " says: ...\n");
    }
}

structure Dog : Animal {
    // Override 'name' with a different default (same type — string).
    string name = "Rex";

    // Override 'speak' with an identical signature.
    void speak() {
        print(self.name + " says: Woof!\n");
    }
}

structure GuideDog : Dog {
    string role = "guide";
}
```

Rules:

- Inheritance is **single** — one base only.
- Chain inheritance is allowed (`GuideDog : Dog : Animal`).
- The base structure must be **declared before** the derived one.
- **Field redefinition**: a derived structure may redeclare an inherited field with the **same type** but a different default. `self.field` always refers to the current structure's own slot.
- **Method override**: the signature (name, parameter types, return type) must be **identical**; a different signature is a semantic error.
- **Inheritance cycles** are a semantic error.

### `super` — calling the base method

Inside a derived structure method, `super.method(...)` calls the **direct base** version of
that method. This is useful when you want to extend, rather than replace, the base behaviour:

```c
structure LoggingDog : Dog {
    void speak() {
        print("[LOG] Dog about to speak\n");
        super.speak();          // calls Dog.speak(), not Animal.speak()
        print("[LOG] Done\n");
    }
}
```

- `super.method(...)` is only available inside a **derived** structure method.
- `super.field` is a semantic error — fields are always accessed via `self`.
- Using `super` in a structure without a base is a semantic error.

### Structures in functions

Structure arguments are passed **by reference**: modifications made inside the function are
visible in the caller. Returning a structure from a global function returns a **copy**.

```c
void reset(Point p) {
    p.x = 0.0;    // modifies the original
    p.y = 0.0;
}

Point makeOrigin() {
    Point p = clone Point;
    return p;     // caller receives a copy
}

void main() {
    Point pt = clone Point;
    pt.x = 5.0;
    reset(pt);
    print(toString(pt.x) + "\n");   // 0.0 — reset modified it in place
}
```

### Arrays of structures

`StructureName[]` works exactly like the built-in array types:

```c
Point[] pts = [];
arrayPush(pts, clone Point);
arrayPush(pts, clone Point);

pts[0].x = 1.0;   // modify in place — OK
pts[1].y = 2.0;

// arrayGet returns a COPY — modifying it does NOT affect pts:
Point copy = arrayGet(pts, 0);
copy.x = 99.0;
print(toString(pts[0].x) + "\n");   // still 1.0

// To modify an element in place, use direct indexed access:
pts[0].x = 99.0;   // this DOES affect the stored element
```

### Polymorphism

All structure methods are **virtual by default**. When a sub-structure overrides a
method, the sub-structure's version is always called at runtime — regardless of the
declared type of the variable.

#### Covariant arrays

An array declared as `Base[]` can hold instances of `Base` **and any of its
sub-structures**. Method calls dispatch dynamically to the actual runtime type.

```c
structure Animal {
    string speak() { return "..."; }
}

structure Dog : Animal {
    string speak() { return "woof"; }
}

structure Cat : Animal {
    string speak() { return "meow"; }
}

// One array, mixed types — works naturally
Animal[] zoo = [clone Dog, clone Cat, clone Dog];

for (int i = 0; i < count(zoo); i++) {
    print(zoo[i].speak() + "\n");   // woof, meow, woof
}
```

Assigning a sub-structure instance to a base-type variable is also valid:

```c
Animal a = clone Dog;
print(a.speak() + "\n");   // woof
```

#### Polymorphic function parameters

A function accepting a base type will work correctly with any sub-type:

```c
void describe(Animal a) {
    print("This animal says: " + a.speak() + "\n");
}

describe(clone Dog);   // This animal says: woof
describe(clone Cat);   // This animal says: meow
```

#### Rules

- **All methods are virtual** — there is no `virtual` or `override` keyword.
- An override must have the **exact same signature** (same return type, same parameter
  types); any mismatch is a semantic error.
- **Fields are not virtual**: `a.x` always accesses the field `x` as declared in the
  base type, even if the instance is a sub-structure.
- `super.method()` explicitly calls the base implementation.

#### Full example — shapes

```c
structure Shape {
    float area()   { return 0.0; }
    string label() { return "shape"; }
}

structure Circle : Shape {
    float radius;
    float area()   { return 3.14159 * self.radius * self.radius; }
    string label() { return "circle"; }
}

structure Rect : Shape {
    float width;
    float height;
    float area()   { return self.width * self.height; }
    string label() { return "rect"; }
}

void printShape(Shape s) {
    print(s.label() + " area=" + toString(s.area()) + "\n");
}

void main() {
    Circle c = clone Circle; c.radius = 5.0;
    Rect   r = clone Rect;   r.width  = 4.0; r.height = 6.0;

    Shape[] shapes = [c, r];
    for (int i = 0; i < count(shapes); i++) {
        printShape(shapes[i]);
    }
    // circle area=78.53975
    // rect   area=24.0
}
```

---

## 11. Predefined Constants

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

```c
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

## 12. Scoping Rules

Cimple uses **lexical (block) scoping**.

- A variable declared inside `{ }` is local to that block and invisible outside it.
- Inner blocks may shadow outer variables by declaring a new variable of the same name.
- A variable must be declared before first use.
- Function parameters are scoped to the function body.
- The `for` loop variable is scoped to the loop body.
- All function declarations are globally visible; a function may be called before its textual
  definition.

```c
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

## 13. Keywords

The following identifiers are reserved and may not be used as variable or function names:

```
bool      break     clone     continue  else      ExecResult
false     float     for       if        import
int       return    self      string    structure super
true      void      while     byte
```

The predefined constants are also reserved (see §10):

```
FLOAT_DIG  FLOAT_EPSILON  FLOAT_MAX  FLOAT_MIN  FLOAT_SIZE
INT_MAX    INT_MIN        INT_SIZE
M_E        M_LN10         M_LN2      M_PI       M_SQRT2  M_TAU
```

---

## 14. Diagnostics

### Error categories

The implementation reports four categories of error, each with a source location:

| Category | When raised | Exit code | Example |
|----------|-------------|-----------|---------|
| **Lexical error** | during tokenisation | `1` | unknown escape `\q`, unterminated string |
| **Syntax error** | during parsing | `1` | missing `;`, unmatched `}` |
| **Semantic error** | during analysis | `1` | type mismatch, undeclared variable, missing `return` |
| **Runtime error** | during execution | `2` | division by zero, out-of-bounds index, file not found |

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | program completed successfully |
| `1` | lexical, syntax, or semantic error |
| `2` | runtime error |

### Error behaviour

- **Lexical and syntax errors** are fatal: the compiler reports the first error and stops
  immediately.
- **Semantic errors** accumulate: up to **10** errors are collected and reported together
  before stopping. This lets you fix several problems in one edit cycle.
- **Runtime errors** abort the program immediately with a message that includes the source
  file, line, and column, followed by an indication line describing the cause.

### Common semantic errors

- Using a variable before declaring it.
- Calling a function with wrong argument count or wrong types.
- Missing `return` on some path of a non-`void` function.
- `break` or `continue` outside a loop.
- Assigning to a string index (`s[i] = ...`).
- Reassigning an array variable as a whole (`xs = [...]` after its declaration).
- Declaring a variable whose name matches a reserved constant (`INT_MAX = 5` is forbidden).
- Calling `toInt(bool)` or any conversion with an unsupported source type.
- Applying `&&`, `||`, `!`, `++`, or `--` to a `byte` value.
- Placing an `import` after a declaration, or inside a function body.

---

## 15. Worked Examples

### 15.1 FizzBuzz

```c
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

### 15.2 Recursive Fibonacci

```c
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

### 15.3 Count words in a line

```c
void main() {
    print("Enter a sentence: ");
    string line = input();
    string[] words = split(line, " ");
    print("Word count: " + toString(count(words)) + "\n");
}
```

### 15.4 Sum an array with a helper function

```c
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

### 15.5 File processing

```c
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

### 15.6 Running an external command

```c
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

### 15.7 Formatted timestamp

```c
void main() {
    int ts = now();
    string stamp = formatDate(ts, "Y-m-d H:i:s");
    print("[" + stamp + " UTC] Program started\n");
}
```

### 15.8 Bitwise permission flags

```c
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

### 15.9 CSV line parser

```c
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

### 15.10 Structures — a simple task list

This example shows structure declaration, `clone`, `self`, method calls, arrays of
structures, and in-place mutation via direct indexed access.

```c
structure Task {
    string title = "";
    bool   done  = false;

    void complete() {
        self.done = true;
    }

    string describe() {
        string mark = "[x]";
        if (!self.done) { mark = "[ ]"; }
        return mark + " " + self.title;
    }
}

void printAll(Task[] tasks) {
    for (int i = 0; i < count(tasks); i++) {
        // tasks[i].describe() calls the method on the stored element
        print(tasks[i].describe() + "\n");
    }
}

void main() {
    Task[] list = [];

    Task t1 = clone Task;
    t1.title = "Buy groceries";
    arrayPush(list, t1);

    Task t2 = clone Task;
    t2.title = "Write report";
    arrayPush(list, t2);

    Task t3 = clone Task;
    t3.title = "Call dentist";
    arrayPush(list, t3);

    // Mark the second task complete directly through the array
    list[1].complete();

    printAll(list);
    // [ ] Buy groceries
    // [x] Write report
    // [ ] Call dentist
}
```

### 15.11 Structures — inheritance with `super`

```c
structure Shape {
    string color = "black";

    string describe() {
        return "a " + self.color + " shape";
    }
}

structure Circle : Shape {
    float radius = 1.0;

    string describe() {
        // Extend the base description instead of replacing it.
        return super.describe() + " (circle, r=" + toString(self.radius) + ")";
    }

    float area() {
        return M_PI * self.radius * self.radius;
    }
}

structure Rectangle : Shape {
    float width  = 1.0;
    float height = 1.0;

    string describe() {
        return super.describe() + " (rect, " +
               toString(self.width) + "x" + toString(self.height) + ")";
    }

    float area() {
        return self.width * self.height;
    }
}

void main() {
    Circle c = clone Circle;
    c.color  = "red";
    c.radius = 5.0;
    print(c.describe() + "\n");          // a red shape (circle, r=5)
    print("area = " + toString(c.area()) + "\n");  // ~78.54

    Rectangle r = clone Rectangle;
    r.color  = "blue";
    r.width  = 4.0;
    r.height = 3.0;
    print(r.describe() + "\n");          // a blue shape (rect, 4x3)
    print("area = " + toString(r.area()) + "\n");  // 12
}
```

---

*End of manual.*
