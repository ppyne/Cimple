#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *cimple_malloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "[FATAL] out of memory\n");
        abort();
    }
    return p;
}

void *cimple_calloc(size_t n, size_t size) {
    void *p = calloc(n, size);
    if (!p) {
        fprintf(stderr, "[FATAL] out of memory\n");
        abort();
    }
    return p;
}

void *cimple_realloc(void *ptr, size_t size) {
    void *p = realloc(ptr, size);
    if (!p) {
        fprintf(stderr, "[FATAL] out of memory\n");
        abort();
    }
    return p;
}

char *cimple_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char  *p   = (char *)cimple_malloc(len + 1);
    memcpy(p, s, len + 1);
    return p;
}

char *cimple_strndup(const char *s, size_t n) {
    if (!s) return NULL;
    size_t len = strlen(s);
    if (len > n) len = n;
    char *p = (char *)cimple_malloc(len + 1);
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

char *cimple_strconcat(const char *left, const char *right) {
    size_t left_len = strlen(left);
    size_t right_len = strlen(right);
    char *out = (char *)cimple_malloc(left_len + right_len + 1);
    memcpy(out, left, left_len);
    memcpy(out + left_len, right, right_len);
    out[left_len + right_len] = '\0';
    return out;
}
