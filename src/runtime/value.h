#ifndef CIMPLE_VALUE_H
#define CIMPLE_VALUE_H

#include "../ast/ast.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Runtime value
 * ----------------------------------------------------------------------- */

typedef struct Value Value;
typedef struct StructFieldVal StructFieldVal;
typedef struct StructVal StructVal;
typedef struct UnionVal UnionVal;
typedef struct ArrayVal ArrayVal;   /* forward typedef so 'ArrayVal **' works inside the struct */

/* Dynamic array (used for arrays at runtime). */
struct ArrayVal {
    CimpleType elem_type;
    char      *struct_name;
    union {
        int64_t *ints;
        double  *floats;
        int     *bools;
        char   **strings;
        unsigned char *bytes;
        struct StructVal **structs;
        struct UnionVal  **unions;
        ArrayVal         **arrays;   /* for 2D arrays: elem_type is itself an array type */
    } data;
    int count;
    int cap;
};

/* Result of exec(). */
typedef struct {
    int    status;
    char  *out;
    char  *err;
} ExecResultVal;

struct Value {
    CimpleType type;
    union {
        int64_t       i;
        double        f;
        int           b;
        char         *s;
        ArrayVal     *arr;
        StructVal    *st;
        UnionVal     *un;
        ExecResultVal exec;
    } u;
};

struct StructFieldVal {
    char      *name;
    CimpleType type;
    char      *struct_name;
    Value      value;
};

struct StructVal {
    char           *struct_name;
    StructFieldVal *fields;
    int             field_count;
};

struct UnionVal {
    char      *union_name;
    int        kind;          /* -1 means no active member */
    Value     *members;
    CimpleType *member_types;
    char     **member_names;
    char     **member_struct_names;
    int        member_count;
};

/* -----------------------------------------------------------------------
 * Value construction
 * ----------------------------------------------------------------------- */
Value  val_int(int64_t v);
Value  val_float(double v);
Value  val_bool(int v);
Value  val_byte(unsigned char v);
Value  val_string(const char *s);      /* duplicates s */
Value  val_string_own(char *s);        /* takes ownership */
Value  val_func(const char *name);     /* duplicates function name */
Value  val_void(void);
Value  val_array(CimpleType elem_type);
Value  val_struct(const char *struct_name, int field_count);
Value  val_union(const char *union_name, int member_count);
Value  val_exec(int status, char *out, char *err);

/* -----------------------------------------------------------------------
 * Array helpers
 * ----------------------------------------------------------------------- */
void   array_push(ArrayVal *a, Value v);
void   array_push_owned(ArrayVal *a, Value *v);
Value  array_pop(ArrayVal *a, int line, int col);
void   array_insert(ArrayVal *a, int idx, Value v, int line, int col);
void   array_remove(ArrayVal *a, int idx, int line, int col);
Value  array_get(ArrayVal *a, int idx, int line, int col);
void   array_set(ArrayVal *a, int idx, Value v, int line, int col);
void   array_set_owned(ArrayVal *a, int idx, Value *v, int line, int col);

/* -----------------------------------------------------------------------
 * Value utilities
 * ----------------------------------------------------------------------- */
Value  value_copy(Value v);     /* deep copy */
void   value_free(Value *v);    /* release owned resources */

/* Convert a value to a display string (caller frees result). */
char  *value_to_display(Value v);

/* Default (zero) value for a type. */
Value  value_default(CimpleType t);

#endif /* CIMPLE_VALUE_H */
