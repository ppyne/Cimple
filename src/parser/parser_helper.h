#ifndef CIMPLE_PARSER_HELPER_H
#define CIMPLE_PARSER_HELPER_H

#include <string.h>
#include "../ast/ast.h"
#include "../lexer/lexer.h"

/* -----------------------------------------------------------------------
 * Parser state — passed as extra argument to Lemon
 * ----------------------------------------------------------------------- */
typedef struct ParseStructName {
    char *name;
    struct ParseStructName *next;
} ParseStructName;

typedef struct {
    AstNode  *root;     /* result of the parse */
    int       error;    /* non-zero if a parse error occurred */
    TokenType last_token_type;
    int       last_token_line;
    int       last_token_col;
    ParseStructName *struct_names;
} ParseState;

/* -----------------------------------------------------------------------
 * Predefined constant names
 * ----------------------------------------------------------------------- */
static inline int is_predefined_constant(const char *name) {
    static const char *consts[] = {
        "INT_MAX", "INT_MIN", "INT_SIZE",
        "FLOAT_SIZE", "FLOAT_DIG",
        "FLOAT_EPSILON", "FLOAT_MIN", "FLOAT_MAX",
        "M_PI", "M_E", "M_TAU", "M_SQRT2", "M_LN2", "M_LN10",
        NULL
    };
    for (int i = 0; consts[i]; i++)
        if (strcmp(name, consts[i]) == 0) return 1;
    return 0;
}

/* -----------------------------------------------------------------------
 * token_type_name — forward declaration so the grammar can use it
 * ----------------------------------------------------------------------- */
const char *token_type_name(TokenType t);

/* -----------------------------------------------------------------------
 * Parser entry point (wraps Lemon-generated Parse())
 * ----------------------------------------------------------------------- */
AstNode *parse_program(const char *source);
int parse_state_has_struct(const ParseState *ps, const char *name);
int parse_state_has_type_name(const ParseState *ps, const char *name);
void parse_state_add_struct(ParseState *ps, const char *name);
void parse_state_free(ParseState *ps);

#endif /* CIMPLE_PARSER_HELPER_H */
