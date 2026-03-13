#include "value.h"
#include "../common/memory.h"
#include "../common/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -----------------------------------------------------------------------
 * Constructors
 * ----------------------------------------------------------------------- */
Value val_int(int64_t v)   { Value r; r.type = TYPE_INT;   r.u.i = v; return r; }
Value val_float(double v)  { Value r; r.type = TYPE_FLOAT; r.u.f = v; return r; }
Value val_bool(int v)      { Value r; r.type = TYPE_BOOL;  r.u.b = v ? 1 : 0; return r; }
Value val_byte(unsigned char v) { Value r; r.type = TYPE_BYTE; r.u.i = v; return r; }
Value val_void(void)       { Value r; r.type = TYPE_VOID;  r.u.i = 0; return r; }

Value val_string(const char *s) {
    Value r;
    r.type  = TYPE_STRING;
    r.u.s   = cimple_strdup(s ? s : "");
    return r;
}

Value val_string_own(char *s) {
    Value r;
    r.type = TYPE_STRING;
    r.u.s  = s;
    return r;
}

Value val_func(const char *name) {
    Value r;
    r.type = TYPE_FUNC;
    r.u.s = cimple_strdup(name ? name : "");
    return r;
}

Value val_array(CimpleType elem_type) {
    Value r;
    r.type  = type_make_array(elem_type);
    r.u.arr = ALLOC(ArrayVal);
    r.u.arr->elem_type = elem_type;
    r.u.arr->struct_name = NULL;
    r.u.arr->count     = 0;
    r.u.arr->cap       = 0;
    r.u.arr->data.ints = NULL;
    return r;
}

Value val_struct(const char *struct_name, int field_count) {
    Value r;
    r.type = TYPE_STRUCT;
    r.u.st = ALLOC(StructVal);
    r.u.st->struct_name = cimple_strdup(struct_name);
    r.u.st->field_count = field_count;
    r.u.st->fields = field_count ? ALLOC_N(StructFieldVal, field_count) : NULL;
    if (field_count > 0)
        memset(r.u.st->fields, 0, (size_t)field_count * sizeof(StructFieldVal));
    return r;
}

Value val_union(const char *union_name, int member_count) {
    Value r;
    r.type = TYPE_UNION;
    r.u.un = ALLOC(UnionVal);
    r.u.un->union_name = cimple_strdup(union_name);
    r.u.un->kind = -1;
    r.u.un->member_count = member_count;
    r.u.un->members = member_count ? ALLOC_N(Value, member_count) : NULL;
    r.u.un->member_types = member_count ? ALLOC_N(CimpleType, member_count) : NULL;
    r.u.un->member_names = member_count ? ALLOC_N(char *, member_count) : NULL;
    r.u.un->member_struct_names = member_count ? ALLOC_N(char *, member_count) : NULL;
    for (int i = 0; i < member_count; i++) {
        r.u.un->members[i] = val_void();
        r.u.un->member_types[i] = TYPE_UNKNOWN;
        r.u.un->member_names[i] = NULL;
        r.u.un->member_struct_names[i] = NULL;
    }
    return r;
}

Value val_exec(int status, char *out, char *err) {
    Value r;
    r.type           = TYPE_EXEC_RESULT;
    r.u.exec.status  = status;
    r.u.exec.out     = out;
    r.u.exec.err     = err;
    return r;
}

/* -----------------------------------------------------------------------
 * Default values
 * ----------------------------------------------------------------------- */
Value value_default(CimpleType t) {
    switch (t) {
    case TYPE_INT:    return val_int(0);
    case TYPE_FLOAT:  return val_float(0.0);
    case TYPE_BOOL:   return val_bool(0);
    case TYPE_STRING: return val_string("");
    case TYPE_FUNC:   return val_func("");
    case TYPE_BYTE:   return val_byte(0);
    case TYPE_UNION: { Value v; v.type = TYPE_VOID; v.u.i = 0; return v; }
    case TYPE_INT_ARR:      return val_array(TYPE_INT);
    case TYPE_FLOAT_ARR:    return val_array(TYPE_FLOAT);
    case TYPE_BOOL_ARR:     return val_array(TYPE_BOOL);
    case TYPE_STR_ARR:      return val_array(TYPE_STRING);
    case TYPE_BYTE_ARR:     return val_array(TYPE_BYTE);
    case TYPE_STRUCT_ARR:   return val_array(TYPE_STRUCT);
    case TYPE_UNION_ARR:    return val_array(TYPE_UNION);
    /* 2D array defaults — empty outer array */
    case TYPE_INT_ARR_ARR:    return val_array(TYPE_INT_ARR);
    case TYPE_FLOAT_ARR_ARR:  return val_array(TYPE_FLOAT_ARR);
    case TYPE_BOOL_ARR_ARR:   return val_array(TYPE_BOOL_ARR);
    case TYPE_STR_ARR_ARR:    return val_array(TYPE_STR_ARR);
    case TYPE_BYTE_ARR_ARR:   return val_array(TYPE_BYTE_ARR);
    case TYPE_STRUCT_ARR_ARR: return val_array(TYPE_STRUCT_ARR);
    case TYPE_UNION_ARR_ARR:  return val_array(TYPE_UNION_ARR);
    default: { Value v; v.type = TYPE_VOID; v.u.i = 0; return v; }
    }
}

/* -----------------------------------------------------------------------
 * Array helpers
 * ----------------------------------------------------------------------- */
static void arr_ensure(ArrayVal *a, int new_count) {
    if (new_count <= a->cap) return;
    int new_cap = a->cap ? a->cap * 2 : 4;
    while (new_cap < new_count) new_cap *= 2;
    switch (a->elem_type) {
    case TYPE_INT:
        a->data.ints = (int64_t *)cimple_realloc(a->data.ints,
                                                   (size_t)new_cap * sizeof(int64_t));
        break;
    case TYPE_FLOAT:
        a->data.floats = (double *)cimple_realloc(a->data.floats,
                                                   (size_t)new_cap * sizeof(double));
        break;
    case TYPE_BOOL:
        a->data.bools = (int *)cimple_realloc(a->data.bools,
                                               (size_t)new_cap * sizeof(int));
        break;
    case TYPE_STRING:
        a->data.strings = (char **)cimple_realloc(a->data.strings,
                                                    (size_t)new_cap * sizeof(char *));
        break;
    case TYPE_BYTE:
        a->data.bytes = (unsigned char *)cimple_realloc(a->data.bytes,
                                                   (size_t)new_cap * sizeof(unsigned char));
        break;
    case TYPE_STRUCT:
        a->data.structs = (StructVal **)cimple_realloc(a->data.structs,
                                                   (size_t)new_cap * sizeof(StructVal *));
        break;
    case TYPE_UNION:
        a->data.unions = (UnionVal **)cimple_realloc(a->data.unions,
                                                   (size_t)new_cap * sizeof(UnionVal *));
        break;
    default:
        /* 2D arrays: elem_type is itself an array type */
        if (type_is_array(a->elem_type)) {
            a->data.arrays = (ArrayVal **)cimple_realloc(a->data.arrays,
                                                         (size_t)new_cap * sizeof(ArrayVal *));
        }
        break;
    }
    a->cap = new_cap;
}

void array_push(ArrayVal *a, Value v) {
    arr_ensure(a, a->count + 1);
    switch (a->elem_type) {
    case TYPE_INT:    a->data.ints[a->count]    = v.u.i; break;
    case TYPE_FLOAT:  a->data.floats[a->count]  = v.u.f; break;
    case TYPE_BOOL:   a->data.bools[a->count]   = v.u.b; break;
    case TYPE_STRING: a->data.strings[a->count] = cimple_strdup(v.u.s); break;
    case TYPE_BYTE:   a->data.bytes[a->count]   = (unsigned char)v.u.i; break;
    case TYPE_STRUCT: a->data.structs[a->count] = value_copy(v).u.st; break;
    case TYPE_UNION:  a->data.unions[a->count] = value_copy(v).u.un; break;
    default:
        if (type_is_array(a->elem_type)) {
            /* deep-copy the inner array and store the ArrayVal* */
            a->data.arrays[a->count] = value_copy(v).u.arr;
        }
        break;
    }
    a->count++;
}

void array_push_owned(ArrayVal *a, Value *v) {
    arr_ensure(a, a->count + 1);
    switch (a->elem_type) {
    case TYPE_INT:
        a->data.ints[a->count++] = v->u.i;
        break;
    case TYPE_FLOAT:
        a->data.floats[a->count++] = v->u.f;
        break;
    case TYPE_BOOL:
        a->data.bools[a->count++] = v->u.b;
        break;
    case TYPE_STRING:
        a->data.strings[a->count++] = v->u.s;
        v->u.s = NULL;
        v->type = TYPE_VOID;
        break;
    case TYPE_BYTE:
        a->data.bytes[a->count++] = (unsigned char)v->u.i;
        break;
    case TYPE_STRUCT:
        a->data.structs[a->count++] = v->u.st;
        v->u.st = NULL;
        v->type = TYPE_VOID;
        break;
    case TYPE_UNION:
        a->data.unions[a->count++] = v->u.un;
        v->u.un = NULL;
        v->type = TYPE_VOID;
        break;
    default:
        if (type_is_array(a->elem_type)) {
            a->data.arrays[a->count++] = v->u.arr;
            v->u.arr = NULL;
            v->type = TYPE_VOID;
        }
        break;
    }
}

Value array_pop(ArrayVal *a, int line, int col) {
    if (a->count == 0)
        error_runtime(line, col, "Cannot pop from empty array");
    a->count--;
    switch (a->elem_type) {
    case TYPE_INT:    return val_int(a->data.ints[a->count]);
    case TYPE_FLOAT:  return val_float(a->data.floats[a->count]);
    case TYPE_BOOL:   return val_bool(a->data.bools[a->count]);
    case TYPE_STRING: {
        char *s = a->data.strings[a->count];
        a->data.strings[a->count] = NULL;
        return val_string_own(s);
    }
    case TYPE_BYTE:   return val_byte(a->data.bytes[a->count]);
    case TYPE_STRUCT: {
        Value v; v.type = TYPE_STRUCT; v.u.st = a->data.structs[a->count]; a->data.structs[a->count] = NULL; return v;
    }
    case TYPE_UNION: {
        Value v; v.type = TYPE_UNION; v.u.un = a->data.unions[a->count]; a->data.unions[a->count] = NULL; return v;
    }
    default:
        if (type_is_array(a->elem_type)) {
            Value v; v.type = a->elem_type; v.u.arr = a->data.arrays[a->count]; a->data.arrays[a->count] = NULL; return v;
        }
        return val_void();
    }
}

void array_insert(ArrayVal *a, int idx, Value v, int line, int col) {
    if (idx < 0 || idx > a->count)
        error_runtime(line, col,
                      "arrayInsert: index out of bounds (Index: %d   Array size: %d)",
                      idx, a->count);
    arr_ensure(a, a->count + 1);
    switch (a->elem_type) {
    case TYPE_INT:
        memmove(&a->data.ints[idx + 1], &a->data.ints[idx],
                (size_t)(a->count - idx) * sizeof(int64_t));
        a->data.ints[idx] = v.u.i;
        break;
    case TYPE_FLOAT:
        memmove(&a->data.floats[idx + 1], &a->data.floats[idx],
                (size_t)(a->count - idx) * sizeof(double));
        a->data.floats[idx] = v.u.f;
        break;
    case TYPE_BOOL:
        memmove(&a->data.bools[idx + 1], &a->data.bools[idx],
                (size_t)(a->count - idx) * sizeof(int));
        a->data.bools[idx] = v.u.b;
        break;
    case TYPE_STRING:
        memmove(&a->data.strings[idx + 1], &a->data.strings[idx],
                (size_t)(a->count - idx) * sizeof(char *));
        a->data.strings[idx] = cimple_strdup(v.u.s);
        break;
    case TYPE_BYTE:
        memmove(&a->data.bytes[idx + 1], &a->data.bytes[idx],
                (size_t)(a->count - idx) * sizeof(unsigned char));
        a->data.bytes[idx] = (unsigned char)v.u.i;
        break;
    case TYPE_STRUCT:
        memmove(&a->data.structs[idx + 1], &a->data.structs[idx],
                (size_t)(a->count - idx) * sizeof(StructVal *));
        a->data.structs[idx] = value_copy(v).u.st;
        break;
    case TYPE_UNION:
        memmove(&a->data.unions[idx + 1], &a->data.unions[idx],
                (size_t)(a->count - idx) * sizeof(UnionVal *));
        a->data.unions[idx] = value_copy(v).u.un;
        break;
    default:
        if (type_is_array(a->elem_type)) {
            memmove(&a->data.arrays[idx + 1], &a->data.arrays[idx],
                    (size_t)(a->count - idx) * sizeof(ArrayVal *));
            a->data.arrays[idx] = value_copy(v).u.arr;
        }
        break;
    }
    a->count++;
}

void array_remove(ArrayVal *a, int idx, int line, int col) {
    if (idx < 0 || idx >= a->count)
        error_runtime(line, col,
                      "arrayRemove: index out of bounds (Index: %d   Array size: %d)",
                      idx, a->count);
    if (a->elem_type == TYPE_STRING)
        free(a->data.strings[idx]);
    if (a->elem_type == TYPE_STRUCT) {
        Value tmp; tmp.type = TYPE_STRUCT; tmp.u.st = a->data.structs[idx];
        value_free(&tmp);
    }
    if (a->elem_type == TYPE_UNION) {
        Value tmp; tmp.type = TYPE_UNION; tmp.u.un = a->data.unions[idx];
        value_free(&tmp);
    }
    if (type_is_array(a->elem_type) && a->data.arrays[idx]) {
        Value tmp; tmp.type = a->elem_type; tmp.u.arr = a->data.arrays[idx];
        value_free(&tmp);
    }
    switch (a->elem_type) {
    case TYPE_INT:
        memmove(&a->data.ints[idx], &a->data.ints[idx + 1],
                (size_t)(a->count - idx - 1) * sizeof(int64_t));
        break;
    case TYPE_FLOAT:
        memmove(&a->data.floats[idx], &a->data.floats[idx + 1],
                (size_t)(a->count - idx - 1) * sizeof(double));
        break;
    case TYPE_BOOL:
        memmove(&a->data.bools[idx], &a->data.bools[idx + 1],
                (size_t)(a->count - idx - 1) * sizeof(int));
        break;
    case TYPE_STRING:
        memmove(&a->data.strings[idx], &a->data.strings[idx + 1],
                (size_t)(a->count - idx - 1) * sizeof(char *));
        break;
    case TYPE_BYTE:
        memmove(&a->data.bytes[idx], &a->data.bytes[idx + 1],
                (size_t)(a->count - idx - 1) * sizeof(unsigned char));
        break;
    case TYPE_STRUCT:
        memmove(&a->data.structs[idx], &a->data.structs[idx + 1],
                (size_t)(a->count - idx - 1) * sizeof(StructVal *));
        break;
    case TYPE_UNION:
        memmove(&a->data.unions[idx], &a->data.unions[idx + 1],
                (size_t)(a->count - idx - 1) * sizeof(UnionVal *));
        break;
    default:
        if (type_is_array(a->elem_type)) {
            memmove(&a->data.arrays[idx], &a->data.arrays[idx + 1],
                    (size_t)(a->count - idx - 1) * sizeof(ArrayVal *));
        }
        break;
    }
    a->count--;
}

Value array_get(ArrayVal *a, int idx, int line, int col) {
    if (idx < 0 || idx >= a->count)
        error_runtime(line, col,
                      "Array index out of bounds (Index: %d   Array size: %d)",
                      idx, a->count);
    switch (a->elem_type) {
    case TYPE_INT:    return val_int(a->data.ints[idx]);
    case TYPE_FLOAT:  return val_float(a->data.floats[idx]);
    case TYPE_BOOL:   return val_bool(a->data.bools[idx]);
    case TYPE_STRING: return val_string(a->data.strings[idx]);
    case TYPE_BYTE:   return val_byte(a->data.bytes[idx]);
    case TYPE_STRUCT: {
        Value v; v.type = TYPE_STRUCT; v.u.st = a->data.structs[idx];
        return value_copy(v);
    }
    case TYPE_UNION: {
        Value v; v.type = TYPE_UNION; v.u.un = a->data.unions[idx];
        return value_copy(v);
    }
    default:
        if (type_is_array(a->elem_type)) {
            Value v; v.type = a->elem_type; v.u.arr = a->data.arrays[idx];
            return value_copy(v);
        }
        return val_void();
    }
}

void array_set(ArrayVal *a, int idx, Value v, int line, int col) {
    if (idx < 0 || idx >= a->count)
        error_runtime(line, col,
                      "Array index out of bounds (Index: %d   Array size: %d)",
                      idx, a->count);
    switch (a->elem_type) {
    case TYPE_INT:   a->data.ints[idx]  = v.u.i; break;
    case TYPE_FLOAT: a->data.floats[idx] = v.u.f; break;
    case TYPE_BOOL:  a->data.bools[idx]  = v.u.b; break;
    case TYPE_STRING:
        free(a->data.strings[idx]);
        a->data.strings[idx] = cimple_strdup(v.u.s);
        break;
    case TYPE_BYTE:   a->data.bytes[idx] = (unsigned char)v.u.i; break;
    case TYPE_STRUCT: {
        Value tmp; tmp.type = TYPE_STRUCT; tmp.u.st = a->data.structs[idx];
        value_free(&tmp);
        a->data.structs[idx] = value_copy(v).u.st;
        break;
    }
    case TYPE_UNION: {
        Value tmp; tmp.type = TYPE_UNION; tmp.u.un = a->data.unions[idx];
        value_free(&tmp);
        a->data.unions[idx] = value_copy(v).u.un;
        break;
    }
    default:
        if (type_is_array(a->elem_type)) {
            if (a->data.arrays[idx]) {
                Value tmp; tmp.type = a->elem_type; tmp.u.arr = a->data.arrays[idx];
                value_free(&tmp);
            }
            a->data.arrays[idx] = value_copy(v).u.arr;
        }
        break;
    }
}

void array_set_owned(ArrayVal *a, int idx, Value *v, int line, int col) {
    if (idx < 0 || idx >= a->count)
        error_runtime(line, col,
                      "Array index out of bounds (Index: %d   Array size: %d)",
                      idx, a->count);
    switch (a->elem_type) {
    case TYPE_INT:
        a->data.ints[idx] = v->u.i;
        break;
    case TYPE_FLOAT:
        a->data.floats[idx] = v->u.f;
        break;
    case TYPE_BOOL:
        a->data.bools[idx] = v->u.b;
        break;
    case TYPE_STRING:
        free(a->data.strings[idx]);
        a->data.strings[idx] = v->u.s;
        v->u.s = NULL;
        v->type = TYPE_VOID;
        break;
    case TYPE_BYTE:
        a->data.bytes[idx] = (unsigned char)v->u.i;
        break;
    case TYPE_STRUCT: {
        Value tmp; tmp.type = TYPE_STRUCT; tmp.u.st = a->data.structs[idx];
        value_free(&tmp);
        a->data.structs[idx] = v->u.st;
        v->u.st = NULL;
        v->type = TYPE_VOID;
        break;
    }
    case TYPE_UNION: {
        Value tmp; tmp.type = TYPE_UNION; tmp.u.un = a->data.unions[idx];
        value_free(&tmp);
        a->data.unions[idx] = v->u.un;
        v->u.un = NULL;
        v->type = TYPE_VOID;
        break;
    }
    default:
        if (type_is_array(a->elem_type)) {
            if (a->data.arrays[idx]) {
                Value tmp; tmp.type = a->elem_type; tmp.u.arr = a->data.arrays[idx];
                value_free(&tmp);
            }
            a->data.arrays[idx] = v->u.arr;
            v->u.arr = NULL;
            v->type = TYPE_VOID;
        }
        break;
    }
}

/* -----------------------------------------------------------------------
 * Deep copy
 * ----------------------------------------------------------------------- */
Value value_copy(Value v) {
    if (v.type == TYPE_STRING)
        return val_string(v.u.s);
    if (v.type == TYPE_FUNC)
        return val_func(v.u.s);
    if (type_is_array(v.type)) {
        ArrayVal *src = v.u.arr;
        Value dst = val_array(src->elem_type);
        ArrayVal *out = dst.u.arr;
        arr_ensure(out, src->count);
        out->count = src->count;
        switch (src->elem_type) {
        case TYPE_INT:
            memcpy(out->data.ints, src->data.ints, (size_t)src->count * sizeof(int64_t));
            break;
        case TYPE_FLOAT:
            memcpy(out->data.floats, src->data.floats, (size_t)src->count * sizeof(double));
            break;
        case TYPE_BOOL:
            memcpy(out->data.bools, src->data.bools, (size_t)src->count * sizeof(int));
            break;
        case TYPE_STRING:
            for (int i = 0; i < src->count; i++) {
                out->data.strings[i] = cimple_strdup(src->data.strings[i]);
            }
            break;
        case TYPE_BYTE:
            memcpy(out->data.bytes, src->data.bytes, (size_t)src->count * sizeof(unsigned char));
            break;
        case TYPE_STRUCT:
            out->struct_name = src->struct_name ? cimple_strdup(src->struct_name) : NULL;
            for (int i = 0; i < src->count; i++) {
                Value tmp; tmp.type = TYPE_STRUCT; tmp.u.st = src->data.structs[i];
                out->data.structs[i] = value_copy(tmp).u.st;
            }
            break;
        case TYPE_UNION:
            out->struct_name = src->struct_name ? cimple_strdup(src->struct_name) : NULL;
            for (int i = 0; i < src->count; i++) {
                Value tmp; tmp.type = TYPE_UNION; tmp.u.un = src->data.unions[i];
                out->data.unions[i] = value_copy(tmp).u.un;
            }
            break;
        default:
            if (type_is_array(src->elem_type)) {
                /* 2D array: deep-copy each inner ArrayVal */
                out->struct_name = src->struct_name ? cimple_strdup(src->struct_name) : NULL;
                for (int i = 0; i < src->count; i++) {
                    Value tmp; tmp.type = src->elem_type; tmp.u.arr = src->data.arrays[i];
                    out->data.arrays[i] = value_copy(tmp).u.arr;
                }
            } else {
                out->count = 0;
            }
            break;
        }
        return dst;
    }
    if (v.type == TYPE_STRUCT) {
        StructVal *src = v.u.st;
        Value out = val_struct(src->struct_name, src->field_count);
        for (int i = 0; i < src->field_count; i++) {
            out.u.st->fields[i].name = cimple_strdup(src->fields[i].name);
            out.u.st->fields[i].type = src->fields[i].type;
            out.u.st->fields[i].struct_name = src->fields[i].struct_name
                ? cimple_strdup(src->fields[i].struct_name) : NULL;
            out.u.st->fields[i].value = value_copy(src->fields[i].value);
        }
        return out;
    }
    if (v.type == TYPE_UNION) {
        UnionVal *src = v.u.un;
        Value out = val_union(src->union_name, src->member_count);
        out.u.un->kind = src->kind;
        for (int i = 0; i < src->member_count; i++) {
            out.u.un->member_types[i] = src->member_types[i];
            out.u.un->member_names[i] = src->member_names[i] ? cimple_strdup(src->member_names[i]) : NULL;
            out.u.un->member_struct_names[i] = src->member_struct_names[i]
                ? cimple_strdup(src->member_struct_names[i]) : NULL;
            out.u.un->members[i] = value_copy(src->members[i]);
        }
        return out;
    }
    if (v.type == TYPE_EXEC_RESULT) {
        return val_exec(v.u.exec.status,
                        cimple_strdup(v.u.exec.out),
                        cimple_strdup(v.u.exec.err));
    }
    return v;
}

/* -----------------------------------------------------------------------
 * Free
 * ----------------------------------------------------------------------- */
void value_free(Value *v) {
    if (!v) return;
    if (v->type == TYPE_STRING || v->type == TYPE_FUNC) {
        free(v->u.s);
        v->u.s = NULL;
    } else if (type_is_array(v->type)) {
        ArrayVal *a = v->u.arr;
        if (a) {
            if (a->elem_type == TYPE_STRING) {
                for (int i = 0; i < a->count; i++)
                    free(a->data.strings[i]);
            }
            if (a->elem_type == TYPE_STRUCT) {
                for (int i = 0; i < a->count; i++) {
                    Value tmp; tmp.type = TYPE_STRUCT; tmp.u.st = a->data.structs[i];
                    value_free(&tmp);
                }
            }
            if (a->elem_type == TYPE_UNION) {
                for (int i = 0; i < a->count; i++) {
                    Value tmp; tmp.type = TYPE_UNION; tmp.u.un = a->data.unions[i];
                    value_free(&tmp);
                }
            }
            if (type_is_array(a->elem_type)) {
                /* 2D array: free each inner array */
                for (int i = 0; i < a->count; i++) {
                    if (a->data.arrays[i]) {
                        Value tmp; tmp.type = a->elem_type; tmp.u.arr = a->data.arrays[i];
                        value_free(&tmp);
                    }
                }
            }
            free(a->struct_name);
            free(a->data.ints);
            free(a);
        }
        v->u.arr = NULL;
    } else if (v->type == TYPE_STRUCT) {
        StructVal *st = v->u.st;
        if (st) {
            free(st->struct_name);
            for (int i = 0; i < st->field_count; i++) {
                free(st->fields[i].name);
                free(st->fields[i].struct_name);
                value_free(&st->fields[i].value);
            }
            free(st->fields);
            free(st);
        }
        v->u.st = NULL;
    } else if (v->type == TYPE_UNION) {
        UnionVal *un = v->u.un;
        if (un) {
            free(un->union_name);
            for (int i = 0; i < un->member_count; i++) {
                free(un->member_names[i]);
                free(un->member_struct_names[i]);
                value_free(&un->members[i]);
            }
            free(un->members);
            free(un->member_types);
            free(un->member_names);
            free(un->member_struct_names);
            free(un);
        }
        v->u.un = NULL;
    } else if (v->type == TYPE_EXEC_RESULT) {
        free(v->u.exec.out);
        free(v->u.exec.err);
        v->u.exec.out = NULL;
        v->u.exec.err = NULL;
    }
}

/* -----------------------------------------------------------------------
 * Display conversion
 * ----------------------------------------------------------------------- */
char *value_to_display(Value v) {
    char buf[64];
    switch (v.type) {
    case TYPE_INT:
        snprintf(buf, sizeof(buf), "%lld", (long long)v.u.i);
        return cimple_strdup(buf);
    case TYPE_FLOAT:
        if (isnan(v.u.f))             return cimple_strdup("NaN");
        if (isinf(v.u.f))             return cimple_strdup(v.u.f > 0 ? "Infinity" : "-Infinity");
        snprintf(buf, sizeof(buf), "%.17g", v.u.f);
        return cimple_strdup(buf);
    case TYPE_BOOL:
        return cimple_strdup(v.u.b ? "true" : "false");
    case TYPE_STRING:
        return cimple_strdup(v.u.s ? v.u.s : "");
    case TYPE_FUNC:
        return cimple_strdup(v.u.s ? v.u.s : "");
    case TYPE_BYTE:
        snprintf(buf, sizeof(buf), "%u", (unsigned)v.u.i);
        return cimple_strdup(buf);
    case TYPE_STRUCT:
        return cimple_strdup(v.u.st && v.u.st->struct_name ? v.u.st->struct_name : "<struct>");
    case TYPE_UNION:
        return cimple_strdup(v.u.un && v.u.un->union_name ? v.u.un->union_name : "<union>");
    default:
        return cimple_strdup("<void>");
    }
}
