/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- AST.
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_string.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_lexer.h"
#include "modules/webgl/src/wgl_builder.h"
#include "modules/webgl/src/wgl_context.h"
#include "modules/webgl/src/wgl_pretty_printer.h"
#include "modules/util/simset.h"

/* static */ void
WGL_Param::Show(WGL_PrettyPrinter *p, WGL_Param::Direction d)
{
    switch(d)
    {
    case WGL_Param::None:
        return;
    case WGL_Param::In:
        p->printer->OutString("in");
        return;
    case WGL_Param::Out:
        p->printer->OutString("out");
        return;
    case WGL_Param::InOut:
        p->printer->OutString("inout");
        return;
    }
}

WGL_Literal::WGL_Literal(WGL_Literal *l)
{
    type = l->type;
    value = l->value;
}

void
WGL_Literal::Show(WGL_PrettyPrinter *p)
{
    switch(type)
    {
    case WGL_Literal::Double:
        if (value.d.value_string)
            p->printer->OutString(value.d.value_string);
        else
            p->printer->OutDouble(value.d.value);
        break;
    case WGL_Literal::Int:
        p->printer->OutInt(value.i);
        break;
    case WGL_Literal::UInt:
        p->printer->OutInt(value.u_int);
        break;
    case WGL_Literal::Bool:
        p->printer->OutBool(value.b);
        break;
    case WGL_Literal::String:
        p->printer->OutString(value.s);
        break;
    default:
        OP_ASSERT(!"Unexpected literal type");
    }
}

/* static*/ WGL_Literal::Value
WGL_Literal::ToDouble(WGL_Literal::Type t, WGL_Literal::Value value)
{
    union WGL_Literal::Value v;
    v.d.value_string = NULL;
    switch (t)
    {
    case WGL_Literal::Double:
        return value;
    case WGL_Literal::Int:
        v.d.value = static_cast<double>(value.i);
        return v;
    case WGL_Literal::UInt:
        v.d.value = static_cast<double>(value.u_int);
        return v;
    case WGL_Literal::Bool:
        v.d.value = static_cast<double>(value.b);
        return v;
    default:
        OP_ASSERT(!"Unexpected literal type");
    case WGL_Literal::String:
        /* No implicit type conversions (from string) */
        v.d.value = 0;
        return v;
    }
}

/* static*/ WGL_Literal::Value
WGL_Literal::ToUnsignedInt(WGL_Literal::Type t, WGL_Literal::Value value)
{
    union WGL_Literal::Value v;
    switch (t)
    {
    case WGL_Literal::Double:
        v.u_int = static_cast<unsigned>(value.d.value);
        return value;
    case WGL_Literal::Int:
        v.u_int = static_cast<unsigned>(value.i);
        return v;
    case WGL_Literal::UInt:
        return value;
    case WGL_Literal::Bool:
        v.u_int = static_cast<unsigned>(value.b);
        return v;
    default:
        OP_ASSERT(!"Unexpected literal type");
    case WGL_Literal::String:
        v.u_int = 0;
        return v;
    }
}

/* static*/ WGL_Literal::Value
WGL_Literal::ToInt(WGL_Literal::Type t, WGL_Literal::Value value)
{
    union WGL_Literal::Value v;
    switch (t)
    {
    case WGL_Literal::Double:
        v.i = static_cast<int>(value.d.value);
        return v;
    case WGL_Literal::Int:
        return value;
    case WGL_Literal::UInt:
        v.i = static_cast<int>(value.u_int);
        return v;
    case WGL_Literal::Bool:
        v.i = static_cast<int>(value.b);
        return v;
    default:
        OP_ASSERT(!"Unexpected literal type");
    case WGL_Literal::String:
        v.i = 0;
        return v;
    }
}

/* static*/ WGL_Literal::Value
WGL_Literal::ToBool(WGL_Literal::Type t, WGL_Literal::Value value)
{
    union WGL_Literal::Value v;
    switch (t)
    {
    case WGL_Literal::Double:
        v.b = value.d.value != 0;
        return value;
    case WGL_Literal::Int:
        v.b = value.i != 0;
        return value;
    case WGL_Literal::UInt:
        v.b = value.u_int != 0;
    case WGL_Literal::Bool:
        return value;
    default:
        OP_ASSERT(!"Unexpected literal type");
    case WGL_Literal::String:
        v.b = FALSE;
        return v;
    }
}

void
WGL_LiteralVector::Show(WGL_PrettyPrinter *p)
{
    switch(element_type)
    {
    case WGL_BasicType::Float:
        p->printer->OutString("vec");
        break;
    case WGL_BasicType::Int:
        p->printer->OutString("ivec");
        break;
    case WGL_BasicType::UInt:
        p->printer->OutString("uvec");
        break;
    case WGL_BasicType::Bool:
        p->printer->OutString("bvec");
        break;
    default:
        OP_ASSERT(!"Unexpected vector literal type");
        return;
    }
    p->printer->OutInt(size);
    p->printer->OutString("(");

    for (unsigned i = 0; i < size; i++)
    {
        switch(element_type)
        {
        case WGL_BasicType::Float:
            p->printer->OutDouble(value.doubles[i]);
            break;
        case WGL_BasicType::Int:
            p->printer->OutInt(value.ints[i]);
            break;
        case WGL_BasicType::UInt:
            p->printer->OutInt(value.u_ints[i]);
            break;
        case WGL_BasicType::Bool:
            p->printer->OutBool(value.bools[i]);
            break;
        }
        if (size > 1 && i < (size - 1))
            p->printer->OutString(", ");
    }
    p->printer->OutString(")");
}

void
WGL_LiteralArray::Show(WGL_PrettyPrinter *p)
{
    switch(element_type)
    {
    case WGL_BasicType::Float:
    case WGL_BasicType::Int:
    case WGL_BasicType::UInt:
    case WGL_BasicType::Bool:
        break;
    default:
        OP_ASSERT(!"Unexpected array literal type");
        return;
    }
    p->printer->OutString("{");

    for (unsigned i = 0; i < size; i++)
    {
        switch(element_type)
        {
        case WGL_BasicType::Float:
            p->printer->OutDouble(value.doubles[i]);
            break;
        case WGL_BasicType::Int:
            p->printer->OutInt(value.ints[i]);
            break;
        case WGL_BasicType::UInt:
            p->printer->OutInt(value.u_ints[i]);
            break;
        case WGL_BasicType::Bool:
            p->printer->OutBool(value.bools[i]);
            break;
        }
        if (size > 1 && i < (size - 1))
            p->printer->OutString(", ");
    }
    p->printer->OutString("}");
}

void
WGL_Op::Show(WGL_PrettyPrinter *p, WGL_Expr::Operator op)
{
    switch(op)
    {
    case WGL_Expr::OpComma: p->printer->OutString(",");break;
    case WGL_Expr::OpEq:  p->printer->OutString("==");break;
    case WGL_Expr::OpNEq:  p->printer->OutString("!=");break;
    case WGL_Expr::OpLt:  p->printer->OutString("<");break;
    case WGL_Expr::OpGt:  p->printer->OutString(">");break;
    case WGL_Expr::OpLe:  p->printer->OutString("<=");break;
    case WGL_Expr::OpGe:  p->printer->OutString(">=");break;
    case WGL_Expr::OpAdd: p->printer->OutString("+");break;
    case WGL_Expr::OpSub: p->printer->OutString("-");break;
    case WGL_Expr::OpMul: p->printer->OutString("*");break;
    case WGL_Expr::OpDiv: p->printer->OutString("/");break;
    case WGL_Expr::OpPreInc:  p->printer->OutString("++");break;
    case WGL_Expr::OpPreDec:  p->printer->OutString("--");break;
    case WGL_Expr::OpNot:  p->printer->OutString("!");break;
    case WGL_Expr::OpNegate: p->printer->OutString("-");break;
    case WGL_Expr::OpPostInc: p->printer->OutString("++");break;
    case WGL_Expr::OpPostDec: p->printer->OutString("--");break;
    case WGL_Expr::OpOr: p->printer->OutString("||");break;
    case WGL_Expr::OpXor: p->printer->OutString("^^");break;
    case WGL_Expr::OpAnd: p->printer->OutString("&&");break;
    default: break;
    }
}

WGL_Expr::Precedence
WGL_Op::GetPrecedence(WGL_Expr::Operator op)
{
    switch(op)
    {
    case WGL_Expr::OpBase:
        return WGL_Expr::PrecLowest;

    case WGL_Expr::OpCond:
        return WGL_Expr::PrecCond;

    case WGL_Expr::OpComma:
        return WGL_Expr::PrecComma;

    case WGL_Expr::OpAssign:
        return WGL_Expr::PrecAssign;

    case WGL_Expr::OpOr:
        return WGL_Expr::PrecLogOr;

    case WGL_Expr::OpXor:
        return WGL_Expr::PrecLogXor;

    case WGL_Expr::OpAnd:
        return WGL_Expr::PrecLogAnd;

    case WGL_Expr::OpFieldSel:
        return WGL_Expr::PrecFieldSel;

    case WGL_Expr::OpEq:
        return WGL_Expr::PrecEq;

    case WGL_Expr::OpNEq:
        return WGL_Expr::PrecEq;

    case WGL_Expr::OpLt:
        return WGL_Expr::PrecRel;

    case WGL_Expr::OpGt:
        return WGL_Expr::PrecRel;

    case WGL_Expr::OpLe:
        return WGL_Expr::PrecRel;

    case WGL_Expr::OpGe:
        return WGL_Expr::PrecRel;

    case WGL_Expr::OpAdd:
        return WGL_Expr::PrecAdd;

    case WGL_Expr::OpSub:
        return WGL_Expr::PrecAdd;

    case WGL_Expr::OpMul:
        return WGL_Expr::PrecMul;

    case WGL_Expr::OpDiv:
        return WGL_Expr::PrecMul;

    case WGL_Expr::OpPreInc:
        return WGL_Expr::PrecPrefix;

    case WGL_Expr::OpPreDec:
        return WGL_Expr::PrecPrefix;

    case WGL_Expr::OpNot:
        return WGL_Expr::PrecPrefix;

    case WGL_Expr::OpNegate:
        return WGL_Expr::PrecPrefix;

    case WGL_Expr::OpPostInc:
        return WGL_Expr::PrecPostfix;

    case WGL_Expr::OpPostDec:
        return WGL_Expr::PrecPostfix;

    default:
        OP_ASSERT(!"Unhandled operator");
    case WGL_Expr::OpCall:
    case WGL_Expr::OpIndex:
        return WGL_Expr::PrecCall;
    }
}

WGL_Expr::Associativity
WGL_Op::GetAssociativity(WGL_Expr::Operator op)
{
    switch(op)
    {
    case WGL_Expr::OpComma:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpEq:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpNEq:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpLt:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpGt:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpLe:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpGe:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpAdd:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpSub:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpMul:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpDiv:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpPreInc:
        return WGL_Expr::AssocRight;

    case WGL_Expr::OpPreDec:
        return WGL_Expr::AssocRight;

    case WGL_Expr::OpNot:
        return WGL_Expr::AssocRight;

    case WGL_Expr::OpNegate:
        return WGL_Expr::AssocRight;

    case WGL_Expr::OpPostInc:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpPostDec:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpOr:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpXor:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpAnd:
        return WGL_Expr::AssocLeft;

    case WGL_Expr::OpCond:
        return WGL_Expr::AssocRight;

    case WGL_Expr::OpAssign:
        return WGL_Expr::AssocRight;

    case WGL_Expr::OpCall:
        return WGL_Expr::AssocNone;

    default:
        return WGL_Expr::AssocLeft;
    }
}

void
WGL_Op::Show(WGL_PrettyPrinter *p, WGL_TypeQualifier::Storage s)
{
    switch(s)
    {
    case WGL_TypeQualifier::NoStorage: return;
    case WGL_TypeQualifier::Const:
        p->printer->OutString("const");
        return;
    case WGL_TypeQualifier::Attribute:
        p->printer->OutString("attribute");
        return;
    case WGL_TypeQualifier::Varying:
        p->printer->OutString("varying");
        return;
    case WGL_TypeQualifier::Uniform:
        p->printer->OutString("uniform");
        return;
    case WGL_TypeQualifier::In:
        p->printer->OutString("in");
        return;
    case WGL_TypeQualifier::Out:
        p->printer->OutString("out");
        return;
    case WGL_TypeQualifier::InOut:
        p->printer->OutString("inout");
        return;
    }
}

void
WGL_Op::Show(WGL_PrettyPrinter *p, WGL_TypeQualifier::InvariantKind i)
{
    if (i==WGL_TypeQualifier::Invariant)
        p->printer->OutString("invariant");
}

/* static */ WGL_Expr *
WGL_Expr::Clone(WGL_Context *context, WGL_Expr *e)
{
    if (!e)
        return NULL;

    switch (e->GetType())
    {
    case WGL_Expr::Lit:
    {
        WGL_LiteralExpr *lit = static_cast<WGL_LiteralExpr *>(e);
        WGL_Literal *c = OP_NEWGRO_L(WGL_Literal, (lit->literal), context->Arena());
        return context->GetASTBuilder()->BuildLiteral(c);
    }
    case WGL_Expr::Var:
    {
        WGL_VarExpr *var = static_cast<WGL_VarExpr *>(e);
        return context->GetASTBuilder()->BuildIdentifier(var->name);
    }
    default:
        OP_ASSERT(!"Uncloneable expression node; cannot happen.");
        return NULL;
    }
}

/* static */ unsigned
WGL_ExprList::ComputeWidth(WGL_ExprList *list)
{
    unsigned size = 0;
    for (WGL_Expr *elem = list->list.First(); elem; elem = elem->Suc())
        size += WGL_Type::GetTypeSize(elem->GetExprType());

    return size;
}

/* virtual */
WGL_Type::~WGL_Type()
{
    if (is_heap_allocated)
        OP_DELETE(type_qualifier);
}

/* static */ void
WGL_Type::Show(WGL_PrettyPrinter *p, WGL_Type::Precision op)
{
    switch(op)
    {
    case WGL_Type::NoPrecision:
        break;

    case WGL_Type::Low:
        p->printer->OutString("lowp ");
        break;

    case WGL_Type::Medium:
        p->printer->OutString("mediump ");
        break;

    case WGL_Type::High:
        p->printer->OutString("highp ");
        break;
    }
}

/* virtual */
WGL_VectorType::~WGL_VectorType()
{
}

/* virtual */
WGL_SamplerType::~WGL_SamplerType()
{
}

/* virtual */
WGL_MatrixType::~WGL_MatrixType()
{
}

/* virtual */
WGL_BasicType::~WGL_BasicType()
{
}

/* virtual */
WGL_ArrayType::~WGL_ArrayType()
{
    if (is_heap_allocated)
        OP_DELETE(element_type);

    /* Somewhat artificial condition for this integrity check, but releasing
     * an array type should only occur when the type is used (and deleted)
     * as an output from the shader. */
    OP_ASSERT(is_constant_value && !length);
}

/* static */ void
WGL_ArrayType::ShowSizes(WGL_PrettyPrinter *printer, WGL_ArrayType *t)
{
    if (t->element_type->GetType() == WGL_Type::Array)
        ShowSizes(printer, static_cast<WGL_ArrayType *>(t->element_type));

    printer->printer->OutString("[");
    t->length->VisitExpr(printer);
    printer->printer->OutString("]");
}

/* virtual */
WGL_StructType::~WGL_StructType()
{
    if (is_heap_allocated)
    {
        WGL_String::Release(tag);
        OP_DELETE(fields);
    }
}

void
WGL_VarDecl::Show(WGL_PrettyPrinter *printer, BOOL without_precision)
{
    WGL_Type::Precision prec = type->precision;
    if (without_precision)
        type->precision = WGL_Type::NoPrecision;

    type->VisitType(printer);
    printer->printer->OutString(" ");
    printer->printer->OutString(identifier);
    if (initialiser)
    {
        printer->printer->OutString(" = ");
        initialiser->VisitExpr(printer);
    }
    printer->printer->OutString(";");
    type->precision = prec;
}

void
WGL_Type::Show(WGL_PrettyPrinter *printer)
{
    if (type_qualifier)
        type_qualifier->Show(printer);

    if (!implicit_precision)
        WGL_Type::Show(printer, precision);
}

WGL_TypeQualifier::~WGL_TypeQualifier()
{
    if (is_heap_allocated)
        OP_DELETE(layouts);
}

void
WGL_TypeQualifier::Show(WGL_PrettyPrinter *printer)
{
    WGL_Op::Show(printer, invariant);
    if (invariant != NoInvariant)
        printer->printer->OutString(" ");
    WGL_Op::Show(printer, storage);
    if (storage != NoStorage)
        printer->printer->OutString(" ");
    if (layouts)
    {
        layouts->VisitLayoutList(printer);
        printer->printer->OutString(" ");
    }
}

WGL_TypeQualifier*
WGL_TypeQualifier::AddInvariant(WGL_TypeQualifier::InvariantKind i)
{
    invariant = i;
    return this;
}

WGL_TypeQualifier*
WGL_TypeQualifier::AddStorage(WGL_TypeQualifier::Storage s)
{
    storage = s;
    return this;
}

WGL_TypeQualifier*
WGL_TypeQualifier::AddLayout(WGL_LayoutList *e)
{
    layouts = e;
    return this;
}

WGL_Type*
WGL_Type::AddTypeQualifier(WGL_TypeQualifier *tq)
{
    type_qualifier = tq;
    return this;
}

WGL_Type*
WGL_Type::AddPrecision(WGL_Type::Precision p)
{
    precision = p;
    return this;
}

WGL_Field::~WGL_Field()
{
    if (is_heap_allocated)
    {
        WGL_String::Release(name);
        OP_DELETE(type);
    }
}

void
WGL_FunctionPrototype::Show(WGL_PrettyPrinter *printer, BOOL as_proto)
{
    return_type->VisitType(printer);
    printer->printer->OutString(" ");
    printer->printer->OutString(name);

    printer->printer->OutString("(");
    parameters->VisitParamList(printer);
    if (as_proto)
        printer->printer->OutString(");");
    else
        printer->printer->OutString(")");
}

/* virtual */ WGL_FieldList *
WGL_FieldVisitor::VisitFieldList(WGL_FieldList *ls)
{
    for (WGL_Field *f = ls->list.First(); f; f = f->Suc())
        f = f->VisitField(this);

    return ls;
}

WGL_LayoutList::~WGL_LayoutList()
{
    if (is_heap_allocated)
        list.Clear();
}

/* static */ WGL_Param *
WGL_ParamList::FindParam(WGL_ParamList *list, WGL_VarName *i)
{
    for (WGL_Param *param = list->list.First(); param; param = param->Suc())
        if (i->Equals(param->name))
            return param;

    return NULL;
}

/* virtual */
WGL_NameType::~WGL_NameType()
{
    if (is_heap_allocated)
        WGL_String::Release(name);
}

/* virtual */ WGL_ExprList *
WGL_ExprVisitor::VisitExprList(WGL_ExprList *ls)
{
    for (WGL_Expr *e = ls->list.First(); e; e = e->Suc())
        e = e->VisitExpr(this);

    return ls;
}

/* virtual */ WGL_ParamList*
WGL_ParamVisitor::VisitParamList(WGL_ParamList *ls)
{
    for (WGL_Param *p = ls->list.First(); p; p = p->Suc())
        p = p->VisitParam(this);

    return ls;
}

/* virtual */ WGL_StmtList*
WGL_StmtVisitor::VisitStmtList(WGL_StmtList *ls)
{
    for (WGL_Stmt *s = ls->list.First(); s; s = s->Suc())
        s = s->VisitStmt(this);

    return ls;
}

/* virtual */ WGL_DeclList*
WGL_DeclVisitor::VisitDeclList(WGL_DeclList* ls)
{
    for (WGL_Decl *s = ls->list.First(); s; s = s->Suc())
        s = s->VisitDecl(this);

    return ls;
}

WGL_FieldList *
WGL_FieldList::Add(WGL_Context *context, WGL_Type *t, WGL_VarName *i)
{
    WGL_Field *f = OP_NEWGRO_L(WGL_Field, (i, t, FALSE), context->Arena());
    f->Into(&list);
    return this;
}

WGL_ParamList *
WGL_ParamList::Add(WGL_Context *context, WGL_Type *t, WGL_VarName *i)
{
    WGL_Param *p1 = OP_NEWGRO_L(WGL_Param, (t, i, WGL_Param::None), context->Arena());
    p1->Into(&list);
    return this;
}

void
WGL_DeclStmt::Add(WGL_Decl *d)
{
    d->Into(&decls);
}

/* static */ WGL_BasicType::Type
WGL_BasicType::GetBasicType(WGL_Type *t)
{
    if (!t)
    {
        /* Could (still) be a sign of incompleteness elsewhere, so assert to flush out. */
        OP_ASSERT(t);
        return WGL_BasicType::Void;
    }

    if (t->GetType() == WGL_Type::Basic)
    {
        WGL_BasicType *base_type = static_cast<WGL_BasicType *>(t);
        return base_type->type;
    }
    else
        return WGL_BasicType::Void;
}

/* static */ WGL_BasicType::Type
WGL_LiteralExpr::GetBasicType(WGL_LiteralExpr *e)
{
    if (!e || !e->literal)
    {
        /* Could (still) be a sign of incompleteness elsewhere, so assert to flush out. */
        OP_ASSERT(0);
        return WGL_BasicType::Void;
    }
    switch (e->literal->type)
    {
    case WGL_Literal::Double:
        return WGL_BasicType::Float;
    case WGL_Literal::Int:
    case WGL_Literal::UInt:
        return WGL_BasicType::Int;
    case WGL_Literal::Bool:
        return WGL_BasicType::Bool;
    default:
        return WGL_BasicType::Void;
    }
}

/* static */ WGL_BasicType::Type
WGL_VectorType::GetElementType(WGL_VectorType *vec)
{
    switch (vec->type)
    {
    case Vec2:
    case Vec3:
    case Vec4:
        return WGL_BasicType::Float;
    case BVec2:
    case BVec3:
    case BVec4:
        return WGL_BasicType::Bool;
    case IVec2:
    case IVec3:
    case IVec4:
        return WGL_BasicType::Int;
    case UVec2:
    case UVec3:
    case UVec4:
        return WGL_BasicType::UInt;
    default:
        OP_ASSERT(!"Unexpected vector kind");
        return WGL_BasicType::Float;
    }
}

/* static */ unsigned
WGL_VectorType::GetMagnitude(WGL_VectorType *vec)
{
    switch (vec->type)
    {
    case Vec2:
    case BVec2:
    case IVec2:
    case UVec2:
        return 2;
    case Vec3:
    case BVec3:
    case IVec3:
    case UVec3:
        return 3;
    case Vec4:
    case BVec4:
    case IVec4:
    case UVec4:
        return 4;
    default:
        OP_ASSERT(!"Unexpected vector kind");
        return 0;
    }
}

/* static */ BOOL
WGL_VectorType::SameElementType(WGL_VectorType *t1, WGL_VectorType *t2)
{
    return GetElementType(t1) == GetElementType(t2);
}

/* static */ BOOL
WGL_VectorType::HasElementType(WGL_VectorType *vec, WGL_BasicType::Type t)
{
    if (vec->type >= WGL_VectorType::Vec2 && vec->type <= WGL_VectorType::Vec4)
        return (t == WGL_BasicType::Float);
    else if (vec->type >= WGL_VectorType::BVec2 && vec->type <= WGL_VectorType::BVec4)
        return (t == WGL_BasicType::Bool);
    else if (vec->type >= WGL_VectorType::IVec2 && vec->type <= WGL_VectorType::IVec4)
        return (t == WGL_BasicType::Int);
    else if (vec->type >= WGL_VectorType::UVec2 && vec->type <= WGL_VectorType::UVec4)
        return (t == WGL_BasicType::UInt);
    else
    {
        OP_ASSERT(!"Unexpected vector kind");
        return FALSE;
    }
}

/* static */ WGL_VectorType::Type
WGL_VectorType::ToVectorType(WGL_BasicType::Type t, unsigned sz)
{
    OP_ASSERT(sz >= 2 && sz <= 4);
    switch (t)
    {
    case WGL_BasicType::Float:
        if (sz == 4)
            return Vec4;
        else if (sz == 3)
            return Vec3;
        else
            return Vec2;
    case WGL_BasicType::Int:
        if (sz == 4)
            return IVec4;
        else if (sz == 3)
            return IVec3;
        else
            return IVec2;
    case WGL_BasicType::UInt:
        if (sz == 4)
            return UVec4;
        else if (sz == 3)
            return UVec3;
        else
            return UVec2;
    case WGL_BasicType::Bool:
        if (sz == 4)
            return BVec4;
        else if (sz == 3)
            return BVec3;
        else
            return BVec2;
    default:
        OP_ASSERT(!"Unexpected vector type");
        return Vec2;
    }
}

/* static */ WGL_Literal::Type
WGL_BasicType::ToLiteralType(WGL_BasicType::Type t)
{
    switch (t)
    {
    case WGL_BasicType::Float:
        return WGL_Literal::Double;
    case WGL_BasicType::Int:
        return WGL_Literal::Int;
    case WGL_BasicType::UInt:
        return WGL_Literal::UInt;
    case WGL_BasicType::Bool:
        return WGL_Literal::Bool;
    default:
        OP_ASSERT(!"Unexpected basic type");
        return WGL_Literal::Double;
    }
}

/* static */ unsigned
WGL_MatrixType::GetRows(WGL_MatrixType *mat)
{
    switch (mat->type)
    {
    case Mat2:
    case Mat2x2:
    case Mat2x3:
    case Mat2x4:
        return 2;
    case Mat3:
    case Mat3x2:
    case Mat3x3:
    case Mat3x4:
        return 3;
    case Mat4:
    case Mat4x2:
    case Mat4x3:
    case Mat4x4:
        return 4;
    default:
        OP_ASSERT(!"Unexpected matrix kind");
        return 0;
    }
}

/* static */ unsigned
WGL_MatrixType::GetColumns(WGL_MatrixType *mat)
{
    switch (mat->type)
    {
    case Mat2:
    case Mat2x2:
    case Mat3x2:
    case Mat4x2:
        return 2;
    case Mat3:
    case Mat2x3:
    case Mat3x3:
    case Mat4x3:
        return 3;
    case Mat4:
    case Mat2x4:
    case Mat3x4:
    case Mat4x4:
        return 4;
    default:
        OP_ASSERT(!"Unexpected matrix kind");
        return 0;
    }
}

/* static */ BOOL
WGL_MatrixType::SameElementType(WGL_MatrixType *m1, WGL_MatrixType *m2)
{
    return GetElementType(m1) == GetElementType(m2);
}

/* static */ BOOL
WGL_MatrixType::HasElementType(WGL_MatrixType *mat, WGL_BasicType::Type t)
{
    return (t == WGL_BasicType::Float);
}

/* static */ WGL_BasicType::Type
WGL_MatrixType::GetElementType(WGL_MatrixType *mat)
{
    return WGL_BasicType::Float;
}


/* static */ BOOL
WGL_Type::HasBasicType(WGL_Type *t, unsigned basic_type_mask)
{
    if (t && t->GetType() == WGL_Type::Basic)
    {
        if (!basic_type_mask)
            return TRUE;

        WGL_BasicType *b = static_cast<WGL_BasicType *>(t);
        unsigned k = 0;
        while (k < 4)
        {
            if ((basic_type_mask & (0x1 << k)) != 0 && b->type == static_cast<WGL_BasicType::Type>(0x1 << k))
                return TRUE;
            k++;
        }
    }
    return FALSE;
}

/* static */ BOOL
WGL_Type::IsVectorType(WGL_Type *t, unsigned basic_type_mask)
{
    if (t && t->GetType() == WGL_Type::Vector)
    {
        if (!basic_type_mask)
            return TRUE;

        WGL_VectorType *v = static_cast<WGL_VectorType *>(t);
        unsigned k = 0;
        while (k < 4)
        {
            if ((basic_type_mask & (0x1 << k)) != 0 && WGL_VectorType::GetElementType(v) == static_cast<WGL_BasicType::Type>(0x1 << k))
                return TRUE;
            k++;
        }
    }
    return FALSE;
}

/* static */ BOOL
WGL_Type::IsMatrixType(WGL_Type *t, unsigned basic_type_mask)
{
    if (t && t->GetType() == WGL_Type::Matrix)
    {
        if (!basic_type_mask)
            return TRUE;

        WGL_MatrixType *mat = static_cast<WGL_MatrixType *>(t);
        unsigned k = 0;
        while (k < 4)
        {
            if ((basic_type_mask & (0x1 << k)) != 0 && WGL_MatrixType::GetElementType(mat) == static_cast<WGL_BasicType::Type>(0x1 << k))
                return TRUE;
            k++;
        }
    }
    return FALSE;
}

/* static */ WGL_Type *
WGL_Type::CopyL(WGL_Type *t)
{
    switch (t->GetType())
    {
    case Basic:
    {
        WGL_BasicType *existing = static_cast<WGL_BasicType *>(t);
        WGL_TypeQualifier *qual = NULL;
        if (existing->type_qualifier)
            qual = OP_NEW_L(WGL_TypeQualifier, (*existing->type_qualifier, TRUE));
        OpStackAutoPtr<WGL_TypeQualifier> anchor_qual(qual);

        WGL_BasicType *basic_type = OP_NEW_L(WGL_BasicType, (existing->type, TRUE));
        anchor_qual.release();

        basic_type->type_qualifier = qual;
        basic_type->precision = existing->precision;
        return basic_type;
    }
    case Vector:
    {
        WGL_VectorType *existing = static_cast<WGL_VectorType *>(t);
        WGL_TypeQualifier *qual = NULL;
        if (existing->type_qualifier)
            qual = OP_NEW_L(WGL_TypeQualifier, (*existing->type_qualifier, TRUE));
        OpStackAutoPtr<WGL_TypeQualifier> anchor_qual(qual);

        WGL_VectorType *vector_type = OP_NEW_L(WGL_VectorType, (existing->type, TRUE));
        anchor_qual.release();

        vector_type->type_qualifier = qual;
        vector_type->precision = existing->precision;
        return vector_type;
    }
    case Matrix:
    {
        WGL_MatrixType *existing = static_cast<WGL_MatrixType *>(t);
        WGL_TypeQualifier *qual = NULL;
        if (existing->type_qualifier)
            qual = OP_NEW_L(WGL_TypeQualifier, (*existing->type_qualifier, TRUE));

        OpStackAutoPtr<WGL_TypeQualifier> anchor_qual(qual);
        WGL_MatrixType *matrix_type = OP_NEW_L(WGL_MatrixType, (existing->type, TRUE));
        anchor_qual.release();

        matrix_type->type_qualifier = qual;
        matrix_type->precision = existing->precision;
        return matrix_type;
    }
    case Sampler:
    {
        WGL_SamplerType *existing = static_cast<WGL_SamplerType *>(t);
        WGL_TypeQualifier *qual = NULL;
        if (existing->type_qualifier)
            qual = OP_NEW_L(WGL_TypeQualifier, (*existing->type_qualifier, TRUE));

        OpStackAutoPtr<WGL_TypeQualifier> anchor_qual(qual);
        WGL_SamplerType *sampler_type = OP_NEW_L(WGL_SamplerType, (existing->type, TRUE));
        anchor_qual.release();

        sampler_type->type_qualifier = qual;
        sampler_type->precision = existing->precision;
        return sampler_type;
    }
    case Name:
    {
        WGL_NameType *existing = static_cast<WGL_NameType *>(t);
        WGL_TypeQualifier *qual = NULL;
        if (existing->type_qualifier)
            qual = OP_NEW_L(WGL_TypeQualifier, (*existing->type_qualifier, TRUE));

        OpStackAutoPtr<WGL_TypeQualifier> anchor_qual(qual);
        WGL_VarName *name = WGL_String::CopyL(existing->name);
        OpStackAutoPtr<WGL_VarName> anchor_name(name);

        WGL_NameType *name_type = OP_NEW_L(WGL_NameType, (name, TRUE));
        anchor_qual.release();
        anchor_name.release();

        name_type->type_qualifier = qual;
        name_type->precision = existing->precision;
        return name_type;
    }
    case Array:
    {
        WGL_ArrayType *existing = static_cast<WGL_ArrayType *>(t);
        WGL_TypeQualifier *qual = NULL;
        if (existing->type_qualifier)
            qual = OP_NEW_L(WGL_TypeQualifier, (*existing->type_qualifier, TRUE));

        OpStackAutoPtr<WGL_TypeQualifier> anchor_qual(qual);
        WGL_Type *element_type = WGL_Type::CopyL(existing->element_type);
        OpStackAutoPtr<WGL_Type> anchor_element_type(element_type);

        OP_ASSERT(existing->is_constant_value);
        WGL_ArrayType *array_type = OP_NEW_L(WGL_ArrayType, (element_type, NULL, TRUE));
        anchor_qual.release();
        anchor_element_type.release();

        array_type->type_qualifier = qual;
        array_type->precision = existing->precision;
        array_type->is_constant_value = TRUE;
        array_type->length_value = existing->length_value;
        return array_type;
    }
    case Struct:
    {
        WGL_StructType *existing = static_cast<WGL_StructType *>(t);
        WGL_TypeQualifier *qual = NULL;
        if (existing->type_qualifier)
            qual = OP_NEW_L(WGL_TypeQualifier, (*existing->type_qualifier, TRUE));
        OpStackAutoPtr<WGL_TypeQualifier> anchor_qual(qual);

        WGL_VarName *name = WGL_String::CopyL(existing->tag);
        OpStackAutoPtr<WGL_VarName> anchor_name(name);

        WGL_FieldList *fields = OP_NEW_L(WGL_FieldList, (TRUE));
        OpStackAutoPtr<WGL_FieldList> anchor_fields(fields);

        WGL_StructType *struct_type = OP_NEW_L(WGL_StructType, (name, fields, TRUE));
        anchor_name.release();
        anchor_fields.release();
        OpStackAutoPtr<WGL_StructType> anchor_struct_type(struct_type);

        struct_type->type_qualifier = qual;
        anchor_qual.release();
        struct_type->precision = existing->precision;

        for (WGL_Field *f = existing->fields->list.First(); f; f = f->Suc())
        {
            WGL_Type *type = WGL_Type::CopyL(f->type);
            OpStackAutoPtr<WGL_Type> anchor_type(type);

            WGL_VarName *field_name = WGL_String::CopyL(f->name);
            OpStackAutoPtr<WGL_VarName> anchor_field_name(field_name);

            WGL_Field *field = OP_NEW_L(WGL_Field, (field_name, type, TRUE));
            anchor_field_name.release();
            anchor_type.release();

            field->Into(&struct_type->fields->list);
        }
        anchor_struct_type.release();
        return struct_type;
    }
    default:
        OP_ASSERT(!"Unrecognised type; cannot happen.");
        return NULL;
    }
}

/* static */ unsigned
WGL_Type::GetTypeSize(WGL_Type *t)
{
    switch (t->GetType())
    {
    case WGL_Type::Basic:
        return 1;
    case WGL_Type::Vector:
        return WGL_VectorType::GetMagnitude(static_cast<WGL_VectorType *>(t));
    case WGL_Type::Matrix:
    {
        WGL_MatrixType *mat = static_cast<WGL_MatrixType *>(t);
        return (WGL_MatrixType::GetColumns(mat) * WGL_MatrixType::GetRows(mat));
    }
    case WGL_Type::Sampler:
        return 1;
    case WGL_Type::Name:
        OP_ASSERT(!"Unexpected name type");
        return 1;
    case WGL_Type::Array:
    {
        WGL_ArrayType *arr = static_cast<WGL_ArrayType *>(t);
        OP_ASSERT(arr->is_constant_value);
        return (arr->length_value * GetTypeSize(arr->element_type));
    }
    case WGL_Type::Struct:
    {
        WGL_StructType *struct_type = static_cast<WGL_StructType *>(t);
        unsigned int sz = 0;
        /* No interesting packing & alignment assumed. */
        for (WGL_Field *f = struct_type->fields ? struct_type->fields->list.First() : NULL; f; f = f->Suc())
            sz += GetTypeSize(f->type);

        return sz;
    }
    default:
        OP_ASSERT(!"Unexpected type");
        return 0;
    }
}

/* static */ WGL_FunctionDefn *
WGL_Decl::GetFunction(WGL_Decl *d, const uni_char *name)
{
    if (d->GetType() == WGL_Decl::Function)
    {
        WGL_FunctionDefn *fun = static_cast<WGL_FunctionDefn *>(d);
        if (fun->head->GetType() == WGL_Decl::Proto && static_cast<WGL_FunctionPrototype *>(fun->head)->name->Equals(name))
            return fun;
    }

    return NULL;
}

#endif // CANVAS3D_SUPPORT
