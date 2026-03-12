#include "error.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ErrorCtx g_error_ctx;
static const SourceMapEntry *g_source_map = NULL;
static int g_source_map_count = 0;

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
    g_error_ctx.semantic_truncated = 0;
    g_error_ctx.had_fatal      = 0;
    memset(g_error_ctx.semantic, 0, sizeof(g_error_ctx.semantic));
}

void error_set_source_map(const SourceMapEntry *entries, int count) {
    g_source_map = entries;
    g_source_map_count = count;
}

void error_clear_source_map(void) {
    g_source_map = NULL;
    g_source_map_count = 0;
}

static void translate_location(int merged_line, const char **file_out, int *line_out) {
    *file_out = g_error_ctx.filename;
    *line_out = merged_line;
    if (!g_source_map || merged_line <= 0) return;
    for (int i = 0; i < g_source_map_count; i++) {
        const SourceMapEntry *entry = &g_source_map[i];
        if (merged_line >= entry->merged_line_start && merged_line <= entry->merged_line_end) {
            *file_out = entry->display_file ? entry->display_file : g_error_ctx.filename;
            *line_out = entry->source_line_start + (merged_line - entry->merged_line_start);
            return;
        }
    }
}

void error_print_at_file(ErrorKind kind, const char *filename, int line, int col,
                         const char *msg, const char *hint) {
    fprintf(stderr, "%s  %s  line %d, column %d\n",
            kind_label(kind), filename ? filename : g_error_ctx.filename, line, col);
    fprintf(stderr, "  %s\n", msg);
    if (hint && hint[0])
        fprintf(stderr, "  -> %s\n", hint);
    fprintf(stderr, "\n");
}

void error_print(ErrorKind kind, int line, int col,
                 const char *msg, const char *hint) {
    const char *display_file;
    int display_line;
    translate_location(line, &display_file, &display_line);
    error_print_at_file(kind, display_file, display_line, col, msg, hint);
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
    va_list ap;
    va_start(ap, fmt);
    if (g_error_ctx.semantic_count >= MAX_ACCUMULATED_ERRORS) {
        g_error_ctx.semantic_truncated = 1;
        va_end(ap);
        return;
    }
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    Error *e = &g_error_ctx.semantic[g_error_ctx.semantic_count++];
    e->kind  = ERR_SEMANTIC;
    e->line  = line;
    e->col   = col;
    strncpy(e->msg, buf, sizeof(e->msg) - 1);
    e->msg[sizeof(e->msg) - 1] = '\0';
    e->hint[0] = '\0';
}

void error_semantic_hint(int line, int col, const char *hint,
                         const char *fmt, ...) {
    if (g_error_ctx.semantic_count >= MAX_ACCUMULATED_ERRORS) {
        g_error_ctx.semantic_truncated = 1;
        return;
    }
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
    e->msg[sizeof(e->msg) - 1] = '\0';
    if (hint && hint[0]) {
        strncpy(e->hint, hint, sizeof(e->hint) - 1);
        e->hint[sizeof(e->hint) - 1] = '\0';
    } else {
        e->hint[0] = '\0';
    }
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

void error_runtime_hint(int line, int col, const char *hint,
                        const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    error_print(ERR_RUNTIME, line, col, buf, hint);
    exit(2);
}

int error_flush_semantic(void) {
    if (g_error_ctx.semantic_count == 0)
        return 0;
    for (int i = 0; i < g_error_ctx.semantic_count; i++) {
        Error *e = &g_error_ctx.semantic[i];
        error_print(e->kind, e->line, e->col, e->msg, e->hint);
    }
    if (g_error_ctx.semantic_truncated) {
        fprintf(stderr, "%d semantic errors found. Further analysis aborted.\n",
                MAX_ACCUMULATED_ERRORS);
    } else {
        fprintf(stderr, "%d semantic error(s) found.\n",
                g_error_ctx.semantic_count);
    }
    return 1;
}
