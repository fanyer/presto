/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009 - 2011
 *
 * WebGL GLSL compiler -- parser.
 *
 * Inspired some by es_parser.
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_lexer.h"
#include "modules/webgl/src/wgl_parser.h"
#include "modules/webgl/src/wgl_string.h"

#include "modules/util/opstring.h"

#define WGL_MAX_DOUBLE_LIT_LENGTH 60

#define MATCH(x) Match(x, __LINE__)
#define OPTIONAL_MATCH(x) Match(x, __LINE__)
#define NotMATCH(x) NotMatch(x, __LINE__)
#define EXPECT_MATCH(t, l) do { if (Current() != t) { WGL_Parser::ErrorExpected(l, __LINE__); } else { Eat(FALSE); } } while(0)
#define REQUIRED(v, act, err) v = act; if (!v) { WGL_Parser::Error(err, __LINE__); }

static WGL_BasicType::Type
ToBasicType(WGL_Token::Token t)
{
    switch (t)
    {
    case WGL_Token::TOKEN_VOID:
        return WGL_BasicType::Void;
    case WGL_Token::TOKEN_FLOAT:
        return WGL_BasicType::Float;
    case WGL_Token::TOKEN_INT:
        return WGL_BasicType::Int;
    case WGL_Token::TOKEN_BOOL:
        return WGL_BasicType::Bool;
    default:
        OP_ASSERT(!"Unexpected basic type token");
        return WGL_BasicType::Void;
    }
}

WGL_Expr*
WGL_Parser::Expression()
{
    WGL_Expr *e1 = AssignmentExpression();
    if (!e1)
        return NULL;
    while (MATCH(WGL_Token::TOKEN_COMMA))
    {
        WGL_Expr *e2 = AssignmentExpression();
        if (!e2)
            Error(WGL_Error::EXPECTED_EXPR, __LINE__);
        else
            e1 = GetASTBuilder()->BuildSeqExpression(e1, e2);
    }
    return e1;
}

WGL_Expr*
WGL_Parser::AssignmentExpression()
{
    WGL_Expr *e1 = CondExpression();

    int tok = Current();
    switch (tok)
    {
    case WGL_Token::TOKEN_MOD_ASSIGN:
    case WGL_Token::TOKEN_LEFT_ASSIGN:
    case WGL_Token::TOKEN_RIGHT_ASSIGN:
    case WGL_Token::TOKEN_AND_ASSIGN:
    case WGL_Token::TOKEN_OR_ASSIGN:
    case WGL_Token::TOKEN_XOR_ASSIGN:
        Error(WGL_Error::RESERVED_OPERATOR, __LINE__);
        return NULL;
    default:
        break;
    }

    while (GET_TOKEN_PRECEDENCE(tok = Current()) == WGL_Expr::PrecAssign)
    {
        Eat();

        /* Weird; no evidence of being supported (any longer?) in the grammar, but conformance tests use it.
         * Just recognise for now. */
        if (token == WGL_Token::TOKEN_HIGH_PREC || token == WGL_Token::TOKEN_MED_PREC || token == WGL_Token::TOKEN_LOW_PREC)
            Eat();

        WGL_Expr *e2 = AssignmentExpression();
        if (!e2)
            Error(WGL_Error::EXPECTED_EXPR, __LINE__);
        e1 = GetASTBuilder()->BuildAssignExpression(tok == WGL_Token::TOKEN_EQUAL ? WGL_Expr::OpAssign : GET_TOKEN_OPERATOR(tok), e1, e2);
    }
    return e1;
}

WGL_Expr*
WGL_Parser::CondExpression()
{
    WGL_Expr *e1 = BinaryExpression(WGL_Expr::PrecCond + 1);
    if (!e1)
        return NULL;

    while (MATCH(WGL_Token::TOKEN_COND))
    {
        WGL_Expr *e2;
        REQUIRED(e2, Expression(), WGL_Error::EXPECTED_EXPR);
        EXPECT_MATCH(WGL_Token::TOKEN_COLON, ":");
        WGL_Expr *e3;
        REQUIRED(e3, AssignmentExpression(), WGL_Error::EXPECTED_EXPR);
        e1 = GetASTBuilder()->BuildCondExpression(e1, e2, e3);
    }
    return e1;
}

WGL_Expr*
WGL_Parser::BinaryExpression(int prec)
{
    WGL_Expr *e1 = UnaryExpression();
    int tok = Current();

    switch(tok)
    {
    case WGL_Token::TOKEN_MOD:
    case WGL_Token::TOKEN_SHIFTL:
    case WGL_Token::TOKEN_SHIFTR:
    case WGL_Token::TOKEN_BITAND:
    case WGL_Token::TOKEN_BITOR:
    case WGL_Token::TOKEN_BITXOR:
        Error(WGL_Error::RESERVED_OPERATOR, __LINE__);
        return NULL;
    default:
        break;
    }

    if (GET_TOKEN_PRECEDENCE(tok) >= WGL_Expr::PrecPrefix)
        return e1;

    for (int k1 = GET_TOKEN_PRECEDENCE(tok); k1 >= prec; k1--)
        while (GET_TOKEN_PRECEDENCE(tok) == k1)
        {
            Eat();
            WGL_Expr *e2;
            REQUIRED(e2, BinaryExpression(k1+1), WGL_Error::EXPECTED_EXPR);
            if (!e1)
                Error(WGL_Error::EXPECTED_EXPR, __LINE__);
            e1 = GetASTBuilder()->BuildBinaryExpression(GET_TOKEN_OPERATOR(tok), e1, e2);
            tok = Current();
        }

    return e1;
}

/* Unary prefix and postfix expressions.  These operators bind tighter than any
   binary operator, and postfix ++ and -- binds more tightly than any prefix
   operator. */
WGL_Expr *
WGL_Parser::UnaryExpression()
{
    int tok  = Current();

    switch (tok)
    {
    case WGL_Token::TOKEN_INC:
    case WGL_Token::TOKEN_DEC:
    case WGL_Token::TOKEN_ADD:
    case WGL_Token::TOKEN_SUB:
    case WGL_Token::TOKEN_NOT:
    case WGL_Token::TOKEN_NEGATE:
    {
        Eat();
        WGL_Expr *e1 = UnaryExpression();
        if (tok == WGL_Token::TOKEN_SUB)
            tok = WGL_Token::TOKEN_NEGATE;
        return GetASTBuilder()->BuildUnaryExpression(GET_TOKEN_OPERATOR(tok), e1);
    }
    case WGL_Token::TOKEN_COMPLEMENT:
        Error(WGL_Error::RESERVED_OPERATOR, __LINE__);
        return NULL;
    default:
        return PostfixExpression();
    }
}

WGL_Expr *
WGL_Parser::PostfixExpression()
{
    WGL_Expr *e1 = NULL;
    int tok = Current();
    BOOL rewriteTexCoords = FALSE;
    switch (tok)
    {
    case WGL_Token::TOKEN_IDENTIFIER:
    {
        WGL_VarName *i = GetIdentifier();

        // Since the textures are stored upside down internally we skip the tc rewrite for now.
        //rewriteTexCoords = rewrite_texcoords && !uni_strcmp(i->value, UNI_L("texture2D"));
        Eat();
        if (context->IsTypeName(i))
            e1 = GetASTBuilder()->BuildTypeConstructor(GetASTBuilder()->BuildTypeName(i));
        else
            e1 = GetASTBuilder()->BuildIdentifier(i);
        break;
    }
    case WGL_Token::TOKEN_TYPE_NAME:
    {
        WGL_VarName *i = GetTypeName();
        Eat();
        e1 = GetASTBuilder()->BuildIdentifier(i);
        break;
    }
    case WGL_Token::TOKEN_PRIM_TYPE_NAME:
    {
        unsigned tok = GetTypeNameTag();
        Eat();
        WGL_Type *t = NULL;

        if (WGL_Context::IsVectorType(tok))
            t = GetASTBuilder()->BuildVectorType(WGL_Context::ToVectorType(tok));
        else if (WGL_Context::IsMatrixType(tok))
            t = GetASTBuilder()->BuildMatrixType(WGL_Context::ToMatrixType(tok));
        else if (WGL_Context::IsSamplerType(tok))
            t = GetASTBuilder()->BuildSamplerType(WGL_Context::ToSamplerType(tok));

        e1 = GetASTBuilder()->BuildTypeConstructor(t);
        break;
    }
    case WGL_Token::TOKEN_FLOAT:
    case WGL_Token::TOKEN_INT:
    case WGL_Token::TOKEN_BOOL:
    {
        Eat();
        WGL_Type *t = GetASTBuilder()->BuildBasicType(ToBasicType(static_cast<WGL_Token::Token>(tok)));
        e1 = GetASTBuilder()->BuildTypeConstructor(t);

        break;
    }
    case WGL_Token::TOKEN_CONST_TRUE:
    case WGL_Token::TOKEN_CONST_FALSE:
    {
        Eat();
        e1 = GetASTBuilder()->BuildBoolLit(tok == WGL_Token::TOKEN_CONST_TRUE);
        break;
    }
    case WGL_Token::TOKEN_CONST_FLOAT:
    case WGL_Token::TOKEN_CONST_INT:
    case WGL_Token::TOKEN_CONST_UINT:
    {
        WGL_Literal *l = GetLiteral(tok);
        Eat();
        e1 = GetASTBuilder()->BuildLiteral(l);
        break;
    }
    case WGL_Token::TOKEN_LPAREN:
    {
        Eat();
        REQUIRED(e1, Expression(), WGL_Error::EXPECTED_EXPR);
        EXPECT_MATCH(WGL_Token::TOKEN_RPAREN, ")");
        break;
    }
    default:
        return NULL;
    }

    if (MATCH(WGL_Token::TOKEN_LPAREN))
    {
        if (MATCH(WGL_Token::TOKEN_VOID))
        {
            EXPECT_MATCH(WGL_Token::TOKEN_RPAREN, ")");
            e1 = GetASTBuilder()->BuildCallExpression(e1, NULL);
        }
        else
        {
            WGL_ExprList *args = GetASTBuilder()->NewExprList();
            while (TRUE)
            {
                WGL_Expr *a1 = AssignmentExpression();
                if (a1) {
                    args->Add(a1);
                }
                if (MATCH(WGL_Token::TOKEN_RPAREN))
                {
                    // If we need to rewrite the texture coordinates from gl to dx coords.
                    if (rewriteTexCoords && args->list.Cardinal() > 1)
                    {
                        // Take the second argument, the texture coordinates, out of the AST.
                        WGL_Expr *texCoord = args->list.First()->Suc();
                        args->list.First()->Suc()->Out();

                        // Apply a translation and a division to it to change it from gl to dx coordinate system.
                        // (x, y) -> (x, 1 - y). We do it this way since we don't know the quality of the compiler
                        // and don't want to risk having the expression evaluated twice. We cannot use a
                        // multiplication since that will be treated as multiplying a row and a column matrix together.
                        WGL_Expr *flippedTexCoord = ApplyBinaryOpWithLiteralVec2(0, 1, WGL_Expr::OpSub, texCoord);
                        WGL_Expr *finalTexCoord = ApplyBinaryOpWithLiteralVec2(-1, 1, WGL_Expr::OpDiv, flippedTexCoord, false);

                        // Put the final texture coordinates back in to the AST again.
                        finalTexCoord->Follow(args->list.First());
                    }
                    e1 = GetASTBuilder()->BuildCallExpression(e1, args);
                    break;
                }
                if (NotMATCH(WGL_Token::TOKEN_COMMA))
                {
                    ErrorExpected(", or )", __LINE__);
                    return NULL;
                }
            }
        }
    }
    while ((tok=Current()) == WGL_Token::TOKEN_LBRACKET || tok == WGL_Token::TOKEN_DOT)
    {
        if (MATCH(WGL_Token::TOKEN_LBRACKET))
        {
            WGL_Expr *e2 = Expression();
            EXPECT_MATCH(WGL_Token::TOKEN_RBRACKET, "]");
            e1 = GetASTBuilder()->BuildIndexExpression(e1, e2);
            while (MATCH(WGL_Token::TOKEN_LBRACKET))
            {
                WGL_Expr *e2 = Expression();
                EXPECT_MATCH(WGL_Token::TOKEN_RBRACKET, "]");
                e1 = GetASTBuilder()->BuildIndexExpression(e1, e2);
            }
        }
        if (MATCH(WGL_Token::TOKEN_DOT))
        {
            if (NotMATCH(WGL_Token::TOKEN_IDENTIFIER))
                ErrorExpected(".<identifier>", __LINE__);
            else
            {
                WGL_VarName *i = GetIdentifier();
                e1 = GetASTBuilder()->BuildFieldSelect(e1, i);
            }
        }
    }
    if ((tok=Current()) == WGL_Token::TOKEN_INC || tok == WGL_Token::TOKEN_DEC)
    {
        Eat();
        return GetASTBuilder()->BuildPostOpExpression(e1, tok == WGL_Token::TOKEN_INC ? WGL_Expr::OpPostInc : WGL_Expr::OpPostDec);
    }
    return e1;
}

WGL_Expr *
WGL_Parser::ApplyBinaryOpWithLiteralVec2(int x, int y, WGL_Expr::Operator op, WGL_Expr *expr, BOOL expr_on_rhs)
{
    WGL_Expr *vec2Constructor = GetASTBuilder()->BuildTypeConstructor(GetASTBuilder()->BuildVectorType(WGL_VectorType::Vec2));
    WGL_ExprList *vec2ConstructorArgs = GetASTBuilder()->NewExprList();
    vec2ConstructorArgs->Add(GetASTBuilder()->BuildIntLit(x));
    vec2ConstructorArgs->Add(GetASTBuilder()->BuildIntLit(y));
    WGL_Expr *vec2ConstructorCall = GetASTBuilder()->BuildCallExpression(vec2Constructor, vec2ConstructorArgs);
    if (expr_on_rhs)
      return GetASTBuilder()->BuildBinaryExpression(op, vec2ConstructorCall, expr);
    else
      return GetASTBuilder()->BuildBinaryExpression(op, expr, vec2ConstructorCall);
}

WGL_StmtList*
WGL_Parser::StatementList(WGL_StmtList *ss)
{
    while (TRUE)
    {
        WGL_Stmt *s = Statement();
        if (!s)
            return ss;
        ss->Add(s);
    }
    return ss;
}

WGL_Stmt*
WGL_Parser::SwitchStatement()
{
    EXPECT_MATCH(WGL_Token::TOKEN_LPAREN, "(");
    WGL_Expr *e;
    REQUIRED(e, Expression(), WGL_Error::EXPECTED_EXPR);
    EXPECT_MATCH(WGL_Token::TOKEN_RPAREN, ")");
    EXPECT_MATCH(WGL_Token::TOKEN_LBRACE, "{");
    WGL_StmtList *ss = GetASTBuilder()->NewStmtList();
    StatementList(ss);
    EXPECT_MATCH(WGL_Token::TOKEN_RBRACE, "}");
    return GetASTBuilder()->BuildSwitchStmt(e, ss);
}

WGL_Stmt*
WGL_Parser::WhileStatement()
{
    WGL_Expr *e;
    EXPECT_MATCH(WGL_Token::TOKEN_LPAREN, "(");
    REQUIRED(e, Expression(), WGL_Error::EXPECTED_EXPR);
    EXPECT_MATCH(WGL_Token::TOKEN_RPAREN, ")");
    EXPECT_MATCH(WGL_Token::TOKEN_LBRACE, "{");
    WGL_Stmt *s = BlockStatement();
    return GetASTBuilder()->BuildWhileStmt(e, s);
}

WGL_Stmt*
WGL_Parser::ForStatement()
{
    EXPECT_MATCH(WGL_Token::TOKEN_LPAREN, "(");

    WGL_Stmt *s1 = NULL;
    WGL_Expr *e1 = NULL;
    WGL_Expr *e2 = NULL;

    if (MATCH(WGL_Token::TOKEN_SEMI))
        s1 = NULL;
    else
        REQUIRED(s1, Statement(), WGL_Error::EXPECTED_STMT);

    if (MATCH(WGL_Token::TOKEN_SEMI))
        e1 = NULL;
    else
        REQUIRED(e1, Expression(), WGL_Error::EXPECTED_EXPR);

    if (MATCH(WGL_Token::TOKEN_SEMI))
        e2 = Expression();

    if (NotMATCH(WGL_Token::TOKEN_RPAREN))
        ErrorExpected(")", __LINE__);

    WGL_Stmt *body = Statement();
    return GetASTBuilder()->BuildForStmt(s1, e1, e2, body);
}

WGL_Stmt*
WGL_Parser::IfStatement()
{
    WGL_Expr *e;
    WGL_Stmt *s;
    unsigned line_no = lexer->GetLineNumber();
    EXPECT_MATCH(WGL_Token::TOKEN_LPAREN, "(");
    REQUIRED(e, Expression(), WGL_Error::EXPECTED_EXPR);
    EXPECT_MATCH(WGL_Token::TOKEN_RPAREN, ")");
    REQUIRED(s, Statement(), WGL_Error::EXPECTED_STMT);
    if (NotMATCH(WGL_Token::TOKEN_ELSE))
        return GetASTBuilder()->BuildIfStmt(e, s, NULL, line_no);

    WGL_Stmt *el;
    REQUIRED(el, Statement(), WGL_Error::EXPECTED_STMT);
    return GetASTBuilder()->BuildIfStmt(e, s, el, line_no);
}

WGL_Stmt*
WGL_Parser::Statement()
{
    int tok = Current();

    if (MATCH(WGL_Token::TOKEN_SEMI))
        return GetASTBuilder()->BuildSimpleStmt(WGL_SimpleStmt::Empty);
    else if (MATCH(WGL_Token::TOKEN_LBRACE))
        if (MATCH(WGL_Token::TOKEN_RBRACE))
            return GetASTBuilder()->BuildSimpleStmt(WGL_SimpleStmt::Empty);
        else
            return BlockStatement();
    else if (MATCH(WGL_Token::TOKEN_SWITCH))
        return SwitchStatement();
    else if (MATCH(WGL_Token::TOKEN_WHILE))
        return WhileStatement();
    else if (MATCH(WGL_Token::TOKEN_DO))
        return DoStatement();
    else if (MATCH(WGL_Token::TOKEN_FOR))
        return ForStatement();
    else if (MATCH(WGL_Token::TOKEN_IF))
        return IfStatement();
    else if (MATCH(WGL_Token::TOKEN_DEFAULT))
    {
        EXPECT_MATCH(WGL_Token::TOKEN_COLON, ":");
        return GetASTBuilder()->BuildSimpleStmt(WGL_SimpleStmt::Default);
    }
    else if (MATCH(WGL_Token::TOKEN_CASE))
    {
        WGL_Expr *e = Expression();
        if (!e)
            Error(WGL_Error::EXPECTED_EXPR, __LINE__);
        EXPECT_MATCH(WGL_Token::TOKEN_COLON, ":");
        return GetASTBuilder()->BuildCaseLabel(e);
    }
    else if (MATCH(WGL_Token::TOKEN_CONTINUE) || MATCH(WGL_Token::TOKEN_BREAK) || MATCH(WGL_Token::TOKEN_DISCARD))
    {
        EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
        WGL_SimpleStmt::Type k = WGL_SimpleStmt::Continue;
        if (tok == WGL_Token::TOKEN_BREAK)
            k = WGL_SimpleStmt::Break;
        else if (tok == WGL_Token::TOKEN_DISCARD)
            k = WGL_SimpleStmt::Discard;
        return GetASTBuilder()->BuildSimpleStmt(k);
    }
    else if (MATCH(WGL_Token::TOKEN_RETURN))
    {
        if (MATCH(WGL_Token::TOKEN_SEMI))
            return GetASTBuilder()->BuildReturnStmt(NULL);
        WGL_Expr *e;
        REQUIRED(e, Expression(), WGL_Error::EXPECTED_EXPR);
        EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
        return GetASTBuilder()->BuildReturnStmt(e);
    }
    else if (Current() == WGL_Token::TOKEN_INVARIANT)
    {
        WGL_DeclStmt *decl_stmt = static_cast<WGL_DeclStmt *>(GetASTBuilder()->BuildDeclStmt(NULL));
        if (InvariantDeclaration(&decl_stmt->decls))
            return decl_stmt;
    }
    else if (MATCH(WGL_Token::TOKEN_PRECISION))
    {
        WGL_Type::Precision pq = PrecQualifier();
        WGL_Type *t = TypeSpecifier();
        EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
        return GetASTBuilder()->BuildDeclStmt(GetASTBuilder()->BuildPrecisionDecl(pq, t));
    }

    if (WGL_Decl *d = SingleDeclaration())
    {
        WGL_Type *type = NULL;
        WGL_DeclStmt *decl_stmt = static_cast<WGL_DeclStmt *>(GetASTBuilder()->BuildDeclStmt(d));

        switch (d->GetType())
        {
        case WGL_Decl::Var:
            type = static_cast<WGL_VarDecl *>(d)->type;
            break;
        case WGL_Decl::Array:
            type = static_cast<WGL_ArrayDecl *>(d)->type;
            break;
        default:
            return decl_stmt;
        }

        BOOL found = FALSE;
        while (LookingAt(WGL_Token::TOKEN_COMMA))
        {
            Eat();
            EXPECT_MATCH(WGL_Token::TOKEN_IDENTIFIER, "id");
            WGL_VarName *i = GetIdentifier();
            WGL_Decl *d1 = VarDeclPartial(type, i);
            if (!d1)
                break;
            decl_stmt->Add(d1);
            found = TRUE;
        }
        if (!found)
            EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
        return decl_stmt;
    }

    if (WGL_Expr *e = Expression())
    {
        EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
        return GetASTBuilder()->BuildExprStmt(e);
    }
    else
        return NULL;
}

WGL_Stmt*
WGL_Parser::DoStatement()
{
   WGL_Stmt *s;
   WGL_Expr *e;
   REQUIRED(s, Statement(), WGL_Error::EXPECTED_STMT);
   EXPECT_MATCH(WGL_Token::TOKEN_WHILE, "while");
   EXPECT_MATCH(WGL_Token::TOKEN_LPAREN, "(");
   REQUIRED(e, Expression(), WGL_Error::EXPECTED_EXPR);
   EXPECT_MATCH(WGL_Token::TOKEN_RPAREN, ")");
   EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");

   return GetASTBuilder()->BuildDoStmt(s,e);
}

WGL_Type *
WGL_Parser::TypeSpecifierNoPrec()
{
    int tok = Current();
    switch (tok)
    {
    case WGL_Token::TOKEN_VOID:
    case WGL_Token::TOKEN_FLOAT:
    case WGL_Token::TOKEN_INT:
    case WGL_Token::TOKEN_BOOL:
        Eat();
        return GetASTBuilder()->BuildBasicType(ToBasicType(static_cast<WGL_Token::Token>(tok)));

    case WGL_Token::TOKEN_PRIM_TYPE_NAME:
    {
        unsigned tok = GetTypeNameTag();
        Eat();

        if (WGL_Context::IsVectorType(tok))
            return GetASTBuilder()->BuildVectorType(WGL_Context::ToVectorType(tok));
        if (WGL_Context::IsMatrixType(tok))
            return GetASTBuilder()->BuildMatrixType(WGL_Context::ToMatrixType(tok));
        if (WGL_Context::IsSamplerType(tok))
            return GetASTBuilder()->BuildSamplerType(WGL_Context::ToSamplerType(tok));
        else
            Error(WGL_Error::ILLEGAL_TYPE_NAME, __LINE__);
    }
    case WGL_Token::TOKEN_TYPE_NAME:
    {
        WGL_VarName *i = GetTypeName();
        Eat();
        return GetASTBuilder()->BuildTypeName(i);
    }
    case WGL_Token::TOKEN_IDENTIFIER:
    {
        WGL_VarName *id = GetIdentifier();
        if (context->IsTypeName(id))
        {
            Eat();
            return GetASTBuilder()->BuildTypeName(id);
        }
        else
            return NULL;
    }
    case WGL_Token::TOKEN_STRUCT:
    {
        Eat();
        if (LookingAt(WGL_Token::TOKEN_IDENTIFIER))
        {
            WGL_VarName *i = GetIdentifier();
            Eat();
            EXPECT_MATCH(WGL_Token::TOKEN_LBRACE, "{");
            WGL_FieldList *s = StructDeclaration();
            EXPECT_MATCH(WGL_Token::TOKEN_RBRACE, "}");
            context->NewTypeName(i);
            return GetASTBuilder()->BuildStructType(i, s);
        }
        else if (NotMATCH(WGL_Token::TOKEN_LBRACE))
            Error(WGL_Error::ILLEGAL_STRUCT, __LINE__);

        WGL_FieldList *s = StructDeclaration();
        if (!s)
            Error(WGL_Error::ILLEGAL_STRUCT, __LINE__);
        EXPECT_MATCH(WGL_Token::TOKEN_RBRACE, "}");
        return GetASTBuilder()->BuildStructType(WGL_String::Make(context, UNI_L("anon")), s);
    }
    default:
        return NULL;
    }
}

WGL_FieldList*
WGL_Parser::StructDeclaration()
{
    WGL_FieldList *fs = GetASTBuilder()->NewFieldList();
    while (TRUE)
    {
        WGL_TypeQualifier *tq = TypeQualify(FALSE/*no invariant seen*/);
        WGL_Type *t = TypeSpecifier();
        if (!t)
            break;
        if (tq)
            t->AddTypeQualifier(tq);
        while (TRUE)
        {
            if (LookingAt(WGL_Token::TOKEN_IDENTIFIER))
            {
                WGL_VarName *i = GetIdentifier();
                Eat();
                if (MATCH(WGL_Token::TOKEN_LBRACKET))
                {
                    WGL_Expr *sz = Expression();
                    EXPECT_MATCH(WGL_Token::TOKEN_RBRACKET, "]");
                    fs->Add(context, GetASTBuilder()->BuildArrayType(t, sz), i);
                }
                else
                    fs->Add(context, t, i);
            }
            else
                Error(WGL_Error::ANON_FIELD, __LINE__);
            if (NotMATCH(WGL_Token::TOKEN_COMMA))
                break;
        }
        if (NotMATCH(WGL_Token::TOKEN_SEMI))
            break;
    }
    return fs;
}

WGL_Type::Precision
WGL_Parser::PrecQualifier()
{
    switch (Current())
    {
    case WGL_Token::TOKEN_HIGH_PREC:
        Eat();
        return WGL_Type::High;
    case WGL_Token::TOKEN_MED_PREC:
        Eat();
        return WGL_Type::Medium;
    case WGL_Token::TOKEN_LOW_PREC:
        Eat();
        return WGL_Type::Low;
    default:
        return WGL_Type::NoPrecision;
    }
}

WGL_Type *
WGL_Parser::TypeSpecifier()
{
    WGL_Type::Precision precLevel = WGL_Type::NoPrecision;
    int tok = Current();
    switch (tok)
    {
    case WGL_Token::TOKEN_HIGH_PREC:
    case WGL_Token::TOKEN_MED_PREC:
    case WGL_Token::TOKEN_LOW_PREC:
        Eat();
        precLevel = TO_PRECEDENCE_LEVEL(tok);
        break;
    }
    WGL_Type *t = TypeSpecifierNoPrec();
    if (t && precLevel > 0)
        return t->AddPrecision(precLevel);
    else
        return t;
}

WGL_Decl *
WGL_Parser::InvariantDeclaration(List<WGL_Decl> *ds)
{
    if (MATCH(WGL_Token::TOKEN_INVARIANT))
    {
        if (Current() != WGL_Token::TOKEN_IDENTIFIER)
        {
            WGL_Type *t = FullType(TRUE);
            if (Current() == WGL_Token::TOKEN_IDENTIFIER)
            {
                WGL_VarName *i = GetIdentifier();
                Eat();
                WGL_Decl *d1 = GetASTBuilder()->BuildTypeDecl(t, i);
                if (!d1)
                    return NULL;

                d1->Into(ds);
                while (MATCH(WGL_Token::TOKEN_COMMA))
                {
                    WGL_VarName *i = GetIdentifier();
                    Eat();
                    WGL_Decl *d = GetASTBuilder()->BuildTypeDecl(t, i);
                    if (!d)
                        return d1;

                    d->Into(ds);
                    if (Current() != WGL_Token::TOKEN_COMMA)
                        break;
                }
                return d1;
            }
        }
        WGL_VarName *i = GetIdentifier();
        Eat();
        WGL_Decl *invariant_decl = GetASTBuilder()->BuildInvariantDecl(i);
        WGL_InvariantDecl *next_decl = static_cast<WGL_InvariantDecl *>(invariant_decl);
        if (!invariant_decl)
            return NULL;

        invariant_decl->Into(ds);
        while (MATCH(WGL_Token::TOKEN_COMMA))
        {
            WGL_VarName *i = GetIdentifier();
            Eat();
            WGL_Decl *d = GetASTBuilder()->BuildInvariantDecl(i);
            if (!d)
                return invariant_decl;
            next_decl->next = static_cast<WGL_InvariantDecl *>(d);
            next_decl = next_decl->next;

            if (Current() != WGL_Token::TOKEN_COMMA)
                break;
        }
        return invariant_decl;
    }
    else
        return NULL;
}

WGL_Decl *
WGL_Parser::SingleDeclaration()
{
    WGL_Type *t = FullType(FALSE);
    if (!t)
        return NULL;

    if (Current() != WGL_Token::TOKEN_IDENTIFIER)
        return GetASTBuilder()->BuildTypeDecl(t,NULL);
    WGL_VarName *i = GetIdentifier();
    Eat();
    return VarDeclPartial(t, i);
}

WGL_Decl *
WGL_Parser::VarDeclPartial(WGL_Type *t, WGL_VarName *i)
{
    if (MATCH(WGL_Token::TOKEN_EQUAL))
    {
        WGL_Expr *e = AssignmentExpression();
        return GetASTBuilder()->BuildVarDecl(t,i,e);
    }
    if (NotMATCH(WGL_Token::TOKEN_LBRACKET))
        return GetASTBuilder()->BuildVarDecl(t,i,NULL);
    WGL_Expr *c = Expression();
    if (!c)
        Error(WGL_Error::EXPECTED_EXPR, __LINE__);
    EXPECT_MATCH(WGL_Token::TOKEN_RBRACKET, "]");
    if (NotMATCH(WGL_Token::TOKEN_EQUAL))
        return GetASTBuilder()->BuildArrayDecl(t,i,c,NULL);
    WGL_Expr *is = Expression();
    if (!is)
        Error(WGL_Error::EXPECTED_EXPR, __LINE__);
    return GetASTBuilder()->BuildArrayDecl(t,i,c,is);
}

WGL_DeclList*
WGL_Parser::InitDeclarators(WGL_DeclList *ds, WGL_Decl *d)
{
    WGL_DeclList *ds_new = NULL;
    WGL_Type *var_decl_type = NULL;

    if (d && d->GetType() == WGL_Decl::Var)
    {
        WGL_VarDecl *var_decl = static_cast<WGL_VarDecl *>(d);
        var_decl_type = var_decl->type;
        d = VarDeclPartial(var_decl_type, var_decl->identifier);
    }

    if (d)
        ds->Add(d);
    else if ((d = SingleDeclaration()))
        ds->Add(d);

    if (!d)
        return ds_new;
    else
        ds_new = ds;

    while (TRUE)
    {
        if (Current() == WGL_Token::TOKEN_SEMI)
            return ds_new;
        if (NotMATCH(WGL_Token::TOKEN_COMMA))
            return ds_new;
        if (LookingAt(WGL_Token::TOKEN_IDENTIFIER) && var_decl_type)
        {
            WGL_VarName *i = GetIdentifier();
            Eat();
            d = VarDeclPartial(var_decl_type, i);
            if (d)
                ds_new->Add(d);
        }
        else
            Error(WGL_Error::EXPECTED_ID, __LINE__);
    }
    return ds_new;
}

WGL_Type *
WGL_Parser::FullType(BOOL seenInvariant)
{
    WGL_TypeQualifier *tq = TypeQualify(seenInvariant);
    WGL_Type *t = TypeSpecifier();
    if (t && tq)
        return t->AddTypeQualifier(tq);
    else
        return t;
}

/* Return a valid type qualifier, if matched. NULL otherwise (or error raised) */
WGL_TypeQualifier*
WGL_Parser::TypeQualify(BOOL seenInvariant)
{
    WGL_TypeQualifier *tq = GetASTBuilder()->NewTypeQualifier();
    WGL_TypeQualifier::InvariantKind iq = WGL_TypeQualifier::Invariant;
    if (seenInvariant || ((iq = TypeQualifierInvariant()) != WGL_TypeQualifier::NoInvariant))
    {
        tq = tq->AddInvariant(iq);
        EXPECT_MATCH(WGL_Token::TOKEN_VARYING, "varying");
        return tq->AddStorage(WGL_TypeQualifier::Varying);
    }
    else
    {
        WGL_LayoutList *lq = TypeQualifierLayout();
        if (lq != NULL)
        {
            tq = tq->AddLayout(lq);
            WGL_TypeQualifier::Storage sq = TypeStorageQualifier();
            if (sq != WGL_TypeQualifier::NoStorage)
                return tq->AddStorage(sq);
            else
                return tq;
        }
        WGL_TypeQualifier::Storage sq = TypeStorageQualifier();
        if (sq != WGL_TypeQualifier::NoStorage)
            return tq->AddStorage(sq);
        else
            return NULL;
    }
}

WGL_TypeQualifier::Storage
WGL_Parser::TypeStorageQualifier()
{
    int tok = Current();
    switch (Current())
    {
    case WGL_Token::TOKEN_CONST:
    case WGL_Token::TOKEN_ATTRIBUTE:
    case WGL_Token::TOKEN_VARYING:
    case WGL_Token::TOKEN_UNIFORM:
    case WGL_Token::TOKEN_IN_:
    case WGL_Token::TOKEN_OUT_:
        Eat();
        return TO_STORAGE_QUALIFIER(tok);
    default:
        return WGL_TypeQualifier::NoStorage;
    }
}

/*
 * Returns a layout type qualifier if matched; NULL if no LAYOUT herald
 * seen; error if a partial parse.
 */
WGL_LayoutList *
WGL_Parser::TypeQualifierLayout()
{
    if (NotMATCH(WGL_Token::TOKEN_LAYOUT))
        return NULL;
    EXPECT_MATCH(WGL_Token::TOKEN_LPAREN, "(");
    WGL_LayoutList *ls = GetASTBuilder()->NewLayoutList();
    while (TRUE)
    {
        if (MATCH(WGL_Token::TOKEN_RPAREN))
            break;
        else if (!LookingAt(WGL_Token::TOKEN_IDENTIFIER))
            Error(WGL_Error::ILLEGAL_TYPE_LAYOUT, __LINE__);
        WGL_VarName *i = GetIdentifier();
        Eat();
        if (MATCH(WGL_Token::TOKEN_COMMA))
        {
            ls->Add(GetASTBuilder()->BuildLayoutTypeQualifier(i,(-1)));
            continue;
        }
        EXPECT_MATCH(WGL_Token::TOKEN_EQUAL, "=");
        if (!LookingAt(WGL_Token::TOKEN_CONST_INT))
            Error(WGL_Error::ILLEGAL_TYPE_LAYOUT, __LINE__);
        int v = GetIntLit();
        Eat();
        ls->Add(GetASTBuilder()->BuildLayoutTypeQualifier(i,v));
        EXPECT_MATCH(WGL_Token::TOKEN_COMMA, ",");
    }
    return ls;
}

WGL_TypeQualifier::InvariantKind
WGL_Parser::TypeQualifierInvariant()
{
    int tok = Current();
    switch (tok)
    {
    case WGL_Token::TOKEN_INVARIANT:
        Eat();
        return TO_INVARIANT_QUALIFIER(tok);
    default:
        return WGL_TypeQualifier::NoInvariant;
    }
}

WGL_DeclList*
WGL_Parser::TopDecls()
{
    WGL_DeclList *ts = GetASTBuilder()->NewDeclList();
    WGL_Decl *d;

    while (!LookingAt(WGL_Token::TOKEN_EOF))
    {
        WGL_Decl *tyd = NULL;
        unsigned fun_start = lexer->GetLineNumber();
        tyd = InvariantDeclaration(&ts->list);
        if (tyd)
        {
            EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
            continue;
        }

        d = FunctionPrototype(tyd);
        if (tyd)
        {
            InitDeclarators(ts, tyd);
            EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
        }
        else if (!d)
        {
            if (!(Declaration(ts)))
                break;
            OPTIONAL_MATCH(WGL_Token::TOKEN_SEMI);
        }
        else
        {
            if (MATCH(WGL_Token::TOKEN_LBRACE))
            {
                WGL_Stmt *s = BlockStatement();
                unsigned fun_end = lexer->GetLineNumber();
                GetASTBuilder()->SetLineNumber(fun_start);
                ts->Add(GetASTBuilder()->BuildFunctionDefn(d, s));
                GetASTBuilder()->SetLineNumber(fun_end);
            }
            else
            {
                OPTIONAL_MATCH(WGL_Token::TOKEN_SEMI);
                ts->Add(d);
            }
        }
    }
    if (!LookingAt(WGL_Token::TOKEN_EOF))
        ParseError();

    return ts;
}

WGL_Stmt *
WGL_Parser::BlockStatement()
{
    WGL_StmtList *ss = GetASTBuilder()->NewStmtList();
    while (TRUE)
    {
        if (MATCH(WGL_Token::TOKEN_RBRACE))
            break;
        WGL_Stmt *s = Statement();
        if (!s)
        {
            Error(WGL_Error::EXPECTED_STMT, __LINE__);
            break;
        }
        else
        {
            ss->Add(s);
            if (Current() == WGL_Token::TOKEN_SEMI)
                Eat();
        }
    }
    return GetASTBuilder()->BuildBlockStmt(ss);
}

WGL_DeclList*
WGL_Parser::Declaration(WGL_DeclList *ds)
{
    if (MATCH(WGL_Token::TOKEN_PRECISION))
    {
        WGL_Type::Precision pq = PrecQualifier();
        WGL_Type *t = TypeSpecifier();
        EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
        return ds->Add(GetASTBuilder()->BuildPrecisionDecl(pq, t));
    }
    WGL_Decl *tydecl = NULL;
    WGL_Decl *proto = FunctionPrototype(tydecl);
    if (proto)
    {
        EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
        return ds->Add(proto);
    }
    WGL_DeclList *ds_added = InitDeclarators(ds, tydecl);
    if (ds_added)
    {
        EXPECT_MATCH(WGL_Token::TOKEN_SEMI, ";");
        return ds;
    }
    return NULL;
}

WGL_Decl *
WGL_Parser::FunctionPrototype(WGL_Decl *&tyd)
{
    WGL_Type *ret_ty = FullType(FALSE);
    if (!ret_ty)
        return NULL;
    if (LookingAt(WGL_Token::TOKEN_IDENTIFIER))
    {
        WGL_VarName *fun = GetIdentifier();
        Eat();
        if (NotMATCH(WGL_Token::TOKEN_LPAREN))
        {
            tyd = GetASTBuilder()->BuildVarDecl(ret_ty, fun, NULL);
            return NULL;
        }
        WGL_ParamList *ps = GetASTBuilder()->NewParamList();
        if (MATCH(WGL_Token::TOKEN_VOID))
        {
            EXPECT_MATCH(WGL_Token::TOKEN_RPAREN, ")");
            return GetASTBuilder()->BuildFunctionProto(ret_ty, fun, ps);
        }
        while (TRUE)
        {
            if (MATCH(WGL_Token::TOKEN_RPAREN))
                return GetASTBuilder()->BuildFunctionProto(ret_ty, fun, ps);

            OPTIONAL_MATCH(WGL_Token::TOKEN_CONST);
            int tok = Current();
            WGL_Param::Direction pqual = WGL_Param::None;
            if (tok == WGL_Token::TOKEN_IN_ || tok == WGL_Token::TOKEN_OUT_ || tok == WGL_Token::TOKEN_INOUT)
            {
                pqual = TO_PARAM_DIRECTION(tok);
                Eat();
            }
            WGL_Type *t = TypeSpecifier();
            if (!t)
                return NULL;
            WGL_VarName *formal = NULL;
            if (LookingAt(WGL_Token::TOKEN_IDENTIFIER))
            {
                formal = GetIdentifier();
                Eat();
            }
            if (MATCH(WGL_Token::TOKEN_COMMA))
            {
                if (!formal)
                    formal = context->NewUniqueVar(TRUE);
                ps->Add(GetASTBuilder()->BuildParam(t, pqual, formal));
            }
            else if (MATCH(WGL_Token::TOKEN_RPAREN))
            {
                if (!formal)
                    formal = context->NewUniqueVar(TRUE);
                ps->Add(GetASTBuilder()->BuildParam(t, pqual, formal));
                return GetASTBuilder()->BuildFunctionProto(ret_ty, fun, ps);
            }
            else if (formal && MATCH(WGL_Token::TOKEN_LBRACKET))
            {
                WGL_Expr *cs;
                REQUIRED(cs, Expression(), WGL_Error::EXPECTED_EXPR);
                EXPECT_MATCH(WGL_Token::TOKEN_RBRACKET, "]");
                if (MATCH(WGL_Token::TOKEN_COMMA))
                    ps->Add(GetASTBuilder()->BuildParam(GetASTBuilder()->BuildArrayType(t,cs), pqual, formal));
                else if (MATCH(WGL_Token::TOKEN_RPAREN))
                {
                    ps->Add(GetASTBuilder()->BuildParam(GetASTBuilder()->BuildArrayType(t,cs), pqual, formal));
                    return GetASTBuilder()->BuildFunctionProto(ret_ty, fun, ps);
                }
                else
                    Error(WGL_Error::EXPECTED_COMMA, __LINE__);
            }
            else
                Error(WGL_Error::EXPECTED_COMMA, __LINE__);
        }
    }
    tyd = GetASTBuilder()->BuildTypeDecl(ret_ty, NULL);
    return NULL;
}

/* static */ void
WGL_Parser::MakeL(WGL_Parser *&parser, WGL_Context *context, WGL_Lexer *l, BOOL for_vertex, BOOL rewrite_tc)
{
    context->ResetL(for_vertex);
    parser = OP_NEWGRO_L(WGL_Parser, (), context->Arena());

    parser->rewrite_texcoords = rewrite_tc;
    parser->lexer = l;
    parser->context = context;
}

int
WGL_Parser::Current()
{
    if (token < 0)
        token = lexer->GetToken();

    return token;
}

int
WGL_Parser::Match(int l, int ll)
{
    if (token == l)
    {
        Eat();
        return 1;
    }
    return 0;
}
int
WGL_Parser::NotMatch(int l, int ll)
{
    if (token != l)
        return 1;

    Eat();
    return 0;
}

int
WGL_Parser::LookingAt(int l)
{
    return (token == l);
}

void
WGL_Parser::Eat(BOOL update_line_number)
{
    token = lexer->GetToken();
    if (update_line_number)
        GetASTBuilder()->SetLineNumber(lexer->GetLineNumber());
}

WGL_VarName *
WGL_Parser::GetIdentifier()
{
    return lexer->lexeme.val_id;
}

WGL_VarName *
WGL_Parser::GetTypeName()
{
    return lexer->lexeme.val_id;
}

unsigned
WGL_Parser::GetTypeNameTag()
{
    return lexer->lexeme.val_ty;
}

WGL_Literal *
WGL_Parser::GetLiteral(unsigned t)
{
    switch (t)
    {
    case WGL_Token::TOKEN_CONST_UINT:
        return OP_NEWGRO_L(WGL_Literal, (lexer->lexeme.val_uint), context->Arena());
    case WGL_Token::TOKEN_CONST_INT:
        return OP_NEWGRO_L(WGL_Literal, (lexer->lexeme.val_int), context->Arena());
    case WGL_Token::TOKEN_CONST_FLOAT:
    {
        /* Passing along unreasonably long literals may cause the driver's compiler
         * to complain (ATI?), so just don't. The limit used is such that there'll
         * be no loss in precision.
         */
        unsigned lit_length = static_cast<unsigned>(uni_strlen(lexer->numeric_buffer));
        uni_char *numeric_str = NULL;
        if (lit_length < WGL_MAX_DOUBLE_LIT_LENGTH)
        {
            numeric_str = OP_NEWGROA_L(uni_char, lit_length + 1, context->Arena());
            uni_strcpy(numeric_str, lexer->numeric_buffer);
        }
        return OP_NEWGRO_L(WGL_Literal, (lexer->lexeme.val_double, numeric_str), context->Arena());
    }
    case WGL_Token::TOKEN_CONST_FALSE:
        return OP_NEWGRO_L(WGL_Literal,(FALSE), context->Arena());
    case WGL_Token::TOKEN_CONST_TRUE:
        return OP_NEWGRO_L(WGL_Literal, (TRUE), context->Arena());
    default:
        return OP_NEWGRO_L(WGL_Literal, (0), context->Arena());
    }
}

int
WGL_Parser::GetIntLit()
{
    return lexer->lexeme.val_int;
}

void
WGL_Parser::Error(WGL_Error::Type t, int l)
{
    WGL_Error *item = OP_NEWGRO_L(WGL_Error, (context, t, context->GetSourceContext(), l), context->Arena());

    context->AddError(item);
#if defined(WGL_STANDALONE) && defined(_DEBUG)
    fprintf(stderr, "ERROR, line %d: %d\n", lexer->GetLineNumber(), t);
    fprintf(stderr, "[debug: from parser line %d]\n", l);
    fflush(stderr);
#endif // WGL_STANDALONE && _DEBUG
}

void
WGL_Parser::Error(WGL_Error::Type t, WGL_VarName *i)
{
    WGL_Error *item = OP_NEWGRO_L(WGL_Error, (context, t, context->GetSourceContext(), lexer->GetLineNumber()), context->Arena());

    item->SetMessageL(WGL_String::Storage(context, i));
    context->AddError(item);
}

void
WGL_Parser::ErrorExpected(const char *s, int l)
{
    WGL_Error *item = OP_NEWGRO_L(WGL_Error, (context, WGL_Error::EXPECTED_INPUT, context->GetSourceContext(), lexer->GetLineNumber()), context->Arena());

    item->SetMessageL(s);
    context->AddError(item);
#if defined(WGL_STANDALONE) && defined(_DEBUG)
    fprintf(stderr, "ERROR expected %s, line %d\n", s, lexer->GetLineNumber());
    fprintf(stderr, "[debug: from parser line %d]\n", l);
    fflush(stderr);
#endif // WGL_STANDALONE && _DEBUG
}

void
WGL_Parser::ParseError()
{
    WGL_Error *item = OP_NEWGRO_L(WGL_Error, (context, WGL_Error::PARSE_ERROR, context->GetSourceContext(), lexer->GetLineNumber()), context->Arena());

    uni_char buf[100]; // ARRAY OK 2010-12-07 sof
    lexer->GetErrorContext(buf, 100);
    item->SetMessageL(buf);
    context->AddError(item);
}

BOOL
WGL_Parser::HaveErrors()
{
    return (context->GetFirstError() != NULL);
}

#endif // CANVAS3D_SUPPORT
