#ifndef CIMPLE_ERROR_H
#define CIMPLE_ERROR_H

#include <stdio.h>

/* -----------------------------------------------------------------------
 * Error kinds
 * ----------------------------------------------------------------------- */
typedef enum {
    ERR_LEXICAL   = 0,
    ERR_SYNTAX    = 1,
    ERR_SEMANTIC  = 2,
    ERR_RUNTIME   = 3
} ErrorKind;

/* -----------------------------------------------------------------------
 * Error record (used for semantic error accumulation)
 * ----------------------------------------------------------------------- */
#define MAX_ACCUMULATED_ERRORS 10

typedef struct {
    ErrorKind kind;
    int       line;
    int       col;
    char      msg[512];
    char      hint[256];
} Error;

/* -----------------------------------------------------------------------
 * Global error context
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *filename;          /* source file path (for messages)  */
    int         semantic_count;    /* number of accumulated errors       */
    int         semantic_truncated;/* reached MAX_ACCUMULATED_ERRORS     */
    Error       semantic[MAX_ACCUMULATED_ERRORS];
    int         had_fatal;         /* non-zero if a fatal error occurred */
} ErrorCtx;

extern ErrorCtx g_error_ctx;

/* -----------------------------------------------------------------------
 * API
 * ----------------------------------------------------------------------- */

/* Initialise the error context for a new source file. */
void error_init(const char *filename);

/* Fatal lexical/syntax error — prints to stderr and exits(1). */
void error_lexical(int line, int col, const char *fmt, ...);
void error_syntax(int line, int col, const char *fmt, ...);

/* Accumulate a semantic error (up to MAX_ACCUMULATED_ERRORS). */
void error_semantic(int line, int col, const char *fmt, ...);

/* Fatal runtime error — prints to stderr and exits(2). */
void error_runtime(int line, int col, const char *fmt, ...);

/* Print all accumulated semantic errors and return 1 if any, else 0. */
int  error_flush_semantic(void);

/* Print a single error with the standard format to stderr. */
void error_print(ErrorKind kind, int line, int col,
                 const char *msg, const char *hint);

#endif /* CIMPLE_ERROR_H */
