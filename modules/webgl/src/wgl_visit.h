/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- traversing the AST
 *
 */
#ifndef WGL_VISIT_H
#define WGL_VISIT_H

#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_context.h"

/** Traverse an AST applying a visitor for each variable encountered,
 *  declarations and uses.
 */
class WGL_VarVisitor
    : public WGL_StmtVisitor
    , public WGL_ExprVisitor
    , public WGL_TypeVisitor
    , public WGL_DeclVisitor
    , public WGL_FieldVisitor
    , public WGL_ParamVisitor
    , public WGL_LayoutVisitor
{
protected:
    WGL_IdBuilder *id_builder;

    enum VarKind
    {
        LayoutName,
        FieldDecl,
        ParamDecl,
        FunctionProto,
        ArrayDecl,
        VarDecl,
        TypeDecl,
        InvariantDecl,
        VarExpr,
        FieldLabel
    };

    virtual WGL_VarName *VisitVar(WGL_VarName *v, VarKind kind);

    enum TypeKind
    {
        TypeName,
        StructName
    };

    virtual WGL_VarName *VisitTypeName(WGL_VarName *v, TypeKind kind);

public:
    WGL_VarVisitor(WGL_IdBuilder *i)
        : id_builder(i)
    {
    }

    virtual WGL_LayoutList *VisitLayoutList(WGL_LayoutList *ls);
    virtual WGL_LayoutPair *VisitLayout(WGL_LayoutPair *lp);
    virtual WGL_Field *VisitField(WGL_Field *f);
    virtual WGL_Param *VisitParam(WGL_Param *p);
    virtual WGL_Decl *VisitDecl(WGL_FunctionPrototype *d);
    virtual WGL_Decl *VisitDecl(WGL_PrecisionDefn *d);
    virtual WGL_Decl *VisitDecl(WGL_ArrayDecl *d);
    virtual WGL_Decl *VisitDecl(WGL_VarDecl *d);
    virtual WGL_Decl *VisitDecl(WGL_TypeDecl *d);
    virtual WGL_Decl *VisitDecl(WGL_InvariantDecl *d);
    virtual WGL_Decl *VisitDecl(WGL_FunctionDefn *d);

    virtual WGL_Stmt *VisitStmt(WGL_SwitchStmt *s) ;
    virtual WGL_Stmt *VisitStmt(WGL_SimpleStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_BodyStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_WhileStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_DoStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_IfStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_ForStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_ReturnStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_ExprStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_DeclStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_CaseLabel *s);

    virtual WGL_Expr *VisitExpr(WGL_LiteralExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_VarExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_TypeConstructorExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_UnaryExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_PostExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_SeqExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_BinaryExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_CondExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_CallExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_IndexExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_SelectExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_AssignExpr *e);

    virtual WGL_Type *VisitType(WGL_BasicType *t);
    virtual WGL_Type *VisitType(WGL_ArrayType *t);
    virtual WGL_Type *VisitType(WGL_VectorType *t);
    virtual WGL_Type *VisitType(WGL_MatrixType *t);
    virtual WGL_Type *VisitType(WGL_SamplerType *t);
    virtual WGL_Type *VisitType(WGL_NameType *t);
    virtual WGL_Type *VisitType(WGL_StructType *t);
};

#endif // WGL_VISIT_H
