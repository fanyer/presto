/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009 - 2010
 *
 * WebGL GLSL compiler -- traversing the AST
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_visit.h"

WGL_VarName *
WGL_VarVisitor::VisitVar(WGL_VarName *v, VarKind kind)
{
    return v;
}

WGL_VarName *
WGL_VarVisitor::VisitTypeName(WGL_VarName *v, TypeKind kind)
{
    return v;
}

/* virtual */ WGL_LayoutList *
WGL_VarVisitor::VisitLayoutList(WGL_LayoutList* ls)
{
    for (WGL_LayoutPair *s = ls->list.First(); s; s = s->Suc())
        s = s->VisitLayout(this);

    return ls;
}

/* virtual */ WGL_LayoutPair *
WGL_VarVisitor::VisitLayout(WGL_LayoutPair* lp)
{
    if (lp->identifier)
        lp->identifier = VisitVar(lp->identifier, LayoutName);

    return lp;
}

/* virtual */ WGL_Field *
WGL_VarVisitor::VisitField(WGL_Field *f)
{
    if (f->type)
        f->type = f->type->VisitType(this);
    if (f->name)
        f->name = VisitVar(f->name, FieldDecl);

    return f;
}

/* virtual */ WGL_Param *
WGL_VarVisitor::VisitParam(WGL_Param *p)
{
    if (p->type)
        p->type = p->type->VisitType(this);

    if (p->name)
        p->name = VisitVar(p->name, ParamDecl);

    return p;
}

/* virtual */ WGL_Decl *
WGL_VarVisitor::VisitDecl(WGL_FunctionPrototype *d)
{
    if (d->return_type)
        d->return_type = d->return_type->VisitType(this);
    if (d->name)
        d->name = VisitVar(d->name, FunctionProto);

    if (d->parameters)
        d->parameters = d->parameters->VisitParamList(this);

    return d;
}

/* virtual */ WGL_Decl *
WGL_VarVisitor::VisitDecl(WGL_PrecisionDefn *d)
{
    if (d->type)
        d->type = d->type->VisitType(this);

    return d;
}

/* virtual */ WGL_Decl *
WGL_VarVisitor::VisitDecl(WGL_ArrayDecl *d)
{
    if (d->type)
        d->type = d->type->VisitType(this);
    if (d->identifier)
        d->identifier = VisitVar(d->identifier, ArrayDecl);

    if (d->initialiser)
        d->initialiser = d->initialiser->VisitExpr(this);
    d->size = d->size->VisitExpr(this);

    return d;
}

/* virtual */ WGL_Decl *
WGL_VarVisitor::VisitDecl(WGL_VarDecl *d)
{
    if (d->type)
        d->type = d->type->VisitType(this);

    if (d->identifier)
        d->identifier = VisitVar(d->identifier, VarDecl);

    if (d->initialiser)
        d->initialiser = d->initialiser->VisitExpr(this);

    return d;
}

/* virtual */ WGL_Decl *
WGL_VarVisitor::VisitDecl(WGL_TypeDecl *d)
{
    if (d->type)
        d->type = d->type->VisitType(this);

    if (d->var_name)
        d->var_name = VisitVar(d->var_name, TypeDecl);

    return d;
}

/* virtual */ WGL_Decl *
WGL_VarVisitor::VisitDecl(WGL_InvariantDecl *d)
{
    if (d->var_name)
        d->var_name = VisitVar(d->var_name, InvariantDecl);

    WGL_InvariantDecl *it = d->next;
    while (it)
    {
        it->var_name = VisitVar(it->var_name, InvariantDecl);
        it = it->next;
    }

    return d;
}

/* virtual */ WGL_Decl *
WGL_VarVisitor::VisitDecl(WGL_FunctionDefn *d)
{
    if (d->head)
        d->head = d->head->VisitDecl(this);

    if (d->body)
        d->body = d->body->VisitStmt(this);

    return d;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_SwitchStmt *s)
{
    if (s->scrutinee)
        s->scrutinee = s->scrutinee->VisitExpr(this);

    if (s->cases)
        s->cases = s->cases->VisitStmtList(this);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_SimpleStmt *s)
{
    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_BodyStmt *s)
{
    if (s->body)
        s->body = VisitStmtList(s->body);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_WhileStmt *s)
{
    if (s->condition)
        s->condition = s->condition->VisitExpr(this);

    if (s->body)
        s->body = s->body->VisitStmt(this);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_DoStmt *s)
{
    if (s->body)
        s->body = s->body->VisitStmt(this);

    if (s->condition)
        s->condition = s->condition->VisitExpr(this);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_IfStmt *s)
{
    if (s->predicate)
        s->predicate = s->predicate->VisitExpr(this);

    if (s->then_part)
        s->then_part = s->then_part->VisitStmt(this);
    if (s->else_part)
        s->else_part = s->else_part->VisitStmt(this);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_ForStmt *s)
{
    if (s->head)
        s->head = s->head->VisitStmt(this);
    if (s->predicate)
        s->predicate = s->predicate->VisitExpr(this);
    if (s->update)
        s->update = s->update->VisitExpr(this);
    if (s->body)
        s->body = s->body->VisitStmt(this);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_ReturnStmt *s)
{
    if (s->value)
        s->value = s->value->VisitExpr(this);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_ExprStmt *s)
{
    if (s->expr)
        s->expr = s->expr->VisitExpr(this);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_DeclStmt *s)
{
    for (WGL_Decl *d = s->decls.First(); d; d = d->Suc())
        d = d->VisitDecl(this);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_VarVisitor::VisitStmt(WGL_CaseLabel *s)
{
    if (s->condition)
        s->condition = s->condition->VisitExpr(this);

    return s;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_LiteralExpr *e)
{
    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_VarExpr *e)
{
    if (e->name)
        e->name = VisitVar(e->name, VarExpr);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_TypeConstructorExpr *e)
{
    if (e->constructor)
        e->constructor = e->constructor->VisitType(this);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_UnaryExpr *e)
{
    if (e->arg)
        e->arg = e->arg->VisitExpr(this);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_PostExpr *e)
{
    if (e->arg)
        e->arg = e->arg->VisitExpr(this);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_SeqExpr *e)
{
    if (e->arg1)
        e->arg1 = e->arg1->VisitExpr(this);

    if (e->arg2)
        e->arg2 = e->arg2->VisitExpr(this);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_BinaryExpr *e)
{
    if (e->arg1)
        e->arg1 = e->arg1->VisitExpr(this);
    if (e->arg2)
        e->arg2 = e->arg2->VisitExpr(this);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_CondExpr *e)
{
    if (e->arg1)
        e->arg1 = e->arg1->VisitExpr(this);
    if (e->arg2)
        e->arg2 = e->arg2->VisitExpr(this);
    if (e->arg3)
        e->arg3 = e->arg3->VisitExpr(this);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_CallExpr *e)
{
    if (e->fun)
        e->fun = e->fun->VisitExpr(this);
    if (e->args)
        e->args = e->args->VisitExprList(this);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_IndexExpr *e)
{
    if (e->array)
        e->array = e->array->VisitExpr(this);
    if (e->index)
        e->index = e->index->VisitExpr(this);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_SelectExpr *e)
{
    if (e->value)
        e->value = e->value->VisitExpr(this);
    if (e->field)
        e->field = VisitVar(e->field, FieldLabel);

    return e;
}

/* virtual */ WGL_Expr *
WGL_VarVisitor::VisitExpr(WGL_AssignExpr *e)
{
    if (e->lhs)
    {
        BOOL lhs_visited = FALSE;
        if (e->lhs->GetType() == WGL_Expr::Var)
        {
            WGL_VarExpr *v = static_cast<WGL_VarExpr *>(e->lhs);
            v->name = VisitVar(v->name, VarDecl);
            lhs_visited = TRUE;
        }
        else if (e->lhs->GetType() == WGL_Expr::Index)
        {
            WGL_IndexExpr *index_expr = static_cast<WGL_IndexExpr *>(e->lhs);
            if (index_expr->array->GetType() == WGL_Expr::Var)
            {
                WGL_VarExpr *v = static_cast<WGL_VarExpr *>(index_expr->array);
                v->name = VisitVar(v->name, VarDecl);
                lhs_visited = TRUE;
            }
        }
        else if (e->lhs->GetType() == WGL_Expr::Select)
        {
            WGL_SelectExpr *select_expr = static_cast<WGL_SelectExpr *>(e->lhs);
            if (select_expr->value->GetType() == WGL_Expr::Var)
            {
                WGL_VarExpr *v = static_cast<WGL_VarExpr *>(select_expr->value);
                v->name = VisitVar(v->name, VarDecl);
                lhs_visited = TRUE;
            }
        }
        if (!lhs_visited)
            e->lhs = e->lhs->VisitExpr(this);
    }
    if (e->rhs)
        e->rhs = e->rhs->VisitExpr(this);

    return e;
}

/* virtual */ WGL_Type *
WGL_VarVisitor::VisitType(WGL_BasicType *t)
{
    return t;
}

/* virtual */ WGL_Type *
WGL_VarVisitor::VisitType(WGL_ArrayType *t)
{
    if (t->element_type)
        t->element_type = t->element_type->VisitType(this);
    if (t->length)
        t->length = t->length->VisitExpr(this);

    return t;
}

/* virtual */ WGL_Type *
WGL_VarVisitor::VisitType(WGL_VectorType *t)
{
    return t;
}

/* virtual */ WGL_Type *
WGL_VarVisitor::VisitType(WGL_MatrixType *t)
{
    return t;
}

/* virtual */ WGL_Type *
WGL_VarVisitor::VisitType(WGL_SamplerType *t)
{
    return t;
}

/* virtual */ WGL_Type *
WGL_VarVisitor::VisitType(WGL_NameType *t)
{
    if (t->name)
        t->name = VisitTypeName(t->name, TypeName);

    return t;
}

/* virtual */ WGL_Type *
WGL_VarVisitor::VisitType(WGL_StructType *t)
{
    if (t->tag)
        t->tag = VisitTypeName(t->tag, StructName);
    if (t->fields)
        t->fields = t->fields->VisitFieldList(this);

    return t;
}

#endif // CANVAS3D_SUPPORT
