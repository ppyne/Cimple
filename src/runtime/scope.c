#include "scope.h"
#include "../common/memory.h"
#include <string.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------
 * Simple hash function
 * ----------------------------------------------------------------------- */
static unsigned int hash_str(const char *s, int buckets) {
    unsigned int h = 5381u;
    while (*s)
        h = h * 33u ^ (unsigned char)*s++;
    return h % (unsigned int)buckets;
}

/* -----------------------------------------------------------------------
 * Scope implementation
 * ----------------------------------------------------------------------- */
Scope *scope_new(Scope *parent, int is_function_scope) {
    Scope *s = ALLOC(Scope);
    memset(s->buckets, 0, sizeof(s->buckets));
    s->parent             = parent;
    s->is_function_scope  = is_function_scope;
    return s;
}

void scope_free(Scope *s) {
    if (!s) return;
    for (int i = 0; i < SCOPE_BUCKET_COUNT; i++) {
        Symbol *sym = s->buckets[i];
        while (sym) {
            Symbol *next = sym->next;
            free(sym->name);
            free(sym->struct_name);
            func_type_free(sym->func_type);
            value_free(&sym->val);
            free(sym);
            sym = next;
        }
    }
    free(s);
}

Symbol *scope_lookup_local(Scope *s, const char *name) {
    unsigned int h = hash_str(name, SCOPE_BUCKET_COUNT);
    Symbol *sym = s->buckets[h];
    while (sym) {
        if (strcmp(sym->name, name) == 0)
            return sym;
        sym = sym->next;
    }
    return NULL;
}

Symbol *scope_lookup(Scope *s, const char *name) {
    while (s) {
        Symbol *sym = scope_lookup_local(s, name);
        if (sym) return sym;
        s = s->parent;
    }
    return NULL;
}

Symbol *scope_define(Scope *s, const char *name, CimpleType type,
                     const char *struct_name, const FuncType *func_type,
                     int line, int col) {
    if (scope_lookup_local(s, name))
        return NULL; /* already defined */
    Symbol *sym  = ALLOC(Symbol);
    sym->name     = cimple_strdup(name);
    sym->type     = type;
    sym->struct_name = struct_name ? cimple_strdup(struct_name) : NULL;
    sym->func_type = func_type_copy(func_type);
    sym->val      = value_default(type);
    sym->is_const = 0;
    sym->line     = line;
    sym->col      = col;
    unsigned int h = hash_str(name, SCOPE_BUCKET_COUNT);
    sym->next      = s->buckets[h];
    s->buckets[h]  = sym;
    return sym;
}

/* -----------------------------------------------------------------------
 * FuncTable implementation
 * ----------------------------------------------------------------------- */
#define FT_BUCKETS 64

FuncTable *func_table_new(void) {
    FuncTable *ft = ALLOC(FuncTable);
    ft->bucket_count = FT_BUCKETS;
    ft->buckets = ALLOC_N(FuncSig *, FT_BUCKETS);
    return ft;
}

void func_table_free(FuncTable *ft) {
    if (!ft) return;
    for (int i = 0; i < ft->bucket_count; i++) {
        FuncSig *sig = ft->buckets[i];
        while (sig) {
            FuncSig *next = sig->next;
            free(sig->name);
            free(sig->ret_struct_name);
            for (int j = 0; j < sig->param_count; j++)
                free(sig->params[j].name);
            for (int j = 0; j < sig->param_count; j++)
                free(sig->params[j].struct_name);
            for (int j = 0; j < sig->param_count; j++)
                func_type_free(sig->params[j].func_type);
            free(sig->params);
            free(sig);
            sig = next;
        }
    }
    free(ft->buckets);
    free(ft);
}

FuncSig *func_table_lookup(FuncTable *ft, const char *name) {
    unsigned int h = hash_str(name, ft->bucket_count);
    FuncSig *sig = ft->buckets[h];
    while (sig) {
        if (strcmp(sig->name, name) == 0)
            return sig;
        sig = sig->next;
    }
    return NULL;
}

int func_table_define(FuncTable *ft, const char *name,
                      CimpleType ret_type,
                      const char *ret_struct_name,
                      FuncParam *params, int param_count,
                      int line, int col) {
    if (func_table_lookup(ft, name))
        return 0; /* already defined */
    FuncSig *sig   = ALLOC(FuncSig);
    sig->name      = cimple_strdup(name);
    sig->ret_type  = ret_type;
    sig->ret_struct_name = ret_struct_name ? cimple_strdup(ret_struct_name) : NULL;
    sig->param_count = param_count;
    sig->params    = ALLOC_N(FuncParam, param_count);
    for (int i = 0; i < param_count; i++) {
        sig->params[i].name = cimple_strdup(params[i].name);
        sig->params[i].type = params[i].type;
        sig->params[i].struct_name = params[i].struct_name
            ? cimple_strdup(params[i].struct_name) : NULL;
        sig->params[i].func_type = func_type_copy(params[i].func_type);
    }
    sig->line      = line;
    sig->col       = col;
    unsigned int h = hash_str(name, ft->bucket_count);
    sig->next      = ft->buckets[h];
    ft->buckets[h] = sig;
    return 1;
}
