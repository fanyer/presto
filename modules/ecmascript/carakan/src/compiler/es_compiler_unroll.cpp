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
#include "modules/ecmascript/carakan/src/compiler/es_compiler_unroll.h"

class ES_LoopBodyAnalyzer
    : public ES_StatementVisitor
{
public:
    ES_LoopBodyAnalyzer(ES_Compiler &compiler, const ES_Compiler::Register &loop_variable, const ES_Compiler::Register &limit_variable, ES_Statement *loop_body, ES_Statement *loop_iteration)
        : compiler(compiler),
          loop_variable(loop_variable),
          limit_variable(limit_variable),
          loop_body(loop_body),
          loop_iteration(loop_iteration),
          repeat(TRUE)
    {
    }

    BOOL Analyze();

    unsigned GetWeight() { return weight; }
    unsigned GetBenefit() { return benefit; }

    ES_Expression *GetModifier() { return modifier_expr; }
    ES_Expression::Type GetModifierType() { return modifier_type; }
    int GetModifierOperand() { return modifier_operand; }

private:
    virtual BOOL Visit(ES_Expression *expression);
    virtual BOOL Enter(ES_Statement *statement, BOOL &skip);
    virtual BOOL Leave(ES_Statement *statement);

    ES_Compiler &compiler;
    ES_Compiler::Register loop_variable, limit_variable;
    ES_Statement *loop_body, *loop_iteration;

    BOOL repeat;
    unsigned weight, benefit;
    unsigned loop_variable_locked;

    ES_Expression *modifier_expr;
    ES_Expression::Type modifier_type;
    int modifier_operand;
};

BOOL
ES_LoopBodyAnalyzer::Analyze()
{
    while (repeat)
    {
        repeat = FALSE;
        weight = benefit = loop_variable_locked = 0;

        modifier_expr = NULL;

        if (!loop_body->CallVisitor(this))
            return FALSE;
        if (loop_iteration && !loop_iteration->CallVisitor(this))
            return FALSE;
    }

    return TRUE;
}

/* virtual */ BOOL
ES_LoopBodyAnalyzer::Visit(ES_Expression *expression)
{
    switch (expression->GetType())
    {
    case ES_Expression::TYPE_IDENTIFIER:
        if (static_cast<ES_IdentifierExpr *>(expression)->AsVariable(compiler) == loop_variable)
            ++benefit;
        break;

    case ES_Expression::TYPE_ARRAY_REFERENCE:
    case ES_Expression::TYPE_PROPERTY_REFERENCE:
    case ES_Expression::TYPE_CALL:
    case ES_Expression::TYPE_NEW:
        weight += 4;
        break;

    case ES_Expression::TYPE_INCREMENT_OR_DECREMENT:
        {
            ES_Expression *target = static_cast<ES_IncrementOrDecrementExpr *>(expression)->GetExpression();

            if (target->GetType() == ES_Expression::TYPE_IDENTIFIER)
            {
                ES_Compiler::Register variable(static_cast<ES_IdentifierExpr *>(target)->AsVariable(compiler));

                if (variable.IsValid())
                    if (variable == loop_variable)
                    {
                        if (loop_variable_locked)
                            return FALSE;

                        if (modifier_expr)
                            /* More than one modification of the loop variable. */
                            return FALSE;

                        modifier_expr = expression;
                        modifier_type = ES_Expression::TYPE_ADD;

                        switch (static_cast<ES_IncrementOrDecrementExpr *>(expression)->GetType())
                        {
                        case ES_IncrementOrDecrementExpr::PRE_INCREMENT:
                        case ES_IncrementOrDecrementExpr::POST_INCREMENT:
                            modifier_operand = 1;
                            break;

                        default:
                            modifier_operand = -1;
                        }
                    }
                    else if (variable == limit_variable)
                        return FALSE;
            }
            else
                ++weight;
        }
        break;

    case ES_Expression::TYPE_ASSIGN:
        {
            ES_AssignExpr *assign = static_cast<ES_AssignExpr *>(expression);
            ES_Expression *target = assign->GetTarget(), *source = assign->GetSource();

            if (!target)
                /* Compound assignment. */
                target = static_cast<ES_BinaryExpr *>(source)->GetLeft();

            if (target->GetType() == ES_Expression::TYPE_IDENTIFIER)
            {
                ES_Compiler::Register variable(static_cast<ES_IdentifierExpr *>(target)->AsVariable(compiler));

                if (variable.IsValid())
                {
                    BOOL target_was_immediate = target->IsImmediateInteger(compiler);
                    BOOL source_is_immediate = source->IsImmediateInteger(compiler);

                    if (target_was_immediate)
                        if (!source_is_immediate)
                        {
                            compiler.ResetLocalValue(variable, FALSE);

                            /* We've changed our minds about a variable that we
                               thought was constant: need to repeat the analysis
                               with this new information in case it
                               propagates. */
                            repeat = TRUE;
                        }
                        else
                            ++benefit;

                    if (variable == loop_variable)
                    {
                        if (loop_variable_locked || !source_is_immediate)
                            return FALSE;

                        switch (source->GetType())
                        {
                        default:
                            return FALSE;

                        case ES_Expression::TYPE_MULTIPLY:
                        case ES_Expression::TYPE_DIVIDE:
                        case ES_Expression::TYPE_SUBTRACT:
                        case ES_Expression::TYPE_ADD:
                        case ES_Expression::TYPE_SHIFT_LEFT:
                        case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT:
                        case ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT:
                            if (modifier_expr)
                                /* More than one modification of the loop variable. */
                                return FALSE;

                            modifier_expr = expression;
                            modifier_type = source->GetType();

                            ES_Expression *left = static_cast<ES_BinaryExpr *>(source)->GetLeft();
                            ES_Expression *right = static_cast<ES_BinaryExpr *>(source)->GetRight();
                            ES_Expression *variable, *immediate;

                            if (left->GetType() == ES_Expression::TYPE_IDENTIFIER)
                                variable = left, immediate = right;
                            else if (right->GetType() == ES_Expression::TYPE_IDENTIFIER)
                                variable = right, immediate = left;
                            else
                                return FALSE;

                            if (immediate->GetType() != ES_Expression::TYPE_LITERAL)
                                return FALSE;

                            if (static_cast<ES_IdentifierExpr *>(variable)->AsVariable(compiler) != loop_variable)
                                return FALSE;

                            modifier_operand = static_cast<ES_LiteralExpr *>(immediate)->GetValue().GetNumAsInt32();
                        }
                    }
                    else if (variable == limit_variable)
                        return FALSE;
                }
            }
        }
        break;

    case ES_Expression::TYPE_COMMA:
        break;

    default:
        if (!expression->IsImmediate(compiler))
            ++weight;
    }

    return TRUE;
}

/* virtual */ BOOL
ES_LoopBodyAnalyzer::Enter(ES_Statement *statement, BOOL &skip)
{
    if (loop_variable_locked)
        ++loop_variable_locked;

    switch (statement->GetType())
    {
    case ES_Statement::TYPE_IF:
        if (loop_variable_locked == 0)
            ++loop_variable_locked;
        return TRUE;

    case ES_Statement::TYPE_WHILE:
    case ES_Statement::TYPE_FOR:
    case ES_Statement::TYPE_FORIN:
        /* Don't unroll nested loops (except inner-most one.) */
        return FALSE;

    case ES_Statement::TYPE_BREAK:
    case ES_Statement::TYPE_CONTINUE:
        /* Skip loops that do funny stuff. */
        return FALSE;

    case ES_Statement::TYPE_WITH:
        /* Code that uses with() is bad and doesn't deserve to be optimized. */
        return FALSE;

    case ES_Statement::TYPE_SWITCH:
        /* A switch will typically generate too much code and extra structures
           to make sense to repeat. */
        return FALSE;

    case ES_Statement::TYPE_TRY:
        /* Same with try statements; doesn't make sense to repeat/optimize. */
        return FALSE;

    default:
        return TRUE;
    }
}

/* virtual */ BOOL
ES_LoopBodyAnalyzer::Leave(ES_Statement *statement)
{
    if (loop_variable_locked)
        --loop_variable_locked;

    return TRUE;
}

BOOL
ES_LoopUnroller::Unroll(const ES_Compiler::Register &dst)
{
#ifdef ECMASCRIPT_DEBUGGER
    if (ES_Parser *parser = compiler.GetParser())
        if (parser->GetIsDebugging())
            return FALSE;
#endif // ECMASCRIPT_DEBUGGER

    if (compiler.HasNestedFunctions() || compiler.UsesEval() || compiler.HasInnerScopes() || !loop_condition)
        return FALSE;

    switch (loop_condition->GetType())
    {
    case ES_Expression::TYPE_IDENTIFIER:
        loop_variable = static_cast<ES_IdentifierExpr *>(loop_condition)->AsVariable(compiler);
        if (!loop_variable.IsValid() || !loop_condition->GetImmediateInteger(compiler, start_value))
            return FALSE;
        break;

    default:
        return FALSE;

    case ES_Expression::TYPE_LESS_THAN:
    case ES_Expression::TYPE_GREATER_THAN:
    case ES_Expression::TYPE_LESS_THAN_OR_EQUAL:
    case ES_Expression::TYPE_GREATER_THAN_OR_EQUAL:
    case ES_Expression::TYPE_EQUAL:
    case ES_Expression::TYPE_STRICT_EQUAL:
    case ES_Expression::TYPE_NOT_EQUAL:
    case ES_Expression::TYPE_STRICT_NOT_EQUAL:
        ES_BooleanBinaryExpr *binary = static_cast<ES_BooleanBinaryExpr *>(loop_condition);
        ES_Expression *left = binary->GetLeft();
        ES_Expression *right = binary->GetRight();
        ES_Expression *limit;

        /* We expect one operand to be a real immediate and the other to be a
           local with a known integer value. */
        if (!left->IsImmediateInteger(compiler) || !right->IsImmediateInteger(compiler))
            return FALSE;

        if (left->GetType() == ES_Expression::TYPE_IDENTIFIER)
        {
            loop_variable = static_cast<ES_IdentifierExpr *>(left)->AsVariable(compiler);
            left->GetImmediateInteger(compiler, start_value);
            limit = right;
        }
        else if (right->GetType() == ES_Expression::TYPE_IDENTIFIER)
        {
            loop_variable = static_cast<ES_IdentifierExpr *>(right)->AsVariable(compiler);
            right->GetImmediateInteger(compiler, start_value);
            limit = left;
        }
        else
            return FALSE;

        if (!loop_variable.IsValid())
            return FALSE;

        limit->GetImmediateInteger(compiler, limit_value);

        if (limit->GetType() == ES_Expression::TYPE_IDENTIFIER)
            limit_variable = static_cast<ES_IdentifierExpr *>(limit)->AsVariable(compiler);
        else if (limit->GetType() != ES_Expression::TYPE_LITERAL)
            return FALSE;
    }

    ES_LoopBodyAnalyzer analyzer(compiler, loop_variable, limit_variable, loop_body, loop_iteration);

    int loop_variable_value = start_value;
    unsigned iterations = 0;

    void *backup = compiler.BackupLocalValues();

    if (!analyzer.Analyze())
        goto abandon;

    if (!analyzer.GetModifier() || analyzer.GetModifierOperand() == 0)
        goto abandon;

    while (TRUE)
    {
        compiler.SetLocalValue(loop_variable, loop_variable_value, FALSE);

        if (loop_type != ES_Statement::TYPE_DO_WHILE || iterations > 0)
        {
            BOOL condition;

            if (!loop_condition->IsKnownCondition(compiler, condition))
                goto abandon;

            if (!condition)
                break;
        }

        if (++iterations > 16)
            goto abandon;

        double current_value(loop_variable_value), next_value;

        switch (analyzer.GetModifierType())
        {
        case ES_Expression::TYPE_MULTIPLY:             next_value = current_value * analyzer.GetModifierOperand(); break;
        case ES_Expression::TYPE_DIVIDE:               next_value = current_value / analyzer.GetModifierOperand(); break;
        case ES_Expression::TYPE_SUBTRACT:             next_value = current_value - analyzer.GetModifierOperand(); break;
        case ES_Expression::TYPE_ADD:                  next_value = current_value + analyzer.GetModifierOperand(); break;
        case ES_Expression::TYPE_SHIFT_LEFT:           loop_variable_value <<= (analyzer.GetModifierOperand() & 31); continue;
        case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT:   loop_variable_value >>= (analyzer.GetModifierOperand() & 31); continue;
        case ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT: loop_variable_value = static_cast<unsigned>(loop_variable_value) >> (analyzer.GetModifierOperand() & 31); continue;
        default: goto abandon;
        }

        ES_Value_Internal value(next_value);

        if (!value.IsInt32())
            goto abandon;

        loop_variable_value = value.GetInt32();
    }

    if (analyzer.GetWeight() * iterations > analyzer.GetBenefit() * 16)
        goto abandon;

    loop_variable_value = start_value;

    if (loop_type == ES_Statement::TYPE_FOR)
        analyzer.GetModifier()->Disable();

    /* Restore everything to the state it was before we started analysing the
       loop. */
    compiler.RestoreLocalValues(backup);

    for (unsigned index = 0; index < iterations; ++index)
    {
        if (loop_type == ES_Statement::TYPE_FOR)
            compiler.SetLocalValue(loop_variable, loop_variable_value, FALSE);

        loop_body->Compile(compiler, dst);
        loop_iteration && loop_iteration->Compile(compiler, dst);

        switch (analyzer.GetModifierType())
        {
        case ES_Expression::TYPE_MULTIPLY:             loop_variable_value = loop_variable_value * analyzer.GetModifierOperand(); break;
        case ES_Expression::TYPE_DIVIDE:               loop_variable_value = loop_variable_value / analyzer.GetModifierOperand(); break;
        case ES_Expression::TYPE_SUBTRACT:             loop_variable_value = loop_variable_value - analyzer.GetModifierOperand(); break;
        case ES_Expression::TYPE_ADD:                  loop_variable_value = loop_variable_value + analyzer.GetModifierOperand(); break;
        case ES_Expression::TYPE_SHIFT_LEFT:           loop_variable_value <<= (analyzer.GetModifierOperand() & 31); continue;
        case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT:   loop_variable_value >>= (analyzer.GetModifierOperand() & 31); continue;
        case ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT: loop_variable_value = static_cast<unsigned>(loop_variable_value) >> (analyzer.GetModifierOperand() & 31); continue;
        default: goto abandon;
        }
    }

    compiler.SetLocalValue(loop_variable, loop_variable_value, FALSE);

    return TRUE;

abandon:
    compiler.RestoreLocalValues(backup);
    return FALSE;
}
