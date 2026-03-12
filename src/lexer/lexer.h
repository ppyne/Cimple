#ifndef CIMPLE_LEXER_H
#define CIMPLE_LEXER_H

#include <stdint.h>
#include "parser.h"

/* -----------------------------------------------------------------------
 * Token types
 * Terminal IDs come from the Lemon-generated parser header.
 * ----------------------------------------------------------------------- */
typedef int TokenType;

enum {
    TOK_EOF = 0,
    TOK_ERROR = -1
};

/* -----------------------------------------------------------------------
 * Token structure
 * ----------------------------------------------------------------------- */
typedef struct {
    TokenType type;
    int       line;
    int       col;
    int       start_offset;
    int       end_offset;

    /* Semantic payload */
    union {
        int64_t  ival;       /* TOK_INT_LIT             */
        double   fval;       /* TOK_FLOAT_LIT            */
        char    *sval;       /* TOK_STRING_LIT, TOK_IDENT */
    } v;
} Token;

/* -----------------------------------------------------------------------
 * Lexer state
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *buf;   /* full source buffer (NUL-terminated)     */
    const char *cur;   /* current position (re2c pointer)         */
    const char *mar;   /* re2c marker                             */
    const char *tok;   /* start of current token                  */
    int         line;
    int         col;
    int         tok_col; /* column at start of current token      */
} Lexer;

/* -----------------------------------------------------------------------
 * API
 * ----------------------------------------------------------------------- */

/* Initialise lexer from a NUL-terminated source buffer. */
void  lexer_init(Lexer *l, const char *src);

/* Return the next token. Caller must free tok.v.sval when TOK_STRING_LIT
 * or TOK_IDENT and done with it. */
Token lexer_next(Lexer *l);

/* Human-readable token name (for error messages). */
const char *token_type_name(TokenType t);

#endif /* CIMPLE_LEXER_H */
