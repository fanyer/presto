/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- traversing the AST
 *
 */
#ifndef WGL_VALIDATE_H
#define WGL_VALIDATE_H

#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_visit.h"
#include "modules/webgl/src/wgl_error.h"

#define WGL_MAX_IDENTIFIER_LENGTH 256

/** WGL_ValidateShader processes a vertex or fragment shader, validating
    it for safety and well-formedness per spec. */
class WGL_ValidateShader
    : public WGL_StmtVisitor
    , public WGL_ExprVisitor
    , public WGL_TypeVisitor
    , public WGL_DeclVisitor

    , public WGL_FieldVisitor
    , public WGL_ParamVisitor
    , public WGL_LayoutVisitor
{
public:
    static void MakeL(WGL_ValidateShader *&validator, WGL_Context *context, WGL_Printer *printer);

    BOOL ValidateL(WGL_DeclList *decls, WGL_Validator::Configuration &config);
    /**< Perform validation on a vertex/fragment shader translation unit.
         If validation reports errors or warnings, FALSE is returned,
         TRUE if input passes muster. The validation will leave if
         OOM is encountered (only.) */

    virtual ~WGL_ValidateShader()
    {
    }

    WGL_VarName *ValidateIdentifier(WGL_VarName *i, BOOL is_use = TRUE, BOOL force_alias = FALSE, BOOL is_program_global_id = FALSE);
    WGL_VarName *ValidateVar(WGL_VarName *i, BOOL is_local_only = FALSE);
    WGL_VarName *ValidateTypeName(WGL_VarName *ty_name, BOOL is_use = TRUE);
    WGL_VarBinding *ValidateVarDefn(WGL_VarName *var, WGL_Type *var_type, BOOL is_read_only = FALSE);

    void CheckUniform(WGL_VaryingOrUniform *uniform, WGL_Type *type, WGL_VarName *var, WGL_Type::Precision prec);
    void CheckVarying(WGL_VaryingOrUniform *varying, WGL_Type *type, WGL_VarName *var, WGL_Type::Precision prec);

    void CheckBinaryExpr(WGL_Expr::Operator op, WGL_Type *t1, WGL_Type *t2);

    BOOL HaveErrors();
    WGL_Error *GetFirstError() { return context->GetFirstError(); }
    void Reset();

    BOOL IsVertexShader() { return for_vertex; }

    virtual WGL_StmtList *VisitStmtList(WGL_StmtList *ls);
    virtual WGL_ExprList *VisitExprList(WGL_ExprList *ls);
    virtual WGL_DeclList *VisitDeclList(WGL_DeclList *ls);
    virtual WGL_FieldList *VisitFieldList(WGL_FieldList *ls);
    virtual WGL_LayoutList *VisitLayoutList(WGL_LayoutList *ls);
    virtual WGL_LayoutPair *VisitLayout(WGL_LayoutPair *lp);
    virtual WGL_Field *VisitField(WGL_Field *f);
    virtual WGL_ParamList *VisitParamList(WGL_ParamList *ls);
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

    BOOL IsVaryingType(WGL_Type *t);
    BOOL IsValidReturnType(WGL_Type *t);

protected:
    WGL_ValidateShader()
        : printer(NULL)
        , context(NULL)
        , for_vertex(TRUE)
    {
    }

    WGL_Printer *printer;
    WGL_Context *context;
    BOOL for_vertex;

    BOOL IsConstructorArgType(WGL_Type *t);
    BOOL IsConstructorArgType(WGL_BuiltinType *t);
    BOOL IsScalarOrVecMat(WGL_Expr *e, BOOL is_nested);

    static BOOL IsRecursivelyCalling(WGL_Context *context, List<WGL_VariableUse> &visited, WGL_VarName *f, WGL_VarName *call);
    /**< @return TRUE if 'f' is called by the function 'call'
                 (or any of its callers). Keep a list of visited
                 functions to prevent duplicate work and cycles. */
};

#endif // WGL_VALIDATE_H
