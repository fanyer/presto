/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#include "core/pch.h"


#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_stmt.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_expr.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_unroll.h"


#define PUSH_LOCATION() \
    const ES_SourceLocation *previous_location = compiler.PushSourceLocation(&location)
#define POP_LOCATION() \
    compiler.PopSourceLocation(previous_location)

#ifdef ECMASCRIPT_DEBUGGER
# define EMIT_DEBUGGER_STOP() \
    if (compiler.GetParser()->GetIsDebugging()) \
        compiler.EmitInstruction(ESI_DEBUGGER_STOP, ES_DebugListener::ESEV_STATEMENT);
#else // ECMASCRIPT_DEBUGGER
# define EMIT_DEBUGGER_STOP()
#endif // ECMASCRIPT_DEBUGGER


/* virtual */ BOOL
ES_ResetLocalValues::Visit(ES_Expression *expression)
{
    ES_Expression *target = NULL;

    switch (expression->GetType())
    {
    case ES_Expression::TYPE_INCREMENT_OR_DECREMENT:
        target = static_cast<ES_IncrementOrDecrementExpr *>(expression)->GetExpression();
        break;

    case ES_Expression::TYPE_ASSIGN:
        target = static_cast<ES_AssignExpr *>(expression)->GetTarget();
        if (!target)
            /* Compound assignment. */
            target = static_cast<ES_BinaryExpr *>(static_cast<ES_AssignExpr *>(expression)->GetSource())->GetLeft();
        break;
    }

    if (target)
        ResetIfIdentifier(target);
    else if (flush_extra)
        FlushIfIdentifier(expression);

    return TRUE;
}


/* virtual */ BOOL
ES_ResetLocalValues::Enter(ES_Statement *statement, BOOL &skip)
{
    switch (statement->GetType())
    {
    case ES_Statement::TYPE_FORIN:
        if (ES_Expression *target = static_cast<ES_ForInStmt *>(statement)->GetTargetExpression())
            ResetIfIdentifier(target);
    }

    return TRUE;
}


/* virtual */ BOOL
ES_ResetLocalValues::Leave(ES_Statement *statement)
{
    return TRUE;
}


BOOL
ES_ResetLocalValues::IsLocal(ES_Expression *expression, ES_Compiler::Register &reg)
{
    if (expression->GetType() == ES_Expression::TYPE_IDENTIFIER)
    {
        reg = static_cast<ES_IdentifierExpr *>(expression)->AsVariable(compiler);
        if (reg.IsValid())
            return TRUE;
    }
    return FALSE;
}

void
ES_ResetLocalValues::ResetIfIdentifier(ES_Expression *expression)
{
    ES_Compiler::Register reg;
    if (IsLocal(expression, reg))
        compiler.ResetLocalValue(reg, TRUE);
}


void
ES_ResetLocalValues::FlushIfIdentifier(ES_Expression *expression)
{
    ES_Compiler::Register reg;
    if (IsLocal(expression, reg))
    {
        ES_Value_Internal value;
        BOOL is_stored;

        /* Doubles and strings are not that useful as immediates, so it's
           probably better to flush them before a loop if they used in the
           loop to avoid flushing them once per iteration of the loop.

           Separately, if a register is about to be updated, but its
           tracked value hasn't been stored, flush it now. Do this so
           that later uses of the register will be able to observe the
           original value. Needed to handle the compilation of branches
           in an ES_ConditionalExpr. */
        if (compiler.GetLocalValue(value, reg, is_stored))
            if (!is_stored && flush_reg_uses && *flush_reg_uses == reg || !flush_reg_uses && (value.IsDouble() || value.IsString()))
                compiler.FlushLocalValue(reg);
    }
}

void
ES_ResetLocalValues::SetFlushIfRegisterUsed(const ES_Compiler::Register *reg)
{
    flush_reg_uses = reg;
    flush_extra = reg != NULL;
}

ES_Statement::ES_Statement()
{
}


ES_Statement::ES_Statement(Type type)
    : type (type)
{
}


void
ES_Statement::Prepare(ES_Compiler &compiler)
{
}


BOOL
ES_Statement::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    return FALSE;
}


BOOL
ES_Statement::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;
    return visitor->Enter(this, skip) && visitor->Leave(this);
}


BOOL
ES_Statement::IsEmpty()
{
    return type == TYPE_EMPTY;
}


BOOL
ES_Statement::IsBlock()
{
    return type == TYPE_BLOCK || type == TYPE_EMPTY && static_cast<ES_EmptyStmt *>(this)->IsBlock();
}


/* static */ BOOL
ES_Statement::VisitStatements(ES_Statement **statements, unsigned statements_count, ES_StatementVisitor *visitor)
{
    for (unsigned statement_index = 0; statement_index < statements_count; ++statement_index)
        if (!statements[statement_index]->CallVisitor(visitor))
            return FALSE;
    return TRUE;
}


void
ES_BlockStmt::Prepare(ES_Compiler &compiler)
{
    for (unsigned index = 0; index < statements_count; ++index)
        statements[index]->Prepare(compiler);
}


BOOL
ES_BlockStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    for (unsigned index = 0; index < statements_count; ++index)
    {
        if (!statements[index]->Compile(compiler, dst))
            return FALSE;
        if (statements[index]->NoExit())
            break;
    }
    return TRUE;
}


BOOL
ES_BlockStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
        for (unsigned index = 0; index < statements_count; ++index)
            if (!statements[index]->CallVisitor(visitor))
                return FALSE;

    return visitor->Leave(this);
}


/* virtual */ BOOL
ES_BlockStmt::NoExit()
{
    return statements_count > 0 && statements[statements_count - 1]->NoExit();
}


void
ES_VariableDeclStmt::Prepare(ES_Compiler &compiler)
{
    for (unsigned index = 0; index < declarations_count; ++index)
        compiler.AddVariable(names[index], FALSE);
}


BOOL
ES_VariableDeclStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    PUSH_LOCATION();
    EMIT_DEBUGGER_STOP();

    for (unsigned index = 0; index < declarations_count; ++index)
        if (initializers[index])
            initializers[index]->CompileInVoidContext(compiler);

    POP_LOCATION();
    return !compiler.HasError();
}


BOOL
ES_VariableDeclStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
        for (unsigned index = 0; index < declarations_count; ++index)
            if (initializers[index])
                if (!initializers[index]->CallVisitor(visitor))
                    return FALSE;

    return visitor->Leave(this);
}


BOOL
ES_EmptyStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    return TRUE;
}


ES_ExpressionStmt::ES_ExpressionStmt(ES_Expression *expression)
  : ES_Statement(TYPE_EXPRESSION),
    expression(expression)
{
    SetSourceLocation(expression->GetSourceLocation());
}


BOOL
ES_ExpressionStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    PUSH_LOCATION();
    EMIT_DEBUGGER_STOP();
    if (dst.IsValid())
        expression->IntoRegister(compiler, dst);
    else
        expression->CompileInVoidContext(compiler);
    POP_LOCATION();

    return !compiler.HasError();
}


BOOL
ES_ExpressionStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
        if (!expression->CallVisitor(visitor))
            return FALSE;

    return visitor->Leave(this);
}


void
ES_IfStmt::Prepare(ES_Compiler &compiler)
{
    ifstmt->Prepare(compiler);

    if (elsestmt)
        elsestmt->Prepare(compiler);
}


BOOL
ES_IfStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (ifstmt->IsEmpty() && !elsestmt)
    {
        PUSH_LOCATION();
        EMIT_DEBUGGER_STOP();
        condition->CompileInVoidContext(compiler);
        POP_LOCATION();
    }
    else
    {
        ES_Compiler::JumpTarget true_target(compiler.ForwardJump()), false_target(compiler.ForwardJump());

        BOOL known_result;
        if (condition->IsKnownCondition(compiler, known_result))
        {
            if (known_result)
                ifstmt->Compile(compiler, dst);
            else if (elsestmt)
                elsestmt->Compile(compiler, dst);
        }
        else
        {
            ES_ResetLocalValues reset_local_values(compiler);

            ifstmt->CallVisitor(&reset_local_values);
            if (elsestmt)
                elsestmt->CallVisitor(&reset_local_values);

            PUSH_LOCATION();
            EMIT_DEBUGGER_STOP();
            condition->CompileAsCondition(compiler, true_target, false_target, ifstmt->IsEmpty());
            POP_LOCATION();

            unsigned snapshot_after_condition = compiler.GetLocalValuesSnapshot();

            if (ifstmt->IsEmpty())
            {
                compiler.SetJumpTarget(false_target);

                if (!elsestmt->Compile(compiler, dst))
                    return FALSE;

                compiler.ResetLocalValues(snapshot_after_condition);
                compiler.SetJumpTarget(true_target);
            }
            else
            {
                compiler.SetJumpTarget(true_target);

                if (!ifstmt->Compile(compiler, dst))
                    return FALSE;

                if (elsestmt)
                {
                    ES_Compiler::JumpTarget end_target(compiler.ForwardJump());

                    compiler.ResetLocalValues(snapshot_after_condition);
                    if (!ifstmt->NoExit())
                        compiler.EmitJump(NULL, ESI_JUMP, end_target);

                    compiler.SetJumpTarget(false_target);

                    if (!elsestmt->Compile(compiler, dst))
                        return FALSE;

                    compiler.ResetLocalValues(snapshot_after_condition);
                    compiler.SetJumpTarget(end_target);
                }
                else
                {
                    compiler.ResetLocalValues(snapshot_after_condition);
                    compiler.SetJumpTarget(false_target);
                }
            }
        }
    }

    return !compiler.HasError();
}


BOOL
ES_IfStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
    {
        if (!condition->CallVisitor(visitor))
            return FALSE;
        if (!ifstmt->CallVisitor(visitor))
            return FALSE;
        if (elsestmt && !elsestmt->CallVisitor(visitor))
            return FALSE;
    }

    return visitor->Leave(this);
}


/* virtual */ BOOL
ES_IfStmt::NoExit()
{
    return ifstmt->NoExit() && elsestmt && elsestmt->NoExit();
}


class ES_PreloadConstant
    : public ES_StatementVisitor
{
public:
    ES_PreloadConstant(ES_Compiler &compiler)
        : compiler(compiler),
          uses_true(FALSE),
          uses_false(FALSE),
          uses_integer(FALSE)
    {
    }

    virtual BOOL Visit(ES_Expression *expr);
    virtual BOOL Enter(ES_Statement *stmt, BOOL &skip);
    virtual BOOL Leave(ES_Statement *stmt);

    ES_Compiler &compiler;

    BOOL uses_true;
    BOOL uses_false;
    BOOL uses_integer;

    int integer_value;

    ES_Compiler::Register true_register;
    ES_Compiler::Register false_register;
    ES_Compiler::Register integer_register;
};


/* virtual */ BOOL
ES_PreloadConstant::Visit(ES_Expression *expr)
{
    if (expr->GetType() == ES_Expression::TYPE_ASSIGN)
    {
        ES_AssignExpr *assignment = static_cast<ES_AssignExpr *>(expr);
        ES_Expression *target = assignment->GetTarget(), *source = assignment->GetSource();

        if (target && (target->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE || target->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE))
            if (source->GetType() == ES_Expression::TYPE_LITERAL)
            {
                ES_LiteralExpr *literal = static_cast<ES_LiteralExpr *>(source);

                if (literal->GetValue().IsBoolean())
                {
                    if (literal->GetValue().GetBoolean())
                    {
                        if (!uses_true)
                        {
                            uses_true = TRUE;
                            true_register = compiler.Temporary();
                            compiler.EmitInstruction(ESI_LOAD_TRUE, true_register);
                        }
                        literal->SetPreloadedRegister(&true_register);
                    }
                    else
                    {
                        if (!uses_false)
                        {
                            uses_false = TRUE;
                            false_register = compiler.Temporary();
                            compiler.EmitInstruction(ESI_LOAD_FALSE, false_register);
                        }
                        literal->SetPreloadedRegister(&false_register);
                    }
                }
                else if (literal->GetValue().IsInt32() && (!uses_integer || literal->GetValue().GetInt32() == integer_value))
                {
                    if (!uses_integer)
                    {
                        uses_integer = TRUE;
                        integer_value = literal->GetValue().GetInt32();
                        integer_register = compiler.Temporary();
                        compiler.EmitInstruction(ESI_LOAD_INT32, integer_register, integer_value);
                    }
                    literal->SetPreloadedRegister(&integer_register);
                }
            }
    }

    return !uses_true || !uses_false || !uses_integer;
}


/* virtual */ BOOL
ES_PreloadConstant::Enter(ES_Statement *stmt, BOOL &skip)
{
    return TRUE;
}


/* virtual */ BOOL
ES_PreloadConstant::Leave(ES_Statement *stmt)
{
    return TRUE;
}


void
ES_WhileStmt::Prepare(ES_Compiler &compiler)
{
    body->Prepare(compiler);
}


BOOL
ES_WhileStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_LoopUnroller unroller(compiler);

    unroller.SetLoopType(type);
    unroller.SetLoopCondition(condition);
    unroller.SetLoopBody(body);

    if (unroller.Unroll(dst))
        return TRUE;

    ES_Compiler::JumpTarget continue_target(compiler.ForwardJump());
    ES_Compiler::JumpTarget break_target(compiler.ForwardJump());
    BOOL check_condition_twice = FALSE;

#ifdef ES_NATIVE_SUPPORT
    compiler.EmitInstruction(ESI_START_LOOP, ES_CodeWord(0));
    unsigned end_index = compiler.CurrentInstructionIndex() - 1;
#endif // ES_NATIVE_SUPPORT

    ES_PreloadConstant preloaded_constants(compiler);

    condition->CallVisitor(&preloaded_constants);
    body->CallVisitor(&preloaded_constants);

    ES_ResetLocalValues reset_local_values(compiler, TRUE);

    condition->CallVisitor(&reset_local_values);
    body->CallVisitor(&reset_local_values);

    if (type == TYPE_WHILE)
    {
        switch (condition->GetType())
        {
        case ES_Expression::TYPE_LESS_THAN:
        case ES_Expression::TYPE_GREATER_THAN:
        case ES_Expression::TYPE_LESS_THAN_OR_EQUAL:
        case ES_Expression::TYPE_GREATER_THAN_OR_EQUAL:
            check_condition_twice = TRUE;
        }

        ES_Compiler::JumpTarget start_target(compiler.ForwardJump());

        if (check_condition_twice)
        {
            ES_Compiler::JumpTarget start_target(compiler.ForwardJump());

            PUSH_LOCATION();
            EMIT_DEBUGGER_STOP();
            condition->CompileAsCondition(compiler, start_target, break_target, FALSE);
            POP_LOCATION();

            compiler.SetJumpTarget(start_target);
        }
        else
            compiler.EmitJump(NULL, ESI_JUMP, continue_target);
    }

    ES_Compiler::JumpTarget body_target(compiler.BackwardJump());

    unsigned labels = 0;
    JString *l = NULL;
    do
    {
        compiler.PushContinuationTarget(break_target, ES_Compiler::TARGET_BREAK, l);
        compiler.PushContinuationTarget(continue_target, ES_Compiler::TARGET_CONTINUE, l);
        labels += 2;
    }
    while ((l = compiler.PopLabel()));

    unsigned snapshot = compiler.GetLocalValuesSnapshot();

    if (!body->Compile(compiler, dst))
        return FALSE;

    compiler.PopContinuationTargets(labels);

    compiler.ResetLocalValues(snapshot);
    compiler.SetJumpTarget(continue_target);

    PUSH_LOCATION();
    EMIT_DEBUGGER_STOP();
    condition->CompileAsCondition(compiler, body_target, break_target, TRUE);
    POP_LOCATION();

    compiler.SetJumpTarget(break_target);
#ifdef ES_NATIVE_SUPPORT
    compiler.SetCodeWord(end_index, compiler.CurrentInstructionIndex());
#endif // ES_NATIVE_SUPPORT

    return !compiler.HasError();
}


BOOL
ES_WhileStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
    {
        if (!condition->CallVisitor(visitor))
            return FALSE;
        if (!body->CallVisitor(visitor))
            return FALSE;
    }

    return visitor->Leave(this);
}


void
ES_ForStmt::Prepare(ES_Compiler &compiler)
{
    if (type == INITTYPE_VARIABLEDECL)
        init.vardecl->Prepare(compiler);
    else if (init.expr)
        init.expr->Prepare(compiler);

    body->Prepare(compiler);

    if (iteration)
        iteration->GetExpression()->SetInVoidContext();
}


BOOL
ES_ForStmt::IsSimpleLoopVariable(ES_Compiler &compiler, JString *&name, int &lower_bound, int &upper_bound)
{
    ES_Expression *initializer = NULL;

    if (type == INITTYPE_VARIABLEDECL)
    {
        if (init.vardecl->GetCount() != 1)
            return FALSE;

        name = init.vardecl->GetName();
        initializer = init.vardecl->GetInitializer();
    }
    else if (init.expr && init.expr->GetExpression()->GetType() == ES_Expression::TYPE_ASSIGN)
    {
        ES_AssignExpr *expr = static_cast<ES_AssignExpr *>(init.expr->GetExpression());
        if (ES_Expression *target = expr->GetTarget())
        {
            if (target->GetType() != ES_Expression::TYPE_IDENTIFIER)
                return FALSE;

            name = static_cast<ES_IdentifierExpr *>(target)->GetName();
            if (!compiler.Local(name).IsValid())
                return FALSE;

            initializer = expr->GetSource();
        }
        else
            return FALSE;
    }
    else
        return FALSE;

    if (!initializer)
        return FALSE;

    ES_LiteralExpr *literal;

    if (initializer->GetType() == ES_Expression::TYPE_ASSIGN)
    {
        if (static_cast<ES_AssignExpr *>(initializer)->GetSource()->GetType() != ES_Expression::TYPE_LITERAL)
            return FALSE;

        literal = static_cast<ES_LiteralExpr *>(static_cast<ES_AssignExpr *>(initializer)->GetSource());
    }
    else if (initializer->GetType() == ES_Expression::TYPE_LITERAL)
        literal = static_cast<ES_LiteralExpr *>(initializer);
    else
        return FALSE;

    if (literal->GetValue().Type() != ESTYPE_INT32)
        return FALSE;

    lower_bound = literal->GetValue().GetInt32();

    if (!iteration || iteration->GetExpression()->GetType() != ES_Expression::TYPE_INCREMENT_OR_DECREMENT)
        return FALSE;

    ES_IncrementOrDecrementExpr *incordec = static_cast<ES_IncrementOrDecrementExpr *>(iteration->GetExpression());
    if (incordec->GetType() == ES_IncrementOrDecrementExpr::PRE_DECREMENT || incordec->GetType() == ES_IncrementOrDecrementExpr::POST_DECREMENT)
        return FALSE;

    if (!condition)
        return FALSE;

    switch (condition->GetType())
    {
    default:
        return FALSE;

    case ES_Expression::TYPE_LESS_THAN:
    case ES_Expression::TYPE_LESS_THAN_OR_EQUAL:
    case ES_Expression::TYPE_GREATER_THAN:
    case ES_Expression::TYPE_GREATER_THAN_OR_EQUAL:
    case ES_Expression::TYPE_NOT_EQUAL:
        ES_RelationalExpr *relexpr = static_cast<ES_RelationalExpr *>(condition);
        ES_Expression *left = relexpr->GetLeft();
        ES_Expression *right = relexpr->GetRight();
        ES_LiteralExpr *constant;
        ES_Expression *variable;

        if (left->GetType() == ES_Expression::TYPE_LITERAL)
        {
            constant = static_cast<ES_LiteralExpr *>(left);
            variable = right;
        }
        else if (right->GetType() == ES_Expression::TYPE_LITERAL)
        {
            constant = static_cast<ES_LiteralExpr *>(right);
            variable = left;
        }
        else
            return FALSE;

        if (!constant->GetValue().IsInt32())
            return FALSE;

        upper_bound = constant->GetValue().GetInt32();

        switch (condition->GetType())
        {
        case ES_Expression::TYPE_LESS_THAN:
            --upper_bound;

        case ES_Expression::TYPE_LESS_THAN_OR_EQUAL:
            if (constant == left)
                return FALSE;
            break;

        case ES_Expression::TYPE_GREATER_THAN:
            --upper_bound;

        case ES_Expression::TYPE_GREATER_THAN_OR_EQUAL:
            if (constant == right)
                return FALSE;

        default:
            --upper_bound;
        }

        if (upper_bound < lower_bound)
            return FALSE;

        if (variable->GetType() != ES_Expression::TYPE_IDENTIFIER)
            return FALSE;

        ES_IdentifierExpr *identifier = static_cast<ES_IdentifierExpr *>(variable);

        return identifier->GetName() == name;
    }
}


BOOL
ES_ForStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (type == INITTYPE_VARIABLEDECL)
    {
        if (!init.vardecl->Compile(compiler, compiler.Invalid()))
            return FALSE;
    }
    else if (init.expr)
        init.expr->Compile(compiler, compiler.Invalid());

    ES_LoopUnroller unroller(compiler);

    unroller.SetLoopType(TYPE_FOR);
    unroller.SetLoopCondition(condition);
    unroller.SetLoopBody(body);
    unroller.SetLoopIteration(iteration);

    if (unroller.Unroll(dst))
        return TRUE;

    ES_Compiler::JumpTarget continue_target(compiler.ForwardJump()), break_target(compiler.ForwardJump());

    unsigned labels = 0;
    JString *l = NULL;
    do
    {
        compiler.PushContinuationTarget(break_target, ES_Compiler::TARGET_BREAK, l);
        compiler.PushContinuationTarget(continue_target, ES_Compiler::TARGET_CONTINUE, l);
        labels += 2;
    }
    while ((l = compiler.PopLabel()));

#ifdef ES_NATIVE_SUPPORT
    compiler.EmitInstruction(ESI_START_LOOP, ES_CodeWord(0));
    unsigned end_index = compiler.CurrentInstructionIndex() - 1;
#endif // ES_NATIVE_SUPPORT

    ES_PreloadConstant preloaded_constants(compiler);

    if (condition)
        condition->CallVisitor(&preloaded_constants);
    if (iteration)
        iteration->CallVisitor(&preloaded_constants);
    body->CallVisitor(&preloaded_constants);

    JString *variable_name;
    int lower_bound, upper_bound;

    BOOL is_simple_loop_variable = IsSimpleLoopVariable(compiler, variable_name, lower_bound, upper_bound);

    if (is_simple_loop_variable)
        compiler.StartSimpleLoopVariable(variable_name, lower_bound, upper_bound);

    ES_ResetLocalValues reset_local_values(compiler, TRUE);

    if (condition)
        condition->CallVisitor(&reset_local_values);
    if (iteration)
        iteration->CallVisitor(&reset_local_values);
    body->CallVisitor(&reset_local_values);

    if (condition)
    {
        BOOL check_condition_twice = FALSE;

        switch (condition->GetType())
        {
        case ES_Expression::TYPE_LESS_THAN:
        case ES_Expression::TYPE_GREATER_THAN:
        case ES_Expression::TYPE_LESS_THAN_OR_EQUAL:
        case ES_Expression::TYPE_GREATER_THAN_OR_EQUAL:
            check_condition_twice = TRUE;
        }

        ES_Compiler::JumpTarget start_target(compiler.ForwardJump());

        if (check_condition_twice)
        {
            if (!is_simple_loop_variable)
            {
                PUSH_LOCATION();
                EMIT_DEBUGGER_STOP();
                condition->CompileAsCondition(compiler, start_target, break_target, FALSE);
                POP_LOCATION();

                compiler.SetJumpTarget(start_target);
            }
        }
        else
            compiler.EmitJump(NULL, ESI_JUMP, start_target);

        unsigned start = compiler.CurrentInstructionIndex();
        unsigned snapshot = compiler.GetLocalValuesSnapshot();

        break_target.SetLocalValuesSnapshot(snapshot);
        continue_target.SetLocalValuesSnapshot(snapshot);

        ES_Compiler::JumpTarget body_target(compiler.BackwardJump());

        if (!body->Compile(compiler, dst))
            return FALSE;

        compiler.ResetLocalValues(snapshot);
        compiler.SetJumpTarget(continue_target);

        if (iteration)
            if (is_simple_loop_variable)
                static_cast<ES_IncrementOrDecrementExpr *>(iteration->GetExpression())->IntoRegister(compiler, compiler.Invalid(), TRUE);
            else
                iteration->Compile(compiler, compiler.Invalid());

        if (!check_condition_twice)
            compiler.SetJumpTarget(start_target);

        PUSH_LOCATION();
        EMIT_DEBUGGER_STOP();
        condition->CompileAsCondition(compiler, body_target, break_target, TRUE, compiler.AddLoop(start));
        POP_LOCATION();
    }
    else
    {
        ES_Compiler::JumpTarget body_target(compiler.BackwardJump());
        unsigned snapshot = compiler.GetLocalValuesSnapshot();

        break_target.SetLocalValuesSnapshot(snapshot);
        continue_target.SetLocalValuesSnapshot(snapshot);

        if (!body->Compile(compiler, dst))
            return FALSE;

        compiler.SetJumpTarget(continue_target);

        if (iteration)
            iteration->Compile(compiler, compiler.Invalid());

        compiler.ResetLocalValues(snapshot);
        compiler.EmitJump(NULL, ESI_JUMP, body_target);
    }

    if (is_simple_loop_variable)
        compiler.EndSimpleLoopVariable(variable_name);

    compiler.PopContinuationTargets(labels);

    compiler.SetJumpTarget(break_target);
#ifdef ES_NATIVE_SUPPORT
    compiler.SetCodeWord(end_index, compiler.CurrentInstructionIndex());
#endif // ES_NATIVE_SUPPORT

    return !compiler.HasError();
}


BOOL
ES_ForStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
    {
        if (type == INITTYPE_VARIABLEDECL)
        {
            if (!init.vardecl->CallVisitor(visitor))
                return FALSE;
        }
        else if (init.expr)
            if (!init.expr->CallVisitor(visitor))
                return FALSE;

        if (condition && !condition->CallVisitor(visitor))
            return FALSE;
        if (iteration && !iteration->CallVisitor(visitor))
            return FALSE;
        if (!body->CallVisitor(visitor))
            return FALSE;
    }

    return visitor->Leave(this);
}


void
ES_ForInStmt::Prepare(ES_Compiler &compiler)
{
    if (type == TYPE_VARIABLEDECL)
        compiler.AddVariable(target.vardecl->GetName(), FALSE);

    body->Prepare (compiler);
}


BOOL
ES_ForInStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_Compiler::Register robject(compiler.Temporary()), renum(compiler.Temporary()), rnenum(compiler.Temporary()), rname;

    PUSH_LOCATION();

    if (type == TYPE_VARIABLEDECL)
    {
        if (compiler.GetCodeType() == ES_Compiler::CODETYPE_FUNCTION && !compiler.HasInnerScopes())
            rname = compiler.Local(target.vardecl->GetName());
        else
            rname = compiler.Temporary();

        if (target.vardecl->GetInitializer())
        {
            target.vardecl->GetInitializer()->IntoRegister(compiler, rname);

            if (rname.IsTemporary())
                if (!compiler.HasUnknownScope())
                {
                    ES_CodeWord::Index scope_index, variable_index;
                    BOOL is_lexical_read_only;

                    if (compiler.GetLexical(scope_index, variable_index, target.vardecl->GetName(), is_lexical_read_only))
                    {
                        if (!is_lexical_read_only)
                            compiler.EmitInstruction(ESI_PUT_LEXICAL, scope_index, variable_index, rname);
                    }
                    else
                        compiler.EmitGlobalAccessor(ESI_PUT_GLOBAL, target.vardecl->GetName(), rname);
                }
                else
                    compiler.EmitScopedAccessor(ESI_PUT_SCOPE, target.vardecl->GetName(), &rname, NULL);
        }

        if (compiler.IsLocal(rname))
            compiler.ResetLocalValue(rname, TRUE);
    }
    else if (target.expr->GetType() == ES_Expression::TYPE_IDENTIFIER)
    {
        ES_Compiler::Register variable(static_cast<ES_IdentifierExpr *>(target.expr)->AsVariable(compiler));

        if (variable.IsValid())
        {
            compiler.ResetLocalValue(variable, TRUE);
            rname = variable;
        }
        else
            rname = compiler.Temporary();
    }
    else if (target.expr->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE || target.expr->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
        rname = compiler.Temporary();
    else
        // FIXME: error reporting and stuff.
        return FALSE;

    source->IntoRegister(compiler, robject);

    ES_ResetLocalValues reset_local_values(compiler, TRUE);

    if (type == TYPE_EXPRESSION)
        target.expr->CallVisitor(&reset_local_values);
    body->CallVisitor(&reset_local_values);

    compiler.EmitInstruction(ESI_LOAD_NULL, rnenum);
    compiler.EmitInstruction(ESI_ENUMERATE, renum, robject, rnenum);

    ES_Compiler::JumpTarget break_target(compiler.ForwardJump());
    ES_Compiler::JumpTarget continue_target(compiler.BackwardJump());

    unsigned snapshot = compiler.GetLocalValuesSnapshot();

    break_target.SetLocalValuesSnapshot(snapshot);
    continue_target.SetLocalValuesSnapshot(snapshot);

    unsigned labels = 0;
    JString *l = NULL;
    do
    {
        compiler.PushContinuationTarget(break_target, ES_Compiler::TARGET_BREAK, l);
        compiler.PushContinuationTarget(continue_target, ES_Compiler::TARGET_CONTINUE, l);
        labels += 2;
    }
    while ((l = compiler.PopLabel()));

    EMIT_DEBUGGER_STOP();
    compiler.EmitInstruction(ESI_NEXT_PROPERTY, rname, renum, robject, rnenum);
    compiler.EmitJump(NULL, ESI_JUMP_IF_FALSE, break_target, UINT_MAX);

    if (rname.IsTemporary())
        if (type == TYPE_VARIABLEDECL || target.expr->GetType() == ES_Expression::TYPE_IDENTIFIER)
        {
            JString *name = type == TYPE_VARIABLEDECL ? target.vardecl->GetName() : static_cast<ES_IdentifierExpr *>(target.expr)->GetName();

            if (!compiler.HasUnknownScope())
            {
                ES_CodeWord::Index scope_index, variable_index;
                BOOL is_lexical_read_only;

                if (compiler.GetLexical(scope_index, variable_index, name, is_lexical_read_only))
                {
                    if (!is_lexical_read_only)
                        compiler.EmitInstruction(ESI_PUT_LEXICAL, scope_index, variable_index, rname);
                }
                else
                    compiler.EmitGlobalAccessor(ESI_PUT_GLOBAL, name, rname);
            }
            else
                compiler.EmitScopedAccessor(ESI_PUT_SCOPE, name, &rname, NULL);
        }
        else if (target.expr->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE)
            static_cast<ES_ArrayReferenceExpr *>(target.expr)->PutFrom(compiler, rname, compiler.Invalid());
        else
            static_cast<ES_PropertyReferenceExpr *>(target.expr)->PutFrom(compiler, rname);

    POP_LOCATION();

    if (!body->Compile(compiler, dst))
        return FALSE;

    compiler.ResetLocalValues(snapshot);
    compiler.EmitJump(NULL, ESI_JUMP, continue_target);
    compiler.SetJumpTarget(break_target);
    compiler.PopContinuationTargets(labels);

    return !compiler.HasError();
}


BOOL
ES_ForInStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
    {
        if (type == TYPE_EXPRESSION)
        {
            if (!target.expr->CallVisitor(visitor))
                return FALSE;
        }
        else if (target.vardecl)
            if (!target.vardecl->CallVisitor(visitor))
                return FALSE;

        if (!source->CallVisitor(visitor))
            return FALSE;
        if (!body->CallVisitor(visitor))
            return FALSE;
    }

    return visitor->Leave(this);
}


BOOL
ES_ContinueStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    BOOL crosses_finally = FALSE;
    const ES_Compiler::JumpTarget *continue_target = compiler.FindContinueTarget(crosses_finally, target);

    PUSH_LOCATION();
    EMIT_DEBUGGER_STOP();
    POP_LOCATION();

    if (continue_target)
    {
        compiler.FlushLocalValues(continue_target->GetLocalValuesSnapshot());

        /* We're going to break past a finally block, so save the target address and then jump to the
           finally handler. The finally handler will either jump to the saved address or delegate to
           yet another finally handler. */
        if (crosses_finally)
        {
            compiler.EmitInstruction(ESI_LOAD_INT32, compiler.GetRethrowTarget(), *continue_target);
            compiler.EmitJump(NULL, ESI_JUMP, *compiler.GetFinallyTarget());
        }
        else
            compiler.EmitJump(NULL, ESI_JUMP, *continue_target);
        return TRUE;
    }
    else
    {
        compiler.SetError(target ? ES_Parser::UNBOUND_CONTINUE : ES_Parser::ILLEGAL_CONTINUE, GetSourceLocation());
        return FALSE;
    }
}


BOOL
ES_BreakStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    BOOL crosses_finally = FALSE;
    const ES_Compiler::JumpTarget *break_target = compiler.FindBreakTarget(crosses_finally, target);

    PUSH_LOCATION();
    EMIT_DEBUGGER_STOP();
    POP_LOCATION();

    if (break_target)
    {
        compiler.FlushLocalValues(break_target->GetLocalValuesSnapshot());

        /* Handled the same way as continue above. */
        if (crosses_finally)
        {
            compiler.EmitInstruction(ESI_LOAD_INT32, compiler.GetRethrowTarget(), *break_target);
            compiler.EmitJump(NULL, ESI_JUMP, *compiler.GetFinallyTarget());
        }

        compiler.EmitJump(NULL, ESI_JUMP, *break_target);
        return TRUE;
    }
    else
    {
        compiler.SetError(target ? ES_Parser::UNBOUND_BREAK : ES_Parser::ILLEGAL_BREAK, GetSourceLocation());
        return FALSE;
    }
}


BOOL
ES_ReturnStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    PUSH_LOCATION();
    EMIT_DEBUGGER_STOP();
    if (compiler.GetFinallyTarget() != NULL)
    {
        compiler.FlushLocalValues();

        /* We're going to return from within a try block that has a finally block.
           To do that we take the address of the target set from ES_Compiler::CompileFunction
           and save that in the specially allotted register that we get via GetRethrowTarget.
           We also save the result of the expression in the special return register
           that we also make sure is initialised at this point. At last we jump to the
           finally handler which will either jump to the saved address or delegate to
           yet another finally handler.
        */
        if (!compiler.TryBlockReturnValueRegister().IsValid())
            compiler.TryBlockReturnValueRegister() = compiler.Temporary();

        if (expression)
            expression->IntoRegister(compiler, compiler.TryBlockReturnValueRegister());
        else
            compiler.EmitInstruction(ESI_LOAD_UNDEFINED, compiler.TryBlockReturnValueRegister());

        /* Separately signal the occurrence of the return. Caught by the outermost
           finally block, which then returns. */
        OP_ASSERT(compiler.TryBlockHaveReturnedRegister().IsValid());
        compiler.EmitInstruction(ESI_LOAD_TRUE, compiler.TryBlockHaveReturnedRegister());

        compiler.EmitInstruction(ESI_LOAD_INT32, compiler.GetRethrowTarget(), compiler.FunctionReturnTarget());
        compiler.EmitJump(NULL, ESI_JUMP, *compiler.GetFinallyTarget());
    }
    else
    {
        if (expression)
            compiler.EmitInstruction(ESI_RETURN_VALUE, expression->AsRegister(compiler));
        else
            compiler.EmitInstruction(ESI_RETURN_NO_VALUE);
    }
    POP_LOCATION();
    return !compiler.HasError();
}


BOOL
ES_ReturnStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
        if (expression && !expression->CallVisitor(visitor))
            return FALSE;

    return visitor->Leave(this);
}


void
ES_WithStmt::Prepare(ES_Compiler &compiler)
{
    /* This statement can do weird things with locals, so forget about tracking
       anything anywhere in this function. */
    compiler.SuspendValueTracking();

    body->Prepare(compiler);
}


BOOL
ES_WithStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_Compiler::Register object(compiler.Temporary());

    PUSH_LOCATION();
    EMIT_DEBUGGER_STOP();
    if (expression->GetValueType() == ESTYPE_OBJECT)
        expression->IntoRegister(compiler, object);
    else
        compiler.EmitInstruction(ESI_TOOBJECT, object, expression->AsRegister(compiler));
    POP_LOCATION();

    compiler.PushInnerScope(object);

    if (!body->Compile(compiler, dst))
        return FALSE;

    compiler.PopInnerScope();
    return !compiler.HasError();
}


BOOL
ES_WithStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
    {
        if (!expression->CallVisitor(visitor))
            return FALSE;
        if (!body->CallVisitor(visitor))
            return FALSE;
    }

    return visitor->Leave(this);
}


void
ES_SwitchStmt::Prepare(ES_Compiler &compiler)
{
    for (unsigned index = 0; index < statements_count; ++index)
        statements[index]->Prepare(compiler);
}


BOOL
ES_SwitchStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    PUSH_LOCATION();
    EMIT_DEBUGGER_STOP();
    POP_LOCATION();

    BOOL has_default_case = FALSE;
    CaseClause *clause = case_clauses;
    int minimum, maximum;

    ES_CodeStatic::SwitchTable *switch_table = NULL;
    unsigned switch_table_index = 0;

    while (clause && !clause->expression)
        clause = clause->next;

    if (clause && clause->expression->GetType() == ES_Expression::TYPE_LITERAL && static_cast<ES_LiteralExpr *>(clause->expression)->GetValue().IsInt32())
    {
        ES_Value_Internal &value = static_cast<ES_LiteralExpr *>(clause->expression)->GetValue();

        minimum = maximum = value.GetInt32();
        clause = clause->next;

        unsigned count = 1;

        while (clause)
        {
            if (clause->expression)
            {
                if (clause->expression->GetType() != ES_Expression::TYPE_LITERAL || !static_cast<ES_LiteralExpr *>(clause->expression)->GetValue().IsInt32())
                    break;

                ES_Value_Internal &value = static_cast<ES_LiteralExpr *>(clause->expression)->GetValue();

                if (value.GetInt32() < minimum)
                    minimum = value.GetInt32();
                else if (value.GetInt32() > maximum)
                    maximum = value.GetInt32();

                ++count;
            }

            clause = clause->next;
        }

        /* Note: elements == 0 w/ min = INT_MIN, max = INT_MAX. */
        unsigned elements = maximum - minimum + 1;

        if (!clause && count * 2 >= elements && count > 1 && elements > 0)
        {
            switch_table = compiler.AddSwitchTable(switch_table_index, minimum, maximum);

            op_memset(switch_table->codeword_indeces + minimum, 0xff, elements * sizeof (unsigned));
        }
    }

    ES_ResetLocalValues reset_local_values(compiler);

    clause = case_clauses;
    while (clause)
    {
        if (clause->expression)
            clause->expression->CallVisitor(&reset_local_values);
        clause = clause->next;
    }

    for (unsigned statement_index = 0; statement_index < statements_count; ++statement_index)
        statements[statement_index]->CallVisitor(&reset_local_values);

    ES_Compiler::Register value;

    if (switch_table)
        value = expression->AsRegister(compiler);
    else
    {
        value = compiler.Temporary();
        expression->IntoRegister(compiler, value);
    }

    unsigned snapshot_after_value = compiler.GetLocalValuesSnapshot();

    clause = case_clauses;

    while (clause)
    {
        if (clause->expression)
        {
            clause->jump_target = compiler.ForwardJump();

            if (switch_table)
                clause->jump_target.SetUsedInTableSwitch();
            else
            {
                int immediate;
                if (clause->expression->GetImmediateInteger(compiler, immediate))
                    compiler.EmitInstruction(ESI_EQ_STRICT_IMM, value, immediate);
                else
                    compiler.EmitInstruction(ESI_EQ_STRICT, value, clause->expression->AsRegister(compiler));

                compiler.EmitJump(NULL, ESI_JUMP_IF_TRUE, clause->jump_target, UINT_MAX);
            }
        }
        else
            has_default_case = TRUE;

        clause = clause->next;
    }

    if (switch_table)
        compiler.EmitInstruction(ESI_TABLE_SWITCH, value, switch_table_index);

    ES_Compiler::JumpTarget break_target(compiler.ForwardJump());
    compiler.PushContinuationTarget(break_target, ES_Compiler::TARGET_BREAK);

    ES_Compiler::JumpTarget default_target(has_default_case ? compiler.ForwardJump() : break_target);

    if (switch_table)
        default_target.SetUsedInTableSwitch();
    else
        compiler.EmitJump(NULL, ESI_JUMP, default_target);

    clause = case_clauses;

    unsigned statement_index = 0;

    break_target.SetLocalValuesSnapshot(snapshot_after_value);

    while (clause)
    {
        if (clause->expression)
            compiler.SetJumpTarget(clause->jump_target);
        else
            compiler.SetJumpTarget(default_target);

        for (unsigned index = 0; index++ < clause->statements_count; ++statement_index)
        {
            if (!statements[statement_index]->Compile(compiler, dst))
                return FALSE;

            if (statements[statement_index]->NoExit())
            {
                compiler.ResetLocalValues(snapshot_after_value, FALSE);

                statement_index += 1 + clause->statements_count - index;
                break;
            }
            else if (index == clause->statements_count)
                compiler.ResetLocalValues(snapshot_after_value, TRUE);
        }

        clause = clause->next;
    }

    compiler.ResetLocalValues(snapshot_after_value);
    compiler.SetJumpTarget(break_target);
    compiler.PopContinuationTargets(1);

    if (switch_table)
    {
        clause = case_clauses;

        while (clause)
        {
            if (clause->expression)
            {
                int value = static_cast<ES_LiteralExpr *>(clause->expression)->GetValue().GetInt32();

                if (switch_table->codeword_indeces[value] == UINT_MAX)
                    switch_table->codeword_indeces[value] = clause->jump_target.Address();
            }

            clause->jump_target = ES_Compiler::JumpTarget();
            clause = clause->next;
        }

        int ncases = switch_table->maximum - switch_table->minimum + 1;
        for (int i = 0; i < ncases; i++)
            if (switch_table->codeword_indeces[switch_table->minimum + i] == UINT_MAX)
                switch_table->codeword_indeces[switch_table->minimum + i] = default_target.Address();

        switch_table->default_codeword_index = default_target.Address();
    }

    return !compiler.HasError();
}


BOOL
ES_SwitchStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
    {
        if (!expression->CallVisitor(visitor))
            return FALSE;

        CaseClause *clause = case_clauses;
        while (clause)
        {
            if (clause->expression)
                if (!clause->expression->CallVisitor(visitor))
                    return FALSE;
            clause = clause->next;
        }

        for (unsigned index = 0; index < statements_count; ++index)
            if (!statements[index]->CallVisitor(visitor))
                return FALSE;
    }

    return visitor->Leave(this);
}


void
ES_LabelledStmt::Prepare(ES_Compiler &compiler)
{
    statement->Prepare(compiler);
}


BOOL
ES_LabelledStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (compiler.IsDuplicateLabel(label))
    {
        compiler.SetError(ES_Parser::DUPLICATE_LABEL, GetSourceLocation());
        return FALSE;
    }

    BOOL non_loop = FALSE;
    switch (statement->GetType())
    {
    case ES_Statement::TYPE_WHILE:
    case ES_Statement::TYPE_DO_WHILE:
    case ES_Statement::TYPE_FOR:
    case ES_Statement::TYPE_FORIN:
    case ES_Statement::TYPE_LABELLED:
        break;

    default:
        non_loop = TRUE;
    }

    ES_ResetLocalValues reset_local_values(compiler);
    statement->CallVisitor(&reset_local_values);

    ES_Compiler::JumpTarget break_target;
    unsigned snapshot = compiler.GetLocalValuesSnapshot();

    if (non_loop)
    {
        break_target = compiler.ForwardJump();
        compiler.PushContinuationTarget(break_target, ES_Compiler::TARGET_BREAK, label);
    }
    else
        compiler.PushLabel(label);

    if (!statement->Compile(compiler, dst))
        return FALSE;

    if (non_loop)
    {
        compiler.ResetLocalValues(snapshot);
        compiler.SetJumpTarget(break_target);
        compiler.PopContinuationTargets(1);
    }

    return TRUE;
}


BOOL
ES_LabelledStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
        if (!statement->CallVisitor(visitor))
            return FALSE;

    return visitor->Leave(this);
}


BOOL
ES_ThrowStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    PUSH_LOCATION();
    EMIT_DEBUGGER_STOP();
    compiler.EmitInstruction(ESI_THROW, expression->AsRegister(compiler));
    POP_LOCATION();

    return !compiler.HasError();
}


BOOL
ES_ThrowStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
        if (!expression->CallVisitor(visitor))
            return FALSE;

    return visitor->Leave(this);
}


void
ES_TryStmt::Prepare(ES_Compiler &compiler)
{
    /* This statement causes unpredictable code-flow, so forget about tracking
       anything anywhere in this function. */
    compiler.SuspendValueTracking();

    try_block->Prepare(compiler);

    if (catch_block)
        catch_block->Prepare(compiler);

    if (finally_block)
        finally_block->Prepare(compiler);
}


class ES_ContainsFunctionExpr
    : public ES_StatementVisitor
{
public:
    ES_ContainsFunctionExpr()
        : result(FALSE)
    {
    }

    virtual BOOL Visit(ES_Expression *expr);
    virtual BOOL Enter(ES_Statement *stmt, BOOL &skip);
    virtual BOOL Leave(ES_Statement *stmt);

    BOOL result;
};


/* virtual */ BOOL
ES_ContainsFunctionExpr::Visit(ES_Expression *expr)
{
    if (expr->GetType() == ES_Expression::TYPE_FUNCTION)
    {
        result = TRUE;
        return FALSE;
    }
    else
        return TRUE;
}


/* virtual */ BOOL
ES_ContainsFunctionExpr::Enter(ES_Statement *stmt, BOOL &skip)
{
    return TRUE;
}


/* virtual */ BOOL
ES_ContainsFunctionExpr::Leave(ES_Statement *stmt)
{
    return TRUE;
}


BOOL
ES_TryStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_Compiler::JumpTarget finally_target, catch_target, finally_done_target;
    ES_Compiler::Register return_target;

    if (finally_block)
    {
        /* Load return_target with the address of finally_done_target so that
           we have a default jump target if the try/catch/finally doesn't
           break/continue/return. Push return_target on the rethrow target
           stack to handle nested try blocks. */
        return_target = compiler.Temporary();

        /* To correctly propagate any non-exceptional return value from nested
           try-finally statements within a function, we arrange for 'return'
           statements to flag that a return is required. Any enclosing 'finally'
           blocks must first be executed before the outermost block returns
           the value. Should any of those intervening blocks also return, their
           return value will naturally be the one propagated in the end. */
        if (!compiler.GetFinallyTarget() && compiler.GetCodeType() == ES_Compiler::CODETYPE_FUNCTION)
        {
            if (!compiler.TryBlockHaveReturnedRegister().IsValid())
                compiler.TryBlockHaveReturnedRegister() = compiler.Temporary();
            compiler.EmitInstruction(ESI_LOAD_FALSE, compiler.TryBlockHaveReturnedRegister());
        }

        compiler.PushRethrowTarget(return_target);
        finally_done_target = compiler.ForwardJump();
        compiler.EmitInstruction(ESI_LOAD_INT32, return_target, finally_done_target);

        finally_target = compiler.ForwardJump();
        compiler.PushFinallyTarget(finally_target);
        compiler.StartExceptionHandler(finally_target, TRUE);
    }

    if (catch_block)
    {
        catch_target = compiler.ForwardJump();
        compiler.StartExceptionHandler(catch_target, FALSE);
    }

    if (!try_block->Compile(compiler, dst))
        return FALSE;

    if (catch_block)
    {
        compiler.EndExceptionHandler();

        /* If we exited normally out of the try block, just jump past the catch
           block. */
        ES_Compiler::JumpTarget after_catch(compiler.ForwardJump());
        compiler.EmitJump(NULL, ESI_JUMP, after_catch);

        compiler.SetJumpTarget(catch_target);

        // FIXME: Register should be accessible inside the catch block.
        ES_Compiler::Register rexception(compiler.Temporary());
        ES_ContainsFunctionExpr contains_function_expr;

        catch_block->CallVisitor(&contains_function_expr);

        BOOL complicated = compiler.HasInnerScopes() || contains_function_expr.result;
        if (complicated)
        {
            compiler.EmitInstruction(ESI_CATCH_SCOPE, rexception, compiler.Identifier(catch_identifier));
            compiler.PushInnerScope(rexception);
        }
        else
        {
            compiler.EmitInstruction(ESI_CATCH, rexception);
            compiler.PushCaughtException(catch_identifier, rexception);
        }

        if (!catch_block->Compile(compiler, dst))
            return FALSE;

        if (complicated)
            compiler.PopInnerScope();
        else
            compiler.PopCaughtException();

        /* If the catch block hasn't returned/jumped, nullify any inner returns
           (whose unwinding subsequently caused an exception.) */
        if (compiler.TryBlockHaveReturnedRegister().IsValid())
            compiler.EmitInstruction(ESI_LOAD_FALSE, compiler.TryBlockHaveReturnedRegister());

        compiler.SetJumpTarget(after_catch);
    }

    if (finally_block)
    {
        compiler.EndExceptionHandler();
        compiler.SetJumpTarget(finally_target);
        compiler.PopFinallyTarget();

        ES_Compiler::Register rexception(compiler.Temporary());
        compiler.EmitInstruction(ESI_CATCH, rexception);

        if (!finally_block->Compile(compiler, compiler.Invalid()))
            return FALSE;

        compiler.PopRethrowTarget();

        /* Try to rethrow a caught exception or unwind to next finally handler if we have
           nested handlers. ESI_RETHROW copies return_target to the rethrow target enclosing
           the current try block if such a block exists (otherwise it will just copy return_target
           to return_target, this is a hack to get away with not making ESI_RETHROW have
           a variable number of operands) */
        if (compiler.GetFinallyTarget() != NULL)
            compiler.EmitInstruction(ESI_RETHROW, rexception, return_target, compiler.GetRethrowTarget());
        else
            compiler.EmitInstruction(ESI_RETHROW, rexception, return_target, return_target);

        /* Jump to the value of return_target. This will only be reached if the rethrow instruction
           above doesn't find an enclosing finally handler and no exception was thrown. The contents
           of return_target will either be the address of finally_done_target or something written
           due to a return/break/continue. The jump to return_target could actually be handled by ESI_RETHROW
           but we choose to do it with ESI_JUMP_INDIRECT to make ESI_RETHROW a bit less magical. */
        compiler.EmitInstruction(ESI_JUMP_INDIRECT, return_target);

        compiler.SetJumpTarget(finally_done_target);
        if (!compiler.GetFinallyTarget() && compiler.GetCodeType() == ES_Compiler::CODETYPE_FUNCTION)
            compiler.EmitJump(&compiler.TryBlockHaveReturnedRegister(), ESI_JUMP_IF_TRUE, compiler.FunctionReturnTarget(), UINT_MAX);
    }

    return TRUE;
}


BOOL
ES_TryStmt::CallVisitor(ES_StatementVisitor *visitor)
{
    BOOL skip = FALSE;

    if (!visitor->Enter(this, skip))
        return FALSE;

    if (!skip)
    {
        if (!try_block->CallVisitor(visitor))
            return FALSE;
        if (catch_block && !catch_block->CallVisitor(visitor))
            return FALSE;
        if (finally_block && !finally_block->CallVisitor(visitor))
            return FALSE;
    }

    return visitor->Leave(this);
}


#ifdef ECMASCRIPT_DEBUGGER
BOOL
ES_DebuggerStmt::Compile(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (compiler.GetParser()->GetIsDebugging())
    {
        PUSH_LOCATION();
        compiler.EmitInstruction(ESI_DEBUGGER_STOP, ES_DebugListener::ESEV_BREAKHERE);
        POP_LOCATION();
    }

    return TRUE;
}
#endif // ECMASCRIPT_DEBUGGER
