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

Value val_array(CimpleType elem_type) {
    Value r;
    r.type  = type_make_array(elem_type);
    r.u.arr = ALLOC(ArrayVal);
    r.u.arr->elem_type = elem_type;
    r.u.arr->count     = 0;
    r.u.arr->cap       = 0;
    r.u.arr->data.ints = NULL;
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
    case TYPE_INT_ARR:   return val_array(TYPE_INT);
    case TYPE_FLOAT_ARR: return val_array(TYPE_FLOAT);
    case TYPE_BOOL_ARR:  return val_array(TYPE_BOOL);
    case TYPE_STR_ARR:   return val_array(TYPE_STRING);
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
    default: break;
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
    default: break;
    }
    a->count++;
}

Value array_pop(ArrayVal *a, int line, int col) {
    if (a->count == 0)
        error_runtime(line, col, "arrayPop: array is empty");
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
    default: return val_void();
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
    default: break;
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
    default: break;
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
    default:          return val_void();
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
    default: break;
    }
}

/* -----------------------------------------------------------------------
 * Deep copy
 * ----------------------------------------------------------------------- */
Value value_copy(Value v) {
    if (v.type == TYPE_STRING)
        return val_string(v.u.s);
    if (type_is_array(v.type)) {
        ArrayVal *src = v.u.arr;
        Value dst = val_array(src->elem_type);
        for (int i = 0; i < src->count; i++) {
            Value elem = array_get(src, i, 0, 0);
            array_push(dst.u.arr, elem);
            if (src->elem_type == TYPE_STRING) value_free(&elem);
        }
        return dst;
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
    if (v->type == TYPE_STRING) {
        free(v->u.s);
        v->u.s = NULL;
    } else if (type_is_array(v->type)) {
        ArrayVal *a = v->u.arr;
        if (a) {
            if (a->elem_type == TYPE_STRING) {
                for (int i = 0; i < a->count; i++)
                    free(a->data.strings[i]);
            }
            free(a->data.ints);
            free(a);
        }
        v->u.arr = NULL;
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
    default:
        return cimple_strdup("<void>");
    }
}
