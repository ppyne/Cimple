/*
 * Cimple parser — Lemon grammar
 *
 * Processed by: lemon parser.y
 * Generates: parser.c, parser.h
 */

%include {
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "parser_helper.h"
#include "../ast/ast.h"
#include "../common/memory.h"
#include "../common/error.h"

typedef struct {
    CimpleType type;
    char      *struct_name;
} ParsedType;

static ParsedType parsed_type_make(CimpleType type, const char *struct_name) {
    ParsedType pt;
    pt.type = type;
    pt.struct_name = struct_name ? cimple_strdup(struct_name) : NULL;
    return pt;
}

static int token_starts_statement_or_decl(TokenType t) {
    switch (t) {
    case TOK_RETURN:
    case TOK_IF:
    case TOK_WHILE:
    case TOK_FOR:
    case TOK_BREAK:
    case TOK_CONTINUE:
    case TOK_IDENT:
    case TOK_KW_INT:
    case TOK_KW_FLOAT:
    case TOK_KW_BOOL:
    case TOK_KW_STRING:
    case TOK_KW_BYTE:
    case TOK_KW_VOID:
    case TOK_KW_EXECRESULT:
    case TOK_KW_STRUCTURE:
    case TOK_KW_SELF:
    case TOK_KW_SUPER:
    case TOK_KW_CLONE:
        return 1;
    default:
        return 0;
    }
}

static int token_can_end_expression(TokenType t) {
    switch (t) {
    case TOK_IDENT:
    case TOK_INT_LIT:
    case TOK_FLOAT_LIT:
    case TOK_STRING_LIT:
    case TOK_TRUE:
    case TOK_FALSE:
    case TOK_RPAREN:
    case TOK_RBRACKET:
    case TOK_PLUSPLUS:
    case TOK_MINUSMINUS:
    case TOK_KW_SELF:
    case TOK_KW_SUPER:
        return 1;
    default:
        return 0;
    }
}
}

%token_type   { Token }
%token_prefix TOK_
%default_type { AstNode * }

%extra_argument { ParseState *ps }

%syntax_error {
    (void)yyminor;
    if (token_starts_statement_or_decl(TOKEN.type) &&
        token_can_end_expression(ps->last_token_type)) {
        error_syntax(TOKEN.line, TOKEN.col, "Missing ';' after statement");
        return;
    }
    switch (TOKEN.type) {
    case TOK_RBRACE:
        error_syntax(TOKEN.line, TOKEN.col, "Missing ';' after statement");
        break;
    case TOK_LBRACE:
        error_syntax(TOKEN.line, TOKEN.col, "Missing ')' to close expression");
        break;
    case TOK_EOF:
        error_syntax(TOKEN.line, TOKEN.col, "Missing '}' to close block");
        break;
    case TOK_IDENT:
        error_syntax(TOKEN.line, TOKEN.col, "Unknown type: '%s'", TOKEN.v.sval);
        break;
    case TOK_RPAREN:
        error_syntax(TOKEN.line, TOKEN.col, "Expected expression");
        break;
    default:
        error_syntax(TOKEN.line, TOKEN.col,
                     "unexpected token %s", token_type_name(TOKEN.type));
        break;
    }
}

%parse_failure {
    error_syntax(0, 0, "parse failure — the input could not be parsed");
}

/* -----------------------------------------------------------------------
 * Operator precedence (low to high)
 * ----------------------------------------------------------------------- */
%right     ASSIGN PLUSEQ MINUSEQ STAREQ SLASHEQ PERCENTEQ.
%right     QUESTION.
%right     ELSE.
%left      OR.
%left      AND.
%left      BOR.
%left      BXOR.
%left      BAND.
%left      EQ NEQ.
%left      LT LEQ GT GEQ.
%left      LSHIFT RSHIFT.
%left      PLUS MINUS.
%left      STAR SLASH PERCENT.
%right     UMINUS NOT BNOT.
%left      DOT.
%nonassoc  LBRACKET.

/* -----------------------------------------------------------------------
 * Program — top level
 * ----------------------------------------------------------------------- */
%type program { AstNode * }

program(P) ::= decl_list(L).
{
    P = ast_new(NODE_PROGRAM, 1, 1);
    P->u.program.decls = L;
    ps->root = P;
}

%type decl_list { NodeList }
%type decl { AstNode * }

decl_list(L) ::= .
{
    nodelist_init(&L);
}

decl_list(L) ::= decl_list(LL) decl(D).
{
    L = LL;
    if (D) nodelist_push(&L, D);
}

/* -----------------------------------------------------------------------
 * Top-level declarations
 * ----------------------------------------------------------------------- */
decl(D) ::= nonvoid_type(T) IDENT(N) SEMICOLON.
{
    D = ast_new(NODE_VAR_DECL, N.line, N.col);
    D->u.var_decl.name        = cimple_strdup(N.v.sval);
    D->u.var_decl.type        = T.type;
    D->u.var_decl.struct_name = T.struct_name;
    D->u.var_decl.init        = NULL;
    D->u.var_decl.is_global   = 1;
    D->type_name_hint         = T.struct_name ? cimple_strdup(T.struct_name) : NULL;
    free(N.v.sval);
}

decl(D) ::= nonvoid_type(T) IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    D = ast_new(NODE_VAR_DECL, N.line, N.col);
    D->u.var_decl.name        = cimple_strdup(N.v.sval);
    D->u.var_decl.type        = T.type;
    D->u.var_decl.struct_name = T.struct_name;
    D->u.var_decl.init        = E;
    D->u.var_decl.is_global   = 1;
    D->type_name_hint         = T.struct_name ? cimple_strdup(T.struct_name) : NULL;
    free(N.v.sval);
}

decl(D) ::= nonvoid_type(T) IDENT(N) LPAREN param_list(PL) RPAREN block(B).
{
    D = ast_new(NODE_FUNC_DECL, N.line, N.col);
    D->u.func_decl.name            = cimple_strdup(N.v.sval);
    D->u.func_decl.ret_type        = T.type;
    D->u.func_decl.ret_struct_name = T.struct_name;
    D->u.func_decl.params          = PL;
    D->u.func_decl.body            = B;
    D->type_name_hint              = T.struct_name ? cimple_strdup(T.struct_name) : NULL;
    free(N.v.sval);
}

decl(D) ::= KW_VOID IDENT(N) LPAREN param_list(PL) RPAREN block(B).
{
    D = ast_new(NODE_FUNC_DECL, N.line, N.col);
    D->u.func_decl.name = cimple_strdup(N.v.sval);
    D->u.func_decl.ret_type = TYPE_VOID;
    D->u.func_decl.ret_struct_name = NULL;
    D->u.func_decl.params = PL;
    D->u.func_decl.body = B;
    free(N.v.sval);
}

decl(D) ::= structure_decl(SD).
{
    D = SD;
}

decl(D) ::= KW_IMPORT STRING_LIT(S) SEMICOLON.
{
    D = NULL;
    free(S.v.sval);
}

/* -----------------------------------------------------------------------
 * Structures
 * ----------------------------------------------------------------------- */
%type structure_decl { AstNode * }
%type structure_header { AstNode * }
%type structure_member_list { NodeList }
%type structure_member { AstNode * }

structure_header(H) ::= KW_STRUCTURE(T) IDENT(N) LBRACE.
{
    H = ast_new(NODE_STRUCT_DECL, T.line, T.col);
    H->u.struct_decl.name = cimple_strdup(N.v.sval);
    H->u.struct_decl.base_name = NULL;
    nodelist_init(&H->u.struct_decl.members);
    parse_state_add_struct(ps, N.v.sval);
    free(N.v.sval);
}

structure_header(H) ::= KW_STRUCTURE(T) IDENT(N) COLON TYPE_IDENT(B) LBRACE.
{
    H = ast_new(NODE_STRUCT_DECL, T.line, T.col);
    H->u.struct_decl.name = cimple_strdup(N.v.sval);
    H->u.struct_decl.base_name = cimple_strdup(B.v.sval);
    nodelist_init(&H->u.struct_decl.members);
    parse_state_add_struct(ps, N.v.sval);
    free(N.v.sval);
    free(B.v.sval);
}

structure_decl(D) ::= structure_header(H) structure_member_list(M) RBRACE.
{
    D = H;
    D->u.struct_decl.members = M;
}

structure_member_list(L) ::= .
{
    nodelist_init(&L);
}

structure_member_list(L) ::= structure_member_list(LL) structure_member(M).
{
    L = LL;
    nodelist_push(&L, M);
}

structure_member(M) ::= nonvoid_type(T) IDENT(N) SEMICOLON.
{
    M = ast_new(NODE_STRUCT_FIELD, N.line, N.col);
    M->u.struct_field.name = cimple_strdup(N.v.sval);
    M->u.struct_field.type = T.type;
    M->u.struct_field.struct_name = T.struct_name;
    M->u.struct_field.init = NULL;
    M->type_name_hint = T.struct_name ? cimple_strdup(T.struct_name) : NULL;
    free(N.v.sval);
}

structure_member(M) ::= nonvoid_type(T) IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    M = ast_new(NODE_STRUCT_FIELD, N.line, N.col);
    M->u.struct_field.name = cimple_strdup(N.v.sval);
    M->u.struct_field.type = T.type;
    M->u.struct_field.struct_name = T.struct_name;
    M->u.struct_field.init = E;
    M->type_name_hint = T.struct_name ? cimple_strdup(T.struct_name) : NULL;
    free(N.v.sval);
}

structure_member(M) ::= nonvoid_type(T) IDENT(N) LPAREN param_list(PL) RPAREN block(B).
{
    M = ast_new(NODE_FUNC_DECL, N.line, N.col);
    M->u.func_decl.name = cimple_strdup(N.v.sval);
    M->u.func_decl.ret_type = T.type;
    M->u.func_decl.ret_struct_name = T.struct_name;
    M->u.func_decl.params = PL;
    M->u.func_decl.body = B;
    M->type_name_hint = T.struct_name ? cimple_strdup(T.struct_name) : NULL;
    free(N.v.sval);
}

structure_member(M) ::= KW_VOID IDENT(N) LPAREN param_list(PL) RPAREN block(B).
{
    M = ast_new(NODE_FUNC_DECL, N.line, N.col);
    M->u.func_decl.name = cimple_strdup(N.v.sval);
    M->u.func_decl.ret_type = TYPE_VOID;
    M->u.func_decl.ret_struct_name = NULL;
    M->u.func_decl.params = PL;
    M->u.func_decl.body = B;
    free(N.v.sval);
}

/* -----------------------------------------------------------------------
 * Types
 * ----------------------------------------------------------------------- */
%type scalar_type  { ParsedType }
%type array_type   { ParsedType }
%type struct_type  { ParsedType }
%type struct_array_type { ParsedType }
%type nonvoid_type { ParsedType }
%type param_type   { ParsedType }

scalar_type(T) ::= KW_INT.    { T = parsed_type_make(TYPE_INT, NULL); }
scalar_type(T) ::= KW_FLOAT.  { T = parsed_type_make(TYPE_FLOAT, NULL); }
scalar_type(T) ::= KW_BOOL.   { T = parsed_type_make(TYPE_BOOL, NULL); }
scalar_type(T) ::= KW_STRING. { T = parsed_type_make(TYPE_STRING, NULL); }
scalar_type(T) ::= KW_BYTE.   { T = parsed_type_make(TYPE_BYTE, NULL); }

array_type(T) ::= KW_INT    LBRACKET RBRACKET. { T = parsed_type_make(TYPE_INT_ARR, NULL); }
array_type(T) ::= KW_FLOAT  LBRACKET RBRACKET. { T = parsed_type_make(TYPE_FLOAT_ARR, NULL); }
array_type(T) ::= KW_BOOL   LBRACKET RBRACKET. { T = parsed_type_make(TYPE_BOOL_ARR, NULL); }
array_type(T) ::= KW_STRING LBRACKET RBRACKET. { T = parsed_type_make(TYPE_STR_ARR, NULL); }
array_type(T) ::= KW_BYTE   LBRACKET RBRACKET. { T = parsed_type_make(TYPE_BYTE_ARR, NULL); }

struct_type(T) ::= TYPE_IDENT(N).
{
    T = parsed_type_make(TYPE_STRUCT, N.v.sval);
    free(N.v.sval);
}

struct_array_type(T) ::= TYPE_IDENT(N) LBRACKET RBRACKET.
{
    T = parsed_type_make(TYPE_STRUCT_ARR, N.v.sval);
    free(N.v.sval);
}

nonvoid_type(T) ::= scalar_type(S).       { T = S; }
nonvoid_type(T) ::= array_type(A).        { T = A; }
nonvoid_type(T) ::= struct_type(S).       { T = S; }
nonvoid_type(T) ::= struct_array_type(A). { T = A; }
nonvoid_type(T) ::= KW_EXECRESULT.        { T = parsed_type_make(TYPE_EXEC_RESULT, NULL); }

param_type(T) ::= nonvoid_type(S).  { T = S; }

/* -----------------------------------------------------------------------
 * Parameter list
 * ----------------------------------------------------------------------- */
%type param_list   { NodeList }
%type param        { AstNode * }

param_list(L) ::= .
{
    nodelist_init(&L);
}

param_list(L) ::= param(P).
{
    nodelist_init(&L);
    nodelist_push(&L, P);
}

param_list(L) ::= param_list(LL) COMMA param(P).
{
    L = LL;
    nodelist_push(&L, P);
}

param(P) ::= param_type(T) IDENT(N).
{
    P = ast_new(NODE_PARAM, N.line, N.col);
    P->u.param.name = cimple_strdup(N.v.sval);
    P->u.param.type = T.type;
    P->u.param.struct_name = T.struct_name;
    P->type_name_hint = T.struct_name ? cimple_strdup(T.struct_name) : NULL;
    free(N.v.sval);
}

/* -----------------------------------------------------------------------
 * Block
 * ----------------------------------------------------------------------- */
%type block { AstNode * }

block(B) ::= LBRACE stmt_list(L) RBRACE.
{
    /* Use the line from the opening brace by using a temp token */
    B = ast_new(NODE_BLOCK, 0, 0);
    B->u.block.stmts = L;
}

%type stmt_list { NodeList }

stmt_list(L) ::= .
{
    nodelist_init(&L);
}

stmt_list(L) ::= stmt_list(LL) stmt(S).
{
    L = LL;
    if (S) nodelist_push(&L, S);
}

/* -----------------------------------------------------------------------
 * Statements
 * ----------------------------------------------------------------------- */
%type stmt { AstNode * }

/* Variable declarations */
stmt(S) ::= nonvoid_type(T) IDENT(N) SEMICOLON.
{
    S = ast_new(NODE_VAR_DECL, N.line, N.col);
    S->u.var_decl.name        = cimple_strdup(N.v.sval);
    S->u.var_decl.type        = T.type;
    S->u.var_decl.struct_name = T.struct_name;
    S->u.var_decl.init        = NULL;
    S->u.var_decl.is_global   = 0;
    S->type_name_hint         = T.struct_name ? cimple_strdup(T.struct_name) : NULL;
    free(N.v.sval);
}

stmt(S) ::= nonvoid_type(T) IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_VAR_DECL, N.line, N.col);
    S->u.var_decl.name        = cimple_strdup(N.v.sval);
    S->u.var_decl.type        = T.type;
    S->u.var_decl.struct_name = T.struct_name;
    S->u.var_decl.init        = E;
    S->u.var_decl.is_global   = 0;
    S->type_name_hint         = T.struct_name ? cimple_strdup(T.struct_name) : NULL;
    free(N.v.sval);
}

/* Assignment */
stmt(S) ::= IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_ASSIGN, N.line, N.col);
    S->u.assign.name  = cimple_strdup(N.v.sval);
    S->u.assign.value = E;
    free(N.v.sval);
}

/* Compound assignment */
stmt(S) ::= IDENT(N) PLUSEQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_ADD;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

stmt(S) ::= IDENT(N) MINUSEQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_SUB;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

stmt(S) ::= IDENT(N) STAREQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_MUL;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

stmt(S) ::= IDENT(N) SLASHEQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_DIV;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

stmt(S) ::= IDENT(N) PERCENTEQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_MOD;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

/* Index assignment */
stmt(S) ::= IDENT(N) LBRACKET expr(I) RBRACKET ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_INDEX_ASSIGN, N.line, N.col);
    S->u.index_assign.name  = cimple_strdup(N.v.sval);
    S->u.index_assign.index = I;
    S->u.index_assign.value = E;
    free(N.v.sval);
}

stmt(S) ::= IDENT(N) LBRACKET expr(I) RBRACKET DOT IDENT(F) ASSIGN expr(E) SEMICOLON.
{
    AstNode *base = ast_ident(N.v.sval, N.line, N.col);
    AstNode *idx = ast_new(NODE_INDEX, N.line, N.col);
    idx->u.index.base = base;
    idx->u.index.index = I;
    AstNode *member = ast_member(idx, F.v.sval, F.line, F.col);
    S = ast_new(NODE_MEMBER_ASSIGN, F.line, F.col);
    S->u.member_assign.target = member;
    S->u.member_assign.value = E;
    free(N.v.sval);
    free(F.v.sval);
}

stmt(S) ::= member_expr(M) ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_MEMBER_ASSIGN, M->line, M->col);
    S->u.member_assign.target = M;
    S->u.member_assign.value = E;
}

/* Increment / decrement as statements */
stmt(S) ::= IDENT(N) PLUSPLUS SEMICOLON.
{
    S = ast_new(NODE_INCR, N.line, N.col);
    S->u.incr_decr.name = cimple_strdup(N.v.sval);
    free(N.v.sval);
}

stmt(S) ::= IDENT(N) MINUSMINUS SEMICOLON.
{
    S = ast_new(NODE_DECR, N.line, N.col);
    S->u.incr_decr.name = cimple_strdup(N.v.sval);
    free(N.v.sval);
}

/* Expression statement (function call only) */
stmt(S) ::= call_expr(C) SEMICOLON.
{
    S = ast_new(NODE_EXPR_STMT, C->line, C->col);
    S->u.expr_stmt.expr = C;
}

stmt(S) ::= method_call_expr(C) SEMICOLON.
{
    S = ast_new(NODE_EXPR_STMT, C->line, C->col);
    S->u.expr_stmt.expr = C;
}

stmt(S) ::= IDENT(N) LBRACKET expr(I) RBRACKET DOT IDENT(M) LPAREN arg_list(AL) RPAREN SEMICOLON.
{
    AstNode *base = ast_ident(N.v.sval, N.line, N.col);
    AstNode *idx = ast_new(NODE_INDEX, N.line, N.col);
    idx->u.index.base = base;
    idx->u.index.index = I;
    AstNode *call = ast_method_call(idx, M.v.sval, AL, 0, M.line, M.col);
    S = ast_new(NODE_EXPR_STMT, call->line, call->col);
    S->u.expr_stmt.expr = call;
    free(N.v.sval);
    free(M.v.sval);
}

/* if */
stmt(S) ::= IF(T) LPAREN expr(C) RPAREN simple_or_block(TH). [ELSE]
{
    S = ast_new(NODE_IF, T.line, T.col);
    S->u.if_stmt.cond        = C;
    S->u.if_stmt.then_branch = TH;
    S->u.if_stmt.else_branch = NULL;
}

stmt(S) ::= IF(T) LPAREN expr(C) RPAREN simple_or_block(TH) ELSE simple_or_block(EL).
{
    S = ast_new(NODE_IF, T.line, T.col);
    S->u.if_stmt.cond        = C;
    S->u.if_stmt.then_branch = TH;
    S->u.if_stmt.else_branch = EL;
}

/* while */
stmt(S) ::= WHILE(T) LPAREN expr(C) RPAREN simple_or_block(B).
{
    S = ast_new(NODE_WHILE, T.line, T.col);
    S->u.while_stmt.cond = C;
    S->u.while_stmt.body = B;
}

/* for */
stmt(S) ::= FOR(T) LPAREN for_init(I) expr(C) SEMICOLON for_update(U) RPAREN simple_or_block(B).
{
    S = ast_new(NODE_FOR, T.line, T.col);
    S->u.for_stmt.init   = I;
    S->u.for_stmt.cond   = C;
    S->u.for_stmt.update = U;
    S->u.for_stmt.body   = B;
}

/* return */
stmt(S) ::= RETURN(T) SEMICOLON.
{
    S = ast_new(NODE_RETURN, T.line, T.col);
    S->u.ret.value = NULL;
}

stmt(S) ::= RETURN(T) expr(E) SEMICOLON.
{
    S = ast_new(NODE_RETURN, T.line, T.col);
    S->u.ret.value = E;
}

/* break / continue */
stmt(S) ::= BREAK(T) SEMICOLON.
{
    S = ast_new(NODE_BREAK, T.line, T.col);
}

stmt(S) ::= CONTINUE(T) SEMICOLON.
{
    S = ast_new(NODE_CONTINUE, T.line, T.col);
}

/* block as statement */
stmt(S) ::= block(B). { S = B; }

/* -----------------------------------------------------------------------
 * simple_or_block: a block { } or a single non-declaration statement
 * (used as body for if/while/for without braces)
 * ----------------------------------------------------------------------- */
%type simple_or_block { AstNode * }

simple_or_block(B) ::= block(BL).               { B = BL; }
simple_or_block(B) ::= simple_stmt(S).           { B = S;  }

%type simple_stmt { AstNode * }

simple_stmt(S) ::= IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_ASSIGN, N.line, N.col);
    S->u.assign.name  = cimple_strdup(N.v.sval);
    S->u.assign.value = E;
    free(N.v.sval);
}

simple_stmt(S) ::= IDENT(N) PLUSEQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_ADD;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

simple_stmt(S) ::= IDENT(N) MINUSEQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_SUB;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

simple_stmt(S) ::= IDENT(N) STAREQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_MUL;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

simple_stmt(S) ::= IDENT(N) SLASHEQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_DIV;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

simple_stmt(S) ::= IDENT(N) PERCENTEQ expr(E) SEMICOLON.
{
    S = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    S->u.compound_assign.name  = cimple_strdup(N.v.sval);
    S->u.compound_assign.op    = OP_MOD;
    S->u.compound_assign.value = E;
    free(N.v.sval);
}

simple_stmt(S) ::= IDENT(N) LBRACKET expr(I) RBRACKET ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_INDEX_ASSIGN, N.line, N.col);
    S->u.index_assign.name  = cimple_strdup(N.v.sval);
    S->u.index_assign.index = I;
    S->u.index_assign.value = E;
    free(N.v.sval);
}

simple_stmt(S) ::= IDENT(N) LBRACKET expr(I) RBRACKET DOT IDENT(F) ASSIGN expr(E) SEMICOLON.
{
    AstNode *base = ast_ident(N.v.sval, N.line, N.col);
    AstNode *idx = ast_new(NODE_INDEX, N.line, N.col);
    idx->u.index.base = base;
    idx->u.index.index = I;
    AstNode *member = ast_member(idx, F.v.sval, F.line, F.col);
    S = ast_new(NODE_MEMBER_ASSIGN, F.line, F.col);
    S->u.member_assign.target = member;
    S->u.member_assign.value = E;
    free(N.v.sval);
    free(F.v.sval);
}

simple_stmt(S) ::= member_expr(M) ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_MEMBER_ASSIGN, M->line, M->col);
    S->u.member_assign.target = M;
    S->u.member_assign.value = E;
}

simple_stmt(S) ::= IDENT(N) PLUSPLUS SEMICOLON.
{
    S = ast_new(NODE_INCR, N.line, N.col);
    S->u.incr_decr.name = cimple_strdup(N.v.sval);
    free(N.v.sval);
}

simple_stmt(S) ::= IDENT(N) MINUSMINUS SEMICOLON.
{
    S = ast_new(NODE_DECR, N.line, N.col);
    S->u.incr_decr.name = cimple_strdup(N.v.sval);
    free(N.v.sval);
}

simple_stmt(S) ::= call_expr(C) SEMICOLON.
{
    S = ast_new(NODE_EXPR_STMT, C->line, C->col);
    S->u.expr_stmt.expr = C;
}

simple_stmt(S) ::= method_call_expr(C) SEMICOLON.
{
    S = ast_new(NODE_EXPR_STMT, C->line, C->col);
    S->u.expr_stmt.expr = C;
}

simple_stmt(S) ::= IDENT(N) LBRACKET expr(I) RBRACKET DOT IDENT(M) LPAREN arg_list(AL) RPAREN SEMICOLON.
{
    AstNode *base = ast_ident(N.v.sval, N.line, N.col);
    AstNode *idx = ast_new(NODE_INDEX, N.line, N.col);
    idx->u.index.base = base;
    idx->u.index.index = I;
    AstNode *call = ast_method_call(idx, M.v.sval, AL, 0, M.line, M.col);
    S = ast_new(NODE_EXPR_STMT, call->line, call->col);
    S->u.expr_stmt.expr = call;
    free(N.v.sval);
    free(M.v.sval);
}

simple_stmt(S) ::= RETURN(T) SEMICOLON.
{
    S = ast_new(NODE_RETURN, T.line, T.col);
    S->u.ret.value = NULL;
}

simple_stmt(S) ::= RETURN(T) expr(E) SEMICOLON.
{
    S = ast_new(NODE_RETURN, T.line, T.col);
    S->u.ret.value = E;
}

simple_stmt(S) ::= BREAK(T) SEMICOLON.
{
    S = ast_new(NODE_BREAK, T.line, T.col);
}

simple_stmt(S) ::= CONTINUE(T) SEMICOLON.
{
    S = ast_new(NODE_CONTINUE, T.line, T.col);
}

simple_stmt(S) ::= IF(T) LPAREN expr(C) RPAREN simple_or_block(TH). [ELSE]
{
    S = ast_new(NODE_IF, T.line, T.col);
    S->u.if_stmt.cond        = C;
    S->u.if_stmt.then_branch = TH;
    S->u.if_stmt.else_branch = NULL;
}

simple_stmt(S) ::= IF(T) LPAREN expr(C) RPAREN simple_or_block(TH) ELSE simple_or_block(EL).
{
    S = ast_new(NODE_IF, T.line, T.col);
    S->u.if_stmt.cond        = C;
    S->u.if_stmt.then_branch = TH;
    S->u.if_stmt.else_branch = EL;
}

simple_stmt(S) ::= WHILE(T) LPAREN expr(C) RPAREN simple_or_block(B).
{
    S = ast_new(NODE_WHILE, T.line, T.col);
    S->u.while_stmt.cond = C;
    S->u.while_stmt.body = B;
}

simple_stmt(S) ::= FOR(T) LPAREN for_init(I) expr(C) SEMICOLON for_update(U) RPAREN simple_or_block(B).
{
    S = ast_new(NODE_FOR, T.line, T.col);
    S->u.for_stmt.init   = I;
    S->u.for_stmt.cond   = C;
    S->u.for_stmt.update = U;
    S->u.for_stmt.body   = B;
}

/* -----------------------------------------------------------------------
 * for-loop init (int i = expr;)
 * ----------------------------------------------------------------------- */
%type for_init  { AstNode * }
%type for_update { AstNode * }

for_init(I) ::= KW_INT IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    I = ast_new(NODE_FOR_INIT, N.line, N.col);
    I->u.for_init.name      = cimple_strdup(N.v.sval);
    I->u.for_init.init_expr = E;
    free(N.v.sval);
}

for_init(I) ::= SEMICOLON.
{
    I = NULL;
}

for_update(U) ::= IDENT(N) ASSIGN expr(E).
{
    U = ast_new(NODE_ASSIGN, N.line, N.col);
    U->u.assign.name  = cimple_strdup(N.v.sval);
    U->u.assign.value = E;
    free(N.v.sval);
}

for_update(U) ::= IDENT(N) PLUSPLUS.
{
    U = ast_new(NODE_INCR, N.line, N.col);
    U->u.incr_decr.name = cimple_strdup(N.v.sval);
    free(N.v.sval);
}

for_update(U) ::= IDENT(N) MINUSMINUS.
{
    U = ast_new(NODE_DECR, N.line, N.col);
    U->u.incr_decr.name = cimple_strdup(N.v.sval);
    free(N.v.sval);
}

for_update(U) ::= IDENT(N) PLUSEQ expr(E).
{
    U = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    U->u.compound_assign.name  = cimple_strdup(N.v.sval);
    U->u.compound_assign.op    = OP_ADD;
    U->u.compound_assign.value = E;
    free(N.v.sval);
}

for_update(U) ::= IDENT(N) MINUSEQ expr(E).
{
    U = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    U->u.compound_assign.name  = cimple_strdup(N.v.sval);
    U->u.compound_assign.op    = OP_SUB;
    U->u.compound_assign.value = E;
    free(N.v.sval);
}

for_update(U) ::= IDENT(N) STAREQ expr(E).
{
    U = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    U->u.compound_assign.name  = cimple_strdup(N.v.sval);
    U->u.compound_assign.op    = OP_MUL;
    U->u.compound_assign.value = E;
    free(N.v.sval);
}

for_update(U) ::= IDENT(N) SLASHEQ expr(E).
{
    U = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    U->u.compound_assign.name  = cimple_strdup(N.v.sval);
    U->u.compound_assign.op    = OP_DIV;
    U->u.compound_assign.value = E;
    free(N.v.sval);
}

for_update(U) ::= IDENT(N) PERCENTEQ expr(E).
{
    U = ast_new(NODE_COMPOUND_ASSIGN, N.line, N.col);
    U->u.compound_assign.name  = cimple_strdup(N.v.sval);
    U->u.compound_assign.op    = OP_MOD;
    U->u.compound_assign.value = E;
    free(N.v.sval);
}

for_update(U) ::= .
{
    U = NULL;
}

/* -----------------------------------------------------------------------
 * Array literal
 * ----------------------------------------------------------------------- */
%type array_lit  { AstNode * }
%type elem_list  { NodeList  }

array_lit(A) ::= LBRACKET(T) RBRACKET.
{
    A = ast_new(NODE_ARRAY_LIT, T.line, T.col);
    nodelist_init(&A->u.array_lit.elems);
    A->u.array_lit.elem_type = TYPE_UNKNOWN;
}

array_lit(A) ::= LBRACKET(T) elem_list(L) RBRACKET.
{
    A = ast_new(NODE_ARRAY_LIT, T.line, T.col);
    A->u.array_lit.elems     = L;
    A->u.array_lit.elem_type = TYPE_UNKNOWN;
}

elem_list(L) ::= expr(E).
{
    nodelist_init(&L);
    nodelist_push(&L, E);
}

elem_list(L) ::= elem_list(LL) COMMA expr(E).
{
    L = LL;
    nodelist_push(&L, E);
}

/* -----------------------------------------------------------------------
 * Expressions
 * ----------------------------------------------------------------------- */
%type expr { AstNode * }

/* Literals */
expr(E) ::= INT_LIT(T).
{
    E = ast_int_lit(T.v.ival, T.line, T.col);
}

expr(E) ::= FLOAT_LIT(T).
{
    E = ast_float_lit(T.v.fval, T.line, T.col);
}

expr(E) ::= TRUE(T).
{
    E = ast_bool_lit(1, T.line, T.col);
}

expr(E) ::= FALSE(T).
{
    E = ast_bool_lit(0, T.line, T.col);
}

expr(E) ::= STRING_LIT(T).
{
    E = ast_string_lit(T.v.sval, T.line, T.col);
    free(T.v.sval);
}

expr(E) ::= array_lit(A). { E = A; }
expr(E) ::= KW_SELF(T).  { E = ast_self(T.line, T.col); }
expr(E) ::= KW_SUPER(T). { E = ast_super(T.line, T.col); }
expr(E) ::= KW_CLONE(T) TYPE_IDENT(N).
{
    E = ast_clone(N.v.sval, T.line, T.col);
    free(N.v.sval);
}

expr(E) ::= KW_CLONE(T) IDENT(N).
{
    E = ast_clone(N.v.sval, T.line, T.col);
    free(N.v.sval);
}

/* Identifier or predefined constant */
expr(E) ::= IDENT(N). [PLUS]
{
    if (is_predefined_constant(N.v.sval)) {
        E = ast_const(N.v.sval, N.line, N.col);
    } else {
        E = ast_ident(N.v.sval, N.line, N.col);
    }
    free(N.v.sval);
}

/* Parenthesised */
expr(E) ::= LPAREN expr(X) RPAREN. { E = X; }

/* Binary operators */
expr(E) ::= expr(L) PLUS(T)    expr(R). { E = ast_binop(OP_ADD, L, R, T.line, T.col); }
expr(E) ::= expr(L) MINUS(T)   expr(R). { E = ast_binop(OP_SUB, L, R, T.line, T.col); }
expr(E) ::= expr(L) STAR(T)    expr(R). { E = ast_binop(OP_MUL, L, R, T.line, T.col); }
expr(E) ::= expr(L) SLASH(T)   expr(R). { E = ast_binop(OP_DIV, L, R, T.line, T.col); }
expr(E) ::= expr(L) PERCENT(T) expr(R). { E = ast_binop(OP_MOD, L, R, T.line, T.col); }
expr(E) ::= expr(L) EQ(T)      expr(R). { E = ast_binop(OP_EQ,  L, R, T.line, T.col); }
expr(E) ::= expr(L) NEQ(T)     expr(R). { E = ast_binop(OP_NEQ, L, R, T.line, T.col); }
expr(E) ::= expr(L) LT(T)      expr(R). { E = ast_binop(OP_LT,  L, R, T.line, T.col); }
expr(E) ::= expr(L) LEQ(T)     expr(R). { E = ast_binop(OP_LEQ, L, R, T.line, T.col); }
expr(E) ::= expr(L) GT(T)      expr(R). { E = ast_binop(OP_GT,  L, R, T.line, T.col); }
expr(E) ::= expr(L) GEQ(T)     expr(R). { E = ast_binop(OP_GEQ, L, R, T.line, T.col); }
expr(E) ::= expr(L) AND(T)     expr(R). { E = ast_binop(OP_AND, L, R, T.line, T.col); }
expr(E) ::= expr(L) OR(T)      expr(R). { E = ast_binop(OP_OR,  L, R, T.line, T.col); }
expr(E) ::= expr(L) BAND(T)    expr(R). { E = ast_binop(OP_BAND, L, R, T.line, T.col); }
expr(E) ::= expr(L) BOR(T)     expr(R). { E = ast_binop(OP_BOR,  L, R, T.line, T.col); }
expr(E) ::= expr(L) BXOR(T)    expr(R). { E = ast_binop(OP_BXOR, L, R, T.line, T.col); }
expr(E) ::= expr(L) LSHIFT(T)  expr(R). { E = ast_binop(OP_LSHIFT, L, R, T.line, T.col); }
expr(E) ::= expr(L) RSHIFT(T)  expr(R). { E = ast_binop(OP_RSHIFT, L, R, T.line, T.col); }

/* Unary operators */
expr(E) ::= MINUS(T) expr(X). [UMINUS]
{
    E = ast_unop(OP_NEG, X, T.line, T.col);
}

expr(E) ::= NOT(T) expr(X).
{
    E = ast_unop(OP_NOT, X, T.line, T.col);
}

expr(E) ::= BNOT(T) expr(X).
{
    E = ast_unop(OP_BNOT, X, T.line, T.col);
}

/* Function call */
expr(E) ::= call_expr(C). { E = C; }
expr(E) ::= member_expr(M). { E = M; }
expr(E) ::= method_call_expr(C). { E = C; }

/* Ternary operator */
expr(E) ::= expr(C) QUESTION expr(T) COLON expr(F). [QUESTION]
{
    E = ast_new(NODE_TERNARY, C->line, C->col);
    E->u.ternary.cond      = C;
    E->u.ternary.then_expr = T;
    E->u.ternary.else_expr = F;
}

/* Array/string indexing */
expr(E) ::= expr(B) LBRACKET expr(I) RBRACKET.
{
    E = ast_new(NODE_INDEX, B->line, B->col);
    E->u.index.base  = B;
    E->u.index.index = I;
}

/* -----------------------------------------------------------------------
 * Function call expression (separate to allow as statement)
 * ----------------------------------------------------------------------- */
%type call_expr { AstNode * }
%type method_call_expr { AstNode * }
%type member_expr { AstNode * }
%type arg_list  { NodeList  }

call_expr(C) ::= IDENT(N) LPAREN arg_list(AL) RPAREN.
{
    C = ast_call(N.v.sval, AL, N.line, N.col);
    free(N.v.sval);
}

member_expr(M) ::= expr(B) DOT IDENT(N).
{
    M = ast_member(B, N.v.sval, N.line, N.col);
    free(N.v.sval);
}

method_call_expr(C) ::= expr(B) DOT IDENT(N) LPAREN arg_list(AL) RPAREN.
{
    C = ast_method_call(B, N.v.sval, AL, B->kind == NODE_SUPER, N.line, N.col);
    free(N.v.sval);
}

arg_list(L) ::= .
{
    nodelist_init(&L);
}

arg_list(L) ::= expr(E).
{
    nodelist_init(&L);
    nodelist_push(&L, E);
}

arg_list(L) ::= arg_list(LL) COMMA expr(E).
{
    L = LL;
    nodelist_push(&L, E);
}
