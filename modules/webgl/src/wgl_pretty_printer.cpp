/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- pretty printer.
 *
 */

#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_pretty_printer.h"

/* virtual */ WGL_Decl *
WGL_PrettyPrinter::VisitDecl(WGL_ArrayDecl *d)
{
    d->type->VisitType(this);
    printer->OutString(" ");
    printer->OutString(d->identifier);
    OP_ASSERT(d->size && "Got unsized array. Parser shouldn't have let it through.");
    if (d->size)
    {
        printer->OutString("[");
        d->size->VisitExpr(this);
        printer->OutString("]");
    }
    else
        printer->OutString("[]");
    if (d->initialiser)
    {
        printer->OutString(" = ");
        d->initialiser->VisitExpr(this);
    }
    printer->OutString(";");

    return d;
}

/* virtual */ WGL_Decl *
WGL_PrettyPrinter::VisitDecl(WGL_FunctionDefn *d)
{
    if (d->head && d->head->GetType() == WGL_Decl::Proto)
        static_cast<WGL_FunctionPrototype *>(d->head)->Show(this, FALSE);
    printer->OutNewline();
    if (d->body)
        d->body->VisitStmt(this);

    return d;
}

/* virtual */ WGL_Decl *
WGL_PrettyPrinter::VisitDecl(WGL_FunctionPrototype *d)
{
    d->Show(this, TRUE);
    return d;
}

/* virtual */ WGL_Decl *
WGL_PrettyPrinter::VisitDecl(WGL_InvariantDecl *d)
{
    printer->OutString("invariant ");
    printer->OutString(d->var_name);
    WGL_InvariantDecl *iv = d->next;
    while (iv)
    {
        printer->OutString(", ");
        printer->OutString(iv->var_name);
        iv = iv->next;
    }
    printer->OutString(";");

    return d;
}

/* virtual */ WGL_Decl *
WGL_PrettyPrinter::VisitDecl(WGL_PrecisionDefn *d)
{
    if (printer->ForGLESTarget())
    {
        printer->OutString("precision ");
        // Downgrade precision if we're targeting a non high precision platform.
        WGL_Type::Precision prec = d->precision == WGL_Type::High && !printer->ForHighpTarget() ? WGL_Type::Medium : d->precision;
        WGL_Type::Show(this, prec);
        d->type->VisitType(this);
        printer->OutString(";");
        printer->OutNewline();
    }
    return d;
}

/* virtual */ WGL_Decl *
WGL_PrettyPrinter::VisitDecl(WGL_TypeDecl *d)
{
    if (d->type)
        d->type->VisitType(this);
    if (d->var_name)
    {
        printer->OutString(" ");
        printer->OutString(d->var_name);
    }
    printer->OutString(";");

    return d;
}

/* virtual */ WGL_Decl *
WGL_PrettyPrinter::VisitDecl(WGL_VarDecl *d)
{
    if (d->type->precision != WGL_Type::NoPrecision && !d->type->implicit_precision)
        d->Show(this, !printer->ForGLESTarget()); // Don't show precision qualifiers on desktop.
    else
        d->Show(this, FALSE);
    return d;
}

/* virtual */ WGL_DeclList *
WGL_PrettyPrinter::VisitDeclList(WGL_DeclList *ds)
{
    for (WGL_Decl *d = ds->list.First(); d; d = d->Suc())
    {
        d->VisitDecl(this);
        printer->OutNewline();
    }
    return ds;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_BodyStmt *s)
{
    printer->OutString("{");
    printer->IncreaseIndent();
    printer->OutNewline();
    if (s->body)
        s->body->VisitStmtList(this);
    printer->DecreaseIndent();
    printer->OutNewline();
    printer->OutString("}");
    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_CaseLabel *s)
{
    if (s->condition)
    {
        printer->OutString("case ");
        s->condition->VisitExpr(this);
        printer->OutString(":");
        printer->OutNewline();
    }
    else
    {
        printer->OutString("default:");
        printer->OutNewline();
    }
    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_DeclStmt *s)
{
    for (WGL_Decl *decl = s->decls.First(); decl; decl = decl->Suc())
    {
        decl->VisitDecl(this);
        if (decl->Suc())
            printer->OutNewline();
    }

    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_DoStmt *s)
{
    printer->OutString("do");
    printer->OutNewline();
    printer->OutString("{");
    printer->OutNewline();
    printer->IncreaseIndent();
    if (s->body)
        s->body->VisitStmt(this);
    printer->DecreaseIndent();
    printer->OutNewline();
    printer->OutString("} while (");
    if (s->condition)
        s->condition->VisitExpr(this);
    else
        printer->OutString("/*empty*/");
    printer->OutString(")");
    printer->OutString(";");

    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_ExprStmt *s)
{
    if (s->expr)
    {
        s->expr->VisitExpr(this);
        printer->OutString(";");
    }
    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_ForStmt *s)
{
    printer->OutString("for (");
    if (s->head)
    {
        s->head->VisitStmt(this);
        if (s->head->GetType() == WGL_Stmt::Simple && static_cast<WGL_SimpleStmt *>(s->head)->type == WGL_SimpleStmt::Empty)
            printer->OutString(";");
    }
    if (s->predicate)
        s->predicate->VisitExpr(this);
    printer->OutString(";");
    if (s->update)
        s->update->VisitExpr(this);
    printer->OutString(")");
    if (s->body)
    {
        if (s->body->GetType() == WGL_Stmt::Body)
        {
            printer->OutNewline();
            s->body->VisitStmt(this);
        }
        else
        {
            printer->IncreaseIndent();
            printer->OutNewline();
            s->body->VisitStmt(this);
            printer->DecreaseIndent();
        }
    }
    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_IfStmt *s)
{
    printer->OutString("if (");
    s->predicate->VisitExpr(this);
    printer->OutString(")");
    if (s->then_part && s->then_part->GetType() == WGL_Stmt::Body)
    {
        printer->OutNewline();
        s->then_part->VisitStmt(this);
    }
    else
    {
        printer->IncreaseIndent();
        printer->OutNewline();
        if (s->then_part)
            s->then_part->VisitStmt(this);
        else
            printer->OutString(";");
        printer->DecreaseIndent();
        printer->OutNewline();
    }
    if (s->else_part)
    {
        printer->OutNewline();
        printer->OutString("else");
        printer->OutNewline();
        if (s->else_part->GetType() == WGL_Stmt::Body)
            s->else_part->VisitStmt(this);
        else
        {
            printer->IncreaseIndent();
            s->else_part->VisitStmt(this);
            printer->DecreaseIndent();
            printer->OutNewline();
        }
    }
    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_ReturnStmt *s)
{
    if (s->value)
    {
        printer->OutString("return (");
        s->value->VisitExpr(this);
        printer->OutString(");");
    }
    else
        printer->OutString("return;");

    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_SimpleStmt *s)
{
    switch (s->type)
    {
    case WGL_SimpleStmt::Empty:
        printer->OutString("/*empty*/");
        break;
    case WGL_SimpleStmt::Default:
        printer->OutString("default");
        break;
    case WGL_SimpleStmt::Continue:
        printer->OutString("continue");
        break;
    case WGL_SimpleStmt::Break:
        printer->OutString("break");
        break;
    case WGL_SimpleStmt::Discard:
        printer->OutString("discard");
        break;
    }
    printer->OutString(";");
    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_SwitchStmt *s)
{
    printer->OutString("switch(");
    if (s->scrutinee)
        s->scrutinee->VisitExpr(this);
    printer->OutString(")");
    printer->OutNewline();
    printer->OutString("{");
    if (s->cases)
        s->cases->VisitStmtList(this);
    printer->OutNewline();
    printer->OutString("}");
    printer->OutNewline();
    return s;
}

/* virtual */ WGL_Stmt *
WGL_PrettyPrinter::VisitStmt(WGL_WhileStmt *s)
{
    printer->OutString("while (");
    if (s->condition)
        s->condition->VisitExpr(this);
    else
        printer->OutString("/*empty*/");
    printer->OutString(")");
    printer->OutNewline();
    if (s->body)
        s->body->VisitStmt(this);

    return s;
}

/* virtual */ WGL_StmtList *
WGL_PrettyPrinter::VisitStmtList(WGL_StmtList *ss)
{
    for (WGL_Stmt *s = ss->list.First(); s; s = s->Suc())
    {
        s->VisitStmt(this);
        if (s->Suc() && (s->Suc()->GetType() != WGL_Stmt::Simple || static_cast<WGL_SimpleStmt *>(s->Suc())->type != WGL_SimpleStmt::Empty))
            printer->OutNewline();
    }
    return ss;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_AssignExpr *e)
{
    BOOL has_no_prec = printer->HasNoPrecedence();
    WGL_Expr::PrecAssoc old_pa = !has_no_prec ? printer->EnterOpScope(WGL_Expr::OpAssign) : static_cast<WGL_Expr::PrecAssoc>(WGL_Expr::PrecLowest);
    if (e->lhs)
    {
        e->lhs->VisitExpr(this);
        printer->OutString(" ");
        WGL_Op::Show(this, e->op);
        printer->OutString("= ");
    }
    if (e->rhs)
        e->rhs->VisitExpr(this);
    else
        printer->OutString("/*empty*/");

    printer->LeaveOpScope(old_pa);
    return e;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_BinaryExpr *e)
{
    WGL_Expr::PrecAssoc old_pa = printer->EnterOpScope(e->op, printer->GetBinaryArg());
    unsigned as_arg = printer->AsBinaryArg(1);
    e->arg1->VisitExpr(this);
    printer->OutString(" ");
    WGL_Op::Show(this, e->op);
    printer->OutString(" ");
    printer->AsBinaryArg(2);
    e->arg2->VisitExpr(this);
    printer->LeaveOpScope(old_pa, as_arg);
    printer->AsBinaryArg(as_arg);
    return e;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_CallExpr *e)
{
    if (e->fun)
    {
        WGL_Expr::PrecAssoc old_pa = printer->EnterOpScope(WGL_Expr::OpCall);
        e->fun->VisitExpr(this);
        e->args->VisitExprList(this);
        printer->LeaveOpScope(old_pa);
    }
    return e;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_CondExpr *e)
{
    WGL_Expr::PrecAssoc old_pa = printer->EnterOpScope(WGL_Expr::OpCond);
    e->arg1->VisitExpr(this);
    printer->OutString(" ? ");
    e->arg2->VisitExpr(this);
    printer->OutString(" : ");
    e->arg3->VisitExpr(this);
    printer->LeaveOpScope(old_pa);
    return e;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_IndexExpr *e)
{
    if (e->array)
    {
        WGL_Expr::PrecAssoc old_pa = printer->EnterOpScope(WGL_Expr::OpIndex);
        e->array->VisitExpr(this);
        printer->OutString("[");
        if (e->index)
            e->index->VisitExpr(this);
        printer->OutString("]");
        printer->LeaveOpScope(old_pa);
    }
    return e;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_LiteralExpr *e)
{
    if (e->literal)
        e->literal->Show(this);

    return e;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_PostExpr *e)
{
    WGL_Expr::PrecAssoc old_pa = printer->EnterOpScope(e->op);
    e->arg->VisitExpr(this);
    WGL_Op::Show(this, e->op);
    printer->LeaveOpScope(old_pa);
    return e;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_SelectExpr *e)
{
    WGL_Expr::PrecAssoc old_pa = printer->EnterOpScope(WGL_Expr::OpFieldSel);
    e->value->VisitExpr(this);
    printer->OutString(".");
    printer->OutString(e->field);
    printer->LeaveOpScope(old_pa);
    return e;
}

/* virtual */  WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_SeqExpr *e)
{
    WGL_Expr::PrecAssoc old_pa = printer->EnterOpScope(WGL_Expr::OpComma);
    e->arg1->VisitExpr(this);
    printer->OutString(",");
    e->arg2->VisitExpr(this);
    printer->LeaveOpScope(old_pa);
    return e;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_TypeConstructorExpr *t)
{
    t->constructor->VisitType(this);
    return t;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_UnaryExpr *e)
{
    WGL_Expr::PrecAssoc old_pa = printer->EnterOpScope(e->op);
    WGL_Op::Show(this, e->op);
    /* Visit the expression if it is not empty/NULL. */
    if (e->arg)
        e->arg->VisitExpr(this);
    printer->LeaveOpScope(old_pa);
    return e;
}

/* virtual */ WGL_Expr *
WGL_PrettyPrinter::VisitExpr(WGL_VarExpr *e)
{
    if (e->name)
        printer->OutString(e->name);
    else if (e->const_name)
        printer->OutString(e->const_name);

    return e;
}

/* virtual */ WGL_ExprList *
WGL_PrettyPrinter::VisitExprList(WGL_ExprList *es)
{
    WGL_Expr::PrecAssoc old_pa = printer->EnterArgListScope();

    BOOL first = TRUE;
    printer->OutString("(");
    for (WGL_Expr *e = es->list.First(); e; e = e->Suc())
    {
        if (!first)
            printer->OutString(", ");
        else
            first = FALSE;
        e->VisitExpr(this);
    }
    printer->OutString(")");

    printer->LeaveArgListScope(old_pa);
    return es;
}

/* virtual */ WGL_Type *
WGL_PrettyPrinter::VisitType(WGL_ArrayType *t)
{
    t->Show(this);

    t->element_type->VisitType(this);
    if (t->is_constant_value)
    {
        printer->OutString("[");
        printer->OutInt(t->length_value);
        printer->OutString("]");
    }
    else if (!t->length)
    {
        printer->OutString("[]");
    }
    else if (t->length_value)
    {
        printer->OutString("[");
        t->length->VisitExpr(this);
        printer->OutString("]");
    }

    return t;
}

/* virtual */ WGL_Type *
WGL_PrettyPrinter::VisitType(WGL_BasicType *t)
{
    t->Show(this);

    switch(t->type)
    {
    case WGL_BasicType::Void:
        printer->OutString("void");
        return t;
    case WGL_BasicType::Int:
        printer->OutString("int");
        return t;
    case WGL_BasicType::UInt:
        printer->OutString("uint");
        return t;
    case WGL_BasicType::Bool:
        printer->OutString("bool");
        return t;
    case WGL_BasicType::Float:
        printer->OutString("float");
        return t;
    }
    return t;
}

/* virtual */ WGL_Type *
WGL_PrettyPrinter::VisitType(WGL_MatrixType *t)
{
    t->Show(this);

    switch(t->type)
    {
    case WGL_MatrixType::Mat2:
        printer->OutStringId(UNI_L("mat2"));
        return t;
    case WGL_MatrixType::Mat3:
        printer->OutStringId(UNI_L("mat3"));
        return t;
    case WGL_MatrixType::Mat4:
        printer->OutStringId(UNI_L("mat4"));
        return t;
    case WGL_MatrixType::Mat2x2:
        printer->OutStringId(UNI_L("mat2x2"));
        return t;
    case WGL_MatrixType::Mat2x3:
        printer->OutStringId(UNI_L("mat2x3"));
        return t;
    case WGL_MatrixType::Mat2x4:
        printer->OutStringId(UNI_L("mat2x4"));
        return t;
    case WGL_MatrixType::Mat3x2:
        printer->OutStringId(UNI_L("mat3x2"));
        return t;
    case WGL_MatrixType::Mat3x3:
        printer->OutStringId(UNI_L("mat3x3"));
        return t;
    case WGL_MatrixType::Mat3x4:
        printer->OutStringId(UNI_L("mat3x4"));
        return t;
    case WGL_MatrixType::Mat4x2:
        printer->OutStringId(UNI_L("mat4x2"));
        return t;
    case WGL_MatrixType::Mat4x3:
        printer->OutStringId(UNI_L("mat4x3"));
        return t;
    case WGL_MatrixType::Mat4x4:
        printer->OutStringId(UNI_L("mat4x4"));
        return t;
    }

    return t;
}

/* virtual */ WGL_Type *
WGL_PrettyPrinter::VisitType(WGL_NameType *t)
{
    t->Show(this);

    printer->OutString(t->name);
    return t;
}

/* virtual */ WGL_Type *
WGL_PrettyPrinter::VisitType(WGL_SamplerType *t)
{
    t->Show(this);

    switch(t->type)
    {
    case WGL_SamplerType::Sampler1D:
        printer->OutStringId(UNI_L("sampler1D"));
        return t;
    case WGL_SamplerType::Sampler2D:
        printer->OutStringId(UNI_L("sampler2D"));
        return t;
    case WGL_SamplerType::Sampler3D:
        printer->OutStringId(UNI_L("sampler3D"));
        return t;
    case WGL_SamplerType::SamplerCube:
        printer->OutStringId(UNI_L("samplerCube"));
        return t;
    case WGL_SamplerType::Sampler1DShadow:
        printer->OutStringId(UNI_L("sampler1DShadow"));
        return t;
    case WGL_SamplerType::Sampler2DShadow:
        printer->OutStringId(UNI_L("sampler2DShadow"));
        return t;
    case WGL_SamplerType::SamplerCubeShadow:
        printer->OutStringId(UNI_L("samplerCubeShadow"));
        return t;
    case WGL_SamplerType::Sampler1DArray:
        printer->OutStringId(UNI_L("sampler1DArray"));
        return t;
    case WGL_SamplerType::Sampler2DArray:
        printer->OutStringId(UNI_L("sampler2DArray"));
        return t;
    case WGL_SamplerType::Sampler1DArrayShadow:
        printer->OutStringId(UNI_L("sampler1DArrayShadow"));
        return t;
    case WGL_SamplerType::Sampler2DArrayShadow:
        printer->OutStringId(UNI_L("sampler2DArrayShadow"));
        return t;
    case WGL_SamplerType::SamplerBuffer:
        printer->OutStringId(UNI_L("samplerBuffer"));
        return t;
    case WGL_SamplerType::Sampler2DMS:
        printer->OutStringId(UNI_L("sampler2DMS"));
        return t;
    case WGL_SamplerType::Sampler2DMSArray:
        printer->OutStringId(UNI_L("sampler2DMSArray"));
        return t;
    case WGL_SamplerType::Sampler2DRect:
        printer->OutStringId(UNI_L("sampler2DRect"));
        return t;
    case WGL_SamplerType::Sampler2DRectShadow:
        printer->OutStringId(UNI_L("sampler2DRectShadow"));
        return t;
    case WGL_SamplerType::ISampler1D:
        printer->OutStringId(UNI_L("isampler1D"));
        return t;
    case WGL_SamplerType::ISampler2D:
        printer->OutStringId(UNI_L("isampler2D"));
        return t;
    case WGL_SamplerType::ISampler3D:
        printer->OutStringId(UNI_L("isampler3D"));
        return t;
    case WGL_SamplerType::ISamplerCube:
        printer->OutStringId(UNI_L("isamplerCube"));
        return t;
    case WGL_SamplerType::ISampler1DArray:
        printer->OutStringId(UNI_L("isampler1DArray"));
        return t;
    case WGL_SamplerType::ISampler2DArray:
        printer->OutStringId(UNI_L("isampler2DArray"));
        return t;
    case WGL_SamplerType::ISampler2DRect:
        printer->OutStringId(UNI_L("isampler2DRect"));
        return t;
    case WGL_SamplerType::ISamplerBuffer:
        printer->OutStringId(UNI_L("isamplerBuffer"));
        return t;
    case WGL_SamplerType::ISampler2DMS:
        printer->OutStringId(UNI_L("isampler2DMS"));
        return t;
    case WGL_SamplerType::ISampler2DMSArray:
        printer->OutStringId(UNI_L("isampler2DMSArray"));
        return t;
    case WGL_SamplerType::USampler1D:
        printer->OutStringId(UNI_L("usampler1D"));
        return t;
    case WGL_SamplerType::USampler2D:
        printer->OutStringId(UNI_L("usampler2D"));
        return t;
    case WGL_SamplerType::USampler3D:
        printer->OutStringId(UNI_L("usampler3D"));
        return t;
    case WGL_SamplerType::USamplerCube:
        printer->OutStringId(UNI_L("usamplerCube"));
        return t;
    case WGL_SamplerType::USampler1DArray:
        printer->OutStringId(UNI_L("usampler1DArray"));
        return t;
    case WGL_SamplerType::USampler2DArray:
        printer->OutStringId(UNI_L("usampler2DArray"));
        return t;
    case WGL_SamplerType::USampler2DRect:
        printer->OutStringId(UNI_L("usampler2DRect"));
        return t;
    case WGL_SamplerType::USamplerBuffer:
        printer->OutStringId(UNI_L("usamplerBuffer"));
        return t;
    case WGL_SamplerType::USampler2DMS:
        printer->OutStringId(UNI_L("usampler2DMS"));
        return t;
    case WGL_SamplerType::USampler2DMSArray:
        printer->OutStringId(UNI_L("usampler2DMS"));
        return t;
    }

    return t;
}

/* virtual */ WGL_Type *
WGL_PrettyPrinter::VisitType(WGL_StructType *t)
{
    t->Show(this);

    printer->OutString("struct ");
    if (t->tag)
        printer->OutString(t->tag);

    printer->OutNewline();
    printer->OutString("{");
    printer->IncreaseIndent();
    printer->OutNewline();
    if (t->fields)
        t->fields->VisitFieldList(this);
    printer->DecreaseIndent();
    printer->OutNewline();
    printer->OutString("}");

    return t;
}


/* virtual */ WGL_Type *
WGL_PrettyPrinter::VisitType(WGL_VectorType *t)
{
    t->Show(this);

    switch (t->type)
    {
    case WGL_VectorType::Vec2:
        printer->OutStringId(UNI_L("vec2"));
        return t;
    case WGL_VectorType::Vec3:
        printer->OutStringId(UNI_L("vec3"));
        return t;
    case WGL_VectorType::Vec4:
        printer->OutStringId(UNI_L("vec4"));
        return t;
    case WGL_VectorType::BVec2:
        printer->OutStringId(UNI_L("bvec2"));
        return t;
    case WGL_VectorType::BVec3:
        printer->OutStringId(UNI_L("bvec3"));
        return t;
    case WGL_VectorType::BVec4:
        printer->OutStringId(UNI_L("bvec4"));
        return t;
    case WGL_VectorType::IVec2:
        printer->OutStringId(UNI_L("ivec2"));
        return t;
    case WGL_VectorType::IVec3:
        printer->OutStringId(UNI_L("ivec3"));
        return t;
    case WGL_VectorType::IVec4:
        printer->OutStringId(UNI_L("ivec4"));
        return t;
    case WGL_VectorType::UVec2:
        printer->OutStringId(UNI_L("uvec2"));
        return t;
    case WGL_VectorType::UVec3:
        printer->OutStringId(UNI_L("uvec3"));
        return t;
    case WGL_VectorType::UVec4:
        printer->OutStringId(UNI_L("uvec4"));
        return t;
    }
    return t;
}

/* virtual */ WGL_Field *
WGL_PrettyPrinter::VisitField(WGL_Field *f)
{
    WGL_Type *t = f->type;
    BOOL is_array = FALSE;
    while (t->GetType() == WGL_Type::Array)
    {
        is_array = TRUE;
        t = static_cast<WGL_ArrayType *>(t)->element_type;
    }

    if (t->GetType() == WGL_Type::Struct)
    {
        OP_ASSERT(static_cast<WGL_StructType*>(t)->tag);
        printer->OutString(static_cast<WGL_StructType*>(t)->tag);
    }
    else
        t->VisitType(this);

    printer->OutString(" ");
    printer->OutString(f->name);
    if (is_array)
        WGL_ArrayType::ShowSizes(this, static_cast<WGL_ArrayType *>(f->type));

    return f;
}

/* virtual */ WGL_FieldList *
WGL_PrettyPrinter::VisitFieldList(WGL_FieldList *fs)
{
    for (WGL_Field *f = fs->list.First(); f; f = f->Suc())
    {
        f->VisitField(this);
        printer->OutString(";");
        if (f->Suc())
            printer->OutNewline();
    }

    return fs;
}

/* virtual */ WGL_Param *
WGL_PrettyPrinter::VisitParam(WGL_Param *p)
{
    WGL_Param::Show(this, p->direction);
    if (p->direction != WGL_Param::None)
        printer->OutString(" ");
    p->type->VisitType(this);
    if (p->name)
    {
        printer->OutString(" ");
        printer->OutString(p->name);
    }

    return p;
}

/* virtual */ WGL_ParamList *
WGL_PrettyPrinter::VisitParamList(WGL_ParamList *ps)
{
    BOOL first = TRUE;
    for (WGL_Param *p = ps->list.First(); p; p = p->Suc())
    {
        if (!first)
            printer->OutString(", ");
        else
            first = FALSE;
        p->VisitParam(this);
    }
    return ps;
}

/* virtual */ WGL_LayoutPair *
WGL_PrettyPrinter::VisitLayout(WGL_LayoutPair *p)
{
    printer->OutString(p->identifier);
    if (p->value != -1)
    {
        printer->OutString(" = ");
        printer->OutInt(p->value);
    }

    return p;
}

/* virtual */ WGL_LayoutList *
WGL_PrettyPrinter::VisitLayoutList(WGL_LayoutList *ls)
{
    BOOL first = TRUE;
    // ToDo: check with grammar, might be slightly off.
    for (WGL_LayoutPair *lp = ls->list.First(); lp; lp = lp->Suc())
    {
        if (!first)
            printer->OutString(", ");
        else
            first = FALSE;
        lp->VisitLayout(this);
    }

    return ls;
}

#endif // CANVAS3D_SUPPORT
