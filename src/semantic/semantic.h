#ifndef CIMPLE_SEMANTIC_H
#define CIMPLE_SEMANTIC_H

#include "../ast/ast.h"
#include "../runtime/scope.h"

typedef struct StructTable StructTable;
typedef struct UnionTable UnionTable;

/* -----------------------------------------------------------------------
 * Semantic analysis context
 * ----------------------------------------------------------------------- */
typedef struct {
    AstNode   *program;       /* current program root                */
    FuncTable *funcs;         /* all declared functions              */
    StructTable *structs;     /* all declared structures             */
    UnionTable *unions;       /* all declared unions                 */
    Scope     *global_scope;  /* global variable scope               */
    Scope     *current;       /* current innermost scope             */

    /* Current function context */
    CimpleType  current_ret;  /* return type of the function being checked */
    const char *current_ret_struct;
    int         in_loop;      /* depth counter for break/continue checking */
    int         has_return;   /* used during path analysis                */
    const char *current_struct_name;
    const char *current_base_name;
    int         in_method;
} SemCtx;

/* -----------------------------------------------------------------------
 * Entry point
 * Returns 0 on success, 1 if semantic errors were found.
 * ----------------------------------------------------------------------- */
int semantic_check(AstNode *program);

/* -----------------------------------------------------------------------
 * Built-in function signature table (also used by interpreter)
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *name;
    CimpleType  ret_type;
    /* param types array (up to 8 params) */
    CimpleType  param_types[8];
    int         param_count;
    int         variadic;          /* allow extra args (e.g. format) */
    int         polymorphic;       /* intrinsic array functions       */
} BuiltinSig;

const BuiltinSig *builtin_lookup(const char *name);
/* For intrinsic conversions (toString etc.) that depend on argument type */
CimpleType builtin_resolve_intrinsic(const char *name, CimpleType arg_type,
                                      int line, int col);

#endif /* CIMPLE_SEMANTIC_H */
