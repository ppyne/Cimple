#ifndef CIMPLE_MEMORY_H
#define CIMPLE_MEMORY_H

#include <stddef.h>

/* Allocate and zero-initialise (aborts on failure). */
void *cimple_malloc(size_t size);
void *cimple_calloc(size_t n, size_t size);
void *cimple_realloc(void *ptr, size_t size);
char *cimple_strdup(const char *s);
char *cimple_strndup(const char *s, size_t n);
char *cimple_strconcat(const char *left, const char *right);

/* Convenience macro for typed allocation. */
#define ALLOC(type)        ((type *)cimple_malloc(sizeof(type)))
#define ALLOC_N(type, n)   ((type *)cimple_calloc((n), sizeof(type)))

#endif /* CIMPLE_MEMORY_H */
