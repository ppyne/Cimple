#ifndef CIMPLE_SCOPE_H
#define CIMPLE_SCOPE_H

#include "../ast/ast.h"
#include "value.h"

/* -----------------------------------------------------------------------
 * Symbol (variable entry in a scope)
 * ----------------------------------------------------------------------- */
typedef struct Symbol {
    char       *name;
    CimpleType  type;
    char       *struct_name;
    FuncType   *func_type;
    Value       val;         /* runtime value (interpreter phase)         */
    int         is_const;   /* predefined constant                        */
    int         line;        /* declaration site                          */
    int         col;
    struct Symbol *next;    /* chained in bucket                          */
} Symbol;

/* -----------------------------------------------------------------------
 * Scope (linked list of frames)
 * ----------------------------------------------------------------------- */
#define SCOPE_BUCKET_COUNT 64

typedef struct Scope {
    Symbol    *buckets[SCOPE_BUCKET_COUNT];
    struct Scope *parent;
    int        is_function_scope; /* true for the outermost scope of a fn */
} Scope;

/* -----------------------------------------------------------------------
 * Function signature (for semantic checks)
 * ----------------------------------------------------------------------- */
typedef struct FuncParam {
    char       *name;
    CimpleType  type;
    char       *struct_name;
    FuncType   *func_type;
} FuncParam;

typedef struct FuncSig {
    char       *name;
    CimpleType  ret_type;
    char       *ret_struct_name;
    FuncParam  *params;
    int         param_count;
    int         line;
    int         col;
    struct FuncSig *next;
} FuncSig;

/* -----------------------------------------------------------------------
 * API — Scopes
 * ----------------------------------------------------------------------- */
Scope  *scope_new(Scope *parent, int is_function_scope);
void    scope_free(Scope *s);

/* Look up a symbol by name (traverses parent chain). */
Symbol *scope_lookup(Scope *s, const char *name);

/* Look up only in the immediate scope (no parent traversal). */
Symbol *scope_lookup_local(Scope *s, const char *name);

/* Define a new symbol (returns NULL on redefinition). */
Symbol *scope_define(Scope *s, const char *name, CimpleType type,
                     const char *struct_name, const FuncType *func_type,
                     int line, int col);

/* -----------------------------------------------------------------------
 * API — Functions
 * ----------------------------------------------------------------------- */
typedef struct {
    FuncSig **buckets;
    int       bucket_count;
} FuncTable;

FuncTable *func_table_new(void);
void       func_table_free(FuncTable *ft);
FuncSig   *func_table_lookup(FuncTable *ft, const char *name);
int        func_table_define(FuncTable *ft, const char *name,
                             CimpleType ret_type,
                             const char *ret_struct_name,
                             FuncParam *params, int param_count,
                             int line, int col);

#endif /* CIMPLE_SCOPE_H */
