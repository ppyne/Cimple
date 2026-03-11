#include "interpreter.h"
#include "builtins.h"
#include "../common/error.h"
#include "../common/memory.h"
#include "../semantic/semantic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Forward declarations */
static Value eval_expr(Interp *ip, Scope *scope, AstNode *n);
static void  exec_stmt(Interp *ip, Scope *scope, AstNode *n);
static Value call_func(Interp *ip, const char *name, Value *args, int nargs,
                        int line, int col);
static AstNode *find_func_decl(Interp *ip, const char *name);

static int is_mutating_array_builtin(const char *name) {
    return strcmp(name, "arrayPush") == 0 ||
           strcmp(name, "arrayPop") == 0 ||
           strcmp(name, "arrayInsert") == 0 ||
           strcmp(name, "arrayRemove") == 0 ||
           strcmp(name, "arraySet") == 0;
}

/* -----------------------------------------------------------------------
 * Predefined constants
 * ----------------------------------------------------------------------- */
#include <stdint.h>
#include <float.h>
static Value eval_constant(const char *name) {
    if (strcmp(name, "INT_MAX")       == 0) return val_int(INT64_MAX);
    if (strcmp(name, "INT_MIN")       == 0) return val_int(INT64_MIN);
    if (strcmp(name, "INT_SIZE")      == 0) return val_int(8);
    if (strcmp(name, "FLOAT_SIZE")    == 0) return val_int(8);
    if (strcmp(name, "FLOAT_DIG")     == 0) return val_int(15);
    if (strcmp(name, "FLOAT_EPSILON") == 0) return val_float(DBL_EPSILON);
    if (strcmp(name, "FLOAT_MIN")     == 0) return val_float(DBL_MIN);
    if (strcmp(name, "FLOAT_MAX")     == 0) return val_float(DBL_MAX);
    if (strcmp(name, "M_PI")          == 0) return val_float(3.141592653589793);
    if (strcmp(name, "M_E")           == 0) return val_float(2.718281828459045);
    if (strcmp(name, "M_TAU")         == 0) return val_float(6.283185307179586);
    if (strcmp(name, "M_SQRT2")       == 0) return val_float(1.4142135623730951);
    if (strcmp(name, "M_LN2")         == 0) return val_float(0.6931471805599453);
    if (strcmp(name, "M_LN10")        == 0) return val_float(2.302585092994046);
    return val_void();
}

/* -----------------------------------------------------------------------
 * eval_expr
 * ----------------------------------------------------------------------- */
static Value eval_expr(Interp *ip, Scope *scope, AstNode *n) {
    if (!n) return val_void();

    switch (n->kind) {
    case NODE_INT_LIT:    return val_int(n->u.int_lit.value);
    case NODE_FLOAT_LIT:  return val_float(n->u.float_lit.value);
    case NODE_BOOL_LIT:   return val_bool(n->u.bool_lit.value);
    case NODE_STRING_LIT: return val_string(n->u.string_lit.value);

    case NODE_CONST:
        return eval_constant(n->u.constant.name);

    case NODE_IDENT: {
        Symbol *sym = scope_lookup(scope, n->u.ident.name);
        if (!sym) error_runtime(n->line, n->col,
                                "undefined variable '%s'", n->u.ident.name);
        return value_copy(sym->val);
    }

    case NODE_ARRAY_LIT: {
        NodeList  *elems = &n->u.array_lit.elems;
        CimpleType elem_t = n->u.array_lit.elem_type;
        /* Determine element type from declaration context if unknown */
        if (elem_t == TYPE_UNKNOWN && elems->count > 0) {
            Value first = eval_expr(ip, scope, elems->items[0]);
            elem_t = first.type;
            value_free(&first);
        }
        if (elem_t == TYPE_UNKNOWN) elem_t = TYPE_INT; /* empty array fallback */
        Value arr = val_array(elem_t);
        for (int i = 0; i < elems->count; i++) {
            Value v = eval_expr(ip, scope, elems->items[i]);
            array_push(arr.u.arr, v);
            value_free(&v);
        }
        return arr;
    }

    case NODE_BINOP: {
        /* Short-circuit for &&, || */
        if (n->u.binop.op == OP_AND) {
            Value l = eval_expr(ip, scope, n->u.binop.left);
            if (!l.u.b) return val_bool(0);
            Value r = eval_expr(ip, scope, n->u.binop.right);
            int res = r.u.b;
            value_free(&r);
            return val_bool(res);
        }
        if (n->u.binop.op == OP_OR) {
            Value l = eval_expr(ip, scope, n->u.binop.left);
            if (l.u.b) return val_bool(1);
            Value r = eval_expr(ip, scope, n->u.binop.right);
            int res = r.u.b;
            value_free(&r);
            return val_bool(res);
        }

        Value l = eval_expr(ip, scope, n->u.binop.left);
        Value r = eval_expr(ip, scope, n->u.binop.right);
        Value result;

        switch (n->u.binop.op) {
        /* Arithmetic */
        case OP_ADD:
            if (l.type == TYPE_STRING && r.type == TYPE_STRING) {
                size_t ll = strlen(l.u.s), rl = strlen(r.u.s);
                char *s = (char *)cimple_malloc(ll + rl + 1);
                memcpy(s, l.u.s, ll);
                memcpy(s + ll, r.u.s, rl);
                s[ll + rl] = '\0';
                result = val_string_own(s);
            } else if (l.type == TYPE_INT) {
                result = val_int(l.u.i + r.u.i);
            } else {
                result = val_float(l.u.f + r.u.f);
            }
            break;
        case OP_SUB:
            result = l.type == TYPE_INT ? val_int(l.u.i - r.u.i)
                                        : val_float(l.u.f - r.u.f);
            break;
        case OP_MUL:
            result = l.type == TYPE_INT ? val_int(l.u.i * r.u.i)
                                        : val_float(l.u.f * r.u.f);
            break;
        case OP_DIV:
            if (l.type == TYPE_INT) {
                if (r.u.i == 0)
                    error_runtime(n->line, n->col, "Integer division by zero");
                result = val_int(l.u.i / r.u.i);
            } else {
                result = val_float(l.u.f / r.u.f);
            }
            break;
        case OP_MOD:
            if (r.u.i == 0)
                error_runtime(n->line, n->col, "Integer modulo by zero");
            result = val_int(l.u.i % r.u.i);
            break;

        /* Comparison */
        case OP_EQ:
            if (l.type == TYPE_INT)    result = val_bool(l.u.i == r.u.i);
            else if (l.type == TYPE_FLOAT) result = val_bool(l.u.f == r.u.f);
            else if (l.type == TYPE_BOOL)  result = val_bool(l.u.b == r.u.b);
            else result = val_bool(strcmp(l.u.s, r.u.s) == 0);
            break;
        case OP_NEQ:
            if (l.type == TYPE_INT)    result = val_bool(l.u.i != r.u.i);
            else if (l.type == TYPE_FLOAT) result = val_bool(l.u.f != r.u.f);
            else if (l.type == TYPE_BOOL)  result = val_bool(l.u.b != r.u.b);
            else result = val_bool(strcmp(l.u.s, r.u.s) != 0);
            break;
        case OP_LT:
            result = l.type == TYPE_INT ? val_bool(l.u.i < r.u.i)
                                        : val_bool(l.u.f < r.u.f);
            break;
        case OP_LEQ:
            result = l.type == TYPE_INT ? val_bool(l.u.i <= r.u.i)
                                        : val_bool(l.u.f <= r.u.f);
            break;
        case OP_GT:
            result = l.type == TYPE_INT ? val_bool(l.u.i > r.u.i)
                                        : val_bool(l.u.f > r.u.f);
            break;
        case OP_GEQ:
            result = l.type == TYPE_INT ? val_bool(l.u.i >= r.u.i)
                                        : val_bool(l.u.f >= r.u.f);
            break;

        /* Bitwise */
        case OP_BAND:   result = val_int(l.u.i & r.u.i);  break;
        case OP_BOR:    result = val_int(l.u.i | r.u.i);  break;
        case OP_BXOR:   result = val_int(l.u.i ^ r.u.i);  break;
        case OP_LSHIFT: result = val_int(l.u.i << (int)r.u.i); break;
        case OP_RSHIFT: result = val_int(l.u.i >> (int)r.u.i); break;

        default: result = val_void(); break;
        }
        value_free(&l);
        value_free(&r);
        return result;
    }

    case NODE_UNOP: {
        Value v = eval_expr(ip, scope, n->u.unop.operand);
        Value result;
        switch (n->u.unop.op) {
        case OP_NOT:  result = val_bool(!v.u.b); break;
        case OP_NEG:
            result = v.type == TYPE_INT ? val_int(-v.u.i) : val_float(-v.u.f);
            break;
        case OP_BNOT: result = val_int(~v.u.i); break;
        default:      result = val_void(); break;
        }
        value_free(&v);
        return result;
    }

    case NODE_INDEX: {
        Value base  = eval_expr(ip, scope, n->u.index.base);
        Value index = eval_expr(ip, scope, n->u.index.index);
        int   idx   = (int)index.u.i;
        Value result;

        if (base.type == TYPE_STRING) {
            int blen = (int)strlen(base.u.s);
            if (idx < 0 || idx >= blen)
                error_runtime(n->line, n->col,
                              "String index out of bounds (Index: %d   String length: %d bytes)",
                              idx, blen);
            char buf[2] = { base.u.s[idx], '\0' };
            result = val_string(buf);
        } else if (type_is_array(base.type)) {
            result = array_get(base.u.arr, idx, n->line, n->col);
        } else {
            error_runtime(n->line, n->col,
                          "Operator '[]' cannot be applied to this value at runtime");
            result = val_void();
        }
        value_free(&base);
        return result;
    }

    case NODE_CALL: {
        const char *fname = n->u.call.name;
        NodeList   *args  = &n->u.call.args;
        Value       arg_vals[32];
        int         borrowed[32] = {0};
        AstNode    *user_func = find_func_decl(ip, fname);
        int nargs = args->count;
        if (nargs > 32) nargs = 32;
        for (int i = 0; i < nargs; i++) {
            int borrow_array_arg =
                args->items[i]->kind == NODE_IDENT &&
                ((i == 0 && is_mutating_array_builtin(fname)) ||
                 (user_func &&
                  i < user_func->u.func_decl.params.count &&
                  type_is_array(user_func->u.func_decl.params.items[i]->u.param.type)));

            if (borrow_array_arg) {
                Symbol *sym = scope_lookup(scope, args->items[i]->u.ident.name);
                if (!sym) {
                    error_runtime(n->line, n->col,
                                  "undefined variable '%s'", args->items[i]->u.ident.name);
                }
                arg_vals[i] = sym->val;
                borrowed[i] = 1;
            } else {
                arg_vals[i] = eval_expr(ip, scope, args->items[i]);
            }
        }
        Value result = call_func(ip, fname, arg_vals, nargs, n->line, n->col);
        for (int i = 0; i < nargs; i++) {
            if (!borrowed[i]) value_free(&arg_vals[i]);
        }
        return result;
    }

    default:
        return val_void();
    }
}

/* -----------------------------------------------------------------------
 * exec_stmt
 * ----------------------------------------------------------------------- */
static void exec_stmt(Interp *ip, Scope *scope, AstNode *n) {
    if (!n || ip->signal != SIGNAL_NONE) return;

    switch (n->kind) {
    case NODE_VAR_DECL: {
        const char *name = n->u.var_decl.name;
        CimpleType   t   = n->u.var_decl.type;

        Value v;
        if (n->u.var_decl.init) {
            v = eval_expr(ip, scope, n->u.var_decl.init);
            /* Propagate element type for empty arrays */
            if (n->u.var_decl.init->kind == NODE_ARRAY_LIT &&
                n->u.var_decl.init->u.array_lit.elems.count == 0 &&
                v.u.arr) {
                v.u.arr->elem_type = type_elem(t);
                v.type = t;
            }
        } else {
            v = value_default(t);
        }

        Symbol *sym = scope_define(scope, name, t, n->line, n->col);
        if (!sym) {
            /* Re-definition at runtime (shouldn't happen if semantic check passed) */
            sym = scope_lookup_local(scope, name);
        }
        if (sym) {
            value_free(&sym->val);
            sym->val = v;
        } else {
            value_free(&v);
        }
        break;
    }

    case NODE_ASSIGN: {
        Symbol *sym = scope_lookup(scope, n->u.assign.name);
        if (!sym) error_runtime(n->line, n->col,
                                "undefined variable '%s'", n->u.assign.name);
        Value v = eval_expr(ip, scope, n->u.assign.value);
        value_free(&sym->val);
        sym->val = v;
        break;
    }

    case NODE_INDEX_ASSIGN: {
        Symbol *sym = scope_lookup(scope, n->u.index_assign.name);
        if (!sym) error_runtime(n->line, n->col,
                                "undefined variable '%s'", n->u.index_assign.name);
        Value idx_v = eval_expr(ip, scope, n->u.index_assign.index);
        Value val   = eval_expr(ip, scope, n->u.index_assign.value);
        int   idx   = (int)idx_v.u.i;
        array_set(sym->val.u.arr, idx, val, n->line, n->col);
        value_free(&idx_v);
        value_free(&val);
        break;
    }

    case NODE_BLOCK: {
        Scope *inner = scope_new(scope, 0);
        NodeList *stmts = &n->u.block.stmts;
        for (int i = 0; i < stmts->count && ip->signal == SIGNAL_NONE; i++)
            exec_stmt(ip, inner, stmts->items[i]);
        scope_free(inner);
        break;
    }

    case NODE_IF: {
        Value cond = eval_expr(ip, scope, n->u.if_stmt.cond);
        if (cond.u.b) {
            exec_stmt(ip, scope, n->u.if_stmt.then_branch);
        } else if (n->u.if_stmt.else_branch) {
            exec_stmt(ip, scope, n->u.if_stmt.else_branch);
        }
        break;
    }

    case NODE_WHILE: {
        for (;;) {
            Value cond = eval_expr(ip, scope, n->u.while_stmt.cond);
            if (!cond.u.b) break;
            exec_stmt(ip, scope, n->u.while_stmt.body);
            if (ip->signal == SIGNAL_BREAK) { ip->signal = SIGNAL_NONE; break; }
            if (ip->signal == SIGNAL_CONTINUE) { ip->signal = SIGNAL_NONE; continue; }
            if (ip->signal == SIGNAL_RETURN) break;
        }
        break;
    }

    case NODE_FOR: {
        Scope *for_scope = scope_new(scope, 0);

        /* Init */
        if (n->u.for_stmt.init) {
            AstNode *init = n->u.for_stmt.init;
            if (init->kind == NODE_FOR_INIT) {
                Value init_v = eval_expr(ip, for_scope, init->u.for_init.init_expr);
                Symbol *sym  = scope_define(for_scope, init->u.for_init.name,
                                            TYPE_INT, init->line, init->col);
                if (sym) { value_free(&sym->val); sym->val = init_v; }
                else value_free(&init_v);
            } else {
                exec_stmt(ip, for_scope, init);
            }
        }

        for (;;) {
            if (n->u.for_stmt.cond) {
                Value cond = eval_expr(ip, for_scope, n->u.for_stmt.cond);
                int ok = cond.u.b;
                value_free(&cond);
                if (!ok) break;
            }
            exec_stmt(ip, for_scope, n->u.for_stmt.body);
            if (ip->signal == SIGNAL_BREAK)    { ip->signal = SIGNAL_NONE; break; }
            if (ip->signal == SIGNAL_CONTINUE) { ip->signal = SIGNAL_NONE; }
            if (ip->signal == SIGNAL_RETURN)   break;

            /* Update */
            if (n->u.for_stmt.update)
                exec_stmt(ip, for_scope, n->u.for_stmt.update);
        }
        scope_free(for_scope);
        break;
    }

    case NODE_RETURN:
        if (n->u.ret.value) {
            value_free(&ip->ret_val);
            ip->ret_val = eval_expr(ip, scope, n->u.ret.value);
        } else {
            ip->ret_val = val_void();
        }
        ip->signal = SIGNAL_RETURN;
        break;

    case NODE_BREAK:
        ip->signal = SIGNAL_BREAK;
        break;

    case NODE_CONTINUE:
        ip->signal = SIGNAL_CONTINUE;
        break;

    case NODE_INCR: {
        Symbol *sym = scope_lookup(scope, n->u.incr_decr.name);
        if (!sym) error_runtime(n->line, n->col,
                                "undefined variable '%s'", n->u.incr_decr.name);
        sym->val.u.i++;
        break;
    }

    case NODE_DECR: {
        Symbol *sym = scope_lookup(scope, n->u.incr_decr.name);
        if (!sym) error_runtime(n->line, n->col,
                                "undefined variable '%s'", n->u.incr_decr.name);
        sym->val.u.i--;
        break;
    }

    case NODE_EXPR_STMT: {
        Value v = eval_expr(ip, scope, n->u.expr_stmt.expr);
        value_free(&v);
        break;
    }

    case NODE_FOR_INIT:
        /* handled in NODE_FOR */
        break;

    default:
        break;
    }
}

/* -----------------------------------------------------------------------
 * User function call
 * ----------------------------------------------------------------------- */
static AstNode *find_func_decl(Interp *ip, const char *name) {
    NodeList *decls = &ip->program->u.program.decls;
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d->kind == NODE_FUNC_DECL &&
            strcmp(d->u.func_decl.name, name) == 0)
            return d;
    }
    return NULL;
}

static Value call_user_func(Interp *ip, AstNode *f, Value *args, int nargs,
                             int call_line, int call_col) {
    (void)call_line; (void)call_col;

    /* Create function scope (child of global) */
    Scope *fn_scope = scope_new(ip->global, 1);
    Symbol *bound_params[32] = {0};
    int borrowed_params[32] = {0};

    /* Bind parameters */
    NodeList *params = &f->u.func_decl.params;
    for (int i = 0; i < params->count && i < nargs; i++) {
        AstNode *p   = params->items[i];
        Symbol  *sym = scope_define(fn_scope, p->u.param.name,
                                    p->u.param.type, p->line, p->col);
        if (sym) {
            if (type_is_array(p->u.param.type) && type_is_array(args[i].type)) {
                value_free(&sym->val);
                sym->val = args[i];
                borrowed_params[i] = 1;
            } else {
                sym->val = value_copy(args[i]);
            }
            bound_params[i] = sym;
        }
    }

    /* Save signal state */
    Signal  saved_sig = ip->signal;
    Value   saved_ret = ip->ret_val;
    ip->signal  = SIGNAL_NONE;
    ip->ret_val = val_void();

    /* Execute body */
    AstNode *body = f->u.func_decl.body;
    NodeList *stmts = &body->u.block.stmts;
    for (int i = 0; i < stmts->count && ip->signal != SIGNAL_RETURN; i++)
        exec_stmt(ip, fn_scope, stmts->items[i]);

    Value ret = ip->ret_val;
    ip->signal  = saved_sig;
    ip->ret_val = saved_ret;

    for (int i = 0; i < params->count && i < nargs; i++) {
        if (borrowed_params[i] && bound_params[i]) {
            bound_params[i]->val = val_void();
        }
    }

    scope_free(fn_scope);
    return ret;
}

/* -----------------------------------------------------------------------
 * call_func — dispatch to builtin or user function
 * ----------------------------------------------------------------------- */
static Value call_func(Interp *ip, const char *name, Value *args, int nargs,
                        int line, int col) {
    /* Try builtin first */
    Value result = builtin_call(name, args, nargs, line, col);
    if (result.type != TYPE_UNKNOWN)
        return result;

    /* Try user function */
    AstNode *f = find_func_decl(ip, name);
    if (f)
        return call_user_func(ip, f, args, nargs, line, col);

    error_runtime(line, col, "Unknown function: '%s'", name);
    return val_void();
}

/* -----------------------------------------------------------------------
 * interp_run — main entry point
 * ----------------------------------------------------------------------- */
int interp_run(AstNode *program, int argc, char **argv) {
    Interp ip;
    ip.program = program;
    ip.global  = scope_new(NULL, 0);
    ip.funcs   = NULL;
    ip.signal  = SIGNAL_NONE;
    ip.ret_val = val_void();

    /* Initialise global variables */
    NodeList *decls = &program->u.program.decls;
    for (int i = 0; i < decls->count; i++) {
        AstNode *d = decls->items[i];
        if (d->kind == NODE_VAR_DECL)
            exec_stmt(&ip, ip.global, d);
    }

    /* Find main */
    AstNode *main_fn = find_func_decl(&ip, "main");
    if (!main_fn) {
        fprintf(stderr, "[RUNTIME ERROR] no 'main' function\n");
        return 1;
    }

    /* Build args */
    Value args[2];
    int nargs = main_fn->u.func_decl.params.count;
    if (nargs >= 1) {
        args[0] = val_array(TYPE_STRING);
        for (int i = 0; i < argc; i++) {
            Value s = val_string(argv[i]);
            array_push(args[0].u.arr, s);
            value_free(&s);
        }
    }

    Value ret = call_user_func(&ip, main_fn, args, nargs, 0, 0);

    int exit_code = 0;
    if (main_fn->u.func_decl.ret_type == TYPE_INT) {
        exit_code = (int)ret.u.i;
    }

    value_free(&ret);
    if (nargs >= 1) value_free(&args[0]);
    value_free(&ip.ret_val);
    scope_free(ip.global);

    return exit_code;
}
