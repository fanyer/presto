/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- AST representation.
 *
 */
#ifndef WGL_AST_H
#define WGL_AST_H

#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_glsl.h"
#include "modules/util/simset.h"
#include "modules/webgl/src/wgl_symbol.h"

class WGL_PrettyPrinter;

class WGL_ExprVisitor;
class WGL_Expr;
class WGL_ExprList;
class WGL_LiteralExpr;
class WGL_VarExpr;
class WGL_TypeConstructorExpr;
class WGL_UnaryExpr;
class WGL_PostExpr;
class WGL_SeqExpr;
class WGL_BinaryExpr;
class WGL_CallExpr;
class WGL_CondExpr;
class WGL_IndexExpr;
class WGL_SelectExpr;
class WGL_AssignExpr;
class WGL_Type;

class WGL_ExprVisitor
{
public:
    virtual WGL_Expr *VisitExpr(WGL_LiteralExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_VarExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_TypeConstructorExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_UnaryExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_PostExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_SeqExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_BinaryExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_CondExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_CallExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_IndexExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_SelectExpr *e) = 0;
    virtual WGL_Expr *VisitExpr(WGL_AssignExpr *e) = 0;

    virtual WGL_ExprList *VisitExprList(WGL_ExprList *ls);
};

class WGL_Expr
    : public ListElement<WGL_Expr>
{
public:
    enum Type
    {
        None = 0,
        Lit,
        Var,
        TypeConstructor,
        Unary,
        PostOp,
        Seq,
        Binary,
        Cond,
        Call,
        Index,
        Select,
        Assign
    };

    WGL_Expr()
        : cached_type(NULL)
    {
    }

    virtual ~WGL_Expr()
    {
    }

    WGL_Type *GetExprType() { return cached_type; }
    /**< Return the recorded type for this expression. The
         static analyser / validator will annotate expressions
         with types, hence this will return NULL prior to (or during)
         it. */

    virtual WGL_Expr::Type GetType()
    {
        return None;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v) = 0;

    enum Precedence
    {
        /* Section 5.1 table */
        PrecLowest    = 0,
        PrecComma     = 1,
        PrecAssign    = 2,  // right assoc + all of them
        PrecCond      = 3,
        PrecLogOr     = 4,
        PrecLogXor    = 5,
        PrecLogAnd    = 6,
        PrecEq        = 7,
        PrecRel       = 8,
        PrecAdd       = 9,
        PrecMul       = 10,
        PrecPrefix    = 11, // right-associative.
        PrecPostfix   = 12,
        PrecCall      = 13,
        PrecFieldSel  = 14,
        PrecHighest   = 15
    };

    enum Associativity
    {
        AssocNone  = 0,
        AssocLeft  = 1,
        AssocRight = 2
    };

    /* Associativity in LSB, precedence above that. */
    typedef unsigned PrecAssoc;

    /* Expression operators, infix and postfix. */
    enum Operator
    {
        OpBase,
        OpCond,
        OpComma,
        OpAssign,

        OpOr,
        OpXor,
        OpAnd,
        OpFieldSel,

        OpEq,
        OpNEq,
        OpLt,
        OpGt,
        OpLe,
        OpGe,

        OpAdd,
        OpSub,
        OpMul,
        OpDiv,
        OpPreInc,
        OpPreDec,
        OpNot,
        OpNegate,
        OpPostInc,
        OpPostDec,
        OpCall,
        OpIndex,
        OpNop
    };

    static WGL_Expr *Clone(WGL_Context *context, WGL_Expr *e);
    /**< Clone the given expression into the environment (and allocator)
         associated with 'context'. OOM is signalled via the context
         if encountered and NULL returned. NULL may also be returned
         if the expression is not cloneable. */

    WGL_Type *cached_type;
    /**< If non-NULL, the computed type for this expression. */
};

class WGL_Literal
{
public:
    enum Type
    {
        Double = 1,
        Int,
        UInt,
        Bool,
        Vector,
        Matrix,
        Array,
        Struct,
        String
    };

    struct DoubleValue
    {
        double value;
        const uni_char *value_string;
    };

    union Value
    {
        DoubleValue d;
        int i;
        unsigned u_int;
        bool b;
        const uni_char *s;
    };

    Type type;
    Value value;

    WGL_Literal(Type t)
        : type(t)
     {
         // Initializing the largest element in the union to zero.
         value.d.value = 0;
         value.d.value_string = NULL;
     }

    WGL_Literal(double d, const uni_char *lit_str)
        : type(Double)

    {
        value.d.value = d;
        value.d.value_string = lit_str;
    }

    WGL_Literal(int i)
        : type(Int)
    {
        value.i = i;
    }

    WGL_Literal(unsigned ui)
        : type(UInt)
    {
        value.u_int = ui;
    }

    WGL_Literal(const uni_char *s)
        : type(String)
    {
        value.s = s; // no copying.
    }

    WGL_Literal(WGL_Literal *l);

    virtual void Show(WGL_PrettyPrinter *p);

    static Value ToDouble(Type t, Value v);
    static Value ToUnsignedInt(Type t, Value v);
    static Value ToInt(Type t, Value v);
    static Value ToBool(Type t, Value v);
};

class WGL_TypeQualifier;
class WGL_TypeVisitor;

class WGL_Type
    : public ListElement<WGL_Type>
{
public:
    enum Type
    {
        NoType = 0,
        Basic,
        Vector,
        Matrix,
        Sampler,
        Name,
        Struct,
        Array
    };

    /** GLSL precision attributes on types. */
    enum Precision
    {
        NoPrecision = 0,
        Low,
        Medium,
        High
    };

    WGL_Type(BOOL is_heap_allocated)
        : type_qualifier(NULL)
        , precision(NoPrecision)
        , implicit_precision(FALSE)
        , is_heap_allocated(is_heap_allocated)
    {
    }

    virtual ~WGL_Type();

    WGL_Type *AddPrecision(WGL_Type::Precision p);
    WGL_Type *AddTypeQualifier(WGL_TypeQualifier *tq);

    virtual Type GetType()
    {
        return NoType;
    }

    virtual WGL_Type *VisitType(WGL_TypeVisitor *v) = 0;

    WGL_TypeQualifier *type_qualifier;
    Precision precision;
    BOOL implicit_precision;
    /**< TRUE if the validator inserted the precision by resolving it. */

    BOOL is_heap_allocated;
    /**< If TRUE, delete type components upon free. */

    void Show(WGL_PrettyPrinter *printer);
    static void Show(WGL_PrettyPrinter *p, Precision op);

    static BOOL IsVectorType(WGL_Type *t, unsigned basic_type_mask = 0);
    static BOOL IsMatrixType(WGL_Type *t, unsigned basic_type_mask = 0);
    static BOOL HasBasicType(WGL_Type *t, unsigned basic_type_mask = 0);

    static WGL_Type *CopyL(WGL_Type *t);

    static unsigned GetTypeSize(WGL_Type *t);
};

class WGL_ArrayType;
class WGL_VectorType;
class WGL_MatrixType;
class WGL_SamplerType;
class WGL_NameType;
class WGL_StructType;
class WGL_BasicType;

class WGL_TypeVisitor
{
public:
    virtual WGL_Type *VisitType(WGL_BasicType *t) = 0;
    virtual WGL_Type *VisitType(WGL_ArrayType *t) = 0;
    virtual WGL_Type *VisitType(WGL_VectorType *t) = 0;
    virtual WGL_Type *VisitType(WGL_MatrixType *t) = 0;
    virtual WGL_Type *VisitType(WGL_SamplerType *t) = 0;
    virtual WGL_Type *VisitType(WGL_NameType *t) = 0;
    virtual WGL_Type *VisitType(WGL_StructType *t) = 0;
};

class WGL_BasicType
    : public WGL_Type
{
public:
    /** Builtin base types for GLSL. Along with these, there are also
     * the vector, matrix and sampler types. */
    enum Type
    {
        Void  = 0,
        Float = 1,
        Int   = 2,
        UInt  = 4,
        Bool  = 8
    };

    WGL_BasicType(Type ty, BOOL is_heap_allocated)
        : WGL_Type(is_heap_allocated)
        , type(ty)
    {
    }

    virtual ~WGL_BasicType();

    virtual WGL_Type::Type GetType()
    {
        return WGL_Type::Basic;
    }

    virtual WGL_Type *VisitType(WGL_TypeVisitor *v)
    {
        return v->VisitType(this);
    }

    static WGL_BasicType::Type GetBasicType(WGL_Type *t);
    static WGL_Literal::Type ToLiteralType(WGL_BasicType::Type t);

    Type type;
};

/** WGL_LiteralVector is constructed internally to hold vector constants;
    i.e., not directly generated by the frontend. */
class WGL_LiteralVector
    : public WGL_Literal
{
public:
    WGL_LiteralVector(WGL_BasicType::Type element_type)
        : WGL_Literal(WGL_Literal::Vector)
        , element_type(element_type)
    {
    }

    union Value
    {
        double *doubles;
        int *ints;
        unsigned *u_ints;
        bool *bools;
    };

    virtual void Show(WGL_PrettyPrinter *p);

    WGL_BasicType::Type element_type;
    Value value;
    unsigned size;
};

/** WGL_LiteralVector is constructed internally to hold array constants;
    not supported by the source language. */
class WGL_LiteralArray
    : public WGL_Literal
{
public:
    WGL_LiteralArray(WGL_BasicType::Type element_type)
        : WGL_Literal(WGL_Literal::Array)
        , element_type(element_type)
        , size(0)
    {
    }

    union Value
    {
        double *doubles;
        int *ints;
        unsigned *u_ints;
        bool *bools;
    };

    virtual void Show(WGL_PrettyPrinter *p);

    WGL_BasicType::Type element_type;
    Value value;
    unsigned size;
};

class WGL_LiteralExpr
    : public WGL_Expr
{
public:
    WGL_LiteralExpr(WGL_Literal *l)
        : literal(l)
    {
    }

    virtual ~WGL_LiteralExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return Lit;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    static WGL_BasicType::Type GetBasicType(WGL_LiteralExpr *e);

    WGL_Literal *literal;
};

class WGL_VarExpr
    : public WGL_Expr
{
public:
    WGL_VarExpr(WGL_VarName *n)
        : name(n)
        , const_name(NULL)
        , intrinsic(WGL_GLSL::NotAnIntrinsic)
    {
    }

    WGL_VarExpr(const uni_char *nm)
        : name(NULL)
        , const_name(nm)
        , intrinsic(WGL_GLSL::NotAnIntrinsic)
    {
    }

    virtual ~WGL_VarExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return Var;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_VarName *name;
    const uni_char *const_name;

    WGL_GLSL::Intrinsic intrinsic;
    /**< If > 0, the variable is a known GLSL intrinsic. The static analyzer
         will decorate the variables (in function application position) with
         this info. */
};

class WGL_TypeConstructorExpr
    : public WGL_Expr
{
public:
    WGL_TypeConstructorExpr(WGL_Type *t)
        : constructor(t)
    {
    }

    virtual ~WGL_TypeConstructorExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::TypeConstructor;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Type *constructor;
};

class WGL_UnaryExpr
    : public WGL_Expr
{
public:
    WGL_UnaryExpr(WGL_Expr::Operator o, WGL_Expr *e)
        : op(o)
        , arg(e)
    {
    }

    virtual ~WGL_UnaryExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::Unary;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Expr::Operator op;
    WGL_Expr *arg;
};

class WGL_PostExpr
    : public WGL_Expr
{
public:
    WGL_PostExpr(WGL_Expr::Operator o, WGL_Expr *e)
        : arg(e)
        , op(o)
    {
    }

    virtual ~WGL_PostExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::PostOp;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Expr *arg;
    WGL_Expr::Operator op;
};


class WGL_SeqExpr
    : public WGL_Expr
{
public:
    WGL_SeqExpr(WGL_Expr *e1, WGL_Expr *e2)
        : arg1(e1)
        , arg2(e2)
    {
    }

    virtual ~WGL_SeqExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::Seq;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Expr *arg1;
    WGL_Expr *arg2;
};

class WGL_BinaryExpr
    : public WGL_Expr
{
public:
    WGL_BinaryExpr(WGL_Expr::Operator o, WGL_Expr *e1, WGL_Expr *e2)
        : op(o)
        , arg1(e1)
        , arg2(e2)
    {
    }

    virtual ~WGL_BinaryExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::Binary;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Expr::Operator op;
    WGL_Expr *arg1;
    WGL_Expr *arg2;
};

class WGL_CondExpr
    : public WGL_Expr
{
public:
    WGL_CondExpr(WGL_Expr *e1, WGL_Expr *e2, WGL_Expr *e3)
        : arg1(e1)
        , arg2(e2)
        , arg3(e3)
    {
    }

    virtual ~WGL_CondExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::Cond;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Expr *arg1;
    WGL_Expr *arg2;
    WGL_Expr *arg3;
};

class WGL_ExprList
    : public ListElement<WGL_ExprList>
{
public:
    WGL_ExprList()
    {
    }

    WGL_ExprList *Add(WGL_Expr *e1)
    {
        e1->Into(&list);
        return this;
    }

    virtual WGL_ExprList *VisitExprList(WGL_ExprVisitor *s)
    {
        return s->VisitExprList(this);
    }

    static unsigned ComputeWidth(WGL_ExprList *list);

    List<WGL_Expr> list;
};

class WGL_CallExpr
    : public WGL_Expr
{
public:
    WGL_CallExpr(WGL_Expr *f, WGL_ExprList *a)
        : fun(f)
        , args(a)
    {
    }

    virtual ~WGL_CallExpr()
    {
        Out();
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::Call;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Expr *fun;
    WGL_ExprList *args;
};

class WGL_IndexExpr
    : public WGL_Expr
{
public:
    WGL_IndexExpr(WGL_Expr *e1, WGL_Expr *e2)
        : array(e1)
        , index(e2)
    {
    }

    virtual ~WGL_IndexExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::Index;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Expr *array;
    WGL_Expr *index;
};

class WGL_SelectExpr
    : public WGL_Expr
{
public:
    WGL_SelectExpr(WGL_Expr *rec, WGL_VarName *f)
        : value(rec)
        , field(f)
    {
    }

    virtual ~WGL_SelectExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::Select;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Expr *value;
    WGL_VarName *field;
};


class WGL_AssignExpr
    : public WGL_Expr
{
public:
    WGL_AssignExpr(WGL_Expr::Operator o, WGL_Expr *e1, WGL_Expr *e2)
        : op(o)
        , lhs(e1)
        , rhs(e2)
    {
    }

    virtual ~WGL_AssignExpr()
    {
    }

    virtual WGL_Expr::Type GetType()
    {
        return WGL_Expr::Assign;
    }

    virtual WGL_Expr *VisitExpr(WGL_ExprVisitor *v)
    {
        return v->VisitExpr(this);
    }

    WGL_Expr::Operator op;
    WGL_Expr *lhs;
    WGL_Expr *rhs;
};

class WGL_Stmt;
class WGL_StmtList;
class WGL_SwitchStmt;
class WGL_SimpleStmt;
class WGL_WhileStmt;
class WGL_BodyStmt;
class WGL_DoStmt;
class WGL_IfStmt;
class WGL_ForStmt;
class WGL_ReturnStmt;
class WGL_ExprStmt;
class WGL_DeclStmt;
class WGL_CaseLabel;

class WGL_StmtVisitor
{
public:
    virtual WGL_Stmt *VisitStmt(WGL_SwitchStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_SimpleStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_BodyStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_WhileStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_DoStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_IfStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_ForStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_ReturnStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_ExprStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_DeclStmt *s) = 0;
    virtual WGL_Stmt *VisitStmt(WGL_CaseLabel *s) = 0;

    virtual WGL_StmtList *VisitStmtList(WGL_StmtList *ls);
};

class WGL_StmtList;

class WGL_Stmt
    : public ListElement<WGL_Stmt>
{
public:
    enum Type
    {
        StmtNone = 0,
        Switch,
        Simple,
        Body,
        While,
        Do,
        StmtDecl,
        StmtLabel,
        For,
        Return,
        StmtExpr,
        StmtIf
    };

    WGL_Stmt()
        : line_number(0)
    {
    }

    virtual Type GetType()
    {
        return StmtNone;
    }
    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v) = 0;

    unsigned line_number;
};

class WGL_StmtList
    : public ListElement<WGL_StmtList>
{
public:
    WGL_StmtList()
    {
    }

    WGL_StmtList *Add(WGL_Stmt *s1 )
    {
        s1->Into(&list);
        return this;
    }

    virtual WGL_StmtList *VisitStmtList(WGL_StmtVisitor *s)
    {
        return s->VisitStmtList(this);
    }

    List<WGL_Stmt> list;
};

class WGL_SwitchStmt
    : public WGL_Stmt
{
public:
    WGL_SwitchStmt(WGL_Expr *e1, WGL_StmtList *ss)
        : scrutinee(e1)
        , cases(ss)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::Switch;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    WGL_Expr *scrutinee;
    WGL_StmtList *cases;
};

class WGL_SimpleStmt
    : public WGL_Stmt
{
public:
    enum Type
    {
        Empty = 1,
        Default,
        Continue,
        Break,
        Discard
    };

    WGL_SimpleStmt(Type k)
        : type(k)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::Simple;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    Type type;
};

class WGL_BodyStmt
    : public WGL_Stmt
{
public:
    WGL_BodyStmt(WGL_StmtList *ss)
        : body(ss)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::Body;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    WGL_StmtList *body;
};

class WGL_WhileStmt
    : public WGL_Stmt
{
public:
    WGL_WhileStmt(WGL_Expr *e1, WGL_Stmt *b)
        : condition(e1)
        , body(b)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::While;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    WGL_Expr *condition;
    WGL_Stmt *body;
};

class WGL_DoStmt
    : public WGL_Stmt
{
public:
    WGL_DoStmt(WGL_Stmt *s, WGL_Expr *e1)
        : body(s)
        , condition(e1)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::Do;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    WGL_Stmt *body;
    WGL_Expr *condition;
};

class WGL_Decl;
class WGL_DeclList;
class WGL_FunctionPrototype;
class WGL_PrecisionDefn;
class WGL_ArrayDecl;
class WGL_VarDecl;
class WGL_TypeDecl;
class WGL_InvariantDecl;
class WGL_FunctionDefn;

class WGL_DeclVisitor
{
public:
    virtual WGL_Decl *VisitDecl(WGL_FunctionPrototype *d) = 0;
    virtual WGL_Decl *VisitDecl(WGL_PrecisionDefn *d) = 0;
    virtual WGL_Decl *VisitDecl(WGL_ArrayDecl *d) = 0;
    virtual WGL_Decl *VisitDecl(WGL_VarDecl *d) = 0;
    virtual WGL_Decl *VisitDecl(WGL_TypeDecl *d) = 0;
    virtual WGL_Decl *VisitDecl(WGL_InvariantDecl *d) = 0;
    virtual WGL_Decl *VisitDecl(WGL_FunctionDefn *d) = 0;

    virtual WGL_DeclList *VisitDeclList(WGL_DeclList *ls);
};

class WGL_CaseLabel
    : public WGL_Stmt
{
public:
    WGL_CaseLabel(WGL_Expr *e1)
      : condition(e1)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::StmtLabel;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    WGL_Expr *condition;
    /**< The 'case' expression; NULL being used to represent 'default'. */
};

class WGL_Decl
    : public ListElement<WGL_Decl>
{
public:
    enum Type
    {
        DeclNone = 0,
        Invariant,
        TypeDecl,
        Var,
        Array,
        Function,
        PrecisionDecl,
        Proto
    };

    WGL_Decl()
        : line_number(0)
    {
    }

    unsigned line_number;

    virtual Type GetType()
    {
        return DeclNone;
    }

    virtual WGL_Decl *VisitDecl(WGL_DeclVisitor *d) = 0;

    static WGL_FunctionDefn *GetFunction(WGL_Decl *d, const uni_char *name);
    /**< If 'd' is a function definition with the given name, return it.
         Otherwise NULL. */
};

class WGL_DeclStmt
    : public WGL_Stmt
{
public:
    WGL_DeclStmt()
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::StmtDecl;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    void Add(WGL_Decl *d);

    List<WGL_Decl> decls;
};

class WGL_ForStmt
    : public WGL_Stmt
{
public:
    WGL_ForStmt(WGL_Stmt *d, WGL_Expr *e1, WGL_Expr *e2, WGL_Stmt *s)
        : head(d)
        , predicate(e1)
        , update(e2)
        , body(s)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::For;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    WGL_Stmt *head;
    WGL_Expr *predicate;
    WGL_Expr *update;
    WGL_Stmt *body;
};

class WGL_ReturnStmt
    : public WGL_Stmt
{
public:
    WGL_ReturnStmt(WGL_Expr *e1)
        : value(e1)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::Return;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    WGL_Expr *value;
};

class WGL_ExprStmt
    : public WGL_Stmt
{
public:
    WGL_ExprStmt(WGL_Expr *e1)
        : expr(e1)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::StmtExpr;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    WGL_Expr *expr;
};

class WGL_VectorType
    : public WGL_Type
{
public:
    enum Type
    {
        /* Note: the relative ordering of these tags MUST mirror
           those defined for tokens (e.g., TYPE_xxx). We must also have
           Vec<m> + <n> = Vec<m + n> and similarly for BVec, etc. (See
           WGL_ValidationState::GetTypeForSwizzle().) */
        Vec2 = 1,
        Vec3,
        Vec4,
        BVec2,
        BVec3,
        BVec4,
        IVec2,
        IVec3,
        IVec4,
        UVec2,
        UVec3,
        UVec4
    };

    WGL_VectorType(Type ty, BOOL is_heap_allocated)
        : WGL_Type(is_heap_allocated)
        , type(ty)
    {
    }

    virtual ~WGL_VectorType();

    virtual WGL_Type::Type GetType()
    {
        return Vector;
    }

    virtual WGL_Type *VisitType(WGL_TypeVisitor *v)
    {
        return v->VisitType(this);
    }

    static WGL_BasicType::Type GetElementType(WGL_VectorType *vec);
    static BOOL SameElementType(WGL_VectorType *t1, WGL_VectorType *t2);
    static BOOL HasElementType(WGL_VectorType *vec, WGL_BasicType::Type t);
    static unsigned GetMagnitude(WGL_VectorType *vec);
    static WGL_VectorType::Type ToVectorType(WGL_BasicType::Type t, unsigned size);

    Type type;
};

class WGL_MatrixType
    : public WGL_Type
{
public:
    enum Type
    {
        /* Note: the relative ordering of these tags MUST mirror
           those defined for tokens (e.g., TYPE_xxx) */
        Mat2 = 1,
        Mat3,
        Mat4,
        Mat2x2,
        Mat2x3,
        Mat2x4,
        Mat3x2,
        Mat3x3,
        Mat3x4,
        Mat4x2,
        Mat4x3,
        Mat4x4
    };

    WGL_MatrixType(Type ty, BOOL is_heap_allocated)
        : WGL_Type(is_heap_allocated)
        , type(ty)
    {
    }

    virtual ~WGL_MatrixType();

    virtual WGL_Type::Type GetType()
    {
        return Matrix;
    }

    virtual WGL_Type *VisitType(WGL_TypeVisitor *v)
    {
        return v->VisitType(this);
    }

    static WGL_BasicType::Type GetElementType(WGL_MatrixType *mat);
    static BOOL SameElementType(WGL_MatrixType *m1, WGL_MatrixType *m2);
    static BOOL HasElementType(WGL_MatrixType *mat, WGL_BasicType::Type t);
    static unsigned GetColumns(WGL_MatrixType *mat);
    static unsigned GetRows(WGL_MatrixType *mat);

    Type type;
};

class WGL_SamplerType
    : public WGL_Type
{
public:
    enum Type
    {
        /* Same comment as above applies here. */
        Sampler1D = 1,
        Sampler2D,
        Sampler3D,
        SamplerCube,
        Sampler1DShadow,
        Sampler2DShadow,
        SamplerCubeShadow,
        Sampler1DArray,
        Sampler2DArray,
        Sampler1DArrayShadow,
        Sampler2DArrayShadow,
        SamplerBuffer,
        Sampler2DMS,
        Sampler2DMSArray,
        Sampler2DRect,
        Sampler2DRectShadow,
        ISampler1D,
        ISampler2D,
        ISampler3D,
        ISamplerCube,
        ISampler1DArray,
        ISampler2DArray,
        ISampler2DRect,
        ISamplerBuffer,
        ISampler2DMS,
        ISampler2DMSArray,
        USampler1D,
        USampler2D,
        USampler3D,
        USamplerCube,
        USampler1DArray,
        USampler2DArray,
        USampler2DRect,
        USamplerBuffer,
        USampler2DMS,
        USampler2DMSArray
    };

    WGL_SamplerType(Type ty, BOOL is_heap_allocated)
        : WGL_Type(is_heap_allocated)
        , type(ty)
    {
    }

    virtual ~WGL_SamplerType();

    virtual WGL_Type::Type GetType()
    {
        return Sampler;
    }

    virtual WGL_Type *VisitType(WGL_TypeVisitor *v)
    {
        return v->VisitType(this);
    }

    Type type;
};

class WGL_NameType
    : public WGL_Type
{
public:
    WGL_NameType(WGL_VarName *n, BOOL is_heap_allocated)
        : WGL_Type(is_heap_allocated)
        , name(n)
    {
    }

    virtual ~WGL_NameType();

    virtual WGL_Type::Type GetType()
    {
        return WGL_Type::Name;
    }

    virtual WGL_Type *VisitType(WGL_TypeVisitor *v)
    {
        return v->VisitType(this);
    }

    WGL_VarName *name;
};

class WGL_IfStmt
    : public WGL_Stmt
{
public:
    WGL_IfStmt(WGL_Expr *e1, WGL_Stmt *s1, WGL_Stmt *s2)
        : predicate(e1)
        , then_part(s1)
        , else_part(s2)
    {
    }

    virtual WGL_Stmt::Type GetType()
    {
        return WGL_Stmt::StmtIf;
    }

    virtual WGL_Stmt *VisitStmt(WGL_StmtVisitor *v)
    {
        return v->VisitStmt(this);
    }

    WGL_Expr *predicate;
    WGL_Stmt *then_part;
    WGL_Stmt *else_part;
};

class WGL_Field;
class WGL_FieldList;

class WGL_FieldVisitor
{
public:
    virtual WGL_Field *VisitField(WGL_Field *d) { return d; }
    virtual WGL_FieldList *VisitFieldList(WGL_FieldList *ls);
};

class WGL_Field
    : public ListElement<WGL_Field>
{
public:
    WGL_Field(WGL_VarName *v, WGL_Type *t, BOOL is_heap_allocated)
        : name(v)
        , type(t)
        , is_heap_allocated(is_heap_allocated)
    {
    }

    ~WGL_Field();

    virtual WGL_Field *VisitField(WGL_FieldVisitor *f)
    {
        return f->VisitField(this);
    }

    WGL_VarName *name;
    WGL_Type *type;

    BOOL is_heap_allocated;
    /**< If TRUE, delete type components upon free. */
};

class WGL_FieldList
    : public ListElement<WGL_FieldList>
{
public:
    WGL_FieldList(BOOL is_heap_allocated)
        : is_heap_allocated(is_heap_allocated)
    {
    }

    ~WGL_FieldList()
    {
        if (is_heap_allocated)
            list.Clear();
    }

    WGL_FieldList *Add(WGL_Context *context, WGL_Type *t, WGL_VarName *i);

    virtual WGL_FieldList *VisitFieldList(WGL_FieldVisitor *s)
    {
        return s->VisitFieldList(this);
    }

    List<WGL_Field> list;

    BOOL is_heap_allocated;
    /**< If TRUE, delete type components upon free. */
};

class WGL_StructType
    : public WGL_Type
{
public:
    WGL_StructType(WGL_VarName *tag, WGL_FieldList *f, BOOL is_heap_allocated)
        : WGL_Type(is_heap_allocated)
        , tag(tag)
        , fields(f)
    {
    }

    virtual ~WGL_StructType();

    virtual WGL_Type::Type GetType()
    {
        return WGL_Type::Struct;
    }

    virtual WGL_Type *VisitType(WGL_TypeVisitor *v)
    {
        return v->VisitType(this);
    }

    WGL_VarName *tag;
    WGL_FieldList *fields;
};

class WGL_ArrayType
    : public WGL_Type
{
public:
    WGL_ArrayType(WGL_Type *t, WGL_Expr *e, BOOL is_heap_allocated)
        : WGL_Type(is_heap_allocated)
        , element_type(t)
        , length(e)
        , is_constant_value(FALSE)
        , length_value(0)
    {
    }

    virtual ~WGL_ArrayType();

    virtual WGL_Type::Type GetType()
    {
        return WGL_Type::Array;
    }

    virtual WGL_Type *VisitType(WGL_TypeVisitor *v)
    {
        return v->VisitType(this);
    }

    static void ShowSizes(WGL_PrettyPrinter *printer, WGL_ArrayType *t);

    WGL_Type *element_type;
    WGL_Expr *length;
    BOOL is_constant_value;
    /**< TRUE => length_value has the constant (folded) array length. */

    unsigned length_value;
    /**< If is_constant_value is TRUE, holds the number of elements in the
         array. */
};

class WGL_InvariantDecl
    : public WGL_Decl
{
public:
    WGL_InvariantDecl(WGL_VarName *i)
        : var_name(i)
        , next(NULL)
    {
    }

    virtual WGL_Decl::Type GetType()
    {
        return WGL_Decl::Invariant;
    }

    virtual WGL_Decl *VisitDecl(WGL_DeclVisitor *v)
    {
        return v->VisitDecl(this);
    }

    WGL_VarName *var_name;
    WGL_InvariantDecl *next;
};

class WGL_TypeDecl
    : public WGL_Decl
{
public:
    WGL_TypeDecl(WGL_Type *t, WGL_VarName *i)
        : type(t)
        , var_name(i)
    {
    }

    virtual WGL_Decl::Type GetType()
    {
        return WGL_Decl::TypeDecl;
    }

    virtual WGL_Decl *VisitDecl(WGL_DeclVisitor *v)
    {
        return v->VisitDecl(this);
    }

    WGL_Type *type;
    WGL_VarName *var_name;
};

class WGL_VarDecl
    : public WGL_Decl
{
public:
    WGL_VarDecl(WGL_Type *t, WGL_VarName *i, WGL_Expr *e)
        : type(t)
        , identifier(i)
        , initialiser(e)
    {
    }

    virtual WGL_Decl::Type GetType()
    {
        return WGL_Decl::Var;
    }

    virtual WGL_Decl *VisitDecl(WGL_DeclVisitor *v)
    {
        return v->VisitDecl(this);
    }

    void Show(WGL_PrettyPrinter *printer, BOOL with_no_precision);

    WGL_Type *type;
    WGL_VarName *identifier;
    WGL_Expr *initialiser;
};

class WGL_ArrayDecl
    : public WGL_Decl
{
public:
    WGL_ArrayDecl(WGL_Type *t, WGL_VarName *i, WGL_Expr *e1, WGL_Expr *e2)
        : type(t)
        , identifier(i)
        , size(e1)
        , initialiser(e2)
    {
    }

    virtual WGL_Decl::Type GetType()
    {
        return WGL_Decl::Array;
    }

    virtual WGL_Decl *VisitDecl(WGL_DeclVisitor *v)
    {
        return v->VisitDecl(this);
    }

    WGL_Type *type;
    WGL_VarName *identifier;
    WGL_Expr *size;
    WGL_Expr *initialiser;
};

class WGL_LayoutPair;
class WGL_LayoutList;

class WGL_LayoutVisitor
{
public:
    virtual WGL_LayoutPair *VisitLayout(WGL_LayoutPair *d)
    {
        return d;
    }

    virtual WGL_LayoutList *VisitLayoutList(WGL_LayoutList *ls) = 0;
};

class WGL_LayoutPair
    : public ListElement<WGL_LayoutPair>
{
public:
    WGL_LayoutPair(WGL_VarName *i, int v)
        : identifier(i)
        , value(v)
    {
    }

    virtual WGL_LayoutPair *VisitLayout(WGL_LayoutVisitor *v)
    {
        return v->VisitLayout(this);
    }

    WGL_VarName *identifier;
    int value;
};

/** Layout qualifiers are not part of the shader language in GLES2.0 (part of OpenGL GLSL 1.50.)
 *  Keep the class and representation in this AST. */
class WGL_LayoutList
    : public ListElement<WGL_LayoutList>
{
public:
    WGL_LayoutList(BOOL is_heap_allocated)
        : is_heap_allocated(is_heap_allocated)
    {
    }

    ~WGL_LayoutList();

    WGL_LayoutList *Add(WGL_LayoutPair *l1)
    {
        l1->Into(&list);
        return this;
    }

    virtual WGL_LayoutList *VisitLayoutList(WGL_LayoutVisitor *l)
    {
        return l->VisitLayoutList(this);
    }

    List<WGL_LayoutPair> list;

    BOOL is_heap_allocated;
    /**< If TRUE, delete type components upon free. */
};


class WGL_TypeQualifier
{
public:
    enum Storage
    {
        NoStorage = 0,
        Const,
        Attribute,
        Varying,
        Uniform,
        In,
        Out,
        InOut
    };

    enum InvariantKind
    {
        NoInvariant = 0,
        Invariant
    };

    WGL_TypeQualifier(BOOL is_heap_allocated)
        : storage(NoStorage)
        , layouts(NULL)
        , invariant(NoInvariant)
        , is_heap_allocated(is_heap_allocated)
    {
    }

    WGL_TypeQualifier(const WGL_TypeQualifier &ty, BOOL is_heap_allocated)
        : storage(ty.storage)
        , layouts(NULL)
        , invariant(ty.invariant)
        , is_heap_allocated(is_heap_allocated)
    {
    }

    ~WGL_TypeQualifier();

    WGL_TypeQualifier *AddInvariant(InvariantKind i);
    WGL_TypeQualifier *AddStorage(Storage s);
    WGL_TypeQualifier *AddLayout(WGL_LayoutList *l);

    void Show(WGL_PrettyPrinter *printer);

    Storage storage;
    WGL_LayoutList *layouts;
    InvariantKind invariant;

    BOOL is_heap_allocated;
    /**< If TRUE, delete type components upon free. */
};

class WGL_FunctionDefn
    : public WGL_Decl
{
public:
    WGL_FunctionDefn(WGL_Decl *d, WGL_Stmt *s)
        : head(d)
        , body(s)
    {
    }

    virtual WGL_Decl::Type GetType()
    {
        return WGL_Decl::Function;
    }

    virtual WGL_Decl *VisitDecl(WGL_DeclVisitor *v)
    {
        return v->VisitDecl(this);
    }

    WGL_Decl *head;
    WGL_Stmt *body;
};

class WGL_PrecisionDefn
    : public WGL_Decl
{
public:
    WGL_PrecisionDefn(WGL_Type::Precision p, WGL_Type *t)
        : precision(p)
        , type(t)
    {
    }

    virtual WGL_Decl::Type GetType()
    {
        return WGL_Decl::PrecisionDecl;
    }

    virtual WGL_Decl *VisitDecl(WGL_DeclVisitor *v)
    {
        return v->VisitDecl(this);
    }

    WGL_Type::Precision precision;
    WGL_Type *type;
};

class WGL_Param;
class WGL_ParamList;

class WGL_ParamVisitor
{
public:
    virtual WGL_Param *VisitParam(WGL_Param *d)
    {
        return d;
    }

    virtual WGL_ParamList *VisitParamList(WGL_ParamList *ls);
};

class WGL_Param
    : public ListElement<WGL_Param>
{
public:
    enum Direction
    {
        None = 0,
        In,
        Out,
        InOut
    };

    WGL_Param(WGL_Type *t, WGL_VarName *i1, Direction d)
        : type(t)
        , name(i1)
        , direction(d)
    {
    }

    virtual WGL_Param *VisitParam(WGL_ParamVisitor *d)
    {
        return d->VisitParam(this);
    }

    static void Show(WGL_PrettyPrinter *p, Direction d);

    WGL_Type *type;
    WGL_VarName *name;
    Direction direction;
};

class WGL_ParamList
    : public ListElement<WGL_ParamList>
{
public:
    WGL_ParamList()
    {
    }

    WGL_ParamList *Add(WGL_Context *context, WGL_Type *t, WGL_VarName *i);

    WGL_ParamList *Add(WGL_Param *p1)
    {
        p1->Into(&list);
        return this;
    }

    virtual WGL_ParamList *VisitParamList(WGL_ParamVisitor *d)
    {
        return d->VisitParamList(this);
    }

    static WGL_Param *FindParam(WGL_ParamList *list, WGL_VarName *i);

    List<WGL_Param> list;
};

class WGL_FunctionPrototype
    : public WGL_Decl
{
public:
    WGL_FunctionPrototype(WGL_Type *t, WGL_VarName *i, WGL_ParamList *ps)
        : return_type(t)
        , name(i)
        , parameters(ps)
    {
    }

    virtual WGL_Decl::Type GetType()
    {
        return WGL_Decl::Proto;
    }

    virtual WGL_Decl *VisitDecl(WGL_DeclVisitor *v)
    {
        return v->VisitDecl(this);
    }

    void Show(WGL_PrettyPrinter *printer, BOOL as_proto);

    WGL_Type *return_type;
    WGL_VarName *name;
    WGL_ParamList *parameters;
};

class WGL_DeclList
    : public ListElement<WGL_DeclList>
{
public:
    WGL_DeclList()
    {
    }

    WGL_DeclList *Add(WGL_Decl *d1)
    {
        d1->Into(&list);
        return this;
    }

    virtual WGL_DeclList *VisitDeclList(WGL_DeclVisitor *s)
    {
        return s->VisitDeclList(this);
    }

    List<WGL_Decl> list;
};

class WGL_Op
{
public:
    static void Show(WGL_PrettyPrinter *p, WGL_Expr::Operator op);
    static void Show(WGL_PrettyPrinter *p, WGL_TypeQualifier::Storage s);
    static void Show(WGL_PrettyPrinter *p, WGL_TypeQualifier::InvariantKind i);

    static WGL_Expr::Precedence GetPrecedence(WGL_Expr::Operator op);
    static WGL_Expr::Associativity GetAssociativity(WGL_Expr::Operator op);
    static WGL_Expr::PrecAssoc ToPrecAssoc(WGL_Expr::Precedence p, WGL_Expr::Associativity a) { return ( (static_cast<unsigned>(a) & 0xff) | (static_cast<unsigned>(p) << 0x8)); }
    static WGL_Expr::Precedence GetPrecAssocP(WGL_Expr::PrecAssoc pa) { return ((WGL_Expr::Precedence)(pa >> 0x8)); }
    static WGL_Expr::Associativity GetPrecAssocA(WGL_Expr::PrecAssoc pa) { return ((WGL_Expr::Associativity)(pa & 0xff)); }
};

#endif // WGL_AST_H
