/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2010
 *
 * WebGL GLSL compiler -- validating the shaders.
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_context.h"
#include "modules/webgl/src/wgl_string.h"
#include "modules/webgl/src/wgl_error.h"
#include "modules/webgl/src/wgl_validate.h"
#include "modules/webgl/src/wgl_builder.h"

/* static */ void
WGL_ValidateShader::MakeL(WGL_ValidateShader *&validator, WGL_Context *context, WGL_Printer *printer)
{
    validator = OP_NEWGRO_L(WGL_ValidateShader, (), context->Arena());

    validator->printer = printer;
    validator->context = context;
}

BOOL
WGL_ValidateShader::ValidateL(WGL_DeclList *decls, WGL_Validator::Configuration &config)
{
    context->ResetL(config.for_vertex);
    context->Initialise(&config);
    this->for_vertex = config.for_vertex;

    VisitDeclList(decls);

    BOOL have_main = FALSE;
    for (WGL_FunctionData *fun = context->GetValidationState().GetFirstFunction(); fun; fun = fun->Suc())
    {
        if (fun->name->Equals(UNI_L("main")))
        {
            have_main = TRUE;
            break;
        }

    }
    if (!have_main)
        context->AddError(WGL_Error::MISSING_MAIN_FUNCTION, UNI_L(""));

    if (!config.for_vertex)
    {
        /* Perform 'compile-time' checks of the fragment shader:
         *  - did it have a precision declaration/specifier.
         *  - exceeded uniform / varying usage by limits that cannot
         *    be supported by later stages. */

        /* There is no requirement for there to be a defaulting 'precision' declaration (but often unavoidable.) */
#if 0
        WGL_BindingScope<WGL_Precision> *global_prec = context->GetValidationState().GetPrecisionScope();
        BOOL prec_ok = global_prec && global_prec->list && !global_prec->list->Empty();
        if (prec_ok)
        {
            prec_ok = FALSE;
            for (WGL_Precision *p = global_prec->list->First(); p; p = p->Suc())
                if (WGL_Type::HasBasicType(p->type, WGL_BasicType::Float))
                {
                    prec_ok = TRUE;
                    break;
                }
        }
        if (!prec_ok)
            context->AddError(WGL_Error::MISSING_PRECISION_DECL, UNI_L(""));
#endif
    }

    return !HaveErrors();
}

WGL_VarName *
WGL_ValidateShader::ValidateIdentifier(WGL_VarName *i, BOOL is_use, BOOL force_alias, BOOL is_program_global_id)
{
    const uni_char *name = i->value;
    if (uni_strncmp(name, "webgl_", 6) == 0 && !context->IsUnique(i))
        context->AddError(WGL_Error::ILLEGAL_NAME, Storage(context, i));
    else if (uni_strncmp(name, "_webgl_", 7) == 0)
        context->AddError(WGL_Error::ILLEGAL_NAME, Storage(context, i));

    /* A GLSL restriction; inherited. Attempts to read gl_XYZ will be caught as out-of-scope accesses. */
    if (!is_use && uni_strncmp(name, "gl_", 3) == 0)
        context->AddError(WGL_Error::ILLEGAL_NAME, Storage(context, i));

    if (!is_use && context->GetConfiguration() && !WGL_Validator::IsGL(context->GetConfiguration()->output_format) && WGL_HLSLBuiltins::IsHLSLKeyword(name))
        force_alias = TRUE;

    unsigned len = WGL_String::Length(i);
    if (len > WGL_MAX_IDENTIFIER_LENGTH)
        context->AddError(WGL_Error::OVERLONG_IDENTIFIER, Storage(context, i));
    /* Potentially troublesome for the underlying compiler to handle;
       replace with our own unique. */
    else if (force_alias || len > 100)
    {
        WGL_VarName *unique = is_program_global_id ? context->NewUniqueHashVar(i) : context->NewUniqueVar(FALSE);
        if (unique)
        {
            context->GetValidationState().AddAlias(i, unique);
            return unique;
        }
    }
    return i;
}

WGL_VarName *
WGL_ValidateShader::ValidateVar(WGL_VarName *var, BOOL is_local_only)
{
    WGL_VarName *alias_var = var;
    WGL_Type *type = context->GetValidationState().LookupVarType(var, &alias_var, is_local_only);
    WGL_FunctionData *fun = NULL;
    if (!type)
        fun = context->GetValidationState().LookupFunction(var);
    if (!type && !fun && !context->GetValidationState().LookupBuiltin(var))
        context->AddError(WGL_Error::UNKNOWN_VAR, Storage(context, var));

    if (is_local_only && type)
        context->AddError(WGL_Error::DUPLICATE_NAME, Storage(context, alias_var));

    /* Record use of a uniform. Not relevant to 'local only', simply avoid. */
    if (!is_local_only)
    {
        context->GetValidationState().LookupAttribute(alias_var, TRUE);
        context->GetValidationState().LookupUniform(alias_var, TRUE);
        if (!IsVertexShader())
            context->GetValidationState().LookupVarying(alias_var, TRUE);
    }
    return alias_var;
}

WGL_VarBinding *
WGL_ValidateShader::ValidateVarDefn(WGL_VarName *var, WGL_Type *var_type, BOOL is_read_only)
{
    WGL_VarName *alias_var = var;
    if (context->GetValidationState().LookupVarType(var, &alias_var, TRUE))
    {
        context->AddError(WGL_Error::DUPLICATE_NAME, Storage(context, var));
        return NULL;
    }

    BOOL is_uniform_varying = FALSE;
    if (var_type && var_type->type_qualifier &&
        (var_type->type_qualifier->storage == WGL_TypeQualifier::Uniform ||
         var_type->type_qualifier->storage == WGL_TypeQualifier::Varying))
        is_uniform_varying = TRUE;

    alias_var = ValidateIdentifier(var, FALSE, FALSE, is_uniform_varying);
    if (var_type)
    {
        unsigned type_mask = WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float;
        BOOL is_basic = WGL_Type::HasBasicType(var_type, type_mask);
        BOOL is_vector = !is_basic && WGL_Type::IsVectorType(var_type, type_mask);
        BOOL is_matrix = !is_basic && !is_vector && WGL_Type::IsMatrixType(var_type, type_mask);
        if (var_type->precision == WGL_Type::NoPrecision && (var_type->GetType() == WGL_Type::Sampler || is_basic || is_vector || is_matrix))
        {
            WGL_Type *prec_type = var_type;
            if (is_vector)
                prec_type = context->GetASTBuilder()->BuildBasicType(WGL_VectorType::GetElementType(static_cast<WGL_VectorType *>(var_type)));
            else if (is_matrix)
                prec_type = context->GetASTBuilder()->BuildBasicType(WGL_MatrixType::GetElementType(static_cast<WGL_MatrixType *>(var_type)));

            WGL_Type::Precision found_prec = context->GetValidationState().LookupPrecision(prec_type);
            if (!IsVertexShader() && found_prec == WGL_Type::NoPrecision && (var_type->GetType() != WGL_Type::Sampler && WGL_Type::HasBasicType(prec_type, WGL_BasicType::Float)))
                context->AddError(WGL_Error::MISSING_PRECISION_DECL, Storage(context, var));
            var_type->precision = found_prec;
            var_type->implicit_precision = TRUE;
        }
    }
    return context->GetValidationState().AddVariable(var_type, alias_var, is_read_only);
}

WGL_VarName *
WGL_ValidateShader::ValidateTypeName(WGL_VarName *var, BOOL is_use)
{
    WGL_Type *type = context->GetValidationState().LookupTypeName(var);
    if (type && !is_use)
    {
        context->AddError(WGL_Error::DUPLICATE_NAME, Storage(context, var));
        return NULL;
    }
    else if (!type && is_use)
    {
        context->AddError(WGL_Error::UNKNOWN_VAR, Storage(context, var));
        return NULL;
    }
    else
        return ValidateIdentifier(var, TRUE);
}

BOOL
WGL_ValidateShader::HaveErrors()
{
    return (context->GetFirstError() != NULL);
}

void
WGL_ValidateShader::Reset()
{
    context->ClearErrors();
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_LiteralExpr *e)
{
    switch (e->literal->type)
    {
    case WGL_Literal::Double:
        return e;
    case WGL_Literal::Int:
        return e;
    case WGL_Literal::UInt:
        return e;
    case WGL_Literal::Bool:
        return e;
    case WGL_Literal::String:
        return e;
    case WGL_Literal::Vector:
        return e;
    case WGL_Literal::Matrix:
        return e;
    case WGL_Literal::Array:
        return e;
    default:
        OP_ASSERT(!"Unexpected literal");
        return e;
    }

}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_VarExpr *e)
{
    e->name = ValidateVar(e->name);
    context->GetValidationState().GetExpressionType(e);
    return e;
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_TypeConstructorExpr *e)
{
    e->constructor = e->constructor->VisitType(this);
    return e;
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_UnaryExpr *e)
{
    /* Return the node if the expression is empty/NULL. */
    if (!e->arg)
        return e;

    e->arg = e->arg->VisitExpr(this);
    WGL_Type *t1 = context->GetValidationState().GetExpressionType(e->arg);
    if (!t1)
        return e;

    switch (e->op)
    {
    case WGL_Expr::OpNot:
        if (!WGL_Type::HasBasicType(t1, WGL_BasicType::Bool))
            context->AddError(WGL_Error::ILLEGAL_ARGUMENT_TYPE, UNI_L(""));
        break;
    case WGL_Expr::OpAdd:
    case WGL_Expr::OpNegate:
    case WGL_Expr::OpPreDec:
    case WGL_Expr::OpPreInc:
        if (!(WGL_Type::HasBasicType(t1, WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float) || WGL_Type::IsVectorType(t1, WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float) || WGL_Type::IsVectorType(t1, WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float)))
            context->AddError(WGL_Error::ILLEGAL_ARGUMENT_TYPE, UNI_L(""));
        break;
    default:
        OP_ASSERT(!"Unexpected unary operator");
        break;
    }
    context->GetValidationState().GetExpressionType(e);
    return e;
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_PostExpr *e)
{
    e->arg = e->arg->VisitExpr(this);
    WGL_Type *t1 = context->GetValidationState().GetExpressionType(e->arg);
    if (!t1)
        return e;

    switch (e->op)
    {
    case WGL_Expr::OpPostInc:
    case WGL_Expr::OpPostDec:
        if (!WGL_Type::HasBasicType(t1, WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float) && !WGL_Type::IsVectorType(t1, WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float) && !WGL_Type::IsVectorType(t1, WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float))
            context->AddError(WGL_Error::ILLEGAL_ARGUMENT_TYPE, UNI_L(""));
        break;
    default:
        OP_ASSERT(!"Unexpected 'post' expression operator");
        break;
    }
    context->GetValidationState().GetExpressionType(e);
    return e;
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_SeqExpr *e)
{
    e->arg1 = e->arg1->VisitExpr(this);
    e->arg2 = e->arg2->VisitExpr(this);
    context->GetValidationState().GetExpressionType(e);
    return e;
}

void
WGL_ValidationState::ValidateEqualityOperation(WGL_Type *t)
{
    /* Know that types to (==) and (!=) are equal, check that
       no arrays or sampler types are used. */
    switch (t->GetType())
    {
    case WGL_Type::Array:
        context->AddError(WGL_Error::ILLEGAL_TYPE_USE, UNI_L("array"));
        return;
    case WGL_Type::Sampler:
        context->AddError(WGL_Error::ILLEGAL_TYPE_USE, UNI_L("sampler"));
        return;
    case WGL_Type::Name:
    {
        WGL_Type *nt = LookupTypeName(static_cast<WGL_NameType *>(t)->name);
        if (nt)
            return ValidateEqualityOperation(nt);
        break;
    }
    case WGL_Type::Struct:
    {
        WGL_StructType *struct_type = static_cast<WGL_StructType *>(t);
        if (struct_type->fields)
            for (WGL_Field *f = struct_type->fields->list.First(); f; f = f->Suc())
                ValidateEqualityOperation(f->type);
        break;
    }
    }
}

void
WGL_ValidateShader::CheckBinaryExpr(WGL_Expr::Operator op, WGL_Type *t1, WGL_Type *t2)
{
    switch (op)
    {
    case WGL_Expr::OpOr:
    case WGL_Expr::OpXor:
    case WGL_Expr::OpAnd:
        if (!WGL_Type::HasBasicType(t1, WGL_BasicType::Bool) || !WGL_Type::HasBasicType(t2, WGL_BasicType::Bool))
            context->AddError(WGL_Error::ILLEGAL_TYPE, UNI_L(""));
        break;
    case WGL_Expr::OpEq:
    case WGL_Expr::OpNEq:
        if (!context->GetValidationState().IsSameType(t1, t2, TRUE))
            context->AddError(WGL_Error::TYPE_MISMATCH, UNI_L(""));
        else
            context->GetValidationState().ValidateEqualityOperation(t1);
        break;

    case WGL_Expr::OpLt:
    case WGL_Expr::OpGt:
    case WGL_Expr::OpLe:
    case WGL_Expr::OpGe:
        if (!context->GetValidationState().IsSameType(t1, t2, TRUE))
            context->AddError(WGL_Error::TYPE_MISMATCH, UNI_L(""));
        else if (!WGL_Type::HasBasicType(t1, WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float))
            context->AddError(WGL_Error::ILLEGAL_TYPE, UNI_L(""));
        break;
    case WGL_Expr::OpAdd:
    case WGL_Expr::OpSub:
    case WGL_Expr::OpMul:
    case WGL_Expr::OpDiv:
        if (!context->GetValidationState().IsSameType(t1, t2, TRUE))
        {
            if (WGL_Type::HasBasicType(t1, WGL_BasicType::Float) && (WGL_Type::IsVectorType(t2, WGL_BasicType::Float) || WGL_Type::IsMatrixType(t2, WGL_BasicType::Float)))
                break;
            if (WGL_Type::HasBasicType(t2, WGL_BasicType::Float) && (WGL_Type::IsVectorType(t1, WGL_BasicType::Float) || WGL_Type::IsMatrixType(t1, WGL_BasicType::Float)))
                break;
            if (WGL_Type::HasBasicType(t1, WGL_BasicType::Int | WGL_BasicType::UInt) && (WGL_Type::IsVectorType(t2, WGL_BasicType::Int | WGL_BasicType::UInt) || WGL_Type::IsMatrixType(t2, WGL_BasicType::Int | WGL_BasicType::UInt)))
                break;
            if (WGL_Type::HasBasicType(t2, WGL_BasicType::Int | WGL_BasicType::UInt) && (WGL_Type::IsVectorType(t1, WGL_BasicType::Int | WGL_BasicType::UInt) || WGL_Type::IsMatrixType(t1, WGL_BasicType::Int | WGL_BasicType::UInt)))
                break;
            if (op == WGL_Expr::OpMul && WGL_Type::IsVectorType(t1) && WGL_Type::IsMatrixType(t2) && WGL_VectorType::GetMagnitude(static_cast<WGL_VectorType *>(t1)) == WGL_MatrixType::GetRows(static_cast<WGL_MatrixType *>(t2)))
                break;
            if (op == WGL_Expr::OpMul && WGL_Type::IsVectorType(t2) && WGL_Type::IsMatrixType(t1) && WGL_VectorType::GetMagnitude(static_cast<WGL_VectorType *>(t2)) == WGL_MatrixType::GetColumns(static_cast<WGL_MatrixType *>(t1)))
                break;

            context->AddError(WGL_Error::UNSUPPORTED_ARGUMENT_COMBINATION, UNI_L(""));
        }
        else if (!(WGL_Type::HasBasicType(t1) || WGL_Type::IsVectorType(t1, WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float) || WGL_Type::IsMatrixType(t1, WGL_BasicType::Int | WGL_BasicType::UInt | WGL_BasicType::Float)))
            context->AddError(WGL_Error::ILLEGAL_TYPE, UNI_L(""));
        break;
    default:
        OP_ASSERT(!"Unhandled binary operator");
        break;
    }
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_BinaryExpr *e)
{
    e->arg1 = e->arg1->VisitExpr(this);
    e->arg2 = e->arg2->VisitExpr(this);
    WGL_Type *t1 = context->GetValidationState().GetExpressionType(e->arg1);
    WGL_Type *t2 = context->GetValidationState().GetExpressionType(e->arg2);
    if (!t1 || !t2)
        return e;
    CheckBinaryExpr(e->op, t1, t2);
    context->GetValidationState().GetExpressionType(e);
    return e;
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_CondExpr *e)
{
    /* This should have been caught in the parser. */
    OP_ASSERT(e->arg1 && e->arg2 && e->arg3);

    e->arg1 = e->arg1->VisitExpr(this);
    WGL_Type *cond_type = context->GetValidationState().GetExpressionType(e->arg1);
    if (cond_type && !WGL_Type::HasBasicType(cond_type, WGL_BasicType::Bool))
        context->AddError(WGL_Error::INVALID_COND_EXPR, UNI_L(""));

    e->arg2 = e->arg2->VisitExpr(this);
    WGL_Type *t2 = context->GetValidationState().GetExpressionType(e->arg2);
    e->arg3 = e->arg3->VisitExpr(this);
    WGL_Type *t3 = context->GetValidationState().GetExpressionType(e->arg3);
    if (t2 && t3 && !context->GetValidationState().IsSameType(t2, t3, TRUE))
        context->AddError(WGL_Error::INVALID_COND_MISMATCH, UNI_L(""));
    context->GetValidationState().GetExpressionType(e);
    return e;
}

BOOL
WGL_ValidationState::GetTypeForSwizzle(WGL_VectorType *vec_type, WGL_SelectExpr *sel_expr, WGL_Type *&result)
{
    unsigned sel_len = WGL_String::Length(sel_expr->field);
    if (sel_len == 0 || sel_len > 4)
    {
        OP_ASSERT(!"Unexpected vector expression");
        return FALSE;
    }

    WGL_BasicType::Type basic_type = WGL_VectorType::GetElementType(vec_type);
    if (basic_type == WGL_BasicType::Void)
    {
        OP_ASSERT(!"Unexpected vector type");
        return FALSE;
    }

    if (sel_len == 1)
        result = context->GetASTBuilder()->BuildBasicType(basic_type);
    else
    {
        WGL_VectorType::Type vtype = WGL_VectorType::Vec2;

        switch (basic_type)
        {
        case WGL_BasicType::Bool:
            vtype = WGL_VectorType::BVec2;
            break;
        case WGL_BasicType::UInt:
            vtype = WGL_VectorType::UVec2;
            break;
        case WGL_BasicType::Int:
            vtype = WGL_VectorType::IVec2;
            break;
        }

        /* This depends on Vec<m> + <n> = Vec<m + n>, and similarly for BVec,
           etc., in the WGL_VectorType::Type enum */
        result = context->GetASTBuilder()->BuildVectorType(static_cast<WGL_VectorType::Type>(vtype + sel_len - 2));
    }

    return TRUE;
}

WGL_Type *
WGL_ValidationState::GetExpressionType(WGL_Expr *e)
{
    if (!e)
        return NULL;
    if (e->cached_type)
        return e->cached_type;
    switch (e->GetType())
    {
    case WGL_Expr::Lit:
        e->cached_type = context->GetASTBuilder()->BuildBasicType(WGL_LiteralExpr::GetBasicType(static_cast<WGL_LiteralExpr *>(e)));
        return e->cached_type;
    case WGL_Expr::Var:
        e->cached_type = context->GetValidationState().LookupVarType(static_cast<WGL_VarExpr *>(e)->name, NULL);
        return e->cached_type;
    case WGL_Expr::TypeConstructor:
        /* TypeConstructor will only appear as the function in a CallExpr. */
        return NULL;
    case WGL_Expr::Unary:
    {
        WGL_UnaryExpr *un_expr = static_cast<WGL_UnaryExpr *>(e);
        switch (un_expr->op)
        {
        case WGL_Expr::OpNot:
            e->cached_type = context->GetASTBuilder()->BuildBasicType(WGL_BasicType::Bool);
            return e->cached_type;
        case WGL_Expr::OpAdd:
        case WGL_Expr::OpNegate:
        case WGL_Expr::OpPreDec:
        case WGL_Expr::OpPreInc:
            e->cached_type = GetExpressionType(un_expr->arg);
            return e->cached_type;
        default:
            OP_ASSERT(!"unexpected unary operation");
            return NULL;
        }
    }
    case WGL_Expr::PostOp:
    {
        WGL_PostExpr *post_expr = static_cast<WGL_PostExpr *>(e);
        e->cached_type = GetExpressionType(post_expr->arg);
        return e->cached_type;
    }
    case WGL_Expr::Seq:
    {
        WGL_SeqExpr *seq_expr = static_cast<WGL_SeqExpr *>(e);
        e->cached_type = GetExpressionType(seq_expr->arg2);
        return e->cached_type;
    }
    case WGL_Expr::Binary:
    {
        WGL_BinaryExpr *bin_expr = static_cast<WGL_BinaryExpr *>(e);
        switch (bin_expr->op)
        {
        case WGL_Expr::OpAdd:
        case WGL_Expr::OpSub:
        case WGL_Expr::OpMul:
        case WGL_Expr::OpDiv:
        {
            WGL_Type *type_lhs = GetExpressionType(bin_expr->arg1);
            WGL_Type *type_rhs = GetExpressionType(bin_expr->arg2);

            if (!type_lhs || !type_rhs)
                return NULL;

            WGL_BasicType::Type t1 = WGL_BasicType::GetBasicType(type_lhs);
            WGL_BasicType::Type t2 = WGL_BasicType::GetBasicType(type_rhs);

            if (t1 == WGL_BasicType::Void || t2 == WGL_BasicType::Void)
            {
                if (t1 == WGL_BasicType::Void && t2 == WGL_BasicType::Void)
                {
                    /* (V,V) or (M,M) */
                    if (IsSameType(type_lhs, type_rhs, TRUE) && (type_lhs->GetType() == WGL_Type::Vector || type_lhs->GetType() == WGL_Type::Matrix))
                        e->cached_type = type_lhs;
                    else if (type_lhs->GetType() == WGL_Type::Matrix && type_rhs->GetType() == WGL_Type::Vector)
                        e->cached_type = type_rhs;
                    else if (type_lhs->GetType() == WGL_Type::Vector && type_rhs->GetType() == WGL_Type::Matrix)
                        e->cached_type = type_lhs;
                    else
                        return NULL;
                }
                else if (t1 != WGL_BasicType::Void)
                     e->cached_type = type_rhs;
                else
                     e->cached_type = type_lhs;

                return e->cached_type;
            }

            if (t1 == WGL_BasicType::Float)
                e->cached_type = type_lhs;
            else if (t2 == WGL_BasicType::Float)
                e->cached_type = type_rhs;
            else if (t1 == WGL_BasicType::Int)
                e->cached_type = type_lhs;
            else
                e->cached_type = type_rhs;

            return e->cached_type;
        }
        case WGL_Expr::OpEq:
        case WGL_Expr::OpNEq:
        case WGL_Expr::OpLt:
        case WGL_Expr::OpLe:
        case WGL_Expr::OpGe:
        case WGL_Expr::OpGt:
        case WGL_Expr::OpOr:
        case WGL_Expr::OpXor:
        case WGL_Expr::OpAnd:
            e->cached_type = context->GetASTBuilder()->BuildBasicType(WGL_BasicType::Bool);
            return e->cached_type;

        default:
            OP_ASSERT(!"unexpected binary operation");
            return NULL;
        }
    }
    case WGL_Expr::Cond:
    {
        WGL_CondExpr *cond_expr = static_cast<WGL_CondExpr *>(e);
        e->cached_type = GetExpressionType(cond_expr->arg2);
        return e->cached_type;
    }
    case WGL_Expr::Call:
    {
        WGL_CallExpr *call_expr = static_cast<WGL_CallExpr *>(e);
        if (call_expr->fun->GetType() == WGL_Expr::TypeConstructor)
        {
            e->cached_type = static_cast<WGL_TypeConstructorExpr *>(call_expr->fun)->constructor;
            return e->cached_type;
        }
        else
        {
            OP_ASSERT(call_expr->fun->GetType() == WGL_Expr::Var);
            WGL_VarExpr *fun = static_cast<WGL_VarExpr *>(call_expr->fun);

            if (WGL_FunctionData *data = context->GetValidationState().LookupFunction(fun->name))
            {
                for (WGL_FunctionDecl *fun_decl = data->decls.First(); fun_decl; fun_decl = fun_decl->Suc())
                {
                    if (!call_expr->args && fun_decl->parameters->list.Empty())
                    {
                        e->cached_type = fun_decl->return_type;
                        return e->cached_type;
                    }
                    else if (call_expr->args->list.Cardinal() == fun_decl->parameters->list.Cardinal())
                    {
                        WGL_Expr *arg = call_expr->args->list.First();
                        BOOL decl_match = TRUE;
                        for (WGL_Param *p = fun_decl->parameters->list.First(); p; p = p->Suc())
                        {
                            if (!context->GetValidationState().HasExpressionType(p->type, arg, TRUE))
                            {
                                decl_match = FALSE;
                                break;
                            }
                            arg = arg->Suc();
                        }
                        if (decl_match)
                        {
                            e->cached_type = fun_decl->return_type;
                            return e->cached_type;
                        }
                    }
                }
                return NULL;
            }
            else if (List<WGL_Builtin> *builtin_decls = context->GetValidationState().LookupBuiltin(fun->name))
            {
                for (WGL_Builtin *builtin = builtin_decls->First(); builtin; builtin = builtin->Suc())
                {
                    if (builtin->IsBuiltinVar() || !call_expr->args)
                        continue;

                    WGL_BuiltinFun *fun = static_cast<WGL_BuiltinFun *>(builtin);

                    if (call_expr->args->list.Cardinal() == fun->parameters.Cardinal())
                    {
                        WGL_Expr *arg = call_expr->args->list.First();
                        BOOL decl_match = TRUE;
                        WGL_Type *generic_type = NULL;
                        for (WGL_BuiltinType *p = fun->parameters.First(); p; p = p->Suc())
                        {
                            WGL_Type *arg_type = context->GetValidationState().GetExpressionType(arg);
                            if (!arg_type || !context->GetValidationState().MatchesBuiltinType(p, arg_type) || generic_type && p->type > WGL_BuiltinType::Sampler && !IsSameType(arg_type, generic_type, TRUE))
                            {
                                decl_match = FALSE;
                                break;
                            }
                            else if (p->type > WGL_BuiltinType::Sampler && !generic_type)
                                generic_type = arg_type;
                            arg = arg->Suc();
                        }
                        if (decl_match)
                        {
                            e->cached_type = ResolveBuiltinReturnType(fun, call_expr->args, generic_type);
                            return e->cached_type;
                        }
                    }
                }
                return NULL;
            }
            else
                return NULL;
        }
    }
    case WGL_Expr::Index:
    {
        WGL_IndexExpr *index_expr = static_cast<WGL_IndexExpr *>(e);
        if (!index_expr->array)
            return NULL;
        WGL_Type *array_type = GetExpressionType(index_expr->array);
        if (!array_type)
            return NULL;
        if (array_type->GetType() == WGL_Type::Array)
            e->cached_type = static_cast<WGL_ArrayType *>(array_type)->element_type;
        else if (array_type->GetType() == WGL_Type::Vector)
            e->cached_type = context->GetASTBuilder()->BuildBasicType(WGL_VectorType::GetElementType(static_cast<WGL_VectorType *>(array_type)));
        else if (array_type->GetType() == WGL_Type::Matrix)
            e->cached_type = context->GetASTBuilder()->BuildVectorType(WGL_VectorType::ToVectorType(WGL_MatrixType::GetElementType(static_cast<WGL_MatrixType *>(array_type)), WGL_MatrixType::GetRows(static_cast<WGL_MatrixType *>(array_type))));
        else
        {
            OP_ASSERT(!"Unexpected indexing expression");
            return NULL;
        }
        return e->cached_type;
    }
    case WGL_Expr::Select:
    {
        WGL_SelectExpr *sel_expr = static_cast<WGL_SelectExpr *>(e);
        if (!sel_expr->value)
            return NULL;
        WGL_Type *struct_type = GetExpressionType(sel_expr->value);
        if (!struct_type)
            return NULL;
        if (struct_type->GetType() == WGL_Type::Name)
        {
            WGL_NameType *name_type = static_cast<WGL_NameType *>(struct_type);
            struct_type = context->GetValidationState().LookupTypeName(name_type->name);
        }
        if (struct_type->GetType() == WGL_Type::Struct)
        {
            if (WGL_Field *f = context->GetValidationState().LookupField(static_cast<WGL_StructType *>(struct_type)->fields, sel_expr->field))
            {
                e->cached_type = f->type;
                return e->cached_type;
            }
            return NULL;
        }
        else if (struct_type->GetType() == WGL_Type::Vector)
        {
            if (!GetTypeForSwizzle(static_cast<WGL_VectorType *>(struct_type), sel_expr, e->cached_type))
                return NULL;
            return e->cached_type;
        }
        else
        {
            OP_ASSERT(!"Unexpected value to select field from.");
            return NULL;
        }
    }
    case WGL_Expr::Assign:
        e->cached_type = GetExpressionType(static_cast<WGL_AssignExpr *>(e)->rhs);
        return e->cached_type;

    default:
        OP_ASSERT(!"Unexpected expression type");
        return NULL;
    }
}

BOOL
WGL_ValidateShader::IsConstructorArgType(WGL_Type *t)
{
    /* NOTE: we are not permitting multi-dim arrays; doesn't appear legal. */
    switch (t->GetType())
    {
    case WGL_Type::Basic:
        if (static_cast<WGL_BasicType *>(t)->type == WGL_BasicType::Void)
            return FALSE;
        break;
    case WGL_Type::Vector:
    case WGL_Type::Matrix:
        break;
    default:
        return FALSE;
    }
    return TRUE;
}

BOOL
WGL_ValidateShader::IsConstructorArgType(WGL_BuiltinType *t)
{
    return t->type == WGL_BuiltinType::Sampler ? FALSE : TRUE;
}

/** Check well-formedness of constructor applications -- TY(arg1,arg2,..,argN).
 *  where TY can be a basic type, vector, matrix or array type (struct names handled elsewhere.)
 *  The argX have to be basic types or vector/matrix applications too (i.e., don't
 *  see nested applications of the array constructors.)
 */
BOOL
WGL_ValidateShader::IsScalarOrVecMat(WGL_Expr *e, BOOL is_nested)
{
    /* Not doing type checking/inference of arbitrary expressions.. merely imposing
     * the syntactic restriction that a constructor may only be passed basic types
     * or (possibly nested) applications of vector/matrix constructors.
     */
    switch (e->GetType())
    {
    case WGL_Expr::Lit:
        return TRUE;
    case WGL_Expr::Var:
    {
        WGL_VarExpr *var_expr = static_cast<WGL_VarExpr *>(e);
        if (WGL_Type *t = context->GetValidationState().LookupVarType(var_expr->name, NULL))
            return IsConstructorArgType(t);
        return FALSE;
    }
    case WGL_Expr::Call:
    {
        WGL_CallExpr *call_expr = static_cast<WGL_CallExpr *>(e);
        switch (call_expr->fun->GetType())
        {
        case WGL_Expr::TypeConstructor:
        {
            WGL_TypeConstructorExpr *ctor_expr = static_cast<WGL_TypeConstructorExpr *>(call_expr->fun);
            switch (ctor_expr->constructor->GetType())
            {
            case WGL_Type::Basic:
            {
                WGL_BasicType *basic_type = static_cast<WGL_BasicType *>(ctor_expr->constructor);
                if (basic_type->type != WGL_BasicType::Float &&
                    basic_type->type != WGL_BasicType::Int)
                    return FALSE;

                for (WGL_Expr *arg = call_expr->args->list.First(); arg; arg = arg->Suc())
                    if (!IsScalarOrVecMat(arg, TRUE))
                        return FALSE;
                return TRUE;
            }
            case WGL_Type::Vector:
            {
                // We need type information for the arguments to be available
                // when running the CG generator. (Proper validation would need
                // this information anyway.)
                for (WGL_Expr *arg = call_expr->args->list.First(); arg; arg = arg->Suc())
                    (void)context->GetValidationState().GetExpressionType(arg);

                WGL_VectorType *vector_type = static_cast<WGL_VectorType *>(ctor_expr->constructor);
                unsigned bound = 1;
                switch (vector_type->type)
                {
                case WGL_VectorType::Vec2:
                case WGL_VectorType::BVec2:
                case WGL_VectorType::IVec2:
                case WGL_VectorType::UVec2:
                    bound = 2;
                    break;
                case WGL_VectorType::Vec3:
                case WGL_VectorType::BVec3:
                case WGL_VectorType::IVec3:
                case WGL_VectorType::UVec3:
                    bound = 3;
                    break;
                case WGL_VectorType::Vec4:
                case WGL_VectorType::BVec4:
                case WGL_VectorType::IVec4:
                case WGL_VectorType::UVec4:
                    bound = 4;
                    break;
                }
                /* GLSL flattens the arguments..that is, vecX(a,b), is interpreted as flatten(a) ++ flatten (b) (where
                 * 'a' and 'b' can be vectors or base types. Or variables bound to such.)
                 */
                if (!call_expr->args || call_expr->args->list.Empty() || call_expr->args->list.Cardinal() > bound)
                    return FALSE;

                for (WGL_Expr *arg = call_expr->args->list.First(); arg; arg = arg->Suc())
                    if (!IsScalarOrVecMat(arg, TRUE))
                        return FALSE;

                return TRUE;
            }
            case WGL_Type::Matrix:
            {
                // We need type information for the arguments to be available
                // when running the CG generator. (Proper validation would need
                // this information anyway.)
                for (WGL_Expr *arg = call_expr->args->list.First(); arg; arg = arg->Suc())
                    (void)context->GetValidationState().GetExpressionType(arg);

                WGL_MatrixType *mat_type = static_cast<WGL_MatrixType *>(ctor_expr->constructor);
                unsigned bound = 1;
                switch (mat_type->type)
                {
                case WGL_MatrixType::Mat2:
                case WGL_MatrixType::Mat2x2:
                    bound = 4;
                    break;
                case WGL_MatrixType::Mat2x3:
                case WGL_MatrixType::Mat3x2:
                    bound = 6;
                    break;
                case WGL_MatrixType::Mat4x2:
                case WGL_MatrixType::Mat2x4:
                    bound = 8;
                    break;
                case WGL_MatrixType::Mat3:
                case WGL_MatrixType::Mat3x3:
                    bound = 9;
                    break;
                case WGL_MatrixType::Mat4x3:
                case WGL_MatrixType::Mat3x4:
                    bound = 12;
                    break;
                case WGL_MatrixType::Mat4:
                case WGL_MatrixType::Mat4x4:
                    bound = 16;
                    break;
                }
                if (!call_expr->args || call_expr->args->list.Empty() || call_expr->args->list.Cardinal() > bound)
                    return FALSE;
                else
                {
                    for (WGL_Expr *arg = call_expr->args->list.First(); arg; arg = arg->Suc())
                        if (!IsScalarOrVecMat(arg, TRUE))
                            return FALSE;

                    return TRUE;
                }
            }
            case WGL_Type::Array:
            {
                if (is_nested)
                    return FALSE;

                WGL_ArrayType *arr_type = static_cast<WGL_ArrayType *>(ctor_expr->constructor);
                if (!IsConstructorArgType(arr_type->element_type))
                    return FALSE;

                for (WGL_Expr *arg = call_expr->args->list.First(); arg; arg = arg->Suc())
                    if (!IsScalarOrVecMat(arg, TRUE))
                        return FALSE;

                return TRUE;
            }
            default:
                return FALSE;
            }
        }
        case WGL_Expr::Var:
        {
            WGL_VarExpr *fun = static_cast<WGL_VarExpr *>(call_expr->fun);
            if (WGL_FunctionData *fun_info = context->GetValidationState().LookupFunction(fun->name))
            {
                /* TODO: perform overload resolution to precisely determine return type of
                   application. */
                if (fun_info->decls.First())
                    return IsConstructorArgType(fun_info->decls.First()->return_type);
            }
            else if (List<WGL_Builtin> *list = context->GetValidationState().LookupBuiltin(fun->name))
            {
                if (!list->First()->IsBuiltinVar())
                    return IsConstructorArgType(static_cast<WGL_BuiltinFun *>(list->First())->return_type);
            }
            return FALSE;
        }
        default:
            return FALSE;
        }

    }
    case WGL_Expr::Index:
    {
        WGL_IndexExpr *index_expr = static_cast<WGL_IndexExpr *>(e);
        WGL_Type *t = context->GetValidationState().GetExpressionType(index_expr->array);
        if (t && t->GetType() == WGL_Type::Array)
            return IsConstructorArgType(static_cast<WGL_ArrayType *>(t)->element_type);
        else if (t && t->GetType() == WGL_Type::Vector)
            return TRUE;
        else if (t && t->GetType() == WGL_Type::Matrix)
            return TRUE;
        else
            return FALSE;
    }
    case WGL_Expr::Select:
    {
        WGL_SelectExpr *sel_expr = static_cast<WGL_SelectExpr *>(e);
        WGL_Type *sel_type = context->GetValidationState().GetExpressionType(sel_expr->value);
        if (sel_type && sel_type->GetType() == WGL_Type::Vector)
            return TRUE;
        else if (sel_type && sel_type->GetType() != WGL_Type::Struct )
            return FALSE;
        else if (WGL_Field *f = sel_type ? context->GetValidationState().LookupField(static_cast<WGL_StructType *>(sel_type)->fields, sel_expr->field) : NULL)
            return IsConstructorArgType(f->type);
        else
            return FALSE;
    }
    case WGL_Expr::Unary:
    case WGL_Expr::Binary:
        return TRUE;
    case WGL_Expr::Cond:
    {
        WGL_CondExpr *cond_expr = static_cast<WGL_CondExpr *>(e);
        return IsScalarOrVecMat(cond_expr->arg2, FALSE);
    }
    default:
        return FALSE;
    }
}

/* static */ BOOL
WGL_ValidateShader::IsRecursivelyCalling(WGL_Context *context, List<WGL_VariableUse> &visited, WGL_VarName *f, WGL_VarName *call)
{
    if (!WGL_VariableUse::IsIn(visited, call))
        if (WGL_FunctionData *fun_data = context->GetValidationState().LookupFunction(call))
            for (WGL_FunctionDecl *decl = fun_data->decls.First(); decl; decl = decl->Suc())
                if (decl->callers && WGL_VariableUse::IsIn(*decl->callers, f))
                    return TRUE;
                else if (decl->callers)
                {
                    WGL_VariableUse *call_use = OP_NEWGRO_L(WGL_VariableUse, (call), context->Arena());
                    call_use->Into(&visited);
                    for (WGL_VariableUse *g = (*decl->callers).First(); g; g = g->Suc())
                        if (IsRecursivelyCalling(context, visited, f, g->var))
                            return TRUE;
                        else if (!WGL_VariableUse::IsIn(visited, g->var))
                        {
                            WGL_VariableUse *g_use = OP_NEWGRO_L(WGL_VariableUse, (g->var), context->Arena());
                            g_use->Into(&visited);
                        }
                }

    return FALSE;
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_CallExpr *e)
{
    switch (e->fun->GetType())
    {
    case WGL_Expr::TypeConstructor:
    {
        WGL_Type *ctor_type = static_cast<WGL_TypeConstructorExpr *>(e->fun)->constructor;
        switch (ctor_type->GetType())
        {
        case WGL_Type::Basic:
        {
            WGL_BasicType *basic_type = static_cast<WGL_BasicType *>(ctor_type);
            switch (basic_type->type)
            {
            case WGL_BasicType::Int:
            case WGL_BasicType::Float:
            case WGL_BasicType::Bool:
                if (!e->args || e->args->list.Cardinal() != 1 || !IsScalarOrVecMat(e->args->list.First(), FALSE))
                    context->AddError(WGL_Error::INVALID_TYPE_CONSTRUCTOR, UNI_L(""));
                break;
            default:
                context->AddError(WGL_Error::INVALID_TYPE_CONSTRUCTOR, UNI_L(""));
                break;
            }
            break;
        }
        case WGL_Type::Array:
        {
            if (!e->args || !IsScalarOrVecMat(e, FALSE))
                context->AddError(WGL_Error::INVALID_TYPE_CONSTRUCTOR, UNI_L(""));

            /* Check and update with array size. */
            WGL_ArrayType *arr_type = static_cast<WGL_ArrayType *>(ctor_type);
            if (arr_type->length)
            {
                int val;
                BOOL success = context->GetValidationState().
                               LiteralToInt(context->GetValidationState().EvalConstExpression(arr_type->length), val);
                if (success)
                    if (e->args->list.Cardinal() != static_cast<unsigned>(val))
                        context->AddError(WGL_Error::INVALID_TYPE_CONSTRUCTOR, UNI_L(""));
                /* Other errors are not of interest right here. */
            }
            else
                arr_type->length = context->GetASTBuilder()->BuildIntLit(e->args->list.Cardinal());

            break;
        }
        case WGL_Type::Vector:
            if (!e->args || !IsScalarOrVecMat(e, FALSE))
                context->AddError(WGL_Error::INVALID_TYPE_CONSTRUCTOR, UNI_L(""));

            break;
        case WGL_Type::Matrix:
            if (!e->args || !IsScalarOrVecMat(e, FALSE))
                context->AddError(WGL_Error::INVALID_TYPE_CONSTRUCTOR, UNI_L(""));

            break;
        case WGL_Type::Name:
        {
            WGL_NameType *const name_type = static_cast<WGL_NameType *>(ctor_type);
            WGL_Type *const type = context->GetValidationState().LookupTypeName(name_type->name);
            if (!type)
            {
                context->AddError(WGL_Error::UNKNOWN_TYPE, Storage(context, name_type->name));
                break;
            }

            if (type->GetType() != WGL_Type::Struct)
            {
                context->AddError(WGL_Error::INVALID_TYPE_CONSTRUCTOR, UNI_L(""));
                break;
            }

            WGL_StructType *struct_type = static_cast<WGL_StructType *>(type);
            if (!(struct_type->fields && e->args &&
                  struct_type->fields->list.Cardinal() == e->args->list.Cardinal()))
            {
                context->AddError(WGL_Error::INVALID_TYPE_CONSTRUCTOR, UNI_L(""));
                break;
            }

            /* Matching number of arguments and fields */
            WGL_Expr *arg = e->args->list.First();
            for (WGL_Field *f = struct_type->fields->list.First(); f; f = f->Suc(), arg = arg->Suc())
                if (!context->GetValidationState().HasExpressionType(f->type, arg, TRUE))
                    context->AddError(WGL_Error::INVALID_CONSTRUCTOR_ARG, UNI_L(""));

            break;
        }
        default:
            /* Sampler, Struct and Array. */
            context->AddError(WGL_Error::INVALID_TYPE_CONSTRUCTOR, UNI_L(""));
            break;
        }
        break;
    }
    case WGL_Expr::Var:
    {
        WGL_VarExpr *fun = static_cast<WGL_VarExpr *>(e->fun);

        if (context->GetValidationState().CurrentFunction() != NULL)
            if (context->GetValidationState().CurrentFunction()->Equals(fun->name))
                context->AddError(WGL_Error::ILLEGAL_RECURSIVE_CALL, Storage(context, fun->name));
            else
            {
                /* Check if CurrentFunction() is called by 'fun'. */
                List<WGL_VariableUse> visited;
                WGL_VarName *current_function = context->GetValidationState().CurrentFunction();
                if (IsRecursivelyCalling(context, visited, current_function, fun->name))
                    context->AddError(WGL_Error::ILLEGAL_RECURSIVE_CALL, Storage(context, fun->name));
                /* Avoid triggering an assert in Head::~Head() - see CORE-43794. */
#ifdef _DEBUG
                visited.RemoveAll();
#endif // _DEBUG
            }

        BOOL matches = FALSE;
        BOOL seen_match_cand = FALSE;
        if (WGL_FunctionData *fun_data = context->GetValidationState().LookupFunction(fun->name))
        {
            context->GetValidationState().AddFunctionCall(fun->name);
            seen_match_cand = TRUE;
            for (WGL_FunctionDecl *fun_decl = fun_data->decls.First(); fun_decl; fun_decl = fun_decl->Suc())
            {
                if ((!e->args || e->args->list.Empty()) && fun_decl->parameters->list.Empty())
                {
                    matches = TRUE;
                    break;
                }
                else if (e->args->list.Cardinal() == fun_decl->parameters->list.Cardinal())
                {
                    WGL_Expr *arg = e->args->list.First();
                    BOOL decl_match = TRUE;
                    for (WGL_Param *p = fun_decl->parameters->list.First(); p; p = p->Suc())
                    {
                        WGL_Type *param_type = p->type;
                        if (param_type && param_type->GetType() == WGL_Type::Name)
                        {
                            WGL_NameType *name_type = static_cast<WGL_NameType *>(param_type);
                            if (WGL_Type *t = context->GetValidationState().LookupTypeName(name_type->name))
                                param_type = t;
                        }
                        if (!context->GetValidationState().HasExpressionType(param_type, arg, TRUE))
                        {
                            decl_match = FALSE;
                            break;
                        }
                        arg = arg->Suc();
                    }
                    if (decl_match)
                    {
                        matches = TRUE;
                        WGL_Expr *arg = e->args->list.First();
                        for (WGL_Param *p = fun_decl->parameters->list.First(); p; p = p->Suc())
                        {
                            if (p->direction >= WGL_Param::Out && !context->GetValidationState().IsLegalReferenceArg(arg))
                                context->AddError(WGL_Error::ILLEGAL_REFERENCE_ARGUMENT, Storage(context, p->name));
                            arg = arg->Suc();
                        }
                        break;
                    }
                }
            }
        }
        if (!matches)
        {
            if (List<WGL_Builtin> *builtin_decls = context->GetValidationState().LookupBuiltin(fun->name))
            {
                if (WGL_BuiltinFun *f = context->GetValidationState().ResolveBuiltinFunction(builtin_decls, e->args))
                {
                    fun->intrinsic = f->intrinsic;
                    matches = TRUE;
                }
                else
                    context->AddError(WGL_Error::MISMATCHED_FUNCTION_APPLICATION, Storage(context, fun->name));
            }
            else if (!seen_match_cand)
                context->AddError(WGL_Error::UNKNOWN_VAR, Storage(context, fun->name));
        }
        if (!matches && seen_match_cand)
            context->AddError(WGL_Error::MISMATCHED_FUNCTION_APPLICATION, UNI_L(""));
        break;
    }
    default:
        context->AddError(WGL_Error::INVALID_CALL_EXPR, UNI_L(""));
        break;
    }

    if (e->args && e->fun)
    {
        e->fun = e->fun->VisitExpr(this);
        e->args = e->args->VisitExprList(this);
        context->GetValidationState().GetExpressionType(e);
    }

    return e;
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_IndexExpr *e)
{
    e->array = e->array->VisitExpr(this);
    WGL_Type *type = context->GetValidationState().GetExpressionType(e->array);
    if (!e->index || !type)
    {
        context->AddError(WGL_Error::ILLEGAL_INDEX_EXPR, UNI_L(""));
        return e;
    }
    e->index = e->index->VisitExpr(this);
    BOOL is_constant = context->GetValidationState().IsConstantExpression(e->index);
    if (is_constant)
    {
        if (type->GetType() == WGL_Type::Array)
        {
            WGL_ArrayType *array_type = static_cast<WGL_ArrayType *>(type);
            int size_array = 0;
            int index_val = 0;
            BOOL success = context->GetValidationState().LiteralToInt(context->GetValidationState().EvalConstExpression(array_type->length), size_array);

            success = success && context->GetValidationState().LiteralToInt(context->GetValidationState().EvalConstExpression(e->index), index_val);
            is_constant = success;
            if (success && index_val >= size_array)
                context->AddError(WGL_Error::INDEX_OUT_OF_BOUNDS_ARRAY, UNI_L(""));
        }
        else if (type->GetType() == WGL_Type::Vector)
        {
            unsigned size_vector = WGL_VectorType::GetMagnitude(static_cast<WGL_VectorType *>(type));
            int index_val = 0;
            if (context->GetValidationState().LiteralToInt(context->GetValidationState().EvalConstExpression(e->index), index_val))
            {
                if (static_cast<unsigned>(index_val) >= size_vector)
                    context->AddError(WGL_Error::INDEX_OUT_OF_BOUNDS_VECTOR, UNI_L(""));
            }
            else
                is_constant = FALSE;
        }
    }
    if (!is_constant && !IsVertexShader() && context->GetConfiguration() && context->GetConfiguration()->fragment_shader_constant_uniform_array_indexing)
    {
        if (type->type_qualifier && type->type_qualifier->storage == WGL_TypeQualifier::Uniform)
            context->AddError(WGL_Error::INDEX_NON_CONSTANT_FRAGMENT, UNI_L(""));
    }
    if (!is_constant && context->GetConfiguration() && context->GetConfiguration()->clamp_out_of_bound_uniform_array_indexing)
    {
        if ((type->GetType() == WGL_Type::Array || type->GetType() == WGL_Type::Vector) && type->type_qualifier && type->type_qualifier->storage == WGL_TypeQualifier::Uniform)
        {
            unsigned mag = 0;

            if (type->GetType() == WGL_Type::Array)
            {
                type = context->GetValidationState().NormaliseType(type);
                if (type->GetType() == WGL_Type::Array)
                {
                    WGL_ArrayType *array_type = static_cast<WGL_ArrayType *>(type);
                    if (array_type->is_constant_value)
                        mag = array_type->length_value;
                }
            }
            else if (type->GetType() == WGL_Type::Vector)
                mag = WGL_VectorType::GetMagnitude(static_cast<WGL_VectorType *>(type));

            if (mag > 0)
            {
                WGL_Expr *fun = context->GetASTBuilder()->BuildIdentifier(WGL_String::Make(context, UNI_L("webgl_op_clamp")));
                WGL_Expr *lit_max = context->GetASTBuilder()->BuildIntLit(mag - 1);
                WGL_ExprList *args = context->GetASTBuilder()->NewExprList();
                e->index->Into(&args->list);
                lit_max->Into(&args->list);
                WGL_Expr *clamp = context->GetASTBuilder()->BuildCallExpression(fun, args);
                e->index = clamp;
                context->SetUsedClamp();
            }
        }
    }
    context->GetValidationState().GetExpressionType(e);
    return e;
}

static int
GetVectorSelectorKind(uni_char ch)
{
    if (ch == 'r' || ch == 'g' || ch == 'b' || ch == 'a')
        return 1;
    else if (ch == 'x' || ch == 'y' || ch == 'z' || ch == 'w')
        return 2;
    else if (ch == 's' || ch == 't' || ch == 'p' || ch == 'q')
        return 3;
    else
        return 0;
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_SelectExpr *e)
{
    e->value = e->value->VisitExpr(this);
    WGL_Type *t = context->GetValidationState().GetExpressionType(e->value);

    if (!t)
    {
        context->AddError(WGL_Error::ILLEGAL_SELECT_EXPR, UNI_L(""));
        return e;
    }

    switch (t->GetType())
    {
    case WGL_Type::Struct:
    {
        WGL_StructType *struct_type = static_cast<WGL_StructType *>(t);
        if (!context->GetValidationState().LookupField(struct_type->fields, e->field))
            context->AddError(WGL_Error::UNKNOWN_FIELD, Storage(context, e->field));

        return e;
    }
    case WGL_Type::Vector:
    {
        WGL_VectorType *vec_type = static_cast<WGL_VectorType *>(t);
        if (e->field->Equals(UNI_L("x")) || e->field->Equals(UNI_L("r")) || e->field->Equals(UNI_L("s")))
            break;
        switch (vec_type->type)
        {
        case WGL_VectorType::Vec4:
        case WGL_VectorType::BVec4:
        case WGL_VectorType::IVec4:
        case WGL_VectorType::UVec4:
            if (e->field->Equals(UNI_L("w")) || e->field->Equals(UNI_L("a")) || e->field->Equals(UNI_L("q")))
                break;
        case WGL_VectorType::Vec3:
        case WGL_VectorType::BVec3:
        case WGL_VectorType::IVec3:
        case WGL_VectorType::UVec3:
            if (e->field->Equals(UNI_L("z")) || e->field->Equals(UNI_L("b")) || e->field->Equals(UNI_L("p")))
                break;
        case WGL_VectorType::Vec2:
        case WGL_VectorType::BVec2:
        case WGL_VectorType::IVec2:
        case WGL_VectorType::UVec2:
            if (e->field->Equals(UNI_L("y")) || e->field->Equals(UNI_L("g")) || e->field->Equals(UNI_L("t")))
                break;
            else
            {
                const uni_char *str = Storage(context, e->field);
                unsigned len = static_cast<unsigned>(uni_strlen(str));
                if (len <= 1 || len > 4)
                {
                    context->AddError(WGL_Error::UNKNOWN_VAR, str);
                    return e;
                }
                int sel_kind_1 = GetVectorSelectorKind(str[0]);
                int sel_kind_2 = GetVectorSelectorKind(str[1]);
                int sel_kind_3 = len >= 3 ? GetVectorSelectorKind(str[2]) : -1;
                int sel_kind_4 = len >= 4 ? GetVectorSelectorKind(str[3]) : -1;

                if (sel_kind_1 == 0 || sel_kind_1 != sel_kind_2 ||
                    sel_kind_3 != -1 && sel_kind_3 != sel_kind_1 ||
                    sel_kind_4 != -1 && sel_kind_4 != sel_kind_1)
                {
                    if (sel_kind_1 == 0 || sel_kind_2 == 0 || sel_kind_3 == 0 || sel_kind_4 == 0)
                        context->AddError(WGL_Error::UNKNOWN_VECTOR_SELECTOR, str);
                    else
                        context->AddError(WGL_Error::ILLEGAL_VECTOR_SELECTOR, str);
                }

                break;
            }

        default:
            context->AddError(WGL_Error::UNKNOWN_VAR, Storage(context, e->field));
            return NULL;
        }
    }
    }

    context->GetValidationState().GetExpressionType(e);
    return e;
}

/* virtual */ WGL_Expr *
WGL_ValidateShader::VisitExpr(WGL_AssignExpr *e)
{
    /* An lhs expression is required. */
    if (!e->lhs)
    {
        context->AddError(WGL_Error::INVALID_LVALUE, UNI_L(""));
        return e;
    }
    e->lhs = e->lhs->VisitExpr(this);
    if (e->rhs)
        e->rhs = e->rhs->VisitExpr(this);
    if (e->lhs->GetType() == WGL_Expr::Var)
    {
        WGL_VarExpr *var_expr = static_cast<WGL_VarExpr *>(e->lhs);
        if (WGL_VarBinding *binding = context->GetValidationState().LookupVarBinding(var_expr->name))
        {
            if (binding->is_read_only || binding->type->type_qualifier && (binding->type->type_qualifier->storage == WGL_TypeQualifier::Const || binding->type->type_qualifier->storage == WGL_TypeQualifier::Uniform))
                context->AddError(WGL_Error::ILLEGAL_VAR_NOT_WRITEABLE, Storage(context, var_expr->name));
        }
        else if (List <WGL_Builtin> *builtin = context->GetValidationState().LookupBuiltin(var_expr->name))
        {
            WGL_BuiltinVar *bvar = builtin->First()->IsBuiltinVar() ? static_cast<WGL_BuiltinVar *>(builtin->First()) : NULL;
            if (!bvar || bvar->is_read_only || bvar->is_const)
                context->AddError(WGL_Error::ILLEGAL_VAR_NOT_WRITEABLE, Storage(context, var_expr->name));
        }
        else if (context->GetValidationState().LookupUniform(var_expr->name))
            context->AddError(WGL_Error::ILLEGAL_VAR_NOT_WRITEABLE, Storage(context, var_expr->name));
        else if (!IsVertexShader() && context->GetValidationState().LookupVarying(var_expr->name))
            context->AddError(WGL_Error::ILLEGAL_VARYING_VAR_NOT_WRITEABLE, Storage(context, var_expr->name));

        WGL_Type *t1 = context->GetValidationState().GetExpressionType(e->lhs);
        WGL_Type *t2 = context->GetValidationState().GetExpressionType(e->rhs);
        if (t1 && t2)
        {
            if (e->op != WGL_Expr::OpAssign)
                CheckBinaryExpr(e->op, t1, t2);
            else if (!context->GetValidationState().IsSameType(t1, t2, TRUE))
                context->AddError(WGL_Error::TYPE_MISMATCH, UNI_L(""));
        }
    }

    context->GetValidationState().GetExpressionType(e);
    return e;
}

/** -- Stmt visitors -- */

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_SwitchStmt *s)
{
    context->SetLineNumber(s->line_number);

    s->scrutinee = s->scrutinee->VisitExpr(this);
    s->cases = s->cases->VisitStmtList(this);

    /* Static checking / validation */
    for (WGL_Stmt *c = s->cases->list.First(); c; c = c->Suc())
        if (c->GetType() == WGL_Stmt::Simple)
            switch ((static_cast<WGL_SimpleStmt *>(c))->type)
            {
            case WGL_SimpleStmt::Continue:
            case WGL_SimpleStmt::Discard:
                context->AddError(WGL_Error::INVALID_STMT, UNI_L("expected 'case' or 'default'"));
                break;
            }
       else
           context->AddError(WGL_Error::INVALID_STMT, UNI_L("expected 'case' or 'default'"));

    return s;
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_SimpleStmt *s)
{
    context->SetLineNumber(s->line_number);

    switch (s->type)
    {
    case WGL_SimpleStmt::Empty:
    case WGL_SimpleStmt::Default:
    case WGL_SimpleStmt::Break:
    case WGL_SimpleStmt::Continue:
        /* TODO: eventually track if we are in an appropriate context for these and
           issue warnings if not. */
        break;
    case WGL_SimpleStmt::Discard:
        if (IsVertexShader())
            context->AddError(WGL_Error::ILLEGAL_DISCARD_STMT, UNI_L(""));
        break;
    default:
        OP_ASSERT(!"Unhandled simple statement");
        break;
    }
    return s;
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_BodyStmt *s)
{
    context->SetLineNumber(s->line_number);

    context->EnterLocalScope(NULL);
    s->body = s->body->VisitStmtList(this);
    context->LeaveLocalScope();
    return s;
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_WhileStmt *s)
{
    context->SetLineNumber(s->line_number);

    s->condition = s->condition->VisitExpr(this);
    WGL_Type *cond_type = context->GetValidationState().GetExpressionType(s->condition);
    if (cond_type && !WGL_Type::HasBasicType(cond_type, WGL_BasicType::Bool))
        context->AddError(WGL_Error::INVALID_PREDICATE_TYPE, UNI_L(""));
    s->body = s->body->VisitStmt(this);
    return s;
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_DoStmt *s)
{
    context->SetLineNumber(s->line_number);

    s->body = s->body->VisitStmt(this);
    s->condition = s->condition->VisitExpr(this);
    WGL_Type *cond_type = context->GetValidationState().GetExpressionType(s->condition);
    if (cond_type && !WGL_Type::HasBasicType(cond_type, WGL_BasicType::Bool))
        context->AddError(WGL_Error::INVALID_PREDICATE_TYPE, UNI_L(""));
    return s;
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_IfStmt *s)
{
    context->SetLineNumber(s->line_number);

    s->predicate = s->predicate->VisitExpr(this);
    WGL_Type *pred_type = context->GetValidationState().GetExpressionType(s->predicate);
    if (pred_type && !WGL_Type::HasBasicType(pred_type, WGL_BasicType::Bool))
        context->AddError(WGL_Error::INVALID_IF_PRED, UNI_L(""));

    s->then_part = s->then_part->VisitStmt(this);
    if (s->else_part)
        s->else_part = s->else_part->VisitStmt(this);
    return s;
}

static WGL_Expr::Operator
ReverseOperator(WGL_Expr::Operator op)
{
    switch (op)
    {
    case WGL_Expr::OpLt: return WGL_Expr::OpGt;
    case WGL_Expr::OpLe: return WGL_Expr::OpGe;
    case WGL_Expr::OpEq: return WGL_Expr::OpEq;
    case WGL_Expr::OpNEq: return WGL_Expr::OpNEq;
    case WGL_Expr::OpGe: return WGL_Expr::OpLe;
    case WGL_Expr::OpGt: return WGL_Expr::OpLt;
    default:
        OP_ASSERT(!"Unexpected relational operator");
        return WGL_Expr::OpEq;
    }
}

static BOOL
IsLoopVariableType(WGL_Type *type)
{
    return WGL_Type::HasBasicType(type, WGL_BasicType::Int | WGL_BasicType::Float | WGL_BasicType::UInt);
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_ForStmt *s)
{
    context->SetLineNumber(s->line_number);

    context->EnterLocalScope(NULL);
    s->head = s->head->VisitStmt(this);

    /* GLSL, Appendix A (4 Control flow) */
    WGL_VarName *loop_index = NULL;
    BOOL invalid_loop_declaration = TRUE;
    WGL_Literal *loop_initialiser = NULL;
    switch (s->head->GetType())
    {
    case WGL_Stmt::StmtDecl:
    {
        WGL_DeclStmt *decl = static_cast<WGL_DeclStmt *>(s->head);
        if (WGL_VarDecl *var_decl = decl->decls.First()->GetType() == WGL_Decl::Var ? static_cast<WGL_VarDecl *>(decl->decls.First()) : NULL)
            if (IsLoopVariableType(var_decl->type) && context->GetValidationState().IsConstantExpression(var_decl->initialiser))
            {
                loop_index = var_decl->identifier;
                loop_initialiser = context->GetValidationState().EvalConstExpression(var_decl->initialiser);
                invalid_loop_declaration = decl->decls.First()->Suc() != NULL || loop_initialiser == NULL;
            }
        break;
    }
    case WGL_Stmt::StmtExpr:
    {
        WGL_ExprStmt *expr_stmt = static_cast<WGL_ExprStmt *>(s->head);
        if (expr_stmt->expr->GetType() == WGL_Expr::Assign)
        {
            WGL_AssignExpr *assign_expr = static_cast<WGL_AssignExpr *>(expr_stmt->expr);
            if (assign_expr->op == WGL_Expr::OpAssign && assign_expr->lhs->GetType() == WGL_Expr::Var)
            {
                WGL_VarExpr *var_expr = static_cast<WGL_VarExpr *>(assign_expr->lhs);
                if (WGL_Type *type = context->GetValidationState().LookupLocalVar(var_expr->name))
                    if (IsLoopVariableType(type) && context->GetValidationState().IsConstantExpression(assign_expr->rhs))
                    {
                        loop_index = var_expr->name;
                        loop_initialiser = context->GetValidationState().EvalConstExpression(assign_expr->rhs);
                        invalid_loop_declaration = loop_initialiser == NULL;
                    }
            }
        }
        break;
    }
    default:
        break;
    }
    if (invalid_loop_declaration)
        context->AddError(WGL_Error::INVALID_LOOP_HEADER, loop_index ? Storage(context, loop_index) : UNI_L("for"));

    s->predicate = s->predicate->VisitExpr(this);
    if (s->update)
        s->update = s->update->VisitExpr(this);
    if (WGL_VarBinding *bind = loop_index ? context->GetValidationState().LookupVarBinding(loop_index) : NULL)
    {
        bind->is_read_only = TRUE;
        bind->value = NULL;
    }
    s->body = s->body->VisitStmt(this);
    context->LeaveLocalScope();

    BOOL invalid_loop_condition = TRUE;
    WGL_Expr::Operator boundary_op = WGL_Expr::OpBase;
    WGL_Literal *loop_limit = NULL;
    if (WGL_BinaryExpr *condition = s->predicate->GetType() == WGL_Expr::Binary ? static_cast<WGL_BinaryExpr *>(s->predicate) : NULL)
        if (condition->op >= WGL_Expr::OpEq && condition->op <= WGL_Expr::OpGe)
            if (WGL_VarExpr *var_expr = condition->arg1->GetType() == WGL_Expr::Var ? static_cast<WGL_VarExpr *>(condition->arg1) : NULL)
            {
                if (loop_index && uni_strcmp(Storage(context, var_expr->name), Storage(context, loop_index)) == 0)
                {
                    if (context->GetValidationState().IsConstantExpression(condition->arg2))
                    {
                        boundary_op = condition->op;
                        loop_limit = context->GetValidationState().EvalConstExpression(condition->arg2);
                        invalid_loop_condition = FALSE;
                    }
                }
            }
            else if (WGL_VarExpr *var_expr = condition->arg2->GetType() == WGL_Expr::Var ? static_cast<WGL_VarExpr *>(condition->arg2) : NULL)
                if (loop_index && uni_strcmp(Storage(context, var_expr->name), Storage(context, loop_index)) == 0)
                    if (context->GetValidationState().IsConstantExpression(condition->arg1))
                    {
                        boundary_op = ReverseOperator(condition->op);
                        loop_limit = context->GetValidationState().EvalConstExpression(condition->arg1);
                        invalid_loop_condition = FALSE;
                    }

    if (invalid_loop_condition)
        context->AddError(WGL_Error::INVALID_LOOP_CONDITION, loop_index ? Storage(context, loop_index) : UNI_L(""));

    if (!s->update)
    {
        context->AddError(WGL_Error::MISSING_LOOP_UPDATE, loop_index ? Storage(context, loop_index) : UNI_L(""));
        return s;
    }

    BOOL invalid_update = TRUE;
    double update_delta = 0;
    WGL_Error::Type update_error = WGL_Error::INVALID_LOOP_UPDATE;
    if (WGL_PostExpr *e = s->update->GetType() == WGL_Expr::PostOp ? static_cast<WGL_PostExpr *>(s->update) : NULL)
    {
        if (e->op == WGL_Expr::OpPostInc || e->op == WGL_Expr::OpPostDec)
            if (WGL_VarExpr *var_expr = e->arg->GetType() == WGL_Expr::Var ? static_cast<WGL_VarExpr *>(e->arg) : NULL)
                if (loop_index && uni_strcmp(Storage(context, loop_index), Storage(context, var_expr->name)) == 0)
                {
                    update_delta = e->op == WGL_Expr::OpPostInc ? 1.0 : (-1.0);
                    invalid_update = FALSE;
                }
    }
    else if (WGL_UnaryExpr *e = s->update->GetType() == WGL_Expr::Unary ? static_cast<WGL_UnaryExpr *>(s->update) : NULL)
    {
        if (e->op == WGL_Expr::OpPreInc || e->op == WGL_Expr::OpPreDec)
            if (WGL_VarExpr *var_expr = e->arg->GetType() == WGL_Expr::Var ? static_cast<WGL_VarExpr *>(e->arg) : NULL)
                if (loop_index && uni_strcmp(Storage(context, loop_index), Storage(context, var_expr->name)) == 0)
                {
                    update_delta = e->op == WGL_Expr::OpPreInc ? 1.0 : (-1.0);
                    invalid_update = FALSE;
                }
    }
    else if (WGL_AssignExpr *assign_expr = s->update->GetType() == WGL_Expr::Assign ? static_cast<WGL_AssignExpr *>(s->update) : NULL)
    {
        if (assign_expr->op == WGL_Expr::OpAdd || assign_expr->op == WGL_Expr::OpSub)
            if (WGL_VarExpr *var_expr = assign_expr->lhs->GetType() == WGL_Expr::Var ? static_cast<WGL_VarExpr *>(assign_expr->lhs) : NULL)
                if (loop_index && uni_str_eq(Storage(context, loop_index), Storage(context, var_expr->name)))
                    if (context->GetValidationState().IsConstantExpression(assign_expr->rhs))
                    {
                        if (WGL_Literal *l = context->GetValidationState().EvalConstExpression(assign_expr->rhs))
                        {
                            if (WGL_Type::HasBasicType(assign_expr->lhs->GetExprType(), WGL_BasicType::Float))
                            {
                                WGL_Literal::Value lv = WGL_Literal::ToDouble(l->type, l->value);
                                update_delta = assign_expr->op == WGL_Expr::OpAdd ? lv.d.value : (-lv.d.value);
                            }
                            else
                            {
                                WGL_Literal::Value lv = WGL_Literal::ToInt(l->type, l->value);
                                update_delta = assign_expr->op == WGL_Expr::OpAdd ? lv.i : (-lv.i);
                            }
                            if (update_delta != 0)
                                invalid_update = FALSE;
                            else
                                update_error = WGL_Error::INVALID_LOOP_NONTERMINATION;
                        }
                    }
    }

    if (invalid_update)
    {
        context->AddError(update_error, loop_index ? Storage(context, loop_index) : UNI_L(""));
        return s;
    }

    if (loop_limit && loop_initialiser)
    {
        WGL_Literal::Value c1 = WGL_Literal::ToDouble(loop_initialiser->type, loop_initialiser->value);
        WGL_Literal::Value c2 = WGL_Literal::ToDouble(loop_limit->type, loop_limit->value);
        BOOL is_illegal = TRUE;
        if (c1.d.value < c2.d.value)
        {
            switch (boundary_op)
            {
            case WGL_Expr::OpLt:
            case WGL_Expr::OpLe:
            case WGL_Expr::OpNEq:
                is_illegal = update_delta <= 0;
                break;
            case WGL_Expr::OpEq:
            case WGL_Expr::OpGe:
            case WGL_Expr::OpGt:
                is_illegal = FALSE;
                break;
            default:
                /* Unknown operator, treat as illegal. */
                break;
            }
        }
        else if (c1.d.value == c2.d.value)
        {
            switch (boundary_op)
            {
            case WGL_Expr::OpLt:
            case WGL_Expr::OpGt:
            case WGL_Expr::OpNEq:
                is_illegal = FALSE;
                break;
            case WGL_Expr::OpLe:
            case WGL_Expr::OpEq:
            case WGL_Expr::OpGe:
                is_illegal = update_delta == 0;
                break;
            default:
                /* Unknown operator, treat as illegal. */
                break;
            }
        }
        else /* c1.d.value > c2.d.value */
        {
            switch (boundary_op)
            {
            case WGL_Expr::OpLt:
            case WGL_Expr::OpLe:
            case WGL_Expr::OpEq:
                is_illegal = FALSE;
                break;
            case WGL_Expr::OpGe:
            case WGL_Expr::OpGt:
            case WGL_Expr::OpNEq:
                is_illegal = update_delta >= 0;
            default:
                /* Unknown operator, treat as illegal. */
                break;
            }
        }
        if (is_illegal)
            context->AddError(WGL_Error::INVALID_LOOP_NONTERMINATION, loop_index ? Storage(context, loop_index) : UNI_L(""));
    }
    return s;
}

WGL_Literal *
WGL_ValidationState::EvalConstExpression(WGL_Expr::Operator op, WGL_Literal::Type t1, WGL_Literal::Value v1, WGL_Literal::Type t2, WGL_Literal::Value v2)
{
    if (t1 == t2)
    {
        switch (t1)
        {
        case WGL_Literal::Double:
        {
            WGL_Literal *n = context->BuildLiteral(WGL_Literal::Double);
            switch (op)
            {
            case WGL_Expr::OpMul:
                n->value.d.value = v1.d.value * v2.d.value;
                break;
            case WGL_Expr::OpDiv:
                n->value.d.value = v1.d.value / v2.d.value;
                break;
            case WGL_Expr::OpAdd:
                n->value.d.value = v1.d.value + v2.d.value;
                break;
            case WGL_Expr::OpSub:
                n->value.d.value = v1.d.value - v2.d.value;
                break;
            case WGL_Expr::OpLt:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.d.value < v2.d.value;
                break;
            case WGL_Expr::OpLe:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.d.value <= v2.d.value;
                break;
            case WGL_Expr::OpEq:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.d.value == v2.d.value;
                break;
            case WGL_Expr::OpNEq:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.d.value != v2.d.value;
                break;
            case WGL_Expr::OpGe:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.d.value >= v2.d.value;
                break;
            case WGL_Expr::OpGt:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.d.value > v2.d.value;
                break;
            case WGL_Expr::OpAnd:
                n->type = WGL_Literal::Bool;
                n->value.b = static_cast<unsigned>(v1.d.value) && static_cast<unsigned>(v2.d.value);
                break;
            case WGL_Expr::OpXor:
                n->type = WGL_Literal::Bool;
                n->value.b = (static_cast<unsigned>(v1.d.value) || static_cast<unsigned>(v2.d.value)) && !(static_cast<unsigned>(v1.d.value) && static_cast<unsigned>(v2.d.value));
                break;
            case WGL_Expr::OpOr:
                n->type = WGL_Literal::Bool;
                n->value.b = static_cast<unsigned>(v1.d.value) || static_cast<unsigned>(v2.d.value);
                break;
            }
            return n;
        }
        case WGL_Literal::Int:
        {
            WGL_Literal *n = context->BuildLiteral(WGL_Literal::Int);
            switch (op)
            {
            case WGL_Expr::OpMul:
                n->value.i = v1.i * v2.i;
                break;
            case WGL_Expr::OpDiv:
                n->value.i = (v2.i != 0) ? (v1.i / v2.i) : 0;
                break;
            case WGL_Expr::OpAdd:
                n->value.i = v1.i + v2.i;
                break;
            case WGL_Expr::OpSub:
                n->value.i = v1.i - v2.i;
                break;
            case WGL_Expr::OpLt:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.i < v2.i;
                break;
            case WGL_Expr::OpLe:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.i <= v2.i;
                break;
            case WGL_Expr::OpEq:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.i == v2.i;
                break;
            case WGL_Expr::OpNEq:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.i != v2.i;
                break;
            case WGL_Expr::OpGe:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.i >= v2.i;
                break;
            case WGL_Expr::OpGt:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.i > v2.i;
                break;
            case WGL_Expr::OpAnd:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.i && v2.i;
                break;
            case WGL_Expr::OpXor:
                n->type = WGL_Literal::Bool;
                n->value.b = (v1.i || v2.i) && !(v1.i && v2.i);
                break;
            case WGL_Expr::OpOr:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.i || v2.i;
                break;
            }
            return n;
        }
        case WGL_Literal::UInt:
        {
            WGL_Literal *n = context->BuildLiteral(WGL_Literal::UInt);
            switch (op)
            {
            case WGL_Expr::OpMul:
                n->value.u_int = v1.u_int * v2.u_int;
                break;
            case WGL_Expr::OpDiv:
                n->value.u_int = v2.u_int != 0 ? (v1.u_int / v2.u_int) : 0;
                break;
            case WGL_Expr::OpAdd:
                n->value.u_int = v1.u_int + v2.u_int;
                break;
            case WGL_Expr::OpSub:
                n->value.u_int = v1.u_int - v2.u_int;
                break;
            case WGL_Expr::OpLt:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.u_int < v2.u_int;
                break;
            case WGL_Expr::OpLe:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.u_int <= v2.u_int;
                break;
            case WGL_Expr::OpEq:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.u_int == v2.u_int;
                break;
            case WGL_Expr::OpNEq:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.u_int != v2.u_int;
                break;
            case WGL_Expr::OpGe:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.u_int >= v2.u_int;
                break;
            case WGL_Expr::OpGt:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.u_int > v2.u_int;
                break;
            case WGL_Expr::OpAnd:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.u_int && v2.u_int;
                break;
            case WGL_Expr::OpXor:
                n->type = WGL_Literal::Bool;
                n->value.b = (v1.u_int || v2.u_int) && !(v1.u_int && v2.u_int);
                break;
            case WGL_Expr::OpOr:
                n->type = WGL_Literal::Bool;
                n->value.b = v1.u_int || v2.u_int;
                break;
            }
            return n;
        }
        case WGL_Literal::Bool:
        {
            WGL_Literal *n = context->BuildLiteral(WGL_Literal::Bool);
            switch (op)
            {
            case WGL_Expr::OpMul:
                n->value.b = v1.b && v2.b;
                break;
            case WGL_Expr::OpDiv:
                n->value.b = v1.b && v2.b;
                break;
            case WGL_Expr::OpAdd:
                n->value.b = v1.b || v2.b;
                break;
            case WGL_Expr::OpSub:
                n->value.b = v1.b || v2.b;
                break;
            case WGL_Expr::OpLt:
                n->value.b = v1.b < v2.b;
                break;
            case WGL_Expr::OpLe:
                n->value.b = v1.b <= v2.b;
                break;
            case WGL_Expr::OpEq:
                n->value.b = v1.b == v2.b;
                break;
            case WGL_Expr::OpNEq:
                n->value.b = v1.b != v2.b;
                break;
            case WGL_Expr::OpGe:
                n->value.b = v1.b >= v2.b;
                break;
            case WGL_Expr::OpGt:
                n->value.b = v1.b > v2.b;
                break;
            case WGL_Expr::OpAnd:
                n->value.b = v1.b && v2.b;
                break;
            case WGL_Expr::OpXor:
                n->value.b = (v1.b || v2.b) && !(v1.b && v2.b);
                break;
            case WGL_Expr::OpOr:
                n->value.b = v1.b || v2.b;
                break;
            }
            return n;
        }
        default:
            OP_ASSERT(0);
        case WGL_Literal::String:
            /* The language does not support them, so little point padding this out.. */
            return NULL;
        }
    }
    else if (t1 == WGL_Literal::Double || t2 == WGL_Literal::Double)
        return EvalConstExpression(op, WGL_Literal::Double, WGL_Literal::ToDouble(t1, v1), WGL_Literal::Double, WGL_Literal::ToDouble(t2, v2));
    else if (t1 == WGL_Literal::UInt || t2 == WGL_Literal::UInt)
        return EvalConstExpression(op, WGL_Literal::UInt, WGL_Literal::ToUnsignedInt(t1, v1), WGL_Literal::UInt, WGL_Literal::ToUnsignedInt(t2, v2));
    else if (t1 == WGL_Literal::Int || t2 == WGL_Literal::Int)
        return EvalConstExpression(op, WGL_Literal::Int, WGL_Literal::ToInt(t1, v1), WGL_Literal::Int, WGL_Literal::ToInt(t2, v2));
    else if (t1 == WGL_Literal::Bool || t2 == WGL_Literal::Bool)
        return EvalConstExpression(op, WGL_Literal::Bool, WGL_Literal::ToBool(t1, v1), WGL_Literal::Bool, WGL_Literal::ToBool(t2, v2));
    else
        return NULL;
}

WGL_BuiltinFun *
WGL_ValidationState::ResolveBuiltinFunction(List<WGL_Builtin> *builtins, WGL_ExprList *args)
{
    for (WGL_Builtin *builtin = builtins->First(); builtin; builtin = builtin->Suc())
    {
        if (builtin->IsBuiltinVar())
            continue;

        WGL_BuiltinFun *fun = static_cast<WGL_BuiltinFun *>(builtin);

        if ((!args || args->list.Empty()) && fun->parameters.Empty())
            return fun;
        else if (args->list.Cardinal() == fun->parameters.Cardinal())
        {
            WGL_Expr *arg = args->list.First();
            BOOL decl_match = TRUE;
            for (WGL_BuiltinType *p = fun->parameters.First(); p; p = p->Suc())
            {
                WGL_Type *arg_type = GetExpressionType(arg);
                if (!arg_type)
                    return NULL;
                if (arg_type && !MatchesBuiltinType(p, arg_type))
                {
                    decl_match = FALSE;
                    break;
                }
                arg = arg->Suc();
            }
            if (decl_match)
                return fun;
        }
    }
    return NULL;
}

WGL_Type *
WGL_ValidationState::ResolveBuiltinReturnType(WGL_BuiltinFun *fun, WGL_ExprList *args, WGL_Type *arg1_type)
{
    /* Concrete type; return type is derivable without considering
       argument types. */
    if (fun->return_type->type <= WGL_BuiltinType::Sampler)
        return FromBuiltinType(fun->return_type);

    /* Resolve the instance type of a generic builtin return type.
       Generically-typed builtins are mostly regular in that the
       return type is equal to that of the argument type(s).

       The relational intrinsics over vectors requires special
       treatment; all having bool vector types (of magnitude
       equal to the argument vectors.)
    */
    switch (fun->intrinsic)
    {
    case WGL_GLSL::LessThan:
    case WGL_GLSL::LessThanEqual:
    case WGL_GLSL::GreaterThan:
    case WGL_GLSL::GreaterThanEqual:
    case WGL_GLSL::Equal:
    case WGL_GLSL::NotEqual:
    case WGL_GLSL::Any:
    case WGL_GLSL::All:
    case WGL_GLSL::Not:
    {
        OP_ASSERT(arg1_type->GetType() == WGL_Type::Vector);
        unsigned magnitude = WGL_VectorType::GetMagnitude(static_cast<WGL_VectorType *>(arg1_type));

        WGL_VectorType::Type return_vector;
        if (magnitude == 2)
            return_vector = WGL_VectorType::BVec2;
        else if (magnitude == 3)
            return_vector = WGL_VectorType::BVec3;
        else
        {
            OP_ASSERT(magnitude == 4);
            return_vector = WGL_VectorType::BVec4;
        }
        return context->GetASTBuilder()->BuildVectorType(return_vector);
    }
    default:
        return arg1_type;
    }
}

#define GLSL_ABS(x) ((x) >= 0.0 ? (x) : (-(x)))
#define GLSL_SIGN(x) ((x) > 0 ? 1.0 : (x) == 0 ? 0 : (-1.0))

WGL_Literal *
WGL_ValidationState::EvalBuiltinFunction(WGL_VarExpr *fun, List<WGL_Builtin> *builtins, WGL_ExprList *args)
{
    if (WGL_BuiltinFun *builtin = ResolveBuiltinFunction(builtins, args))
    {
        WGL_BuiltinType *arg1 = builtin->parameters.First();
        WGL_BuiltinType *arg2 = arg1 ? arg1->Suc() : NULL;

        WGL_Literal *l = EvalConstExpression(args->list.First());
        if (!l)
            return NULL;
        if (builtin->arity == 1 && builtin->return_type->type == WGL_BuiltinType::Gen && arg1->type == WGL_BuiltinType::Gen)
        {
            switch (builtin->intrinsic)
            {
            case WGL_GLSL::Sign:
            case WGL_GLSL::Abs:
                break;
            default:
                return NULL;
            }
            switch (l->type)
            {
            case WGL_Literal::Double:
            {
                WGL_Literal *result = context->BuildLiteral(l->type);
                switch (builtin->intrinsic)
                {
                case WGL_GLSL::Sign:
                    result->value.d.value = GLSL_SIGN(l->value.d.value);
                    break;
                case WGL_GLSL::Abs:
                    result->value.d.value = GLSL_ABS(l->value.d.value);
                    break;
                default:
                    return NULL;
                }
                return result;
            }
            case WGL_Literal::Vector:
            {
                WGL_LiteralVector *lvec = static_cast<WGL_LiteralVector *>(l);
                WGL_LiteralVector *lit_vector = OP_NEWGRO_L(WGL_LiteralVector, (WGL_BasicType::Float), context->Arena());
                lit_vector->size = lvec->size;
                lit_vector->value.doubles = OP_NEWGROA_L(double, lit_vector->size, context->Arena());
                for (unsigned i = 0; i < lit_vector->size ; i++)
                {
                    switch (builtin->intrinsic)
                    {
                    case WGL_GLSL::Sign:
                        lit_vector->value.doubles[i] = GLSL_SIGN(lvec->value.doubles[i]);
                        break;
                    case WGL_GLSL::Abs:
                        lit_vector->value.doubles[i] = GLSL_ABS(lvec->value.doubles[i]);
                        break;
                    default:
                        return NULL;
                    }
                }
                return lit_vector;
            }
            default:
                OP_ASSERT(!"A curious builtin application");
                return NULL;
            }
        }
        else if (builtin->arity == 2 && builtin->return_type->type == WGL_BuiltinType::Gen && arg1->type == WGL_BuiltinType::Gen && (arg2->type == WGL_BuiltinType::Gen || arg2->type == WGL_BuiltinType::Basic && arg2->value.basic_type == WGL_BasicType::Float))
        {
            switch (builtin->intrinsic)
            {
            case WGL_GLSL::Max:
            case WGL_GLSL::Min:
                break;
            default:
                return NULL;
            }
            switch (l->type)
            {
            case WGL_Literal::Double:
            {
                /* Know that at least one argument is constant. */
                WGL_Literal *l1 = EvalConstExpression(args->list.First());
                WGL_Literal *l2 = EvalConstExpression(args->list.First()->Suc());
                double d1 = (-1);
                double d2 = (-1);
                BOOL success1 = LiteralToDouble(l1, d1);
                BOOL success2 = LiteralToDouble(l2, d2);
                WGL_Literal *result = context->BuildLiteral(l->type);
                switch (builtin->intrinsic)
                {
                case WGL_GLSL::Max:
                    if (success1 && success2)
                        result->value.d.value = MAX(d1, d2);
                    else if (success1)
                        result->value.d.value = d1;
                    else
                        result->value.d.value = d2;
                    return result;
                case WGL_GLSL::Min:
                    if (success1 && success2)
                        result->value.d.value = MIN(d1, d2);
                    else if (success1)
                        result->value.d.value = d1;
                    else
                        result->value.d.value = d2;
                    return result;
                default:
                    return NULL;
                }
            }
            default:
                return NULL;
            }
        }
        else
            return NULL;
    }
    else
        return NULL;
}

WGL_Literal *
WGL_ValidationState::EvalConstExpression(WGL_Expr *e)
{
    switch (e->GetType())
    {
    case WGL_Expr::Lit:
        return static_cast<WGL_LiteralExpr *>(e)->literal;
    case WGL_Expr::TypeConstructor:
        OP_ASSERT(!"Unexpected occurrence of a type constructor");
        return NULL;
    case WGL_Expr::Var:
        if (WGL_VarBinding *binder = LookupVarBinding(static_cast<WGL_VarExpr *>(e)->name))
        {
            if (binder->value)
                return EvalConstExpression(binder->value);
        }
        return NULL;
    case WGL_Expr::Call:
    {
        WGL_CallExpr *fun_call = static_cast<WGL_CallExpr *>(e);
#if 0
        for (WGL_Expr *arg = fun_call->args ? fun_call->args->list.First() : NULL; arg; arg = arg->Suc())
            if (!IsConstantExpression(arg))
                return NULL;
#endif
        if (fun_call->fun->GetType() == WGL_Expr::Var && fun_call->args && fun_call->args->list.Cardinal() > 0)
        {
            WGL_VarExpr *f = static_cast<WGL_VarExpr *>(fun_call->fun);
            if (List <WGL_Builtin> *builtins = LookupBuiltin(f->name))
                return EvalBuiltinFunction(f, builtins, fun_call->args);
            else
                /* This implementation does not support inlining & evaluation of local (const) functions. */
                return NULL;
        }
        else if (fun_call->fun->GetType() == WGL_Expr::TypeConstructor && fun_call->args && fun_call->args->list.Cardinal() > 0)
        {
            WGL_TypeConstructorExpr *tycon_expr = static_cast<WGL_TypeConstructorExpr *>(fun_call->fun);
            switch (tycon_expr->constructor->GetType())
            {
            case WGL_Type::Basic:
            {
                WGL_BasicType *basic_type = static_cast<WGL_BasicType *>(tycon_expr->constructor);
                WGL_Literal *l = EvalConstExpression(fun_call->args->list.First());
                WGL_Literal *result = context->BuildLiteral(WGL_BasicType::ToLiteralType(basic_type->type));
                switch (basic_type->type)
                {
                case WGL_BasicType::Float:
                {
                    double val;
                    if (result && LiteralToDouble(l, val))
                    {
                        result->value.d.value = val;
                        return result;
                    }
                    break;
                }
                case WGL_BasicType::Int:
                case WGL_BasicType::UInt:
                case WGL_BasicType::Bool:
                {
                    int val;
                    if (result && LiteralToInt(l, val))
                    {
                        switch (basic_type->type)
                        {
                        case WGL_BasicType::UInt:
                            result->value.u_int = static_cast<unsigned>(val);
                            return result;
                        case WGL_BasicType::Bool:
                            result->value.b = val != 0;
                        case WGL_BasicType::Int:
                        default:
                            result->value.i = val;
                            return result;
                        }
                    }
                    break;
                }
                }
                return NULL;
            }
            case WGL_Type::Vector:
            {
                WGL_VectorType *vec_type = static_cast<WGL_VectorType *>(tycon_expr->constructor);
                WGL_BasicType::Type element_type = WGL_VectorType::GetElementType(vec_type);
                WGL_LiteralVector *lit_vector = OP_NEWGRO_L(WGL_LiteralVector, (element_type), context->Arena());
                lit_vector->size = fun_call->args->list.Cardinal();
                switch (element_type)
                {
                case WGL_BasicType::Float:
                    lit_vector->value.doubles = OP_NEWGROA_L(double, lit_vector->size, context->Arena());
                    break;
                case WGL_BasicType::Int:
                    lit_vector->value.ints = OP_NEWGROA_L(int, lit_vector->size, context->Arena());
                    break;
                case WGL_BasicType::UInt:
                    lit_vector->value.u_ints = OP_NEWGROA_L(unsigned, lit_vector->size, context->Arena());
                    break;
                case WGL_BasicType::Bool:
                    lit_vector->value.bools = OP_NEWGROA_L(bool, lit_vector->size, context->Arena());
                    break;
                }

                WGL_Expr *arg = fun_call->args->list.First();
                WGL_Literal *arg_lit = EvalConstExpression(arg);
                unsigned arg_idx = 0;
                for (unsigned i = 0; i < lit_vector->size; i++)
                {
                    if (arg_lit->type == WGL_Literal::Vector)
                    {
                        WGL_LiteralVector *lit_vec = static_cast<WGL_LiteralVector *>(arg_lit);
                        if (arg_idx < lit_vec->size)
                        {
                            switch (element_type)
                            {
                            case WGL_BasicType::Float:
                                lit_vector->value.doubles[i] = lit_vec->value.doubles[arg_idx++];
                                break;
                            case WGL_BasicType::Int:
                                lit_vector->value.ints[i] = lit_vec->value.ints[arg_idx++];
                                break;
                            case WGL_BasicType::UInt:
                                lit_vector->value.u_ints[i] = lit_vec->value.u_ints[arg_idx++];
                                break;
                            case WGL_BasicType::Bool:
                                lit_vector->value.bools[i] = lit_vec->value.bools[arg_idx++];
                                break;
                            }
                        }
                        if (arg_idx >= lit_vec->size)
                        {
                            arg_idx = 0;
                            if (arg)
                                arg = arg->Suc();
                            if (arg)
                                arg_lit = EvalConstExpression(arg);
                        }
                    }
                    else if (element_type == WGL_BasicType::Float)
                    {
                        double d;
                        if (LiteralToDouble(arg_lit, d))
                            lit_vector->value.doubles[i] = d;

                        if (arg)
                            arg = arg->Suc();
                        if (arg)
                            arg_lit = EvalConstExpression(arg);
                    }
                    else
                    {
                        int i;
                        if (LiteralToInt(arg_lit, i))
                        {
                            switch (element_type)
                            {
                            case WGL_BasicType::Int:
                                lit_vector->value.ints[i] = i;
                                break;
                            case WGL_BasicType::UInt:
                                lit_vector->value.u_ints[i] = static_cast<unsigned>(i);
                                break;
                            case WGL_BasicType::Bool:
                                lit_vector->value.bools[i] = i != 0;
                                break;
                            default:
                                OP_ASSERT(!"The 'impossible' happened, unexpected element type.");
                                return NULL;
                            }
                        }
                        if (arg)
                            arg = arg->Suc();
                        if (arg)
                            arg_lit = EvalConstExpression(arg);
                    }
                }
            }
            case WGL_Type::Matrix:
                OP_ASSERT(!"Support for (constant) matrix constructors not in place; a reminder..");
            case WGL_Type::Array:
                /* Language will have to be extended first to support array literals.. */
            default:
                return NULL;
            }
        }
        else
            return NULL;
    }
    case WGL_Expr::Select:
        /* TODO: track constant structures / fields with known values. */
        return NULL;
    case WGL_Expr::Index:
    {
        WGL_IndexExpr *index_expr = static_cast<WGL_IndexExpr *>(e);
        if (index_expr->array->GetType() == WGL_Expr::Var)
        {
            WGL_VarExpr *array_var = static_cast<WGL_VarExpr *>(index_expr->array);
            if (WGL_VarBinding *binder = LookupVarBinding(array_var->name))
                if (binder->value && IsConstantExpression(binder->value))
                {
                    WGL_Literal *idx = EvalConstExpression(index_expr->index);
                    int idx_val = -1;
                    if (LiteralToInt(idx, idx_val))
                    {
                        switch (binder->type->GetType())
                        {
                        case WGL_Type::Vector:
                        {
                            WGL_Literal *c_array = EvalConstExpression(binder->value);
                            OP_ASSERT(c_array->type == WGL_Literal::Vector);
                            WGL_LiteralVector *cvec = static_cast<WGL_LiteralVector *>(c_array);
                            if (static_cast<unsigned>(idx_val) < cvec->size)
                            {
                                WGL_Literal *l = context->BuildLiteral(WGL_BasicType::ToLiteralType(cvec->element_type));
                                if (l)
                                {
                                    switch (l->type)
                                    {
                                    case WGL_Literal::Double:
                                        l->value.d.value = cvec->value.doubles[idx_val];
                                        return l;
                                    case WGL_Literal::Int:
                                        l->value.i = cvec->value.ints[idx_val];
                                        return l;
                                    case WGL_Literal::UInt:
                                        l->value.u_int = cvec->value.u_ints[idx_val];
                                        return l;
                                    case WGL_Literal::Bool:
                                        l->value.b = cvec->value.bools[idx_val];
                                        return l;
                                    default:
                                        return NULL;
                                    }
                                }
                            }
                            return NULL;
                        }
                        case WGL_Type::Array:
                        {
                            WGL_Literal *c_array = EvalConstExpression(binder->value);
                            OP_ASSERT(c_array->type == WGL_Literal::Array);
                            WGL_LiteralArray *carr = static_cast<WGL_LiteralArray *>(c_array);
                            if (static_cast<unsigned>(idx_val) < carr->size)
                            {
                                WGL_Literal *l = context->BuildLiteral(WGL_BasicType::ToLiteralType(carr->element_type));
                                if (l)
                                {
                                    switch (l->type)
                                    {
                                    case WGL_Literal::Double:
                                        l->value.d.value = carr->value.doubles[idx_val];
                                        return l;
                                    case WGL_Literal::Int:
                                        l->value.i = carr->value.ints[idx_val];
                                        return l;
                                    case WGL_Literal::UInt:
                                        l->value.u_int = carr->value.u_ints[idx_val];
                                        return l;
                                    case WGL_Literal::Bool:
                                        l->value.b = carr->value.bools[idx_val];
                                        return l;
                                    default:
                                        return NULL;
                                    }
                                }
                            }
                            return NULL;
                        }
                        default:
                            return NULL;
                        }
                    }
                }
        }
        return NULL;
    }
    case WGL_Expr::Assign:
        return NULL;
    case WGL_Expr::PostOp:
    {
        WGL_PostExpr *post_expr = static_cast<WGL_PostExpr *>(e);
        return EvalConstExpression(post_expr->arg);
    }
    case WGL_Expr::Seq:
    {
        WGL_SeqExpr *post_expr = static_cast<WGL_SeqExpr *>(e);
        return EvalConstExpression(post_expr->arg2);
    }
    case WGL_Expr::Cond:
    {
        WGL_CondExpr *cond_expr = static_cast<WGL_CondExpr *>(e);

        if (WGL_Literal *cond = EvalConstExpression(cond_expr->arg1))
        {
            WGL_Literal::Value v = WGL_Literal::ToBool(cond->type, cond->value);
            return v.b ? EvalConstExpression(cond_expr->arg2) : EvalConstExpression(cond_expr->arg3);
        }
        else
            return NULL;
    }
    case WGL_Expr::Binary:
    {
        WGL_BinaryExpr *bin_expr = static_cast<WGL_BinaryExpr *>(e);
        if (WGL_Literal *l1 = EvalConstExpression(bin_expr->arg1))
            if (WGL_Literal *l2 = EvalConstExpression(bin_expr->arg2))
                return EvalConstExpression(bin_expr->op, l1->type, l1->value, l2->type, l2->value);

        return NULL;
    }

    case WGL_Expr::Unary:
    {
        WGL_UnaryExpr *un_expr = static_cast<WGL_UnaryExpr *>(e);
        WGL_Literal *l = EvalConstExpression(un_expr->arg);

        switch (un_expr->op)
        {
        case WGL_Expr::OpAdd:
            return l;
        case WGL_Expr::OpNot:
        {
            WGL_Literal *n = context->BuildLiteral(WGL_Literal::Bool);

            switch (l->type)
            {
            case WGL_Literal::Double:
                n->value.b = l->value.d.value == 0;
                break;
            case WGL_Literal::Int:
                n->value.b = l->value.i == 0;
                break;
            case WGL_Literal::UInt:
                n->value.b = l->value.u_int == 0;
                break;
            case WGL_Literal::Bool:
                n->value.b = l->value.b == FALSE;
                break;
            case WGL_Literal::String:
                n->value.b = l->value.s == NULL;
                break;
            }
            return n;
        }
        case WGL_Expr::OpNegate:
            switch (l->type)
            {
            case WGL_Literal::Double:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Double);

                n->value.d.value = -l->value.d.value;
                return n;
            }
            case WGL_Literal::Int:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Int);

                n->value.i = -l->value.i;
                return n;
            }
            case WGL_Literal::UInt:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Int);

                n->value.i = - static_cast<int>(l->value.u_int);
                return n;
            }
            case WGL_Literal::Bool:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Bool);

                n->value.b = l->value.b == FALSE;
                return n;
            }
            case WGL_Literal::String:
                return l;
            }
        case WGL_Expr::OpPreInc:
            switch (l->type)
            {
            case WGL_Literal::Double:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Double);

                n->value.d.value = l->value.d.value + 1;
                return n;
            }
            case WGL_Literal::Int:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Int);

                n->value.i = l->value.i + 1;
                return n;
            }
            case WGL_Literal::UInt:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::UInt);

                n->value.u_int = l->value.u_int + 1;
                return n;
            }
            case WGL_Literal::Bool:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Bool);

                n->value.b = !l->value.b;
                return n;
            }
            case WGL_Literal::String:
                return l;
            }
        case WGL_Expr::OpPreDec:
            switch (l->type)
            {
            case WGL_Literal::Double:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Double);

                n->value.d.value = l->value.d.value + 1;
                return n;
            }
            case WGL_Literal::Int:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Int);

                n->value.i = l->value.i + 1;
                return n;
            }
            case WGL_Literal::UInt:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::UInt);

                n->value.u_int = l->value.u_int + 1;
                return n;
            }
            case WGL_Literal::Bool:
            {
                WGL_Literal *n = context->BuildLiteral(WGL_Literal::Bool);

                n->value.b = !l->value.b;
                return n;
            }
            case WGL_Literal::String:
                return l;
            }
        }
    }
    default:
        OP_ASSERT(!"Unhandled expression type");
        return NULL;
    }
}

BOOL
WGL_ValidationState::LiteralToInt(WGL_Literal *lit, int &val)
{
    if (!lit)
        return FALSE;

    switch (lit->type)
    {
    case WGL_Literal::Double:
        if (!op_isfinite(lit->value.d.value))
            return FALSE;
        else if (lit->value.d.value >= INT_MIN && lit->value.d.value <= INT_MAX)
        {
            val = static_cast<int>(lit->value.d.value);
            return TRUE;
        }
        else
            return FALSE;

    case WGL_Literal::Int:
    case WGL_Literal::UInt:
        val = lit->value.i;
        return TRUE;

    case WGL_Literal::Bool:
        val = lit->value.b ? 1 : 0;
        return TRUE;

    default:
        return FALSE;
    }
}

BOOL
WGL_ValidationState::LiteralToDouble(WGL_Literal *lit, double &val)
{
    if (!lit)
        return FALSE;

    switch (lit->type)
    {
    case WGL_Literal::Double:
        val = lit->value.d.value;
        return TRUE;
    case WGL_Literal::Int:
    case WGL_Literal::UInt:
        val = lit->value.i;
        return TRUE;

    case WGL_Literal::Bool:
        val = lit->value.b ? 1 : 0;
        return TRUE;

    default:
        return FALSE;
    }
}

/* static */ BOOL
WGL_ValidationState::IsSameType(WGL_Type *t1, WGL_Type *t2, BOOL type_equality)
{
    if (t1 == t2)
        return TRUE;

    if (t1->GetType() != t2->GetType())
        return FALSE;

    switch (t1->GetType())
    {
    case WGL_Type::Basic:
        if (type_equality)
            return (static_cast<WGL_BasicType *>(t1)->type == static_cast<WGL_BasicType *>(t2)->type);
        else
        {
            WGL_BasicType *b1 = static_cast<WGL_BasicType *>(t1);
            WGL_BasicType *b2 = static_cast<WGL_BasicType *>(t2);
            if (b1->type == WGL_BasicType::Void || b2->type == WGL_BasicType::Void)
                return FALSE;
            else
                return TRUE;
        }
    case WGL_Type::Vector:
    {
        WGL_VectorType *v1 = static_cast<WGL_VectorType *>(t1);
        WGL_VectorType *v2 = static_cast<WGL_VectorType *>(t2);

        return v1->type == v2->type;
    }
    case WGL_Type::Matrix:
    {
        WGL_MatrixType *m1 = static_cast<WGL_MatrixType *>(t1);
        WGL_MatrixType *m2 = static_cast<WGL_MatrixType *>(t2);

        return m1->type == m2->type;
    }
    case WGL_Type::Sampler:
    {
        WGL_SamplerType *s1 = static_cast<WGL_SamplerType *>(t1);
        WGL_SamplerType *s2 = static_cast<WGL_SamplerType *>(t2);

        return s1->type == s2->type;
    }
    case WGL_Type::Name:
    {
        WGL_NameType *n1 = static_cast<WGL_NameType *>(t1);
        WGL_NameType *n2 = static_cast<WGL_NameType *>(t2);

        return n1->name->Equals(n2->name);
    }
    case WGL_Type::Struct:
    {
        WGL_StructType *s1 = static_cast<WGL_StructType *>(t1);
        WGL_StructType *s2 = static_cast<WGL_StructType *>(t2);
        if (!s1->tag->Equals(s2->tag))
            return FALSE;
        if (s1->fields->list.Cardinal() != s2->fields->list.Cardinal())
            return FALSE;

        /* Identical types if same tag..but just for the fun of it. */
        WGL_Field *f1 = s1->fields->list.First();
        for (WGL_Field *f2 = s2->fields->list.First(); f2; f2 = f2->Suc())
        {
            if (!IsSameType(f1->type, f2->type, TRUE))
                return FALSE;
            f1 = f1->Suc();
        }
        return TRUE;
    }
    case WGL_Type::Array:
    {
        WGL_ArrayType *a1 = static_cast<WGL_ArrayType *>(t1);
        WGL_ArrayType *a2 = static_cast<WGL_ArrayType *>(t2);
        if (!IsSameType(a1->element_type, a2->element_type, TRUE))
            return FALSE;

        /* An error if this predicate is called on non-normalised types. */
        OP_ASSERT(a1->is_constant_value && a2->is_constant_value);

        if (a1->is_constant_value && a2->is_constant_value)
            return (a1->length_value == a2->length_value);
        else
            return FALSE;
    }
    default:
        OP_ASSERT(!"Unhandled type");
        return FALSE;
    }
}

WGL_Type *
WGL_ValidationState::NormaliseType(WGL_Type *t)
{
     switch (t->GetType())
    {
    case WGL_Type::Basic:
    case WGL_Type::Vector:
    case WGL_Type::Matrix:
    case WGL_Type::Sampler:
        return t;
    case WGL_Type::Name:
    {
        WGL_NameType *n = static_cast<WGL_NameType *>(t);
        if (WGL_Type *type = LookupTypeName(n->name))
            return NormaliseType(type);
        else
            return t;
    }
    case WGL_Type::Struct:
    {
        WGL_StructType *s = static_cast<WGL_StructType *>(t);
        for (WGL_Field *f = s->fields ? s->fields->list.First() : NULL; f; f = f->Suc())
            f->type = NormaliseType(f->type);

        return t;
    }
    case WGL_Type::Array:
    {
        WGL_ArrayType *a = static_cast<WGL_ArrayType *>(t);
        a->element_type = NormaliseType(a->element_type);
        if (!a->is_constant_value)
        {
            if (WGL_Literal *l = EvalConstExpression(a->length))
                if (l->type == WGL_Literal::Int && l->value.i >= 0)
                {
                    a->length_value = static_cast<unsigned>(l->value.i);
                    a->is_constant_value = TRUE;
                }
                else if (l->type == WGL_Literal::UInt)
                {
                    a->length_value = l->value.u_int;
                    a->is_constant_value = TRUE;
                }
        }
        return t;
    }
    default:
        OP_ASSERT(!"Unhandled type");
        return t;
    }
}

BOOL
WGL_ValidationState::HasExpressionType(WGL_Type *t, WGL_Expr *e, BOOL type_equality)
{
    switch (e->GetType())
    {
    case WGL_Expr::Lit:
    {
        WGL_LiteralExpr *lit = static_cast<WGL_LiteralExpr *>(e);
        switch (lit->literal->type)
        {
        case WGL_Literal::Double:
            if (t->GetType() == WGL_Type::Basic)
            {
                WGL_BasicType *base_type = static_cast<WGL_BasicType *>(t);
                return (type_equality && base_type->type == WGL_BasicType::Float || (!type_equality && base_type->type != WGL_BasicType::Void));
            }
            else
                return FALSE;
        case WGL_Literal::Int:
        case WGL_Literal::UInt:
            if (t->GetType() == WGL_Type::Basic)
            {
                WGL_BasicType *base_type = static_cast<WGL_BasicType *>(t);
                if (type_equality && base_type->type == WGL_BasicType::UInt)
                    return (lit->literal->type == WGL_Literal::UInt || lit->literal->value.i >= 0);
                else
                    return (type_equality && base_type->type == WGL_BasicType::Int || (!type_equality && base_type->type != WGL_BasicType::Void));
            }
            else
                return FALSE;
        case WGL_Literal::Bool:
            if (t->GetType() == WGL_Type::Basic)
            {
                WGL_BasicType *base_type = static_cast<WGL_BasicType *>(t);
                return (type_equality && base_type->type == WGL_BasicType::Bool || (!type_equality && base_type->type != WGL_BasicType::Void));
            }
            else
                return FALSE;
        default:
            return FALSE;
        }
    }
    case WGL_Expr::Var:
    {
        WGL_VarExpr *var_expr = static_cast<WGL_VarExpr *>(e);
        if (WGL_Type *var_type = LookupVarType(var_expr->name, NULL))
            return IsSameType(var_type, t, type_equality);
        else
            return FALSE;
    }
    case WGL_Expr::TypeConstructor:
    {
        WGL_TypeConstructorExpr *type_expr = static_cast<WGL_TypeConstructorExpr *>(e);
        return IsSameType(type_expr->constructor, t, type_equality);
    }
    case WGL_Expr::Unary:
        return HasExpressionType(t, static_cast<WGL_UnaryExpr *>(e)->arg, type_equality);
    case WGL_Expr::Binary:
        if (WGL_Type *bin_type = GetExpressionType(e))
            return IsSameType(t, bin_type, type_equality);
        else
            return FALSE;
    case WGL_Expr::Assign:
        return HasExpressionType(t, static_cast<WGL_AssignExpr *>(e)->rhs, type_equality);
    case WGL_Expr::Seq:
        return HasExpressionType(t, static_cast<WGL_SeqExpr *>(e)->arg2, type_equality);
    case WGL_Expr::Select:
    {
        WGL_SelectExpr *select_expr = static_cast<WGL_SelectExpr *>(e);
        if (WGL_Type *type_struct = GetExpressionType(select_expr->value))
        {
            if (type_struct->GetType() == WGL_Type::Struct)
            {
                if (WGL_Field *f = LookupField(static_cast<WGL_StructType *>(type_struct)->fields, select_expr->field))
                    return IsSameType(t, f->type, type_equality);
            }
            else if (type_struct->GetType() == WGL_Type::Vector)
            {
                WGL_VectorType *vector_struct = static_cast<WGL_VectorType *>(type_struct);
                const uni_char *str = Storage(context, select_expr->field);
                unsigned len = static_cast<unsigned>(uni_strlen(str));
                if (len <= 4 && len > 1 && t->GetType() == WGL_Type::Vector)
                {
                    WGL_VectorType *vec = static_cast<WGL_VectorType *>(t);
                    return (WGL_VectorType::GetMagnitude(vec) == len && WGL_VectorType::SameElementType(vec, vector_struct));
                }
                else if (len == 1 && t->GetType() == WGL_Type::Basic)
                    return WGL_VectorType::GetElementType(vector_struct) == static_cast<WGL_BasicType *>(t)->type;
            }
        }
        return FALSE;
    }
    case WGL_Expr::Index:
    {
        WGL_IndexExpr *index_expr = static_cast<WGL_IndexExpr *>(e);
        if (WGL_Type *type_array = GetExpressionType(index_expr->array))
            if (type_array->GetType() == WGL_Type::Array)
                return IsSameType(t, static_cast<WGL_ArrayType *>(type_array)->element_type, type_equality);

        return FALSE;
    }
    case WGL_Expr::Call:
        if (WGL_Type *app_type = GetExpressionType(e))
            return IsSameType(t, context->GetValidationState().NormaliseType(app_type), type_equality);
        else
            return FALSE;
    default:
        return FALSE;
    }
}

WGL_Type *
WGL_ValidationState::FromBuiltinType(WGL_BuiltinType *t)
{
    switch (t->type)
    {
    case WGL_BuiltinType::Basic:
        return context->GetASTBuilder()->BuildBasicType(t->value.basic_type);
    case WGL_BuiltinType::Vector:
        return context->GetASTBuilder()->BuildVectorType(t->value.vector_type);
    case WGL_BuiltinType::Matrix:
        return context->GetASTBuilder()->BuildMatrixType(t->value.matrix_type);
    case WGL_BuiltinType::Sampler:
        return context->GetASTBuilder()->BuildSamplerType(t->value.sampler_type);
    default:
        return NULL;
    }
}

BOOL
WGL_ValidationState::MatchesBuiltinType(WGL_BuiltinType *t, WGL_Type *type)
{
    switch (t->type)
    {
    case WGL_BuiltinType::Basic:
        if (type->GetType() == WGL_Type::Basic)
            return t->Matches(static_cast<WGL_BasicType *>(type)->type, FALSE);
        else
            return FALSE;

    case WGL_BuiltinType::Vector:
    case WGL_BuiltinType::Vec:
    case WGL_BuiltinType::IVec:
    case WGL_BuiltinType::BVec:
        if (type->GetType() == WGL_Type::Vector)
            return t->Matches(static_cast<WGL_VectorType *>(type)->type, TRUE);
        else
            return FALSE;

    case WGL_BuiltinType::Matrix:
    case WGL_BuiltinType::Mat:
        if (type->GetType() == WGL_Type::Matrix)
            return t->Matches(static_cast<WGL_MatrixType *>(type)->type, TRUE);
        else
            return FALSE;

    case WGL_BuiltinType::Sampler:
        if (type->GetType() == WGL_Type::Sampler)
            return t->Matches(static_cast<WGL_SamplerType *>(type)->type, TRUE);
        else
            return FALSE;

    case WGL_BuiltinType::Gen:
        if (type->GetType() == WGL_Type::Basic)
            return t->Matches(static_cast<WGL_BasicType *>(type)->type, FALSE);
        else if (type->GetType() == WGL_Type::Vector)
            return t->Matches(static_cast<WGL_VectorType *>(type)->type, TRUE);
        else
            return FALSE;

    default:
        OP_ASSERT(!"Unhandled type");
        return FALSE;
    }
}

BOOL
WGL_ValidationState::IsConstantBuiltin(const uni_char *str)
{
    /* Everything goes except for the texture lookup functions;
       if they match the prefix "texture", that's all we need to know
       (since it is a builtin -- if the spec disallows other builtins,
       this will have to be updated.) */
    unsigned len = static_cast<unsigned>(uni_strlen(str));
    if (len > 6 && uni_strncmp("texture", str, 6) == 0)
        return FALSE;

    return TRUE;
}

BOOL
WGL_ValidationState::IsConstantExpression(WGL_Expr *e)
{
    if (!e)
        return FALSE;

    switch (e->GetType())
    {
    case WGL_Expr::Lit:
        return TRUE;
    case WGL_Expr::Var:
        if (WGL_Type *type = LookupVarType(static_cast<WGL_VarExpr *>(e)->name, NULL))
            /* NOTE: GLSL does not recognise 'uniform's as constant; not bound until run-time */
            return type->GetType() != WGL_Type::Array && (!type->type_qualifier || type->type_qualifier->storage == WGL_TypeQualifier::Const);
        else
            return FALSE;
    case WGL_Expr::TypeConstructor:
        return FALSE;
    case WGL_Expr::Call:
    {
        WGL_CallExpr *call_expr = static_cast<WGL_CallExpr *>(e);

        if (!call_expr->args)
            return FALSE;

        if (call_expr->fun->GetType() == WGL_Expr::TypeConstructor)
        {
            for (WGL_Expr *arg = call_expr->args->list.First(); arg; arg = arg->Suc())
                if (!IsConstantExpression(arg))
                    return FALSE;
            return TRUE;
        }
        else if (call_expr->fun->GetType() == WGL_Expr::Var)
        {
            WGL_VarExpr *fun = static_cast<WGL_VarExpr *>(call_expr->fun);
            if (List<WGL_Builtin> *builtins = LookupBuiltin(fun->name))
                if (WGL_BuiltinFun *f = ResolveBuiltinFunction(builtins, call_expr->args))
                    if (IsConstantBuiltin(f->name))
                    {
                        BOOL matched_one = FALSE;
                        for (WGL_Expr *arg = call_expr->args->list.First(); arg; arg = arg->Suc())
                            if (!IsConstantExpression(arg))
                            {
                                if (f->intrinsic != WGL_GLSL::Max && f->intrinsic != WGL_GLSL::Min)
                                    return FALSE;
                            }
                            else
                                matched_one = TRUE;

                        return matched_one;
                    }
        }
        return FALSE;
    }
    case WGL_Expr::Assign:
        return FALSE;
    case WGL_Expr::Select:
    {
        WGL_SelectExpr *select_expr = static_cast<WGL_SelectExpr *>(e);
        /* NOTE: assume well-typed (and to a known field.)
         * If the 'record' is a constructor application (of a struct),
         * it's a constant expression if all fields are.
         */
        if (select_expr->GetType() == WGL_Expr::Call)
            return IsConstantExpression(select_expr->value);
        else
            return FALSE;
    }
    case WGL_Expr::Index:
    {
        WGL_IndexExpr *index_expr = static_cast<WGL_IndexExpr *>(e);
        WGL_Type *type = GetExpressionType(index_expr->array);
        return type && type->GetType() != WGL_Type::Array && IsConstantExpression(index_expr->array) && IsConstantExpression(index_expr->index);
    }
    case WGL_Expr::Unary:
    {
        WGL_UnaryExpr *unary_expr = static_cast<WGL_UnaryExpr *>(e);
        if (unary_expr->op == WGL_Expr::OpPreDec || unary_expr->op == WGL_Expr::OpPreInc)
            return FALSE;
        else
            return IsConstantExpression(unary_expr->arg);
    }
    case WGL_Expr::PostOp:
        /* I don't think these can be considered 'constant expressions'..but the spec
           doesn't call them out as not. (GLSL ES2.0, Section 5.10). */
        return FALSE;
    case WGL_Expr::Seq:
    {
        WGL_SeqExpr *seq_expr = static_cast<WGL_SeqExpr *>(e);
        return IsConstantExpression(seq_expr->arg1) && IsConstantExpression(seq_expr->arg2);
    }
    case WGL_Expr::Binary:
    {
        WGL_BinaryExpr *bin_expr = static_cast<WGL_BinaryExpr *>(e);
        return IsConstantExpression(bin_expr->arg1) && IsConstantExpression(bin_expr->arg2);
    }
    case WGL_Expr::Cond:
    {
        WGL_CondExpr *cond_expr = static_cast<WGL_CondExpr *>(e);
        return IsConstantExpression(cond_expr->arg1) && IsConstantExpression(cond_expr->arg2) && IsConstantExpression(cond_expr->arg3);
    }
    default:
        OP_ASSERT(!"Unhandled expression type");
        return FALSE;
    }
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_ReturnStmt *s)
{
    context->SetLineNumber(s->line_number);

    if (s->value)
        s->value = s->value->VisitExpr(this);
    return s;
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_ExprStmt *s)
{
    context->SetLineNumber(s->line_number);

    if (s->expr)
        s->expr = s->expr->VisitExpr(this);
    return s;
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_DeclStmt *s)
{
    context->SetLineNumber(s->line_number);

    for (WGL_Decl *decl = s->decls.First(); decl; decl = decl->Suc())
        decl = decl->VisitDecl(this);

    return s;
}

/* virtual */ WGL_Stmt *
WGL_ValidateShader::VisitStmt(WGL_CaseLabel *s)
{
    context->SetLineNumber(s->line_number);

    s->condition = s->condition->VisitExpr(this);
    return s;
}

/* virtual */ WGL_Type *
WGL_ValidateShader::VisitType(WGL_BasicType *t)
{
    return t;
}

/* virtual */ WGL_Type *
WGL_ValidateShader::VisitType(WGL_ArrayType *t)
{
    t->element_type = t->element_type->VisitType(this);
    if (t->length)
    {
        t->length = t->length->VisitExpr(this);
        if (WGL_Literal *l = context->GetValidationState().EvalConstExpression(t->length))
        {
            WGL_Literal::Value lv = WGL_Literal::ToInt(l->type, l->value);
            if (lv.i == 0)
                context->AddError(WGL_Error::ARRAY_SIZE_IS_ZERO, UNI_L(""));
            else
            {
                t->is_constant_value = TRUE;
                t->length_value = static_cast<unsigned>(lv.i);
            }
        }
    }

    return t;
}

/* virtual */ WGL_Type *
WGL_ValidateShader::VisitType(WGL_VectorType *t)
{
    return t;
}
/* virtual */ WGL_Type *
WGL_ValidateShader::VisitType(WGL_MatrixType *t)
{
    return t;
}

/* virtual */ WGL_Type *
WGL_ValidateShader::VisitType(WGL_SamplerType *t)
{
    return t;
}

/* virtual */ WGL_Type *
WGL_ValidateShader::VisitType(WGL_NameType *t)
{
    ValidateTypeName(t->name, TRUE);
    return t;
}

/* virtual */ WGL_Type *
WGL_ValidateShader::VisitType(WGL_StructType *t)
{
    ValidateTypeName(t->tag, FALSE);
    if (!t->fields)
        context->AddError(WGL_Error::EMPTY_STRUCT, Storage(context, t->tag));
    else
    {
        context->EnterLocalScope(NULL);
        for (WGL_Field *f = t->fields->list.First(); f; f = f->Suc())
            if (!f->name)
                context->AddError(WGL_Error::ANON_FIELD, Storage(context, t->tag));
            else if (f->type && f->type->GetType() == WGL_Type::Struct)
                context->AddError(WGL_Error::ILLEGAL_NESTED_STRUCT, Storage(context, t->tag));
            else
                f->VisitField(this);
        context->LeaveLocalScope();
    }
    context->GetValidationState().AddStructType(t->tag, static_cast<WGL_Type *>(t));
    return t;
}

/* virtual */ WGL_Decl *
WGL_ValidateShader::VisitDecl(WGL_FunctionPrototype *proto)
{
    context->SetLineNumber(proto->line_number);
    if (proto->return_type)
        proto->return_type = proto->return_type->VisitType(this);

    if (proto->name && proto->return_type)
    {
        BOOL overlaps = context->GetValidationState().ConflictsWithBuiltin(proto->name, proto->parameters, proto->return_type);
        if (overlaps)
            context->AddError(WGL_Error::ILLEGAL_BUILTIN_OVERRIDE, Storage(context, proto->name));

        ValidateIdentifier(proto->name, FALSE);

        WGL_SourceLoc loc(context->GetSourceContext(), context->GetLineNumber());
        context->GetValidationState().AddFunction(proto->name, proto->return_type, proto->parameters, TRUE, loc);
    }

    context->EnterLocalScope(NULL);
    if (proto->parameters)
        proto->parameters = VisitParamList(proto->parameters);
    context->LeaveLocalScope();
    return proto;
}

/* virtual */ WGL_Decl *
WGL_ValidateShader::VisitDecl(WGL_PrecisionDefn *d)
{
    context->SetLineNumber(d->line_number);

    d->type = d->type->VisitType(this);
    BOOL precision_ok = FALSE;
    switch (d->type->GetType())
    {
    case WGL_Type::Basic:
    {
        WGL_BasicType *basic_type = static_cast<WGL_BasicType *>(d->type);
        if (basic_type->type == WGL_BasicType::Int || basic_type->type == WGL_BasicType::Float)
            precision_ok = TRUE;
        else
            context->AddError(WGL_Error::ILLEGAL_PRECISION_DECL, UNI_L("unsupported basic type"));
        break;
    }
    case WGL_Type::Sampler:
        precision_ok = TRUE;
        break;
    default:
        context->AddError(WGL_Error::ILLEGAL_PRECISION_DECL, UNI_L("unsupported type"));
        break;
    }
    if (precision_ok)
    {
        OP_ASSERT(d->type);
        context->GetValidationState().AddPrecision(d->type, d->precision);
    }

    return d;
}

/* virtual */ WGL_Decl *
WGL_ValidateShader::VisitDecl(WGL_ArrayDecl *d)
{
    context->SetLineNumber(d->line_number);

    d->type = d->type->VisitType(this);

    WGL_Type *array_type = NULL;
    WGL_VarName *alias_name = d->identifier;
    if (context->GetValidationState().LookupVarType(d->identifier, &alias_name, TRUE) != NULL)
        context->AddError(WGL_Error::DUPLICATE_NAME, Storage(context, d->identifier));
    else if (d->size)
    {
        array_type = context->GetASTBuilder()->BuildArrayType(d->type, d->size);
        WGL_VarBinding *binding = ValidateVarDefn(d->identifier, array_type);
        if (binding)
            d->identifier = binding->var;
    }

    if (d->size)
    {
        d->size = d->size->VisitExpr(this);
        if (!context->GetValidationState().IsConstantExpression(d->size))
            context->AddError(WGL_Error::EXPRESSION_NOT_CONSTANT, Storage(context, d->identifier));
        else if (array_type)
        {
            if (WGL_Literal *l = context->GetValidationState().EvalConstExpression(d->size))
            {
                WGL_Literal::Value lv = WGL_Literal::ToInt(l->type, l->value);
                if (lv.i == 0)
                    context->AddError(WGL_Error::ARRAY_SIZE_IS_ZERO, Storage(context, d->identifier));
                else
                {
                    WGL_ArrayType *array_ty = static_cast<WGL_ArrayType *>(array_type);
                    array_ty->is_constant_value = TRUE;
                    array_ty->length_value = static_cast<unsigned>(lv.i);
                }
            }
        }
    }
    else
        context->AddError(WGL_Error::ILLEGAL_UNSIZED_ARRAY_DECL, UNI_L(""));

    if (d->initialiser)
        d->initialiser = d->initialiser->VisitExpr(this);

    WGL_Type *resolved_t = array_type ? context->GetValidationState().NormaliseType(array_type) : NULL;

    if (d->type && d->type->type_qualifier && d->type->type_qualifier->storage == WGL_TypeQualifier::Attribute)
        context->AddError(WGL_Error::INVALID_ATTRIBUTE_DECL, Storage(context, d->identifier));
    else if (d->type && d->type->type_qualifier && d->type->type_qualifier->storage == WGL_TypeQualifier::Varying)
        context->GetValidationState().AddVarying(resolved_t, d->identifier, WGL_Type::NoPrecision, context->GetLineNumber());
    else if (d->type && d->type->type_qualifier && d->type->type_qualifier->storage == WGL_TypeQualifier::Uniform)
        context->GetValidationState().AddUniform(resolved_t, d->identifier, WGL_Type::NoPrecision, context->GetLineNumber());

    return d;
}

/* virtual */ WGL_Decl *
WGL_ValidateShader::VisitDecl(WGL_VarDecl *d)
{
    context->SetLineNumber(d->line_number);

    d->type->VisitType(this);
    if (d->initialiser)
        d->initialiser->VisitExpr(this);

    if (context->GetValidationState().LookupVarType(d->identifier, NULL, TRUE))
        context->AddError(WGL_Error::DUPLICATE_NAME, Storage(context, d->identifier));
    else
    {
        WGL_VarBinding *binder = ValidateVarDefn(d->identifier, d->type);
        if (binder)
            d->identifier = binder->var;
        /* For the purposes of catching out-of-bounds errors and the like,
           track constant-valued bindings. */
        if (binder && d->initialiser)
        {
            if (context->GetValidationState().IsConstantExpression(d->initialiser))
                binder->value = d->initialiser;
            WGL_Type *t = context->GetValidationState().GetExpressionType(d->initialiser);
            WGL_Type *resolved_t = t ? context->GetValidationState().NormaliseType(t) : NULL;
            WGL_Type *resolved_type = context->GetValidationState().NormaliseType(binder->type);
            if (t && !context->GetValidationState().IsSameType(resolved_type, resolved_t, TRUE))
                context->AddError(WGL_Error::TYPE_MISMATCH, UNI_L(""));
        }
    }

    if (WGL_TypeQualifier *type_qual = d->type->type_qualifier)
    {
        /* If a vertex shader, simply add them to the list..write and conflicting
         * definitions will be caught separately. For fragment shaders (run after vertex shaders..),
         * we check against the vertex shader uniforms to ensure consistency. */
        if (type_qual->storage == WGL_TypeQualifier::Uniform)
        {
            /* This would be the place to default the current precision if 'NoPrecision'. */
            WGL_Type::Precision precision = d->type->precision;
            if (WGL_VaryingOrUniform *uniform = context->GetValidationState().LookupUniform(d->identifier))
                CheckUniform(uniform, d->type, d->identifier, precision);
            else if (context->GetValidationState().GetScopeLevel() > 0)
                context->AddError(WGL_Error::INVALID_LOCAL_UNIFORM_DECL, Storage(context, d->identifier));
            else
            {
                WGL_Type *resolved_t = d->type ? context->GetValidationState().NormaliseType(d->type) : NULL;
                context->GetValidationState().AddUniform(resolved_t, d->identifier, precision, context->GetLineNumber());
            }
        }
        else if (type_qual->storage == WGL_TypeQualifier::Attribute)
        {
            BOOL attribute_decl_ok = TRUE;
            /* Check and register attribute declaration */
            if (!IsVertexShader() || context->GetValidationState().GetScopeLevel() > 0)
            {
                attribute_decl_ok = FALSE;
                context->AddError(!IsVertexShader() ? WGL_Error::INVALID_ATTRIBUTE_DECL : WGL_Error::INVALID_LOCAL_ATTRIBUTE_DECL, Storage(context, d->identifier));
            }
            else
            {
                switch (d->type->GetType())
                {
                case WGL_Type::Vector:
                {
                    WGL_VectorType *vector_type = static_cast<WGL_VectorType *>(d->type);
                    if (!(vector_type->type == WGL_VectorType::Vec2 || vector_type->type == WGL_VectorType::Vec3 || vector_type->type == WGL_VectorType::Vec4))
                    {
                        attribute_decl_ok = FALSE;
                        context->AddError(WGL_Error::INVALID_ATTRIBUTE_DECL_TYPE, Storage(context, d->identifier));
                    }
                    break;
                }
                case WGL_Type::Matrix:
                {
                    WGL_MatrixType *matrix_type = static_cast<WGL_MatrixType *>(d->type);
                    if (!(matrix_type->type == WGL_MatrixType::Mat2 || matrix_type->type == WGL_MatrixType::Mat3 || matrix_type->type == WGL_MatrixType::Mat4))
                    {
                        attribute_decl_ok = FALSE;
                        context->AddError(WGL_Error::INVALID_ATTRIBUTE_DECL_TYPE, Storage(context, d->identifier));
                    }
                    break;
                }
                case WGL_Type::Basic:
                {
                    WGL_BasicType *base_type = static_cast<WGL_BasicType *>(d->type);
                    if (base_type->type != WGL_BasicType::Float)
                    {
                        attribute_decl_ok = FALSE;
                        context->AddError(WGL_Error::INVALID_ATTRIBUTE_DECL_TYPE, Storage(context, d->identifier));
                    }
                    break;
                }
                default:
                    attribute_decl_ok = FALSE;
                    context->AddError(WGL_Error::INVALID_ATTRIBUTE_DECL, Storage(context, d->identifier));
                    break;
                }
            }
            if (attribute_decl_ok)
            {
                WGL_Type *resolved_t = d->type ? context->GetValidationState().NormaliseType(d->type) : NULL;
                context->GetValidationState().AddAttribute(resolved_t, d->identifier, context->GetLineNumber());
            }
        }
        else if (type_qual->storage == WGL_TypeQualifier::Varying)
        {
            BOOL varying_decl_ok = TRUE;
            if (context->GetValidationState().GetScopeLevel() > 0)
            {
                varying_decl_ok = FALSE;
                context->AddError(WGL_Error::INVALID_LOCAL_VARYING_DECL, Storage(context, d->identifier));
            }
            else if ((d->type->GetType() == WGL_Type::Array && !IsVaryingType(static_cast<WGL_ArrayType *>(d->type)->element_type)) ||
                     !IsVaryingType(d->type))
            {
                varying_decl_ok = FALSE;
                context->AddError(WGL_Error::INVALID_VARYING_DECL_TYPE, Storage(context, d->identifier));
            }
            if (varying_decl_ok)
            {
                WGL_Type::Precision precision = d->type->precision;
                if (WGL_VaryingOrUniform *varying = context->GetValidationState().LookupVarying(d->identifier))
                    CheckVarying(varying, d->type, d->identifier, precision);
                else
                {
                    WGL_Type *resolved_t = d->type ? context->GetValidationState().NormaliseType(d->type) : NULL;
                    context->GetValidationState().AddVarying(resolved_t, d->identifier, precision, context->GetLineNumber());
                }
            }
        }
    }
    return d;
}

BOOL
WGL_ValidateShader::IsVaryingType(WGL_Type *type)
{
    switch (type->GetType())
    {
    case WGL_Type::Vector:
    {
        WGL_VectorType *vector_type = static_cast<WGL_VectorType *>(type);
        return (vector_type->type == WGL_VectorType::Vec2 || vector_type->type == WGL_VectorType::Vec3 || vector_type->type == WGL_VectorType::Vec4);
    }
    case WGL_Type::Matrix:
    {
        WGL_MatrixType *matrix_type = static_cast<WGL_MatrixType *>(type);
        return (matrix_type->type == WGL_MatrixType::Mat2 || matrix_type->type == WGL_MatrixType::Mat3 || matrix_type->type == WGL_MatrixType::Mat4);
    }
    case WGL_Type::Basic:
    {
        WGL_BasicType *base_type = static_cast<WGL_BasicType *>(type);
        return (base_type->type == WGL_BasicType::Float);
    }
    default:
        return FALSE;
    }
}

/* virtual */ WGL_Decl *
WGL_ValidateShader::VisitDecl(WGL_TypeDecl *d)
{
    context->SetLineNumber(d->line_number);

    d->type = d->type->VisitType(this);
    if (d->var_name)
    {
        WGL_VarBinding *binding = ValidateVarDefn(d->var_name, d->type);
        if (binding)
            d->var_name = binding->var;
    }

    if (WGL_TypeQualifier *type_qual = d->type->type_qualifier)
    {
        BOOL is_attribute = type_qual->storage == WGL_TypeQualifier::Attribute;
        if (is_attribute || type_qual->storage == WGL_TypeQualifier::Uniform)
        {
            /* Check and register attribute declaration */
            if (context->GetValidationState().GetScopeLevel() > 0)
            {
                /* Get the var_name if there is one or "". */
                const uni_char *var_name = d->var_name ? Storage(context, d->var_name) : UNI_L("");
                if (is_attribute)
                    context->AddError(WGL_Error::INVALID_LOCAL_ATTRIBUTE_DECL, var_name);
                else
                    context->AddError(WGL_Error::INVALID_LOCAL_UNIFORM_DECL, var_name);
            }
        }
    }
    return d;
}

/* virtual */ WGL_Decl *
WGL_ValidateShader::VisitDecl(WGL_InvariantDecl *d)
{
    context->SetLineNumber(d->line_number);

    /* Only permitted at global scope. */
    if (context->GetValidationState().GetScopeLevel() > 0)
    {
        context->AddError(WGL_Error::ILLEGAL_INVARIANT_DECL, Storage(context, d->var_name));
        return d;
    }

    WGL_InvariantDecl *it = d;
    while (it)
    {
        d->var_name = ValidateVar(d->var_name, FALSE);
        if (List <WGL_Builtin> *builtin = context->GetValidationState().LookupBuiltin(d->var_name))
        {
            if (!builtin->First()->IsBuiltinVar())
                context->AddError(WGL_Error::ILLEGAL_INVARIANT_DECL, Storage(context, d->var_name));
            else
            {
                WGL_BuiltinVar *var = static_cast<WGL_BuiltinVar *>(builtin->First());
                if (!var->is_output)
                    context->AddError(WGL_Error::ILLEGAL_INVARIANT_DECL, Storage(context, d->var_name));
            }
        }
        else if (WGL_Type *t = context->GetValidationState().LookupVarType(d->var_name, NULL))
            if (!t->type_qualifier || t->type_qualifier->storage != WGL_TypeQualifier::Varying)
                context->AddError(WGL_Error::ILLEGAL_INVARIANT_DECL, Storage(context, d->var_name));

        it = it->next;
    }
    return d;
}

/* virtual */ WGL_Decl *
WGL_ValidateShader::VisitDecl(WGL_FunctionDefn *d)
{
    context->SetLineNumber(d->line_number);
    if (d->head->GetType() != WGL_Decl::Proto)
        context->AddError(WGL_Error::INVALID_FUNCTION_DEFN, UNI_L("fun"));
    else
    {
        WGL_FunctionPrototype *proto = static_cast<WGL_FunctionPrototype *>(d->head);

        if (context->GetValidationState().CurrentFunction() != NULL)
            context->AddError(WGL_Error::ILLEGAL_NESTED_FUNCTION, Storage(context, proto->name));

        proto->return_type = proto->return_type->VisitType(this);

        if (!IsValidReturnType(proto->return_type))
            context->AddError(WGL_Error::ILLEGAL_RETURN_TYPE, Storage(context, proto->name));

        BOOL overlaps = context->GetValidationState().ConflictsWithBuiltin(proto->name, proto->parameters, proto->return_type);
        if (overlaps)
            context->AddError(WGL_Error::ILLEGAL_BUILTIN_OVERRIDE, Storage(context, proto->name));

        ValidateIdentifier(proto->name, FALSE);
        context->EnterLocalScope(proto->return_type);
        proto->parameters = VisitParamList(proto->parameters);

        WGL_SourceLoc loc(context->GetSourceContext(), context->GetLineNumber());
        context->GetValidationState().AddFunction(proto->name, proto->return_type, proto->parameters, FALSE, loc);

        /* Nested functions aren't allowed, but track the current one
           for error handling purposes. */
        List<WGL_VariableUse> *old_callers = NULL;
        WGL_VarName *old_fun_name = context->GetValidationState().SetFunction(proto->name, old_callers);
        d->body = d->body->VisitStmt(this);

        if (WGL_FunctionData *fun_data = context->GetValidationState().LookupFunction(proto->name))
            fun_data->decls.Last()->callers = context->GetValidationState().GetFunctionCalls();

        context->GetValidationState().SetFunction(old_fun_name, old_callers);
        context->LeaveLocalScope();
    }
    return d;
}

/* virtual */ WGL_Field *
WGL_ValidateShader::VisitField(WGL_Field *f)
{
    if (context->GetValidationState().LookupVarType(f->name, NULL, TRUE))
        context->AddError(WGL_Error::DUPLICATE_NAME, Storage(context, f->name));
    else
    {
        WGL_VarBinding *binding = ValidateVarDefn(f->name, f->type);
        if (binding)
            f->name = binding->var;
    }

    if (f->type->GetType() == WGL_Type::Array)
    {
        WGL_ArrayType *array_type = static_cast<WGL_ArrayType *>(f->type);
        if (context->GetValidationState().IsConstantExpression(array_type->length))
        {
            WGL_Type *t = context->GetValidationState().GetExpressionType(array_type->length);
            if (!t || t->GetType() == WGL_Type::Basic && static_cast<WGL_BasicType *>(t)->type != WGL_BasicType::Int)
                context->AddError(WGL_Error::ILLEGAL_NON_CONSTANT_ARRAY, UNI_L(""));
            else
            {
                int val = 0;
                BOOL success = context->GetValidationState().LiteralToInt(context->GetValidationState().EvalConstExpression(array_type->length), val);
                if (!success || val <= 0)
                    context->AddError(WGL_Error::ILLEGAL_NON_CONSTANT_ARRAY, UNI_L(""));
                else
                {
                    array_type->is_constant_value = TRUE;
                    array_type->length_value = val;
                }
            }
        }
        else
            context->AddError(WGL_Error::ILLEGAL_NON_CONSTANT_ARRAY, UNI_L(""));
    }
    return f;
}

/* virtual */ WGL_Param *
WGL_ValidateShader::VisitParam(WGL_Param *p)
{
    p->type = p->type->VisitType(this);
    if (WGL_VarBinding *binding = ValidateVarDefn(p->name, p->type))
        p->name = binding->var;

    if (p->type)
        p->type = context->GetValidationState().NormaliseType(p->type);

    return p;
}

/* virtual */ WGL_LayoutPair *
WGL_ValidateShader::VisitLayout(WGL_LayoutPair *lp)
{
    lp->identifier = ValidateVar(lp->identifier);
    return lp;
}

/* virtual */ WGL_LayoutList *
WGL_ValidateShader::VisitLayoutList(WGL_LayoutList *ls)
{
    for (WGL_LayoutPair *lp = ls->list.First(); lp; lp = lp->Suc())
        lp = lp->VisitLayout(this);

    return ls;
}

/* virtual */ WGL_FieldList *
WGL_ValidateShader::VisitFieldList(WGL_FieldList *ls)
{
    for (WGL_Field *f = ls->list.First(); f; f = f->Suc())
        f = f->VisitField(this);

    return ls;
}

/* virtual */ WGL_ParamList *
WGL_ValidateShader::VisitParamList(WGL_ParamList *ls)
{
    for (WGL_Param *p = ls->list.First(); p; p = p->Suc())
        p = p->VisitParam(this);

    return ls;
}

/* virtual */ WGL_DeclList*
WGL_ValidateShader::VisitDeclList(WGL_DeclList *ls)
{
    for (WGL_Decl *d = ls->list.First(); d; d = d->Suc())
        d = d->VisitDecl(this);

    return ls;
}

/* virtual */ WGL_StmtList *
WGL_ValidateShader::VisitStmtList(WGL_StmtList *ls)
{
    for (WGL_Stmt *s = ls->list.First();  s; s = s->Suc())
        s = s->VisitStmt(this);

    return ls;
}

/* virtual */ WGL_ExprList *
WGL_ValidateShader::VisitExprList(WGL_ExprList *ls)
{
    for (WGL_Expr *e = ls->list.First(); e; e = e->Suc())
        e = e->VisitExpr(this);

    return ls;
}

void
WGL_ValidateShader::CheckUniform(WGL_VaryingOrUniform *uniform, WGL_Type *type, WGL_VarName *var, WGL_Type::Precision prec)
{
    /* NOTE: perform the overall uniform check afterwards; dependent on usage. */
    if (!context->GetValidationState().IsSameType(uniform->type, type, TRUE) || prec != uniform->precision)
        context->AddError(WGL_Error::INCOMPATIBLE_UNIFORM_DECL, Storage(context, var));
}

void
WGL_ValidateShader::CheckVarying(WGL_VaryingOrUniform *varying, WGL_Type *type, WGL_VarName *var, WGL_Type::Precision prec)
{
    if (!context->GetValidationState().IsSameType(varying->type, type, TRUE) || prec != varying->precision)
        context->AddError(WGL_Error::INCOMPATIBLE_VARYING_DECL, Storage(context, var));
}

BOOL
WGL_ValidateShader::IsValidReturnType(WGL_Type *type)
{
    switch (type->GetType())
    {
    case WGL_Type::Array:
        return FALSE;
    case WGL_Type::Struct:
    {
        WGL_StructType *struct_type = static_cast<WGL_StructType *>(type);
        if (!struct_type->fields)
            return FALSE;
        else
        {
            for (WGL_Field *f = struct_type->fields->list.First(); f; f = f->Suc())
                if (!IsValidReturnType(f->type))
                    return FALSE;

            return TRUE;
        }
    }
    default:
        return TRUE;
    }
}

/* static */ void
WGL_ValidationState::CheckVariableConsistency(WGL_Context *context, WGL_ShaderVariable::Kind kind, List<WGL_ShaderVariable> &l1, List<WGL_ShaderVariable> &l2)
{
    WGL_ShaderVariable *uni2 = l2.First();
    for (WGL_ShaderVariable *uni1 = l1.First(); uni1 && uni2; uni1 = uni1->Suc())
    {
        /* Adjust 'uni2' to refer to next match candidate. */
        while (uni2 && uni1->name && uni2->name && uni_strcmp(uni2->name, uni1->name) < 0)
            uni2 = uni2->Suc();

        if (uni2 && uni1->name && uni2->name && uni_strcmp(uni2->name, uni1->name) == 0)
        {
            if (!WGL_ValidationState::IsSameType(uni1->type, uni2->type, TRUE))
            {
                WGL_Error::Type err = WGL_Error::INCOMPATIBLE_UNIFORM_DECL;
                switch (kind)
                {
                case WGL_ShaderVariable::Attribute:
                    err = WGL_Error::INCOMPATIBLE_ATTRIBUTE_DECL;
                    break;
                case WGL_ShaderVariable::Varying:
                    err = WGL_Error::INCOMPATIBLE_VARYING_DECL;
                    break;
                case WGL_ShaderVariable::Uniform:
                    err = WGL_Error::INCOMPATIBLE_UNIFORM_DECL;
                    break;
                default:
                    OP_ASSERT(!"Unexpected variable kind");
                    break;
                }
                context->AddError(err, uni1->name);
            }
            uni2 = uni2->Suc();
        }
    }
}

static BOOL
IsSameAliasNotVar(const WGL_ShaderVariable *u, const WGL_ShaderVariable *v)
{
    return u->alias_name && uni_str_eq(v->alias_name, u->alias_name) && !uni_str_eq(v->name, u->name);
}

/* static */ void
WGL_ValidationState::CheckVariableAliases(WGL_Context *context, WGL_ShaderVariables *vs, const WGL_ShaderVariable *v, WGL_ShaderVariable::Kind kind)
{
    if (v->alias_name)
    {
        for (WGL_ShaderVariable *u = vs->uniforms.First(); u; u = u->Suc())
            if (IsSameAliasNotVar(u, v))
            {
                OP_ASSERT(!"Alias names matched same hash-derived unique. Considered 'impossible'.");
                context->AddError(WGL_Error::INCOMPATIBLE_UNIFORM_DECL, v->name);
            }
        for (WGL_ShaderVariable *u = vs->varyings.First(); u; u = u->Suc())
            if (IsSameAliasNotVar(u, v))
            {
                OP_ASSERT(!"Alias names matched same hash-derived unique. Considered 'impossible'.");
                context->AddError(WGL_Error::INCOMPATIBLE_VARYING_DECL, v->name);
            }
    }
}

/* static */ void
WGL_ValidationState::CheckVariableAliasOverlap(WGL_Context *context, WGL_ShaderVariables *v1, WGL_ShaderVariables *v2)
{
    /* Verify that aliases don't accidentally overlap; we issue an error if this
       should accidentally happen. The practical likelihood should be close to zero. */
    for (WGL_ShaderVariable *u = v1->uniforms.First(); u; u = u->Suc())
    {
        CheckVariableAliases(context, v1, u, WGL_ShaderVariable::Uniform);
        CheckVariableAliases(context, v2, u, WGL_ShaderVariable::Uniform);
    }
    for (WGL_ShaderVariable *u = v1->varyings.First(); u; u = u->Suc())
    {
        CheckVariableAliases(context, v1, u, WGL_ShaderVariable::Varying);
        CheckVariableAliases(context, v2, u, WGL_ShaderVariable::Varying);
    }

}

void
WGL_ValidationState::CheckVariableConsistency(WGL_Context *context, WGL_ShaderVariables *v1, WGL_ShaderVariables *v2)
{
    /* Given the sets of variables produced by two different programs, determine if they are consistent. */
    CheckVariableConsistency(context, WGL_ShaderVariable::Uniform, v1->uniforms, v2->uniforms);
    CheckVariableConsistency(context, WGL_ShaderVariable::Varying, v1->varyings, v2->varyings);

    /* Attributes are only allowed in vertex shaders, but assuming more than one such shader
       might be linked together, checking consistency across these is required. */
    CheckVariableConsistency(context, WGL_ShaderVariable::Attribute, v1->attributes, v2->attributes);

    CheckVariableAliasOverlap(context, v1, v2);
    CheckVariableAliasOverlap(context, v2, v1);
}

#endif // CANVAS3D_SUPPORT
