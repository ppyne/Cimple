#ifndef CIMPLE_BUILTINS_H
#define CIMPLE_BUILTINS_H

#include <stdio.h>
#include "../runtime/value.h"

/*
 * Dispatch a built-in function call.
 * Returns a Value with type == TYPE_UNKNOWN if the name is not a builtin.
 */
Value builtin_call(const char *name, Value *args, int nargs, int line, int col);

/*
 * Write a JSON array of all builtin function names to `out`.
 * Used by `cimple dump-language`.
 */
void builtin_dump_names_json(FILE *out);

#endif /* CIMPLE_BUILTINS_H */
