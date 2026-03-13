#include "semantic.h"
#include "../common/error.h"
#include "../common/memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct StructFieldSig {
    char *name;
    CimpleType type;
    char *struct_name;
    AstNode *decl;
    struct StructFieldSig *next;
} StructFieldSig;

typedef struct StructMethodSig {
    char *name;
    CimpleType ret_type;
    char *ret_struct_name;
    FuncParam *params;
    int param_count;
    AstNode *decl;
    struct StructMethodSig *next;
} StructMethodSig;

typedef struct StructInfo {
    char *name;
    char *base_name;
    AstNode *decl;
    StructFieldSig *fields;
    StructMethodSig *methods;
    struct StructInfo *next;
} StructInfo;

struct StructTable {
    StructInfo *head;
};

/* -----------------------------------------------------------------------
 * Built-in function signatures
 * ----------------------------------------------------------------------- */

/*
 * For polymorphic intrinsics (toString, toInt etc.) we register them once
 * and the intrinsic resolver handles overload selection.
 * For polymorphic array ops we use a separate marker.
 */
static const BuiltinSig BUILTINS[] = {
    /* I/O */
    { "print",      TYPE_VOID,   { TYPE_STRING },          1, 0, 0 },
    { "input",      TYPE_STRING, { TYPE_UNKNOWN },          0, 0, 0 },

    /* String */
    { "len",        TYPE_INT,    { TYPE_STRING },           1, 0, 0 },
    { "glyphLen",   TYPE_INT,    { TYPE_STRING },           1, 0, 0 },
    { "glyphAt",    TYPE_STRING, { TYPE_STRING, TYPE_INT }, 2, 0, 0 },
    { "byteAt",     TYPE_INT,    { TYPE_STRING, TYPE_INT }, 2, 0, 0 },
    { "substr",     TYPE_STRING, { TYPE_STRING, TYPE_INT, TYPE_INT }, 3, 0, 0 },
    { "indexOf",    TYPE_INT,    { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "contains",   TYPE_BOOL,   { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "startsWith", TYPE_BOOL,   { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "endsWith",   TYPE_BOOL,   { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "replace",    TYPE_STRING, { TYPE_STRING, TYPE_STRING, TYPE_STRING }, 3, 0, 0 },
    { "format",     TYPE_STRING, { TYPE_STRING, TYPE_STR_ARR }, 2, 0, 0 },
    { "join",       TYPE_STRING, { TYPE_STR_ARR, TYPE_STRING }, 2, 0, 0 },
    { "split",      TYPE_STR_ARR,{ TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "concat",     TYPE_STRING, { TYPE_STR_ARR },           1, 0, 0 },
    { "trim",       TYPE_STRING, { TYPE_STRING },           1, 0, 0 },
    { "trimLeft",   TYPE_STRING, { TYPE_STRING },           1, 0, 0 },
    { "trimRight",  TYPE_STRING, { TYPE_STRING },           1, 0, 0 },
    { "toUpper",    TYPE_STRING, { TYPE_STRING },           1, 0, 0 },
    { "toLower",    TYPE_STRING, { TYPE_STRING },           1, 0, 0 },
    { "capitalize", TYPE_STRING, { TYPE_STRING },           1, 0, 0 },
    { "padLeft",    TYPE_STRING, { TYPE_STRING, TYPE_INT, TYPE_STRING }, 3, 0, 0 },
    { "padRight",   TYPE_STRING, { TYPE_STRING, TYPE_INT, TYPE_STRING }, 3, 0, 0 },
    { "repeat",     TYPE_STRING, { TYPE_STRING, TYPE_INT }, 2, 0, 0 },
    { "lastIndexOf",TYPE_INT,    { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "countOccurrences", TYPE_INT, { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "isBlank",    TYPE_BOOL,   { TYPE_STRING },           1, 0, 0 },

    /* Intrinsic conversions — resolved separately */
    { "toString",   TYPE_STRING, { TYPE_UNKNOWN },          1, 0, 0 },
    { "toInt",      TYPE_INT,    { TYPE_UNKNOWN },          1, 0, 0 },
    { "toFloat",    TYPE_FLOAT,  { TYPE_UNKNOWN },          1, 0, 0 },
    { "toBool",     TYPE_BOOL,   { TYPE_UNKNOWN },          1, 0, 0 },

    /* String validation */
    { "isIntString",   TYPE_BOOL, { TYPE_STRING }, 1, 0, 0 },
    { "isFloatString", TYPE_BOOL, { TYPE_STRING }, 1, 0, 0 },
    { "isBoolString",  TYPE_BOOL, { TYPE_STRING }, 1, 0, 0 },

    /* Float math */
    { "isNaN",              TYPE_BOOL,  { TYPE_FLOAT },               1, 0, 0 },
    { "isInfinite",         TYPE_BOOL,  { TYPE_FLOAT },               1, 0, 0 },
    { "isFinite",           TYPE_BOOL,  { TYPE_FLOAT },               1, 0, 0 },
    { "isPositiveInfinity", TYPE_BOOL,  { TYPE_FLOAT },               1, 0, 0 },
    { "isNegativeInfinity", TYPE_BOOL,  { TYPE_FLOAT },               1, 0, 0 },
    { "abs",        TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "min",        TYPE_FLOAT,  { TYPE_FLOAT, TYPE_FLOAT },   2, 0, 0 },
    { "max",        TYPE_FLOAT,  { TYPE_FLOAT, TYPE_FLOAT },   2, 0, 0 },
    { "clamp",      TYPE_FLOAT,  { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT }, 3, 0, 0 },
    { "floor",      TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "ceil",       TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "round",      TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "trunc",      TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "fmod",       TYPE_FLOAT,  { TYPE_FLOAT, TYPE_FLOAT },   2, 0, 0 },
    { "sqrt",       TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "pow",        TYPE_FLOAT,  { TYPE_FLOAT, TYPE_FLOAT },   2, 0, 0 },
    { "approxEqual",TYPE_BOOL,   { TYPE_FLOAT, TYPE_FLOAT, TYPE_FLOAT }, 3, 0, 0 },
    { "sin",        TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "cos",        TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "tan",        TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "asin",       TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "acos",       TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "atan",       TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "atan2",      TYPE_FLOAT,  { TYPE_FLOAT, TYPE_FLOAT },   2, 0, 0 },
    { "exp",        TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "log",        TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "log2",       TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },
    { "log10",      TYPE_FLOAT,  { TYPE_FLOAT },               1, 0, 0 },

    /* Integer math */
    { "absInt",     TYPE_INT,   { TYPE_INT },                  1, 0, 0 },
    { "minInt",     TYPE_INT,   { TYPE_INT, TYPE_INT },        2, 0, 0 },
    { "maxInt",     TYPE_INT,   { TYPE_INT, TYPE_INT },        2, 0, 0 },
    { "clampInt",   TYPE_INT,   { TYPE_INT, TYPE_INT, TYPE_INT }, 3, 0, 0 },
    { "isEven",     TYPE_BOOL,  { TYPE_INT },                  1, 0, 0 },
    { "isOdd",      TYPE_BOOL,  { TYPE_INT },                  1, 0, 0 },
    { "safeDivInt", TYPE_INT,   { TYPE_INT, TYPE_INT, TYPE_INT }, 3, 0, 0 },
    { "safeModInt", TYPE_INT,   { TYPE_INT, TYPE_INT, TYPE_INT }, 3, 0, 0 },

    /* Array intrinsics — polymorphic flag */
    { "count",       TYPE_INT,   { TYPE_UNKNOWN }, 1, 0, 1 },
    { "arrayPush",   TYPE_VOID,  { TYPE_UNKNOWN, TYPE_UNKNOWN }, 2, 0, 1 },
    { "arrayPop",    TYPE_UNKNOWN,{ TYPE_UNKNOWN }, 1, 0, 1 },
    { "arrayInsert", TYPE_VOID,  { TYPE_UNKNOWN, TYPE_INT, TYPE_UNKNOWN }, 3, 0, 1 },
    { "arrayRemove", TYPE_VOID,  { TYPE_UNKNOWN, TYPE_INT }, 2, 0, 1 },
    { "arrayGet",    TYPE_UNKNOWN,{ TYPE_UNKNOWN, TYPE_INT }, 2, 0, 1 },
    { "arraySet",    TYPE_VOID,  { TYPE_UNKNOWN, TYPE_INT, TYPE_UNKNOWN }, 3, 0, 1 },

    /* File I/O */
    { "readFile",    TYPE_STRING,   { TYPE_STRING },              1, 0, 0 },
    { "writeFile",   TYPE_VOID,     { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "appendFile",  TYPE_VOID,     { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "readFileBytes", TYPE_BYTE_ARR, { TYPE_STRING },            1, 0, 0 },
    { "writeFileBytes", TYPE_VOID, { TYPE_STRING, TYPE_BYTE_ARR }, 2, 0, 0 },
    { "appendFileBytes", TYPE_VOID, { TYPE_STRING, TYPE_BYTE_ARR }, 2, 0, 0 },
    { "fileExists",  TYPE_BOOL,     { TYPE_STRING },              1, 0, 0 },
    { "tempPath",    TYPE_STRING,   { TYPE_UNKNOWN },             0, 0, 0 },
    { "remove",      TYPE_VOID,     { TYPE_STRING },              1, 0, 0 },
    { "chmod",       TYPE_VOID,     { TYPE_STRING, TYPE_INT },    2, 0, 0 },
    { "cwd",         TYPE_STRING,   { TYPE_UNKNOWN },             0, 0, 0 },
    { "copy",        TYPE_VOID,     { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "move",        TYPE_VOID,     { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },
    { "isReadable",  TYPE_BOOL,     { TYPE_STRING },              1, 0, 0 },
    { "isWritable",  TYPE_BOOL,     { TYPE_STRING },              1, 0, 0 },
    { "isExecutable",TYPE_BOOL,     { TYPE_STRING },              1, 0, 0 },
    { "isDirectory", TYPE_BOOL,     { TYPE_STRING },              1, 0, 0 },
    { "dirname",     TYPE_STRING,   { TYPE_STRING },              1, 0, 0 },
    { "basename",    TYPE_STRING,   { TYPE_STRING },              1, 0, 0 },
    { "filename",    TYPE_STRING,   { TYPE_STRING },              1, 0, 0 },
    { "extension",   TYPE_STRING,   { TYPE_STRING },              1, 0, 0 },
    { "readLines",   TYPE_STR_ARR,  { TYPE_STRING },              1, 0, 0 },
    { "writeLines",  TYPE_VOID,     { TYPE_STRING, TYPE_STR_ARR }, 2, 0, 0 },
    { "byteToInt",   TYPE_INT,      { TYPE_BYTE },                1, 0, 0 },
    { "intToByte",   TYPE_BYTE,     { TYPE_INT },                 1, 0, 0 },
    { "stringToBytes", TYPE_BYTE_ARR, { TYPE_STRING },            1, 0, 0 },
    { "bytesToString", TYPE_STRING, { TYPE_BYTE_ARR },            1, 0, 0 },
    { "intToBytes",  TYPE_BYTE_ARR, { TYPE_INT },                 1, 0, 0 },
    { "floatToBytes",TYPE_BYTE_ARR, { TYPE_FLOAT },               1, 0, 0 },
    { "bytesToInt",  TYPE_INT,      { TYPE_BYTE_ARR },            1, 0, 0 },
    { "bytesToFloat",TYPE_FLOAT,    { TYPE_BYTE_ARR },            1, 0, 0 },

    /* exec */
    { "exec",        TYPE_EXEC_RESULT, { TYPE_STR_ARR, TYPE_STR_ARR }, 2, 0, 0 },
    { "execStatus",  TYPE_INT,     { TYPE_EXEC_RESULT }, 1, 0, 0 },
    { "execStdout",  TYPE_STRING,  { TYPE_EXEC_RESULT }, 1, 0, 0 },
    { "execStderr",  TYPE_STRING,  { TYPE_EXEC_RESULT }, 1, 0, 0 },

    /* Environment */
    { "getEnv",      TYPE_STRING,  { TYPE_STRING, TYPE_STRING }, 2, 0, 0 },

    /* Time */
    { "now",          TYPE_INT, { TYPE_UNKNOWN }, 0, 0, 0 },
    { "epochToYear",  TYPE_INT, { TYPE_INT },     1, 0, 0 },
    { "epochToMonth", TYPE_INT, { TYPE_INT },     1, 0, 0 },
    { "epochToDay",   TYPE_INT, { TYPE_INT },     1, 0, 0 },
    { "epochToHour",  TYPE_INT, { TYPE_INT },     1, 0, 0 },
    { "epochToMinute",TYPE_INT, { TYPE_INT },     1, 0, 0 },
    { "epochToSecond",TYPE_INT, { TYPE_INT },     1, 0, 0 },
    { "makeEpoch",    TYPE_INT, { TYPE_INT, TYPE_INT, TYPE_INT,
                                   TYPE_INT, TYPE_INT, TYPE_INT }, 6, 0, 0 },
    { "formatDate",   TYPE_STRING, { TYPE_INT, TYPE_STRING }, 2, 0, 0 },

    { NULL, TYPE_UNKNOWN, {TYPE_UNKNOWN}, 0, 0, 0 }
};

const BuiltinSig *builtin_lookup(const char *name) {
    for (int i = 0; BUILTINS[i].name; i++) {
        if (strcmp(BUILTINS[i].name, name) == 0)
            return &BUILTINS[i];
    }
    return NULL;
}

/* -----------------------------------------------------------------------
 * Forward declarations
 * ----------------------------------------------------------------------- */
static void check_stmt(SemCtx *ctx, AstNode *n);
static CimpleType check_expr(SemCtx *ctx, AstNode *n);
static void error_type_mismatch(int line, int col, const char *context,
                                CimpleType expected, CimpleType got);
static void error_operator_type(int line, int col, const char *op,
                                const char *expected_types, CimpleType got);
static int types_equal_ex(CimpleType a, const char *a_name,
                          CimpleType b, const char *b_name);
static StructTable *struct_table_new(void);
static void struct_table_free(StructTable *st);
static StructInfo *struct_table_lookup(StructTable *st, const char *name);
static StructInfo *struct_table_define(StructTable *st, const char *name,
                                       const char *base_name, AstNode *decl);
static StructFieldSig *struct_lookup_field(StructTable *st, StructInfo *si, const char *name);
static StructMethodSig *struct_lookup_method(StructTable *st, StructInfo *si, const char *name);
static void collect_structs(SemCtx *ctx, AstNode *program);
static void collect_functions(SemCtx *ctx, AstNode *program);
static void validate_structs(SemCtx *ctx);
static void check_struct_methods(SemCtx *ctx);

/* Resolve intrinsic conversion overloads statically. */
CimpleType builtin_resolve_intrinsic(const char *name, CimpleType arg_type,
                                      int line, int col) {
    if (strcmp(name, "toString") == 0) {
        if (arg_type == TYPE_INT || arg_type == TYPE_FLOAT || arg_type == TYPE_BOOL || arg_type == TYPE_BYTE)
            return TYPE_STRING;
        error_operator_type(line, col, "toString()", "int, float, bool or byte", arg_type);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "toInt") == 0) {
        if (arg_type == TYPE_FLOAT || arg_type == TYPE_STRING)
            return TYPE_INT;
        error_operator_type(line, col, "toInt()", "float or string", arg_type);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "toFloat") == 0) {
        if (arg_type == TYPE_INT || arg_type == TYPE_STRING)
            return TYPE_FLOAT;
        error_operator_type(line, col, "toFloat()", "int or string", arg_type);
        return TYPE_UNKNOWN;
    }
    if (strcmp(name, "toBool") == 0) {
        if (arg_type == TYPE_STRING)
            return TYPE_BOOL;
        error_operator_type(line, col, "toBool()", "string", arg_type);
        return TYPE_UNKNOWN;
    }
    return TYPE_UNKNOWN;
}

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */
static int types_equal(CimpleType a, CimpleType b) {
    return a == b;
}

static int types_equal_ex(CimpleType a, const char *a_name,
                          CimpleType b, const char *b_name) {
    if (a != b) return 0;
    if (a == TYPE_STRUCT || a == TYPE_STRUCT_ARR) {
        if (!a_name || !b_name) return 0;
        return strcmp(a_name, b_name) == 0;
    }
    return 1;
}

static int is_int_like(CimpleType t) {
    return t == TYPE_INT || t == TYPE_BYTE;
}

static StructTable *struct_table_new(void) {
    StructTable *st = ALLOC(StructTable);
    st->head = NULL;
    return st;
}

static void struct_table_free(StructTable *st) {
    if (!st) return;
    StructInfo *si = st->head;
    while (si) {
        StructInfo *next = si->next;
        StructFieldSig *field = si->fields;
        while (field) {
            StructFieldSig *fnext = field->next;
            free(field->name);
            free(field->struct_name);
            free(field);
            field = fnext;
        }
        StructMethodSig *method = si->methods;
        while (method) {
            StructMethodSig *mnext = method->next;
            free(method->name);
            free(method->ret_struct_name);
            for (int i = 0; i < method->param_count; i++) {
                free(method->params[i].name);
                free(method->params[i].struct_name);
            }
            free(method->params);
            free(method);
            method = mnext;
        }
        free(si->name);
        free(si->base_name);
        free(si);
        si = next;
    }
    free(st);
}

static StructInfo *struct_table_lookup(StructTable *st, const char *name) {
    for (StructInfo *it = st->head; it; it = it->next)
        if (strcmp(it->name, name) == 0) return it;
    return NULL;
}

static StructInfo *struct_table_define(StructTable *st, const char *name,
                                       const char *base_name, AstNode *decl) {
    if (struct_table_lookup(st, name)) return NULL;
    StructInfo *si = ALLOC(StructInfo);
    si->name = cimple_strdup(name);
    si->base_name = base_name ? cimple_strdup(base_name) : NULL;
    si->decl = decl;
    si->fields = NULL;
    si->methods = NULL;
    si->next = NULL;
    if (!st->head) st->head = si;
    else {
        StructInfo *tail = st->head;
        while (tail->next) tail = tail->next;
        tail->next = si;
    }
    return si;
}

static StructFieldSig *struct_lookup_field(StructTable *st, StructInfo *si, const char *name) {
    for (StructFieldSig *f = si ? si->fields : NULL; f; f = f->next)
        if (strcmp(f->name, name) == 0) return f;
    if (si && si->base_name) {
        StructInfo *base = struct_table_lookup(st, si->base_name);
        if (base) return struct_lookup_field(st, base, name);
    }
    return NULL;
}

static StructMethodSig *struct_lookup_method(StructTable *st, StructInfo *si, const char *name) {
    for (StructMethodSig *m = si ? si->methods : NULL; m; m = m->next)
        if (strcmp(m->name, name) == 0) return m;
    if (si && si->base_name) {
        StructInfo *base = struct_table_lookup(st, si->base_name);
        if (base) return struct_lookup_method(st, base, name);
    }
    return NULL;
}

static int extract_int_literal_expr(AstNode *n, int64_t *out) {
    if (!n) return 0;
    if (n->kind == NODE_INT_LIT) {
        *out = n->u.int_lit.value;
        return 1;
    }
    if (n->kind == NODE_UNOP &&
        n->u.unop.op == OP_NEG &&
        n->u.unop.operand &&
        n->u.unop.operand->kind == NODE_INT_LIT) {
        *out = -n->u.unop.operand->u.int_lit.value;
        return 1;
    }
    return 0;
}

static int is_byte_compatible_literal_expr(AstNode *n) {
    int64_t value;
    return extract_int_literal_expr(n, &value) && value >= 0 && value <= 255;
}

static void error_byte_literal_out_of_range(int line, int col, int64_t value) {
    error_semantic_hint(line, col,
                        "Valid range: 0 to 255.",
                        "Byte literal out of range: %lld",
                        (long long)value);
}

static int array_lit_matches_declared_type(AstNode *lit, CimpleType target_type) {
    if (!lit || lit->kind != NODE_ARRAY_LIT || !type_is_array(target_type)) return 0;
    CimpleType elem = type_elem(target_type);
    NodeList *elems = &lit->u.array_lit.elems;
    for (int i = 0; i < elems->count; i++) {
        CimpleType got = elems->items[i]->type;
        if (elem == TYPE_BYTE) {
            if (got == TYPE_BYTE || is_byte_compatible_literal_expr(elems->items[i])) continue;
            return 0;
        }
        if (got != elem) return 0;
    }
    return 1;
}

static void coerce_array_lit_to_type(AstNode *lit, CimpleType target_type) {
    if (!lit || lit->kind != NODE_ARRAY_LIT || !type_is_array(target_type)) return;
    lit->u.array_lit.elem_type = type_elem(target_type);
    lit->type = target_type;
}

static int report_first_invalid_byte_array_literal(AstNode *lit) {
    if (!lit || lit->kind != NODE_ARRAY_LIT) return 0;
    NodeList *elems = &lit->u.array_lit.elems;
    for (int i = 0; i < elems->count; i++) {
        int64_t literal_value;
        if (extract_int_literal_expr(elems->items[i], &literal_value) &&
            (literal_value < 0 || literal_value > 255)) {
            error_byte_literal_out_of_range(elems->items[i]->line,
                                            elems->items[i]->col,
                                            literal_value);
            return 1;
        }
    }
    return 0;
}

static void error_type_mismatch(int line, int col, const char *context,
                                CimpleType expected, CimpleType got) {
    char hint[160];
    snprintf(hint, sizeof(hint), "Expected '%s', got '%s'.",
             type_name(expected), type_name(got));
    error_semantic_hint(line, col, hint, "Type mismatch in %s", context);
}

static void error_operator_type(int line, int col, const char *op,
                                const char *expected_types, CimpleType got) {
    char hint[192];
    snprintf(hint, sizeof(hint), "Operator '%s' requires %s.", op, expected_types);
    error_semantic_hint(line, col, hint,
                        "Operator '%s' cannot be applied to type '%s'",
                        op, type_name(got));
}

/* Push/pop scope wrappers */
static void push_scope(SemCtx *ctx, int is_fn) {
    ctx->current = scope_new(ctx->current, is_fn);
}
static void pop_scope(SemCtx *ctx) {
    Scope *old    = ctx->current;
    ctx->current  = old->parent;
    scope_free(old);
}

/* -----------------------------------------------------------------------
 * Predefined constant types
 * ----------------------------------------------------------------------- */
static CimpleType const_type(const char *name) {
    if (strcmp(name, "FLOAT_EPSILON") == 0 ||
        strcmp(name, "FLOAT_MIN")     == 0 ||
        strcmp(name, "FLOAT_MAX")     == 0 ||
        strcmp(name, "M_PI")          == 0 ||
        strcmp(name, "M_E")           == 0 ||
        strcmp(name, "M_TAU")         == 0 ||
        strcmp(name, "M_SQRT2")       == 0 ||
        strcmp(name, "M_LN2")         == 0 ||
        strcmp(name, "M_LN10")        == 0)
        return TYPE_FLOAT;
    return TYPE_INT;
}

/* -----------------------------------------------------------------------
 * check_expr — type-check an expression, return its type
 * ----------------------------------------------------------------------- */
static CimpleType check_expr(SemCtx *ctx, AstNode *n) {
    if (!n) return TYPE_UNKNOWN;

    switch (n->kind) {
    case NODE_INT_LIT:    n->type = TYPE_INT;    return TYPE_INT;
    case NODE_FLOAT_LIT:  n->type = TYPE_FLOAT;  return TYPE_FLOAT;
    case NODE_BOOL_LIT:   n->type = TYPE_BOOL;   return TYPE_BOOL;
    case NODE_STRING_LIT: n->type = TYPE_STRING; return TYPE_STRING;

    case NODE_CONST:
        n->type = const_type(n->u.constant.name);
        return n->type;

    case NODE_IDENT: {
        Symbol *sym = scope_lookup(ctx->current, n->u.ident.name);
        if (!sym) {
            char hint[160];
            snprintf(hint, sizeof(hint), "'%s' must be declared before use.",
                     n->u.ident.name);
            error_semantic_hint(n->line, n->col, hint,
                                "Undefined variable: '%s'", n->u.ident.name);
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        n->type = sym->type;
        free(n->type_name_hint);
        n->type_name_hint = sym->struct_name ? cimple_strdup(sym->struct_name) : NULL;
        return sym->type;
    }

    case NODE_SELF:
        if (!ctx->in_method || !ctx->current_struct_name) {
            error_semantic(n->line, n->col, "'self' used outside of a structure method");
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        n->type = TYPE_STRUCT;
        free(n->type_name_hint);
        n->type_name_hint = cimple_strdup(ctx->current_struct_name);
        return TYPE_STRUCT;

    case NODE_SUPER:
        if (!ctx->in_method || !ctx->current_base_name) {
            error_semantic(n->line, n->col, "'super' used outside of a derived structure method");
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        n->type = TYPE_STRUCT;
        free(n->type_name_hint);
        n->type_name_hint = cimple_strdup(ctx->current_base_name);
        return TYPE_STRUCT;

    case NODE_CLONE: {
        StructInfo *si = struct_table_lookup(ctx->structs, n->u.clone_expr.struct_name);
        if (!si) {
            Symbol *sym = scope_lookup(ctx->current, n->u.clone_expr.struct_name);
            if (sym) {
                error_semantic_hint(n->line, n->col,
                                    "Use 'clone StructureName' to create an instance.",
                                    "Cannot clone a variable: '%s'",
                                    n->u.clone_expr.struct_name);
            } else {
                error_semantic(n->line, n->col,
                               "Unknown type: '%s'", n->u.clone_expr.struct_name);
            }
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        n->type = TYPE_STRUCT;
        free(n->type_name_hint);
        n->type_name_hint = cimple_strdup(si->name);
        return TYPE_STRUCT;
    }

    case NODE_ARRAY_LIT: {
        NodeList *elems = &n->u.array_lit.elems;
        if (elems->count == 0) {
            n->type = TYPE_UNKNOWN; /* will be resolved at assignment */
            return TYPE_UNKNOWN;
        }
        CimpleType elem_t = check_expr(ctx, elems->items[0]);
        for (int i = 1; i < elems->count; i++) {
            CimpleType t = check_expr(ctx, elems->items[i]);
            if (elem_t == TYPE_BYTE) {
                int64_t literal_value;
                if (extract_int_literal_expr(elems->items[i], &literal_value)) {
                    if (literal_value < 0 || literal_value > 255) {
                        error_byte_literal_out_of_range(elems->items[i]->line,
                                                        elems->items[i]->col,
                                                        literal_value);
                    }
                    continue;
                }
                if (t == TYPE_BYTE) continue;
            }
            if (t != elem_t) {
                char hint[160];
                snprintf(hint, sizeof(hint), "Expected '%s', got '%s' at index %d.",
                         type_name(elem_t), type_name(t), i);
                error_semantic_hint(elems->items[i]->line, elems->items[i]->col, hint,
                                    "Array element type mismatch");
            }
        }
        n->u.array_lit.elem_type = elem_t;
        n->type = type_make_array(elem_t);
        return n->type;
    }

    case NODE_BINOP: {
        CimpleType lt = check_expr(ctx, n->u.binop.left);
        CimpleType rt = check_expr(ctx, n->u.binop.right);
        OpKind op = n->u.binop.op;

        /* String concatenation */
        if (op == OP_ADD && lt == TYPE_STRING && rt == TYPE_STRING) {
            n->type = TYPE_STRING;
            return TYPE_STRING;
        }

        /* Logical */
        if (op == OP_AND || op == OP_OR) {
            const char *opname = op == OP_AND ? "&&" : "||";
            if (lt != TYPE_BOOL) {
                if (lt == TYPE_BYTE)
                    error_semantic_hint(n->line, n->col,
                                        "Logical operators require 'bool' operands.",
                                        "Operator '%s' cannot be applied to type 'byte'", opname);
                else
                    error_operator_type(n->line, n->col, opname, "bool operands", lt);
            }
            if (rt != TYPE_BOOL) {
                if (rt == TYPE_BYTE)
                    error_semantic_hint(n->line, n->col,
                                        "Logical operators require 'bool' operands.",
                                        "Operator '%s' cannot be applied to type 'byte'", opname);
                else
                    error_operator_type(n->line, n->col, opname, "bool operands", rt);
            }
            n->type = TYPE_BOOL;
            return TYPE_BOOL;
        }

        /* Comparison */
        if (op == OP_EQ || op == OP_NEQ || op == OP_LT ||
            op == OP_LEQ || op == OP_GT || op == OP_GEQ) {
            if (!(is_int_like(lt) && is_int_like(rt)) && !types_equal(lt, rt))
                error_type_mismatch(n->line, n->col, "comparison", lt, rt);
            n->type = TYPE_BOOL;
            return TYPE_BOOL;
        }

        /* Arithmetic (+, -, *, /, %) */
        if (op == OP_ADD || op == OP_SUB || op == OP_MUL ||
            op == OP_DIV || op == OP_MOD) {
            if (is_int_like(lt) && is_int_like(rt)) {
                n->type = TYPE_INT; return TYPE_INT;
            }
            if (lt == TYPE_FLOAT && rt == TYPE_FLOAT) {
                n->type = TYPE_FLOAT; return TYPE_FLOAT;
            }
            if (op == OP_MOD && (lt != TYPE_INT || rt != TYPE_INT)) {
                error_operator_type(n->line, n->col, "%", "int operands",
                                    lt != TYPE_INT ? lt : rt);
            } else {
                error_type_mismatch(n->line, n->col, "arithmetic", lt, rt);
            }
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }

        /* Bitwise */
        if (op == OP_BAND || op == OP_BOR || op == OP_BXOR ||
            op == OP_LSHIFT || op == OP_RSHIFT) {
            if (!is_int_like(lt) || !is_int_like(rt))
                error_operator_type(n->line, n->col,
                                    op == OP_BAND ? "&" :
                                    op == OP_BOR ? "|" :
                                    op == OP_BXOR ? "^" :
                                    op == OP_LSHIFT ? "<<" : ">>",
                                    "int operands", !is_int_like(lt) ? lt : rt);
            if ((op == OP_BAND || op == OP_BOR || op == OP_BXOR) &&
                lt == TYPE_BYTE && rt == TYPE_BYTE) {
                n->type = TYPE_BYTE;
                return TYPE_BYTE;
            }
            n->type = TYPE_INT;
            return TYPE_INT;
        }

        n->type = TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }

    case NODE_UNOP: {
        CimpleType t = check_expr(ctx, n->u.unop.operand);
        OpKind op = n->u.unop.op;
        if (op == OP_NOT) {
            if (t != TYPE_BOOL) {
                if (t == TYPE_BYTE)
                    error_semantic_hint(n->line, n->col,
                                        "Logical operators require 'bool' operands.",
                                        "Operator '!' cannot be applied to type 'byte'");
                else
                    error_operator_type(n->line, n->col, "!", "a bool operand", t);
            }
            n->type = TYPE_BOOL; return TYPE_BOOL;
        }
        if (op == OP_NEG) {
            if (!is_int_like(t) && t != TYPE_FLOAT)
                error_operator_type(n->line, n->col, "-", "a numeric operand", t);
            n->type = t; return t;
        }
        if (op == OP_BNOT) {
            if (!is_int_like(t))
                error_operator_type(n->line, n->col, "~", "an int operand", t);
            n->type = (t == TYPE_BYTE) ? TYPE_BYTE : TYPE_INT; return n->type;
        }
        n->type = TYPE_UNKNOWN; return TYPE_UNKNOWN;
    }

    case NODE_CALL: {
        const char *fname = n->u.call.name;
        NodeList   *args  = &n->u.call.args;

        /* First check argument types */
        CimpleType arg_types[16];
        int nargs = args->count;
        for (int i = 0; i < nargs && i < 16; i++)
            arg_types[i] = check_expr(ctx, args->items[i]);

        /* Intrinsic conversions */
        if (strcmp(fname, "toString") == 0 || strcmp(fname, "toInt") == 0 ||
            strcmp(fname, "toFloat")  == 0 || strcmp(fname, "toBool") == 0) {
            if (nargs != 1)
                error_semantic_hint(n->line, n->col, "Expected 1, got 0.",
                                    "Wrong number of arguments for '%s'", fname);
            CimpleType ret = builtin_resolve_intrinsic(fname,
                                nargs > 0 ? arg_types[0] : TYPE_UNKNOWN,
                                n->line, n->col);
            n->type = ret;
            return ret;
        }

        /* Try builtin */
        const BuiltinSig *sig = builtin_lookup(fname);
        if (sig) {
            if (sig->polymorphic) {
                /* Array intrinsics — basic validation */
                if (nargs < sig->param_count)
                    error_semantic_hint(n->line, n->col, "Not enough arguments were provided.",
                                        "Wrong number of arguments for '%s'", fname);
                else if (nargs > 0 && !type_is_array(arg_types[0]) &&
                         arg_types[0] != TYPE_UNKNOWN)
                    error_operator_type(n->line, n->col, fname,
                                        "an array as first argument", arg_types[0]);

                if (nargs > 0 && type_is_array(arg_types[0])) {
                    CimpleType elem = type_elem(arg_types[0]);

                    if ((strcmp(fname, "arrayInsert") == 0 ||
                         strcmp(fname, "arrayGet") == 0 ||
                         strcmp(fname, "arraySet") == 0 ||
                         strcmp(fname, "arrayRemove") == 0) &&
                        nargs > 1 &&
                        arg_types[1] != TYPE_INT &&
                        arg_types[1] != TYPE_UNKNOWN) {
                        error_type_mismatch(args->items[1]->line, args->items[1]->col,
                                            "index expression", TYPE_INT, arg_types[1]);
                    }

                    if ((strcmp(fname, "arrayPush") == 0 && nargs > 1) ||
                        (strcmp(fname, "arrayInsert") == 0 && nargs > 2) ||
                        (strcmp(fname, "arraySet") == 0 && nargs > 2)) {
                        int value_index = strcmp(fname, "arrayPush") == 0 ? 1 : 2;
                        CimpleType value_t = arg_types[value_index];
                        if (elem == TYPE_BYTE && is_byte_compatible_literal_expr(args->items[value_index])) {
                            /* ok */
                        } else if (value_t != TYPE_UNKNOWN && !types_equal(value_t, elem)) {
                            if (strcmp(fname, "arraySet") == 0) {
                                error_type_mismatch(args->items[value_index]->line,
                                                    args->items[value_index]->col,
                                                    "array assignment", elem, value_t);
                            } else {
                                error_semantic(args->items[value_index]->line,
                                               args->items[value_index]->col,
                                               "Array element type mismatch");
                            }
                        }
                    }
                }

                /* Determine return type for poly ops */
                CimpleType ret = sig->ret_type;
                if (ret == TYPE_UNKNOWN && nargs > 0 && type_is_array(arg_types[0]))
                    ret = type_elem(arg_types[0]);
                n->type = ret;
                free(n->type_name_hint);
                n->type_name_hint = (ret == TYPE_STRUCT && nargs > 0 && args->items[0]->type_name_hint)
                    ? cimple_strdup(args->items[0]->type_name_hint) : NULL;
                return ret;
            }

            /* Regular builtin — check arg count */
            if (nargs != sig->param_count) {
                char hint[128];
                snprintf(hint, sizeof(hint), "Expected %d, got %d.",
                         sig->param_count, nargs);
                error_semantic_hint(n->line, n->col, hint,
                                    "Wrong number of arguments for '%s'", fname);
            }
            else {
                /* Check arg types */
                for (int i = 0; i < nargs && i < sig->param_count; i++) {
                    CimpleType expected = sig->param_types[i];
                    if (expected == TYPE_BYTE_ARR &&
                        args->items[i]->kind == NODE_ARRAY_LIT &&
                        array_lit_matches_declared_type(args->items[i], expected)) {
                        coerce_array_lit_to_type(args->items[i], expected);
                        continue;
                    }
                    if (expected == TYPE_BYTE_ARR &&
                        args->items[i]->kind == NODE_ARRAY_LIT &&
                        report_first_invalid_byte_array_literal(args->items[i])) {
                        continue;
                    }
                    if (expected != TYPE_UNKNOWN &&
                        arg_types[i] != TYPE_UNKNOWN &&
                        !types_equal_ex(arg_types[i], args->items[i]->type_name_hint,
                                        expected, NULL)) {
                        error_type_mismatch(args->items[i]->line, args->items[i]->col,
                                            "function call", expected, arg_types[i]);
                    }
                }
            }
            n->type = sig->ret_type;
            return sig->ret_type;
        }

        /* User function */
        FuncSig *fsig = func_table_lookup(ctx->funcs, fname);
        if (!fsig) {
            char hint[192];
            snprintf(hint, sizeof(hint),
                     "'%s' is not defined in this scope or the standard library.",
                     fname);
            error_semantic_hint(n->line, n->col, hint,
                                "Unknown function: '%s'", fname);
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        if (nargs != fsig->param_count) {
            char hint[128];
            snprintf(hint, sizeof(hint), "Expected %d, got %d.",
                     fsig->param_count, nargs);
            error_semantic_hint(n->line, n->col, hint,
                                "Wrong number of arguments for '%s'", fname);
        }
        else {
            for (int i = 0; i < nargs; i++) {
                if (!types_equal_ex(arg_types[i], args->items[i]->type_name_hint,
                                    fsig->params[i].type, fsig->params[i].struct_name))
                    error_type_mismatch(args->items[i]->line, args->items[i]->col,
                                        "function call", fsig->params[i].type, arg_types[i]);
            }
        }
        n->type = fsig->ret_type;
        free(n->type_name_hint);
        n->type_name_hint = fsig->ret_struct_name ? cimple_strdup(fsig->ret_struct_name) : NULL;
        return fsig->ret_type;
    }

    case NODE_MEMBER: {
        CimpleType base_t = check_expr(ctx, n->u.member.base);
        if (n->u.member.base->kind == NODE_SUPER) {
            error_semantic_hint(n->line, n->col,
                                "Use 'self.<field>' to access fields.",
                                "Cannot access field via 'super'");
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        if (base_t != TYPE_STRUCT || !n->u.member.base->type_name_hint) {
            error_semantic(n->line, n->col,
                           "Member access requires a structure instance");
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        StructInfo *si = struct_table_lookup(ctx->structs, n->u.member.base->type_name_hint);
        StructFieldSig *field = struct_lookup_field(ctx->structs, si, n->u.member.name);
        if (!field) {
            error_semantic(n->line, n->col,
                           "Unknown field '%s' on structure '%s'",
                           n->u.member.name, si ? si->name : n->u.member.base->type_name_hint);
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        n->type = field->type;
        free(n->type_name_hint);
        n->type_name_hint = field->struct_name ? cimple_strdup(field->struct_name) : NULL;
        return n->type;
    }

    case NODE_METHOD_CALL: {
        CimpleType base_t = check_expr(ctx, n->u.method_call.base);
        if (base_t != TYPE_STRUCT || !n->u.method_call.base->type_name_hint) {
            error_semantic(n->line, n->col,
                           "Member access requires a structure instance");
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        StructInfo *si = struct_table_lookup(ctx->structs, n->u.method_call.base->type_name_hint);
        if (n->u.method_call.is_super && n->u.method_call.base->kind != NODE_SUPER) {
            n->u.method_call.is_super = 0;
        }
        if (n->u.method_call.is_super && !ctx->current_base_name) {
            error_semantic(n->line, n->col, "'super' used outside of a derived structure method");
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        if (n->u.method_call.is_super && si && si->base_name) {
            si = struct_table_lookup(ctx->structs, si->base_name);
        }
        StructMethodSig *method = struct_lookup_method(ctx->structs, si, n->u.method_call.name);
        if (!method) {
            error_semantic(n->line, n->col,
                           "Unknown method '%s' on structure '%s'",
                           n->u.method_call.name,
                           si ? si->name : n->u.method_call.base->type_name_hint);
            n->type = TYPE_UNKNOWN;
            return TYPE_UNKNOWN;
        }
        for (int i = 0; i < n->u.method_call.args.count; i++)
            (void)check_expr(ctx, n->u.method_call.args.items[i]);
        if (n->u.method_call.args.count != method->param_count) {
            char hint[128];
            snprintf(hint, sizeof(hint), "Expected %d, got %d.",
                     method->param_count, n->u.method_call.args.count);
            error_semantic_hint(n->line, n->col, hint,
                                "Wrong number of arguments for '%s'",
                                n->u.method_call.name);
        } else {
            for (int i = 0; i < method->param_count; i++) {
                AstNode *arg = n->u.method_call.args.items[i];
                if (!types_equal_ex(arg->type, arg->type_name_hint,
                                    method->params[i].type, method->params[i].struct_name)) {
                    error_type_mismatch(arg->line, arg->col, "function call",
                                        method->params[i].type, arg->type);
                }
            }
        }
        n->type = method->ret_type;
        free(n->type_name_hint);
        n->type_name_hint = method->ret_struct_name ? cimple_strdup(method->ret_struct_name) : NULL;
        return n->type;
    }

    case NODE_INDEX: {
        CimpleType base_t = check_expr(ctx, n->u.index.base);
        CimpleType idx_t  = check_expr(ctx, n->u.index.index);
        if (idx_t != TYPE_INT && idx_t != TYPE_UNKNOWN)
            error_type_mismatch(n->u.index.index->line, n->u.index.index->col,
                                "index expression", TYPE_INT, idx_t);
        if (base_t == TYPE_STRING) {
            n->type = TYPE_STRING;
            return TYPE_STRING;
        }
        if (type_is_array(base_t)) {
            CimpleType elem = type_elem(base_t);
            n->type = elem;
            free(n->type_name_hint);
            n->type_name_hint = (elem == TYPE_STRUCT && n->u.index.base->type_name_hint)
                ? cimple_strdup(n->u.index.base->type_name_hint) : NULL;
            return elem;
        }
        if (base_t != TYPE_UNKNOWN)
            error_operator_type(n->line, n->col, "[]", "an array or string", base_t);
        n->type = TYPE_UNKNOWN;
        return TYPE_UNKNOWN;
    }

    default:
        return TYPE_UNKNOWN;
    }
}

/* -----------------------------------------------------------------------
 * check_block_returns — return-path analysis for a block
 * Returns 1 if all paths return, 0 otherwise.
 * ----------------------------------------------------------------------- */
static int block_always_returns(AstNode *block);

static int stmt_always_returns(AstNode *n) {
    if (!n) return 0;
    switch (n->kind) {
    case NODE_RETURN: return 1;
    case NODE_BLOCK:  return block_always_returns(n);
    case NODE_IF:
        if (!n->u.if_stmt.else_branch) return 0;
        return stmt_always_returns(n->u.if_stmt.then_branch) &&
               stmt_always_returns(n->u.if_stmt.else_branch);
    default: return 0;
    }
}

static int block_always_returns(AstNode *block) {
    if (!block || block->kind != NODE_BLOCK) return 0;
    NodeList *stmts = &block->u.block.stmts;
    for (int i = 0; i < stmts->count; i++) {
        if (stmt_always_returns(stmts->items[i])) return 1;
    }
    return 0;
}

static int is_reserved_constant_name(const char *name) {
    static const char *reserved_consts[] = {
        "INT_MAX","INT_MIN","INT_SIZE","FLOAT_SIZE","FLOAT_DIG",
        "FLOAT_EPSILON","FLOAT_MIN","FLOAT_MAX",
        "M_PI","M_E","M_TAU","M_SQRT2","M_LN2","M_LN10", NULL
    };
    for (int i = 0; reserved_consts[i]; i++) {
        if (strcmp(name, reserved_consts[i]) == 0) return 1;
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * check_stmt
 * ----------------------------------------------------------------------- */
static void check_stmt(SemCtx *ctx, AstNode *n) {
    if (!n) return;

    switch (n->kind) {
    case NODE_VAR_DECL: {
        const char *name = n->u.var_decl.name;
        CimpleType   t   = n->u.var_decl.type;

        /* Check for predefined constant collision */
        if (is_reserved_constant_name(name)) {
            error_semantic(n->line, n->col,
                           "'%s' is a reserved identifier",
                           name);
            return;
        }

        /* ExecResult must be initialised */
        if (t == TYPE_EXEC_RESULT && !n->u.var_decl.init) {
            error_semantic(n->line, n->col,
                           "'ExecResult' variable must be initialized with exec()",
                           name);
        }

        /* Define in current scope */
        Symbol *sym = scope_define(ctx->current, name, t,
                                   n->u.var_decl.struct_name,
                                   n->line, n->col);
        if (!sym) {
            Symbol *existing = scope_lookup_local(ctx->current, name);
            char hint[128];
            snprintf(hint, sizeof(hint), "First declaration at line %d, column %d.",
                     existing ? existing->line : 0, existing ? existing->col : 0);
            error_semantic_hint(n->line, n->col, hint,
                                "Variable '%s' already declared", name);
            return;
        }

        /* Type-check initialiser */
        if (n->u.var_decl.init) {
            CimpleType it = check_expr(ctx, n->u.var_decl.init);
            /* Empty array lit */
            if (n->u.var_decl.init->kind == NODE_ARRAY_LIT &&
                n->u.var_decl.init->u.array_lit.elems.count == 0) {
                /* ok — typed by declaration */
            } else if (t == TYPE_BYTE && is_byte_compatible_literal_expr(n->u.var_decl.init)) {
                /* ok */
            } else if (t == TYPE_BYTE_ARR &&
                       array_lit_matches_declared_type(n->u.var_decl.init, t)) {
                coerce_array_lit_to_type(n->u.var_decl.init, t);
                /* ok */
            } else if (it != TYPE_UNKNOWN &&
                       !types_equal_ex(it, n->u.var_decl.init->type_name_hint,
                                       t, n->u.var_decl.struct_name)) {
                int64_t literal_value;
                if (t == TYPE_BYTE && extract_int_literal_expr(n->u.var_decl.init, &literal_value)) {
                    error_byte_literal_out_of_range(n->u.var_decl.init->line,
                                                    n->u.var_decl.init->col,
                                                    literal_value);
                    break;
                }
                if (t == TYPE_BYTE && it == TYPE_INT) {
                    error_semantic_hint(n->u.var_decl.init->line,
                                        n->u.var_decl.init->col,
                                        "Use intToByte() to convert.",
                                        "Cannot assign 'int' to 'byte'");
                    break;
                }
                if (t == TYPE_BYTE_ARR &&
                    report_first_invalid_byte_array_literal(n->u.var_decl.init)) {
                    break;
                }
                char hint[160];
                snprintf(hint, sizeof(hint),
                         "Expected '%s', got '%s'.", type_name(t), type_name(it));
                error_semantic_hint(n->line, n->col, hint,
                                    "Type mismatch in initialization");
            }
        }
        break;
    }

    case NODE_ASSIGN: {
        if (is_reserved_constant_name(n->u.assign.name)) {
            error_semantic(n->line, n->col,
                           "Cannot assign to predefined constant '%s'",
                           n->u.assign.name);
            break;
        }
        Symbol *sym = scope_lookup(ctx->current, n->u.assign.name);
        if (!sym) {
            char hint[160];
            snprintf(hint, sizeof(hint), "'%s' must be declared before use.",
                     n->u.assign.name);
            error_semantic_hint(n->line, n->col, hint,
                                "Undefined variable: '%s'", n->u.assign.name);
            break;
        }
        if (sym->is_const) {
            error_semantic(n->line, n->col,
                           "Cannot assign to predefined constant '%s'", n->u.assign.name);
            break;
        }
        /* Disallow whole-array reassignment */
        if (type_is_array(sym->type)) {
            error_operator_type(n->line, n->col, "=",
                                "a scalar variable on the left-hand side", sym->type);
            break;
        }
        CimpleType vt = check_expr(ctx, n->u.assign.value);
        if (sym->type == TYPE_BYTE && is_byte_compatible_literal_expr(n->u.assign.value)) {
            break;
        }
        if (strcmp(n->u.assign.name, "self") == 0) {
            error_semantic(n->line, n->col, "Cannot reassign 'self'");
            break;
        }
        if (vt != TYPE_UNKNOWN &&
            !types_equal_ex(vt, n->u.assign.value->type_name_hint,
                            sym->type, sym->struct_name)) {
            int64_t literal_value;
            if (sym->type == TYPE_BYTE && extract_int_literal_expr(n->u.assign.value, &literal_value)) {
                error_byte_literal_out_of_range(n->u.assign.value->line,
                                                n->u.assign.value->col,
                                                literal_value);
            } else if (sym->type == TYPE_BYTE && vt == TYPE_INT) {
                error_semantic_hint(n->line, n->col,
                                    "Use intToByte() to convert.",
                                    "Cannot assign 'int' to 'byte'");
            } else {
                error_semantic_hint(n->line, n->col,
                                    "Use toInt(), toFloat(), toString() or toBool() to convert.",
                                    "Type mismatch in assignment");
            }
        }
        break;
    }

    case NODE_INDEX_ASSIGN: {
        Symbol *sym = scope_lookup(ctx->current, n->u.index_assign.name);
        if (!sym) {
            char hint[160];
            snprintf(hint, sizeof(hint), "'%s' must be declared before use.",
                     n->u.index_assign.name);
            error_semantic_hint(n->line, n->col, hint,
                                "Undefined variable: '%s'", n->u.index_assign.name);
            break;
        }
        if (sym->type == TYPE_STRING) {
            error_semantic_hint(n->line, n->col,
                                "Use replace() or build a new string with concatenation.",
                                "Strings are immutable: index assignment is not allowed");
            break;
        }
        if (!type_is_array(sym->type)) {
            error_operator_type(n->line, n->col, "[]", "an array", sym->type);
            break;
        }
        CimpleType idx_t = check_expr(ctx, n->u.index_assign.index);
        if (idx_t != TYPE_INT && idx_t != TYPE_UNKNOWN)
            error_type_mismatch(n->u.index_assign.index->line,
                                n->u.index_assign.index->col,
                                "index expression", TYPE_INT, idx_t);
        CimpleType val_t = check_expr(ctx, n->u.index_assign.value);
        CimpleType elem  = type_elem(sym->type);
        if (elem == TYPE_BYTE && is_byte_compatible_literal_expr(n->u.index_assign.value)) {
            break;
        }
        if (val_t != TYPE_UNKNOWN &&
            !types_equal_ex(val_t, n->u.index_assign.value->type_name_hint, elem, NULL)) {
            int64_t literal_value;
            if (elem == TYPE_BYTE && extract_int_literal_expr(n->u.index_assign.value, &literal_value)) {
                error_byte_literal_out_of_range(n->u.index_assign.value->line,
                                                n->u.index_assign.value->col,
                                                literal_value);
            } else if (elem == TYPE_BYTE && val_t == TYPE_INT) {
                error_semantic_hint(n->u.index_assign.value->line,
                                    n->u.index_assign.value->col,
                                    "Use intToByte() to convert.",
                                    "Cannot assign 'int' to 'byte'");
            } else {
                error_type_mismatch(n->u.index_assign.value->line,
                                    n->u.index_assign.value->col,
                                    "array assignment", elem, val_t);
            }
        }
        break;
    }

    case NODE_MEMBER_ASSIGN: {
        CimpleType lhs_t = check_expr(ctx, n->u.member_assign.target);
        CimpleType rhs_t = check_expr(ctx, n->u.member_assign.value);
        if (rhs_t != TYPE_UNKNOWN &&
            !types_equal_ex(rhs_t, n->u.member_assign.value->type_name_hint,
                            lhs_t, n->u.member_assign.target->type_name_hint)) {
            error_semantic_hint(n->line, n->col,
                                "Use a value of the same member type.",
                                "Type mismatch in assignment");
        }
        break;
    }

    case NODE_BLOCK:
        push_scope(ctx, 0);
        for (int i = 0; i < n->u.block.stmts.count; i++)
            check_stmt(ctx, n->u.block.stmts.items[i]);
        pop_scope(ctx);
        break;

    case NODE_IF:
        check_expr(ctx, n->u.if_stmt.cond);
        if (n->u.if_stmt.cond->type != TYPE_BOOL &&
            n->u.if_stmt.cond->type != TYPE_UNKNOWN)
            error_type_mismatch(n->line, n->col, "if condition",
                                TYPE_BOOL, n->u.if_stmt.cond->type);
        check_stmt(ctx, n->u.if_stmt.then_branch);
        check_stmt(ctx, n->u.if_stmt.else_branch);
        break;

    case NODE_WHILE:
        check_expr(ctx, n->u.while_stmt.cond);
        if (n->u.while_stmt.cond->type != TYPE_BOOL &&
            n->u.while_stmt.cond->type != TYPE_UNKNOWN)
            error_type_mismatch(n->line, n->col, "while condition",
                                TYPE_BOOL, n->u.while_stmt.cond->type);
        ctx->in_loop++;
        check_stmt(ctx, n->u.while_stmt.body);
        ctx->in_loop--;
        break;

    case NODE_FOR: {
        push_scope(ctx, 0);
        /* init */
        if (n->u.for_stmt.init) {
            AstNode *init = n->u.for_stmt.init;
            if (init->kind == NODE_FOR_INIT) {
                Symbol *sym = scope_define(ctx->current,
                                           init->u.for_init.name,
                                           TYPE_INT, NULL,
                                           init->line, init->col);
                if (!sym)
                    error_semantic(init->line, init->col,
                                   "Variable '%s' already declared",
                                   init->u.for_init.name);
                CimpleType et = check_expr(ctx, init->u.for_init.init_expr);
                if (et != TYPE_INT && et != TYPE_UNKNOWN)
                    error_type_mismatch(init->line, init->col,
                                        "for-loop initialization", TYPE_INT, et);
            }
        }
        /* cond */
        check_expr(ctx, n->u.for_stmt.cond);
        if (n->u.for_stmt.cond &&
            n->u.for_stmt.cond->type != TYPE_BOOL &&
            n->u.for_stmt.cond->type != TYPE_UNKNOWN)
            error_type_mismatch(n->line, n->col, "for condition",
                                TYPE_BOOL, n->u.for_stmt.cond->type);
        /* update — just check it */
        check_stmt(ctx, n->u.for_stmt.update);
        /* body */
        ctx->in_loop++;
        check_stmt(ctx, n->u.for_stmt.body);
        ctx->in_loop--;
        pop_scope(ctx);
        break;
    }

    case NODE_RETURN:
        if (n->u.ret.value) {
            CimpleType vt = check_expr(ctx, n->u.ret.value);
            if (ctx->current_ret == TYPE_VOID)
                error_semantic_hint(n->line, n->col,
                                    "A void function cannot return a value.",
                                    "Wrong return type");
            else if (vt != TYPE_UNKNOWN && !types_equal(vt, ctx->current_ret))
                error_semantic_hint(n->line, n->col,
                                    "Expected the function return type here.",
                                    "Wrong return type");
        } else {
            if (ctx->current_ret != TYPE_VOID)
                error_semantic_hint(n->line, n->col,
                                    "Expected the function return type here.",
                                    "Wrong return type");
        }
        break;

    case NODE_BREAK:
    case NODE_CONTINUE:
        if (ctx->in_loop == 0)
            error_semantic(n->line, n->col,
                           "'%s' outside loop",
                           n->kind == NODE_BREAK ? "break" : "continue");
        break;

    case NODE_INCR:
    case NODE_DECR: {
        const char *name = n->u.incr_decr.name;
        Symbol *sym = scope_lookup(ctx->current, name);
        if (!sym)
            error_semantic_hint(n->line, n->col,
                                "The variable must be declared before it can be incremented or decremented.",
                                "Undefined variable: '%s'", name);
        else if (sym->type != TYPE_INT)
            error_operator_type(n->line, n->col,
                                n->kind == NODE_INCR ? "++" : "--",
                                "an int variable", sym->type);
        break;
    }

    case NODE_EXPR_STMT:
        check_expr(ctx, n->u.expr_stmt.expr);
        break;

    case NODE_STRUCT_DECL:
        break;

    default:
        break;
    }
}

/* -----------------------------------------------------------------------
 * First pass — collect all function signatures
 * ----------------------------------------------------------------------- */
static void collect_structs(SemCtx *ctx, AstNode *program) {
    NodeList *decls = &program->u.program.decls;
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d->kind != NODE_STRUCT_DECL) continue;
        if (!struct_table_define(ctx->structs, d->u.struct_decl.name,
                                 d->u.struct_decl.base_name, d)) {
            error_semantic(d->line, d->col,
                           "structure '%s' already declared",
                           d->u.struct_decl.name);
        }
    }
}

static int struct_method_signature_matches(const StructMethodSig *a,
                                           const StructMethodSig *b) {
    if (a->ret_type != b->ret_type) return 0;
    if (!types_equal_ex(a->ret_type, a->ret_struct_name,
                        b->ret_type, b->ret_struct_name)) return 0;
    if (a->param_count != b->param_count) return 0;
    for (int i = 0; i < a->param_count; i++) {
        if (!types_equal_ex(a->params[i].type, a->params[i].struct_name,
                            b->params[i].type, b->params[i].struct_name)) return 0;
    }
    return 1;
}

static int struct_depends_on(StructTable *st, const char *from, const char *target) {
    StructInfo *si = struct_table_lookup(st, from);
    if (!si) return 0;
    for (StructFieldSig *f = si->fields; f; f = f->next) {
        if (f->type == TYPE_STRUCT && f->struct_name) {
            if (strcmp(f->struct_name, target) == 0) return 1;
            if (struct_depends_on(st, f->struct_name, target)) return 1;
        }
    }
    return 0;
}

static void validate_structs(SemCtx *ctx) {
    for (StructInfo *si = ctx->structs->head; si; si = si->next) {
        if (!(si->name[0] >= 'A' && si->name[0] <= 'Z')) {
            error_semantic(si->decl->line, si->decl->col,
                           "Structure name must start with an uppercase letter: '%s'",
                           si->name);
        }
        if (si->base_name && !struct_table_lookup(ctx->structs, si->base_name)) {
            error_semantic(si->decl->line, si->decl->col,
                           "Unknown base structure: '%s'", si->base_name);
        }

        StructInfo *base = si->base_name ? struct_table_lookup(ctx->structs, si->base_name) : NULL;
        NodeList *members = &si->decl->u.struct_decl.members;
        for (int i = 0; i < members->count; i++) {
            AstNode *m = members->items[i];
            if (m->kind == NODE_STRUCT_FIELD) {
                StructFieldSig *field = ALLOC(StructFieldSig);
                field->name = cimple_strdup(m->u.struct_field.name);
                field->type = m->u.struct_field.type;
                field->struct_name = m->u.struct_field.struct_name
                    ? cimple_strdup(m->u.struct_field.struct_name) : NULL;
                field->decl = m;
                field->next = si->fields;
                si->fields = field;

                if (field->type == TYPE_STRUCT && !m->u.struct_field.init) {
                    error_semantic_hint(m->line, m->col,
                                        "Fields of structure type must be explicitly initialized with 'clone StructureName'.",
                                        "Field '%s' in structure '%s' has no default value",
                                        field->name, si->name);
                }
                if (field->type == TYPE_STRUCT && (!field->struct_name || !struct_table_lookup(ctx->structs, field->struct_name))) {
                    error_semantic(m->line, m->col,
                                   "Unknown type: '%s'", field->struct_name ? field->struct_name : "<unknown>");
                }
                if (field->type == TYPE_STRUCT &&
                    (!m->u.struct_field.init ||
                     m->u.struct_field.init->kind != NODE_CLONE ||
                     strcmp(m->u.struct_field.init->u.clone_expr.struct_name, field->struct_name) != 0)) {
                    error_semantic_hint(m->line, m->col,
                                        "Fields of structure type must be explicitly initialized with 'clone StructureName'.",
                                        "Field '%s' in structure '%s' has no default value",
                                        field->name, si->name);
                }
                if (field->type == TYPE_STRUCT && field->struct_name &&
                    (strcmp(field->struct_name, si->name) == 0 ||
                     struct_depends_on(ctx->structs, field->struct_name, si->name))) {
                    error_semantic_hint(m->line, m->col,
                                        "Structures cannot contain fields of their own type (direct or indirect).",
                                        "Recursive field '%s' in structure '%s'",
                                        field->name, si->name);
                }
                for (StructFieldSig *other = si->fields->next; other; other = other->next) {
                    if (strcmp(other->name, field->name) == 0) {
                        error_semantic(m->line, m->col,
                                       "Duplicate field '%s' in structure '%s'",
                                       field->name, si->name);
                    }
                }
                if (base) {
                    StructFieldSig *base_field = struct_lookup_field(ctx->structs, base, field->name);
                    if (base_field &&
                        !types_equal_ex(field->type, field->struct_name,
                                        base_field->type, base_field->struct_name)) {
                        error_semantic_hint(m->line, m->col,
                                            "Redefined fields must have the same type (default value may differ).",
                                            "Field '%s' in '%s' redefines '%s.%s' with a different type",
                                            field->name, si->name, base->name, field->name);
                    }
                }
            } else if (m->kind == NODE_FUNC_DECL) {
                StructMethodSig *method = ALLOC(StructMethodSig);
                method->name = cimple_strdup(m->u.func_decl.name);
                method->ret_type = m->u.func_decl.ret_type;
                method->ret_struct_name = m->u.func_decl.ret_struct_name
                    ? cimple_strdup(m->u.func_decl.ret_struct_name) : NULL;
                method->param_count = m->u.func_decl.params.count;
                method->params = method->param_count ? ALLOC_N(FuncParam, method->param_count) : NULL;
                for (int j = 0; j < method->param_count; j++) {
                    AstNode *p = m->u.func_decl.params.items[j];
                    method->params[j].name = cimple_strdup(p->u.param.name);
                    method->params[j].type = p->u.param.type;
                    method->params[j].struct_name = p->u.param.struct_name
                        ? cimple_strdup(p->u.param.struct_name) : NULL;
                }
                method->decl = m;
                method->next = si->methods;
                si->methods = method;
                for (StructMethodSig *other = si->methods->next; other; other = other->next) {
                    if (strcmp(other->name, method->name) == 0) {
                        error_semantic(m->line, m->col,
                                       "Duplicate method '%s' in structure '%s'",
                                       method->name, si->name);
                    }
                }
                if (base) {
                    StructMethodSig *base_method = struct_lookup_method(ctx->structs, base, method->name);
                    if (base_method && !struct_method_signature_matches(method, base_method)) {
                        error_semantic_hint(m->line, m->col,
                                            "Signatures must be identical to override a method.",
                                            "Method '%s' in '%s' overrides '%s.%s' with a different signature",
                                            method->name, si->name, base->name, method->name);
                    }
                }
            }
        }
    }
}

static void collect_functions(SemCtx *ctx, AstNode *program) {
    NodeList *decls = &program->u.program.decls;
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d->kind != NODE_FUNC_DECL) continue;

        const char *name = d->u.func_decl.name;

        /* Check builtin collision */
        if (builtin_lookup(name)) {
            error_semantic(d->line, d->col,
                           "function '%s' conflicts with a built-in function", name);
            continue;
        }

        NodeList *params = &d->u.func_decl.params;
        FuncParam *fp = params->count > 0
                        ? ALLOC_N(FuncParam, params->count)
                        : NULL;
        for (int j = 0; j < params->count; j++) {
            fp[j].name = cimple_strdup(params->items[j]->u.param.name);
            fp[j].type = params->items[j]->u.param.type;
            fp[j].struct_name = params->items[j]->u.param.struct_name
                ? cimple_strdup(params->items[j]->u.param.struct_name) : NULL;
        }

        if (!func_table_define(ctx->funcs, name,
                               d->u.func_decl.ret_type,
                               d->u.func_decl.ret_struct_name,
                               fp, params->count,
                               d->line, d->col)) {
            error_semantic(d->line, d->col,
                           "function '%s' already declared", name);
        }
        for (int j = 0; j < params->count; j++)
            free(fp[j].struct_name);
        free(fp);
    }
}

static void check_struct_methods(SemCtx *ctx) {
    for (StructInfo *si = ctx->structs->head; si; si = si->next) {
        for (StructMethodSig *method = si->methods; method; method = method->next) {
            AstNode *f = method->decl;
            push_scope(ctx, 1);
            ctx->current_ret = f->u.func_decl.ret_type;
            ctx->current_struct_name = si->name;
            ctx->current_base_name = si->base_name;
            ctx->in_method = 1;

            scope_define(ctx->current, "self", TYPE_STRUCT, si->name, f->line, f->col);
            for (int i = 0; i < f->u.func_decl.params.count; i++) {
                AstNode *p = f->u.func_decl.params.items[i];
                scope_define(ctx->current, p->u.param.name, p->u.param.type,
                             p->u.param.struct_name, p->line, p->col);
            }
            NodeList *stmts = &f->u.func_decl.body->u.block.stmts;
            for (int i = 0; i < stmts->count; i++)
                check_stmt(ctx, stmts->items[i]);
            if (ctx->current_ret != TYPE_VOID &&
                !block_always_returns(f->u.func_decl.body)) {
                error_semantic_hint(f->line, f->col,
                                    "Function returns a value but not all paths return one.",
                                    "Missing return in function '%s'",
                                    f->u.func_decl.name);
            }
            ctx->current_struct_name = NULL;
            ctx->current_base_name = NULL;
            ctx->in_method = 0;
            pop_scope(ctx);
        }
    }
}

/* -----------------------------------------------------------------------
 * Check a single function declaration
 * ----------------------------------------------------------------------- */
static void check_func(SemCtx *ctx, AstNode *f) {
    push_scope(ctx, 1);
    ctx->current_ret = f->u.func_decl.ret_type;

    /* Define parameters */
    NodeList *params = &f->u.func_decl.params;
    for (int i = 0; i < params->count; i++) {
        AstNode *p = params->items[i];
        scope_define(ctx->current, p->u.param.name, p->u.param.type,
                     p->u.param.struct_name, p->line, p->col);
    }

    /* Check body */
    AstNode *body = f->u.func_decl.body;
    if (body) {
        /* Check statements inside the block without creating a new scope
         * (the function scope IS the parameter scope). */
        NodeList *stmts = &body->u.block.stmts;
        for (int i = 0; i < stmts->count; i++)
            check_stmt(ctx, stmts->items[i]);

        /* Return-path analysis for non-void functions */
        if (ctx->current_ret != TYPE_VOID) {
            if (!block_always_returns(body))
                error_semantic_hint(f->line, f->col,
                                    "Function returns a value but not all paths return one.",
                                    "Missing return in function '%s'",
                                    f->u.func_decl.name);
        }
    }

    pop_scope(ctx);
}

/* -----------------------------------------------------------------------
 * check_main — validate the main function signature
 * ----------------------------------------------------------------------- */
static void check_main(SemCtx *ctx, AstNode *program) {
    (void)ctx;
    NodeList *decls = &program->u.program.decls;
    AstNode  *main_fn = NULL;

    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d->kind == NODE_FUNC_DECL &&
            strcmp(d->u.func_decl.name, "main") == 0) {
            main_fn = d;
            break;
        }
    }

    if (!main_fn) {
        error_semantic(1, 1, "program has no 'main' function");
        return;
    }

    CimpleType ret    = main_fn->u.func_decl.ret_type;
    int        npar   = main_fn->u.func_decl.params.count;
    NodeList  *params = &main_fn->u.func_decl.params;

    if (ret != TYPE_INT && ret != TYPE_VOID) {
        error_semantic_hint(main_fn->line, main_fn->col,
                            "Valid signatures: void main(), int main(), void main(string[] args), int main(string[] args).",
                            "Invalid signature for 'main'");
    }

    if (npar == 0) {
        /* ok */
    } else if (npar == 1) {
        CimpleType pt = params->items[0]->u.param.type;
        if (pt != TYPE_STR_ARR)
            error_semantic_hint(main_fn->line, main_fn->col,
                                "Valid signatures: void main(), int main(), void main(string[] args), int main(string[] args).",
                                "Invalid signature for 'main'");
    } else {
        error_semantic_hint(main_fn->line, main_fn->col,
                            "Valid signatures: void main(), int main(), void main(string[] args), int main(string[] args).",
                            "Invalid signature for 'main'");
    }
}

/* -----------------------------------------------------------------------
 * Public entry point
 * ----------------------------------------------------------------------- */
int semantic_check(AstNode *program) {
    SemCtx ctx;
    ctx.funcs        = func_table_new();
    ctx.structs      = struct_table_new();
    ctx.global_scope = scope_new(NULL, 0);
    ctx.current      = ctx.global_scope;
    ctx.current_ret  = TYPE_VOID;
    ctx.in_loop      = 0;
    ctx.has_return   = 0;
    ctx.current_struct_name = NULL;
    ctx.current_base_name = NULL;
    ctx.in_method = 0;

    collect_structs(&ctx, program);
    validate_structs(&ctx);
    /* Collect all function signatures first */
    collect_functions(&ctx, program);

    /* Check global variables */
    NodeList *decls = &program->u.program.decls;
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d->kind == NODE_VAR_DECL) {
            check_stmt(&ctx, d);
        }
    }

    /* Check main signature */
    check_main(&ctx, program);

    /* Check each function body */
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d->kind == NODE_FUNC_DECL) {
            ctx.in_loop = 0;
            check_func(&ctx, d);
        }
    }
    check_struct_methods(&ctx);

    int had_errors = error_flush_semantic();

    func_table_free(ctx.funcs);
    struct_table_free(ctx.structs);
    scope_free(ctx.global_scope);

    return had_errors;
}
