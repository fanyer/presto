/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- AST building framework and classes.
 *
 */
#ifndef WGL_BUILDER_H
#define WGL_BUILDER_H

#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_context.h"

class WGL_ASTBuilder
{
public:
    WGL_ASTBuilder(WGL_Context *context)
        : context(context)
        , line_number(0)
    {
    }

    void SetLineNumber(unsigned l)
    {
        line_number = l;
    }

    WGL_Expr *BuildUnaryExpression(WGL_Expr::Operator op, WGL_Expr *e1);
    WGL_Expr *BuildBinaryExpression(WGL_Expr::Operator op, WGL_Expr *e1, WGL_Expr *e2);
    WGL_Expr *BuildSeqExpression(WGL_Expr *e1, WGL_Expr *e2);
    WGL_Expr *BuildAssignExpression(WGL_Expr::Operator op, WGL_Expr *e1, WGL_Expr *e2);
    WGL_Expr *BuildCondExpression(WGL_Expr *e1, WGL_Expr *e2, WGL_Expr *e3);

    WGL_Expr *BuildIdentifier(WGL_VarName *i);
    WGL_Expr *BuildTypeConstructor(WGL_Type *t);
    WGL_Expr *BuildBoolLit(BOOL b);
    WGL_Expr *BuildIntLit(int i);
    WGL_Expr *BuildLiteral(WGL_Literal *l);

    WGL_Expr *BuildCallExpression(WGL_Expr *f, WGL_ExprList *args);
    WGL_Expr *BuildIndexExpression(WGL_Expr *arr, WGL_Expr *index);
    WGL_Expr *BuildFieldSelect(WGL_Expr *rec, WGL_VarName *field);
    WGL_Expr *BuildPostOpExpression(WGL_Expr *e1, WGL_Expr::Operator op);

    WGL_Stmt *BuildSwitchStmt(WGL_Expr *s, WGL_StmtList *ss);
    WGL_Stmt *BuildSimpleStmt(WGL_SimpleStmt::Type s);
    WGL_Stmt *BuildBlockStmt(WGL_StmtList *ls);
    WGL_Stmt *BuildWhileStmt(WGL_Expr *e, WGL_Stmt *ss);
    WGL_Stmt *BuildDoStmt(WGL_Stmt *ss, WGL_Expr *e);
    WGL_Stmt *BuildDeclStmt(WGL_Decl *d);
    WGL_Stmt *BuildForStmt(WGL_Stmt *d, WGL_Expr *e1, WGL_Expr *e2, WGL_Stmt *ss);
    WGL_Stmt *BuildIfStmt(WGL_Expr *e1, WGL_Stmt *s1, WGL_Stmt *s2, unsigned line_no);
    WGL_Stmt *BuildReturnStmt(WGL_Expr *e1);
    WGL_Stmt *BuildExprStmt(WGL_Expr *e1);
    WGL_Stmt *BuildCaseLabel(WGL_Expr *e1);

    WGL_Type *BuildBasicType(WGL_BasicType::Type t);
    WGL_Type *BuildVectorType(WGL_VectorType::Type t);
    WGL_Type *BuildMatrixType(WGL_MatrixType::Type t);
    WGL_Type *BuildSamplerType(WGL_SamplerType::Type t);
    WGL_Type *BuildTypeName(WGL_VarName *i);
    WGL_Type *BuildStructType(WGL_VarName *tag, WGL_FieldList *s);
    WGL_Type *BuildArrayType(WGL_Type *t, WGL_Expr *e);

    WGL_Decl *BuildInvariantDecl(WGL_VarName *i);
    WGL_Decl *BuildTypeDecl(WGL_Type *t, WGL_VarName *i);
    WGL_Decl *BuildVarDecl(WGL_Type *t, WGL_VarName *i, WGL_Expr *e);
    WGL_Decl *BuildArrayDecl(WGL_Type *t, WGL_VarName *i, WGL_Expr *dim, WGL_Expr *init);
    WGL_Decl *BuildArrayVarDecl(WGL_Decl *d, WGL_VarName *i, WGL_Expr *dim, WGL_Expr *init);
    WGL_Decl *BuildVarDeclTo(WGL_Decl *d, WGL_VarName *i, WGL_Expr *e);

    WGL_LayoutPair *BuildLayoutTypeQualifier(WGL_VarName *i, int v);
    WGL_TypeQualifier::InvariantKind BuildInvariantQualifier(WGL_TypeQualifier::InvariantKind iq);

    WGL_Decl *BuildFunctionDefn(WGL_Decl *d, WGL_Stmt *body);
    WGL_Decl *BuildPrecisionDecl(WGL_Type::Precision pq, WGL_Type *t);

    WGL_Param *BuildParam(WGL_Type *t, WGL_Param::Direction pq, WGL_VarName *i);
    WGL_Decl *BuildFunctionProto(WGL_Type *t, WGL_VarName *f, WGL_ParamList *ps);

    WGL_TypeQualifier *NewTypeQualifier();
    WGL_LayoutList *NewLayoutList();
    WGL_ExprList *NewExprList();
    WGL_DeclList *NewDeclList();
    WGL_StmtList *NewStmtList();
    WGL_ParamList *NewParamList();
    WGL_FieldList *NewFieldList();

private:
    WGL_Context *context;
    unsigned line_number;
};

#endif // WGL_BUILDER_H
