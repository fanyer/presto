/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#include "core/pch.h"


/**
 *
 * Code generation strategy for expressions
 * ========================================
 *
 * There are two types of expressions: those that have their value in a register
 * already, and those that don't.  There are also two types of contexts in which
 * a sub-expression's value can be calculated: one in which the value just needs
 * to be in some register for further use, or one in which the value needs to be
 * in a specific register.
 *
 * These four match pairwise: an expression whose value is in a register already
 * used in a context where any register index will do is a no-op; nothing needs
 * to be done.  Also, an expression that needs to calculate its value into some
 * external register is ever so happy to do so into a specific register
 * determined by the context in which it is used.
 *
 * The four also mismatch (more or less) pairwise: an expression whose value is
 * in a register already used in a context where it must be in some other
 * register provokes an ESI_COPY, which is essentially always wasted cycles.  An
 * expression that needs to calculate into an external register used in a
 * context where we don't already have a register to put it in means we need to
 * allocate a temporary register for the purpose.
 *
 *
 * Examples of expressions whose value is in a register already:
 *  - local variable references
 *  - simple and compound assignments to local variables (the value is first
 *    stored in the local variable register and then that register is used)
 *
 * Examples of the other type of expressions:
 *  - all binary expressions that produce a non-boolean value
 *  - literal expressions (NOTE: moving constants into registers early and then
 *    treating them as constant local variables is an obvious optimization)
 *
 *  - function calls (return value is in a register that first needs to hold the
 *    'this' object for the call)
 *
 *
 * In the implementation the two types of expressions implement different
 * virtual functions to generate their code: IntoRegister() is used to generate
 * code that puts a value into a register determined by the caller, and
 * AsRegister() returns the register in which the value is (sometimes after
 * having generated code that puts it there.)  Bridge implementations of the two
 * functions in the base class takes care of expressions used in the "wrong"
 * context; the bridge IntoRegister() calls AsRegister() and emits an ESI_COPY
 * to put the value in the specified register, and the bridge AsRegister()
 * allocates a temporary register, calls IntoRegister() to put the value there,
 * and returns the temporary register.  Some types of expressions implement both
 * IntoRegister() and AsRegister() since there are cases where either is the
 * best.
 *
 *
 * As a special case, IntoRegister() can be called with an invalid destination
 * register.  This means "result will not be used."  The compiler automatically
 * drops instructions emitted with an invalid register as the first operand.
 * FIXME: The compiler should of course only do this for instructions known to
 * have no side-effects; in other cases it should allocate a temporary register
 * and use instead.  ("Result will not be used" is also signalled by calling
 * ES_Expression::SetInVoidContext().  There's no real point in having both
 * mechanisms so SetInVoidContext() will probably be removed.)
 *
 * Another special case is the optional temporary register provided by the
 * caller to AsRegister().  This basically means the caller had a currently
 * unused temporary register, and in case AsRegister() needs to allocate a
 * temporary register to put the result in, it might as well use that one.  The
 * important difference between this and IntoRegister() is that AsRegister() is
 * free to ignore the provided temporary register, and the caller will mostly
 * not care either way.  This is simply a mechanism to reuse temporary registers
 * slightly more efficiently.  It is for instance used in the compilation of
 * binary expressions; sub-expressions are compiled using AsRegister(), with the
 * binary expression's destination register as the suggested temporary register
 * to AsRegister().  (And the other way around; if the binary expression is
 * compiled in AsRegister() mode, it will reuse either sub-expression's result
 * register for the final result provided that register is reusable.)
 *
 *
 * Registers are handled by the class ES_Compiler::Register.  For non-temporary
 * registers (for instance local variables) this is mostly a carrier of a
 * register index.  For temporary registers, the ES_Compiler::Register objects
 * hold references to an underlying reference counted object representing the
 * allocated temporary register (ES_Compiler::TemporaryRegister.)  Allocation is
 * very simple: the lowest register index not used is returned.
 *
 * A special case here is function calls: a function call expression needs to
 * allocate argc+2 temporary registers at the top of the register frame which
 * are then reused as the bottom of the called function's register frame.  These
 * are allocated like regular temporary registers except that an unused
 * temporary register with a lower index than a used temporary register can not
 * be returned.
 *
 *
 * A third context in which an expression can be compiled exist: as the
 * condition for a conditional jump.  This is implemented by the function
 * CompileAsCondition(), which takes two jump targets (one for true results and
 * one for false results) and a flag saying whether it should prefer to
 * conditionally jump to the true target or the false target (it will always
 * jump to both, but one of the jumps will usually have a zero offset and thus
 * an obvious no-op, and optimized away by the compiler later.)
 *
 * Like for IntoRegister() and AsRegister() there is a default implementation in
 * the base class that takes care of expressions that have no need for special
 * behaviour.  This implementation uses ESI_CONDITION to convert the value in a
 * register (the register in which the expression's value is stored) into a
 * boolean in the "implicit boolean register" that controls conditional jump
 * instructions, and then emits one conditional jump (to the prefered target)
 * and an unconditional jump to the other target.
 *
 *
 * The expression that has the most interesting CompileAsCondition() override is
 * the ES_LogicalExpr (used for '&&' and '||',) since it implements its shortcut
 * semantics by jumping to the provided jump targets.
 *
 */


#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_stmt.h"
#include "modules/ecmascript/carakan/src/compiler/es_compiler_expr.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"


void
ES_Expression::CompileInVoidContext(ES_Compiler &compiler)
{
    ES_Compiler::Register dst;

    switch (GetType())
    {
    case ES_Expression::TYPE_THIS:
    case ES_Expression::TYPE_LITERAL:
    case ES_Expression::TYPE_REGEXP_LITERAL:
    case ES_Expression::TYPE_FUNCTION:
        /* Guaranteed side-effect free: just forget about it. */
        return;

    case ES_Expression::TYPE_IDENTIFIER:
    case ES_Expression::TYPE_ARRAY_LITERAL:
    case ES_Expression::TYPE_OBJECT_LITERAL:
    case ES_Expression::TYPE_INCREMENT_OR_DECREMENT:
    case ES_Expression::TYPE_CALL:
    case ES_Expression::TYPE_NEW:
    case ES_Expression::TYPE_VOID:
    case ES_Expression::TYPE_LESS_THAN:
    case ES_Expression::TYPE_GREATER_THAN:
    case ES_Expression::TYPE_LESS_THAN_OR_EQUAL:
    case ES_Expression::TYPE_GREATER_THAN_OR_EQUAL:
    case ES_Expression::TYPE_INSTANCEOF:
    case ES_Expression::TYPE_IN:
    case ES_Expression::TYPE_EQUAL:
    case ES_Expression::TYPE_NOT_EQUAL:
    case ES_Expression::TYPE_STRICT_EQUAL:
    case ES_Expression::TYPE_STRICT_NOT_EQUAL:
    case ES_Expression::TYPE_ASSIGN:
    case ES_Expression::TYPE_LOGICAL_AND:
    case ES_Expression::TYPE_LOGICAL_OR:
    case ES_Expression::TYPE_CONDITIONAL:
        /* Doesn't need a valid destination register; in fact, fares better
           without it. */
        dst = compiler.Invalid();
        break;

    case ES_Expression::TYPE_ARRAY_REFERENCE:
    case ES_Expression::TYPE_PROPERTY_REFERENCE:
    case ES_Expression::TYPE_DELETE:
    case ES_Expression::TYPE_TYPEOF:
    case ES_Expression::TYPE_PLUS:
    case ES_Expression::TYPE_MINUS:
    case ES_Expression::TYPE_BITWISE_NOT:
    case ES_Expression::TYPE_LOGICAL_NOT:
    case ES_Expression::TYPE_COMMA:
        /* Must emit code because of (possible) side-effects, and
           emitted instruction must have a destination register. */
        dst = compiler.Temporary();
        break;

    case ES_Expression::TYPE_MULTIPLY:
    case ES_Expression::TYPE_DIVIDE:
    case ES_Expression::TYPE_REMAINDER:
    case ES_Expression::TYPE_SUBTRACT:
    case ES_Expression::TYPE_ADD:
    case ES_Expression::TYPE_SHIFT_LEFT:
    case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT:
    case ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT:
    case ES_Expression::TYPE_BITWISE_AND:
    case ES_Expression::TYPE_BITWISE_XOR:
    case ES_Expression::TYPE_BITWISE_OR:
        ES_BinaryExpr *binary = static_cast<ES_BinaryExpr *>(this);

        if (binary->GetLeft()->IsPrimitiveType() && binary->GetRight()->IsPrimitiveType())
        {
            binary->GetLeft()->CompileInVoidContext(compiler);
            binary->GetRight()->CompileInVoidContext(compiler);
            return;
        }
        else
            dst = compiler.Temporary();
    }

    SetInVoidContext();
    IntoRegister(compiler, dst);
}


BOOL
ES_Expression::IsLValue ()
{
    switch (GetType())
    {
    //case TYPE_THIS:
    case TYPE_IDENTIFIER:
    //case TYPE_LITERAL:
    //case TYPE_ARRAY_LITERAL:
    //case TYPE_OBJECT_LITERAL:
    case TYPE_ARRAY_REFERENCE:
    case TYPE_PROPERTY_REFERENCE:
    //case TYPE_CALL:
    //case TYPE_NEW:
        return TRUE;

    default:
        return FALSE;
    }
}


BOOL
ES_Expression::ImplicitBooleanResult()
{
    switch (type)
    {
    case TYPE_LESS_THAN:
    case TYPE_GREATER_THAN:
    case TYPE_LESS_THAN_OR_EQUAL:
    case TYPE_GREATER_THAN_OR_EQUAL:
    case TYPE_INSTANCEOF:
    case TYPE_IN:
    case TYPE_EQUAL:
    case TYPE_NOT_EQUAL:
    case TYPE_STRICT_EQUAL:
    case TYPE_STRICT_NOT_EQUAL:
        return TRUE;

    default:
        return FALSE;
    }
}


BOOL
ES_Expression::IsKnownCondition(ES_Compiler &compiler, BOOL &result)
{
    switch (type)
    {
    case TYPE_LESS_THAN:
    case TYPE_GREATER_THAN:
    case TYPE_LESS_THAN_OR_EQUAL:
    case TYPE_GREATER_THAN_OR_EQUAL:
    case TYPE_EQUAL:
    case TYPE_NOT_EQUAL:
    case TYPE_STRICT_EQUAL:
    case TYPE_STRICT_NOT_EQUAL:
        return static_cast<ES_BooleanBinaryExpr *>(this)->IsKnownCondition(compiler, result);

    default:
        ES_Value_Internal value;
        if (GetImmediateValue(compiler, value))
        {
            result = value.AsBoolean().GetBoolean();
            return TRUE;
        }
        return FALSE;
    }
}


BOOL
ES_Expression::IsImmediate(ES_Compiler &compiler)
{
    ES_Value_Internal value;
    return GetImmediateValue(compiler, value);
}


BOOL
ES_Expression::GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value)
{
    switch (type)
    {
    case TYPE_LITERAL:
        value = static_cast<ES_LiteralExpr *>(this)->GetValue();
        return TRUE;

    case TYPE_IDENTIFIER:
        return static_cast<ES_IdentifierExpr *>(this)->GetImmediateValue(compiler, value);

    case TYPE_MULTIPLY:
    case TYPE_DIVIDE:
    case TYPE_REMAINDER:
    case TYPE_SUBTRACT:
    case TYPE_ADD:
    case TYPE_SHIFT_LEFT:
    case TYPE_SHIFT_SIGNED_RIGHT:
    case TYPE_SHIFT_UNSIGNED_RIGHT:
    case TYPE_BITWISE_AND:
    case TYPE_BITWISE_XOR:
    case TYPE_BITWISE_OR:
    case TYPE_LESS_THAN:
    case TYPE_GREATER_THAN:
    case TYPE_LESS_THAN_OR_EQUAL:
    case TYPE_GREATER_THAN_OR_EQUAL:
    case TYPE_EQUAL:
    case TYPE_NOT_EQUAL:
    case TYPE_STRICT_EQUAL:
    case TYPE_STRICT_NOT_EQUAL:
        return static_cast<ES_BinaryExpr *>(this)->GetImmediateValue(compiler, value);

    case TYPE_TYPEOF:
    case TYPE_MINUS:
    case TYPE_BITWISE_NOT:
    case TYPE_LOGICAL_NOT:
        return static_cast<ES_UnaryExpr *>(this)->GetImmediateValue(compiler, value);

    case TYPE_COMMA:
        return static_cast<ES_CommaExpr *>(this)->GetImmediateValue(compiler, value);

    default:
        return FALSE;
    }
}


BOOL
ES_Expression::IsImmediateInteger(ES_Compiler &compiler)
{
    ES_Value_Internal value;
    return GetImmediateValue(compiler, value) && value.IsInt32();
}


BOOL
ES_Expression::GetImmediateInteger(ES_Compiler &compiler, int &ivalue)
{
    ES_Value_Internal value;
    if (GetImmediateValue(compiler, value) && value.IsInt32())
    {
        ivalue = value.GetInt32();
        return TRUE;
    }
    else
        return FALSE;
}


BOOL
ES_Expression::IsImmediateString(ES_Compiler &compiler)
{
    ES_Value_Internal value;
    return GetImmediateValue(compiler, value) && value.IsString();
}


BOOL
ES_Expression::GetImmediateString(ES_Compiler &compiler, JString *&svalue)
{
    ES_Value_Internal value;
    if (GetImmediateValue(compiler, value) && value.IsString())
    {
        svalue = value.GetString();
        return TRUE;
    }
    else
        return FALSE;
}


BOOL
ES_Expression::CanHaveSideEffects(ES_Compiler &compiler)
{
    if (type == TYPE_LITERAL || type == TYPE_THIS)
        return FALSE;
    else if (type == TYPE_IDENTIFIER)
        return compiler.HasInnerScopes() || !compiler.Local(static_cast<ES_IdentifierExpr *>(this)->GetName()).IsValid();
    else
        return TRUE;
}


BOOL
ES_Expression::IsIdentifier(JString *&name)
{
    if (type == TYPE_IDENTIFIER)
    {
        name = static_cast<ES_IdentifierExpr *>(this)->GetName();
        return TRUE;
    }
    else
        return FALSE;
}


JString *
ES_Expression::AsDebugName(ES_Context *context)
{
    if (GetType() == ES_Expression::TYPE_IDENTIFIER)
        return static_cast<ES_IdentifierExpr *>(this)->GetName();
    else if (GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
    {
        ES_Expression *base = static_cast<ES_PropertyReferenceExpr *>(this)->GetBase();

        if (base->GetType() == ES_Expression::TYPE_IDENTIFIER || base->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
        {
            unsigned length = Length(static_cast<ES_PropertyReferenceExpr *>(this)->GetName());

            while (TRUE)
                if (base->GetType() == ES_Expression::TYPE_IDENTIFIER)
                {
                    length += Length(static_cast<ES_IdentifierExpr *>(base)->GetName()) + 1;
                    break;
                }
                else if (base->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
                {
                    length += Length(static_cast<ES_PropertyReferenceExpr *>(base)->GetName()) + 1;
                    base = static_cast<ES_PropertyReferenceExpr *>(base)->GetBase();
                }
                else
                    goto skip;

            if (base->GetType() == ES_Expression::TYPE_IDENTIFIER)
            {
                JString *name = JString::Make(context, length);
                uni_char *storage = Storage(context, name) + length;

                base = this;

                while (TRUE)
                {
                    JString *part;

                    if (base->GetType() == ES_Expression::TYPE_IDENTIFIER)
                        part = static_cast<ES_IdentifierExpr *>(base)->GetName();
                    else
                        part = static_cast<ES_PropertyReferenceExpr *>(base)->GetName();

                    storage -= Length(part);
                    op_memcpy(storage, Storage(context, part), Length(part) * sizeof(uni_char));

                    if (base->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
                    {
                        base = static_cast<ES_PropertyReferenceExpr *>(base)->GetBase();

                        storage -= 1;
                        *storage = '.';
                    }
                    else
                        break;
                }

                OP_ASSERT(storage == Storage(context, name));

                return name;
            }
        }

    skip:
        return static_cast<ES_PropertyReferenceExpr *>(this)->GetName();
    }
    else
        return NULL;
}


void
ES_Expression::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_Compiler::Register result(AsRegister(compiler, dst.IsValid() ? &dst : NULL));

    if (dst.IsValid() && result != dst)
        compiler.EmitInstruction(ESI_COPY, dst, result);
}


ES_Compiler::Register
ES_Expression::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    ES_Compiler::Register dst;

    if (temporary && temporary->IsValid())
        dst = *temporary;
    else
    {
        dst = compiler.Temporary();
        dst.SetIsReusable();
    }

    IntoRegister(compiler, dst);

    return dst;
}


void
ES_Expression::CompileAsCondition(ES_Compiler &compiler, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index)
{
    BOOL result;
    if (IsKnownCondition(compiler, result))
        if (result)
            compiler.EmitJump(NULL, ESI_JUMP, true_target);
        else
            compiler.EmitJump(NULL, ESI_JUMP, false_target);
    else
    {
        ES_Compiler::Register condition(AsRegister(compiler));
        EmitConditionalJumps(compiler, &condition, true_target, false_target, prefer_jtrue, loop_index);
    }
}


BOOL
ES_Expression::CallVisitor(ES_ExpressionVisitor *visitor)
{
    return visitor->Visit(this);
}


void
ES_Expression::EmitConditionalJumps(ES_Compiler &compiler, const ES_Compiler::Register *condition_on, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index)
{
    if (prefer_jtrue)
    {
        compiler.EmitJump(condition_on, ESI_JUMP_IF_TRUE, true_target, loop_index);
        compiler.EmitJump(NULL, ESI_JUMP, false_target);
    }
    else
    {
        compiler.EmitJump(condition_on, ESI_JUMP_IF_FALSE, false_target, loop_index);
        compiler.EmitJump(NULL, ESI_JUMP, true_target);
    }
}


ES_Compiler::Register
ES_ThisExpr::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    return compiler.This();
}


void
ES_IdentifierExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    IntoRegister(compiler, dst, FALSE);
}


ES_Compiler::Register
ES_IdentifierExpr::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    ES_Compiler::Register variable(AsVariable(compiler));
    if (variable.IsValid())
    {
        ES_Value_Internal value;
        BOOL is_stored;

        if (compiler.GetLocalValue(value, variable, is_stored) && !is_stored)
            compiler.StoreRegisterValue(variable, value);

        return variable;
    }

    ES_Compiler::Register dst(temporary ? *temporary : compiler.Temporary());
    IntoRegister(compiler, dst);
    return dst;
}


ES_Compiler::Register
ES_IdentifierExpr::AsVariable(ES_Compiler &compiler, BOOL for_assignment/* = FALSE*/)
{
    if (!compiler.HasInnerScopes())
        return compiler.Local(identifier, for_assignment);
    else
        return compiler.Invalid();
}


ES_Compiler::Register
ES_IdentifierExpr::AsRegisterQuiet(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    ES_Compiler::Register variable(AsVariable(compiler));
    if (variable.IsValid())
        return variable;

    ES_Compiler::Register dst(temporary ? *temporary : compiler.Temporary());
    IntoRegister(compiler, dst, TRUE);
    return dst;
}


BOOL
ES_IdentifierExpr::GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value)
{
    ES_Compiler::Register variable(AsVariable(compiler));
    BOOL is_stored;

    return variable.IsValid() && compiler.GetLocalValue(value, variable, is_stored);
}


void
ES_IdentifierExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst, BOOL quiet)
{
    ES_Compiler::Register variable(AsVariable(compiler));
    if (variable.IsValid())
    {
        ES_Value_Internal value;
        BOOL is_stored;

        if (compiler.GetLocalValue(value, variable, is_stored))
        {
            if (dst.IsValid() && (dst != variable || !is_stored))
                compiler.StoreRegisterValue(dst, value);
        }
        else if (dst.IsValid() && dst != variable)
            compiler.EmitInstruction(ESI_COPY, dst, variable);

        return;
    }

    ES_Compiler::Register use_dst;
    if (dst.IsValid())
        use_dst = dst;
    else
        use_dst = compiler.Temporary();

    ES_CodeWord::Index scope_index, variable_index;
    BOOL is_lexical_read_only;

    if (compiler.GetLexical(scope_index, variable_index, identifier, is_lexical_read_only))
        compiler.EmitInstruction(ESI_GET_LEXICAL, use_dst, scope_index, variable_index);
    else if (!compiler.HasUnknownScope())
        compiler.EmitGlobalAccessor(ESI_GET_GLOBAL, identifier, use_dst, quiet);
    else
        compiler.EmitScopedAccessor(ESI_GET_SCOPE, identifier, &use_dst, NULL, quiet);
}


void
ES_LiteralExpr::CompileAsCondition(ES_Compiler &compiler, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index)
{
    value.ToBoolean();
    compiler.EmitJump(NULL, ESI_JUMP, value.GetBoolean() ? true_target : false_target);
}


void
ES_LiteralExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (in_void_context)
        return;
    else
        compiler.StoreRegisterValue(dst, value);
}


ES_Compiler::Register
ES_LiteralExpr::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    if (preloaded_register)
        return *preloaded_register;
    else
        return ES_Expression::AsRegister(compiler, temporary);
}

BOOL
ES_LiteralExpr::IntoRegisterNegated(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (value.IsInt32())
        compiler.EmitInstruction(ESI_LOAD_INT32, dst, -value.GetInt32());
    else if (value.IsBoolean())
        if (value.GetBoolean())
            compiler.EmitInstruction(ESI_LOAD_INT32, dst, -1);
        else
            compiler.EmitInstruction(ESI_LOAD_DOUBLE, dst, compiler.Double(compiler.GetContext()->rt_data->NegativeZero));
    else if (value.IsNumber())
        compiler.EmitInstruction(ESI_LOAD_DOUBLE, dst, compiler.Double(-value.GetNumAsDouble()));
    else
        return FALSE;

    return TRUE;
}


void
ES_RegExpLiteralExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    IntoRegister(compiler, dst, FALSE);
}

void
ES_RegExpLiteralExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst, BOOL can_share)
{
    if (info->source == UINT_MAX)
        info->source = compiler.String(source);

    if (index == UINT_MAX)
        index = compiler.RegExp(info);

    if (can_share)
        index |= 0x80000000u;

    compiler.EmitInstruction(ESI_NEW_REGEXP, dst, index);
}


void
ES_ArrayLiteralExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_Compiler::Register array(dst.IsValid() && (expressions_count == 0 || dst.IsReusable()) ? dst : compiler.Temporary());

    unsigned constants = 0;

    for (unsigned index = 0; index < expressions_count; ++index)
        if (ES_Expression *expression = expressions[index])
            if (expression->GetType() == TYPE_LITERAL)
                ++constants;

    BOOL using_construct;

    if (constants >= 4)
    {
        using_construct = TRUE;

        ES_CodeStatic::ConstantArrayLiteral *cal;
        unsigned cal_index;

        compiler.AddConstantArrayLiteral(cal, cal_index, constants, expressions_count);

        for (unsigned index1 = 0, index2 = 0; index1 < expressions_count && index2 < constants; ++index1)
            if (ES_Expression *expression = expressions[index1])
                if (expression->GetType() == TYPE_LITERAL)
                {
                    cal->indeces[index2] = index1;

                    ES_Value_Internal &value1 = static_cast<ES_LiteralExpr *>(expression)->GetValue();
                    int &value2 = cal->values[index2].value;

                    switch (cal->values[index2].type = value1.Type())
                    {
                    default:
                        break;

                    case ESTYPE_BOOLEAN:
                        value2 = value1.GetBoolean() ? 1 : 0;
                        break;

                    case ESTYPE_INT32:
                        value2 = value1.GetInt32();
                        break;

                    case ESTYPE_DOUBLE:
                        value2 = compiler.Double(value1.GetDouble());
                        break;

                    case ESTYPE_STRING:
                        value2 = compiler.String(value1.GetString());
                    }

                    ++index2;
                }

        compiler.EmitInstruction(ESI_CONSTRUCT_ARRAY, array, expressions_count, cal_index);
    }
    else
    {
        using_construct = FALSE;
        compiler.EmitInstruction(ESI_NEW_ARRAY, array, expressions_count);
    }

    if (!using_construct || constants < expressions_count)
        for (unsigned index = 0; index < expressions_count; ++index)
            if (ES_Expression *expression = expressions[index])
                if (!using_construct || expression->GetType() != TYPE_LITERAL)
                    compiler.EmitInstruction(ESI_PUTI_IMM, array, index, expression->AsRegister(compiler));

    if (dst.IsValid() && dst != array)
        compiler.EmitInstruction(ESI_COPY, dst, array);
}


BOOL
ES_ArrayLiteralExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    for (unsigned index = 0; index < expressions_count; ++index)
        if (ES_Expression *expression = expressions[index])
            if (!expression->CallVisitor(visitor))
                return FALSE;

    return TRUE;
}


BOOL
ES_ArrayLiteralExpr::IsDense()
{
    for (unsigned index = 0; index < expressions_count; ++index)
        if (!expressions[index])
            return FALSE;
    return TRUE;
}


void
ES_ObjectLiteralExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_Compiler::Register object(dst.IsValid() && (properties_count == 0 || dst.IsReusable()) ? dst : compiler.Temporary());
    unsigned index = 0, simple_named_count;

    for (simple_named_count = 0; simple_named_count < properties_count; ++simple_named_count)
    {
        Property &property = properties[simple_named_count];
        unsigned property_index;

        if (!property.expression || convertindex(Storage(NULL, property.name), Length(property.name), property_index) || property.name->Equals("__proto__", 9))
            break;
    }

    if (simple_named_count != 0)
    {
        ES_CodeStatic::ObjectLiteralClass *klass;

        unsigned object_literal_class_index = compiler.AddObjectLiteralClass(klass);

        klass->properties = OP_NEWA_L(unsigned, simple_named_count);
        klass->properties_count = simple_named_count;

        ES_Compiler::Register *values = OP_NEWGROA_L(ES_Compiler::Register, simple_named_count, compiler.Arena());

        for (index = 0; index < simple_named_count; ++index)
        {
            Property &property = properties[index];

            klass->properties[index] = compiler.String(property.name) | (property.expression->GetType() == TYPE_FUNCTION ? 0x80000000u : 0);
            values[index] = compiler.Temporary();
            property.expression->IntoRegister(compiler, values[index]);
        }

        compiler.EmitCONSTRUCT_OBJECT(object, object_literal_class_index, simple_named_count, values);

        for (unsigned value_index = 0; value_index < simple_named_count; ++value_index)
            values[value_index] = compiler.Invalid();
    }
    else
        compiler.EmitInstruction(ESI_NEW_OBJECT, object, compiler.GetObjectLiteralClassIndex());

    for (; index < properties_count; ++index)
    {
        Property &property = properties[index];

        if (property.expression)
        {
            ES_Compiler::Register value(property.expression->AsRegister(compiler));
            unsigned property_index;

            if (convertindex(Storage(NULL, property.name), Length(property.name), property_index))
                compiler.EmitInstruction(ESI_PUTI_IMM, object, property_index, value);
            else
                compiler.EmitPropertyAccessor(ESI_INIT_PROPERTY, property.name, object, value);
        }
        else
        {
            unsigned name = compiler.Identifier(property.name);

            if (property.getter)
            {
                ES_FunctionExpr *getter = static_cast<ES_FunctionExpr *>(property.getter);
                ES_CodeWord::Index function_index;

                if (getter->GetFunctionCode())
                    function_index = compiler.Function(getter->GetFunctionCode());
                else
                    function_index = getter->GetFunctionIndex();

                OP_ASSERT(!compiler.HasInnerScopes() || compiler.GetFunctionData(function_index).inside_inner_scope);

                compiler.EmitInstruction(ESI_DEFINE_GETTER, object, name, function_index, compiler.GetInnerScopeIndex());
            }
            if (property.setter)
            {
                ES_FunctionExpr *setter = static_cast<ES_FunctionExpr *>(property.setter);
                ES_CodeWord::Index function_index;

                if (setter->GetFunctionCode())
                    function_index = compiler.Function(setter->GetFunctionCode());
                else
                    function_index = setter->GetFunctionIndex();

                OP_ASSERT(!compiler.HasInnerScopes() || compiler.GetFunctionData(function_index).inside_inner_scope);

                compiler.EmitInstruction(ESI_DEFINE_SETTER, object, name, function_index, compiler.GetInnerScopeIndex());
            }
        }
    }

    if (dst.IsValid() && dst != object)
        compiler.EmitInstruction(ESI_COPY, dst, object);
}


BOOL
ES_ObjectLiteralExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    for (unsigned index = 0; index < properties_count; ++index)
        if (ES_Expression *expression = properties[index].expression)
            if (!expression->CallVisitor(visitor))
                return FALSE;

    return TRUE;
}


void
ES_FunctionExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (function_code)
        compiler.EmitInstruction(ESI_NEW_FUNCTION, dst, compiler.Function(function_code), compiler.GetInnerScopeIndex());
    else
    {
        OP_ASSERT(!compiler.HasInnerScopes() || compiler.GetFunctionData(function_index).inside_inner_scope);

        compiler.EmitInstruction(ESI_NEW_FUNCTION, dst, function_index, compiler.GetInnerScopeIndex());
    }
}


ES_Compiler::Register
ES_PropertyAccessorExpr::BaseAsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    if (rbase.IsValid())
    {
        // We don't want to keep the register alive beyond this, so we
        // reset it and return a copy of it.
        ES_Compiler::Register temporary_variable(rbase);
        rbase.Reset();
        return temporary_variable;
    }
    else
        return base->AsRegister(compiler, temporary);
}


class ES_IsVariableTrampled
    : private ES_ExpressionVisitor
{
public:
    ES_IsVariableTrampled(ES_Compiler &compiler, ES_Expression *expr, JString *name)
        : name(name)
    {
        if (!expr->CanHaveSideEffects(compiler))
            is_trampled = FALSE;
        else
            is_trampled = compiler.UsesEval() || compiler.HasNestedFunctions() || !expr->CallVisitor(this);
    }

    BOOL is_trampled;

private:
    virtual BOOL Visit(ES_Expression *expression);

    BOOL IsTheVariable(ES_Expression *expression)
    {
        return expression->GetType() == ES_Expression::TYPE_IDENTIFIER && static_cast<ES_IdentifierExpr *>(expression)->GetName() == name;
    }

    JString *name;
};


BOOL
ES_IsVariableTrampled::Visit(ES_Expression *expression)
{
    switch (expression->GetType())
    {
    case ES_Expression::TYPE_ASSIGN:
        {
            ES_Expression *target = static_cast<ES_AssignExpr *>(expression)->GetTarget();

            if (!target)
            {
                ES_Expression *source = static_cast<ES_AssignExpr *>(expression)->GetSource();
                target = static_cast<ES_BinaryExpr *>(source)->GetLeft();
            }

            if (IsTheVariable(target))
                return FALSE;
        }
        break;

    case ES_Expression::TYPE_INCREMENT_OR_DECREMENT:
        {
            ES_Expression *target = static_cast<ES_IncrementOrDecrementExpr *>(expression)->GetExpression();

            if (IsTheVariable(target))
                return FALSE;
        }
        break;
    }

    return TRUE;
}


void
ES_ArrayReferenceExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    BOOL previous_want_object = compiler.PushWantObject();
    ES_Compiler::Register rbase(BaseAsRegister(compiler, dst.IsTemporary() ? &dst : NULL));
    compiler.PopWantObject(previous_want_object);

    int immediate;
    if (index->GetImmediateInteger(compiler, immediate) && immediate >= 0)
    {
        compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());
        compiler.EmitInstruction(ESI_GETI_IMM, dst, rbase, immediate);
        return;
    }

    if (compiler.IsLocal(rbase))
    {
        JString *name = compiler.GetLocal(rbase);
        ES_IsVariableTrampled checker(compiler, index, name);

        if (checker.is_trampled)
        {
            ES_Compiler::Register tmp(rbase);
            rbase = compiler.Temporary();
            compiler.EmitInstruction(ESI_COPY, rbase, tmp);
        }
    }

    ES_Compiler::Register rindex = index->AsRegister(compiler, dst.IsReusable() && dst != rbase ? &dst : NULL);
    compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());

    compiler.EmitPropertyAccessor(ESI_GET, rindex, rbase, dst);
}


BOOL
ES_ArrayReferenceExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (!base->CallVisitor(visitor))
        return FALSE;

    if (!index->CallVisitor(visitor))
        return FALSE;

    return TRUE;
}


void
ES_ArrayReferenceExpr::EvaluateIndex(ES_Compiler &compiler, ES_Compiler::Register &rindex)
{
    if (!index->IsImmediateInteger(compiler))
        rindex = index->AsRegister(compiler);
    else
        rindex = compiler.Invalid();
}


void
ES_ArrayReferenceExpr::GetTo(ES_Compiler &compiler, const ES_Compiler::Register &dst, ES_Compiler::Register &rindex)
{
    BOOL previous_want_object = compiler.PushWantObject();
    ES_Compiler::Register rbase(BaseAsRegister(compiler, dst.IsTemporary() ? &dst : NULL));
    compiler.PopWantObject(previous_want_object);

    int immediate;
    if (index->GetImmediateInteger(compiler, immediate) && immediate >= 0)
    {
        compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());
        compiler.EmitInstruction(ESI_GETI_IMM, dst, rbase, immediate);
        return;
    }

    rindex = index->AsRegister(compiler);

    compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());

    compiler.EmitInstruction(ESI_GET, dst, rbase, rindex);
}


void
ES_ArrayReferenceExpr::PutFrom(ES_Compiler &compiler, const ES_Compiler::Register &src, const ES_Compiler::Register &rindex, const ES_Compiler::Register *rbase0)
{
    BOOL previous_want_object = compiler.PushWantObject();
    ES_Compiler::Register rbase(rbase0 ? *rbase0 : BaseAsRegister(compiler));
    compiler.PopWantObject(previous_want_object);

    int immediate;
    if (index->GetImmediateInteger(compiler, immediate) && immediate >= 0)
    {
        compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());
        compiler.EmitInstruction(ESI_PUTI_IMM, rbase, immediate, src);
        return;
    }

    ES_Compiler::Register rindex_used = rindex.IsValid() ? rindex : index->AsRegister(compiler);
    compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());

    compiler.EmitInstruction(ESI_PUT, rbase, rindex_used, src);
}


void
ES_ArrayReferenceExpr::Delete(ES_Compiler &compiler)
{
    ES_Compiler::Register rbase(BaseAsRegister(compiler));

    int immediate;
    if (index->GetImmediateInteger(compiler, immediate) && immediate >= 0)
    {
        compiler.EmitInstruction(ESI_DELETEI_IMM, rbase, immediate);
        return;
    }

    ES_Compiler::Register rindex = index->AsRegister(compiler);
    compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());

    compiler.EmitInstruction(ESI_DELETE, rbase, rindex);
}


void
ES_PropertyReferenceExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    BOOL previous_want_object = compiler.PushWantObject();
    ES_Compiler::Register rbase(BaseAsRegister(compiler, dst.IsTemporary() ? &dst : NULL));
    compiler.PopWantObject(previous_want_object);

    compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());

    compiler.EmitPropertyAccessor(ESI_GETN_IMM, name, rbase, dst);
}


BOOL
ES_PropertyReferenceExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (!base->CallVisitor(visitor))
        return FALSE;

    return TRUE;
}


void
ES_PropertyReferenceExpr::GetTo(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_Compiler::Register rbase(BaseAsRegister(compiler, dst.IsTemporary() ? &dst : NULL));

    compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());

    compiler.EmitPropertyAccessor(ESI_GETN_IMM, name, rbase, dst);
}


void
ES_PropertyReferenceExpr::PutFrom(ES_Compiler &compiler, const ES_Compiler::Register &src, const ES_Compiler::Register *rbase0)
{
    ES_Compiler::Register rbase(rbase0 ? *rbase0 : BaseAsRegister(compiler));

    compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());

    compiler.EmitPropertyAccessor(ESI_PUTN_IMM, name, rbase, src);
}


void
ES_PropertyReferenceExpr::Delete(ES_Compiler &compiler)
{
    ES_Compiler::Register rbase(BaseAsRegister(compiler));

    compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, base->GetSourceLocation());

    compiler.EmitInstruction(ESI_DELETEN_IMM, rbase, compiler.Identifier(name));
}


ES_Compiler::Register
ES_CallExpr::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    if (compiler.IsRedirectedCall(this))
    {
        ES_Compiler::Register rfunction(compiler.Temporary(TRUE));
        ES_Compiler::Register rapply(compiler.Temporary(TRUE));

        static_cast<ES_PropertyReferenceExpr *>(function)->GetBase()->IntoRegister(compiler, rfunction);

        compiler.EmitPropertyAccessor(ESI_GETN_IMM, compiler.GetApplyIdentifier(), rfunction, rapply);
        compiler.EmitInstruction(ESI_REDIRECTED_CALL, rfunction, rapply);

        return compiler.This();
    }
    else
    {
        ES_Expression **args = arguments;
        unsigned argc = arguments_count, argc_flag = 0;
        BOOL is_apply = FALSE;

        if (function->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
        {
            JString *fnname = static_cast<ES_PropertyReferenceExpr *>(function)->GetName();

            // When debugging, don't emit ESI_APPLY.
            if (fnname->Equals("apply", 5) && argc == 2 && args[1]->GetType() == ES_Expression::TYPE_ARRAY_LITERAL && (is_apply = !compiler.GetParser()->GetIsDebugging()))
            {
                ES_ArrayLiteralExpr *array = static_cast<ES_ArrayLiteralExpr *>(args[1]);

                if (array->IsDense())
                {
                    ES_Expression **expressions = array->GetExpressions();

                    argc = 1 + array->GetCount();

                    args = OP_NEWGROA_L(ES_Expression *, argc, compiler.Arena());
                    args[0] = arguments[0];

                    for (unsigned index = 1; index < argc; ++index)
                        args[index] = expressions[index - 1];
                }
                else
                    is_apply = FALSE;
            }
        }

        ES_Compiler::Register rfunction;
        ES_Compiler::Register rthis;
        ES_Compiler::Register rarg0;

        if (is_apply)
        {
            rfunction = compiler.Temporary(TRUE);
            rarg0 = compiler.Temporary(TRUE);
            rthis = compiler.Temporary(TRUE);
        }
        else
            rthis = compiler.Temporary(TRUE, !is_apply && temporary && compiler.SafeAsThisRegister(*temporary) ? temporary : NULL);

        ES_CodeWord::Index scope_index, variable_index;
        BOOL is_local = FALSE, is_lexical = FALSE, is_lexical_read_only;

        if (function->GetType() == ES_Expression::TYPE_IDENTIFIER)
        {
            is_local = static_cast<ES_IdentifierExpr *>(function)->AsVariable(compiler, FALSE).IsValid();
            is_lexical = !is_local && compiler.GetLexical(scope_index, variable_index, static_cast<ES_IdentifierExpr*>(function)->GetName(), is_lexical_read_only);
        }

        if (!is_local && !is_lexical && compiler.HasUnknownScope() && function->GetType() == ES_Expression::TYPE_IDENTIFIER)
        {
            OP_ASSERT(!is_apply);
            rfunction = compiler.Temporary(TRUE);
            compiler.EmitScopedAccessor(ESI_GET_SCOPE_REF, static_cast<ES_IdentifierExpr*>(function)->GetName(), &rfunction, &rthis, TRUE);
        }
        else
        {
            if (function->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE ||
                function->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
            {
                ES_PropertyAccessorExpr *base = static_cast<ES_PropertyAccessorExpr *>(function);

#ifdef ES_BASE_REFERENCE_INFORMATION
                BOOL previous_want_object = compiler.PushWantObject();
#endif // ES_BASE_REFERENCE_INFORMATION

                if (function->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE && base->GetBase()->GetType() == TYPE_REGEXP_LITERAL)
                {
                    JString *name = static_cast<ES_PropertyReferenceExpr *>(function)->GetName();

                    if (name->Equals("test", 4) || name->Equals("exec", 4))
                    {
                        static_cast<ES_RegExpLiteralExpr *>(base->GetBase())->IntoRegister(compiler, rthis, TRUE);
                        goto this_done;
                    }
                }

                base->GetBase()->IntoRegister(compiler, rthis);

            this_done:
#ifdef ES_BASE_REFERENCE_INFORMATION
                compiler.PopWantObject(previous_want_object);
#endif // ES_BASE_REFERENCE_INFORMATION

                base->SetRegister(rthis);
            }
            else
                argc_flag = 0x80000000u;

            if (!rfunction.IsValid())
                rfunction = compiler.Temporary(TRUE);

            BOOL previous_want_object = compiler.PushWantObject();
            function->IntoRegister(compiler, rfunction);
            compiler.PopWantObject(previous_want_object);
        }

        ES_Compiler::Register *rarguments = OP_NEWA_L(ES_Compiler::Register, argc);
        ANCHOR_ARRAY(ES_Compiler::Register, rarguments);

        for (unsigned index = 0; index < argc; ++index)
        {
            if (index == 0 && rarg0.IsValid())
                rarguments[0] = rarg0;
            else
                rarguments[index] = compiler.Temporary(TRUE);

            args[index]->IntoRegister(compiler, rarguments[index]);
        }

        compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, function->GetSourceLocation());

        if (is_apply)
        {
            compiler.EmitInstruction(ESI_APPLY, rfunction, argc - 1);
            return rarg0;
        }
        else if (IsEvalCall(compiler))
        {
            unsigned inner_scope_index = compiler.GetInnerScopeIndex();

            if (compiler.GetCodeType() == ES_Compiler::CODETYPE_FUNCTION && inner_scope_index == ~0u && !compiler.HasLexicalScopes() && !compiler.HasOuterScopes() && compiler.HasSingleEvalCall())
                --inner_scope_index;

            compiler.EmitInstruction(ESI_EVAL, rthis, argc | argc_flag, inner_scope_index, compiler.GetEvalCache());
        }
        else
            compiler.EmitInstruction(ESI_CALL, rthis, argc | argc_flag);

        // NOTE: assuming called function stores return value in the
        // register used for 'this'.
        return rthis;
    }
}


BOOL
ES_CallExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (!function->CallVisitor(visitor))
        return FALSE;

    for (unsigned index = 0; index < arguments_count; ++index)
        if (!arguments[index]->CallVisitor(visitor))
            return FALSE;

    return TRUE;
}


ES_Compiler::Register
ES_NewExpr::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    ES_Compiler::Register rthis(compiler.Temporary(TRUE, temporary && temporary->IsTemporary() ? temporary : NULL));
    ES_Compiler::Register rfunction(compiler.Temporary(TRUE));

    function->IntoRegister(compiler, rfunction);

    ES_Compiler::Register *rarguments = OP_NEWA_L(ES_Compiler::Register, arguments_count);
    ANCHOR_ARRAY(ES_Compiler::Register, rarguments);

    for (unsigned index = 0; index < arguments_count; ++index)
    {
        rarguments[index] = compiler.Temporary(TRUE);
        arguments[index]->IntoRegister(compiler, rarguments[index]);
    }

    compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, function->GetSourceLocation());

    compiler.EmitInstruction(ESI_CONSTRUCT, rthis, arguments_count);

    return rthis;
}


void
ES_IncrementOrDecrementExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (!is_disabled || !in_void_context)
        IntoRegister(compiler, dst, FALSE);
}


BOOL
ES_IncrementOrDecrementExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (!expression->CallVisitor(visitor))
        return FALSE;

    return TRUE;
}


void
ES_IncrementOrDecrementExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst, BOOL is_loop_variable_increment)
{
    if (expression->GetType() == ES_Expression::TYPE_IDENTIFIER)
    {
        if (!is_loop_variable_increment)
            compiler.AssignedVariable(static_cast<ES_IdentifierExpr *>(expression)->GetName());

        ES_Compiler::Register variable(static_cast<ES_IdentifierExpr *>(expression)->AsVariable(compiler, TRUE));

        if (variable.IsValid())
        {
            ES_Compiler::Register variable_comp(static_cast<ES_IdentifierExpr *>(expression)->AsVariable(compiler, FALSE));
            ES_Value_Internal value;
            BOOL is_stored, is_known;

            is_known = compiler.GetLocalValue(value, variable, is_stored);

            if (variable != variable_comp)
            {
                /* The function identifier is ReadOnly so copy it to a temporary first. */
                if (is_known)
                    compiler.StoreRegisterValue(variable, value);
                else
                    compiler.EmitInstruction(ESI_COPY, variable, variable_comp);
            }
            else if (is_known)
            {
                if (value.IsNumber())
                {
                    if (dst.IsValid() && (type == POST_INCREMENT || type == POST_DECREMENT))
                        compiler.StoreRegisterValue(dst, value);

                    value.SetNumber(value.GetNumAsDouble() + (type == PRE_INCREMENT || type == POST_INCREMENT ? 1 : -1));
                    compiler.SetLocalValue(variable, value, FALSE);

                    if (dst.IsValid() && (type == PRE_INCREMENT || type == PRE_DECREMENT))
                        compiler.StoreRegisterValue(dst, value);

                    return;
                }
                else
                    compiler.ResetLocalValue(variable, TRUE);
            }

            if (!in_void_context)
                if (type == POST_INCREMENT || type == POST_DECREMENT)
                {
                    compiler.EmitInstruction(ESI_TONUMBER, dst, variable);
                    if (dst != variable) // skip the INC/DEC in cases like: a = a++
                        compiler.EmitInstruction(GetInstruction(), variable);
                }
                else //(type == PRE_INCREMENT || type == PRE_DECREMENT)
                {
                    compiler.EmitInstruction(GetInstruction(), variable);
                    if (dst != variable) // skip the COPY in cases like: a = ++a
                        compiler.EmitInstruction(ESI_COPY, dst, variable);
                }
            else //in_void_context
                compiler.EmitInstruction(GetInstruction(), variable);

            return;
        }

        JString *name = static_cast<ES_IdentifierExpr *>(expression)->GetName();

        ES_CodeWord::Index scope_index, variable_index;
        BOOL is_lexical, is_lexical_read_only;

        is_lexical = compiler.GetLexical(scope_index, variable_index, name, is_lexical_read_only);

        if (is_lexical || !compiler.HasUnknownScope())
        {
            ES_Compiler::Register rvalue(compiler.Temporary());

            if (is_lexical)
                compiler.EmitInstruction(ESI_GET_LEXICAL, rvalue, scope_index, variable_index);
            else
                compiler.EmitGlobalAccessor(ESI_GET_GLOBAL, name, rvalue);

            if (!in_void_context && (type == POST_INCREMENT || type == POST_DECREMENT))
                compiler.EmitInstruction(ESI_TONUMBER, dst, rvalue);

            compiler.EmitInstruction(GetInstruction(), rvalue);

            if (is_lexical)
            {
                if (!is_lexical_read_only)
                    compiler.EmitInstruction(ESI_PUT_LEXICAL, scope_index, variable_index, rvalue);
                else if (compiler.IsStrictMode())
                    compiler.EmitInstruction(ESI_THROW_BUILTIN, ES_CodeStatic::TYPEERROR_INVALID_ASSIGNMENT);
            }
            else
                compiler.EmitGlobalAccessor(ESI_PUT_GLOBAL, name, rvalue);

            if (!in_void_context && (type == PRE_INCREMENT || type == PRE_DECREMENT))
                compiler.EmitInstruction(ESI_COPY, dst, rvalue);

            return;
        }

        ES_Compiler::Register rvalue(compiler.Temporary()), rbase(compiler.Temporary());

        compiler.EmitScopedAccessor(ESI_GET_SCOPE_REF, name, &rvalue, &rbase);

        if (!in_void_context && (type == POST_INCREMENT || type == POST_DECREMENT))
            compiler.EmitInstruction(ESI_TONUMBER, dst, rvalue);

        compiler.EmitInstruction(GetInstruction(), rvalue);
        compiler.EmitPropertyAccessor(ESI_PUTN_IMM, name, rbase, rvalue);

        if (!in_void_context && (type == PRE_INCREMENT || type == PRE_DECREMENT))
            compiler.EmitInstruction(ESI_COPY, dst, rvalue);
    }
    else if (expression->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE ||
             expression->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
    {
        ES_PropertyAccessorExpr *pae = static_cast<ES_PropertyAccessorExpr *>(expression);

        ES_Compiler::Register rbase(pae->GetBase()->AsRegister(compiler)), rvalue(compiler.Temporary()), rindex;
        pae->SetRegister(rbase);

        if (expression->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE)
            static_cast<ES_ArrayReferenceExpr *>(pae)->GetTo(compiler, rvalue, rindex);
        else
            static_cast<ES_PropertyReferenceExpr *>(pae)->GetTo(compiler, rvalue);

        if (!in_void_context && (type == POST_INCREMENT || type == POST_DECREMENT))
            compiler.EmitInstruction(ESI_TONUMBER, dst, rvalue);

        compiler.EmitInstruction(GetInstruction(), rvalue);

        if (!in_void_context && (type == PRE_INCREMENT || type == PRE_DECREMENT))
            compiler.EmitInstruction(ESI_COPY, dst, rvalue);

        if (expression->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE)
            static_cast<ES_ArrayReferenceExpr *>(pae)->PutFrom(compiler, rvalue, rindex);
        else
            static_cast<ES_PropertyReferenceExpr *>(pae)->PutFrom(compiler, rvalue);
    }
    else
    {   // subject to in/decrement is not an l-value;
        // evaluate expression (for side effects) then throw ReferenceError
        ES_Compiler::Register rvalue(compiler.Temporary());

        compiler.EmitInstruction(ESI_TONUMBER, rvalue, expression->AsRegister(compiler, &rvalue));
        compiler.EmitInstruction(ESI_THROW_BUILTIN, ES_CodeStatic::TYPEERROR_EXPECTED_LVALUE);

        if (dst.IsValid())
            /* We need to write something into the destination register,
               otherwise ES_Analyzer becomes upset and crashes. */
            compiler.EmitInstruction(ESI_LOAD_UNDEFINED, dst);
    }
}


ES_Compiler::Register
ES_IncrementOrDecrementExpr::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    if (in_void_context || type == PRE_INCREMENT || type == PRE_DECREMENT)
        if (expression->GetType() == ES_Expression::TYPE_IDENTIFIER)
        {
            ES_Compiler::Register variable(static_cast<ES_IdentifierExpr *>(expression)->AsVariable(compiler));
            if (variable.IsValid())
            {
                ES_Value_Internal value;
                BOOL is_stored;

                if (compiler.GetLocalValue(value, variable, is_stored))
                    if (value.IsNumber())
                    {
                        value.SetNumber(value.GetNumAsDouble() + (type == PRE_INCREMENT ? 1 : -1));
                        compiler.StoreRegisterValue(variable, value);

                        return variable;
                    }
                    else
                        compiler.ResetLocalValue(variable, TRUE);

                compiler.EmitInstruction(GetInstruction(), variable);

                return variable;
            }
        }

    return ES_Expression::AsRegister(compiler, temporary);
}


ES_Instruction
ES_IncrementOrDecrementExpr::GetInstruction()
{
    if (type == PRE_INCREMENT || type == POST_INCREMENT)
        return ESI_INC;
    else
        return ESI_DEC;
}


void
ES_DeleteExpr::CompileAsCondition(ES_Compiler &compiler, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index)
{
    if (expression->GetType() == ES_Expression::TYPE_IDENTIFIER)
    {
        ES_Compiler::Register variable(static_cast<ES_IdentifierExpr *>(expression)->AsVariable(compiler));
        if (variable.IsValid())
        {
            if (compiler.IsStrictMode())
            {
                compiler.SetError(ES_Parser::INVALID_USE_OF_DELETE, GetSourceLocation());
                return;
            }

            /* Local variables are always DontDelete. */
            compiler.EmitJump(NULL, ESI_JUMP, false_target);
            return;
        }

        JString *name = static_cast<ES_IdentifierExpr *>(expression)->GetName();

        ES_CodeWord::Index scope_index, variable_index;
        BOOL is_lexical_read_only;

        if (compiler.GetLexical(scope_index, variable_index, name, is_lexical_read_only))
        {
            if (compiler.IsStrictMode())
            {
                compiler.SetError(ES_Parser::INVALID_USE_OF_DELETE, GetSourceLocation());
                return;
            }

            /* Local variables in enclosing scopes are also always DontDelete. */
            compiler.EmitJump(NULL, ESI_JUMP, false_target);
            return;
        }

        if (!compiler.HasUnknownScope())
        {
            unsigned gindex;

            if (compiler.GetGlobalIndex(gindex, name))
            {
                /* Global variables are also DontDelete. */
                compiler.EmitJump(NULL, ESI_JUMP, false_target);
                return;
            }
        }

        compiler.EmitScopedAccessor(ESI_DELETE_SCOPE, name, NULL, NULL);
    }
    else if (expression->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE)
        static_cast<ES_ArrayReferenceExpr *>(expression)->Delete(compiler);
    else if (expression->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
        static_cast<ES_PropertyReferenceExpr *>(expression)->Delete(compiler);
    else
    {
        /* For some reason, deleting something that makes no sense to delete
           should return true. */
        compiler.EmitJump(NULL, ESI_JUMP, true_target);
        return;
    }

    EmitConditionalJumps(compiler, NULL, true_target, false_target, prefer_jtrue, loop_index);
}


void
ES_DeleteExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (expression->GetType() == ES_Expression::TYPE_IDENTIFIER)
    {
        ES_Compiler::Register variable(static_cast<ES_IdentifierExpr *>(expression)->AsVariable(compiler));
        if (variable.IsValid())
        {
            if (compiler.IsStrictMode())
            {
                compiler.SetError(ES_Parser::INVALID_USE_OF_DELETE, GetSourceLocation());
                return;
            }

            /* Local variables are always DontDelete. */
            compiler.EmitInstruction(ESI_LOAD_FALSE, dst);
            return;
        }

        JString *name = static_cast<ES_IdentifierExpr *>(expression)->GetName();

        ES_CodeWord::Index scope_index, variable_index;
        BOOL is_lexical_read_only;

        if (compiler.GetLexical(scope_index, variable_index, name, is_lexical_read_only))
        {
            if (compiler.IsStrictMode())
            {
                compiler.SetError(ES_Parser::INVALID_USE_OF_DELETE, GetSourceLocation());
                return;
            }

            /* Local variables in enclosing scopes are also always DontDelete. */
            compiler.EmitInstruction(ESI_LOAD_FALSE, dst);
            return;
        }

        if (!compiler.HasUnknownScope())
        {
            unsigned gindex;

            if (compiler.GetGlobalIndex(gindex, name))
            {
                /* Global variables are also DontDelete. */
                compiler.EmitInstruction(ESI_LOAD_FALSE, dst);
                return;
            }
        }

        compiler.EmitScopedAccessor(ESI_DELETE_SCOPE, name, NULL, NULL);
    }
    else if (expression->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE)
        static_cast<ES_ArrayReferenceExpr *>(expression)->Delete(compiler);
    else if (expression->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
        static_cast<ES_PropertyReferenceExpr *>(expression)->Delete(compiler);
    else if (dst.IsValid())
    {
        expression->AsRegister(compiler, &dst);
        /* For some reason, deleting something that makes no sense to delete
           should return true. */
        compiler.EmitInstruction(ESI_LOAD_TRUE, dst);
        return;
    }

    compiler.EmitInstruction(ESI_STORE_BOOLEAN, dst);
}


BOOL
ES_DeleteExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (!expression->CallVisitor(visitor))
        return FALSE;

    return TRUE;
}


ES_Instruction
ES_UnaryExpr::GetInstruction()
{
    OP_ASSERT(type == ES_Expression::TYPE_MINUS || type == ES_Expression::TYPE_BITWISE_NOT);

    if (type == ES_Expression::TYPE_MINUS)
        return ESI_NEG;
    else
        return ESI_COMPL;
}


void
ES_UnaryExpr::CompileAsCondition(ES_Compiler &compiler, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index)
{
    if (type == TYPE_LOGICAL_NOT)
        expression->CompileAsCondition(compiler, false_target, true_target, !prefer_jtrue, loop_index);
    else
        ES_Expression::CompileAsCondition(compiler, true_target, false_target, prefer_jtrue, loop_index);
}


void
ES_UnaryExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_Instruction instruction = ESI_LAST_INSTRUCTION;
    ES_Value_Internal value;

    if (GetImmediateValue(compiler, value))
    {
        if (dst.IsValid())
            compiler.StoreRegisterValue(dst, value);

        return;
    }

    switch (type)
    {
    case TYPE_VOID:
        expression->CompileInVoidContext(compiler);

        // NOTE: dst will typically be invalid, since void expressions
        // would normally be in 'void contexts'.
        if (dst.IsValid())
            compiler.EmitInstruction(ESI_LOAD_UNDEFINED, dst);
        return;

    case TYPE_TYPEOF:
        {
            instruction = ESI_TYPEOF;

            if (expression->GetImmediateValue(compiler, value))
            {
                if (dst.IsValid())
                    compiler.SetLocalValue(dst, value.TypeString(compiler.GetContext()), FALSE);
                return;
            }
            else if (expression->GetType() == TYPE_IDENTIFIER)
            {
                compiler.EmitInstruction(instruction, dst, static_cast<ES_IdentifierExpr *>(expression)->AsRegisterQuiet(compiler, &dst));
                return;
            }
        }
        break;

    case TYPE_PLUS:
        if (expression->GetValueType() != ESTYPE_DOUBLE)
            instruction = ESI_TONUMBER;
        else
            instruction = ESI_COPY;
        break;

    case TYPE_MINUS:
        if (expression->GetType() == TYPE_LITERAL && static_cast<ES_LiteralExpr *>(expression)->IntoRegisterNegated(compiler, dst))
            return;

        instruction = ESI_NEG;
        break;

    case TYPE_BITWISE_NOT:
        instruction = ESI_COMPL;
        break;

    case TYPE_LOGICAL_NOT:
        instruction = ESI_NOT;
        break;
    }

    ES_Compiler::Register src(expression->AsRegister(compiler, &dst));

    if (instruction != ESI_COPY || src != dst)
        compiler.EmitInstruction(instruction, dst, src);
}


BOOL
ES_UnaryExpr::GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value)
{
    if (expression->GetImmediateValue(compiler, value))
        switch (type)
        {
        case TYPE_PLUS:
        case TYPE_MINUS:
        case TYPE_BITWISE_NOT:
            value = value.AsNumber(compiler.GetContext());

            if (type == TYPE_MINUS)
                value.SetNumber(-value.GetNumAsDouble());
            else if (type == TYPE_BITWISE_NOT)
                value.SetInt32(~value.GetNumAsInt32());

            return TRUE;

        case TYPE_LOGICAL_NOT:
            value.SetBoolean(!value.AsBoolean().GetBoolean());
            return TRUE;
        }

    return FALSE;
}


BOOL
ES_UnaryExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (!expression->CallVisitor(visitor))
        return FALSE;

    return TRUE;
}


void
ES_BinaryExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (is_compound_assign)
        CompileCompoundAssignment(compiler, dst, TRUE);
    else
    {
        ES_Value_Internal value;
        if (GetImmediateValue(compiler, value))
        {
            if (dst.IsValid())
                compiler.StoreRegisterValue(dst, value);
        }
        else
            CompileExpressions(compiler, dst);
    }
}


ES_Compiler::Register
ES_BinaryExpr::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary_in)
{
    ES_Compiler::Register temporary;

    if (temporary_in)
        temporary = *temporary_in;

    if (is_compound_assign)
        return CompileCompoundAssignment(compiler, temporary, FALSE);
    else
    {
        ES_Value_Internal value;
        if (GetImmediateValue(compiler, value))
        {
            if (!temporary.IsValid())
                temporary = compiler.Temporary();

            compiler.StoreRegisterValue(temporary, value);
            return temporary;
        }
        else
            return CompileExpressions(compiler, temporary);
    }
}


BOOL
ES_BinaryExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (!left->CallVisitor(visitor))
        return FALSE;

    if (!right->CallVisitor(visitor))
        return FALSE;

    return TRUE;
}


/*
  x += y

   ADD reg(x), reg(x), reg(y)
     => reg(x)

  o.x += o.y

   GET reg(t1), reg(o), imm("x")
   GET reg(t2), reg(o), imm("y")
   ADD reg(t1), reg(t1), reg(t2)
   PUT reg(o), imm("x"), reg(t1)
     => reg(t1)

  x += o.y

   GET reg(t1), reg(o), imm("y")
   ADD reg(x), reg(x), reg(t1)
     => reg(x)

  o.x += y

   GET reg(t1), reg(o), imm("x")
   ADD reg(t1), reg(t1), reg(y)
   PUT reg(o), imm("x"), reg(t1)
     => reg(t1)

 */


ES_Compiler::Register
ES_BinaryExpr::CompileCompoundAssignment(ES_Compiler &compiler, const ES_Compiler::Register &dst, BOOL into_register)
{
    ES_Compiler::Register rleft, rleftv, rright, rbase, rindex;
    ES_CodeWord::Index scope_index = 0, variable_index = 0;
    JString *name = 0;

    ES_Compiler::Register intermediate = dst;

    enum { CASE_VARIABLE, CASE_LEXICAL, CASE_GLOBAL, CASE_SCOPED, CASE_ARRAY, CASE_PROPERTY, CASE_INVALID, CASE_READ_ONLY } what_case;

    if (left->GetType() == ES_Expression::TYPE_IDENTIFIER)
    {
        name = static_cast<ES_IdentifierExpr *>(left)->GetName();

        compiler.AssignedVariable(name);

        rleft = static_cast<ES_IdentifierExpr *>(left)->AsVariable(compiler, TRUE);

        if (rleft.IsValid())
        {
            what_case = CASE_VARIABLE;

            ES_Value_Internal value;
            if (GetImmediateValue(compiler, value))
            {
                if (in_void_context)
                    compiler.SetLocalValue(rleft, value, FALSE);
                else
                {
                    compiler.StoreRegisterValue(rleft, value);

                    if (into_register && dst != rleft)
                        compiler.EmitInstruction(ESI_COPY, dst, rleft);
                }

                return rleft;
            }

            BOOL is_stored;
            if (compiler.GetLocalValue(value, rleft, is_stored) && !is_stored)
                compiler.StoreRegisterValue(rleft, value);

            ES_IsVariableTrampled checker(compiler, right, name);

            if (checker.is_trampled)
            {
                rleftv = compiler.Temporary();
                compiler.EmitInstruction(ESI_COPY, rleftv, rleft);
            }
            else
                rleftv = rleft;
        }
        else
        {
            rleft = rleftv = compiler.Temporary();

            BOOL is_lexical_read_only;
            if (compiler.GetLexical(scope_index, variable_index, name, is_lexical_read_only))
            {
                compiler.EmitInstruction(ESI_GET_LEXICAL, rleft, scope_index, variable_index);
                what_case = is_lexical_read_only ? CASE_READ_ONLY : CASE_LEXICAL;
            }
            else if (!compiler.HasUnknownScope())
            {
                compiler.EmitGlobalAccessor(ESI_GET_GLOBAL, name, rleft);
                what_case = CASE_GLOBAL;
            }
            else
            {
                rbase = compiler.Temporary();
                compiler.EmitScopedAccessor(ESI_GET_SCOPE_REF, name, &rleft, &rbase);
                what_case = CASE_SCOPED;
            }
        }
    }
    else if (left->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE ||
             left->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE)
    {
        rbase = static_cast<ES_PropertyAccessorExpr *>(left)->BaseAsRegister(compiler);

        if (dst.IsValid() && dst.IsReusable() && rbase != dst)
            rleft = rleftv = dst;
        else
            intermediate = rleft = rleftv = compiler.Temporary();

        if (compiler.IsLocal(rbase))
        {
            JString *name = compiler.GetLocal(rbase);
            ES_IsVariableTrampled checker(compiler, right, name);

            if (checker.is_trampled)
            {
                ES_Compiler::Register tmp(rbase);
                rbase = compiler.Temporary();
                compiler.EmitInstruction(ESI_COPY, rbase, tmp);
                static_cast<ES_PropertyAccessorExpr *>(left)->SetRegister(rbase);
            }
        }
        else
            static_cast<ES_PropertyAccessorExpr *>(left)->SetRegister(rbase);

        if (left->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE)
        {
            if (compiler.IsLocal(rbase))
            {
                JString *name = compiler.GetLocal(rbase);
                ES_IsVariableTrampled checker(compiler, static_cast<ES_ArrayReferenceExpr *>(left)->GetIndex(), name);

                if (checker.is_trampled)
                {
                    ES_Compiler::Register tmp(rbase);
                    rbase = compiler.Temporary();
                    compiler.EmitInstruction(ESI_COPY, rbase, tmp);
                    static_cast<ES_PropertyAccessorExpr *>(left)->SetRegister(rbase);
                }
            }

            static_cast<ES_ArrayReferenceExpr *>(left)->GetTo(compiler, rleft, rindex);
            what_case = CASE_ARRAY;

            if (compiler.IsLocal(rindex))
            {
                JString *name = compiler.GetLocal(rindex);
                ES_IsVariableTrampled checker(compiler, right, name);

                if (checker.is_trampled)
                {
                    ES_Compiler::Register tmp(rindex);
                    rindex = compiler.Temporary();
                    compiler.EmitInstruction(ESI_COPY, rindex, tmp);
                }
            }
        }
        else
        {
            static_cast<ES_PropertyReferenceExpr *>(left)->GetTo(compiler, rleft);
            what_case = CASE_PROPERTY;
        }
    }
    else
    {
        // subject to assignment is not an l-value;
        // evaluate left expression (mostly for side effects) then throw ReferenceError
        rleft = rleftv = dst.IsValid() ? dst : compiler.Temporary();
        left->IntoRegister(compiler, rleft);
        what_case = CASE_INVALID;
    }

    if (!intermediate.IsValid())
        intermediate = rleft;

    BOOL immediate_rhs = FALSE;
    int immediate = 0;

    if (HasImmediateRHS() && right->GetImmediateInteger(compiler, immediate))
        immediate_rhs = TRUE;
    else
        rright = right->AsRegister(compiler);

    if (immediate_rhs)
        compiler.EmitInstruction(static_cast<ES_Instruction>(GetInstruction() + 1), intermediate, rleftv, immediate);
    else
        compiler.EmitInstruction(GetInstruction(), intermediate, rleftv, rright);

    switch (what_case)
    {
    case CASE_VARIABLE:
        /* if LHS was a variable reference, and dst was invalid, we're
           done; no-one cared about the result of the expression, and
           the assignment has been performed already since the
           variable register was used as the destination register. */
        if (intermediate != rleft)
            compiler.EmitInstruction(ESI_COPY, rleft, intermediate);
        compiler.ResetLocalValue(rleft, FALSE);
        break;

    case CASE_LEXICAL:
        compiler.EmitInstruction(ESI_PUT_LEXICAL, scope_index, variable_index, intermediate);
        break;

    case CASE_GLOBAL:
        compiler.EmitGlobalAccessor(ESI_PUT_GLOBAL, name, intermediate);
        break;

    case CASE_SCOPED:
        compiler.EmitPropertyAccessor(ESI_PUTN_IMM, name, rbase, intermediate);
        break;

    case CASE_ARRAY:
        static_cast<ES_ArrayReferenceExpr *>(left)->PutFrom(compiler, intermediate, rindex, &rbase);
        break;

    case CASE_PROPERTY:
        static_cast<ES_PropertyReferenceExpr *>(left)->PutFrom(compiler, intermediate, &rbase);
        break;

    case CASE_INVALID:
        compiler.EmitInstruction(ESI_THROW_BUILTIN, 1);

        if (dst.IsValid())
            /* We need to write something into the destination register,
               otherwise ES_Analyzer becomes upset and crashes. */
            compiler.EmitInstruction(ESI_LOAD_UNDEFINED, dst);

    case CASE_READ_ONLY:
        if (compiler.IsStrictMode())
            compiler.EmitInstruction(ESI_THROW_BUILTIN, ES_CodeStatic::TYPEERROR_INVALID_ASSIGNMENT);
        break;
    }

    if (into_register && dst != intermediate)
        compiler.EmitInstruction(ESI_COPY, dst, intermediate);

    return intermediate;
}

static void
CountTerms(ES_Compiler &compiler, ES_Expression *expr, unsigned &total, unsigned &complex)
{
    switch (expr->GetType())
    {
    default:
        ++complex;
        ++total;
        return;

    case ES_Expression::TYPE_IDENTIFIER:
        if (!static_cast<ES_IdentifierExpr *>(expr)->AsVariable(compiler).IsValid())
            ++complex;
        ++total;
        return;

    case ES_Expression::TYPE_LITERAL:
        return;

    case ES_Expression::TYPE_MINUS:
    case ES_Expression::TYPE_BITWISE_NOT:
        CountTerms(compiler, static_cast<ES_UnaryExpr *>(expr)->GetExpression(), total, complex);
        break;

    case ES_Expression::TYPE_MULTIPLY:
    case ES_Expression::TYPE_DIVIDE:
    case ES_Expression::TYPE_REMAINDER:
    case ES_Expression::TYPE_SUBTRACT:
    case ES_Expression::TYPE_ADD:
    case ES_Expression::TYPE_SHIFT_LEFT:
    case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT:
    case ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT:
        ES_BinaryExpr *binary = static_cast<ES_BinaryExpr *>(expr);
        CountTerms(compiler, binary->GetLeft(), total, complex);
        CountTerms(compiler, binary->GetRight(), total, complex);
    }
}

static void
ConvertToPrimitive(ES_Compiler &compiler, ES_Expression *expr, ES_Compiler::Register &reg, ES_Value_Internal::HintType hint)
{
    if (expr->GetValueType() == ESTYPE_UNDEFINED || expr->GetValueType() == ESTYPE_OBJECT)
    {
        if (reg.IsReusable())
            compiler.EmitInstruction(ESI_TOPRIMITIVE1, reg, hint);
        else
        {
            ES_Compiler::Register target = compiler.Temporary();
            compiler.EmitInstruction(ESI_TOPRIMITIVE, target, reg, hint);
            reg = target;
        }
    }
}

static void
EvaluateTerms(ES_Compiler &compiler, ES_Expression *expr, ES_Compiler::Register *&registers)
{
    switch (expr->GetType())
    {
    default:
        *registers++ = expr->AsRegister(compiler);
        return;

    case ES_Expression::TYPE_LITERAL:
        return;

    case ES_Expression::TYPE_MINUS:
    case ES_Expression::TYPE_BITWISE_NOT:
    {
        ES_Expression *unary = static_cast<ES_UnaryExpr *>(expr)->GetExpression();
        EvaluateTerms(compiler, unary, registers);
        ConvertToPrimitive(compiler, unary, registers[-1], ES_Value_Internal::HintNumber);
        return;
    }

    case ES_Expression::TYPE_MULTIPLY:
    case ES_Expression::TYPE_DIVIDE:
    case ES_Expression::TYPE_REMAINDER:
    case ES_Expression::TYPE_SUBTRACT:
    case ES_Expression::TYPE_ADD:
    case ES_Expression::TYPE_SHIFT_LEFT:
    case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT:
    case ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT:
        ES_BinaryExpr *binary = static_cast<ES_BinaryExpr *>(expr);
        EvaluateTerms(compiler, binary->GetLeft(), registers);
        ES_Compiler::Register &rleft(registers[-1]);
        EvaluateTerms(compiler, binary->GetRight(), registers);
        ES_Compiler::Register &rright(registers[-1]);
        ES_Value_Internal::HintType hint = expr->GetType() == ES_Expression::TYPE_ADD ? ES_Value_Internal::HintNone : ES_Value_Internal::HintNumber;
        ConvertToPrimitive(compiler, binary->GetLeft(), rleft, hint);
        ConvertToPrimitive(compiler, binary->GetRight(), rright, hint);
    }
}

static ES_Compiler::Register
EvaluateExpressions(ES_Compiler &compiler, ES_Expression *expr, ES_Compiler::Register *&registers, const ES_Compiler::Register &dst)
{
    switch (expr->GetType())
    {
    default:
        return *registers++;

    case ES_Expression::TYPE_LITERAL:
        OP_ASSERT(FALSE);
        return dst;

    case ES_Expression::TYPE_MINUS:
    case ES_Expression::TYPE_BITWISE_NOT:
    {
        ES_UnaryExpr *unary = static_cast<ES_UnaryExpr *>(expr);
        ES_Compiler::Register result(dst);

        ES_Compiler::Register r;
        if (unary->GetExpression()->GetType() == ES_Expression::TYPE_LITERAL)
        {
            r = compiler.Temporary();
            unary->GetExpression()->IntoRegister(compiler, r);
        }
        else
        {
            r = EvaluateExpressions(compiler, unary->GetExpression(), registers, ES_Compiler::Register());
            if (!result.IsValid())
                result = r;
        }

        OP_ASSERT(result.IsValid());

        compiler.EmitInstruction(unary->GetInstruction(), result, r);
        return result;
    }

    case ES_Expression::TYPE_MULTIPLY:
    case ES_Expression::TYPE_DIVIDE:
    case ES_Expression::TYPE_REMAINDER:
    case ES_Expression::TYPE_SUBTRACT:
    case ES_Expression::TYPE_ADD:
    case ES_Expression::TYPE_SHIFT_LEFT:
    case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT:
    case ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT:
        ES_BinaryExpr *binary = static_cast<ES_BinaryExpr *>(expr);
        ES_Compiler::Register result(dst);

        ES_Compiler::Register rleft;
        if (binary->GetLeft()->GetType() == ES_Expression::TYPE_LITERAL)
        {
            rleft = compiler.Temporary();
            binary->GetLeft()->IntoRegister(compiler, rleft);
        }
        else
        {
            rleft = EvaluateExpressions(compiler, binary->GetLeft(), registers, ES_Compiler::Register());
            if (!result.IsValid())
                result = rleft;
        }

        ES_Compiler::Register rright;
        if (binary->GetRight()->GetType() == ES_Expression::TYPE_LITERAL)
        {
            rright = compiler.Temporary();
            binary->GetRight()->IntoRegister(compiler, rright);
        }
        else
        {
            rright = EvaluateExpressions(compiler, binary->GetRight(), registers, ES_Compiler::Register());
            if (!result.IsValid())
                result = rright;
        }

        OP_ASSERT(result.IsValid());

        compiler.EmitInstruction(binary->GetInstruction(), result, rleft, rright);
        return result;
    }
}


#ifdef ES_COMBINED_ADD_SUPPORT

static BOOL
IsPrimitiveType(ES_ValueType type)
{
    return type != ESTYPE_OBJECT && type != ESTYPE_UNDEFINED;
}

static void
CompileAddLeftBranchToPrimitive(ES_Compiler &compiler, ES_BinaryExpr *expr, ES_Compiler::Register *regs, unsigned offset)
{
    OP_ASSERT(expr->GetType() == ES_Expression::TYPE_ADD);
    ES_Value_Internal::HintType hint = ES_Value_Internal::HintNone;
    if (offset >= 2)
    {
        OP_ASSERT(expr->GetLeft()->GetType() == ES_Expression::TYPE_ADD);

        CompileAddLeftBranchToPrimitive(compiler, static_cast<ES_BinaryExpr *>(expr->GetLeft()), regs, offset - 1);

        if (IsPrimitiveType(expr->GetRight()->GetValueType()))
            expr->GetRight()->IntoRegister(compiler, regs[offset]);
        else
            compiler.EmitInstruction(ESI_TOPRIMITIVE, regs[offset], expr->GetRight()->AsRegister(compiler, &regs[offset]), hint);
    }
    else if (expr->GetLeft()->GetType() == ES_Expression::TYPE_LITERAL)
    {
        expr->GetLeft()->IntoRegister(compiler, regs[0]);

        if (IsPrimitiveType(expr->GetRight()->GetValueType()))
            expr->GetRight()->IntoRegister(compiler, regs[1]);
        else
            compiler.EmitInstruction(ESI_TOPRIMITIVE, regs[1], expr->GetRight()->AsRegister(compiler, &regs[1]), hint);
    }
    else
    {
        expr->GetLeft()->IntoRegister(compiler, regs[0]);
        expr->GetRight()->IntoRegister(compiler, regs[1]);

        if (!IsPrimitiveType(expr->GetLeft()->GetValueType()))
            compiler.EmitInstruction(ESI_TOPRIMITIVE1, regs[0], hint);
        if (!IsPrimitiveType(expr->GetRight()->GetValueType()))
            compiler.EmitInstruction(ESI_TOPRIMITIVE1, regs[1], hint);
    }
}

#endif // ES_COMBINED_ADD_SUPPORT


ES_Compiler::Register
ES_BinaryExpr::CompileExpressions(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    ES_Compiler::Register result, rleft, rright;
    BOOL implicit_destination = ImplicitDestination();
    BOOL immediate_rhs = FALSE, operands_reversed = FALSE;
    int immediate = 0;

    unsigned terms_total = 0, terms_complex = 0;

    if (GetValueType() != ESTYPE_STRING && !compiler.UsesEval() && !compiler.HasNestedFunctions())
    {
        CountTerms(compiler, this, terms_total, terms_complex);

        if (terms_complex >= 4)
        {
            ES_Compiler::Register *registers = OP_NEWA_L(ES_Compiler::Register, terms_total), *rptr = registers;
            ANCHOR_ARRAY(ES_Compiler::Register, registers);

            EvaluateTerms(compiler, this, rptr);

            OP_ASSERT(rptr == registers + terms_total);

            return EvaluateExpressions(compiler, this, registers, dst);
        }
    }

#ifdef ES_COMBINED_ADD_SUPPORT
    if (GetType() == TYPE_ADD && GetValueType() == ESTYPE_STRING)
    {
        unsigned count = 2;

        ES_Expression *leftbranch = left;
        while (leftbranch->GetType() == TYPE_ADD && leftbranch->GetValueType() == ESTYPE_STRING)
        {
            ++count;
            leftbranch = static_cast<ES_BinaryExpr *>(leftbranch)->left;
        }

        if (count == 2 && (left->IsImmediateString(compiler) || right->IsImmediateString(compiler)) ||
            count == 3 && (left->GetType() == TYPE_ADD && static_cast<ES_BinaryExpr *>(left)->GetLeft()->IsImmediateString(compiler) && right->IsImmediateString(compiler)))
        {
            ES_Compiler::Register result;

            if (dst.IsValid())
                result = dst;
            else
                result = compiler.Temporary();

            unsigned prefix = UINT_MAX, suffix = UINT_MAX;
            ES_Compiler::Register rvalue;

            JString *leftstring;
            if (left->GetImmediateString(compiler, leftstring))
            {
                if (Length(leftstring) != 0)
                    prefix = compiler.String(leftstring);
                rvalue = right->AsRegister(compiler);
            }
            else if (count == 3)
            {
                static_cast<ES_BinaryExpr *>(left)->GetLeft()->GetImmediateString(compiler, leftstring);
                if (Length(leftstring) != 0)
                    prefix = compiler.String(leftstring);
                rvalue = static_cast<ES_BinaryExpr *>(left)->GetRight()->AsRegister(compiler);
            }
            else
                rvalue = left->AsRegister(compiler);

            JString *rightstring;
            if (right->GetImmediateString(compiler, rightstring))
                if (Length(rightstring) != 0)
                    suffix = compiler.String(rightstring);

            /* Note: it's possible for both prefix and suffix to be UINT_MAX, in
               which case ESI_FORMAT_STRING essentially becomes ESI_TOSTRING.
               If we added an ESI_TOSTRING instruction, we could use that here. */

            compiler.EmitInstruction(ESI_FORMAT_STRING, result, rvalue, prefix, suffix, compiler.GetFormatStringCache());
            return result;
        }

        if (left->GetType() == TYPE_ADD && left->GetValueType() == ESTYPE_STRING)
        {
            ES_Compiler::Register result;

            if (dst.IsValid())
                result = dst;
            else
                result = compiler.Temporary();

            ES_Compiler::Register *regs = OP_NEWA_L(ES_Compiler::Register, count);
            ANCHOR_ARRAY(ES_Compiler::Register, regs);

            for (unsigned index = 0; index < count; ++index)
                regs[index] = compiler.Temporary();

            CompileAddLeftBranchToPrimitive(compiler, this, regs, count - 1);

            compiler.EmitADDN(result, count, regs);

            return result;
        }
    }
#endif // ES_COMBINED_ADD_SUPPORT

    if (!left->IsImmediateInteger(compiler))
        rleft = left->AsRegister(compiler, dst.IsTemporary() ? &dst : 0);

    if (HasImmediateRHS())
        if (right->GetImmediateInteger(compiler, immediate))
            immediate_rhs = TRUE;
        else if (ReverseOperands(compiler) && left->GetImmediateInteger(compiler, immediate))
        {
            immediate_rhs = TRUE;
            operands_reversed = TRUE;
        }

    if ((GetType() == TYPE_EQUAL || GetType() == TYPE_NOT_EQUAL) &&
        (left->GetType() == TYPE_LITERAL && static_cast<ES_LiteralExpr *>(left)->GetValue().IsNullOrUndefined() ||
         right->GetType() == TYPE_LITERAL && static_cast<ES_LiteralExpr *>(right)->GetValue().IsNullOrUndefined()))
    {
        ES_Expression *value = (left->GetType() == TYPE_LITERAL && static_cast<ES_LiteralExpr *>(left)->GetValue().IsNullOrUndefined()) ? right : left;
        ES_Instruction instruction = GetType() == TYPE_EQUAL ? ESI_IS_NULL_OR_UNDEFINED : ESI_IS_NOT_NULL_OR_UNDEFINED;

        if (value == left && rleft.IsValid())
            compiler.EmitInstruction(instruction, rleft);
        else
        {
            const ES_Compiler::Register *temporary = (!in_void_context && !in_boolean_context && dst.IsValid()) ? &dst : NULL;
            compiler.EmitInstruction(instruction, value->AsRegister(compiler, temporary));
        }

        if (!in_void_context && !in_boolean_context)
        {
            if (dst.IsValid())
                result = dst;
            else
                result = compiler.Temporary();

            compiler.EmitInstruction(ESI_STORE_BOOLEAN, result);
        }

        return result;
    }

    ES_Expression *first = operands_reversed ? right : left;
    ES_Expression *second = operands_reversed ? left : right;

    if (first->GetType() != TYPE_LITERAL || immediate_rhs || right->GetType() == TYPE_LITERAL)
    {
        if (!rleft.IsValid())
            rleft = first->AsRegister(compiler, dst.IsTemporary() ? &dst : 0);

        if (compiler.IsLocal(rleft))
        {
            JString *name = compiler.GetLocal(rleft);
            ES_IsVariableTrampled checker(compiler, second, name);

            if (checker.is_trampled)
            {
                ES_Compiler::Register rleft2 = compiler.Temporary();
                compiler.EmitInstruction(ESI_COPY, rleft2, rleft);
                rleft = rleft2;
            }
        }

        first = NULL;
    }

    if (!immediate_rhs)
    {
        rright = second->AsRegister(compiler, dst.IsTemporary() && rleft != dst ? &dst : 0);

        if (first && compiler.IsLocal(rright))
        {
            JString *name = compiler.GetLocal(rright);
            ES_IsVariableTrampled checker(compiler, first, name);

            if (checker.is_trampled)
            {
                ES_Compiler::Register rright2 = compiler.Temporary();
                compiler.EmitInstruction(ESI_COPY, rright2, rright);
                rright = rright2;
            }
        }
    }

    if (first)
        rleft = first->AsRegister(compiler, dst.IsTemporary() && rright != dst ? &dst : 0);

    if (dst.IsValid())
        result = dst;
    else if (rleft.IsReusable())
        result = rleft;
    else if (rright.IsReusable())
        result = rright;
    else if ((!in_void_context && !in_boolean_context) || !implicit_destination)
        result = compiler.Temporary();

    ES_Compiler::Register src1, src2;

    if (!ReverseOperands(compiler) || operands_reversed)
    {
        src1 = rleft;
        src2 = rright;
    }
    else
    {
        src1 = rright;
        src2 = rleft;
    }

    if (implicit_destination)
    {
        if (immediate_rhs)
            compiler.EmitInstruction(static_cast<ES_Instruction>(GetInstruction() + 1), src1, immediate);
        else
            compiler.EmitInstruction(GetInstruction(), src1, src2);

        if (!in_void_context && !in_boolean_context)
            compiler.EmitInstruction(ESI_STORE_BOOLEAN, result);
    }
    else
    {
        if (immediate_rhs)
            compiler.EmitInstruction(static_cast<ES_Instruction>(GetInstruction() + 1), result, src1, immediate);
        else
            compiler.EmitInstruction(GetInstruction(), result, src1, src2);

        if (compiler.IsLocal(result))
            compiler.ResetLocalValue(result, FALSE);
    }

    return result;
}


ES_Instruction
ES_BinaryExpr::GetInstruction()
{
    switch (type)
    {
    case ES_Expression::TYPE_MULTIPLY: return ESI_MUL;
    case ES_Expression::TYPE_DIVIDE: return ESI_DIV;
    case ES_Expression::TYPE_REMAINDER: return ESI_REM;
    case ES_Expression::TYPE_SUBTRACT: return ESI_SUB;
    case ES_Expression::TYPE_ADD: return ESI_ADD;
    case ES_Expression::TYPE_SHIFT_LEFT: return ESI_LSHIFT;
    case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT: return ESI_RSHIFT_SIGNED;
    case ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT: return ESI_RSHIFT_UNSIGNED;
    case ES_Expression::TYPE_LESS_THAN: return ESI_LT;
    case ES_Expression::TYPE_GREATER_THAN: return ESI_GT;
    case ES_Expression::TYPE_LESS_THAN_OR_EQUAL: return ESI_LTE;
    case ES_Expression::TYPE_GREATER_THAN_OR_EQUAL: return ESI_GTE;
    case ES_Expression::TYPE_INSTANCEOF: return ESI_INSTANCEOF;
    case ES_Expression::TYPE_IN: return ESI_HASPROPERTY;
    case ES_Expression::TYPE_EQUAL: return ESI_EQ;
    case ES_Expression::TYPE_NOT_EQUAL: return ESI_NEQ;
    case ES_Expression::TYPE_STRICT_EQUAL: return ESI_EQ_STRICT;
    case ES_Expression::TYPE_STRICT_NOT_EQUAL: return ESI_NEQ_STRICT;
    case ES_Expression::TYPE_BITWISE_AND: return ESI_BITAND;
    case ES_Expression::TYPE_BITWISE_OR: return ESI_BITOR;
    default:
    case ES_Expression::TYPE_BITWISE_XOR: return ESI_BITXOR;
    }
}


BOOL
ES_BinaryExpr::GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value)
{
    switch (type)
    {
    case ES_Expression::TYPE_LESS_THAN:
    case ES_Expression::TYPE_GREATER_THAN:
    case ES_Expression::TYPE_LESS_THAN_OR_EQUAL:
    case ES_Expression::TYPE_GREATER_THAN_OR_EQUAL:
    case ES_Expression::TYPE_EQUAL:
    case ES_Expression::TYPE_NOT_EQUAL:
    case ES_Expression::TYPE_STRICT_EQUAL:
    case ES_Expression::TYPE_STRICT_NOT_EQUAL:
        BOOL result;
        if (IsKnownCondition(compiler, result))
        {
            value.SetBoolean(result);
            return TRUE;
        }
        else
            return FALSE;
    }

    ES_Value_Internal lhs, rhs;

    if (!left->GetImmediateValue(compiler, lhs) || !right->GetImmediateValue(compiler, rhs))
        return FALSE;

    if (type == ES_Expression::TYPE_ADD && (lhs.IsString() || rhs.IsString()))
    {
        JString *lhs_string = lhs.AsString(compiler.GetContext()).GetString();
        JString *rhs_string = rhs.AsString(compiler.GetContext()).GetString();
        JString *result;

        if (Length(lhs_string) == 0)
            result = rhs_string;
        else if (Length(rhs_string) == 0)
            result = lhs_string;
        else
        {
            result = JString::Make(compiler.GetContext(), Length(lhs_string) + Length(rhs_string));

            op_memcpy(Storage(NULL, result), Storage(compiler.GetContext(), lhs_string), Length(lhs_string) * 2);
            op_memcpy(Storage(NULL, result) + Length(lhs_string), Storage(compiler.GetContext(), rhs_string), Length(rhs_string) * 2);
        }

        value.SetString(result);
        return TRUE;
    }

    lhs = lhs.AsNumber(compiler.GetContext());
    rhs = rhs.AsNumber(compiler.GetContext());

    int lhsi = lhs.GetNumAsInt32(), rhsi = rhs.GetNumAsInt32();
    double lhsd = lhs.GetNumAsDouble(), rhsd = rhs.GetNumAsDouble();

    switch (type)
    {
    case ES_Expression::TYPE_MULTIPLY:  value.SetNumber(lhsd * rhsd); return TRUE;
    case ES_Expression::TYPE_DIVIDE:    value.SetNumber(lhsd / rhsd); return TRUE;
    case ES_Expression::TYPE_SUBTRACT:  value.SetNumber(lhsd - rhsd); return TRUE;
    case ES_Expression::TYPE_ADD:       value.SetNumber(lhsd + rhsd); return TRUE;

    case ES_Expression::TYPE_REMAINDER:
        if (op_isnan(lhsd) || op_isnan(rhsd) || op_isinf(lhsd) || rhsd == 0)
            value.SetDouble(op_nan(NULL));
        else if (op_isinf(rhsd) || lhsd == 0)
            value.SetNumber(lhsd);
        else
            value.SetNumber(op_fmod(lhsd, rhsd));
        return TRUE;

    case ES_Expression::TYPE_SHIFT_LEFT:         value.SetInt32(lhsi << (rhsi & 0x1f)); return TRUE;
    case ES_Expression::TYPE_SHIFT_SIGNED_RIGHT: value.SetInt32(lhsi >> (rhsi & 0x1f)); return TRUE;
    case ES_Expression::TYPE_BITWISE_AND:        value.SetInt32(lhsi & rhsi); return TRUE;
    case ES_Expression::TYPE_BITWISE_OR:         value.SetInt32(lhsi | rhsi); return TRUE;
    case ES_Expression::TYPE_BITWISE_XOR:        value.SetInt32(lhsi ^ rhsi); return TRUE;

    case ES_Expression::TYPE_SHIFT_UNSIGNED_RIGHT:
        value.SetUInt32(static_cast<unsigned>(lhsi) >> (rhsi & 0x1f));
        return TRUE;

    default:
        return FALSE;
    }
}


BOOL
ES_BinaryExpr::ReverseOperands(ES_Compiler &compiler)
{
    switch (type)
    {
    case ES_Expression::TYPE_MULTIPLY:
    case ES_Expression::TYPE_EQUAL:
    case ES_Expression::TYPE_NOT_EQUAL:
    case ES_Expression::TYPE_STRICT_EQUAL:
    case ES_Expression::TYPE_STRICT_NOT_EQUAL:
    case ES_Expression::TYPE_BITWISE_AND:
    case ES_Expression::TYPE_BITWISE_XOR:
    case ES_Expression::TYPE_BITWISE_OR:
        return left->IsImmediateInteger(compiler) && !right->IsImmediateInteger(compiler);

    default:
        return type == ES_Expression::TYPE_IN;
    }
}


BOOL
ES_BinaryExpr::ImplicitDestination()
{
    return type >= ES_Expression::TYPE_LESS_THAN && type <= ES_Expression::TYPE_STRICT_NOT_EQUAL;
}


BOOL
ES_BinaryExpr::HasImmediateRHS()
{
    return type >= TYPE_MULTIPLY && type <= TYPE_GREATER_THAN_OR_EQUAL || type >= TYPE_EQUAL && type <= TYPE_BITWISE_OR;
}


void
ES_BooleanBinaryExpr::CompileAsCondition(ES_Compiler &compiler, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index)
{
    SetInBooleanContext();

    BOOL known_result;

    if (IsKnownCondition(compiler, known_result))
        if (known_result)
            compiler.EmitJump(NULL, ESI_JUMP, true_target);
        else
            compiler.EmitJump(NULL, ESI_JUMP, false_target);
    else
    {
        CompileExpressions(compiler, compiler.Invalid());
        EmitConditionalJumps(compiler, NULL, true_target, false_target, prefer_jtrue, loop_index);
    }
}


BOOL
ES_BooleanBinaryExpr::IsKnownCondition(ES_Compiler &compiler, BOOL &result)
{
    ES_Value_Internal lhs, rhs;

    if (left->GetImmediateValue(compiler, lhs) && right->GetImmediateValue(compiler, rhs))
    {
        ES_Context *context = compiler.GetContext();
        double lhsd = lhs.AsNumber(context).GetNumAsDouble();
        double rhsd = rhs.AsNumber(context).GetNumAsDouble();

        switch (type)
        {
        default:
            return FALSE;

        case TYPE_STRICT_EQUAL:
        case TYPE_STRICT_NOT_EQUAL:
            if (lhs.Type() != rhs.Type() && !(lhs.IsNumber() && rhs.IsNumber()))
                result = FALSE;
            else
            {
            case TYPE_EQUAL:
            case TYPE_NOT_EQUAL:
                if (lhs.IsNullOrUndefined() && rhs.IsNullOrUndefined())
                    result = TRUE;
                else if (lhs.IsString() && rhs.IsString())
                    result = Equals(lhs.GetString(), rhs.GetString());
                else if (!lhs.IsNullOrUndefined() && !rhs.IsNullOrUndefined())
                    result = EqDouble(lhsd, rhsd);
                else
                    result = FALSE;
            }

            if (type == TYPE_NOT_EQUAL || type == TYPE_STRICT_NOT_EQUAL)
                result = !result;

            break;

        case TYPE_LESS_THAN:
        case TYPE_GREATER_THAN:
        case TYPE_LESS_THAN_OR_EQUAL:
        case TYPE_GREATER_THAN_OR_EQUAL:
            if (lhs.IsString() && rhs.IsString())
            {
                lhsd = Compare(compiler.GetContext(), lhs.GetString(), rhs.GetString());
                rhsd = 0;
            }

            if (type == TYPE_LESS_THAN)
                result = lhsd < rhsd;
            else if (type == TYPE_GREATER_THAN)
                result = lhsd > rhsd;
            else if (type == TYPE_LESS_THAN_OR_EQUAL)
                result = lhsd <= rhsd;
            else
                result = lhsd >= rhsd;
        }

        return TRUE;
    }
    else
        return FALSE;
}


void
ES_LogicalExpr::CompileAsCondition(ES_Compiler &compiler, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index)
{
    // OPTIMIZE: for cases where operands are simple and shortcut
    // semantics don't matter, it ought to be possible to produce
    // fewer instructions and a single conditional jump here.

    ES_ResetLocalValues reset_local_values(compiler);

    left->CallVisitor(&reset_local_values);
    right->CallVisitor(&reset_local_values);

    left->SetInBooleanContext();
    right->SetInBooleanContext();

    ES_Compiler::JumpTarget left_true_target, left_false_target, evaluate_right = compiler.ForwardJump();
    BOOL left_prefer_jtrue;

    if (type == ES_Expression::TYPE_LOGICAL_AND)
    {
        left_true_target = evaluate_right;
        left_false_target = false_target;
        left_prefer_jtrue = FALSE;
    }
    else
    {
        left_true_target = true_target;
        left_false_target = evaluate_right;
        left_prefer_jtrue = TRUE;
    }

    left->CompileAsCondition(compiler, left_true_target, left_false_target, left_prefer_jtrue, loop_index);
    unsigned snapshot_after_first = compiler.GetLocalValuesSnapshot();

    compiler.SetJumpTarget(evaluate_right);

    right->CompileAsCondition(compiler, true_target, false_target, prefer_jtrue, loop_index);
    compiler.ResetLocalValues(snapshot_after_first);
}


static unsigned
ComputeLogicalDepth(ES_LogicalExpr *expr)
{
    unsigned depth = 0;

    while (expr->GetLeft()->GetType() == expr->GetType())
    {
        ++depth;
        expr = static_cast<ES_LogicalExpr *>(expr->GetLeft());
    }

    return depth;
}


static void
CompileLogicalExpr(ES_Compiler &compiler, ES_LogicalExpr *expr, const ES_Compiler::Register &dst, ES_Instruction jump_instruction, const ES_Compiler::JumpTarget &jump_target, ES_LogicalExpr **stack)
{
    ES_Expression::Type type = expr->GetType();
    unsigned depth = 0;

    while (expr->GetLeft()->GetType() == type)
    {
        OP_ASSERT(stack);
        stack[depth++] = expr;
        expr = static_cast<ES_LogicalExpr *>(expr->GetLeft());
    }

    BOOL first = TRUE;
    unsigned snapshot = compiler.GetLocalValuesSnapshot(), snapshot_after_first = 0;

    while (TRUE)
    {
        ES_Expression *do_expr = first ? expr->GetLeft() : expr->GetRight();

        if (do_expr->ImplicitBooleanResult())
        {
            if (dst.IsValid())
                do_expr->IntoRegister(compiler, dst);
            else
                do_expr->CompileInVoidContext(compiler);

            if (first)
                snapshot_after_first = compiler.GetLocalValuesSnapshot();

            if (first || depth != 0)
            {
                compiler.FlushLocalValues(snapshot);
                snapshot = compiler.GetLocalValuesSnapshot();

                BOOL result;
                if (do_expr->IsKnownCondition(compiler, result))
                {
                    if (jump_instruction == (result ? ESI_JUMP_IF_TRUE : ESI_JUMP_IF_FALSE))
                    {
                        compiler.EmitJump(NULL, ESI_JUMP, jump_target);
                        return;
                    }
                }
                else
                    compiler.EmitJump(NULL, jump_instruction, jump_target, UINT_MAX);
            }
        }
        else
        {
            ES_Compiler::Register use_dst;

            if (dst.IsValid())
            {
                do_expr->IntoRegister(compiler, dst);
                use_dst = dst;
            }
            else if (first || depth != 0)
                use_dst = do_expr->AsRegister(compiler);
            else
                do_expr->CompileInVoidContext(compiler);

            if (first)
                snapshot_after_first = compiler.GetLocalValuesSnapshot();

            if (first || depth != 0)
            {
                compiler.FlushLocalValues(snapshot);
                snapshot = compiler.GetLocalValuesSnapshot();

                compiler.EmitJump(&use_dst, jump_instruction, jump_target, UINT_MAX);
            }
        }

        if (first)
            first = FALSE;
        else if (depth == 0)
            break;
        else
            expr = stack[--depth];
    }

    compiler.ResetLocalValues(snapshot_after_first);
}


/* virtual */ void
ES_LogicalExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    // OPTIMIZE: for cases where operands are simple and shortcut
    // semantics don't matter, it ought to be possible to produce
    // fewer instructions and a single conditional jump here.

    ES_ResetLocalValues reset_local_values(compiler);

    left->CallVisitor(&reset_local_values);
    right->CallVisitor(&reset_local_values);

    ES_Compiler::Register use_dst;

    if (!dst.IsValid() || dst.IsReusable())
        use_dst = dst;
    else
    {
        use_dst = compiler.Temporary();
        use_dst.SetIsReusable();
    }

    ES_Compiler::JumpTarget jump_target(compiler.ForwardJump());
    unsigned depth = ComputeLogicalDepth(this);
    ES_LogicalExpr **stack = depth ? OP_NEWGROA_L(ES_LogicalExpr *, depth, compiler.Arena()) : NULL;
    ES_Instruction jump_instruction;

    if (type == ES_Expression::TYPE_LOGICAL_AND)
        jump_instruction = ESI_JUMP_IF_FALSE;
    else
        jump_instruction = ESI_JUMP_IF_TRUE;

    CompileLogicalExpr(compiler, this, use_dst, jump_instruction, jump_target, stack);

    compiler.SetJumpTarget(jump_target);

    if (dst != use_dst)
        compiler.EmitInstruction(ESI_COPY, dst, use_dst);
}


BOOL
ES_LogicalExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (!left->CallVisitor(visitor))
        return FALSE;

    if (!right->CallVisitor(visitor))
        return FALSE;

    return TRUE;
}


void
ES_ConditionalExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    BOOL known_result;
    if (condition->IsKnownCondition(compiler, known_result))
    {
        ES_Expression *use = known_result ? first : second;

        if (in_void_context)
            use->CompileInVoidContext(compiler);
        else
            use->IntoRegister(compiler, dst);
    }
    else
    {
        ES_ResetLocalValues reset_local_values(compiler);

        first->CallVisitor(&reset_local_values);
        reset_local_values.SetFlushIfRegisterUsed(&dst);
        second->CallVisitor(&reset_local_values);

        ES_Compiler::JumpTarget true_target(compiler.ForwardJump());
        ES_Compiler::JumpTarget false_target(compiler.ForwardJump());
        ES_Compiler::JumpTarget end_target(compiler.ForwardJump());

        condition->CompileAsCondition(compiler, true_target, false_target, FALSE);

        unsigned snapshot_after_condition = compiler.GetLocalValuesSnapshot();

        compiler.SetJumpTarget(true_target);

        in_void_context ? first->CompileInVoidContext(compiler) : first->IntoRegister(compiler, dst);

        compiler.ResetLocalValues(snapshot_after_condition);
        compiler.EmitJump(NULL, ESI_JUMP, end_target);
        compiler.SetJumpTarget(false_target);

        in_void_context ? second->CompileInVoidContext(compiler) : second->IntoRegister(compiler, dst);

        compiler.ResetLocalValues(snapshot_after_condition);
        compiler.SetJumpTarget(end_target);
    }
}


BOOL
ES_ConditionalExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (!condition->CallVisitor(visitor))
        return FALSE;

    if (!first->CallVisitor(visitor))
        return FALSE;

    if (!second->CallVisitor(visitor))
        return FALSE;

    return TRUE;
}


void
ES_AssignExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    if (dst.IsValid())
        Generate(compiler, &dst, TRUE);
    else if (!is_disabled)
        Generate(compiler, NULL, FALSE);
}


ES_Compiler::Register
ES_AssignExpr::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    return Generate(compiler, temporary, FALSE);
}


ES_Compiler::Register
ES_AssignExpr::Generate(ES_Compiler &compiler, const ES_Compiler::Register *dst, BOOL into_register)
{
    ES_Expression *actual_target = target ? target : static_cast<ES_BinaryExpr *>(source)->GetLeft();

    if (!actual_target->IsLValue())
    {
        ES_Compiler::Register temporary(dst && dst->IsTemporary() ? *dst : compiler.Temporary());

        if (target)
        {
            target->SetInVoidContext();
            target->IntoRegister(compiler, temporary);
        }

        source->SetInVoidContext();
        source->IntoRegister(compiler, temporary);

        compiler.AddDebugRecord(ES_CodeStatic::DebugRecord::TYPE_BASE_EXPRESSION, actual_target->GetSourceLocation());

        compiler.EmitInstruction(ESI_THROW_BUILTIN, 1);

        /* What we return is not significant, since the code is never reached,
           but if we return a temporary register that we didn't write anything
           into (that is, an undefined value) then ES_Analyzer becomes upset and
           crashes. */
        compiler.EmitInstruction(ESI_LOAD_UNDEFINED, temporary);
        return temporary;
    }

    if (target)
    {
        OP_ASSERT(target->IsLValue());

        if (actual_target->GetType() == ES_Expression::TYPE_IDENTIFIER)
        {
            compiler.AssignedVariable(static_cast<ES_IdentifierExpr *>(actual_target)->GetName());

            ES_Compiler::Register variable(static_cast<ES_IdentifierExpr *>(actual_target)->AsVariable(compiler, TRUE));

            if (variable.IsValid())
            {
                ES_Value_Internal value;
                BOOL known_value;

                if (source->GetImmediateValue(compiler, value))
                    known_value = TRUE;
                else
                {
                    source->IntoRegister(compiler, variable);
                    compiler.ResetLocalValue(variable, FALSE);
                    known_value = FALSE;
                }

                if (known_value)
                    compiler.SetLocalValue(variable, value, FALSE);

                if (into_register && *dst != variable)
                {
                    if (known_value)
                        compiler.StoreRegisterValue(*dst, value);
                    else
                        compiler.EmitInstruction(ESI_COPY, *dst, variable);

                    return *dst;
                }

                if (!in_void_context)
                    compiler.FlushLocalValue(variable);

                return variable;
            }
            else
            {
                /* If 'dst' is specified (valid) then suggest to 'source' to use
                   that as a temporary register for its result.  'Source' might
                   still return a different register, in which case we need to copy
                   into 'dst', if 'dst' is specified. */

                ES_Compiler::Register rsource(source->AsRegister(compiler, dst ? dst : NULL));
                JString *name = static_cast<ES_IdentifierExpr *>(actual_target)->GetName();

                ES_CodeWord::Index scope_index, variable_index;
                BOOL is_lexical_read_only;

                if (compiler.GetLexical(scope_index, variable_index, name, is_lexical_read_only))
                {
                    if (!is_lexical_read_only)
                        compiler.EmitInstruction(ESI_PUT_LEXICAL, scope_index, variable_index, rsource);
                    else if (compiler.IsStrictMode())
                        compiler.EmitInstruction(ESI_THROW_BUILTIN, ES_CodeStatic::TYPEERROR_INVALID_ASSIGNMENT);
                }
                else if (!compiler.HasUnknownScope())
                    compiler.EmitGlobalAccessor(ESI_PUT_GLOBAL, name, rsource);
                else
                    compiler.EmitScopedAccessor(ESI_PUT_SCOPE, name, &rsource, NULL);

                if (into_register && rsource != *dst)
                {
                    compiler.EmitInstruction(ESI_COPY, *dst, rsource);
                    return *dst;
                }

                return rsource;
            }
        }
        else
        {
            OP_ASSERT(actual_target->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE || actual_target->GetType() == ES_Expression::TYPE_PROPERTY_REFERENCE);

            BOOL previous_want_object = compiler.PushWantObject();
            ES_Compiler::Register rbase(static_cast<ES_PropertyAccessorExpr *>(actual_target)->BaseAsRegister(compiler));
            compiler.PopWantObject(previous_want_object);

            ES_Compiler::Register rindex;

            if (actual_target->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE)
            {
                if (compiler.IsLocal(rbase))
                {
                    JString *name = compiler.GetLocal(rbase);
                    ES_IsVariableTrampled checker(compiler, static_cast<ES_ArrayReferenceExpr *>(actual_target)->GetIndex(), name);

                    if (checker.is_trampled || into_register && *dst == rbase)
                    {
                        ES_Compiler::Register tmp(rbase);
                        rbase = compiler.Temporary();
                        compiler.EmitInstruction(ESI_COPY, rbase, tmp);
                    }
                    else if (dst && *dst == rbase)
                        dst = NULL;
                }

                static_cast<ES_ArrayReferenceExpr *>(actual_target)->EvaluateIndex(compiler, rindex);

                if (compiler.IsLocal(rindex))
                {
                    JString *name = compiler.GetLocal(rindex);
                    ES_IsVariableTrampled checker(compiler, source, name);

                    if (checker.is_trampled || into_register && *dst == rindex)
                    {
                        ES_Compiler::Register rvalue(rindex);
                        rindex = compiler.Temporary();
                        compiler.EmitInstruction(ESI_COPY, rindex, rvalue);
                    }
                    else if (dst && *dst == rindex)
                        dst = NULL;
                }
            }

            if (compiler.IsLocal(rbase))
            {
                JString *name = compiler.GetLocal(rbase);
                ES_IsVariableTrampled checker(compiler, source, name);

                if (checker.is_trampled || into_register && *dst == rbase)
                {
                    ES_Compiler::Register tmp(rbase);
                    rbase = compiler.Temporary();
                    compiler.EmitInstruction(ESI_COPY, rbase, tmp);
                }
                else if (dst && *dst == rbase)
                    dst = NULL;
            }

            ES_Compiler::Register rsource(source->AsRegister(compiler, dst && *dst != rbase ? dst : NULL));

            if (actual_target->GetType() == ES_Expression::TYPE_ARRAY_REFERENCE)
                static_cast<ES_ArrayReferenceExpr *>(actual_target)->PutFrom(compiler, rsource, rindex, &rbase);
            else
                static_cast<ES_PropertyReferenceExpr *>(actual_target)->PutFrom(compiler, rsource, &rbase);

            if (into_register && rsource != *dst)
            {
                compiler.EmitInstruction(ESI_COPY, *dst, rsource);
                return *dst;
            }

            return rsource;
        }
    }
    else if (into_register)
    {
        source->IntoRegister(compiler, *dst);
        return *dst;
    }
    else
    {
        if (in_void_context)
            source->SetInVoidContext();

        return source->AsRegister(compiler, dst);
    }
}


BOOL
ES_AssignExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    if (target && !target->CallVisitor(visitor))
        return FALSE;

    if (!source->CallVisitor(visitor))
        return FALSE;

    return TRUE;
}


void
ES_CommaExpr::CompileAsCondition(ES_Compiler &compiler, const ES_Compiler::JumpTarget &true_target, const ES_Compiler::JumpTarget &false_target, BOOL prefer_jtrue, unsigned loop_index)
{
    BOOL result;
    if (IsKnownCondition(compiler, result))
        if (result)
            compiler.EmitJump(NULL, ESI_JUMP, true_target);
        else
            compiler.EmitJump(NULL, ESI_JUMP, false_target);
    else
    {
        unsigned snapshot = compiler.GetLocalValuesSnapshot();

        for (unsigned index = 0; index < expressions_count - 1; ++index)
            expressions[index]->CompileInVoidContext(compiler);

        /* Flush the local values on the left before compiling the
           condition on the right (and taking a jump.) */
        compiler.FlushLocalValues(snapshot);

        GetLastExpression()->CompileAsCondition(compiler, true_target, false_target, prefer_jtrue, loop_index);
    }
}


void
ES_CommaExpr::IntoRegister(ES_Compiler &compiler, const ES_Compiler::Register &dst)
{
    for (unsigned index = 0; index < expressions_count - 1; ++index)
        expressions[index]->CompileInVoidContext(compiler);

    if (in_void_context)
        GetLastExpression()->CompileInVoidContext(compiler);
    else
        GetLastExpression()->IntoRegister(compiler, dst);
}


ES_Compiler::Register
ES_CommaExpr::AsRegister(ES_Compiler &compiler, const ES_Compiler::Register *temporary)
{
    for (unsigned index = 0; index < expressions_count - 1; ++index)
        expressions[index]->CompileInVoidContext(compiler);

    return GetLastExpression()->AsRegister(compiler, temporary);
}


BOOL
ES_CommaExpr::CallVisitor(ES_ExpressionVisitor *visitor)
{
    if (!visitor->Visit(this))
        return FALSE;

    for (unsigned index = 0; index < expressions_count; ++index)
        if (!expressions[index]->CallVisitor(visitor))
            return FALSE;

    return TRUE;
}


BOOL
ES_CommaExpr::GetImmediateValue(ES_Compiler &compiler, ES_Value_Internal &value)
{
    for (unsigned index = 0; index < expressions_count - 1; ++index)
        if (!expressions[index]->IsImmediate(compiler))
            return FALSE;

    return GetLastExpression()->GetImmediateValue(compiler, value);
}
