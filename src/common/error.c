#include "error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ErrorCtx g_error_ctx;

static const char *kind_label(ErrorKind k) {
    switch (k) {
    case ERR_LEXICAL:  return "[LEXICAL ERROR]";
    case ERR_SYNTAX:   return "[SYNTAX ERROR]";
    case ERR_SEMANTIC: return "[SEMANTIC ERROR]";
    case ERR_RUNTIME:  return "[RUNTIME ERROR]";
    default:           return "[ERROR]";
    }
}

void error_init(const char *filename) {
    g_error_ctx.filename       = filename ? filename : "<unknown>";
    g_error_ctx.semantic_count = 0;
    g_error_ctx.had_fatal      = 0;
    memset(g_error_ctx.semantic, 0, sizeof(g_error_ctx.semantic));
}

void error_print(ErrorKind kind, int line, int col,
                 const char *msg, const char *hint) {
    fprintf(stderr, "%s  %s  line %d, column %d\n",
            kind_label(kind), g_error_ctx.filename, line, col);
    fprintf(stderr, "  %s\n", msg);
    if (hint && hint[0])
        fprintf(stderr, "  -> %s\n", hint);
    fprintf(stderr, "\n");
}

void error_lexical(int line, int col, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    error_print(ERR_LEXICAL, line, col, buf, NULL);
    exit(1);
}

void error_syntax(int line, int col, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    error_print(ERR_SYNTAX, line, col, buf, NULL);
    exit(1);
}

void error_semantic(int line, int col, const char *fmt, ...) {
    if (g_error_ctx.semantic_count >= MAX_ACCUMULATED_ERRORS)
        return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Error *e = &g_error_ctx.semantic[g_error_ctx.semantic_count++];
    e->kind  = ERR_SEMANTIC;
    e->line  = line;
    e->col   = col;
    strncpy(e->msg, buf, sizeof(e->msg) - 1);
    e->hint[0] = '\0';
}

void error_runtime(int line, int col, const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    error_print(ERR_RUNTIME, line, col, buf, NULL);
    exit(2);
}

int error_flush_semantic(void) {
    if (g_error_ctx.semantic_count == 0)
        return 0;
    for (int i = 0; i < g_error_ctx.semantic_count; i++) {
        Error *e = &g_error_ctx.semantic[i];
        error_print(e->kind, e->line, e->col, e->msg, e->hint);
    }
    return 1;
}
