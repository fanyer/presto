/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- parser
 *
 */
#ifndef WGL_PARSER_H
#define WGL_PARSER_H
#include "modules/webgl/src/wgl_lexer.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_builder.h"
#include "modules/webgl/src/wgl_context.h"
#include "modules/webgl/src/wgl_error.h"

class WGL_Parser
{
public:
    static void MakeL(WGL_Parser *&parser, WGL_Context *context, WGL_Lexer *lexer, BOOL for_vertex, BOOL rewrite_texcoords);

    ~WGL_Parser();

    WGL_Expr *Expression();
    WGL_Expr *AssignmentExpression();
    WGL_Expr *CondExpression();
    WGL_Expr *BinaryExpression(int prec);
    WGL_Expr *UnaryExpression();
    WGL_Expr *PostfixExpression();

    WGL_StmtList *StatementList(WGL_StmtList *ss);
    WGL_Stmt *Statement();
    WGL_Stmt *SwitchStatement();
    WGL_Stmt *WhileStatement();
    WGL_Stmt *ForStatement();
    WGL_Stmt *IfStatement();
    WGL_Stmt *DoStatement();
    WGL_Stmt *BlockStatement();

    WGL_Type *TypeSpecifierNoPrec();
    WGL_Type *TypeSpecifier();
    WGL_FieldList *StructDeclaration();
    WGL_Type::Precision PrecQualifier();

    WGL_Decl *SingleDeclaration();
    WGL_Decl *InvariantDeclaration(List<WGL_Decl> *ts);
    WGL_Decl *VarDeclPartial(WGL_Type *t, WGL_VarName *i);
    WGL_Type *FullType(BOOL seenInvariant);
    WGL_TypeQualifier *TypeQualify(BOOL seenInvariant);
    WGL_TypeQualifier::Storage TypeStorageQualifier();
    WGL_LayoutList *TypeQualifierLayout();
    WGL_TypeQualifier::InvariantKind TypeQualifierInvariant();
    WGL_DeclList *TopDecls();
    WGL_DeclList *Declaration(WGL_DeclList *ls);
    WGL_Decl *FunctionPrototype(WGL_Decl *&tyd);
    WGL_DeclList *InitDeclarators(WGL_DeclList *ls, WGL_Decl *d0);

    WGL_ASTBuilder *GetASTBuilder() { return context->GetASTBuilder(); }
    BOOL HaveErrors();
    WGL_Error *GetFirstError() { return context->GetFirstError(); }

private:
    WGL_Parser()
        : context(NULL)
        , lexer(NULL)
        , token(-1)
        , rewrite_texcoords(FALSE)
    {
    }

    WGL_Context *context;
    WGL_Lexer *lexer;
    int token;
    BOOL rewrite_texcoords;

    int Current();
    int Match(int l, int ll);
    int NotMatch(int l, int ll);
    int LookingAt(int l);
    void Eat(BOOL update_line_number = TRUE);
    WGL_VarName *GetIdentifier();
    WGL_Literal *GetLiteral(unsigned t);
    int GetIntLit();

    WGL_VarName *GetTypeName();
    unsigned GetTypeNameTag();

    void Error(WGL_Error::Type t, int l);
    void Error(WGL_Error::Type t, WGL_VarName *i);
    void ErrorExpected(const char *s, int l);
    void ParseError();

    /* Helper method for applying a binary operator with a literal vec2. */
    WGL_Expr *ApplyBinaryOpWithLiteralVec2(int x, int y, WGL_Expr::Operator op, WGL_Expr *expr, BOOL expr_on_rhs = TRUE);
};

#endif // WGL_PARSER_H
