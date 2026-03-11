#ifndef CIMPLE_LEXER_H
#define CIMPLE_LEXER_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Token types
 * Must be kept in sync with the token IDs expected by the Lemon grammar.
 * ----------------------------------------------------------------------- */
typedef enum {
    /* Literals */
    TOK_INT_LIT    = 1,
    TOK_FLOAT_LIT,
    TOK_STRING_LIT,
    TOK_TRUE,
    TOK_FALSE,

    /* Types */
    TOK_KW_INT,
    TOK_KW_FLOAT,
    TOK_KW_BOOL,
    TOK_KW_STRING,
    TOK_KW_VOID,
    TOK_KW_EXECRESULT,

    /* Control flow */
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_BREAK,
    TOK_CONTINUE,
    TOK_RETURN,

    /* Identifier */
    TOK_IDENT,

    /* Arithmetic operators */
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_PERCENT,

    /* Comparison */
    TOK_EQ,      /* == */
    TOK_NEQ,     /* != */
    TOK_LT,
    TOK_LEQ,
    TOK_GT,
    TOK_GEQ,

    /* Logical */
    TOK_AND,     /* && */
    TOK_OR,      /* || */
    TOK_NOT,     /* !  */

    /* Bitwise */
    TOK_BAND,    /* &  */
    TOK_BOR,     /* |  */
    TOK_BXOR,    /* ^  */
    TOK_BNOT,    /* ~  */
    TOK_LSHIFT,  /* << */
    TOK_RSHIFT,  /* >> */

    /* Assignment */
    TOK_ASSIGN,  /* = */

    /* Increment / decrement */
    TOK_PLUSPLUS,   /* ++ */
    TOK_MINUSMINUS, /* -- */

    /* Delimiters */
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACKET,
    TOK_RBRACKET,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_SEMICOLON,
    TOK_COMMA,

    /* Special */
    TOK_EOF,
    TOK_ERROR
} TokenType;

/* -----------------------------------------------------------------------
 * Token structure
 * ----------------------------------------------------------------------- */
typedef struct {
    TokenType type;
    int       line;
    int       col;

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
