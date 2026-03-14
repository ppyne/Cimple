#ifndef CIMPLE_AST_H
#define CIMPLE_AST_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Type identifiers
 * ----------------------------------------------------------------------- */
typedef enum {
    TYPE_INT       = 0,
    TYPE_FLOAT     = 1,
    TYPE_BOOL      = 2,
    TYPE_STRING    = 3,
    TYPE_BYTE      = 4,
    TYPE_FUNC      = 5,
    TYPE_VOID      = 6,
    /* Array variants */
    TYPE_INT_ARR   = 7,
    TYPE_FLOAT_ARR = 8,
    TYPE_BOOL_ARR  = 9,
    TYPE_STR_ARR   = 10,
    TYPE_BYTE_ARR  = 11,
    /* Opaque */
    TYPE_EXEC_RESULT = 12,
    TYPE_STRUCT      = 13,
    TYPE_STRUCT_ARR  = 14,
    TYPE_UNION       = 15,
    TYPE_UNION_ARR   = 16,
    /* 2-D array variants */
    TYPE_INT_ARR_ARR    = 18,
    TYPE_FLOAT_ARR_ARR  = 19,
    TYPE_BOOL_ARR_ARR   = 20,
    TYPE_STR_ARR_ARR    = 21,
    TYPE_BYTE_ARR_ARR   = 22,
    TYPE_STRUCT_ARR_ARR = 23,
    TYPE_UNION_ARR_ARR  = 24,
    TYPE_REGEXP         = 26,
    TYPE_REGEXP_MATCH   = 27,
    TYPE_REGEXP_MATCH_ARR = 29,
    TYPE_UNKNOWN        = 30
} CimpleType;

typedef struct FuncType {
    CimpleType ret;
    CimpleType params[8];
    int        param_count;
} FuncType;

/* Returns true if t is an array type (1D or 2D). */
static inline int type_is_array(CimpleType t) {
    return (t >= TYPE_INT_ARR && t <= TYPE_BYTE_ARR)
        || t == TYPE_STRUCT_ARR  || t == TYPE_UNION_ARR
        || t == TYPE_INT_ARR_ARR || t == TYPE_FLOAT_ARR_ARR
        || t == TYPE_BOOL_ARR_ARR || t == TYPE_STR_ARR_ARR
        || t == TYPE_BYTE_ARR_ARR || t == TYPE_STRUCT_ARR_ARR
        || t == TYPE_UNION_ARR_ARR || t == TYPE_REGEXP_MATCH_ARR;
}

/* Returns the element type of an array type. */
static inline CimpleType type_elem(CimpleType t) {
    switch (t) {
    case TYPE_INT_ARR:      return TYPE_INT;
    case TYPE_FLOAT_ARR:    return TYPE_FLOAT;
    case TYPE_BOOL_ARR:     return TYPE_BOOL;
    case TYPE_STR_ARR:      return TYPE_STRING;
    case TYPE_BYTE_ARR:     return TYPE_BYTE;
    case TYPE_STRUCT_ARR:   return TYPE_STRUCT;
    case TYPE_UNION_ARR:    return TYPE_UNION;
    case TYPE_REGEXP_MATCH_ARR: return TYPE_REGEXP_MATCH;
    /* 2D → 1D */
    case TYPE_INT_ARR_ARR:    return TYPE_INT_ARR;
    case TYPE_FLOAT_ARR_ARR:  return TYPE_FLOAT_ARR;
    case TYPE_BOOL_ARR_ARR:   return TYPE_BOOL_ARR;
    case TYPE_STR_ARR_ARR:    return TYPE_STR_ARR;
    case TYPE_BYTE_ARR_ARR:   return TYPE_BYTE_ARR;
    case TYPE_STRUCT_ARR_ARR: return TYPE_STRUCT_ARR;
    case TYPE_UNION_ARR_ARR:  return TYPE_UNION_ARR;
    default:                  return TYPE_UNKNOWN;
    }
}

/* Returns the array type for a scalar or 1D array type. */
static inline CimpleType type_make_array(CimpleType t) {
    switch (t) {
    case TYPE_INT:        return TYPE_INT_ARR;
    case TYPE_FLOAT:      return TYPE_FLOAT_ARR;
    case TYPE_BOOL:       return TYPE_BOOL_ARR;
    case TYPE_STRING:     return TYPE_STR_ARR;
    case TYPE_BYTE:       return TYPE_BYTE_ARR;
    case TYPE_STRUCT:     return TYPE_STRUCT_ARR;
    case TYPE_UNION:      return TYPE_UNION_ARR;
    case TYPE_REGEXP_MATCH: return TYPE_REGEXP_MATCH_ARR;
    /* 1D → 2D */
    case TYPE_INT_ARR:    return TYPE_INT_ARR_ARR;
    case TYPE_FLOAT_ARR:  return TYPE_FLOAT_ARR_ARR;
    case TYPE_BOOL_ARR:   return TYPE_BOOL_ARR_ARR;
    case TYPE_STR_ARR:    return TYPE_STR_ARR_ARR;
    case TYPE_BYTE_ARR:   return TYPE_BYTE_ARR_ARR;
    case TYPE_STRUCT_ARR: return TYPE_STRUCT_ARR_ARR;
    case TYPE_UNION_ARR:  return TYPE_UNION_ARR_ARR;
    default:              return TYPE_UNKNOWN;
    }
}

const char *type_name(CimpleType t);
FuncType *func_type_new(CimpleType ret);
FuncType *func_type_copy(const FuncType *src);
void      func_type_free(FuncType *ft);

/* -----------------------------------------------------------------------
 * AST node kinds
 * ----------------------------------------------------------------------- */
typedef enum {
    /* Declarations */
    NODE_PROGRAM,        /* list of top-level declarations              */
    NODE_FUNC_DECL,      /* function declaration                        */
    NODE_PARAM,          /* single parameter                            */
    NODE_VAR_DECL,       /* variable declaration (scalar or array)      */
    NODE_STRUCT_DECL,    /* structure declaration                       */
    NODE_STRUCT_FIELD,   /* field declaration inside a structure        */
    NODE_UNION_DECL,     /* union declaration                           */

    /* Statements */
    NODE_BLOCK,          /* { stmt* }                                   */
    NODE_ASSIGN,         /* ident = expr                                */
    NODE_INDEX_ASSIGN,   /* ident[expr] = expr                          */
    NODE_INDEX2_ASSIGN,  /* ident[expr][expr] = expr                    */
    NODE_MEMBER_ASSIGN,  /* member = expr                               */
    NODE_IF,             /* if (cond) stmt [else stmt]                  */
    NODE_WHILE,          /* while (cond) stmt                           */
    NODE_FOR,            /* for (init; cond; update) stmt               */
    NODE_FOR_INIT,       /* int i = expr  (for-loop init)               */
    NODE_RETURN,         /* return [expr]                               */
    NODE_BREAK,          /* break                                       */
    NODE_CONTINUE,       /* continue                                    */
    NODE_EXPR_STMT,      /* expression used as statement                */
    NODE_SWITCH,         /* switch(expr) { case ... }                   */
    NODE_CASE,           /* case value: stmts                           */
    NODE_DEFAULT_CASE,   /* default: stmts                              */
    NODE_THROW,          /* throw expr                                  */
    NODE_TRY_CATCH,      /* try { } catch (...) { }                     */
    NODE_CATCH_CLAUSE,   /* catch (Type name) { ... }                   */

    /* Expressions */
    NODE_INT_LIT,        /* integer literal                             */
    NODE_FLOAT_LIT,      /* float literal                               */
    NODE_BOOL_LIT,       /* true / false                                */
    NODE_STRING_LIT,     /* string literal (decoded)                    */
    NODE_ARRAY_LIT,      /* [ expr, expr, ... ]                         */
    NODE_IDENT,          /* identifier                                  */
    NODE_FUNC_REF,       /* named function used as a value              */
    NODE_BINOP,          /* binary operator                             */
    NODE_UNOP,           /* unary operator                              */
    NODE_CALL,           /* function call                               */
    NODE_INDEX,          /* expr[expr]                                  */
    NODE_MEMBER,         /* expr.ident / self.ident                     */
    NODE_KIND_ACCESS,    /* expr.kind on union                          */
    NODE_METHOD_CALL,    /* expr.method(...) / super.method(...)        */
    NODE_CLONE,          /* clone StructName                            */
    NODE_SELF,           /* self                                        */
    NODE_SUPER,          /* super                                       */
    NODE_INCR,           /* i++                                         */
    NODE_DECR,           /* i--                                         */
    NODE_CONST,          /* predefined constant (INT_MAX etc.)          */
    NODE_COMPOUND_ASSIGN,/* name op= expr  (+=, -=, *=, /=, %=)        */
    NODE_TERNARY,        /* cond ? then : else                          */
} NodeKind;

/* -----------------------------------------------------------------------
 * Binary / unary operator tags
 * ----------------------------------------------------------------------- */
typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_AND, OP_OR,
    OP_BAND, OP_BOR, OP_BXOR, OP_LSHIFT, OP_RSHIFT,
    OP_EQ, OP_NEQ, OP_LT, OP_LEQ, OP_GT, OP_GEQ,
    OP_NOT, OP_NEG, OP_BNOT   /* unary */
} OpKind;

/* -----------------------------------------------------------------------
 * Dynamic node list (used for children)
 * ----------------------------------------------------------------------- */
typedef struct AstNode AstNode;

typedef struct {
    AstNode **items;
    int       count;
    int       cap;
} NodeList;

void nodelist_init(NodeList *l);
void nodelist_push(NodeList *l, AstNode *n);
void nodelist_free(NodeList *l);

/* -----------------------------------------------------------------------
 * AST node
 * ----------------------------------------------------------------------- */
struct AstNode {
    NodeKind  kind;
    int       line;
    int       col;
    CimpleType type;   /* inferred/declared type (set by semantic pass) */
    char     *type_name_hint; /* structure name when type is TYPE_STRUCT/_ARR */
    FuncType *func_type_hint; /* function signature when type is TYPE_FUNC    */

    union {
        /* NODE_PROGRAM */
        struct { NodeList decls; } program;

        /* NODE_FUNC_DECL */
        struct {
            char      *name;
            CimpleType ret_type;
            char      *ret_struct_name;
            NodeList   params;   /* NODE_PARAM */
            AstNode   *body;     /* NODE_BLOCK  */
        } func_decl;

        /* NODE_PARAM */
        struct {
            char      *name;
            CimpleType type;
            char      *struct_name;
            FuncType  *func_type;
        } param;

        /* NODE_VAR_DECL */
        struct {
            char      *name;
            CimpleType type;
            char      *struct_name;
            FuncType  *func_type;
            AstNode   *init;    /* NULL if uninitialized                */
            int        is_global;
        } var_decl;

        /* NODE_STRUCT_DECL */
        struct {
            char    *name;
            char    *base_name;   /* NULL if no base */
            NodeList members;     /* NODE_STRUCT_FIELD and NODE_FUNC_DECL */
        } struct_decl;

        /* NODE_STRUCT_FIELD */
        struct {
            char      *name;
            CimpleType type;
            char      *struct_name;
            AstNode   *init;
        } struct_field;

        /* NODE_UNION_DECL */
        struct {
            char    *name;
            NodeList members;     /* NODE_STRUCT_FIELD reused */
        } union_decl;

        /* NODE_BLOCK */
        struct { NodeList stmts; } block;

        /* NODE_ASSIGN */
        struct {
            char    *name;
            AstNode *value;
        } assign;

        /* NODE_INDEX_ASSIGN */
        struct {
            char    *name;
            AstNode *index;
            AstNode *value;
        } index_assign;

        /* NODE_INDEX2_ASSIGN */
        struct {
            char    *name;
            AstNode *index1;
            AstNode *index2;
            AstNode *value;
        } index2_assign;

        /* NODE_MEMBER_ASSIGN */
        struct {
            AstNode *target;
            AstNode *value;
        } member_assign;

        /* NODE_IF */
        struct {
            AstNode *cond;
            AstNode *then_branch;
            AstNode *else_branch; /* NULL if no else */
        } if_stmt;

        /* NODE_WHILE */
        struct {
            AstNode *cond;
            AstNode *body;
        } while_stmt;

        /* NODE_FOR */
        struct {
            AstNode *init;   /* NODE_FOR_INIT or NODE_ASSIGN */
            AstNode *cond;
            AstNode *update; /* NODE_ASSIGN, NODE_INCR or NODE_DECR */
            AstNode *body;
        } for_stmt;

        /* NODE_FOR_INIT */
        struct {
            char    *name;
            AstNode *init_expr;
        } for_init;

        /* NODE_RETURN */
        struct { AstNode *value; /* NULL for void return */ } ret;

        /* NODE_EXPR_STMT */
        struct { AstNode *expr; } expr_stmt;

        /* NODE_SWITCH */
        struct {
            AstNode *expr;
            NodeList cases;       /* NODE_CASE / NODE_DEFAULT_CASE */
        } switch_stmt;

        /* NODE_CASE / NODE_DEFAULT_CASE */
        struct {
            AstNode *value;       /* NULL for default */
            NodeList stmts;
        } case_stmt;

        /* NODE_THROW */
        struct {
            AstNode *expr;
        } throw_stmt;

        /* NODE_TRY_CATCH */
        struct {
            AstNode *try_block;
            NodeList clauses;     /* NODE_CATCH_CLAUSE */
        } try_catch;

        /* NODE_CATCH_CLAUSE */
        struct {
            char    *type_name;
            char    *var_name;
            AstNode *block;
        } catch_clause;

        /* NODE_INT_LIT */
        struct { int64_t value; } int_lit;

        /* NODE_FLOAT_LIT */
        struct { double value; } float_lit;

        /* NODE_BOOL_LIT */
        struct { int value; } bool_lit;

        /* NODE_STRING_LIT */
        struct { char *value; } string_lit;

        /* NODE_ARRAY_LIT */
        struct {
            NodeList   elems;
            CimpleType elem_type; /* resolved element type */
        } array_lit;

        /* NODE_IDENT */
        struct { char *name; } ident;

        /* NODE_FUNC_REF */
        struct { char *name; } func_ref;

        /* NODE_BINOP */
        struct {
            OpKind   op;
            AstNode *left;
            AstNode *right;
        } binop;

        /* NODE_UNOP */
        struct {
            OpKind   op;
            AstNode *operand;
        } unop;

        /* NODE_CALL */
        struct {
            char    *name;
            NodeList args;
        } call;

        /* NODE_INDEX */
        struct {
            AstNode *base;  /* array or string expression */
            AstNode *index;
        } index;

        /* NODE_MEMBER */
        struct {
            AstNode *base;
            char    *name;
        } member;

        /* NODE_KIND_ACCESS */
        struct {
            AstNode *base;
        } kind_access;

        /* NODE_METHOD_CALL */
        struct {
            AstNode *base;   /* NULL only for super.method(...) */
            char    *name;
            NodeList args;
            int      is_super;
        } method_call;

        /* NODE_CLONE */
        struct { char *struct_name; } clone_expr;

        /* NODE_INCR / NODE_DECR */
        struct { char *name; } incr_decr;

        /* NODE_CONST */
        struct { char *name; } constant;

        /* NODE_COMPOUND_ASSIGN */
        struct {
            char   *name;
            OpKind  op;     /* OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD */
            AstNode *value;
        } compound_assign;

        /* NODE_TERNARY */
        struct {
            AstNode *cond;
            AstNode *then_expr;
            AstNode *else_expr;
        } ternary;
    } u;
};

/* -----------------------------------------------------------------------
 * Construction helpers
 * ----------------------------------------------------------------------- */
AstNode *ast_new(NodeKind kind, int line, int col);
void     ast_free(AstNode *n);

/* Specialised constructors */
AstNode *ast_int_lit(int64_t v, int line, int col);
AstNode *ast_float_lit(double v, int line, int col);
AstNode *ast_bool_lit(int v, int line, int col);
AstNode *ast_string_lit(const char *s, int line, int col);
AstNode *ast_ident(const char *name, int line, int col);
AstNode *ast_func_ref(const char *name, int line, int col);
AstNode *ast_const(const char *name, int line, int col);
AstNode *ast_binop(OpKind op, AstNode *l, AstNode *r, int line, int col);
AstNode *ast_unop(OpKind op, AstNode *operand, int line, int col);
AstNode *ast_call(const char *name, NodeList args, int line, int col);
AstNode *ast_member(AstNode *base, const char *name, int line, int col);
AstNode *ast_method_call(AstNode *base, const char *name, NodeList args,
                         int is_super, int line, int col);
AstNode *ast_clone(const char *struct_name, int line, int col);
AstNode *ast_self(int line, int col);
AstNode *ast_super(int line, int col);

#endif /* CIMPLE_AST_H */
