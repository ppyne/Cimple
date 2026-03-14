# Codex Implementation Prompt — Map type `V[K]`

## Context

Cimple is a small, statically typed, C-like language implemented in C11.
Pipeline: re2c lexer → Lemon parser → AST → semantic analysis → AST interpreter.

Read `SPECIFICATION.md §6.13` and `§9.15` for the full normative spec.
Read `MANUAL.md §3.2b` and `§8.17` for user-facing documentation and examples.

This task adds the `V[K]` map type to every layer of the compiler and runtime.

---

## 1. Overview of changes required

| File | What to add |
|------|-------------|
| `src/ast/ast.h` | `TYPE_MAP = 31`; extend `ParsedType`; add `NODE_MAP_LIT` |
| `src/ast/ast.c` | `ast_free` and `type_name` for new nodes/types |
| `src/runtime/value.h` | `MapVal`, `MapEntry` structs; `Value.u.map`; map API |
| `src/runtime/value.c` | Full map implementation: hash table, get/set/has/remove/keys/values/clear/copy/free |
| `src/lexer/lexer.re` | No new tokens — `COLON`, `LBRACKET`, `RBRACKET` already exist |
| `src/parser/parser.y` | `map_key_type` non-terminal; map type rules; map literal grammar |
| `src/parser/parser_helper.c` | No changes needed |
| `src/semantic/semantic.c` | Type-check map declarations, literals, index access, builtins |
| `src/interpreter/interpreter.c` | eval_expr and exec_stmt for map index access and map literals |
| `src/interpreter/builtins.c` | mapHas, mapRemove, mapKeys, mapValues, mapSize, mapClear; extend count() |
| `tests/` | Positive and negative tests (see §9) |

---

## 2. `src/ast/ast.h`

### 2.1 New `CimpleType` value

```c
TYPE_MAP        = 31,   /* V[K] map type */
/* TYPE_UNKNOWN remains at 30 */
```

### 2.2 Extend `ParsedType`

Add `key_type` field to carry the key type for map declarations:

```c
typedef struct {
    CimpleType type;
    char      *struct_name;
    FuncType  *func_type;
    CimpleType key_type;          /* TYPE_STRING/INT/BOOL for TYPE_MAP; TYPE_UNKNOWN otherwise */
    CimpleType inner_key_type;    /* for 2D map: key type of inner map; TYPE_UNKNOWN otherwise */
} ParsedType;
```

Add constructor helper alongside `parsed_type_make`:

```c
static ParsedType parsed_type_make_map(CimpleType val_type,
                                        const char *val_struct_name,
                                        CimpleType  key_type,
                                        CimpleType  inner_key_type) {
    ParsedType pt;
    pt.type             = TYPE_MAP;
    pt.struct_name      = val_struct_name ? cimple_strdup(val_struct_name) : NULL;
    pt.func_type        = NULL;
    pt.key_type         = key_type;
    pt.inner_key_type   = inner_key_type;
    return pt;
}
```

Ensure `parsed_type_make` initialises the new fields to `TYPE_UNKNOWN`/`NULL`.

### 2.3 New AST node `NODE_MAP_LIT`

```c
NODE_MAP_LIT,   /* ["k": v, ...] map literal */
```

Union member:

```c
/* NODE_MAP_LIT */
struct {
    NodeList keys;    /* expression nodes for keys   */
    NodeList values;  /* expression nodes for values */
} map_lit;
```

### 2.4 Update `type_name()` in `ast.c`

```c
case TYPE_MAP: return "map";
```

### 2.5 Update `ast_free()` in `ast.c`

```c
case NODE_MAP_LIT:
    nodelist_free(&n->u.map_lit.keys);
    nodelist_free(&n->u.map_lit.values);
    break;
```

---

## 3. `src/runtime/value.h` and `value.c`

### 3.1 `MapEntry` and `MapVal` structs

Add to `value.h` before `Value`:

```c
typedef struct MapEntry MapEntry;
struct MapEntry {
    /* key storage — one of the two is active depending on key_type */
    char    *key_str;   /* TYPE_STRING and TYPE_BOOL (as "true"/"false") */
    int64_t  key_int;   /* TYPE_INT and TYPE_BOOL (as 0/1) */
    Value    value;
    MapEntry *next;     /* chaining for hash collision */
};

#define MAP_INITIAL_BUCKETS 16

typedef struct {
    CimpleType  key_type;           /* TYPE_STRING, TYPE_INT, or TYPE_BOOL */
    CimpleType  val_type;           /* value element type */
    char       *val_struct_name;    /* non-NULL iff val_type == TYPE_STRUCT */
    CimpleType  inner_key_type;     /* non-UNKNOWN iff val_type == TYPE_MAP (2D) */
    MapEntry  **buckets;
    int         bucket_count;
    int         count;
} MapVal;
```

### 3.2 Extend the `Value` union

In `struct Value`, add inside the `union u`:

```c
MapVal *map;
```

### 3.3 Map API in `value.h`

```c
Value   val_map(MapVal *m);
MapVal *map_new(CimpleType key_type, CimpleType val_type,
                const char *val_struct_name, CimpleType inner_key_type);
void    map_free(MapVal *m);
MapVal *map_copy(const MapVal *m);

/* Returns a copy of the stored value, or zero-value of val_type if key absent.
   For TYPE_STRUCT val_type: returns val_void() — caller must clone from struct decl.
   For TYPE_MAP val_type (2D): returns val_map of a new empty inner map. */
Value   map_get(MapVal *m, Value key);

/* Stores value (takes ownership). Creates entry if absent. */
void    map_set(MapVal *m, Value key, Value value);

/* Returns 1 if key is present, 0 otherwise. */
int     map_has(MapVal *m, Value key);

/* Removes entry. No-op if absent. */
void    map_remove(MapVal *m, Value key);

/* Returns an ArrayVal* of keys (caller owns). */
ArrayVal *map_keys(MapVal *m);

/* Returns an ArrayVal* of values copies (caller owns). */
ArrayVal *map_values(MapVal *m);

void    map_clear(MapVal *m);
int     map_size(MapVal *m);

/* Returns pointer to the entry's value slot (creates entry if absent).
   Used by the interpreter for lvalue resolution (e.g. m["k"].field = v). */
Value  *map_get_or_create(MapVal *m, Value key);
```

### 3.4 Implementation notes for `value.c`

**Hashing:**
- For `TYPE_STRING` keys: djb2 hash of the string.
- For `TYPE_INT` and `TYPE_BOOL` keys: `(uint64_t)key_int % bucket_count`.

**Equality:**
- `TYPE_STRING`: `strcmp`.
- `TYPE_INT` / `TYPE_BOOL`: integer equality.

**Resize:** When `count > bucket_count * 0.75`, double the bucket count and rehash.

**`map_copy`:** Deep copy — copy each entry, deep-copy the value via `value_copy`.

**`value_free` / `value_copy`:** Extend both functions to handle `TYPE_MAP`:
```c
case TYPE_MAP:
    /* value_copy: */  v.u.map = map_copy(src.u.map); break;
    /* value_free: */  map_free(v->u.map); break;
```

**`map_keys` return type:** The key type of the returned `ArrayVal` is:
- `string[]` for `TYPE_STRING` keys.
- `int[]` for `TYPE_INT` and `TYPE_BOOL` keys.

**`map_get` zero values:**
```c
switch (m->val_type) {
    case TYPE_INT:    return val_int(0);
    case TYPE_FLOAT:  return val_float(0.0);
    case TYPE_BOOL:   return val_bool(0);
    case TYPE_STRING: return val_string("");
    case TYPE_BYTE:   return val_byte(0);
    case TYPE_MAP: {
        MapVal *inner = map_new(m->inner_key_type, /* needs inner val_type */...);
        /* For TYPE_MAP val, embed inner val_type inside MapVal — see §3.1 note below */
        return val_map(inner);
    }
    default: return val_void();   /* TYPE_STRUCT: caller handles */
}
```

**Note on nested-map zero values:** For 2D maps (`val_type == TYPE_MAP`), `map_get` needs to know the inner map's val_type. Extend `MapVal` with `inner_val_type` and `inner_val_struct_name` fields:

```c
CimpleType  inner_val_type;
char       *inner_val_struct_name;
```

These are populated at declaration time (the semantic pass knows both key types and the final value type).

---

## 4. `src/lexer/lexer.re`

**No changes required.** `COLON` (`:`) already exists (used in `case:` and `catch`), and `LBRACKET`/`RBRACKET` already exist.

---

## 5. `src/parser/parser.y`

### 5.1 Add `map_key_type` non-terminal

```lemon
%type map_key_type { CimpleType }

map_key_type(K) ::= KW_STRING.  { K = TYPE_STRING; }
map_key_type(K) ::= KW_INT.     { K = TYPE_INT;    }
map_key_type(K) ::= KW_BOOL.    { K = TYPE_BOOL;   }
```

### 5.2 Map type rules (1D)

Add rules for scalar value types — one rule per scalar × all key types:

```lemon
/* scalar value types with explicit key */
nonvoid_type(T) ::= KW_INT    LBRACKET map_key_type(K) RBRACKET. {
    T = parsed_type_make_map(TYPE_INT,    NULL, K, TYPE_UNKNOWN); }
nonvoid_type(T) ::= KW_FLOAT  LBRACKET map_key_type(K) RBRACKET. {
    T = parsed_type_make_map(TYPE_FLOAT,  NULL, K, TYPE_UNKNOWN); }
nonvoid_type(T) ::= KW_BOOL   LBRACKET map_key_type(K) RBRACKET. {
    T = parsed_type_make_map(TYPE_BOOL,   NULL, K, TYPE_UNKNOWN); }
nonvoid_type(T) ::= KW_STRING LBRACKET map_key_type(K) RBRACKET. {
    T = parsed_type_make_map(TYPE_STRING, NULL, K, TYPE_UNKNOWN); }
nonvoid_type(T) ::= KW_BYTE   LBRACKET map_key_type(K) RBRACKET. {
    T = parsed_type_make_map(TYPE_BYTE,   NULL, K, TYPE_UNKNOWN); }

/* struct value type */
%type struct_map_type { ParsedType }
struct_map_type(T) ::= TYPE_IDENT(N) LBRACKET map_key_type(K) RBRACKET. {
    T = parsed_type_make_map(TYPE_STRUCT, N.v.sval, K, TYPE_UNKNOWN);
    free(N.v.sval);
}
nonvoid_type(T) ::= struct_map_type(A). { T = A; }
```

### 5.3 Map type rules (2D)

```lemon
%type map2d_type { ParsedType }

/* scalar inner value */
map2d_type(T) ::= KW_INT    LBRACKET map_key_type(K1) RBRACKET LBRACKET map_key_type(K2) RBRACKET. {
    T = parsed_type_make_map(TYPE_INT,    NULL, K1, K2); }
map2d_type(T) ::= KW_FLOAT  LBRACKET map_key_type(K1) RBRACKET LBRACKET map_key_type(K2) RBRACKET. {
    T = parsed_type_make_map(TYPE_FLOAT,  NULL, K1, K2); }
map2d_type(T) ::= KW_BOOL   LBRACKET map_key_type(K1) RBRACKET LBRACKET map_key_type(K2) RBRACKET. {
    T = parsed_type_make_map(TYPE_BOOL,   NULL, K1, K2); }
map2d_type(T) ::= KW_STRING LBRACKET map_key_type(K1) RBRACKET LBRACKET map_key_type(K2) RBRACKET. {
    T = parsed_type_make_map(TYPE_STRING, NULL, K1, K2); }
map2d_type(T) ::= KW_BYTE   LBRACKET map_key_type(K1) RBRACKET LBRACKET map_key_type(K2) RBRACKET. {
    T = parsed_type_make_map(TYPE_BYTE,   NULL, K1, K2); }

/* struct inner value */
map2d_type(T) ::= TYPE_IDENT(N) LBRACKET map_key_type(K1) RBRACKET LBRACKET map_key_type(K2) RBRACKET. {
    T = parsed_type_make_map(TYPE_STRUCT, N.v.sval, K1, K2);
    free(N.v.sval);
}

nonvoid_type(T) ::= map2d_type(A). { T = A; }
```

**Note on `inner_key_type` interpretation for 2D maps:**
For `int[string][bool]`, the ParsedType is:
- `type = TYPE_MAP`, `key_type = TYPE_STRING` (outer key), `inner_key_type = TYPE_BOOL` (inner key), `val_type` (struct_name = NULL, val = TYPE_INT).

The semantic and interpreter layers use `inner_key_type != TYPE_UNKNOWN` to detect 2D maps.

### 5.4 Map literal grammar

```lemon
%type map_entry_list { struct { NodeList keys; NodeList values; } }

/* This is tricky to represent in Lemon's %type system.
   Use a dedicated AstNode approach instead: */

/* NODE_MAP_LIT is built inline */
expr(E) ::= LBRACKET map_entries(M) RBRACKET. {
    E = ast_new(NODE_MAP_LIT, /* line/col from token */ 0, 0);
    E->u.map_lit.keys   = M.keys;
    E->u.map_lit.values = M.values;
}

%type map_entries { struct { NodeList keys; NodeList values; } }

map_entries(M) ::= expr(K) COLON expr(V). {
    nodelist_init(&M.keys);
    nodelist_init(&M.values);
    nodelist_push(&M.keys,   K);
    nodelist_push(&M.values, V);
}
map_entries(M) ::= map_entries(ML) COMMA expr(K) COLON expr(V). {
    M = ML;
    nodelist_push(&M.keys,   K);
    nodelist_push(&M.values, V);
}
```

**Important:** The grammar already has `expr ::= LBRACKET expr_list RBRACKET` for array literals and `expr ::= LBRACKET RBRACKET` for empty. The `COLON` after the first `expr` is the disambiguator:
- After `[expr`, if next token is `COLON` → shift into `map_entries`.
- After `[expr`, if next token is `]` or `,` → it's an array element.

Lemon's default shift-over-reduce resolves this conflict correctly. Verify that no reduce/reduce conflicts appear; if they do, restructure the grammar to factor the common `[expr` prefix.

**Empty literal `[]`** remains `NODE_ARRAY_LIT` with zero elements (`elem_type = TYPE_UNKNOWN`). The semantic pass detects the context type and assigns it as empty array or empty map accordingly.

---

## 6. `src/semantic/semantic.c`

### 6.1 Map type validation in `check_var_decl` / `NODE_VAR_DECL`

When the declared type is `TYPE_MAP`:

1. **Key type check:** `key_type` must be `TYPE_STRING`, `TYPE_INT`, or `TYPE_BOOL`. Otherwise:
   ```
   error_semantic(line, col, "map key type must be string, int, or bool; got '%s'", ...)
   ```

2. **`float` key explicitly forbidden:**
   ```
   error_semantic(line, col, "float map key type is not allowed")
   ```

3. **Struct key forbidden:**
   ```
   error_semantic(line, col, "struct and union types cannot be used as map keys")
   ```

4. **`void` value type:**
   ```
   error_semantic(line, col, "void map value type is not allowed")
   ```

5. **Array-of-maps forbidden** (`V[K][]`): This cannot be expressed in the grammar as defined, so no explicit check is needed — it simply won't parse.

6. **3D maps:** If a `map2d_type` has `inner_key_type != TYPE_UNKNOWN`, validate neither K1 nor K2 is float/struct. 3D maps cannot be expressed in the grammar as defined.

7. **Empty literal `[]` as map initialiser:**
   When `NODE_ARRAY_LIT` with no elements is used to initialise a `TYPE_MAP` variable, accept it as an empty map. Set `n->u.array_lit.elem_type` to `TYPE_MAP` (or use a dedicated flag) so the interpreter can distinguish.

### 6.2 Map literal `NODE_MAP_LIT`

```c
case NODE_MAP_LIT: {
    /* Infer expected key/value types from context (the surrounding var_decl type). */
    /* Check all key exprs match expected key_type. */
    /* Check all value exprs match expected val_type. */
    /* Check for duplicate literal keys (string keys only — int keys checked at runtime). */
    /* Set n->inferred_type = TYPE_MAP with key/val info stored in type_name_hint or a new field. */
}
```

The semantic pass needs access to the "expected type" from the surrounding `NODE_VAR_DECL`. Thread this via the `SemCtx` (e.g., add `expected_map_key_type` / `expected_map_val_type` fields set before calling `check_expr` on the initialiser).

### 6.3 Map index read (`NODE_INDEX` on a map)

When `base` has type `TYPE_MAP`:
- `index` type must match `m->key_type`.
- Result type is `m->val_type` (with `struct_name` if struct value).

### 6.4 Map index assign (`NODE_INDEX_ASSIGN` on a map)

Same type checks as read. The value expression type must match `m->val_type`.

### 6.5 2D map index read/write

For `m[k1][k2]`:
- Outer index type = `outer_key_type`.
- Inner index type = `inner_key_type`.
- For `NODE_INDEX2_ASSIGN`: same checks.

### 6.6 Reserved built-in function names

The names `mapHas`, `mapRemove`, `mapKeys`, `mapValues`, `mapSize`, `mapClear` are reserved. Declaring a user function with one of these names is a **semantic error**.

### 6.7 Built-in function signatures

Register in the builtin table:

```c
{ "mapHas",    BI_MAP_HAS    },
{ "mapRemove", BI_MAP_REMOVE },
{ "mapKeys",   BI_MAP_KEYS   },
{ "mapValues", BI_MAP_VALUES },
{ "mapSize",   BI_MAP_SIZE   },
{ "mapClear",  BI_MAP_CLEAR  },
```

Argument/return type checking in the semantic pass:
- `mapHas(V[K] m, K key) → bool`
- `mapRemove(V[K] m, K key) → void`
- `mapKeys(V[K] m) → K[]`
- `mapValues(V[K] m) → V[]`
- `mapSize(V[K] m) → int`
- `mapClear(V[K] m) → void`
- `count(V[K] m) → int` — extend existing count() to also accept maps.

---

## 7. `src/interpreter/interpreter.c`

### 7.1 `NODE_MAP_LIT` evaluation

```c
case NODE_MAP_LIT: {
    /* Get expected types from the node's type_info or from context. */
    MapVal *m = map_new(expected_key_type, expected_val_type,
                        expected_val_struct_name, expected_inner_key_type);
    for (int i = 0; i < n->u.map_lit.keys.count; i++) {
        Value k = eval_expr(ip, scope, n->u.map_lit.keys.items[i]);
        if (interp_has_throw(ip)) { map_free(m); value_free(&k); return val_void(); }
        Value v = eval_expr(ip, scope, n->u.map_lit.values.items[i]);
        if (interp_has_throw(ip)) { map_free(m); value_free(&k); value_free(&v); return val_void(); }
        map_set(m, k, v);
        value_free(&k);   /* map_set copied key internally */
        value_free(&v);   /* map_set took ownership of value */
    }
    return val_map(m);
}
```

**Type info on `NODE_MAP_LIT`:** The semantic pass must annotate the literal node with the expected key/value types. Add fields `map_key_type` and `map_val_type` and `map_val_struct_name` to the `NODE_MAP_LIT` union struct in `ast.h`, and populate them in `semantic.c`.

### 7.2 `NODE_INDEX` (read) on a map

In `eval_expr` `NODE_INDEX` case, after evaluating `base`:

```c
if (base.type == TYPE_MAP) {
    Value idx = eval_expr(ip, scope, n->u.index.index);
    if (interp_has_throw(ip)) { value_free(&base); value_free(&idx); return val_void(); }
    Value result = map_get(base.u.map, idx);
    if (result.type == TYPE_VOID && base.u.map->val_type == TYPE_STRUCT) {
        /* struct zero value: clone */
        result = clone_struct_instance(ip, base.u.map->val_struct_name, scope,
                                       n->line, n->col);
    }
    value_free(&base);
    value_free(&idx);
    return result;
}
/* existing array path follows */
```

### 7.3 Lvalue resolution for maps (`resolve_value_lvalue`)

In `NODE_INDEX` case of `resolve_value_lvalue`, after checking base type:

```c
if (base->type == TYPE_MAP) {
    Value idx = eval_expr(ip, scope, n->u.index.index);
    if (interp_has_throw(ip)) { value_free(&idx); return NULL; }
    Value *slot = map_get_or_create(base->u.map, idx);
    value_free(&idx);
    return slot;
}
```

`map_get_or_create` creates an entry with the zero value if absent, and returns a pointer to `entry->value`. This makes `m["k"].field = v` work correctly.

### 7.4 `NODE_INDEX_ASSIGN` on a map

In `exec_stmt` `NODE_INDEX_ASSIGN`:

```c
Value base_val = eval_expr(ip, scope, n->u.index_assign.base_or_name...);
if (base_val.type == TYPE_MAP) {
    Value key = eval_expr(ip, scope, n->u.index_assign.index);
    if (interp_has_throw(ip)) { ... }
    Value val = eval_expr(ip, scope, n->u.index_assign.value);
    if (interp_has_throw(ip)) { ... }
    map_set(base_val.u.map, key, val);
    value_free(&key);
    value_free(&val);
    value_free(&base_val);
    break;
}
/* existing array path */
```

### 7.5 `NODE_INDEX2_ASSIGN` for 2D maps

Extend the existing `NODE_INDEX2_ASSIGN` handler:

```c
case NODE_INDEX2_ASSIGN: {
    Symbol *sym = scope_lookup(scope, n->u.index2_assign.name);
    /* ... error checks ... */
    Value i1v = eval_expr(...index1...);
    Value i2v = eval_expr(...index2...);
    Value val = eval_expr(...value...);
    /* check for throws */

    if (sym->val.type == TYPE_MAP) {
        /* 2D map: outer[i1] gives inner map, then inner[i2] = val */
        Value *inner_slot = map_get_or_create(sym->val.u.map, i1v);
        if (inner_slot->type != TYPE_MAP) {
            /* create inner map */
            MapVal *inner = map_new(sym->val.u.map->inner_key_type,
                                    sym->val.u.map->inner_val_type,
                                    sym->val.u.map->inner_val_struct_name,
                                    TYPE_UNKNOWN);
            value_free(inner_slot);
            *inner_slot = val_map(inner);
        }
        map_set(inner_slot->u.map, i2v, val);
    } else {
        /* existing 2D array path */
        ...
    }
    value_free(&i1v); value_free(&i2v); value_free(&val);
    break;
}
```

---

## 8. `src/interpreter/builtins.c`

Add to the builtin enum:

```c
BI_MAP_HAS,
BI_MAP_REMOVE,
BI_MAP_KEYS,
BI_MAP_VALUES,
BI_MAP_SIZE,
BI_MAP_CLEAR,
```

Implementation pattern (same as existing builtins):

```c
if (id == BI_MAP_HAS) {
    /* args[0] = map, args[1] = key */
    int found = map_has(args[0].u.map, args[1]);
    return val_bool(found);
}
if (id == BI_MAP_REMOVE) {
    map_remove(args[0].u.map, args[1]);
    return val_void();
}
if (id == BI_MAP_KEYS) {
    ArrayVal *arr = map_keys(args[0].u.map);
    return val_array(arr);
}
if (id == BI_MAP_VALUES) {
    ArrayVal *arr = map_values(args[0].u.map);
    return val_array(arr);
}
if (id == BI_MAP_SIZE) {
    return val_int(map_size(args[0].u.map));
}
if (id == BI_MAP_CLEAR) {
    map_clear(args[0].u.map);
    return val_void();
}
```

Extend the existing `count()` builtin to also handle `TYPE_MAP`:

```c
if (id == BI_COUNT) {
    if (args[0].type == TYPE_MAP)
        return val_int(map_size(args[0].u.map));
    /* existing array path */
}
```

---

## 9. Tests

### 9.1 Positive tests — directory `tests/positive/maps/`

#### `basic/` — declaration, write, read, zero value

```c
void main() {
    int[string] m;
    m["alice"] = 10;
    m["bob"]   = 7;
    print(toString(m["alice"]) + "\n");   // 10
    print(toString(m["bob"])   + "\n");   // 7
    print(toString(m["carol"]) + "\n");   // 0  (absent key → zero value)
}
```
`expected_stdout`: `10\n7\n0\n`

#### `literal/` — map literal

```c
void main() {
    int[string] scores = ["alice": 10, "bob": 7, "carol": 3];
    print(toString(scores["alice"]) + "\n");   // 10
    print(toString(mapSize(scores)) + "\n");   // 3
}
```

#### `word_count/` — realistic use case

```c
void main() {
    string[] words = ["the", "cat", "the", "mat", "cat", "the"];
    int[string] freq;
    for (int i = 0; i < count(words); i++) {
        freq[words[i]]++;
    }
    print(toString(freq["the"]) + "\n");   // 3
    print(toString(freq["cat"]) + "\n");   // 2
    print(toString(freq["mat"]) + "\n");   // 1
}
```

#### `functions/` — mapHas, mapRemove, mapKeys, mapValues, mapSize, mapClear

```c
void main() {
    int[string] m = ["a": 1, "b": 2, "c": 3];
    print(toString(mapHas(m, "a"))    + "\n");   // true
    print(toString(mapHas(m, "z"))    + "\n");   // false
    print(toString(mapSize(m))        + "\n");   // 3
    mapRemove(m, "b");
    print(toString(mapSize(m))        + "\n");   // 2
    mapRemove(m, "z");                            // no error
    print(toString(mapSize(m))        + "\n");   // 2
    mapClear(m);
    print(toString(mapSize(m))        + "\n");   // 0
}
```

#### `int_keys/` — int key type

```c
void main() {
    string[int] labels;
    labels[0] = "zero";
    labels[1] = "one";
    labels[5] = "five";
    print(labels[0] + "\n");   // zero
    print(labels[3] + "\n");   // ""  (absent)
    print(toString(mapSize(labels)) + "\n");   // 3
}
```

#### `struct_values/` — struct as value type

```c
structure Point { float x = 0.0; float y = 0.0; }

void main() {
    Point[string] pts;
    pts["origin"].x = 1.0;
    pts["origin"].y = 2.0;
    print(toString(pts["origin"].x) + "\n");   // 1.0
    print(toString(pts["missing"].x) + "\n");  // 0.0
}
```

#### `map2d/` — 2D maps

```c
void main() {
    int[string][string] grid;
    grid["r1"]["c1"] = 1;
    grid["r1"]["c2"] = 2;
    grid["r2"]["c1"] = 3;
    print(toString(grid["r1"]["c2"]) + "\n");   // 2
    print(toString(grid["r3"]["c1"]) + "\n");   // 0  (both keys absent)
}
```

#### `empty_literal/` — `[]` as map initialiser

```c
void main() {
    int[string] m = [];
    m["x"] = 99;
    print(toString(mapSize(m)) + "\n");    // 1
    print(toString(m["x"])     + "\n");    // 99
}
```

#### `count_alias/` — count() works on maps

```c
void main() {
    int[string] m = ["a": 1, "b": 2];
    print(toString(count(m)) + "\n");    // 2
}
```

### 9.2 Negative tests — directory `tests/negative/maps/`

#### `float_key/` — float key type is forbidden

```c
void main() { int[float] m; }
```
`expected_stderr`: `float map key type is not allowed`

#### `struct_key/` — struct key type is forbidden

```c
structure P { int x = 0; }
void main() { int[P] m; }
```
`expected_stderr`: `map key type must be`

#### `type_mismatch_literal/` — wrong value type in literal

```c
void main() {
    int[string] m = ["a": "hello"];
}
```
`expected_stderr` contains: `type mismatch`

#### `duplicate_key/` — duplicate key in literal

```c
void main() {
    int[string] m = ["a": 1, "a": 2];
}
```
`expected_stderr` contains: `duplicate key`

#### `array_of_maps/` — array of maps is forbidden

This cannot be expressed in the grammar as defined (grammar doesn't produce `V[K][]`), so no test is needed. If the grammar accidentally allows it, add a test.

#### `map_literal_in_array_ctx/` — map literal where array expected

```c
void main() {
    int[] a = ["x": 1];
}
```
`expected_stderr` contains: `type mismatch` or `map literal`

---

## 10. Important invariants

1. **`V[]` and `V[int]` are distinct types.** `V[]` is a dense array (existing). `V[int]` is a sparse map. The semantic pass must reject assigning one to the other.

2. **No array iteration functions on maps.** `arrayPush`, `arrayPop`, `arrayInsert`, `arrayRemove`, `arrayGet`, `arraySet` must reject map arguments.

3. **Thread safety of `g_current_interp`.** Already a concern; no new issues here.

4. **`value_free` on `TYPE_MAP` must be complete.** Every `MapVal` created must eventually be freed. Follow the same ownership model as `ArrayVal`.

5. **`map_get` never inserts.** Only `map_set` and `map_get_or_create` insert. Reading a missing key never modifies the map.

6. **`mapKeys` / `mapValues` order consistency.** If called on the same map without modification in between, the i-th key corresponds to the i-th value. Implement by iterating buckets in the same deterministic order.

7. **Struct value zero-value auto-clone.** When `map_get` returns `val_void()` for a `TYPE_STRUCT` value, the interpreter must clone a fresh instance rather than returning a void. This avoids null pointer dereferences on field access.

8. **After every `error_runtime` call, return early.** Follow the existing post-exception safety pattern — check `interp_has_throw(ip)` after every map operation that could throw.

---

## 11. Commit message

```
feat: implement map type V[K] — declaration, literals, stdlib functions

Add map<key→value> collections with V[K] syntax.
- Type system: TYPE_MAP, ParsedType.key_type/inner_key_type
- Runtime: MapVal hash table (djb2, chaining, auto-resize)
- Grammar: map_key_type non-terminal, map type rules (1D + 2D),
           NODE_MAP_LIT for ["k": v] literals
- Semantic: key type validation, literal type-check, duplicate key
            detection, builtin registration, reserved name guard
- Interpreter: NODE_MAP_LIT eval, map index read/write, lvalue
               resolution (map_get_or_create for field access),
               2D map auto-create of inner map on write
- Builtins: mapHas, mapRemove, mapKeys, mapValues, mapSize,
            mapClear, count() extended to maps
- Key rules: V[] ≠ V[int]; float keys forbidden; missing key → zero
             value on read (never inserts); mapRemove idempotent
- Tests: 9 positive, 4 negative; full suite must pass
```
