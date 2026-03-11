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
}

%token_type   { Token }
%token_prefix TOK_
%default_type { AstNode * }

%extra_argument { ParseState *ps }

%syntax_error {
    (void)yyminor;
    error_syntax(TOKEN.line, TOKEN.col,
                 "unexpected token %s", token_type_name(TOKEN.type));
}

%parse_failure {
    error_syntax(0, 0, "parse failure — the input could not be parsed");
}

/* -----------------------------------------------------------------------
 * Operator precedence (low to high)
 * ----------------------------------------------------------------------- */
%right     ASSIGN.
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
    nodelist_push(&L, D);
}

/* -----------------------------------------------------------------------
 * Top-level declarations
 * ----------------------------------------------------------------------- */
decl(D) ::= scalar_type(T) IDENT(N) SEMICOLON.
{
    D = ast_new(NODE_VAR_DECL, N.line, N.col);
    D->u.var_decl.name      = cimple_strdup(N.v.sval);
    D->u.var_decl.type      = T;
    D->u.var_decl.init      = NULL;
    D->u.var_decl.is_global = 1;
    free(N.v.sval);
}

decl(D) ::= scalar_type(T) IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    D = ast_new(NODE_VAR_DECL, N.line, N.col);
    D->u.var_decl.name      = cimple_strdup(N.v.sval);
    D->u.var_decl.type      = T;
    D->u.var_decl.init      = E;
    D->u.var_decl.is_global = 1;
    free(N.v.sval);
}

decl(D) ::= scalar_type(T) IDENT(N) LPAREN param_list(PL) RPAREN block(B).
{
    D = ast_new(NODE_FUNC_DECL, N.line, N.col);
    D->u.func_decl.name     = cimple_strdup(N.v.sval);
    D->u.func_decl.ret_type = T;
    D->u.func_decl.params   = PL;
    D->u.func_decl.body     = B;
    free(N.v.sval);
}

decl(D) ::= array_type(T) IDENT(N) SEMICOLON.
{
    D = ast_new(NODE_VAR_DECL, N.line, N.col);
    D->u.var_decl.name      = cimple_strdup(N.v.sval);
    D->u.var_decl.type      = T;
    D->u.var_decl.init      = NULL;
    D->u.var_decl.is_global = 1;
    free(N.v.sval);
}

decl(D) ::= array_type(T) IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    D = ast_new(NODE_VAR_DECL, N.line, N.col);
    D->u.var_decl.name      = cimple_strdup(N.v.sval);
    D->u.var_decl.type      = T;
    D->u.var_decl.init      = E;
    D->u.var_decl.is_global = 1;
    free(N.v.sval);
}

decl(D) ::= array_type(T) IDENT(N) LPAREN param_list(PL) RPAREN block(B).
{
    D = ast_new(NODE_FUNC_DECL, N.line, N.col);
    D->u.func_decl.name     = cimple_strdup(N.v.sval);
    D->u.func_decl.ret_type = T;
    D->u.func_decl.params   = PL;
    D->u.func_decl.body     = B;
    free(N.v.sval);
}

decl(D) ::= KW_VOID IDENT(N) LPAREN param_list(PL) RPAREN block(B).
{
    D = ast_new(NODE_FUNC_DECL, N.line, N.col);
    D->u.func_decl.name     = cimple_strdup(N.v.sval);
    D->u.func_decl.ret_type = TYPE_VOID;
    D->u.func_decl.params   = PL;
    D->u.func_decl.body     = B;
    free(N.v.sval);
}

decl(D) ::= KW_EXECRESULT IDENT(N) SEMICOLON.
{
    D = ast_new(NODE_VAR_DECL, N.line, N.col);
    D->u.var_decl.name      = cimple_strdup(N.v.sval);
    D->u.var_decl.type      = TYPE_EXEC_RESULT;
    D->u.var_decl.init      = NULL;
    D->u.var_decl.is_global = 1;
    free(N.v.sval);
}

decl(D) ::= KW_EXECRESULT IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    D = ast_new(NODE_VAR_DECL, N.line, N.col);
    D->u.var_decl.name      = cimple_strdup(N.v.sval);
    D->u.var_decl.type      = TYPE_EXEC_RESULT;
    D->u.var_decl.init      = E;
    D->u.var_decl.is_global = 1;
    free(N.v.sval);
}

decl(D) ::= KW_EXECRESULT IDENT(N) LPAREN param_list(PL) RPAREN block(B).
{
    D = ast_new(NODE_FUNC_DECL, N.line, N.col);
    D->u.func_decl.name     = cimple_strdup(N.v.sval);
    D->u.func_decl.ret_type = TYPE_EXEC_RESULT;
    D->u.func_decl.params   = PL;
    D->u.func_decl.body     = B;
    free(N.v.sval);
}

/* -----------------------------------------------------------------------
 * Types
 * ----------------------------------------------------------------------- */
%type scalar_type  { CimpleType }
%type array_type   { CimpleType }
%type param_type   { CimpleType }

scalar_type(T) ::= KW_INT.    { T = TYPE_INT;    }
scalar_type(T) ::= KW_FLOAT.  { T = TYPE_FLOAT;  }
scalar_type(T) ::= KW_BOOL.   { T = TYPE_BOOL;   }
scalar_type(T) ::= KW_STRING. { T = TYPE_STRING; }

array_type(T) ::= KW_INT    LBRACKET RBRACKET. { T = TYPE_INT_ARR;   }
array_type(T) ::= KW_FLOAT  LBRACKET RBRACKET. { T = TYPE_FLOAT_ARR; }
array_type(T) ::= KW_BOOL   LBRACKET RBRACKET. { T = TYPE_BOOL_ARR;  }
array_type(T) ::= KW_STRING LBRACKET RBRACKET. { T = TYPE_STR_ARR;   }

param_type(T) ::= scalar_type(S).  { T = S; }
param_type(T) ::= array_type(A).   { T = A; }
param_type(T) ::= KW_EXECRESULT.   { T = TYPE_EXEC_RESULT; }

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
    P->u.param.type = T;
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
stmt(S) ::= scalar_type(T) IDENT(N) SEMICOLON.
{
    S = ast_new(NODE_VAR_DECL, N.line, N.col);
    S->u.var_decl.name      = cimple_strdup(N.v.sval);
    S->u.var_decl.type      = T;
    S->u.var_decl.init      = NULL;
    S->u.var_decl.is_global = 0;
    free(N.v.sval);
}

stmt(S) ::= scalar_type(T) IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_VAR_DECL, N.line, N.col);
    S->u.var_decl.name      = cimple_strdup(N.v.sval);
    S->u.var_decl.type      = T;
    S->u.var_decl.init      = E;
    S->u.var_decl.is_global = 0;
    free(N.v.sval);
}

stmt(S) ::= array_type(T) IDENT(N) SEMICOLON.
{
    S = ast_new(NODE_VAR_DECL, N.line, N.col);
    S->u.var_decl.name      = cimple_strdup(N.v.sval);
    S->u.var_decl.type      = T;
    S->u.var_decl.init      = NULL;
    S->u.var_decl.is_global = 0;
    free(N.v.sval);
}

stmt(S) ::= array_type(T) IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_VAR_DECL, N.line, N.col);
    S->u.var_decl.name      = cimple_strdup(N.v.sval);
    S->u.var_decl.type      = T;
    S->u.var_decl.init      = E;
    S->u.var_decl.is_global = 0;
    free(N.v.sval);
}

stmt(S) ::= KW_EXECRESULT IDENT(N) ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_VAR_DECL, N.line, N.col);
    S->u.var_decl.name      = cimple_strdup(N.v.sval);
    S->u.var_decl.type      = TYPE_EXEC_RESULT;
    S->u.var_decl.init      = E;
    S->u.var_decl.is_global = 0;
    free(N.v.sval);
}

stmt(S) ::= KW_EXECRESULT IDENT(N) SEMICOLON.
{
    S = ast_new(NODE_VAR_DECL, N.line, N.col);
    S->u.var_decl.name      = cimple_strdup(N.v.sval);
    S->u.var_decl.type      = TYPE_EXEC_RESULT;
    S->u.var_decl.init      = NULL;
    S->u.var_decl.is_global = 0;
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

/* Index assignment */
stmt(S) ::= IDENT(N) LBRACKET expr(I) RBRACKET ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_INDEX_ASSIGN, N.line, N.col);
    S->u.index_assign.name  = cimple_strdup(N.v.sval);
    S->u.index_assign.index = I;
    S->u.index_assign.value = E;
    free(N.v.sval);
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

simple_stmt(S) ::= IDENT(N) LBRACKET expr(I) RBRACKET ASSIGN expr(E) SEMICOLON.
{
    S = ast_new(NODE_INDEX_ASSIGN, N.line, N.col);
    S->u.index_assign.name  = cimple_strdup(N.v.sval);
    S->u.index_assign.index = I;
    S->u.index_assign.value = E;
    free(N.v.sval);
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

/* Identifier or predefined constant */
expr(E) ::= IDENT(N).
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
%type arg_list  { NodeList  }

call_expr(C) ::= IDENT(N) LPAREN arg_list(AL) RPAREN.
{
    C = ast_call(N.v.sval, AL, N.line, N.col);
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
