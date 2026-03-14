# Codex prompt — Implémentation des exceptions dans Cimple

## Contexte

Cimple est un langage C-like statiquement typé, interprété via un AST en C11.
Pipeline : re2c → Lemon → AST → analyse sémantique → interpréteur.

Dépôt : `/Users/avialle/dev/Cimple/`
Build : `cd build && cmake --build .`
Tests : `CIMPLE_BIN=./cimple bash ../tests/run_tests.sh`

---

## Objectif

Ajouter les exceptions selon la spécification (`SPECIFICATION.md §6.12`, `§10.11`,
`§10.12`, `§19.13`, `§20.5`).
**Ne pas dévier de la spec.** Résumé des règles normatives :

- Hiérarchie built-in : `Exception { string message; }` ← base
  `RuntimeException : Exception`, `IoException : Exception { string path; }`,
  `RegExpException : Exception`
- L'utilisateur peut définir `structure Foo : Exception { … }` mais **ne peut pas**
  hériter de `RuntimeException`, `IoException` ou `RegExpException`.
- `throw expr` : `expr` doit être un type exception (hérite de `Exception`).
- `try { … } catch (TypeX nom) { … } catch (TypeY nom) { … }` — au moins un catch.
- Clauses testées dans l'ordre ; première compatible choisie.
- `catch (Exception e)` avant une clause plus spécifique → **erreur sémantique**.
- Variables du bloc `try` non visibles dans `catch`.
- Exception non rattrapée → stderr `Runtime error: <TypeName>: <message>`, exit 2.
- `break`/`continue`/`return` dans `try` fonctionnent normalement.
- Pas de `finally` en V1.

---

## Architecture du mécanisme d'exception

### Principe : signal `SIGNAL_THROW`

Le mécanisme existant utilise déjà une enum `Signal` dans `interpreter.h` :

```c
typedef enum {
    SIGNAL_NONE     = 0,
    SIGNAL_RETURN   = 1,
    SIGNAL_BREAK    = 2,
    SIGNAL_CONTINUE = 3
} Signal;
```

Ajouter :

```c
    SIGNAL_THROW    = 4
```

Ajouter dans la structure `Interp` :

```c
    Value       throw_val;        /* objet exception levé (StructVal)          */
    char       *throw_type_name;  /* nom du type dynamique (strdup, à libérer) */
```

Ajouter un pointeur global (dans `interpreter.c`, déclaré `extern` dans `interpreter.h`) :

```c
Interp *g_current_interp = NULL;
```

Ce pointeur est mis à jour en début/fin de `interp_run`.

### `error_runtime` modifié

`src/common/error.c` — `error_runtime` ne doit plus appeler `exit(2)` quand
l'interpréteur est actif. À la place :

```c
extern Interp *g_current_interp;  // déclaré dans interpreter.c

void error_runtime(int line, int col, const char *fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);

    if (g_current_interp && g_current_interp->signal == SIGNAL_NONE) {
        /* Throw a RuntimeException */
        interp_throw_builtin(g_current_interp, "RuntimeException", buf, NULL);
        return;   /* propagation par signal */
    }
    error_print(ERR_RUNTIME, line, col, buf, NULL);
    exit(2);
}
```

Même logique pour `error_runtime_hint` (throw `RuntimeException`).

Ajouter dans `interpreter.c` (et déclarer dans `interpreter.h`) :

```c
/* Construit un StructVal built-in et lève SIGNAL_THROW */
void interp_throw_builtin(Interp *ip, const char *type_name,
                          const char *message, const char *path);
```

`interp_throw_builtin` :
- Crée un `StructVal` avec les champs appropriés (voir table ci-dessous).
- Appelle `interp_throw(ip, type_name, val)`.

```c
void interp_throw(Interp *ip, const char *type_name, Value val) {
    if (ip->signal != SIGNAL_NONE) return; /* ne pas écraser un signal existant */
    ip->signal          = SIGNAL_THROW;
    ip->throw_val       = val;
    ip->throw_type_name = cimple_strdup(type_name);
}
```

### Table des champs des exceptions built-in

| Type | Champs |
|------|--------|
| `RuntimeException` | `message` (string) |
| `IoException` | `message` (string), `path` (string) |
| `RegExpException` | `message` (string) |

Ces types n'ont pas d'entrée dans `struct_decls` — leur existence est connue du
sémantique via une table statique de noms réservés (voir §19 ci-dessous).

---

## Fichiers à modifier

### 1. `src/ast/ast.h`

Ajouter dans `NodeKind` (avant `NODE_TERNARY`) :

```c
    NODE_THROW,          /* throw expr                                  */
    NODE_TRY_CATCH,      /* try { } catch (T n) { } …                  */
    NODE_CATCH_CLAUSE,   /* (TypeName varname) { block }                */
```

Ajouter dans l'union `u` :

```c
        /* NODE_THROW */
        struct {
            AstNode *expr;
        } throw_stmt;

        /* NODE_TRY_CATCH */
        struct {
            AstNode  *try_block;    /* NODE_BLOCK                        */
            NodeList  clauses;      /* list of NODE_CATCH_CLAUSE         */
        } try_catch;

        /* NODE_CATCH_CLAUSE */
        struct {
            char    *type_name;     /* "IoException", "ParseError", …    */
            char    *var_name;      /* bound variable name               */
            AstNode *block;         /* NODE_BLOCK                        */
        } catch_clause;
```

### 2. `src/ast/ast.c`

**`ast_free`** : ajouter les cas pour `NODE_THROW`, `NODE_TRY_CATCH`, `NODE_CATCH_CLAUSE`
(libérer les champs `char *` et les nœuds fils récursivement).

### 3. `src/lexer/lexer.re`

Ajouter trois mots-clés dans la règle des identifiants :

```c
"throw"   { EMIT(KW_THROW);   continue; }
"try"     { EMIT(KW_TRY);     continue; }
"catch"   { EMIT(KW_CATCH);   continue; }
```

(Suivre exactement le même patron que `"return"`, `"break"`, etc.)

### 4. `src/parser/parser.y`

**Déclarations de type :**

```lemon
%type catch_clause_list { NodeList }
%type catch_clause      { AstNode * }
```

**Tokens :**

```lemon
%token KW_THROW KW_TRY KW_CATCH.
```

**Règle `throw` (dans `stmt` ET `simple_stmt`) :**

```lemon
stmt(S) ::= KW_THROW(T) expr(E) SEMICOLON.
{
    S = ast_new(NODE_THROW, T.line, T.col);
    S->u.throw_stmt.expr = E;
}
```

**Règle `try/catch` (dans `stmt` seulement — pas `simple_stmt`) :**

```lemon
stmt(S) ::= KW_TRY(T) block(B) catch_clause_list(CS).
{
    S = ast_new(NODE_TRY_CATCH, T.line, T.col);
    S->u.try_catch.try_block = B;
    S->u.try_catch.clauses   = CS;
}

catch_clause_list(L) ::= catch_clause_list(L0) catch_clause(C).
{
    L = L0;
    nodelist_push(&L, C);
}
catch_clause_list(L) ::= catch_clause(C).
{
    nodelist_init(&L);
    nodelist_push(&L, C);
}

catch_clause(C) ::= KW_CATCH(T) LPAREN TYPE_IDENT(N) IDENT(V) RPAREN block(B).
{
    C = ast_new(NODE_CATCH_CLAUSE, T.line, T.col);
    C->u.catch_clause.type_name = cimple_strdup(N.v.sval); free(N.v.sval);
    C->u.catch_clause.var_name  = cimple_strdup(V.v.sval); free(V.v.sval);
    C->u.catch_clause.block     = B;
}
```

> **Note :** `Exception` est un `TYPE_IDENT` connu grâce à la pré-collecte existante.
> Les quatre noms réservés doivent être enregistrés comme `TYPE_IDENT` built-in au
> même titre que les structures déclarées.

### 5. `src/semantic/semantic.c`

#### 5a. Noms réservés et table statique

En début de `semantic_check`, enregistrer les quatre types built-in dans la
`StructTable` avec les informations de base :

```c
static const struct { const char *name; const char *base; } BUILTIN_EXCEPTIONS[] = {
    { "Exception",        NULL          },
    { "RuntimeException", "Exception"   },
    { "IoException",      "Exception"   },
    { "RegExpException",  "Exception"   },
};
```

Pour chacun, appeler `struct_table_register(ctx->structs, name, base_name, NULL)`
(NULL pour le nœud AST puisque ce sont des types built-in).

Enregistrer également `Exception`, `RuntimeException`, `IoException`, `RegExpException`
comme `TYPE_IDENT` connus dans la pré-collecte (passe de pré-scan du parseur) —
ou injecter les noms directement dans la table des types connus avant le parse.

#### 5b. Vérification des déclarations de structures

Dans `check_struct_decl` :

- Si le nom de la structure est l'un des quatre noms réservés → **erreur sémantique** :
  `"'Exception' is a reserved name"` (adapter selon le nom).
- Si `base_name` est `RuntimeException`, `IoException` ou `RegExpException` →
  **erreur sémantique** : `"Cannot inherit from built-in exception type '%s'"`.

#### 5c. Helper : `is_exception_type`

```c
static int is_exception_type(SemanticCtx *ctx, const char *name) {
    if (!name) return 0;
    return struct_is_subtype(ctx->structs, name, "Exception");
}
```

#### 5d. `NODE_THROW`

```c
case NODE_THROW: {
    CimpleType t = check_expr(ctx, n->u.throw_stmt.expr);
    const char *tname = n->u.throw_stmt.expr->type_name_hint;
    if (t != TYPE_STRUCT || !is_exception_type(ctx, tname))
        error_semantic(n->line, n->col,
            "throw requires a value of type Exception or a subtype");
    break;
}
```

#### 5e. `NODE_TRY_CATCH`

```c
case NODE_TRY_CATCH: {
    /* Vérifier le bloc try */
    check_stmt(ctx, n->u.try_catch.try_block);

    /* Au moins un catch (erreur syntaxique normalement, mais défense en profondeur) */
    if (n->u.try_catch.clauses.count == 0)
        error_semantic(n->line, n->col, "try requires at least one catch clause");

    /* Vérifier chaque clause ; détecter les clauses inaccessibles */
    int has_catchall = 0;
    for (int i = 0; i < n->u.try_catch.clauses.count; i++) {
        AstNode *cl = n->u.try_catch.clauses.nodes[i];
        const char *tname = cl->u.catch_clause.type_name;

        if (has_catchall)
            error_semantic(cl->line, cl->col,
                "unreachable catch clause after catch (Exception)");

        if (!is_exception_type(ctx, tname))
            error_semantic(cl->line, cl->col,
                "'%s' is not an exception type", tname);

        /* Vérifier que cette clause n'est pas masquée par une précédente */
        for (int j = 0; j < i; j++) {
            const char *prev = n->u.try_catch.clauses.nodes[j]->u.catch_clause.type_name;
            if (struct_is_subtype(ctx->structs, tname, prev))
                error_semantic(cl->line, cl->col,
                    "unreachable catch clause: '%s' is a subtype of '%s'", tname, prev);
        }

        if (strcmp(tname, "Exception") == 0) has_catchall = 1;

        /* Vérifier le bloc catch dans une sous-portée avec `nom` déclaré */
        Scope *catch_scope = scope_push(ctx->current);
        scope_declare(catch_scope, cl->u.catch_clause.var_name,
                      TYPE_STRUCT, tname, cl->line, cl->col);
        ctx->current = catch_scope;
        check_stmt(ctx, cl->u.catch_clause.block);
        ctx->current = scope_pop(ctx->current);
    }
    break;
}
```

### 6. `src/interpreter/interpreter.c`

#### 6a. `interp_throw_builtin`

Construire un `StructVal` avec le bon nombre de champs.
Les champs des built-in sont connus statiquement :

| Type | Champs (dans l'ordre) |
|------|-----------------------|
| `RuntimeException` | `message` |
| `IoException` | `message`, `path` |
| `RegExpException` | `message` |

```c
void interp_throw_builtin(Interp *ip, const char *type_name,
                          const char *message, const char *path) {
    int nfields = (strcmp(type_name, "IoException") == 0) ? 2 : 1;
    Value v = val_struct(type_name, nfields);
    v.u.sv->field_names[0] = cimple_strdup("message");
    v.u.sv->fields[0]      = val_string(message ? message : "");
    if (nfields == 2) {
        v.u.sv->field_names[1] = cimple_strdup("path");
        v.u.sv->fields[1]      = val_string(path ? path : "");
    }
    interp_throw(ip, type_name, v);
}
```

#### 6b. `NODE_THROW` dans `exec_stmt`

```c
case NODE_THROW: {
    Value v = eval_expr(ip, scope, n->u.throw_stmt.expr);
    if (ip->signal != SIGNAL_THROW) {
        const char *tname = n->u.throw_stmt.expr->type_name_hint;
        interp_throw(ip, tname ? tname : "Exception", v);
    } else {
        value_free(&v);
    }
    break;
}
```

#### 6c. `NODE_TRY_CATCH` dans `exec_stmt`

```c
case NODE_TRY_CATCH: {
    /* Exécuter le bloc try */
    exec_stmt(ip, scope, n->u.try_catch.try_block);

    if (ip->signal != SIGNAL_THROW) break;  /* pas d'exception : continuer */

    /* Une exception a été levée : chercher une clause compatible */
    Value  thrown     = ip->throw_val;
    char  *thrown_tname = ip->throw_type_name;
    /* Réinitialiser le signal AVANT de tester les clauses */
    ip->signal          = SIGNAL_NONE;
    ip->throw_type_name = NULL;

    int handled = 0;
    for (int i = 0; i < n->u.try_catch.clauses.count; i++) {
        AstNode *cl = n->u.try_catch.clauses.nodes[i];
        const char *catch_type = cl->u.catch_clause.type_name;

        /* compatible si thrown_tname == catch_type ou sous-type */
        int match = (strcmp(thrown_tname, catch_type) == 0);
        if (!match) {
            /* vérifier l'héritage via la table de structs du programme */
            /* (appeler struct_is_subtype sur les structs runtime) */
            match = interp_struct_is_subtype(ip, thrown_tname, catch_type);
        }
        if (!match) continue;

        /* Clause trouvée : créer une portée, lier la variable */
        Scope *catch_scope = scope_push(scope);
        scope_declare_val(catch_scope, cl->u.catch_clause.var_name,
                          TYPE_STRUCT, thrown_tname, thrown, cl->line, cl->col);
        exec_stmt(ip, catch_scope, cl->u.catch_clause.block);
        scope_pop_free(catch_scope);
        value_free(&thrown);
        handled = 1;
        break;
    }
    free(thrown_tname);

    if (!handled) {
        /* Aucune clause compatible : re-propager */
        ip->signal          = SIGNAL_THROW;
        ip->throw_val       = thrown;
        ip->throw_type_name = cimple_strdup(thrown_tname ? thrown_tname : "Exception");
        /* thrown_tname déjà libéré — utiliser la copie faite ci-dessus */
    }
    break;
}
```

> **`interp_struct_is_subtype`** : fonction wrapper qui utilise la `StructDeclTable`
> de l'interpréteur pour remonter la chaîne `base_name`, identique à
> `struct_is_subtype` du sémantique mais sur les données runtime.

#### 6d. Propagation de `SIGNAL_THROW` dans `exec_stmt`

Partout où `ip->signal` est testé pour `SIGNAL_RETURN` ou `SIGNAL_BREAK`, ajouter
`SIGNAL_THROW` dans les mêmes guards :

- Boucles `while` / `for` : `if (ip->signal) break;`
  (le signal THROW remonte naturellement puisqu'on break du loop et qu'on revient
  dans le parent qui vérifie lui aussi)
- Blocs séquentiels : `if (ip->signal) return;`

Pattern à appliquer partout où le code fait :
```c
if (ip->signal == SIGNAL_RETURN || ip->signal == SIGNAL_BREAK) break;
```
→ remplacer par :
```c
if (ip->signal) break;
```
(SIGNAL_NONE == 0, donc `if (ip->signal)` est vrai pour tout signal non nul)

#### 6e. `eval_expr` — propagation après chaque sous-évaluation

Après chaque appel à `eval_expr` dans `eval_expr`, vérifier :
```c
if (ip->signal == SIGNAL_THROW) {
    /* libérer les valeurs intermédiaires déjà allouées */
    value_free(&left);   /* si applicable */
    return val_int(0);   /* valeur poison non utilisée */
}
```

Cas critiques à couvrir : NODE_BINOP (après eval left, avant eval right),
NODE_CALL (après eval de chaque argument), NODE_INDEX (après eval base et index),
NODE_MEMBER_CALL (après eval objet).

#### 6f. Exception non rattrapée dans `interp_run`

À la fin de `interp_run`, après l'appel à `exec_stmt(ip, ..., program)` :

```c
if (ip->signal == SIGNAL_THROW) {
    const char *tname = ip->throw_type_name ? ip->throw_type_name : "Exception";
    const char *msg   = "";
    /* extraire le champ message si c'est un StructVal */
    if (ip->throw_val.type == TYPE_STRUCT && ip->throw_val.u.sv) {
        for (int i = 0; i < ip->throw_val.u.sv->field_count; i++) {
            if (strcmp(ip->throw_val.u.sv->field_names[i], "message") == 0) {
                msg = ip->throw_val.u.sv->fields[i].u.s;
                break;
            }
        }
    }
    fprintf(stderr, "Runtime error: %s: %s\n", tname, msg);
    value_free(&ip->throw_val);
    free(ip->throw_type_name);
    return 2;
}
```

### 7. `src/interpreter/builtins.c`

Les fonctions de fichiers (`BI_READ_FILE`, `BI_WRITE_FILE`, etc.) appellent
actuellement `error_runtime(...)` qui appelait `exit(2)`.
Après la modification de `error_runtime`, elles appellent `interp_throw_builtin`
via le signal — **aucune modification directe requise** dans `builtins.c` pour les
fonctions fichier, puisque `error_runtime` fait maintenant la redirection.

Cependant, pour `IoException` (qui a un champ `path`), `error_runtime` ordinaire
ne connaît pas le chemin. Il faut remplacer les appels `error_runtime` dans les
fonctions fichier par un appel explicite :

```c
interp_throw_builtin(g_current_interp, "IoException", message, path);
return val_int(0);  /* valeur poison */
```

(Vérifier d'abord `if (g_current_interp)` ; sinon faire `exit(2)` comme avant.)

Pour `BI_REGEX_*`, remplacer de même les `error_runtime` par
`interp_throw_builtin(g_current_interp, "RegExpException", message, NULL)`.

**`BI_ASSERT`** : remplacer `error_runtime` par
`interp_throw_builtin(g_current_interp, "RuntimeException", "Assertion failed", NULL)`.

### 8. `src/runtime/value.c`

`array_get`, `array_set`, `array_pop` appellent `error_runtime`. Après la modification
de `error_runtime`, ils doivent retourner une valeur poison si `g_current_interp`
a le signal SIGNAL_THROW :

```c
/* Après error_runtime(...) : */
if (g_current_interp && g_current_interp->signal == SIGNAL_THROW)
    return val_int(0);
exit(2);  /* ne devrait jamais être atteint */
```

---

## Tests à créer

### `tests/positive/exceptions/basic/`

**`input.ci`** :
```c
function main() : void {
    try {
        int[] a = [1, 2, 3];
        int v = a[10];
        print("no exception\n");
    } catch (RuntimeException e) {
        print("caught: " + e.message + "\n");
    }
}
```
**`expected_stdout`** : `caught: Index 10 out of bounds (size 3)\n`
**`expected_exit`** : `0`

---

### `tests/positive/exceptions/custom_type/`

**`input.ci`** :
```c
structure ParseError : Exception {
    int line;
}

function parse(string s) : void {
    if (s == "") {
        throw ParseError { message: "empty input", line: 1 };
    }
}

function main() : void {
    try {
        parse("");
    } catch (ParseError e) {
        print("parse error line " + toString(e.line) + ": " + e.message + "\n");
    } catch (Exception e) {
        print("other: " + e.message + "\n");
    }
}
```
**`expected_stdout`** : `parse error line 1: empty input\n`
**`expected_exit`** : `0`

---

### `tests/positive/exceptions/io_catch/`

**`input.ci`** :
```c
function main() : void {
    try {
        string s = readFile("/nonexistent_cimple_test_file_xyz.txt");
        print(s);
    } catch (IoException e) {
        print("io error: " + e.path + "\n");
    }
}
```
**`expected_stdout`** : `io error: /nonexistent_cimple_test_file_xyz.txt\n`
**`expected_exit`** : `0`

---

### `tests/positive/exceptions/propagation/`

**`input.ci`** :
```c
function inner() : void {
    throw RuntimeException { message: "inner error" };
}

function middle() : void {
    inner();
}

function main() : void {
    try {
        middle();
    } catch (RuntimeException e) {
        print("propagated: " + e.message + "\n");
    }
}
```
**`expected_stdout`** : `propagated: inner error\n`
**`expected_exit`** : `0`

---

### `tests/positive/exceptions/uncaught/`

**`input.ci`** :
```c
function main() : void {
    throw RuntimeException { message: "fatal" };
}
```
**`expected_exit`** : `2`
**`expected_stderr`** : `Runtime error: RuntimeException: fatal\n`

---

### `tests/negative/exceptions/reserved_name/`

**`input.ci`** :
```c
structure RuntimeException {
    string message;
}
function main() : void {}
```
**`expected_exit`** : `1`
**`expected_stderr`** (contient) : `'RuntimeException' is a reserved name`

---

### `tests/negative/exceptions/inherit_builtin/`

**`input.ci`** :
```c
structure MyError : RuntimeException {
    string extra;
}
function main() : void {}
```
**`expected_exit`** : `1`
**`expected_stderr`** (contient) : `Cannot inherit from built-in exception type`

---

### `tests/negative/exceptions/unreachable_catch/`

**`input.ci`** :
```c
function main() : void {
    try {
        print("ok\n");
    } catch (Exception e) {
        print("all\n");
    } catch (RuntimeException e) {
        print("runtime\n");
    }
}
```
**`expected_exit`** : `1`
**`expected_stderr`** (contient) : `unreachable catch clause`

---

### `tests/negative/exceptions/throw_non_exception/`

**`input.ci`** :
```c
function main() : void {
    throw 42;
}
```
**`expected_exit`** : `1`
**`expected_stderr`** (contient) : `throw requires a value of type Exception`

---

## Ordre d'implémentation recommandé

1. `src/ast/ast.h` — nouveaux nœuds
2. `src/ast/ast.c` — `ast_free` pour les nouveaux nœuds
3. `src/lexer/lexer.re` — 3 nouveaux tokens
4. `src/parser/parser.y` — règles throw + try/catch
5. `src/interpreter/interpreter.h` — SIGNAL_THROW, throw_val, throw_type_name, g_current_interp
6. `src/interpreter/interpreter.c` — interp_throw, interp_throw_builtin, NODE_THROW, NODE_TRY_CATCH, propagation, uncaught handler
7. `src/common/error.c` — error_runtime sans exit
8. `src/runtime/value.c` — valeurs poison après error_runtime
9. `src/interpreter/builtins.c` — IoException / RegExpException / assert
10. `src/semantic/semantic.c` — built-ins enregistrés, NODE_THROW, NODE_TRY_CATCH
11. Tests

## Vérification finale

```sh
cd /Users/avialle/dev/Cimple/build && cmake --build . 2>&1 | tail -5
CIMPLE_BIN=./cimple bash ../tests/run_tests.sh 2>&1 | tail -10
```

Tous les tests existants doivent passer. Les nouveaux tests exceptions doivent passer.
