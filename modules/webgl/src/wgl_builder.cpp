/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009 - 2010
 *
 * WebGL GLSL compiler -- AST builder routines.
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_builder.h"

WGL_Expr *
WGL_ASTBuilder::BuildUnaryExpression(WGL_Expr::Operator op, WGL_Expr *e1)
{
    return OP_NEWGRO_L(WGL_UnaryExpr, (op,e1), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildBinaryExpression(WGL_Expr::Operator op, WGL_Expr *e1, WGL_Expr *e2)
{
    return OP_NEWGRO_L(WGL_BinaryExpr, (op,e1,e2), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildSeqExpression(WGL_Expr *e1, WGL_Expr *e2)
{
    return OP_NEWGRO_L(WGL_SeqExpr, (e1,e2), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildAssignExpression(WGL_Expr::Operator op, WGL_Expr *e1, WGL_Expr *e2)
{
    return OP_NEWGRO_L(WGL_AssignExpr, (op,e1,e2), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildCondExpression(WGL_Expr *e1, WGL_Expr *e2, WGL_Expr *e3)
{
    return OP_NEWGRO_L(WGL_CondExpr, (e1,e2,e3), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildIdentifier(WGL_VarName *i)
{
    return OP_NEWGRO_L(WGL_VarExpr, (i), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildTypeConstructor(WGL_Type *t)
{
    return OP_NEWGRO_L(WGL_TypeConstructorExpr, (t), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildBoolLit(BOOL b)
{
    WGL_Literal *lit = context->BuildLiteral(WGL_Literal::Bool);
    lit->value.b = !!b;
    return OP_NEWGRO_L(WGL_LiteralExpr, (lit), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildIntLit(int i)
{
    WGL_Literal *lit = context->BuildLiteral(WGL_Literal::Int);
    lit->value.i = i;
    return OP_NEWGRO_L(WGL_LiteralExpr, (lit), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildLiteral(WGL_Literal *l)
{
    return OP_NEWGRO_L(WGL_LiteralExpr, (l), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildCallExpression(WGL_Expr *f, WGL_ExprList *args)
{
    return OP_NEWGRO_L(WGL_CallExpr, (f,args), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildIndexExpression(WGL_Expr *arr, WGL_Expr *index)
{
    return OP_NEWGRO_L(WGL_IndexExpr, (arr,index), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildFieldSelect(WGL_Expr *rec, WGL_VarName *field)
{
    return OP_NEWGRO_L(WGL_SelectExpr, (rec,field), context->Arena());
}

WGL_Expr *
WGL_ASTBuilder::BuildPostOpExpression(WGL_Expr *e1, WGL_Expr::Operator op)
{
    return OP_NEWGRO_L(WGL_PostExpr, (op,e1), context->Arena());
}

WGL_Stmt *
WGL_ASTBuilder::BuildSwitchStmt(WGL_Expr *e, WGL_StmtList *ss)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_SwitchStmt, (e,ss), context->Arena());
    stmt->line_number = line_number;

    return stmt;
}

WGL_Stmt *
WGL_ASTBuilder::BuildSimpleStmt(WGL_SimpleStmt::Type s)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_SimpleStmt, (s), context->Arena());
    stmt->line_number = line_number;
    return stmt;
}

WGL_Stmt *
WGL_ASTBuilder::BuildBlockStmt(WGL_StmtList *ls)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_BodyStmt, (ls), context->Arena());
    stmt->line_number = line_number;

    return stmt;
}

WGL_Stmt *
WGL_ASTBuilder::BuildWhileStmt(WGL_Expr *e, WGL_Stmt *ss)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_WhileStmt, (e,ss), context->Arena());
    stmt->line_number = line_number;

    return stmt;
}

WGL_Stmt *
WGL_ASTBuilder::BuildDoStmt(WGL_Stmt *s, WGL_Expr *e)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_DoStmt, (s,e), context->Arena());
    stmt->line_number = line_number;

    return stmt;
}

WGL_Stmt *
WGL_ASTBuilder::BuildDeclStmt(WGL_Decl *d)
{
    WGL_Stmt *decl = OP_NEWGRO_L(WGL_DeclStmt, (), context->Arena());
    decl->line_number = line_number;
    if (d)
        d->Into(&static_cast<WGL_DeclStmt *>(decl)->decls);

    return decl;
}

WGL_Stmt *
WGL_ASTBuilder::BuildForStmt(WGL_Stmt *d, WGL_Expr *e1, WGL_Expr *e2, WGL_Stmt *ss)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_ForStmt, (d,e1,e2,ss), context->Arena());
    stmt->line_number = line_number;

    return stmt;
}

WGL_Stmt *
WGL_ASTBuilder::BuildIfStmt(WGL_Expr *e1, WGL_Stmt *s1, WGL_Stmt *s2, unsigned line_no)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_IfStmt, (e1,s1,s2), context->Arena());
    stmt->line_number = line_no > 0 ? line_no : line_number;

    return stmt;
}

WGL_Stmt *
WGL_ASTBuilder::BuildReturnStmt(WGL_Expr *e1)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_ReturnStmt, (e1), context->Arena());
    stmt->line_number = line_number;

    return stmt;
}

WGL_Stmt *
WGL_ASTBuilder::BuildExprStmt(WGL_Expr *e1)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_ExprStmt, (e1), context->Arena());
    stmt->line_number = line_number;

    return stmt;
}

WGL_Stmt *
WGL_ASTBuilder::BuildCaseLabel(WGL_Expr *e1)
{
    WGL_Stmt *stmt = OP_NEWGRO_L(WGL_CaseLabel, (e1), context->Arena());
    stmt->line_number = line_number;

    return stmt;
}

WGL_Type *
WGL_ASTBuilder::BuildBasicType(WGL_BasicType::Type t)
{
    return OP_NEWGRO_L(WGL_BasicType, (t, FALSE), context->Arena());
}

WGL_Type *
WGL_ASTBuilder::BuildVectorType(WGL_VectorType::Type t)
{
    return OP_NEWGRO_L(WGL_VectorType, (t, FALSE), context->Arena());
}

WGL_Type *
WGL_ASTBuilder::BuildMatrixType(WGL_MatrixType::Type t)
{
    return OP_NEWGRO_L(WGL_MatrixType, (t, FALSE), context->Arena());
}

WGL_Type *
WGL_ASTBuilder::BuildSamplerType(WGL_SamplerType::Type t)
{
    return OP_NEWGRO_L(WGL_SamplerType, (t, FALSE), context->Arena());
}

WGL_Type *
WGL_ASTBuilder::BuildTypeName(WGL_VarName *n)
{
    return OP_NEWGRO_L(WGL_NameType, (n, FALSE), context->Arena());
}

WGL_Type *
WGL_ASTBuilder::BuildStructType(WGL_VarName *tag, WGL_FieldList *fs)
{
    return OP_NEWGRO_L(WGL_StructType, (tag, fs, FALSE), context->Arena());
}

WGL_Type *
WGL_ASTBuilder::BuildArrayType(WGL_Type *t, WGL_Expr *e)
{
    WGL_ArrayType *array_type = OP_NEWGRO_L(WGL_ArrayType, (t, e, FALSE), context->Arena());
    if (t)
        array_type->type_qualifier = t->type_qualifier;
    return array_type;
}

WGL_Decl *
WGL_ASTBuilder::BuildInvariantDecl(WGL_VarName *i)
{
    WGL_Decl *decl = OP_NEWGRO_L(WGL_InvariantDecl, (i), context->Arena());
    decl->line_number = line_number;

    return decl;
}

WGL_Decl *
WGL_ASTBuilder::BuildTypeDecl(WGL_Type *t, WGL_VarName *i)
{
    WGL_Decl *decl = OP_NEWGRO_L(WGL_TypeDecl, (t,i), context->Arena());
    decl->line_number = line_number;

    return decl;
}

WGL_Decl *
WGL_ASTBuilder::BuildVarDecl(WGL_Type *t, WGL_VarName *i, WGL_Expr *e)
{
    if (!t)
        t->type_qualifier = NULL;

    WGL_Decl *decl = OP_NEWGRO_L(WGL_VarDecl, (t, i, e), context->Arena());
    decl->line_number = line_number;

    return decl;
}

WGL_Decl *
WGL_ASTBuilder::BuildArrayDecl(WGL_Type *t, WGL_VarName *i, WGL_Expr *dim, WGL_Expr *init)
{
    WGL_Decl *decl = OP_NEWGRO_L(WGL_ArrayDecl, (t, i, dim, init), context->Arena());
    decl->line_number = line_number;

    return decl;
}

WGL_Decl *
WGL_ASTBuilder::BuildArrayVarDecl(WGL_Decl *d, WGL_VarName *i, WGL_Expr *dim, WGL_Expr *init)
{
    OP_ASSERT(d->GetType() == WGL_Decl::TypeDecl);
    WGL_Decl *decl = OP_NEWGRO_L(WGL_ArrayDecl, (static_cast<WGL_TypeDecl *>(d)->type, i, dim, init), context->Arena());
    decl->line_number = line_number;

    return decl;
}

WGL_Decl *
WGL_ASTBuilder::BuildVarDeclTo(WGL_Decl *d, WGL_VarName *i, WGL_Expr *e)
{
    WGL_Type *t = NULL;
    if (d->GetType() == WGL_Decl::TypeDecl)
        t = static_cast<WGL_TypeDecl *>(d)->type;
    else if (d->GetType() == WGL_Decl::Var)
        t = static_cast<WGL_VarDecl *>(d)->type;
    else
        OP_ASSERT(!"unexpected variable declaration type");

    WGL_Decl *decl = OP_NEWGRO_L(WGL_VarDecl, (t, i, e), context->Arena());
    decl->line_number = line_number;

    return decl;
}

WGL_LayoutPair *
WGL_ASTBuilder::BuildLayoutTypeQualifier(WGL_VarName *i, int v)
{
    return OP_NEWGRO_L(WGL_LayoutPair, (i,v), context->Arena());
}

WGL_Decl *
WGL_ASTBuilder::BuildFunctionDefn(WGL_Decl *d, WGL_Stmt *body)
{
    WGL_Decl *decl = OP_NEWGRO_L(WGL_FunctionDefn, (d,body), context->Arena());
    decl->line_number = line_number;

    return decl;
}

WGL_Decl *
WGL_ASTBuilder::BuildPrecisionDecl(WGL_Type::Precision pq, WGL_Type *t)
{
    WGL_Decl *decl = OP_NEWGRO_L(WGL_PrecisionDefn, (pq,t), context->Arena());
    decl->line_number = line_number;

    return decl;
}

WGL_Param *
WGL_ASTBuilder::BuildParam(WGL_Type *t, WGL_Param::Direction pq, WGL_VarName *i)
{
    return OP_NEWGRO_L(WGL_Param, (t,i, pq), context->Arena());
}

WGL_Decl *
WGL_ASTBuilder::BuildFunctionProto(WGL_Type *t, WGL_VarName *f, WGL_ParamList *ps)
{
    WGL_Decl *decl = OP_NEWGRO_L(WGL_FunctionPrototype, (t,f, ps), context->Arena());
    decl->line_number = line_number;

    return decl;
}

WGL_TypeQualifier *
WGL_ASTBuilder::NewTypeQualifier()
{
    return OP_NEWGRO_L(WGL_TypeQualifier, (FALSE), context->Arena());
}

WGL_ExprList *
WGL_ASTBuilder::NewExprList()
{
    return OP_NEWGRO_L(WGL_ExprList, (), context->Arena());
}

WGL_LayoutList *
WGL_ASTBuilder::NewLayoutList()
{
    return OP_NEWGRO_L(WGL_LayoutList, (FALSE), context->Arena());
}

WGL_DeclList *
WGL_ASTBuilder::NewDeclList()
{
    return OP_NEWGRO_L(WGL_DeclList, (), context->Arena());
}

WGL_StmtList *
WGL_ASTBuilder::NewStmtList()
{
    return OP_NEWGRO_L(WGL_StmtList, (), context->Arena());
}

WGL_ParamList *
WGL_ASTBuilder::NewParamList()
{
    return OP_NEWGRO_L(WGL_ParamList, (), context->Arena());
}

WGL_FieldList *
WGL_ASTBuilder::NewFieldList()
{
    return OP_NEWGRO_L(WGL_FieldList, (FALSE), context->Arena());
}

#endif // CANVAS3D_SUPPORT
