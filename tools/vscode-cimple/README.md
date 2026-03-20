# Cimple Language Extension for VS Code

Provides syntax highlighting, auto-completion, hover documentation, and document outlines for the [Cimple](https://github.com/your-org/cimple) language (`.ci` files).

## Features

- **Syntax highlighting** — keywords, types, built-in functions and constants, strings (including triple-quoted), numbers, comments, operators, structure/union declarations, function definitions, `self`/`super` member access.
- **Auto-completion** — all built-in functions with signatures and snippets, built-in constants, built-in types, and user-defined functions, structures, and unions discovered across workspace `.ci` files.
- **Dot completion** — after `varName.`, shows fields and methods of the resolved type.
- **Hover documentation** — hover over any built-in function, constant, or class to see its signature and description.
- **Document outline** — the VS Code Outline view lists all functions, structures (with fields and methods), and unions in the current file.
- **Snippets** — common patterns: `main`, `fn`, `structure`, `for`, `forin`, `try`, `clone`, `throw`, and more.
- **Language configuration** — bracket matching, auto-closing pairs, comment toggling, and indentation rules.

## Installation (development)

```sh
cd tools/vscode-cimple
npm install
npm run compile
```

Then press **F5** in VS Code to open an Extension Development Host, or package with `vsce package`.

## Language Quick Reference

```cimple
// Types: int, float, bool, string, byte, void
// Arrays: int[], string[] ...
// Maps:   int[string], string[string] ...

structure Point {
    float x = 0.0;
    float y = 0.0;

    void _init(float x, float y) {
        self.x = x;
        self.y = y;
    }

    void move(float dx, float dy) {
        self.x = self.x + dx;
        self.y = self.y + dy;
    }
}

void main() {
    Point p = clone Point(1.0, 2.0);
    p.move(0.5, -1.0);
    print(format("x={}, y={}", p.x, p.y));
}
```

## Snippets

| Prefix      | Expands to                            |
|-------------|---------------------------------------|
| `main`      | `void main() { ... }`                 |
| `fn`        | Function definition                   |
| `structure` | Structure definition                  |
| `for`       | C-style for loop                      |
| `forin`     | For-in loop over array                |
| `forinmap`  | For-in loop over map (key, value)     |
| `try`       | Try-catch block                       |
| `trycf`     | Try-catch-finally block               |
| `clone`     | `clone StructName`                    |
| `throw`     | `throw clone Exception("msg");`       |
| `_init`     | Constructor method                    |

## Requirements

- VS Code 1.85.0 or later
- Node.js + npm (for compilation)
