/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_STATEMENTS_H
#define ES_STATEMENTS_H


#include "modules/ecmascript/carakan/src/compiler/es_compiler.h"
#include "modules/ecmascript/carakan/src/compiler/es_source_location.h"


class ES_Expression;


#define STATEMENT_FUNCTIONS \
    virtual void Prepare(ES_Compiler &compiler); \
    virtual BOOL Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst); \
    virtual BOOL CallVisitor(ES_StatementVisitor *visitor);

#define STATEMENT_FUNCTIONS_NO_PREPARE \
    virtual BOOL Compile (ES_Compiler &compiler, const ES_Compiler::Register &dst); \
    virtual BOOL CallVisitor(ES_StatementVisitor *visitor);


class ES_Statement
{
public:
    enum Type
    {
        TYPE_BLOCK,
        TYPE_VARIABLEDECL,
        TYPE_EMPTY,
        TYPE_EXPRESSION,
        TYPE_IF,
        TYPE_WHILE,
        TYPE_DO_WHILE,
        TYPE_FOR,
        TYPE_FORIN,
        TYPE_CONTINUE,
        TYPE_BREAK,
        TYPE_RETURN,
        TYPE_WITH,
        TYPE_SWITCH,
        TYPE_LABELLED,
        TYPE_THROW,
        TYPE_TRY,
        TYPE_DEBUGGER
    };

    ES_Statement();
    ES_Statement(Type type);

    Type GetType() { return type; }

    STATEMENT_FUNCTIONS

    BOOL IsEmpty();
    BOOL IsBlock();

    static BOOL VisitStatements(ES_Statement **statements, unsigned statements_count, ES_StatementVisitor *visitor);

    virtual BOOL NoExit() { return FALSE; }

    void SetSourceLocation(const ES_SourceLocation &l) { location = l; }
    const ES_SourceLocation& GetSourceLocation() const { return location; }
    void SetLength(unsigned length) { location.SetLength(length); }

protected:
    ES_SourceLocation location;

    Type type;
};


class ES_BlockStmt
    : public ES_Statement
{
public:
    ES_BlockStmt(unsigned statements_count, ES_Statement **statements)
        : ES_Statement(TYPE_BLOCK),
          statements_count(statements_count),
          statements(statements)
    {
    }

    STATEMENT_FUNCTIONS

    virtual BOOL NoExit();

    ES_Statement *GetLastStatement()
    {
        if (statements_count > 0)
            return statements[statements_count - 1];
        else
            return 0;
    }

private:
    unsigned statements_count;
    ES_Statement **statements;
};


class ES_VariableDeclStmt
    : public ES_Statement
{
public:
    ES_VariableDeclStmt(unsigned declarations_count, JString **names, ES_Expression **initializers)
        : ES_Statement(TYPE_VARIABLEDECL),
          declarations_count(declarations_count),
          names(names),
          initializers(initializers)
    {
    }

    /* For use on variable declaration statements used as children of
       ES_ForInStmt, where the syntax only allows a single declaration. */
    JString *GetName() { OP_ASSERT(declarations_count == 1); return names[0]; }
    ES_Expression *GetInitializer() { OP_ASSERT(declarations_count == 1); return initializers[0]; }

    unsigned GetCount() { return declarations_count; }
    JString *GetName(unsigned index) { return names[index]; }
    ES_Expression *GetInitializer(unsigned index) { return initializers[index]; }

    STATEMENT_FUNCTIONS

private:
    unsigned declarations_count;
    JString **names;
    ES_Expression **initializers;
};


class ES_EmptyStmt
  : public ES_Statement
{
public:
    ES_EmptyStmt(BOOL is_block = FALSE)
        : ES_Statement(TYPE_EMPTY),
          is_block(is_block)
    {
    }

    virtual BOOL Compile (ES_Compiler &compiler, const ES_Compiler::Register &dst);

    BOOL IsBlock() { return is_block; }

protected:
    BOOL is_block;
};


class ES_ExpressionStmt
  : public ES_Statement
{
public:
    ES_ExpressionStmt(ES_Expression *expression);

    STATEMENT_FUNCTIONS_NO_PREPARE

    ES_Expression *GetExpression() { return expression; }

private:
    ES_Expression *expression;
};


class ES_IfStmt
  : public ES_Statement
{
public:
    ES_IfStmt(ES_Expression *condition, ES_Statement *ifstmt, ES_Statement *elsestmt0)
        : ES_Statement(TYPE_IF),
          condition(condition),
          ifstmt(ifstmt),
          elsestmt(elsestmt0)
    {
        if (elsestmt && elsestmt->IsEmpty())
            elsestmt = NULL;
    }

    STATEMENT_FUNCTIONS

    virtual BOOL NoExit();

    ES_Expression *GetCondition() { return condition; }
    ES_Statement *GetTrueBranch() { return ifstmt; }
    ES_Statement *GetFalseBranch() { return elsestmt; }

private:
    ES_Expression *condition;
    ES_Statement *ifstmt;
    ES_Statement *elsestmt;
};


class ES_WhileStmt
    : public ES_Statement
{
public:
    ES_WhileStmt(Type type, ES_Expression *condition, ES_Statement *body)
        : ES_Statement(type),
          condition(condition),
          body(body)
    {
    }

    STATEMENT_FUNCTIONS

private:
    ES_Expression *condition;
    ES_Statement *body;
};


class ES_ForStmt
  : public ES_Statement
{
public:
    enum Type
    {
        INITTYPE_EXPRESSION,
        INITTYPE_VARIABLEDECL
    };

    ES_ForStmt(ES_ExpressionStmt *init_stmt, ES_Expression *condition, ES_ExpressionStmt *iteration, ES_Statement *body)
        : ES_Statement(TYPE_FOR),
          type(INITTYPE_EXPRESSION),
          condition(condition),
          iteration(iteration),
          body(body)
    {
        init.expr = init_stmt;
    }

    ES_ForStmt(ES_VariableDeclStmt *init_vardecl, ES_Expression *condition, ES_ExpressionStmt *iteration, ES_Statement *body)
        : ES_Statement(TYPE_FOR),
          type(INITTYPE_VARIABLEDECL),
          condition(condition),
          iteration(iteration),
          body(body)
    {
        init.vardecl = init_vardecl;
    }

    STATEMENT_FUNCTIONS

private:
    BOOL IsSimpleLoopVariable(ES_Compiler &compiler, JString *&name, int &lower_bound, int &upper_bound);

    Type type;
    union
    {
        ES_ExpressionStmt *expr;
        ES_VariableDeclStmt *vardecl;
    } init;
    ES_Expression *condition;
    ES_ExpressionStmt *iteration;
    ES_Statement *body;
};


class ES_ForInStmt
  : public ES_Statement
{
public:
    enum Type
    {
        TYPE_EXPRESSION,
        TYPE_VARIABLEDECL
    };

    ES_ForInStmt(ES_Expression *target_expr, ES_Expression *source, ES_Statement *body)
        : ES_Statement(TYPE_FORIN),
          type(TYPE_EXPRESSION),
          source(source),
          body(body)
    {
        target.expr = target_expr;
    }

    ES_ForInStmt(ES_VariableDeclStmt *init_decl, ES_Expression *source, ES_Statement *body)
        : ES_Statement(TYPE_FORIN),
          type(TYPE_VARIABLEDECL),
          source(source),
          body(body)
    {
        target.vardecl = init_decl;
    }

    STATEMENT_FUNCTIONS

    ES_Expression *GetTargetExpression() { return type == TYPE_EXPRESSION ? target.expr : NULL; }

private:
    Type type;

    union
    {
        ES_Expression *expr;
        ES_VariableDeclStmt *vardecl;
    } target;

    ES_Expression *source;
    ES_Statement *body;
};


class ES_ContinueStmt
  : public ES_Statement
{
public:
    ES_ContinueStmt(JString *target)
        : ES_Statement(TYPE_CONTINUE),
          target(target)
    {
    }

    virtual BOOL Compile (ES_Compiler &compiler, const ES_Compiler::Register &dst);
    virtual BOOL NoExit() { return TRUE; }

private:
  JString *target;
};


class ES_BreakStmt
  : public ES_Statement
{
public:
    ES_BreakStmt(JString *target)
        : ES_Statement(TYPE_BREAK),
          target(target)
    {
    }

    virtual BOOL Compile (ES_Compiler &compiler, const ES_Compiler::Register &dst);
    virtual BOOL NoExit() { return TRUE; }

private:
    JString *target;
};


class ES_ReturnStmt
  : public ES_Statement
{
public:
    ES_ReturnStmt(ES_Expression *expression)
        : ES_Statement(TYPE_RETURN),
          expression(expression)
    {
    }

    STATEMENT_FUNCTIONS_NO_PREPARE

    virtual BOOL NoExit() { return TRUE; }

    ES_Expression *GetReturnValue() { return expression; }

private:
    ES_Expression *expression;
};


class ES_WithStmt
  : public ES_Statement
{
public:
    ES_WithStmt(ES_Expression *expression, ES_Statement *body)
        : ES_Statement(TYPE_WITH),
          expression(expression),
          body(body)
    {
    }

    STATEMENT_FUNCTIONS

private:
    ES_Expression *expression;
    ES_Statement *body;
};


class ES_SwitchStmt
  : public ES_Statement
{
public:
    class CaseClause
    {
    public:
        ES_Expression *expression;
        unsigned statements_count;

        ES_SourceLocation location;

        CaseClause *next;

    protected:
        friend class ES_SwitchStmt;
        ES_Compiler::JumpTarget jump_target;
    };

    ES_SwitchStmt(ES_Expression *expression, CaseClause *case_clauses, unsigned statements_count, ES_Statement **statements)
        : ES_Statement(TYPE_SWITCH),
          expression(expression),
          case_clauses(case_clauses),
          statements_count(statements_count),
          statements(statements)
    {
    }

    STATEMENT_FUNCTIONS

private:
    ES_Expression *expression;
    CaseClause *case_clauses;
    unsigned statements_count;
    ES_Statement **statements;
};


class ES_LabelledStmt
  : public ES_Statement
{
public:
    ES_LabelledStmt(JString *label, ES_Statement *statement)
        : ES_Statement(TYPE_LABELLED),
          label(label),
          statement(statement)
    {
    }

    STATEMENT_FUNCTIONS

private:
    JString *label;
    ES_Statement *statement;
};


class ES_ThrowStmt
  : public ES_Statement
{
public:
    ES_ThrowStmt(ES_Expression *expression)
        : ES_Statement(TYPE_THROW),
          expression(expression)
    {
    }

    STATEMENT_FUNCTIONS_NO_PREPARE

    virtual BOOL NoExit() { return TRUE; }

private:
    ES_Expression *expression;
};


class ES_TryStmt
  : public ES_Statement
{
public:
    ES_TryStmt(ES_Statement *try_block, JString *catch_identifier, ES_Statement *catch_block, ES_Statement *finally_block)
        : ES_Statement(TYPE_TRY),
          try_block(try_block),
          catch_identifier(catch_identifier),
          catch_block(catch_block),
          finally_block(finally_block)
    {
    }

    STATEMENT_FUNCTIONS

private:
    ES_Statement *try_block;
    JString *catch_identifier;
    ES_Statement *catch_block;
    ES_Statement *finally_block;
};


#ifdef ECMASCRIPT_DEBUGGER
class ES_DebuggerStmt
  : public ES_Statement
{
public:
    ES_DebuggerStmt()
        : ES_Statement(TYPE_DEBUGGER)
    {
    }

    virtual BOOL Compile (ES_Compiler &compiler, const ES_Compiler::Register &dst);
};
#endif // ECMASCRIPT_DEBUGGER


#endif /* ES_STATEMENTS_H */
