#include "semantic.h"
#include "../common/error.h"
#include "../common/memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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
    { "fileExists",  TYPE_BOOL,     { TYPE_STRING },              1, 0, 0 },
    { "readLines",   TYPE_STR_ARR,  { TYPE_STRING },              1, 0, 0 },
    { "writeLines",  TYPE_VOID,     { TYPE_STRING, TYPE_STR_ARR }, 2, 0, 0 },

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

/* Resolve intrinsic conversion overloads statically. */
CimpleType builtin_resolve_intrinsic(const char *name, CimpleType arg_type,
                                      int line, int col) {
    if (strcmp(name, "toString") == 0) {
        if (arg_type == TYPE_INT || arg_type == TYPE_FLOAT || arg_type == TYPE_BOOL)
            return TYPE_STRING;
        error_operator_type(line, col, "toString()", "int, float or bool", arg_type);
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
        return sym->type;
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
            if (lt != TYPE_BOOL)
                error_operator_type(n->line, n->col,
                                    op == OP_AND ? "&&" : "||",
                                    "bool operands", lt);
            if (rt != TYPE_BOOL)
                error_operator_type(n->line, n->col,
                                    op == OP_AND ? "&&" : "||",
                                    "bool operands", rt);
            n->type = TYPE_BOOL;
            return TYPE_BOOL;
        }

        /* Comparison */
        if (op == OP_EQ || op == OP_NEQ || op == OP_LT ||
            op == OP_LEQ || op == OP_GT || op == OP_GEQ) {
            if (!types_equal(lt, rt))
                error_type_mismatch(n->line, n->col, "comparison", lt, rt);
            n->type = TYPE_BOOL;
            return TYPE_BOOL;
        }

        /* Arithmetic (+, -, *, /, %) */
        if (op == OP_ADD || op == OP_SUB || op == OP_MUL ||
            op == OP_DIV || op == OP_MOD) {
            if (lt == TYPE_INT && rt == TYPE_INT) {
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
            if (lt != TYPE_INT || rt != TYPE_INT)
                error_operator_type(n->line, n->col,
                                    op == OP_BAND ? "&" :
                                    op == OP_BOR ? "|" :
                                    op == OP_BXOR ? "^" :
                                    op == OP_LSHIFT ? "<<" : ">>",
                                    "int operands", lt != TYPE_INT ? lt : rt);
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
            if (t != TYPE_BOOL)
                error_operator_type(n->line, n->col, "!", "a bool operand", t);
            n->type = TYPE_BOOL; return TYPE_BOOL;
        }
        if (op == OP_NEG) {
            if (t != TYPE_INT && t != TYPE_FLOAT)
                error_operator_type(n->line, n->col, "-", "a numeric operand", t);
            n->type = t; return t;
        }
        if (op == OP_BNOT) {
            if (t != TYPE_INT)
                error_operator_type(n->line, n->col, "~", "an int operand", t);
            n->type = TYPE_INT; return TYPE_INT;
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

                /* Determine return type for poly ops */
                CimpleType ret = sig->ret_type;
                if (ret == TYPE_UNKNOWN && nargs > 0 && type_is_array(arg_types[0]))
                    ret = type_elem(arg_types[0]);
                n->type = ret;
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
                    if (expected != TYPE_UNKNOWN &&
                        arg_types[i] != TYPE_UNKNOWN &&
                        !types_equal(arg_types[i], expected)) {
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
                if (!types_equal(arg_types[i], fsig->params[i].type))
                    error_type_mismatch(args->items[i]->line, args->items[i]->col,
                                        "function call", fsig->params[i].type, arg_types[i]);
            }
        }
        n->type = fsig->ret_type;
        return fsig->ret_type;
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
        Symbol *sym = scope_define(ctx->current, name, t, n->line, n->col);
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
            } else if (it != TYPE_UNKNOWN && !types_equal(it, t)) {
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
        if (vt != TYPE_UNKNOWN && !types_equal(vt, sym->type))
            error_semantic_hint(n->line, n->col,
                                "Use toInt(), toFloat(), toString() or toBool() to convert.",
                                "Type mismatch in assignment");
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
        if (val_t != TYPE_UNKNOWN && !types_equal(val_t, elem))
            error_type_mismatch(n->u.index_assign.value->line,
                                n->u.index_assign.value->col,
                                "array assignment", elem, val_t);
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
                                           TYPE_INT, init->line, init->col);
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

    default:
        break;
    }
}

/* -----------------------------------------------------------------------
 * First pass — collect all function signatures
 * ----------------------------------------------------------------------- */
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
        }

        if (!func_table_define(ctx->funcs, name,
                               d->u.func_decl.ret_type,
                               fp, params->count,
                               d->line, d->col)) {
            error_semantic(d->line, d->col,
                           "function '%s' already declared", name);
        }
        free(fp);
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
                     p->line, p->col);
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
    ctx.global_scope = scope_new(NULL, 0);
    ctx.current      = ctx.global_scope;
    ctx.current_ret  = TYPE_VOID;
    ctx.in_loop      = 0;
    ctx.has_return   = 0;

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

    int had_errors = error_flush_semantic();

    func_table_free(ctx.funcs);
    scope_free(ctx.global_scope);

    return had_errors;
}
