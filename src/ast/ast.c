#include "ast.h"
#include "../common/memory.h"
#include <stdlib.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * Type name
 * ----------------------------------------------------------------------- */
const char *type_name(CimpleType t) {
    switch (t) {
    case TYPE_INT:         return "int";
    case TYPE_FLOAT:       return "float";
    case TYPE_BOOL:        return "bool";
    case TYPE_STRING:      return "string";
    case TYPE_BYTE:        return "byte";
    case TYPE_VOID:        return "void";
    case TYPE_INT_ARR:     return "int[]";
    case TYPE_FLOAT_ARR:   return "float[]";
    case TYPE_BOOL_ARR:    return "bool[]";
    case TYPE_STR_ARR:     return "string[]";
    case TYPE_BYTE_ARR:    return "byte[]";
    case TYPE_EXEC_RESULT: return "ExecResult";
    default:               return "<unknown>";
    }
}

/* -----------------------------------------------------------------------
 * NodeList
 * ----------------------------------------------------------------------- */
void nodelist_init(NodeList *l) {
    l->items = NULL;
    l->count = 0;
    l->cap   = 0;
}

void nodelist_push(NodeList *l, AstNode *n) {
    if (l->count == l->cap) {
        l->cap   = l->cap ? l->cap * 2 : 4;
        l->items = (AstNode **)cimple_realloc(l->items,
                                               (size_t)l->cap * sizeof(AstNode *));
    }
    l->items[l->count++] = n;
}

void nodelist_free(NodeList *l) {
    for (int i = 0; i < l->count; i++)
        ast_free(l->items[i]);
    free(l->items);
    l->items = NULL;
    l->count = 0;
    l->cap   = 0;
}

/* -----------------------------------------------------------------------
 * Generic node allocation
 * ----------------------------------------------------------------------- */
AstNode *ast_new(NodeKind kind, int line, int col) {
    AstNode *n = ALLOC(AstNode);
    n->kind    = kind;
    n->line    = line;
    n->col     = col;
    n->type    = TYPE_UNKNOWN;
    return n;
}

/* -----------------------------------------------------------------------
 * Recursive free
 * ----------------------------------------------------------------------- */
void ast_free(AstNode *n) {
    if (!n) return;
    switch (n->kind) {
    case NODE_PROGRAM:
        nodelist_free(&n->u.program.decls);
        break;
    case NODE_FUNC_DECL:
        free(n->u.func_decl.name);
        nodelist_free(&n->u.func_decl.params);
        ast_free(n->u.func_decl.body);
        break;
    case NODE_PARAM:
        free(n->u.param.name);
        break;
    case NODE_VAR_DECL:
        free(n->u.var_decl.name);
        ast_free(n->u.var_decl.init);
        break;
    case NODE_BLOCK:
        nodelist_free(&n->u.block.stmts);
        break;
    case NODE_ASSIGN:
        free(n->u.assign.name);
        ast_free(n->u.assign.value);
        break;
    case NODE_INDEX_ASSIGN:
        free(n->u.index_assign.name);
        ast_free(n->u.index_assign.index);
        ast_free(n->u.index_assign.value);
        break;
    case NODE_IF:
        ast_free(n->u.if_stmt.cond);
        ast_free(n->u.if_stmt.then_branch);
        ast_free(n->u.if_stmt.else_branch);
        break;
    case NODE_WHILE:
        ast_free(n->u.while_stmt.cond);
        ast_free(n->u.while_stmt.body);
        break;
    case NODE_FOR:
        ast_free(n->u.for_stmt.init);
        ast_free(n->u.for_stmt.cond);
        ast_free(n->u.for_stmt.update);
        ast_free(n->u.for_stmt.body);
        break;
    case NODE_FOR_INIT:
        free(n->u.for_init.name);
        ast_free(n->u.for_init.init_expr);
        break;
    case NODE_RETURN:
        ast_free(n->u.ret.value);
        break;
    case NODE_EXPR_STMT:
        ast_free(n->u.expr_stmt.expr);
        break;
    case NODE_STRING_LIT:
        free(n->u.string_lit.value);
        break;
    case NODE_ARRAY_LIT:
        nodelist_free(&n->u.array_lit.elems);
        break;
    case NODE_IDENT:
        free(n->u.ident.name);
        break;
    case NODE_BINOP:
        ast_free(n->u.binop.left);
        ast_free(n->u.binop.right);
        break;
    case NODE_UNOP:
        ast_free(n->u.unop.operand);
        break;
    case NODE_CALL:
        free(n->u.call.name);
        nodelist_free(&n->u.call.args);
        break;
    case NODE_INDEX:
        ast_free(n->u.index.base);
        ast_free(n->u.index.index);
        break;
    case NODE_INCR:
    case NODE_DECR:
        free(n->u.incr_decr.name);
        break;
    case NODE_CONST:
        free(n->u.constant.name);
        break;
    default:
        break;
    }
    free(n);
}

/* -----------------------------------------------------------------------
 * Convenience constructors
 * ----------------------------------------------------------------------- */
AstNode *ast_int_lit(int64_t v, int line, int col) {
    AstNode *n = ast_new(NODE_INT_LIT, line, col);
    n->u.int_lit.value = v;
    n->type = TYPE_INT;
    return n;
}

AstNode *ast_float_lit(double v, int line, int col) {
    AstNode *n = ast_new(NODE_FLOAT_LIT, line, col);
    n->u.float_lit.value = v;
    n->type = TYPE_FLOAT;
    return n;
}

AstNode *ast_bool_lit(int v, int line, int col) {
    AstNode *n = ast_new(NODE_BOOL_LIT, line, col);
    n->u.bool_lit.value = v;
    n->type = TYPE_BOOL;
    return n;
}

AstNode *ast_string_lit(const char *s, int line, int col) {
    AstNode *n = ast_new(NODE_STRING_LIT, line, col);
    n->u.string_lit.value = cimple_strdup(s);
    n->type = TYPE_STRING;
    return n;
}

AstNode *ast_ident(const char *name, int line, int col) {
    AstNode *n = ast_new(NODE_IDENT, line, col);
    n->u.ident.name = cimple_strdup(name);
    return n;
}

AstNode *ast_const(const char *name, int line, int col) {
    AstNode *n = ast_new(NODE_CONST, line, col);
    n->u.constant.name = cimple_strdup(name);
    return n;
}

AstNode *ast_binop(OpKind op, AstNode *l, AstNode *r, int line, int col) {
    AstNode *n = ast_new(NODE_BINOP, line, col);
    n->u.binop.op    = op;
    n->u.binop.left  = l;
    n->u.binop.right = r;
    return n;
}

AstNode *ast_unop(OpKind op, AstNode *operand, int line, int col) {
    AstNode *n = ast_new(NODE_UNOP, line, col);
    n->u.unop.op      = op;
    n->u.unop.operand = operand;
    return n;
}

AstNode *ast_call(const char *name, NodeList args, int line, int col) {
    AstNode *n = ast_new(NODE_CALL, line, col);
    n->u.call.name = cimple_strdup(name);
    n->u.call.args = args;
    return n;
}
