/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_EXPRESSIONS_H
#define ES_EXPRESSIONS_H


#include "modules/ecmascript/carakan/src/compiler/es_compiler.h"
#include "modules/ecmascript/carakan/src/compiler/es_source_location.h"


#define COMPILE_AS_CONDITION virtual void CompileAsCondition(ES_Compiler &compiler, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index = UINT_MAX)
#define INTO_REGISTER virtual void IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
#define AS_REGISTER virtual ES_Compiler::Register AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary = NULL)
#define CALL_VISITOR virtual BOOL CallVisitor(ES_ExpressionVisitor *visitor)


class ES_Expression
{
public:
    enum Type
    {
        TYPE_THIS,
        TYPE_IDENTIFIER,
        TYPE_LITERAL,
        TYPE_REGEXP_LITERAL,
        TYPE_ARRAY_LITERAL,
        TYPE_OBJECT_LITERAL,
        TYPE_FUNCTION,
        TYPE_ARRAY_REFERENCE,
        TYPE_PROPERTY_REFERENCE,
        TYPE_CALL,
        TYPE_NEW,
        TYPE_INCREMENT_OR_DECREMENT,
        TYPE_DELETE,
        TYPE_VOID,
        TYPE_TYPEOF,
        TYPE_PLUS,
        TYPE_MINUS,
        TYPE_BITWISE_NOT,
        TYPE_LOGICAL_NOT,
        TYPE_MULTIPLY,
        TYPE_DIVIDE,
        TYPE_REMAINDER,
        TYPE_SUBTRACT,
        TYPE_ADD,
        TYPE_SHIFT_LEFT,
        TYPE_SHIFT_SIGNED_RIGHT,
        TYPE_SHIFT_UNSIGNED_RIGHT,
        TYPE_LESS_THAN,
        TYPE_GREATER_THAN,
        TYPE_LESS_THAN_OR_EQUAL,
        TYPE_GREATER_THAN_OR_EQUAL,
        TYPE_INSTANCEOF,
        TYPE_IN,
        TYPE_EQUAL,
        TYPE_NOT_EQUAL,
        TYPE_STRICT_EQUAL,
        TYPE_STRICT_NOT_EQUAL,
        TYPE_BITWISE_AND,
        TYPE_BITWISE_XOR,
        TYPE_BITWISE_OR,
        TYPE_LOGICAL_AND,
        TYPE_LOGICAL_OR,
        TYPE_CONDITIONAL,
        TYPE_ASSIGN,
        TYPE_COMMA
    };

    enum Production
    {
        PROD_EXPRESSION,
        PROD_ASSIGNMENT_EXPR,
        PROD_CONDITIONAL_EXPR,
        PROD_LOGICAL_OR_EXPR,
        PROD_LOGICAL_AND_EXPR,
        PROD_BITWISE_OR_EXPR,
        PROD_BITWISE_XOR_EXPR,
        PROD_BITWISE_AND_EXPR,
        PROD_EQUALITY_EXPR,
        PROD_RELATIONAL_EXPR,
        PROD_SHIFT_EXPR,
        PROD_ADDITIVE_EXPR,
        PROD_MULTIPLICATIVE_EXPR,
        PROD_UNARY_EXPR,
        PROD_POSTFIX_EXPR,
        PROD_LEFT_HAND_SIDE_EXPR,
        PROD_CALL_EXPR,
        PROD_NEW_EXPR,
        PROD_MEMBER_EXPR,
        PROD_PRIMARY_EXPR
    };

    ES_Expression(Type type, ES_ValueType value_type)
        : type(type),
          value_type(value_type),
          in_void_context(FALSE),
          in_boolean_context(FALSE),
          is_disabled(FALSE)
    {
        if (value_type == ESTYPE_INT32)
            this->value_type = ESTYPE_DOUBLE;
    }

    Type GetType() { return type; }
    ES_ValueType GetValueType() { return value_type; }
    BOOL IsPrimitiveType() { return value_type != ESTYPE_UNDEFINED && value_type != ESTYPE_OBJECT; }

    void SetInVoidContext() { in_void_context = TRUE; }
    void SetInBooleanContext() { in_boolean_context = TRUE; }

    void CompileInVoidContext(ES_Compiler &compiler);

    COMPILE_AS_CONDITION;
    INTO_REGISTER;
    AS_REGISTER;
    CALL_VISITOR;

    BOOL IsLValue();
    BOOL ImplicitBooleanResult();
    BOOL IsKnownCondition(ES_Compiler &compiler, BOOL &result);

    BOOL IsImmediate(ES_Compiler &compiler);
    BOOL GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value);

    BOOL IsImmediateInteger(ES_Compiler &compiler);
    BOOL GetImmediateInteger(ES_Compiler &compiler, int &value);

    BOOL IsImmediateString(ES_Compiler &compiler);
    BOOL GetImmediateString(ES_Compiler &compiler, JString *&value);

    BOOL CanHaveSideEffects(ES_Compiler &compiler);

    void Disable() { is_disabled = TRUE; }

    BOOL IsIdentifier(JString *&name);

    BOOL HasSourceLocation() { return location.IsValid(); }
    void SetSourceLocation(const ES_SourceLocation &l) { location = l; }
    void SetLength(unsigned length) { location.SetLength(length); }
    const ES_SourceLocation &GetSourceLocation() { return location; }

    JString *AsDebugName(ES_Context *context);

protected:
    void EmitConditionalJumps(ES_Compiler &compiler, const ES_Compiler::Register *condition_on, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index = UINT_MAX);

    Type type;
    ES_ValueType value_type;
    BOOL in_void_context;
    BOOL in_boolean_context;
    BOOL is_disabled;

    ES_SourceLocation location;
};


class ES_ThisExpr
    : public ES_Expression
{
public:
    ES_ThisExpr()
        : ES_Expression(ES_Expression::TYPE_THIS, ESTYPE_OBJECT)
    {
    }

    AS_REGISTER;
};


class ES_IdentifierExpr
    : public ES_Expression
{
public:
    ES_IdentifierExpr(JString *identifier)
        : ES_Expression(ES_Expression::TYPE_IDENTIFIER, ESTYPE_UNDEFINED),
          identifier(identifier)
    {
    }

    INTO_REGISTER;
    AS_REGISTER;

    JString *GetName() { return identifier; }

    ES_Compiler::Register AsVariable(ES_Compiler &compiler, BOOL for_assignment = FALSE);
    ES_Compiler::Register AsRegisterQuiet(ES_Compiler &compiler, const ES_Compiler::Register *temporary = NULL);

    BOOL GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value);

private:
    JString *identifier;

    void IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst, BOOL quiet);
};


class ES_LiteralExpr
    : public ES_Expression
{
public:
    ES_LiteralExpr(const ES_Value_Internal &value)
        : ES_Expression(ES_Expression::TYPE_LITERAL, value.Type()),
          value(value),
          preloaded_register(NULL)
    {
    }

    COMPILE_AS_CONDITION;
    INTO_REGISTER;
    AS_REGISTER;

    BOOL IntoRegisterNegated(ES_Compiler &compiler, const ES_Compiler::Register &dst);

    ES_Value_Internal &GetValue() { return value; }
    void SetPreloadedRegister(ES_Compiler::Register *rpreloaded) { preloaded_register = rpreloaded; }

private:
    ES_Value_Internal value;
    ES_Compiler::Register *preloaded_register;
};


class ES_RegExpLiteralExpr
    : public ES_Expression
{
public:
    ES_RegExpLiteralExpr(ES_RegExp_Information *info, JString *source)
        : ES_Expression(ES_Expression::TYPE_REGEXP_LITERAL, ESTYPE_OBJECT),
          info(info),
          source(source),
          index(UINT_MAX)
    {
    }

    INTO_REGISTER;

    void IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst, BOOL can_share);

private:
    ES_RegExp_Information *info;
    JString *source;
    unsigned index;
};


class ES_ArrayLiteralExpr
  : public ES_Expression
{
public:
    ES_ArrayLiteralExpr(unsigned exprs_count, ES_Expression **exprs)
        : ES_Expression(ES_Expression::TYPE_ARRAY_LITERAL, ESTYPE_OBJECT),
          expressions_count(exprs_count),
          expressions(exprs)
    {
    }

    INTO_REGISTER;
    CALL_VISITOR;

    BOOL IsDense();

    ES_Expression **GetExpressions() { return expressions; }
    unsigned GetCount() { return expressions_count; }

private:
    unsigned expressions_count;
    ES_Expression **expressions;
};


class ES_ObjectLiteralExpr
    : public ES_Expression
{
public:
    class Property
    {
    public:
        JString *name;
        ES_Expression *expression;
        ES_Expression *getter;
        ES_Expression *setter;
    };

    ES_ObjectLiteralExpr(unsigned props_count, Property *properties)
        : ES_Expression(ES_Expression::TYPE_OBJECT_LITERAL, ESTYPE_OBJECT),
          properties_count(props_count),
          properties(properties)
    {
    }

    INTO_REGISTER;
    CALL_VISITOR;

private:
    unsigned properties_count;
    Property *properties;
};

class ES_FunctionExpr
  : public ES_Expression
{
public:
    ES_FunctionExpr(ES_FunctionCode *function_code)
        : ES_Expression(ES_Expression::TYPE_FUNCTION, ESTYPE_OBJECT),
          function_code(function_code)
    {
    }

    ES_FunctionExpr(unsigned function_index)
        : ES_Expression(ES_Expression::TYPE_FUNCTION, ESTYPE_OBJECT),
          function_code(NULL),
          function_index(function_index)
    {
    }

    INTO_REGISTER;

    ES_FunctionCode *GetFunctionCode() { return function_code; }
    void SetFunctionCode(ES_FunctionCode *code) { function_code = code; }

    unsigned GetFunctionIndex() { return function_index; }

private:
    ES_FunctionCode *function_code;
    unsigned function_index;
};


class ES_PropertyAccessorExpr
    : public ES_Expression
{
public:
    ES_PropertyAccessorExpr(Type type, ES_Expression *base)
        : ES_Expression(type, ESTYPE_UNDEFINED),
          base(base)
    {
    }

    void SetRegister(const ES_Compiler::Register &r) { rbase = r; }
    ES_Compiler::Register BaseAsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary = NULL);

    ES_Expression *GetBase() { return base; }

protected:
    ES_Expression *base;
    ES_Compiler::Register rbase;
};


class ES_ArrayReferenceExpr
    : public ES_PropertyAccessorExpr
{
public:
    ES_ArrayReferenceExpr(ES_Expression *base, ES_Expression *index)
        : ES_PropertyAccessorExpr(ES_Expression::TYPE_ARRAY_REFERENCE, base),
          index(index)
    {
    }

    INTO_REGISTER;
    CALL_VISITOR;

    void EvaluateIndex(ES_Compiler &compiler, ES_Compiler::Register &rindex);
    void GetTo(ES_Compiler &compiler, const ES_Compiler::Register &dst, ES_Compiler::Register &rindex);
    void PutFrom(ES_Compiler &compiler, const ES_Compiler::Register &src, const ES_Compiler::Register &rindex, const ES_Compiler::Register *rbase = NULL);
    void Delete(ES_Compiler &compiler);

    ES_Expression *GetIndex() { return index; }

private:
    ES_Expression *index;
};


class ES_PropertyReferenceExpr
    : public ES_PropertyAccessorExpr
{
public:
    ES_PropertyReferenceExpr(ES_Expression *base, JString *name)
        : ES_PropertyAccessorExpr(ES_Expression::TYPE_PROPERTY_REFERENCE, base),
          name(name)
    {
    }

    INTO_REGISTER;
    CALL_VISITOR;

    void GetTo(ES_Compiler &compiler, const ES_Compiler::Register &dst);
    void PutFrom(ES_Compiler &compiler, const ES_Compiler::Register &src, const ES_Compiler::Register *rbase = NULL);
    void Delete(ES_Compiler &compiler);

    JString *GetName() { return name; }

private:
    JString *name;
};


class ES_CallExpr
    : public ES_Expression
{
public:
    ES_CallExpr(ES_Expression *func, unsigned args_count, ES_Expression **args)
        : ES_Expression(ES_Expression::TYPE_CALL, ESTYPE_UNDEFINED),
          function(func),
          arguments_count(args_count),
          arguments(args)
    {
    }

    AS_REGISTER;
    CALL_VISITOR;

    ES_Expression *GetFunction() { return function; }

    unsigned GetArgumentsCount() { return arguments_count; }
    ES_Expression **GetArguments() { return arguments; }

    BOOL IsEvalCall(ES_Compiler &compiler) { return function->GetType() == ES_Expression::TYPE_IDENTIFIER && static_cast<ES_IdentifierExpr *>(function)->GetName() == compiler.GetEvalIdentifier(); }

protected:
    ES_CallExpr(ES_Expression::Type type, ES_ValueType value_type, ES_Expression *func, unsigned args_count, ES_Expression **args)
        : ES_Expression(type, value_type),
          function(func),
          arguments_count(args_count),
          arguments(args)
    {
    }

    void Compile(ES_Compiler &compiler);

    ES_Expression *function;
    unsigned arguments_count;
    ES_Expression **arguments;
};

class ES_NewExpr
    : public ES_CallExpr
{
public:
    ES_NewExpr(ES_Expression *ctor, unsigned args_count, ES_Expression **args)
        : ES_CallExpr(ES_Expression::TYPE_NEW, ESTYPE_OBJECT, ctor, args_count, args)
    {
    }

    AS_REGISTER;
};


class ES_IncrementOrDecrementExpr
    : public ES_Expression
{
public:
    enum Type
    {
        PRE_INCREMENT,
        PRE_DECREMENT,
        POST_INCREMENT,
        POST_DECREMENT
    };

    ES_IncrementOrDecrementExpr(Type type, ES_Expression *expr)
        : ES_Expression(ES_Expression::TYPE_INCREMENT_OR_DECREMENT, ESTYPE_DOUBLE),
          type(type),
          expression(expr)
    {
    }

    INTO_REGISTER;
    AS_REGISTER;
    CALL_VISITOR;

    void IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst, BOOL is_loop_variable_increment);

    Type GetType() { return type; }

    ES_Expression *GetExpression() { return expression; }

private:
    ES_Instruction GetInstruction();

    Type type;
    ES_Expression *expression;
};


class ES_DeleteExpr
  : public ES_Expression
{
public:
    ES_DeleteExpr(ES_Expression *expr)
        : ES_Expression(ES_Expression::TYPE_DELETE, ESTYPE_BOOLEAN),
          expression(expr)
    {
    }

    COMPILE_AS_CONDITION;
    INTO_REGISTER;
    CALL_VISITOR;

private:
    ES_Expression *expression;
};


class ES_UnaryExpr
    : public ES_Expression
{
public:
    ES_UnaryExpr(Type type, ES_Expression *expr)
        : ES_Expression(type, ESTYPE_UNDEFINED),
          expression(expr)
    {
        ES_Expression::type = type;

        switch(type)
        {
        case TYPE_VOID:
            break;

        case TYPE_TYPEOF:
            value_type = ESTYPE_STRING;
            break;

        case TYPE_PLUS:
        case TYPE_MINUS:
        case TYPE_BITWISE_NOT:
            value_type = ESTYPE_DOUBLE;
            break;

        case TYPE_LOGICAL_NOT:
            value_type = ESTYPE_BOOLEAN;

        default: ;
        }
    }

    COMPILE_AS_CONDITION;
    INTO_REGISTER;
    CALL_VISITOR;

    BOOL GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value);

    ES_Expression *GetExpression() { return expression; }
    ES_Instruction GetInstruction();

private:
    ES_Expression *expression;
};


class ES_BinaryExpr
    : public ES_Expression
{
public:
    ES_BinaryExpr(Type type, ES_Expression *left, ES_Expression *right)
        : ES_Expression(type, ESTYPE_DOUBLE),
          left(left),
          right(right),
          is_compound_assign(false)
    {
        SetSourceLocation(ES_SourceLocation(left->GetSourceLocation(), right->GetSourceLocation()));
    }

    INTO_REGISTER;
    AS_REGISTER;
    CALL_VISITOR;

    BOOL GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value);

    void SetIsCompoundAssign() { is_compound_assign = true; }

    BOOL ReverseOperands(ES_Compiler &compiler);
    BOOL ImplicitDestination();
    BOOL HasImmediateRHS();

    ES_Expression *GetLeft() { return left; }
    ES_Expression *GetRight() { return right; }

    ES_Instruction GetInstruction();

protected:
    ES_Compiler::Register CompileCompoundAssignment(ES_Compiler &compiler, const ES_Compiler::Register &dst, BOOL into_register);
    ES_Compiler::Register CompileExpressions(ES_Compiler &compiler, const ES_Compiler::Register &dst);

    ES_Expression *left;
    ES_Expression *right;
    bool is_compound_assign;
};


class ES_BinaryNumberExpr
  : public ES_BinaryExpr
{
public:
    ES_BinaryNumberExpr(Type type, ES_Expression *left, ES_Expression *right)
        : ES_BinaryExpr(type, left, right)
    {
    }
};


class ES_AddExpr
  : public ES_BinaryExpr
{
public:
    enum AddType
    {
        UNKNOWN,
        STRINGS
    };

    ES_AddExpr(AddType add_type, ES_Expression *left, ES_Expression *right)
        : ES_BinaryExpr(TYPE_ADD, left, right),
          add_type(add_type)
    {
        if (add_type == STRINGS || left->GetValueType() == ESTYPE_STRING || right->GetValueType() == ESTYPE_STRING)
            value_type = ESTYPE_STRING;
        else if (left->GetValueType() == ESTYPE_DOUBLE && right->GetValueType() == ESTYPE_DOUBLE)
            value_type = ESTYPE_DOUBLE;
        else
            value_type = ESTYPE_UNDEFINED;
    }

private:
    AddType add_type;
};


class ES_ShiftExpr
  : public ES_BinaryExpr
{
public:
    ES_ShiftExpr(Type type, ES_Expression *left, ES_Expression *right)
        : ES_BinaryExpr(type, left, right)
    {
    }
};


class ES_BooleanBinaryExpr
    : public ES_BinaryExpr
{
public:
    ES_BooleanBinaryExpr(Type type, ES_Expression *left, ES_Expression *right)
        : ES_BinaryExpr(type, left, right)
    {
        ES_Expression::value_type = ESTYPE_BOOLEAN;
    }

    COMPILE_AS_CONDITION;

    BOOL IsKnownCondition(ES_Compiler &compiler, BOOL &result);
};


class ES_RelationalExpr
    : public ES_BooleanBinaryExpr
{
public:
    ES_RelationalExpr(Type type, ES_Expression *left, ES_Expression *right)
        : ES_BooleanBinaryExpr(type, left, right)
    {
        ES_Expression::value_type = ESTYPE_BOOLEAN;
    }
};


class ES_InstanceofOrInExpr
    : public ES_BooleanBinaryExpr
{
public:
    ES_InstanceofOrInExpr(Type type, ES_Expression *left, ES_Expression *right)
        : ES_BooleanBinaryExpr(type, left, right)
    {
        value_type = ESTYPE_BOOLEAN;
    }
};


class ES_EqualityExpr
    : public ES_BooleanBinaryExpr
{
public:
    ES_EqualityExpr(Type type, ES_Expression *left, ES_Expression *right)
        : ES_BooleanBinaryExpr(type, left, right)
    {
        value_type = ESTYPE_BOOLEAN;
    }
};


class ES_BitwiseExpr
    : public ES_BinaryExpr
{
public:
    ES_BitwiseExpr(Type type, ES_Expression *left, ES_Expression *right)
        : ES_BinaryExpr(type, left, right)
    {
    }
};


class ES_LogicalExpr
  : public ES_Expression
{
public:
    ES_LogicalExpr(Type type, ES_Expression *left, ES_Expression *right)
        : ES_Expression(type, ESTYPE_UNDEFINED),
          left(left),
          right(right)
    {
        if (left->GetValueType() == right->GetValueType())
            value_type = left->GetValueType();

        SetSourceLocation(ES_SourceLocation(left->GetSourceLocation(), right->GetSourceLocation()));
    }

    COMPILE_AS_CONDITION;
    INTO_REGISTER;
    CALL_VISITOR;

    ES_Expression *GetLeft() { return left; }
    ES_Expression *GetRight() { return right; }

private:
    ES_Expression *left;
    ES_Expression *right;
};


class ES_ConditionalExpr
  : public ES_Expression
{
public:
    ES_ConditionalExpr(ES_Expression *condition, ES_Expression *first, ES_Expression *second)
        : ES_Expression(ES_Expression::TYPE_CONDITIONAL, ESTYPE_UNDEFINED),
          condition(condition),
          first(first),
          second(second)
    {
        if (first->GetValueType() == second->GetValueType())
            value_type = first->GetValueType();

        SetSourceLocation(ES_SourceLocation(condition->GetSourceLocation(), second->GetSourceLocation()));
    }

    INTO_REGISTER;
    CALL_VISITOR;

private:
    ES_Expression *condition;
    ES_Expression *first;
    ES_Expression *second;
};


class ES_AssignExpr
  : public ES_Expression
{
public:
    ES_AssignExpr(ES_Expression *target, ES_Expression *source)
        : ES_Expression(ES_Expression::TYPE_ASSIGN, ESTYPE_UNDEFINED),
          target(target),
          source(source)
    {
        if (target)
            SetSourceLocation(ES_SourceLocation(target->GetSourceLocation(), source->GetSourceLocation()));
        else
            SetSourceLocation(source->GetSourceLocation());
    }

    INTO_REGISTER;
    AS_REGISTER;
    CALL_VISITOR;

    ES_Expression *GetTarget() { return target; }
    ES_Expression *GetSource() { return source; }

private:
    ES_Compiler::Register Generate(ES_Compiler &compiler, const ES_Compiler::Register *dst, BOOL into_register);

    ES_Expression *target;
    ES_Expression *source;
};


class ES_CommaExpr
    : public ES_Expression
{
public:
    ES_CommaExpr(ES_Expression **expressions, unsigned expressions_count)
        : ES_Expression(ES_Expression::TYPE_COMMA, expressions[expressions_count - 1]->GetValueType()),
          expressions(expressions),
          expressions_count(expressions_count)
    {
        SetSourceLocation(ES_SourceLocation(expressions[0]->GetSourceLocation(), expressions[expressions_count - 1]->GetSourceLocation()));
    }

    COMPILE_AS_CONDITION;
    INTO_REGISTER;
    AS_REGISTER;
    CALL_VISITOR;

    BOOL GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value);

    ES_Expression *GetLastExpression() { return expressions[expressions_count - 1]; }

private:
    ES_Expression **expressions;
    unsigned expressions_count;
};


#endif /* ES_EXPRESSIONS_H */
