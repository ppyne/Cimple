#include "interpreter.h"
#include "builtins.h"
#include "../common/error.h"
#include "../common/memory.h"
#include "../semantic/semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* Forward declarations */
static Value eval_expr(Interp *ip, Scope *scope, AstNode *n);
static void  exec_stmt(Interp *ip, Scope *scope, AstNode *n);
static Value call_func(Interp *ip, const char *name, Value *args, int nargs,
                        int line, int col);
static AstNode *find_func_decl(Interp *ip, const char *name);
static AstNode *find_struct_decl(Interp *ip, const char *name);
static FuncDeclTable *func_decl_table_build(AstNode *program);
static void func_decl_table_free(FuncDeclTable *table);
static StructDeclTable *struct_decl_table_build(AstNode *program);
static void struct_decl_table_free(StructDeclTable *table);
static Value clone_struct_instance(Interp *ip, const char *name, Scope *scope, int line, int col);
static StructFieldVal *struct_field_lookup(StructVal *st, const char *name);
static AstNode *find_struct_method_decl(Interp *ip, const char *struct_name,
                                        const char *method_name, int use_base_only);
static const char *find_struct_method_owner(Interp *ip, const char *struct_name,
                                            const char *method_name, int use_base_only);
static Value call_method(Interp *ip, Value *base, const char *method_name,
                         int use_base_only, Value *args, int nargs,
                         int line, int col);
static Value *resolve_value_lvalue(Interp *ip, Scope *scope, AstNode *n, int line, int col);
static Value *resolve_struct_lvalue(Interp *ip, Scope *scope, AstNode *n, int line, int col);

Interp *g_current_interp = NULL;

static int interp_has_throw(Interp *ip) {
    return ip && ip->signal == SIGNAL_THROW;
}

static void interp_clear_throw(Interp *ip) {
    if (!ip) return;
    value_free(&ip->throw_val);
    ip->throw_val = val_void();
    free(ip->throw_type_name);
    ip->throw_type_name = NULL;
    ip->throw_line = 0;
    ip->throw_col = 0;
}

static int is_mutating_array_builtin(const char *name) {
    return strcmp(name, "arrayPush") == 0 ||
           strcmp(name, "arrayPop") == 0 ||
           strcmp(name, "arrayInsert") == 0 ||
           strcmp(name, "arrayRemove") == 0 ||
           strcmp(name, "arraySet") == 0;
}

static int is_map_builtin(const char *name) {
    return strcmp(name, "mapHas") == 0 ||
           strcmp(name, "mapRemove") == 0 ||
           strcmp(name, "mapKeys") == 0 ||
           strcmp(name, "mapValues") == 0 ||
           strcmp(name, "mapSize") == 0 ||
           strcmp(name, "mapClear") == 0 ||
           strcmp(name, "count") == 0;
}

static int is_int_like_value(CimpleType t) {
    return t == TYPE_INT || t == TYPE_BYTE;
}

static uint64_t hash_name(const char *name) {
    uint64_t hash = UINT64_C(1469598103934665603);
    while (*name) {
        hash ^= (unsigned char)*name++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static FuncDeclTable *func_decl_table_build(AstNode *program) {
    NodeList *decls = &program->u.program.decls;
    int func_count = 0;
    for (int i = 0; i < decls->count; i++) {
        if (decls->items[i]->kind == NODE_FUNC_DECL) func_count++;
    }

    int bucket_count = 32;
    while (bucket_count < func_count * 2) bucket_count <<= 1;

    FuncDeclTable *table = ALLOC(FuncDeclTable);
    table->bucket_count = bucket_count;
    table->buckets = ALLOC_N(FuncDeclEntry *, (size_t)bucket_count);

    for (int i = 0; i < decls->count; i++) {
        AstNode *decl = decls->items[i];
        if (decl->kind != NODE_FUNC_DECL) continue;

        uint64_t hash = hash_name(decl->u.func_decl.name);
        int bucket = (int)(hash & (unsigned long)(bucket_count - 1));
        FuncDeclEntry *entry = ALLOC(FuncDeclEntry);
        entry->name = decl->u.func_decl.name;
        entry->decl = decl;
        entry->next = table->buckets[bucket];
        table->buckets[bucket] = entry;
    }

    return table;
}

static void func_decl_table_free(FuncDeclTable *table) {
    if (!table) return;
    for (int i = 0; i < table->bucket_count; i++) {
        FuncDeclEntry *entry = table->buckets[i];
        while (entry) {
            FuncDeclEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(table->buckets);
    free(table);
}

static StructDeclTable *struct_decl_table_build(AstNode *program) {
    NodeList *decls = &program->u.program.decls;
    int count = 0;
    for (int i = 0; i < decls->count; i++) {
        if (decls->items[i]->kind == NODE_STRUCT_DECL) count++;
    }
    int bucket_count = 32;
    while (bucket_count < count * 2) bucket_count <<= 1;
    StructDeclTable *table = ALLOC(StructDeclTable);
    table->bucket_count = bucket_count;
    table->buckets = ALLOC_N(StructDeclEntry *, (size_t)bucket_count);
    for (int i = 0; i < decls->count; i++) {
        AstNode *decl = decls->items[i];
        if (decl->kind != NODE_STRUCT_DECL) continue;
        int bucket = (int)(hash_name(decl->u.struct_decl.name) & (unsigned long)(bucket_count - 1));
        StructDeclEntry *entry = ALLOC(StructDeclEntry);
        entry->name = decl->u.struct_decl.name;
        entry->decl = decl;
        entry->next = table->buckets[bucket];
        table->buckets[bucket] = entry;
    }
    return table;
}

static void struct_decl_table_free(StructDeclTable *table) {
    if (!table) return;
    for (int i = 0; i < table->bucket_count; i++) {
        StructDeclEntry *entry = table->buckets[i];
        while (entry) {
            StructDeclEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(table->buckets);
    free(table);
}

static UnionDeclTable *union_decl_table_build(AstNode *program) {
    NodeList *decls = &program->u.program.decls;
    int count = 0;
    for (int i = 0; i < decls->count; i++) {
        if (decls->items[i]->kind == NODE_UNION_DECL) count++;
    }
    int bucket_count = 32;
    while (bucket_count < count * 2) bucket_count <<= 1;
    UnionDeclTable *table = ALLOC(UnionDeclTable);
    table->bucket_count = bucket_count;
    table->buckets = ALLOC_N(UnionDeclEntry *, (size_t)bucket_count);
    for (int i = 0; i < decls->count; i++) {
        AstNode *decl = decls->items[i];
        if (decl->kind != NODE_UNION_DECL) continue;
        int bucket = (int)(hash_name(decl->u.union_decl.name) & (unsigned long)(bucket_count - 1));
        UnionDeclEntry *entry = ALLOC(UnionDeclEntry);
        entry->name = decl->u.union_decl.name;
        entry->decl = decl;
        entry->next = table->buckets[bucket];
        table->buckets[bucket] = entry;
    }
    return table;
}

static void union_decl_table_free(UnionDeclTable *table) {
    if (!table) return;
    for (int i = 0; i < table->bucket_count; i++) {
        UnionDeclEntry *entry = table->buckets[i];
        while (entry) {
            UnionDeclEntry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
    free(table->buckets);
    free(table);
}

/* -----------------------------------------------------------------------
 * Predefined constants
 * ----------------------------------------------------------------------- */
#include <stdint.h>
#include <float.h>
static Value eval_constant(const char *name) {
    if (strcmp(name, "INT_MAX")       == 0) return val_int(INT64_MAX);
    if (strcmp(name, "INT_MIN")       == 0) return val_int(INT64_MIN);
    if (strcmp(name, "INT_SIZE")      == 0) return val_int(8);
    if (strcmp(name, "FLOAT_SIZE")    == 0) return val_int(8);
    if (strcmp(name, "FLOAT_DIG")     == 0) return val_int(15);
    if (strcmp(name, "FLOAT_EPSILON") == 0) return val_float(DBL_EPSILON);
    if (strcmp(name, "FLOAT_MIN")     == 0) return val_float(DBL_MIN);
    if (strcmp(name, "FLOAT_MAX")     == 0) return val_float(DBL_MAX);
    if (strcmp(name, "M_PI")          == 0) return val_float(3.141592653589793);
    if (strcmp(name, "M_E")           == 0) return val_float(2.718281828459045);
    if (strcmp(name, "M_TAU")         == 0) return val_float(6.283185307179586);
    if (strcmp(name, "M_SQRT2")       == 0) return val_float(1.4142135623730951);
    if (strcmp(name, "M_LN2")         == 0) return val_float(0.6931471805599453);
    if (strcmp(name, "M_LN10")        == 0) return val_float(2.302585092994046);
    return val_void();
}

static AstNode *find_struct_decl(Interp *ip, const char *name) {
    if (!ip->struct_decls) return NULL;
    int bucket = (int)(hash_name(name) & (unsigned long)(ip->struct_decls->bucket_count - 1));
    for (StructDeclEntry *entry = ip->struct_decls->buckets[bucket]; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) return entry->decl;
    }
    return NULL;
}

static AstNode *find_union_decl(Interp *ip, const char *name) {
    if (!ip->union_decls) return NULL;
    int bucket = (int)(hash_name(name) & (unsigned long)(ip->union_decls->bucket_count - 1));
    for (UnionDeclEntry *entry = ip->union_decls->buckets[bucket]; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) return entry->decl;
    }
    return NULL;
}

int interp_struct_is_subtype(Interp *ip, const char *actual, const char *expected) {
    if (!actual || !expected) return 0;
    if (strcmp(actual, expected) == 0) return 1;
    if (strcmp(expected, "Exception") == 0 &&
        (strcmp(actual, "RuntimeException") == 0 ||
         strcmp(actual, "IoException") == 0 ||
         strcmp(actual, "RegExpException") == 0))
        return 1;
    if ((strcmp(actual, "RuntimeException") == 0 ||
         strcmp(actual, "IoException") == 0 ||
         strcmp(actual, "RegExpException") == 0) &&
        strcmp(expected, "Exception") != 0)
        return 0;
    for (AstNode *decl = find_struct_decl(ip, actual);
         decl && decl->u.struct_decl.base_name;
         decl = find_struct_decl(ip, decl->u.struct_decl.base_name)) {
        if (strcmp(decl->u.struct_decl.base_name, expected) == 0) return 1;
    }
    return 0;
}

void interp_throw(Interp *ip, const char *type_name, Value val) {
    if (!ip || ip->signal != SIGNAL_NONE) {
        value_free(&val);
        return;
    }
    ip->signal = SIGNAL_THROW;
    ip->throw_val = val;
    ip->throw_type_name = cimple_strdup(type_name ? type_name : "Exception");
}

void interp_throw_builtin(Interp *ip, const char *type_name,
                          const char *message, const char *path) {
    if (!ip) return;
    int field_count = strcmp(type_name, "IoException") == 0 ? 2 : 1;
    Value ex = val_struct(type_name, field_count);
    ex.u.st->fields[0].name = cimple_strdup("message");
    ex.u.st->fields[0].type = TYPE_STRING;
    ex.u.st->fields[0].value = val_string(message ? message : "");
    if (field_count == 2) {
        ex.u.st->fields[1].name = cimple_strdup("path");
        ex.u.st->fields[1].type = TYPE_STRING;
        ex.u.st->fields[1].value = val_string(path ? path : "");
    }
    interp_throw(ip, type_name, ex);
}

static StructFieldVal *struct_field_lookup(StructVal *st, const char *name) {
    if (!st) return NULL;
    for (int i = 0; i < st->field_count; i++) {
        if (!st->fields[i].name) continue;
        if (strcmp(st->fields[i].name, name) == 0) return &st->fields[i];
    }
    return NULL;
}

static int builtin_exception_field_count(const char *name) {
    if (!name) return 0;
    if (strcmp(name, "IoException") == 0) return 2;
    if (strcmp(name, "Exception") == 0 ||
        strcmp(name, "RuntimeException") == 0 ||
        strcmp(name, "RegExpException") == 0) return 1;
    return 0;
}

static int builtin_exception_has_field(const char *type_name, const char *field_name) {
    if (!type_name || !field_name) return 0;
    if ((strcmp(type_name, "Exception") == 0 ||
         strcmp(type_name, "RuntimeException") == 0 ||
         strcmp(type_name, "RegExpException") == 0) &&
        strcmp(field_name, "message") == 0)
        return 1;
    if (strcmp(type_name, "IoException") == 0 &&
        (strcmp(field_name, "message") == 0 || strcmp(field_name, "path") == 0))
        return 1;
    return 0;
}

static void fill_builtin_exception_defaults(StructVal *out, const char *type_name) {
    if (!out || !type_name) return;
    int idx = 0;
    while (idx < out->field_count && out->fields[idx].name) idx++;
    if (strcmp(type_name, "Exception") == 0 ||
        strcmp(type_name, "RuntimeException") == 0 ||
        strcmp(type_name, "RegExpException") == 0) {
        out->fields[idx].name = cimple_strdup("message");
        out->fields[idx].type = TYPE_STRING;
        out->fields[idx].value = val_string("");
        return;
    }
    if (strcmp(type_name, "IoException") == 0) {
        out->fields[idx].name = cimple_strdup("message");
        out->fields[idx].type = TYPE_STRING;
        out->fields[idx].value = val_string("");
        out->fields[idx + 1].name = cimple_strdup("path");
        out->fields[idx + 1].type = TYPE_STRING;
        out->fields[idx + 1].value = val_string("");
    }
}

static int struct_decl_has_field(Interp *ip, AstNode *decl, const char *name) {
    if (!decl) return 0;
    for (int i = 0; i < decl->u.struct_decl.members.count; i++) {
        AstNode *m = decl->u.struct_decl.members.items[i];
        if (m->kind == NODE_STRUCT_FIELD &&
            strcmp(m->u.struct_field.name, name) == 0) return 1;
    }
    if (decl->u.struct_decl.base_name) {
        if (builtin_exception_has_field(decl->u.struct_decl.base_name, name))
            return 1;
        AstNode *base = find_struct_decl(ip, decl->u.struct_decl.base_name);
        return struct_decl_has_field(ip, base, name);
    }
    return 0;
}

static int struct_count_fields(Interp *ip, AstNode *decl) {
    int count = 0;
    if (decl->u.struct_decl.base_name) {
        AstNode *base = find_struct_decl(ip, decl->u.struct_decl.base_name);
        if (base) count += struct_count_fields(ip, base);
        else count += builtin_exception_field_count(decl->u.struct_decl.base_name);
    }
    for (int i = 0; i < decl->u.struct_decl.members.count; i++) {
        AstNode *m = decl->u.struct_decl.members.items[i];
        if (m->kind == NODE_STRUCT_FIELD) {
            if (decl->u.struct_decl.base_name) {
                /* Redefinition replaces inherited slot */
                AstNode *base = find_struct_decl(ip, decl->u.struct_decl.base_name);
                if (struct_decl_has_field(ip, base, m->u.struct_field.name)) continue;
            }
            count++;
        }
    }
    return count;
}

static void struct_fill_defaults(Interp *ip, StructVal *out, AstNode *decl, Scope *scope) {
    if (decl->u.struct_decl.base_name) {
        AstNode *base = find_struct_decl(ip, decl->u.struct_decl.base_name);
        if (base) struct_fill_defaults(ip, out, base, scope);
        else fill_builtin_exception_defaults(out, decl->u.struct_decl.base_name);
    }
    for (int i = 0; i < decl->u.struct_decl.members.count; i++) {
        AstNode *m = decl->u.struct_decl.members.items[i];
        if (m->kind != NODE_STRUCT_FIELD) continue;
        StructFieldVal *slot = struct_field_lookup(out, m->u.struct_field.name);
        Value init = m->u.struct_field.init
            ? eval_expr(ip, scope, m->u.struct_field.init)
            : value_default(m->u.struct_field.type);
        if (slot) {
            value_free(&slot->value);
            slot->type = m->u.struct_field.type;
            free(slot->struct_name);
            slot->struct_name = m->u.struct_field.struct_name
                ? cimple_strdup(m->u.struct_field.struct_name) : NULL;
            slot->value = init;
            continue;
        }
        int idx = 0;
        while (idx < out->field_count && out->fields[idx].name) idx++;
        out->fields[idx].name = cimple_strdup(m->u.struct_field.name);
        out->fields[idx].type = m->u.struct_field.type;
        out->fields[idx].struct_name = m->u.struct_field.struct_name
            ? cimple_strdup(m->u.struct_field.struct_name) : NULL;
        out->fields[idx].value = init;
    }
}

static int union_member_index(UnionVal *un, const char *name) {
    if (!un) return -1;
    for (int i = 0; i < un->member_count; i++) {
        if (un->member_names[i] && strcmp(un->member_names[i], name) == 0) return i;
    }
    return -1;
}

static Value create_union_instance(Interp *ip, const char *name) {
    AstNode *decl = find_union_decl(ip, name);
    if (!decl) return val_void();
    int count = decl->u.union_decl.members.count;
    Value out = val_union(name, count);
    for (int i = 0; i < count; i++) {
        AstNode *m = decl->u.union_decl.members.items[i];
        out.u.un->member_names[i] = cimple_strdup(m->u.struct_field.name);
        out.u.un->member_types[i] = m->u.struct_field.type;
        out.u.un->member_struct_names[i] = m->u.struct_field.struct_name
            ? cimple_strdup(m->u.struct_field.struct_name) : NULL;
        out.u.un->members[i] = value_default(m->u.struct_field.type);
    }
    return out;
}

static Value clone_struct_instance(Interp *ip, const char *name, Scope *scope, int line, int col) {
    AstNode *decl = find_struct_decl(ip, name);
    if (!decl) {
        error_runtime(line, col, "Unknown structure '%s'", name);
        return val_void();
    }
    Value out = val_struct(name, struct_count_fields(ip, decl));
    struct_fill_defaults(ip, out.u.st, decl, scope ? scope : ip->global);
    if (interp_has_throw(ip)) {
        value_free(&out);
        return val_void();
    }
    return out;
}

static Value *resolve_value_lvalue(Interp *ip, Scope *scope, AstNode *n, int line, int col) {
    if (n->kind == NODE_IDENT) {
        Symbol *sym = scope_lookup(scope, n->u.ident.name);
        if (!sym) {
            error_runtime(line, col, "undefined variable '%s'", n->u.ident.name);
            return NULL;
        }
        return &sym->val;
    }
    if (n->kind == NODE_SELF || n->kind == NODE_SUPER) {
        Symbol *sym = scope_lookup(scope, "self");
        if (!sym) {
            error_runtime(line, col, "'self' is not available here");
            return NULL;
        }
        return &sym->val;
    }
    if (n->kind == NODE_INDEX) {
        Value *base = resolve_value_lvalue(ip, scope, n->u.index.base, line, col);
        if (!base) return NULL;
        if (base->type == TYPE_MAP) {
            Value key = eval_expr(ip, scope, n->u.index.index);
            if (interp_has_throw(ip)) {
                value_free(&key);
                return NULL;
            }
            Value *slot = map_get_or_create(base->u.map, key);
            value_free(&key);
            return slot;
        }
        if (!type_is_array(base->type)) {
            error_runtime(line, col, "Operator '[]' cannot be applied to this value at runtime");
            return NULL;
        }
        Value idx = eval_expr(ip, scope, n->u.index.index);
        if (interp_has_throw(ip)) {
            value_free(&idx);
            return NULL;
        }
        int index = (int)idx.u.i;
        value_free(&idx);
        if (index < 0 || index >= base->u.arr->count) {
            error_runtime(line, col,
                          "Array index out of bounds (Index: %d   Array size: %d)",
                          index, base->u.arr->count);
            return NULL;
        }
        static Value slot;
        if (base->u.arr->elem_type == TYPE_STRUCT) {
            slot.type = TYPE_STRUCT;
            slot.u.st = base->u.arr->data.structs[index];
        } else if (base->u.arr->elem_type == TYPE_UNION) {
            slot.type = TYPE_UNION;
            slot.u.un = base->u.arr->data.unions[index];
        } else {
            error_runtime(line, col, "Indexed lvalue is not a structure or union");
            return NULL;
        }
        return &slot;
    }
    if (n->kind == NODE_MEMBER) {
        Value *base = resolve_value_lvalue(ip, scope, n->u.member.base, line, col);
        if (!base) return NULL;
        if (base->type == TYPE_STRUCT) {
            StructFieldVal *field = struct_field_lookup(base->u.st, n->u.member.name);
            if (!field) {
                error_runtime(line, col, "Unknown field '%s'", n->u.member.name);
                return NULL;
            }
            return &field->value;
        }
        if (base->type == TYPE_UNION) {
            if (strcmp(n->u.member.name, "kind") == 0) {
                error_runtime(line, col, "Cannot assign to union kind field");
                return NULL;
            }
            int idx = union_member_index(base->u.un, n->u.member.name);
            if (idx < 0) {
                error_runtime(line, col, "Unknown field '%s'", n->u.member.name);
                return NULL;
            }
            base->u.un->kind = idx;
            return &base->u.un->members[idx];
        }
        error_runtime(line, col, "Member access requires a structure instance");
        return NULL;
    }
    error_runtime(line, col, "Invalid lvalue");
    return NULL;
}

static Value *resolve_struct_lvalue(Interp *ip, Scope *scope, AstNode *n, int line, int col) {
    Value *out = resolve_value_lvalue(ip, scope, n, line, col);
    if (!out) return NULL;
    if (out->type != TYPE_STRUCT)
        error_runtime(line, col, "Member access requires a structure instance");
    if (interp_has_throw(ip)) return NULL;
    return out;
}

static AstNode *find_struct_method_decl(Interp *ip, const char *struct_name,
                                        const char *method_name, int use_base_only) {
    AstNode *decl = find_struct_decl(ip, struct_name);
    if (!decl) return NULL;
    if (use_base_only && decl->u.struct_decl.base_name) {
        decl = find_struct_decl(ip, decl->u.struct_decl.base_name);
    }
    for (; decl; decl = decl->u.struct_decl.base_name ? find_struct_decl(ip, decl->u.struct_decl.base_name) : NULL) {
        for (int i = 0; i < decl->u.struct_decl.members.count; i++) {
            AstNode *m = decl->u.struct_decl.members.items[i];
            if (m->kind == NODE_FUNC_DECL && strcmp(m->u.func_decl.name, method_name) == 0)
                return m;
        }
        if (use_base_only) break;
    }
    return NULL;
}

static const char *find_struct_method_owner(Interp *ip, const char *struct_name,
                                            const char *method_name, int use_base_only) {
    AstNode *decl = find_struct_decl(ip, struct_name);
    if (!decl) return NULL;
    if (use_base_only && decl->u.struct_decl.base_name)
        decl = find_struct_decl(ip, decl->u.struct_decl.base_name);
    for (; decl; decl = decl->u.struct_decl.base_name ? find_struct_decl(ip, decl->u.struct_decl.base_name) : NULL) {
        for (int i = 0; i < decl->u.struct_decl.members.count; i++) {
            AstNode *m = decl->u.struct_decl.members.items[i];
            if (m->kind == NODE_FUNC_DECL && strcmp(m->u.func_decl.name, method_name) == 0)
                return decl->u.struct_decl.name;
        }
        if (use_base_only) break;
    }
    return NULL;
}

/* -----------------------------------------------------------------------
 * eval_expr
 * ----------------------------------------------------------------------- */
static Value eval_expr(Interp *ip, Scope *scope, AstNode *n) {
    if (!n) return val_void();

    switch (n->kind) {
    case NODE_INT_LIT:    return val_int(n->u.int_lit.value);
    case NODE_FLOAT_LIT:  return val_float(n->u.float_lit.value);
    case NODE_BOOL_LIT:   return val_bool(n->u.bool_lit.value);
    case NODE_STRING_LIT: return val_string(n->u.string_lit.value);

    case NODE_CONST:
        return eval_constant(n->u.constant.name);

    case NODE_IDENT: {
        Symbol *sym = scope_lookup(scope, n->u.ident.name);
        if (!sym) {
            error_runtime(n->line, n->col,
                          "undefined variable '%s'", n->u.ident.name);
            return val_void();
        }
        return value_copy(sym->val);
    }

    case NODE_FUNC_REF:
        return val_func(n->u.func_ref.name);

    case NODE_SELF: {
        Symbol *sym = scope_lookup(scope, "self");
        if (!sym) {
            error_runtime(n->line, n->col, "'self' is not available here");
            return val_void();
        }
        return sym->val;
    }

    case NODE_SUPER: {
        Symbol *sym = scope_lookup(scope, "self");
        if (!sym) {
            error_runtime(n->line, n->col, "'super' is not available here");
            return val_void();
        }
        return sym->val;
    }

    case NODE_CLONE:
        return clone_struct_instance(ip, n->u.clone_expr.struct_name, scope, n->line, n->col);

    case NODE_ARRAY_LIT: {
        NodeList  *elems = &n->u.array_lit.elems;
        CimpleType elem_t = n->u.array_lit.elem_type;
        /* Determine element type from declaration context if unknown */
        if (elem_t == TYPE_UNKNOWN && elems->count > 0) {
            Value first = eval_expr(ip, scope, elems->items[0]);
            if (interp_has_throw(ip)) return val_void();
            elem_t = first.type;
            value_free(&first);
        }
        if (elem_t == TYPE_UNKNOWN) elem_t = TYPE_INT; /* empty array fallback */
        Value arr = val_array(elem_t);
        for (int i = 0; i < elems->count; i++) {
            Value v = eval_expr(ip, scope, elems->items[i]);
            if (interp_has_throw(ip)) {
                value_free(&v);
                value_free(&arr);
                return val_void();
            }
            array_push_owned(arr.u.arr, &v);
        }
        return arr;
    }

    case NODE_MAP_LIT: {
        MapVal *m = map_new(n->map_key_type, n->map_val_type, n->map_val_struct_name,
                            n->map_inner_key_type, n->map_val_type, n->map_val_struct_name);
        for (int i = 0; i < n->u.map_lit.keys.count; i++) {
            Value k = eval_expr(ip, scope, n->u.map_lit.keys.items[i]);
            Value v = eval_expr(ip, scope, n->u.map_lit.values.items[i]);
            if (interp_has_throw(ip)) {
                value_free(&k);
                value_free(&v);
                map_free(m);
                return val_void();
            }
            map_set(m, k, v);
            value_free(&k);
            value_free(&v);
        }
        return val_map(m);
    }

    case NODE_BINOP: {
        /* Short-circuit for &&, || */
        if (n->u.binop.op == OP_AND) {
            Value l = eval_expr(ip, scope, n->u.binop.left);
            if (interp_has_throw(ip)) return val_void();
            if (!l.u.b) return val_bool(0);
            Value r = eval_expr(ip, scope, n->u.binop.right);
            if (interp_has_throw(ip)) {
                value_free(&r);
                return val_void();
            }
            int res = r.u.b;
            value_free(&r);
            return val_bool(res);
        }
        if (n->u.binop.op == OP_OR) {
            Value l = eval_expr(ip, scope, n->u.binop.left);
            if (interp_has_throw(ip)) return val_void();
            if (l.u.b) return val_bool(1);
            Value r = eval_expr(ip, scope, n->u.binop.right);
            if (interp_has_throw(ip)) {
                value_free(&r);
                return val_void();
            }
            int res = r.u.b;
            value_free(&r);
            return val_bool(res);
        }

        Value l = eval_expr(ip, scope, n->u.binop.left);
        if (interp_has_throw(ip)) return val_void();
        Value r = eval_expr(ip, scope, n->u.binop.right);
        if (interp_has_throw(ip)) {
            value_free(&l);
            value_free(&r);
            return val_void();
        }
        Value result;

        switch (n->u.binop.op) {
        /* Arithmetic */
        case OP_ADD:
            if (l.type == TYPE_STRING && r.type == TYPE_STRING) {
                result = val_string_own(cimple_strconcat(l.u.s, r.u.s));
            } else if (is_int_like_value(l.type) && is_int_like_value(r.type)) {
                result = val_int(l.u.i + r.u.i);
            } else {
                result = val_float(l.u.f + r.u.f);
            }
            break;
        case OP_SUB:
            result = is_int_like_value(l.type) && is_int_like_value(r.type) ? val_int(l.u.i - r.u.i)
                                        : val_float(l.u.f - r.u.f);
            break;
        case OP_MUL:
            result = is_int_like_value(l.type) && is_int_like_value(r.type) ? val_int(l.u.i * r.u.i)
                                        : val_float(l.u.f * r.u.f);
            break;
        case OP_DIV:
            if (is_int_like_value(l.type) && is_int_like_value(r.type)) {
                if (r.u.i == 0)
                    error_runtime(n->line, n->col, "Integer division by zero");
                if (interp_has_throw(ip)) { value_free(&l); value_free(&r); return val_void(); }
                result = val_int(l.u.i / r.u.i);
            } else {
                result = val_float(l.u.f / r.u.f);
            }
            break;
        case OP_MOD:
            if (r.u.i == 0)
                error_runtime(n->line, n->col, "Integer modulo by zero");
            if (interp_has_throw(ip)) { value_free(&l); value_free(&r); return val_void(); }
            result = val_int(l.u.i % r.u.i);
            break;

        /* Comparison */
        case OP_EQ:
            if (is_int_like_value(l.type) && is_int_like_value(r.type)) result = val_bool(l.u.i == r.u.i);
            else if (l.type == TYPE_FLOAT) result = val_bool(l.u.f == r.u.f);
            else if (l.type == TYPE_BOOL)  result = val_bool(l.u.b == r.u.b);
            else result = val_bool(strcmp(l.u.s, r.u.s) == 0);
            break;
        case OP_NEQ:
            if (is_int_like_value(l.type) && is_int_like_value(r.type)) result = val_bool(l.u.i != r.u.i);
            else if (l.type == TYPE_FLOAT) result = val_bool(l.u.f != r.u.f);
            else if (l.type == TYPE_BOOL)  result = val_bool(l.u.b != r.u.b);
            else result = val_bool(strcmp(l.u.s, r.u.s) != 0);
            break;
        case OP_LT:
            result = is_int_like_value(l.type) && is_int_like_value(r.type) ? val_bool(l.u.i < r.u.i)
                                        : val_bool(l.u.f < r.u.f);
            break;
        case OP_LEQ:
            result = is_int_like_value(l.type) && is_int_like_value(r.type) ? val_bool(l.u.i <= r.u.i)
                                        : val_bool(l.u.f <= r.u.f);
            break;
        case OP_GT:
            result = is_int_like_value(l.type) && is_int_like_value(r.type) ? val_bool(l.u.i > r.u.i)
                                        : val_bool(l.u.f > r.u.f);
            break;
        case OP_GEQ:
            result = is_int_like_value(l.type) && is_int_like_value(r.type) ? val_bool(l.u.i >= r.u.i)
                                        : val_bool(l.u.f >= r.u.f);
            break;

        /* Bitwise */
        case OP_BAND:   result = (l.type == TYPE_BYTE && r.type == TYPE_BYTE) ? val_byte((unsigned char)(l.u.i & r.u.i)) : val_int(l.u.i & r.u.i);  break;
        case OP_BOR:    result = (l.type == TYPE_BYTE && r.type == TYPE_BYTE) ? val_byte((unsigned char)(l.u.i | r.u.i)) : val_int(l.u.i | r.u.i);  break;
        case OP_BXOR:   result = (l.type == TYPE_BYTE && r.type == TYPE_BYTE) ? val_byte((unsigned char)(l.u.i ^ r.u.i)) : val_int(l.u.i ^ r.u.i);  break;
        case OP_LSHIFT: result = val_int(l.u.i << (int)r.u.i); break;
        case OP_RSHIFT: result = val_int(l.u.i >> (int)r.u.i); break;

        default: result = val_void(); break;
        }
        value_free(&l);
        value_free(&r);
        return result;
    }

    case NODE_TERNARY: {
        Value cond = eval_expr(ip, scope, n->u.ternary.cond);
        if (interp_has_throw(ip)) return val_void();
        int c = cond.u.b;
        value_free(&cond);
        return eval_expr(ip, scope, c ? n->u.ternary.then_expr : n->u.ternary.else_expr);
    }

    case NODE_UNOP: {
        Value v = eval_expr(ip, scope, n->u.unop.operand);
        if (interp_has_throw(ip)) return val_void();
        Value result;
        switch (n->u.unop.op) {
        case OP_NOT:  result = val_bool(!v.u.b); break;
        case OP_NEG:
            result = is_int_like_value(v.type) ? val_int(-v.u.i) : val_float(-v.u.f);
            break;
        case OP_BNOT: result = (v.type == TYPE_BYTE) ? val_byte((unsigned char)(~v.u.i)) : val_int(~v.u.i); break;
        default:      result = val_void(); break;
        }
        value_free(&v);
        return result;
    }

    case NODE_INDEX: {
        Value base  = eval_expr(ip, scope, n->u.index.base);
        if (interp_has_throw(ip)) return val_void();
        Value index = eval_expr(ip, scope, n->u.index.index);
        if (interp_has_throw(ip)) { value_free(&base); value_free(&index); return val_void(); }
        int   idx   = (int)index.u.i;
        Value result;

        if (base.type == TYPE_STRING) {
            int blen = (int)strlen(base.u.s);
            if (idx < 0 || idx >= blen)
                error_runtime(n->line, n->col,
                              "String index out of bounds (Index: %d   String length: %d bytes)",
                              idx, blen);
            if (interp_has_throw(ip)) { value_free(&base); return val_void(); }
            char buf[2] = { base.u.s[idx], '\0' };
            result = val_string(buf);
        } else if (type_is_array(base.type)) {
            result = array_get(base.u.arr, idx, n->line, n->col);
        } else if (base.type == TYPE_MAP) {
            result = map_get(base.u.map, index);
        } else {
            error_runtime(n->line, n->col,
                          "Operator '[]' cannot be applied to this value at runtime");
            result = val_void();
        }
        value_free(&base);
        value_free(&index);
        return result;
    }

    case NODE_MEMBER: {
        if (n->u.member.base->kind == NODE_IDENT) {
            AstNode *union_decl = find_union_decl(ip, n->u.member.base->u.ident.name);
            if (union_decl) {
                char upper_name[128];
                size_t k = 0;
                for (const char *p = n->u.member.name; *p && k + 1 < sizeof(upper_name); p++) {
                    char c = *p;
                    upper_name[k++] = (char)((c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c);
                }
                upper_name[k] = '\0';
                for (int i = 0; i < union_decl->u.union_decl.members.count; i++) {
                    AstNode *m = union_decl->u.union_decl.members.items[i];
                    if (strcmp(m->u.struct_field.name, n->u.member.name) == 0 ||
                        strcmp(m->u.struct_field.name, upper_name) == 0)
                        return val_int(i);
                }
            }
        }
        Value base = eval_expr(ip, scope, n->u.member.base);
        if (interp_has_throw(ip)) return val_void();
        Value out;
        if (base.type == TYPE_STRUCT) {
            StructFieldVal *field = struct_field_lookup(base.u.st, n->u.member.name);
            if (!field) {
                error_runtime(n->line, n->col, "Unknown field '%s'", n->u.member.name);
                value_free(&base);
                return val_void();
            }
            out = value_copy(field->value);
        } else if (base.type == TYPE_UNION) {
            if (strcmp(n->u.member.name, "kind") == 0) {
                out = val_int(base.u.un->kind);
            } else {
                int idx = union_member_index(base.u.un, n->u.member.name);
                if (idx < 0) {
                    error_runtime(n->line, n->col, "Unknown field '%s'", n->u.member.name);
                    value_free(&base);
                    return val_void();
                }
                if (base.u.un->kind != idx)
                    error_runtime(n->line, n->col,
                                  "Union member '%s' is not active", n->u.member.name);
                if (interp_has_throw(ip)) { value_free(&base); return val_void(); }
                out = value_copy(base.u.un->members[idx]);
            }
        } else {
            error_runtime(n->line, n->col, "Member access requires a structure instance");
            value_free(&base);
            return val_void();
        }
        if (n->u.member.base->kind != NODE_SELF && n->u.member.base->kind != NODE_SUPER)
            value_free(&base);
        return out;
    }

    case NODE_METHOD_CALL: {
        Value arg_vals[32];
        int nargs = n->u.method_call.args.count;
        if (nargs > 32) nargs = 32;
        for (int i = 0; i < nargs; i++)
            arg_vals[i] = eval_expr(ip, scope, n->u.method_call.args.items[i]);
        if (interp_has_throw(ip)) {
            for (int i = 0; i < nargs; i++) value_free(&arg_vals[i]);
            return val_void();
        }
        Value *base = resolve_struct_lvalue(ip, scope, n->u.method_call.base, n->line, n->col);
        if (!base) {
            for (int i = 0; i < nargs; i++) value_free(&arg_vals[i]);
            return val_void();
        }
        Value out = call_method(ip, base, n->u.method_call.name,
                                n->u.method_call.is_super,
                                arg_vals, nargs, n->line, n->col);
        for (int i = 0; i < nargs; i++) value_free(&arg_vals[i]);
        return out;
    }

    case NODE_CALL: {
        const char *fname = n->u.call.name;
        NodeList   *args  = &n->u.call.args;
        Value       arg_vals[32];
        int         borrowed[32] = {0};
        AstNode    *user_func = find_func_decl(ip, fname);
        Symbol     *call_sym = scope_lookup(scope, fname);
        const char *dispatch_name = fname;
        int nargs = args->count;
        const FuncType *callback_sig = NULL;
        if (call_sym && call_sym->type == TYPE_FUNC) {
            if (!call_sym->val.u.s || !call_sym->val.u.s[0])
                error_runtime(n->line, n->col, "Function variable '%s' is not initialized", fname);
            dispatch_name = call_sym->val.u.s;
            user_func = find_func_decl(ip, dispatch_name);
            callback_sig = call_sym->func_type;
        }
        if (nargs > 32) nargs = 32;
        for (int i = 0; i < nargs; i++) {
            int borrow_array_arg =
                args->items[i]->kind == NODE_IDENT &&
                ((i == 0 && is_mutating_array_builtin(dispatch_name)) ||
                 (i == 0 && is_map_builtin(dispatch_name)) ||
                 (callback_sig &&
                  i < callback_sig->param_count &&
                  (type_is_array(callback_sig->params[i]) ||
                   callback_sig->params[i] == TYPE_MAP ||
                   callback_sig->params[i] == TYPE_STRUCT)) ||
                 (user_func &&
                  i < user_func->u.func_decl.params.count &&
                  (type_is_array(user_func->u.func_decl.params.items[i]->u.param.type) ||
                   user_func->u.func_decl.params.items[i]->u.param.type == TYPE_MAP ||
                   user_func->u.func_decl.params.items[i]->u.param.type == TYPE_STRUCT)));

            if (borrow_array_arg) {
                Symbol *sym = scope_lookup(scope, args->items[i]->u.ident.name);
                if (!sym) {
                    error_runtime(n->line, n->col,
                                  "undefined variable '%s'", args->items[i]->u.ident.name);
                    for (int j = 0; j < i; j++) if (!borrowed[j]) value_free(&arg_vals[j]);
                    return val_void();
                }
                arg_vals[i] = sym->val;
                borrowed[i] = 1;
            } else {
                arg_vals[i] = eval_expr(ip, scope, args->items[i]);
                if (interp_has_throw(ip)) {
                    for (int j = 0; j <= i; j++) if (!borrowed[j]) value_free(&arg_vals[j]);
                    return val_void();
                }
            }
        }
        Value result = call_func(ip, dispatch_name, arg_vals, nargs, n->line, n->col);
        for (int i = 0; i < nargs; i++) {
            if (!borrowed[i]) value_free(&arg_vals[i]);
        }
        return result;
    }

    default:
        return val_void();
    }
}

/* -----------------------------------------------------------------------
 * exec_stmt
 * ----------------------------------------------------------------------- */
static void exec_stmt(Interp *ip, Scope *scope, AstNode *n) {
    if (!n || ip->signal != SIGNAL_NONE) return;

    switch (n->kind) {
    case NODE_VAR_DECL: {
        const char *name = n->u.var_decl.name;
        CimpleType   t   = n->u.var_decl.type;

        Value v;
        if (n->u.var_decl.init) {
            v = eval_expr(ip, scope, n->u.var_decl.init);
            if (interp_has_throw(ip)) {
                value_free(&v);
                break;
            }
            /* Propagate element type for empty arrays */
            if (n->u.var_decl.init->kind == NODE_ARRAY_LIT &&
                n->u.var_decl.init->u.array_lit.elems.count == 0) {
                if (t == TYPE_MAP) {
                    value_free(&v);
                    v = val_map(map_new(n->map_key_type, n->map_val_type, n->map_val_struct_name,
                                        n->map_inner_key_type, n->map_val_type, n->map_val_struct_name));
                } else if (v.u.arr) {
                    v.u.arr->elem_type = type_elem(t);
                    v.type = t;
                }
            }
        } else {
            v = (t == TYPE_UNION && n->u.var_decl.struct_name)
                ? create_union_instance(ip, n->u.var_decl.struct_name)
                : (t == TYPE_MAP)
                    ? val_map(map_new(n->map_key_type, n->map_val_type, n->map_val_struct_name,
                                      n->map_inner_key_type, n->map_val_type, n->map_val_struct_name))
                : value_default(t);
        }

        Symbol *sym = scope_define(scope, name, t,
                                   n->u.var_decl.struct_name,
                                   n->u.var_decl.func_type,
                                   n->map_key_type, n->map_inner_key_type,
                                   n->map_val_type, n->map_val_struct_name,
                                   n->line, n->col);
        if (!sym) {
            /* Re-definition at runtime (shouldn't happen if semantic check passed) */
            sym = scope_lookup_local(scope, name);
        }
        if (sym) {
            value_free(&sym->val);
            if (t == TYPE_BYTE && v.type == TYPE_INT) v.type = TYPE_BYTE;
            sym->val = v;
        } else {
            value_free(&v);
        }
        break;
    }

    case NODE_ASSIGN: {
        Symbol *sym = scope_lookup(scope, n->u.assign.name);
        if (!sym) {
            error_runtime(n->line, n->col,
                          "undefined variable '%s'", n->u.assign.name);
            break;
        }
        Value v = eval_expr(ip, scope, n->u.assign.value);
        if (interp_has_throw(ip)) {
            value_free(&v);
            break;
        }
        if (sym->type == TYPE_BYTE && v.type == TYPE_INT) v.type = TYPE_BYTE;
        value_free(&sym->val);
        sym->val = v;
        break;
    }

    case NODE_INDEX_ASSIGN: {
        Symbol *sym = scope_lookup(scope, n->u.index_assign.name);
        if (!sym) {
            error_runtime(n->line, n->col,
                          "undefined variable '%s'", n->u.index_assign.name);
            break;
        }
        Value idx_v = eval_expr(ip, scope, n->u.index_assign.index);
        Value val   = eval_expr(ip, scope, n->u.index_assign.value);
        if (interp_has_throw(ip)) {
            value_free(&idx_v);
            value_free(&val);
            break;
        }
        if (sym->type == TYPE_MAP) {
            Value *slot = map_get_or_create(sym->val.u.map, idx_v);
            value_free(slot);
            *slot = value_copy(val);
        } else {
            int idx = (int)idx_v.u.i;
            array_set_owned(sym->val.u.arr, idx, &val, n->line, n->col);
        }
        value_free(&idx_v);
        value_free(&val);
        break;
    }

    case NODE_INDEX2_ASSIGN: {
        Symbol *sym = scope_lookup(scope, n->u.index2_assign.name);
        if (!sym) {
            error_runtime(n->line, n->col,
                          "undefined variable '%s'", n->u.index2_assign.name);
            break;
        }
        Value i1v = eval_expr(ip, scope, n->u.index2_assign.index1);
        Value i2v = eval_expr(ip, scope, n->u.index2_assign.index2);
        Value val = eval_expr(ip, scope, n->u.index2_assign.value);
        if (interp_has_throw(ip)) {
            value_free(&i1v);
            value_free(&i2v);
            value_free(&val);
            break;
        }
        if (sym->type == TYPE_MAP) {
            Value *outer_slot = map_get_or_create(sym->val.u.map, i1v);
            if (outer_slot->type != TYPE_MAP) {
                value_free(outer_slot);
                *outer_slot = val_map(map_new(sym->map_inner_key_type, sym->map_val_type,
                                              sym->map_val_struct_name,
                                              TYPE_UNKNOWN, TYPE_UNKNOWN, NULL));
            }
            Value *inner_slot = map_get_or_create(outer_slot->u.map, i2v);
            value_free(inner_slot);
            *inner_slot = value_copy(val);
        } else {
            int i1 = (int)i1v.u.i;
            int i2 = (int)i2v.u.i;
            if (i1 < 0 || i1 >= sym->val.u.arr->count)
                error_runtime(n->line, n->col, "row index %d out of bounds (size %d)",
                              i1, sym->val.u.arr->count);
            if (interp_has_throw(ip)) {
                value_free(&i1v);
                value_free(&i2v);
                value_free(&val);
                break;
            }
            ArrayVal *inner = sym->val.u.arr->data.arrays[i1];
            array_set_owned(inner, i2, &val, n->line, n->col);
        }
        value_free(&i1v);
        value_free(&i2v);
        value_free(&val);
        break;
    }

    case NODE_MEMBER_ASSIGN: {
        AstNode *target = n->u.member_assign.target;
        if (target->kind != NODE_MEMBER)
            error_runtime(n->line, n->col, "Invalid member assignment");
        if (interp_has_throw(ip)) break;
        Value *field_value = resolve_value_lvalue(ip, scope, target, n->line, n->col);
        if (!field_value) break;
        Value v = eval_expr(ip, scope, n->u.member_assign.value);
        if (interp_has_throw(ip)) {
            value_free(&v);
            break;
        }
        value_free(field_value);
        *field_value = v;
        break;
    }

    case NODE_BLOCK: {
        Scope *inner = scope_new(scope, 0);
        NodeList *stmts = &n->u.block.stmts;
        for (int i = 0; i < stmts->count && ip->signal == SIGNAL_NONE; i++)
            exec_stmt(ip, inner, stmts->items[i]);
        scope_free(inner);
        break;
    }

    case NODE_IF: {
        Value cond = eval_expr(ip, scope, n->u.if_stmt.cond);
        if (interp_has_throw(ip)) {
            value_free(&cond);
            break;
        }
        if (cond.u.b) {
            exec_stmt(ip, scope, n->u.if_stmt.then_branch);
        } else if (n->u.if_stmt.else_branch) {
            exec_stmt(ip, scope, n->u.if_stmt.else_branch);
        }
        break;
    }

    case NODE_WHILE: {
        for (;;) {
            Value cond = eval_expr(ip, scope, n->u.while_stmt.cond);
            if (interp_has_throw(ip)) {
                value_free(&cond);
                break;
            }
            if (!cond.u.b) break;
            exec_stmt(ip, scope, n->u.while_stmt.body);
            if (ip->signal == SIGNAL_BREAK) { ip->signal = SIGNAL_NONE; break; }
            if (ip->signal == SIGNAL_CONTINUE) { ip->signal = SIGNAL_NONE; continue; }
            if (ip->signal == SIGNAL_RETURN || ip->signal == SIGNAL_THROW) break;
        }
        break;
    }

    case NODE_SWITCH: {
        Value cond = eval_expr(ip, scope, n->u.switch_stmt.expr);
        int matched = 0;
        AstNode *default_case = NULL;
        for (int i = 0; i < n->u.switch_stmt.cases.count; i++) {
            AstNode *c = n->u.switch_stmt.cases.items[i];
            if (c->kind == NODE_DEFAULT_CASE) {
                default_case = c;
                continue;
            }
            Value case_val = eval_expr(ip, scope, c->u.case_stmt.value);
            if (interp_has_throw(ip)) {
                value_free(&case_val);
                break;
            }
            int same = (cond.type == TYPE_INT || cond.type == TYPE_BYTE)
                ? (cond.u.i == case_val.u.i)
                : 0;
            value_free(&case_val);
            if (!same) continue;
            matched = 1;
            Scope *case_scope = scope_new(scope, 0);
            for (int j = 0; j < c->u.case_stmt.stmts.count && ip->signal == SIGNAL_NONE; j++)
                exec_stmt(ip, case_scope, c->u.case_stmt.stmts.items[j]);
            scope_free(case_scope);
            break;
        }
        if (!matched && default_case) {
            Scope *case_scope = scope_new(scope, 0);
            for (int j = 0; j < default_case->u.case_stmt.stmts.count && ip->signal == SIGNAL_NONE; j++)
                exec_stmt(ip, case_scope, default_case->u.case_stmt.stmts.items[j]);
            scope_free(case_scope);
        }
        if (ip->signal == SIGNAL_BREAK) ip->signal = SIGNAL_NONE;
        value_free(&cond);
        break;
    }

    case NODE_FOR: {
        Scope *for_scope = scope_new(scope, 0);

        /* Init */
        if (n->u.for_stmt.init) {
            AstNode *init = n->u.for_stmt.init;
            if (init->kind == NODE_FOR_INIT) {
                Value init_v = eval_expr(ip, for_scope, init->u.for_init.init_expr);
                if (interp_has_throw(ip)) {
                    value_free(&init_v);
                    scope_free(for_scope);
                    break;
                }
                Symbol *sym  = scope_define(for_scope, init->u.for_init.name,
                                            TYPE_INT, NULL, NULL,
                                            TYPE_UNKNOWN, TYPE_UNKNOWN, TYPE_UNKNOWN, NULL,
                                            init->line, init->col);
                if (sym) { value_free(&sym->val); sym->val = init_v; }
                else value_free(&init_v);
            } else {
                exec_stmt(ip, for_scope, init);
            }
        }

        for (;;) {
            if (n->u.for_stmt.cond) {
                Value cond = eval_expr(ip, for_scope, n->u.for_stmt.cond);
                if (interp_has_throw(ip)) {
                    value_free(&cond);
                    break;
                }
                int ok = cond.u.b;
                value_free(&cond);
                if (!ok) break;
            }
            exec_stmt(ip, for_scope, n->u.for_stmt.body);
            if (ip->signal == SIGNAL_BREAK)    { ip->signal = SIGNAL_NONE; break; }
            if (ip->signal == SIGNAL_CONTINUE) { ip->signal = SIGNAL_NONE; }
            if (ip->signal == SIGNAL_RETURN || ip->signal == SIGNAL_THROW)   break;

            /* Update */
            if (n->u.for_stmt.update)
                exec_stmt(ip, for_scope, n->u.for_stmt.update);
        }
        scope_free(for_scope);
        break;
    }

    case NODE_RETURN:
        if (n->u.ret.value) {
            value_free(&ip->ret_val);
            ip->ret_val = eval_expr(ip, scope, n->u.ret.value);
            if (interp_has_throw(ip)) {
                value_free(&ip->ret_val);
                ip->ret_val = val_void();
                break;
            }
        } else {
            ip->ret_val = val_void();
        }
        ip->signal = SIGNAL_RETURN;
        break;

    case NODE_THROW: {
        Value ex = eval_expr(ip, scope, n->u.throw_stmt.expr);
        if (interp_has_throw(ip)) {
            value_free(&ex);
            break;
        }
        const char *type_name = "Exception";
        if (ex.type == TYPE_STRUCT && ex.u.st && ex.u.st->struct_name)
            type_name = ex.u.st->struct_name;
        else if (n->u.throw_stmt.expr->type_name_hint)
            type_name = n->u.throw_stmt.expr->type_name_hint;
        interp_throw(ip, type_name, ex);
        ip->throw_line = n->line;
        ip->throw_col = n->col;
        break;
    }

    case NODE_TRY_CATCH: {
        exec_stmt(ip, scope, n->u.try_catch.try_block);
        if (ip->signal != SIGNAL_THROW)
            break;

        Value thrown = ip->throw_val;
        char *thrown_type = ip->throw_type_name;
        ip->throw_val = val_void();
        ip->throw_type_name = NULL;
        ip->signal = SIGNAL_NONE;

        int handled = 0;
        for (int i = 0; i < n->u.try_catch.clauses.count; i++) {
            AstNode *cl = n->u.try_catch.clauses.items[i];
            if (!interp_struct_is_subtype(ip, thrown_type, cl->u.catch_clause.type_name))
                continue;
            Scope *catch_scope = scope_new(scope, 0);
            Symbol *sym = scope_define(catch_scope, cl->u.catch_clause.var_name,
                                       TYPE_STRUCT, thrown_type, NULL,
                                       TYPE_UNKNOWN, TYPE_UNKNOWN, TYPE_UNKNOWN, NULL,
                                       cl->line, cl->col);
            if (sym) {
                value_free(&sym->val);
                sym->val = thrown;
            } else {
                value_free(&thrown);
            }
            exec_stmt(ip, catch_scope, cl->u.catch_clause.block);
            /* scope_free calls value_free on each symbol — do NOT clear sym->val
               beforehand or the thrown value leaks without being freed. */
            scope_free(catch_scope);
            handled = 1;
            break;
        }
        if (!handled) {
            ip->signal = SIGNAL_THROW;
            ip->throw_val = thrown;
            ip->throw_type_name = thrown_type;
        } else {
            free(thrown_type);
        }
        break;
    }

    case NODE_BREAK:
        ip->signal = SIGNAL_BREAK;
        break;

    case NODE_CONTINUE:
        ip->signal = SIGNAL_CONTINUE;
        break;

    case NODE_INCR: {
        Symbol *sym = scope_lookup(scope, n->u.incr_decr.name);
        if (!sym) {
            error_runtime(n->line, n->col,
                          "undefined variable '%s'", n->u.incr_decr.name);
            break;
        }
        sym->val.u.i++;
        break;
    }

    case NODE_DECR: {
        Symbol *sym = scope_lookup(scope, n->u.incr_decr.name);
        if (!sym) {
            error_runtime(n->line, n->col,
                          "undefined variable '%s'", n->u.incr_decr.name);
            break;
        }
        sym->val.u.i--;
        break;
    }

    case NODE_COMPOUND_ASSIGN: {
        Symbol *sym = scope_lookup(scope, n->u.compound_assign.name);
        if (!sym) {
            error_runtime(n->line, n->col,
                          "undefined variable '%s'",
                          n->u.compound_assign.name);
            break;
        }
        Value rhs = eval_expr(ip, scope, n->u.compound_assign.value);
        if (interp_has_throw(ip)) {
            value_free(&rhs);
            break;
        }
        int is_int = (sym->val.type == TYPE_INT);
        switch (n->u.compound_assign.op) {
        case OP_ADD:
            if (is_int) sym->val.u.i += rhs.u.i;
            else        sym->val.u.f += rhs.u.f;
            break;
        case OP_SUB:
            if (is_int) sym->val.u.i -= rhs.u.i;
            else        sym->val.u.f -= rhs.u.f;
            break;
        case OP_MUL:
            if (is_int) sym->val.u.i *= rhs.u.i;
            else        sym->val.u.f *= rhs.u.f;
            break;
        case OP_DIV:
            if (is_int) {
                if (rhs.u.i == 0)
                    error_runtime(n->line, n->col, "Division by zero");
                if (interp_has_throw(ip)) break;
                sym->val.u.i /= rhs.u.i;
            } else sym->val.u.f /= rhs.u.f;
            break;
        case OP_MOD:
            if (is_int) {
                if (rhs.u.i == 0)
                    error_runtime(n->line, n->col, "Modulo by zero");
                if (interp_has_throw(ip)) break;
                sym->val.u.i %= rhs.u.i;
            } else sym->val.u.f = fmod(sym->val.u.f, rhs.u.f);
            break;
        default: break;
        }
        value_free(&rhs);
        break;
    }

    case NODE_EXPR_STMT: {
        AstNode *expr = n->u.expr_stmt.expr;
        /* void union member activation: `x.none;` sets the active member */
        if (expr->kind == NODE_MEMBER && expr->type == TYPE_VOID) {
            (void)resolve_value_lvalue(ip, scope, expr, n->line, n->col);
            break;
        }
        Value v = eval_expr(ip, scope, expr);
        if (interp_has_throw(ip)) {
            value_free(&v);
            break;
        }
        value_free(&v);
        break;
    }

    case NODE_FOR_INIT:
        /* handled in NODE_FOR */
        break;

    default:
        break;
    }
}

/* -----------------------------------------------------------------------
 * User function call
 * ----------------------------------------------------------------------- */
static AstNode *find_func_decl(Interp *ip, const char *name) {
    if (!ip->func_decls) return NULL;
    uint64_t hash = hash_name(name);
    int bucket = (int)(hash & (unsigned long)(ip->func_decls->bucket_count - 1));
    for (FuncDeclEntry *entry = ip->func_decls->buckets[bucket]; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) return entry->decl;
    }
    return NULL;
}

static Value call_user_func(Interp *ip, AstNode *f, Value *args, int nargs,
                             int call_line, int call_col) {
    (void)call_line; (void)call_col;

    /* Create function scope (child of global) */
    Scope *fn_scope = scope_new(ip->global, 1);
    Symbol *bound_params[32] = {0};
    int borrowed_params[32] = {0};

    /* Bind parameters */
    NodeList *params = &f->u.func_decl.params;
    for (int i = 0; i < params->count && i < nargs; i++) {
        AstNode *p   = params->items[i];
        Symbol  *sym = scope_define(fn_scope, p->u.param.name,
                                    p->u.param.type, p->u.param.struct_name,
                                    p->u.param.func_type,
                                    p->map_key_type, p->map_inner_key_type,
                                    p->map_val_type, p->map_val_struct_name,
                                    p->line, p->col);
        if (sym) {
            if ((type_is_array(p->u.param.type) && type_is_array(args[i].type)) ||
                (p->u.param.type == TYPE_STRUCT && args[i].type == TYPE_STRUCT) ||
                (p->u.param.type == TYPE_MAP && args[i].type == TYPE_MAP)) {
                value_free(&sym->val);
                sym->val = args[i];
                borrowed_params[i] = 1;
            } else {
                sym->val = value_copy(args[i]);
            }
            bound_params[i] = sym;
        }
    }

    /* Save signal state */
    Signal  saved_sig = ip->signal;
    Value   saved_ret = ip->ret_val;
    Value   saved_throw = ip->throw_val;
    char   *saved_throw_type = ip->throw_type_name;
    ip->signal  = SIGNAL_NONE;
    ip->ret_val = val_void();
    ip->throw_val = val_void();
    ip->throw_type_name = NULL;

    /* Execute body */
    AstNode *body = f->u.func_decl.body;
    NodeList *stmts = &body->u.block.stmts;
    for (int i = 0; i < stmts->count && ip->signal == SIGNAL_NONE; i++)
        exec_stmt(ip, fn_scope, stmts->items[i]);

    Value ret = ip->ret_val;
    Signal result_sig = ip->signal;
    Value thrown = ip->throw_val;
    char *thrown_type = ip->throw_type_name;
    ip->signal  = saved_sig;
    ip->ret_val = saved_ret;
    ip->throw_val = saved_throw;
    ip->throw_type_name = saved_throw_type;

    for (int i = 0; i < params->count && i < nargs; i++) {
        if (borrowed_params[i] && bound_params[i]) {
            bound_params[i]->val = val_void();
        }
    }

    scope_free(fn_scope);
    if (result_sig == SIGNAL_THROW) {
        value_free(&ret);
        interp_throw(ip, thrown_type ? thrown_type : "Exception", thrown);
        free(thrown_type);
        return val_void();
    }
    if (f->u.func_decl.ret_type == TYPE_STRUCT) {
        Value copy = value_copy(ret);
        value_free(&ret);
        return copy;
    }
    return ret;
}

static Value call_method(Interp *ip, Value *base, const char *method_name,
                         int use_base_only, Value *args, int nargs,
                         int line, int col) {
    if (base->type != TYPE_STRUCT || !base->u.st)
        error_runtime(line, col, "Member access requires a structure instance");
    const char *search_struct = base->u.st->struct_name;
    if (use_base_only && ip->current_method_base)
        search_struct = ip->current_method_base;
    AstNode *method = find_struct_method_decl(ip, search_struct, method_name, 0);
    const char *owner = find_struct_method_owner(ip, search_struct, method_name, 0);
    if (!method)
        error_runtime(line, col, "Unknown method '%s'", method_name);

    Scope *fn_scope = scope_new(ip->global, 1);
    Symbol *self_sym = scope_define(fn_scope, "self", TYPE_STRUCT, base->u.st->struct_name, NULL,
                                    TYPE_UNKNOWN, TYPE_UNKNOWN, TYPE_UNKNOWN, NULL,
                                    line, col);
    value_free(&self_sym->val);
    self_sym->val = *base;

    for (int i = 0; i < method->u.func_decl.params.count && i < nargs; i++) {
        AstNode *p = method->u.func_decl.params.items[i];
        Symbol *sym = scope_define(fn_scope, p->u.param.name,
                                   p->u.param.type, p->u.param.struct_name,
                                   p->u.param.func_type,
                                   p->map_key_type, p->map_inner_key_type,
                                   p->map_val_type, p->map_val_struct_name,
                                   p->line, p->col);
        if (type_is_array(p->u.param.type) || p->u.param.type == TYPE_STRUCT || p->u.param.type == TYPE_MAP)
            sym->val = args[i];
        else
            sym->val = value_copy(args[i]);
    }

    Signal saved_sig = ip->signal;
    Value saved_ret = ip->ret_val;
    Value saved_throw = ip->throw_val;
    char *saved_throw_type = ip->throw_type_name;
    const char *saved_method_struct = ip->current_method_struct;
    const char *saved_method_base = ip->current_method_base;
    ip->signal = SIGNAL_NONE;
    ip->ret_val = val_void();
    ip->throw_val = val_void();
    ip->throw_type_name = NULL;
    ip->current_method_struct = owner;
    if (owner) {
        AstNode *owner_decl = find_struct_decl(ip, owner);
        ip->current_method_base = owner_decl
            ? owner_decl->u.struct_decl.base_name
            : NULL;
    } else {
        ip->current_method_base = NULL;
    }

    NodeList *stmts = &method->u.func_decl.body->u.block.stmts;
    for (int i = 0; i < stmts->count && ip->signal == SIGNAL_NONE; i++)
        exec_stmt(ip, fn_scope, stmts->items[i]);

    Value ret = ip->ret_val;
    Signal result_sig = ip->signal;
    Value thrown = ip->throw_val;
    char *thrown_type = ip->throw_type_name;
    ip->signal = saved_sig;
    ip->ret_val = saved_ret;
    ip->throw_val = saved_throw;
    ip->throw_type_name = saved_throw_type;
    ip->current_method_struct = saved_method_struct;
    ip->current_method_base = saved_method_base;
    self_sym->val = val_void();
    scope_free(fn_scope);
    if (result_sig == SIGNAL_THROW) {
        value_free(&ret);
        interp_throw(ip, thrown_type ? thrown_type : "Exception", thrown);
        free(thrown_type);
        return val_void();
    }
    if (method->u.func_decl.ret_type == TYPE_STRUCT) {
        Value copy = value_copy(ret);
        value_free(&ret);
        return copy;
    }
    return ret;
}

/* -----------------------------------------------------------------------
 * call_func — dispatch to builtin or user function
 * ----------------------------------------------------------------------- */
static Value call_func(Interp *ip, const char *name, Value *args, int nargs,
                        int line, int col) {
    /* Try builtin first */
    Value result = builtin_call(name, args, nargs, line, col);
    if (result.type != TYPE_UNKNOWN)
        return result;

    /* Try user function */
    AstNode *f = find_func_decl(ip, name);
    if (f)
        return call_user_func(ip, f, args, nargs, line, col);

    error_runtime(line, col, "Unknown function: '%s'", name);
    return val_void();
}

/* -----------------------------------------------------------------------
 * interp_run — main entry point
 * ----------------------------------------------------------------------- */
int interp_run(AstNode *program, int argc, char **argv) {
    srand((unsigned int)time(NULL));

    Interp ip;
    ip.program = program;
    ip.global  = scope_new(NULL, 0);
    ip.funcs   = NULL;
    ip.func_decls = func_decl_table_build(program);
    ip.struct_decls = struct_decl_table_build(program);
    ip.union_decls = union_decl_table_build(program);
    ip.signal  = SIGNAL_NONE;
    ip.ret_val = val_void();
    ip.throw_val = val_void();
    ip.throw_type_name = NULL;
    ip.throw_line = 0;
    ip.throw_col = 0;
    ip.current_method_struct = NULL;
    ip.current_method_base = NULL;
    ip.runtime_error_type = NULL;
    ip.runtime_error_path = NULL;
    g_current_interp = &ip;

    /* Initialise global variables */
    NodeList *decls = &program->u.program.decls;
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d->kind == NODE_VAR_DECL)
            exec_stmt(&ip, ip.global, d);
    }

    /* Find main */
    AstNode *main_fn = find_func_decl(&ip, "main");
    if (!main_fn) {
        fprintf(stderr, "[RUNTIME ERROR] no 'main' function\n");
        return 1;
    }

    /* Build args */
    Value args[2];
    int nargs = main_fn->u.func_decl.params.count;
    if (nargs >= 1) {
        args[0] = val_array(TYPE_STRING);
        for (int i = 0; i < argc; i++) {
            Value s = val_string(argv[i]);
            array_push_owned(args[0].u.arr, &s);
        }
    }

    Value ret = call_user_func(&ip, main_fn, args, nargs, 0, 0);

    int exit_code = 0;
    if (ip.signal == SIGNAL_THROW) {
        const char *tname = ip.throw_type_name ? ip.throw_type_name : "Exception";
        const char *msg = "";
        if (ip.throw_line > 0) {
            const char *file = NULL;
            int mapped_line = 0;
            error_translate_location(ip.throw_line, &file, &mapped_line);
            fprintf(stderr, "%s  line %d, column %d\n",
                    file ? file : "<unknown>", mapped_line, ip.throw_col);
        }
        if (ip.throw_val.type == TYPE_STRUCT && ip.throw_val.u.st) {
            StructFieldVal *field = struct_field_lookup(ip.throw_val.u.st, "message");
            if (field && field->value.type == TYPE_STRING && field->value.u.s)
                msg = field->value.u.s;
        }
        fprintf(stderr, "Runtime error: %s: %s\n", tname, msg);
        exit_code = 2;
    } else if (main_fn->u.func_decl.ret_type == TYPE_INT) {
        exit_code = (int)ret.u.i;
    }

    value_free(&ret);
    if (nargs >= 1) value_free(&args[0]);
    value_free(&ip.ret_val);
    interp_clear_throw(&ip);
    func_decl_table_free(ip.func_decls);
    struct_decl_table_free(ip.struct_decls);
    union_decl_table_free(ip.union_decls);
    scope_free(ip.global);
    g_current_interp = NULL;

    return exit_code;
}
