#ifndef CIMPLE_INTERPRETER_H
#define CIMPLE_INTERPRETER_H

#include "../ast/ast.h"
#include "../runtime/value.h"
#include "../runtime/scope.h"

/* -----------------------------------------------------------------------
 * Control-flow signals
 * ----------------------------------------------------------------------- */
typedef enum {
    SIGNAL_NONE     = 0,
    SIGNAL_RETURN   = 1,
    SIGNAL_BREAK    = 2,
    SIGNAL_CONTINUE = 3
} Signal;

typedef struct FuncDeclEntry {
    const char *name;
    AstNode *decl;
    struct FuncDeclEntry *next;
} FuncDeclEntry;

typedef struct {
    FuncDeclEntry **buckets;
    int bucket_count;
} FuncDeclTable;

/* -----------------------------------------------------------------------
 * Interpreter context
 * ----------------------------------------------------------------------- */
typedef struct {
    AstNode    *program;
    Scope      *global;
    FuncTable  *funcs;      /* user-defined function table */
    FuncDeclTable *func_decls;

    /* Current execution state */
    Signal      signal;
    Value       ret_val;    /* value set by SIGNAL_RETURN */
} Interp;

/* -----------------------------------------------------------------------
 * API
 * ----------------------------------------------------------------------- */
int interp_run(AstNode *program, int argc, char **argv);

#endif /* CIMPLE_INTERPRETER_H */
