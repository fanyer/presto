/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- pretty printer.
 *
 */
#ifndef WGL_PRETTY_PRINTER_H
#define WGL_PRETTY_PRINTER_H

#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_printer.h"

/** Traverse an AST and show it; parameterised over the printer to use. */
class WGL_PrettyPrinter
    : public WGL_StmtVisitor
    , public WGL_ExprVisitor
    , public WGL_TypeVisitor
    , public WGL_DeclVisitor
    , public WGL_FieldVisitor
    , public WGL_ParamVisitor
    , public WGL_LayoutVisitor
{
public:
    WGL_PrettyPrinter(WGL_Printer *p)
        : printer(p)
    {
    }

    ~WGL_PrettyPrinter()
    {
    }


    WGL_Printer *printer;

    virtual WGL_Decl *VisitDecl(WGL_ArrayDecl *d);
    virtual WGL_Decl *VisitDecl(WGL_FunctionDefn *d);
    virtual WGL_Decl *VisitDecl(WGL_FunctionPrototype *d);
    virtual WGL_Decl *VisitDecl(WGL_InvariantDecl *d);
    virtual WGL_Decl *VisitDecl(WGL_PrecisionDefn *d);
    virtual WGL_Decl *VisitDecl(WGL_TypeDecl *d);
    virtual WGL_Decl *VisitDecl(WGL_VarDecl *d);
    virtual WGL_DeclList *VisitDeclList(WGL_DeclList *ls);

    virtual WGL_Stmt *VisitStmt(WGL_BodyStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_CaseLabel *s);
    virtual WGL_Stmt *VisitStmt(WGL_DeclStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_DoStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_ExprStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_ForStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_IfStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_ReturnStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_SimpleStmt *s);
    virtual WGL_Stmt *VisitStmt(WGL_SwitchStmt *s) ;
    virtual WGL_Stmt *VisitStmt(WGL_WhileStmt *s);
    virtual WGL_StmtList *VisitStmtList(WGL_StmtList *ls);

    virtual WGL_Expr *VisitExpr(WGL_AssignExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_BinaryExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_CallExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_CondExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_IndexExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_LiteralExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_PostExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_SelectExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_SeqExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_TypeConstructorExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_UnaryExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_VarExpr *e);
    virtual WGL_ExprList *VisitExprList(WGL_ExprList *ls);

    virtual WGL_Type *VisitType(WGL_ArrayType *t);
    virtual WGL_Type *VisitType(WGL_BasicType *t);
    virtual WGL_Type *VisitType(WGL_MatrixType *t);
    virtual WGL_Type *VisitType(WGL_NameType *t);
    virtual WGL_Type *VisitType(WGL_SamplerType *t);
    virtual WGL_Type *VisitType(WGL_StructType *t);
    virtual WGL_Type *VisitType(WGL_VectorType *t);

    virtual WGL_Field *VisitField(WGL_Field *f);
    virtual WGL_FieldList *VisitFieldList(WGL_FieldList *ls);

    virtual WGL_Param *VisitParam(WGL_Param *p);
    virtual WGL_ParamList *VisitParamList(WGL_ParamList *p);

    virtual WGL_LayoutPair *VisitLayout(WGL_LayoutPair *lp);
    virtual WGL_LayoutList *VisitLayoutList(WGL_LayoutList *ls);
};

#endif // WGL_PRETTY_PRINTER_H
