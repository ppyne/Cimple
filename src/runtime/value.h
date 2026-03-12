#ifndef CIMPLE_VALUE_H
#define CIMPLE_VALUE_H

#include "../ast/ast.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * Runtime value
 * ----------------------------------------------------------------------- */

/* Dynamic array (used for int[], float[], bool[], string[] at runtime). */
typedef struct {
    CimpleType elem_type;
    union {
        int64_t *ints;
        double  *floats;
        int     *bools;
        char   **strings;
        unsigned char *bytes;
    } data;
    int count;
    int cap;
} ArrayVal;

/* Result of exec(). */
typedef struct {
    int    status;
    char  *out;
    char  *err;
} ExecResultVal;

typedef struct Value Value;

struct Value {
    CimpleType type;
    union {
        int64_t       i;
        double        f;
        int           b;
        char         *s;
        ArrayVal     *arr;
        ExecResultVal exec;
    } u;
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
Value  val_void(void);
Value  val_array(CimpleType elem_type);
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
