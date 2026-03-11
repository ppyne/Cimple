#ifndef CIMPLE_BUILTINS_H
#define CIMPLE_BUILTINS_H

#include "../runtime/value.h"

/*
 * Dispatch a built-in function call.
 * Returns a Value with type == TYPE_UNKNOWN if the name is not a builtin.
 */
Value builtin_call(const char *name, Value *args, int nargs, int line, int col);

#endif /* CIMPLE_BUILTINS_H */
