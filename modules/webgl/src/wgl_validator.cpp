/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL validator.
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_validator.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_string.h"

#include "modules/webgl/src/wgl_pretty_printer.h"
#include "modules/webgl/src/wgl_printer_console.h"

#ifdef WGL_STANDALONE
#include <stdio.h>
#include "modules/webgl/src/wgl_printer_stdio.h"
#endif // WGL_STANDALONE

#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_builder.h"
#include "modules/webgl/src/wgl_context.h"
#include "modules/webgl/src/wgl_parser.h"
#include "modules/webgl/src/wgl_validate.h"
#include "modules/webgl/src/wgl_cgbuilder.h"

/* static */ OP_BOOLEAN
WGL_Validator::Validate(Configuration &config, const uni_char *url, const uni_char *source, unsigned length, WGL_ShaderVariables **variables)
{
    BOOL result = FALSE;
    OpMemGroup region;

    WGL_Context *context;
    RETURN_IF_ERROR(WGL_Context::Make(context, &region, config.support_oes_derivatives));

    TRAPD(status, result = ValidateL(context, config, url, source, length, variables));
    context->Release();

    RETURN_IF_ERROR(status);
    return result ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

/* static */ BOOL
WGL_Validator::ValidateL(WGL_Context *context, Configuration &config, const uni_char *url, const uni_char *source, unsigned length, WGL_ShaderVariables **variables)
{

    const BOOL for_gles = (config.output_format == VEGA3D_SHADER_LANGUAGE(SHADER_GLSL_ES));
#ifdef WGL_STANDALONE
    WGL_StdioPrinter printer(stdout, for_gles, config.support_higp_fragment);
#else
    WGL_ConsolePrinter printer(url, config.output_log, config.console_logging, for_gles, config.support_higp_fragment);
#endif // WGL_STANDALONE

    WGL_ProgramText webgl_glsl_source;
    webgl_glsl_source.program_text = const_cast<uni_char *>(source);
    webgl_glsl_source.program_text_length = length;

    WGL_Lexer *lexer;
    WGL_Lexer::MakeL(lexer, context);
    lexer->ConstructL(&webgl_glsl_source, 1, config.for_vertex);

    if (lexer->HaveErrors())
    {
        WGL_Error::ReportErrorsL(&printer, lexer->GetFirstError());
        return FALSE;
    }

    WGL_ASTBuilder builder(context);
    context->SetASTBuilder(&builder);

    WGL_Parser *parser;
    BOOL rewriteTexCoords = !IsGL(config.output_format);
    WGL_Parser::MakeL(parser, context, lexer, config.for_vertex, rewriteTexCoords);

    WGL_DeclList *shader_decls = parser->TopDecls();

    if (parser->HaveErrors())
    {
        WGL_Error::ReportErrorsL(&printer, parser->GetFirstError());
        return FALSE;
    }

    /* Validate the program (perform semantic analysis, check the conditions
       specified in the 'config' argument, etc.) and transform the AST to be
       suitable for compilation to either GLSL or HLSL. We also record
       uniforms, varying, and attributes used in the shader here. */

    WGL_ValidateShader *validator;
    WGL_ValidateShader::MakeL(validator, context, &printer);
    if (!validator->ValidateL(shader_decls, config))
    {
        WGL_Error::ReportErrorsL(&printer, validator->GetFirstError());
        return FALSE;
    }

    if (variables)
    {
        WGL_ShaderVariables *vars = OP_NEW_L(WGL_ShaderVariables, ());
        if (OpStatus::IsError(context->RecordResult(config.for_vertex, vars)))
        {
            OP_DELETE(vars);
            LEAVE(OpStatus::ERR_NO_MEMORY);
        }

        *variables = vars;
    }

    /* Generate a shader program for the target platform (OpenGL/GLSL or
       Direct3D/HLSL) from the AST. If config.output_source is NULL, only
       validation will be done. */

    if (shader_decls && config.output_source)
    {
        OpString output16;
        const char *const clamp_fn =
            "int webgl_op_clamp(int x, int ma) { return (x < 0 ? 0 : (x > ma ? ma : x)); }";

        if (IsGL(config.output_format))
        {
            WGL_ConsolePrinter printer(url, &output16, FALSE, for_gles, config.support_higp_fragment);
            PrintVersionAndExtensionPrelude(&printer, config.output_version, context);
            WGL_PrettyPrinter pretty(&printer);
            if (context->GetUsedClamp())
            {
                pretty.printer->OutString(clamp_fn);
                pretty.printer->OutNewline();
            }
            pretty.VisitDeclList(shader_decls);
        }
        else // HLSL 9/10
        {
            WGL_ASTBuilder fragment_builder(context);
            WGL_ConsolePrinter printer(url, &output16, FALSE, FALSE, FALSE);

            WGL_HLSLBuiltins::SemanticInfo semantic_info[] =
                { {UNI_L("g_Normal") , UNI_L("NORMAL"), WGL_HLSLBuiltins::NORMAL},
                  {UNI_L("g_Diffuse"), UNI_L("COLOR") , WGL_HLSLBuiltins::COLOR } };

            WGL_CgBuilder::Configuration cg_config(config.output_format == VEGA3D_SHADER_LANGUAGE(SHADER_HLSL_9) ? 9 : 10);
            cg_config.vertex_semantic_info          = semantic_info;
            cg_config.vertex_semantic_info_elements = ARRAY_SIZE(semantic_info);
            cg_config.pixel_semantic_info          = semantic_info;
            cg_config.pixel_semantic_info_elements = ARRAY_SIZE(semantic_info);

            WGL_CgBuilder cg(&cg_config, context, &printer, &fragment_builder);
            if (context->GetUsedClamp())
            {
                printer.OutString(clamp_fn);
                printer.OutNewline();
            }
            cg.GenerateCode(*variables, config.for_vertex ? shader_decls : NULL, config.for_vertex ? NULL : shader_decls);
        }

        LEAVE_IF_ERROR(config.output_source->Set(output16.CStr()));
    }

    return TRUE;
}

WGL_ShaderVariable::~WGL_ShaderVariable()
{
    if (owned)
    {
        OP_DELETEA(name);
        OP_DELETEA(alias_name);
        OP_DELETE(type);
    }
}

void
WGL_ShaderVariable::SetAliasL(const uni_char *name, BOOL copy)
{
    OP_DELETEA(alias_name);
    if (copy)
    {
        alias_name = NULL;
        alias_name = OP_NEWA_L(uni_char, uni_strlen(name) + 1);
        uni_strcpy(const_cast<uni_char *>(alias_name), name);
    }
    else
        alias_name = name;
}

#ifdef WGL_STANDALONE
/* static */ void
WGL_ShaderVariable::ShowL(WGL_PrettyPrinter *p)
{
    if (type)
        type->VisitType(p);
    if (name)
    {
        p->printer->OutString(" ");
        p->printer->OutString(name);
    }
}
#endif // WGL_STANDALONE

WGL_ShaderVariables::~WGL_ShaderVariables()
{
    Release();
}

void
WGL_ShaderVariables::Release()
{
    if (owned)
    {
        uniforms.Clear();
        varyings.Clear();
        attributes.Clear();
    }
    else
    {
        uniforms.RemoveAll();
        varyings.RemoveAll();
        attributes.RemoveAll();
    }
}

static BOOL
InsertVariable(List<WGL_ShaderVariable> &list, WGL_ShaderVariable *var, BOOL owned)
{
    WGL_ShaderVariable *elem = list.First();
    while (elem)
    {
        int compare = uni_strcmp(var->name, elem->name);
        if (compare < 0)
        {
            var->Precede(elem);
            return TRUE;
        }
        /* NOTE: the variables being merged are assumed to have already
           been checked for consistency. Hence equal names implies
           that their types are equal/consistent, so no duplicate kept. */
        else if (compare == 0)
        {
            if (owned)
                OP_DELETE(var);
            return FALSE;
        }
        else
            elem = elem->Suc();
    }

    var->Into(&list);
    return TRUE;
}

BOOL
WGL_ShaderVariables::AddAttribute(WGL_ShaderVariable *var)
{
    return InsertVariable(attributes, var, owned);
}

BOOL
WGL_ShaderVariables::AddAttributeL(WGL_Context *context, WGL_VarName *var, WGL_Type *type, WGL_Type::Precision p)
{
    OP_ASSERT(owned || context != NULL);

    const uni_char *name = Storage(context, var);
    WGL_ShaderVariable *builtin = NULL;
    if (owned)
        builtin = OP_NEW_L(WGL_ShaderVariable, (WGL_ShaderVariable::Attribute, name, p, type, TRUE));
    else
        builtin = OP_NEWGRO_L(WGL_ShaderVariable, (WGL_ShaderVariable::Attribute, name, p, type, FALSE), context->Arena());

    if (WGL_VarName *alias = context ? context->FindAlias(var) : NULL)
        builtin->SetAliasL(Storage(context, alias), TRUE);

    return InsertVariable(attributes, builtin, owned);
}

BOOL
WGL_ShaderVariables::AddAttributeL(WGL_Context *context, const uni_char *name, WGL_Type *type, WGL_Type::Precision p, const uni_char *alias_name)
{
    OP_ASSERT(owned || context != NULL);

    WGL_ShaderVariable *builtin = NULL;
    if (owned)
        builtin = OP_NEW_L(WGL_ShaderVariable, (WGL_ShaderVariable::Attribute, name, p, type, TRUE));
    else
        builtin = OP_NEWGRO_L(WGL_ShaderVariable, (WGL_ShaderVariable::Attribute, name, p, type, FALSE), context->Arena());

    if (alias_name)
        builtin->SetAliasL(alias_name);
    else if (WGL_VarName *alias = context ? context->FindAlias(name) : NULL)
        builtin->SetAliasL(Storage(context, alias), TRUE);

    return InsertVariable(attributes, builtin, owned);
}

BOOL
WGL_ShaderVariables::AddUniform(WGL_ShaderVariable *var)
{
    return InsertVariable(uniforms, var, owned);
}

BOOL
WGL_ShaderVariables::AddUniformL(WGL_Context *context, WGL_VarName *var, WGL_Type *type, WGL_Type::Precision p)
{
    OP_ASSERT(owned || context != NULL);

    const uni_char *name = Storage(context, var);
    WGL_ShaderVariable *builtin = NULL;
    if (owned)
        builtin = OP_NEW_L(WGL_ShaderVariable, (WGL_ShaderVariable::Uniform, name, p, type, TRUE));
    else
        builtin = OP_NEWGRO_L(WGL_ShaderVariable, (WGL_ShaderVariable::Uniform, name, p, type, FALSE), context->Arena());

    if (WGL_VarName *alias = context ? context->FindAlias(var) : NULL)
        builtin->SetAliasL(Storage(context, alias), TRUE);

    return InsertVariable(uniforms, builtin, owned);
}

BOOL
WGL_ShaderVariables::AddUniformL(WGL_Context *context, const uni_char *name, WGL_Type *type, WGL_Type::Precision p, const uni_char *alias_name)
{
    OP_ASSERT(owned || context != NULL);

    WGL_ShaderVariable *builtin = NULL;
    if (owned)
        builtin = OP_NEW_L(WGL_ShaderVariable, (WGL_ShaderVariable::Uniform, name, p, type, TRUE));
    else
        builtin = OP_NEWGRO_L(WGL_ShaderVariable, (WGL_ShaderVariable::Uniform, name, p, type, FALSE), context->Arena());

    if (alias_name)
        builtin->SetAliasL(alias_name);
    else if (WGL_VarName *alias = context ? context->FindAlias(name) : NULL)
        builtin->SetAliasL(Storage(context, alias), TRUE);

    return InsertVariable(uniforms, builtin, owned);
}

BOOL
WGL_ShaderVariables::AddVarying(WGL_ShaderVariable *var)
{
    return InsertVariable(varyings, var, owned);
}

BOOL
WGL_ShaderVariables::AddVaryingL(WGL_Context *context, WGL_VarName *var, WGL_Type *type, WGL_Type::Precision p)
{
    OP_ASSERT(owned || context != NULL);

    const uni_char *name = Storage(context, var);
    WGL_ShaderVariable *builtin = NULL;
    if (owned)
        builtin = OP_NEW_L(WGL_ShaderVariable, (WGL_ShaderVariable::Varying, name, p, type, TRUE));
    else
        builtin = OP_NEWGRO_L(WGL_ShaderVariable, (WGL_ShaderVariable::Varying, name, p, type, FALSE), context->Arena());

    if (WGL_VarName *alias = context ? context->FindAlias(var) : NULL)
        builtin->SetAliasL(Storage(context, alias), TRUE);

    return InsertVariable(varyings, builtin, owned);
}

BOOL
WGL_ShaderVariables::AddVaryingL(WGL_Context *context, const uni_char *name, WGL_Type *type, WGL_Type::Precision p, const uni_char *alias_name)
{
    OP_ASSERT(owned || context != NULL);

    WGL_ShaderVariable *builtin = NULL;
    if (owned)
        builtin = OP_NEW_L(WGL_ShaderVariable, (WGL_ShaderVariable::Varying, name, p, type, TRUE));
    else
        builtin = OP_NEWGRO_L(WGL_ShaderVariable, (WGL_ShaderVariable::Varying, name, p, type, FALSE), context->Arena());

    if (alias_name)
        builtin->SetAliasL(alias_name);
    else if (WGL_VarName *alias = context ? context->FindAlias(name) : NULL)
        builtin->SetAliasL(Storage(context, alias), TRUE);

    return InsertVariable(varyings, builtin, owned);
}

BOOL
WGL_ShaderVariables::AddVariableL(WGL_Context *context, WGL_ShaderVariable::Kind kind, WGL_VarName *var, WGL_Type *type)
{
    switch (kind)
    {
    case WGL_ShaderVariable::Uniform:
        return AddUniformL(context, var, type);
    case WGL_ShaderVariable::Varying:
        return AddVaryingL(context, var, type);
    default:
    case WGL_ShaderVariable::Attribute:
        return AddAttributeL(context, var, type);
    }
}

void
WGL_ShaderVariables::CopyVariableL(WGL_ShaderVariable *v)
{
    OP_ASSERT(owned);

    uni_char *name_copy = OP_NEWA_L(uni_char, uni_strlen(v->name) + 1);
    uni_strcpy(name_copy, v->name);
    ANCHOR_ARRAY(uni_char, name_copy);

    uni_char *alias_copy = NULL;
    if (v->alias_name)
    {
        alias_copy = OP_NEWA_L(uni_char, uni_strlen(v->alias_name) + 1);
        uni_strcpy(alias_copy, v->alias_name);
    }
    ANCHOR_ARRAY(uni_char, alias_copy);

    WGL_Type *type_copy = WGL_Type::CopyL(v->type);
    OpStackAutoPtr<WGL_Type> anchor_type(type_copy);

    switch (v->kind)
    {
    case WGL_ShaderVariable::Uniform:
        AddUniformL(NULL, name_copy, type_copy, v->precision, alias_copy);
        break;
    case WGL_ShaderVariable::Varying:
        AddVaryingL(NULL, name_copy, type_copy, v->precision, alias_copy);
        break;
    default:
    case WGL_ShaderVariable::Attribute:
        AddAttributeL(NULL, name_copy, type_copy, v->precision, alias_copy);
        break;
    }
    ANCHOR_ARRAY_RELEASE(name_copy);
    ANCHOR_ARRAY_RELEASE(alias_copy);
    anchor_type.release();
}

/* static */ WGL_ShaderVariables *
WGL_ShaderVariables::MakeL(WGL_ShaderVariables *c)
{
    WGL_ShaderVariables *vars = OP_NEW_L(WGL_ShaderVariables, ());
    OpStackAutoPtr<WGL_ShaderVariables> anchor_vars(vars);
    vars->MergeWithL(c);
    return anchor_vars.release();
}

void
WGL_ShaderVariables::MergeWithL(WGL_ShaderVariables *vars)
{
    /* Merging doesn't preserve duplicates, hence checking the
       result of CopyVariableL() isn't checked below.

       The validator will separately validate variable sets
       for consistency. */

    for (WGL_ShaderVariable *v = vars->attributes.First(); v; v = v->Suc())
        CopyVariableL(v);

    for (WGL_ShaderVariable *v = vars->uniforms.First(); v; v = v->Suc())
        CopyVariableL(v);

    for (WGL_ShaderVariable *v = vars->varyings.First(); v; v = v->Suc())
        CopyVariableL(v);
}

#ifdef WGL_STANDALONE
/* static */ void
WGL_ShaderVariables::ShowListL(WGL_PrettyPrinter *p, const char *label, List<WGL_ShaderVariable> &list)
{
    p->printer->OutString("// ");
    p->printer->OutString(label);
    p->printer->OutNewline();
    for (WGL_ShaderVariable *v = list.First(); v; v = v->Suc())
    {
        v->ShowL(p);
        p->printer->OutString(";");
        p->printer->OutNewline();
        if (v->type->GetType() == WGL_Type::Array)
        {
            WGL_ArrayType *array_type = static_cast<WGL_ArrayType *>(v->type);
            OP_ASSERT(array_type->is_constant_value);
            for (unsigned i = 0; i < array_type->length_value; i++)
            {
                v->type = array_type->element_type;
                v->ShowL(p);
                p->printer->OutString("[");
                p->printer->OutInt(i);
                p->printer->OutString("]");
                p->printer->OutString(";");
                p->printer->OutNewline();
            }
            v->type = array_type;
        }
        else if (v->type->GetType() == WGL_Type::Struct)
        {
            WGL_StructType *struct_type = static_cast<WGL_StructType *>(v->type);
            WGL_Field *f = struct_type->fields ? struct_type->fields->list.First() : NULL;
            for (; f; f = f->Suc())
            {
                v->type = f->type;
                v->ShowL(p);
                p->printer->OutString(".");
                p->printer->OutString(f->name);
                p->printer->OutString(";");
                p->printer->OutNewline();
            }
            v->type = struct_type;
        }
    }
}
#endif // WGL_STANDALONE

#ifdef WGL_STANDALONE
/* static */ OP_STATUS
WGL_ShaderVariables::Show(WGL_PrettyPrinter *p)
{
    TRAPD(status,
          ShowListL(p, "attributes", attributes);
          ShowListL(p, "uniforms", uniforms);
          ShowListL(p, "varyings", varyings));

    return status;
}
#endif // WGL_STANDALONE

OP_BOOLEAN
WGL_ShaderVariables::LookupAttribute(const char *name, VEGA3D_CONSTANT_TYPE &constant_type, unsigned &length, const uni_char **original_name)
{
    BOOL unused;
    return WGL_ShaderVariables::LookupListByName(attributes, name, FALSE, FALSE, constant_type, length, unused, original_name);
}

OP_BOOLEAN
WGL_ShaderVariables::LookupUniform(const char *name, VEGA3D_CONSTANT_TYPE &constant_type, unsigned &length, BOOL &is_array, const uni_char **original_name)
{
    return WGL_ShaderVariables::LookupListByName(uniforms, name, TRUE, TRUE, constant_type, length, is_array, original_name);
}

OP_BOOLEAN
WGL_ShaderVariables::LookupVarying(const char *name, VEGA3D_CONSTANT_TYPE &constant_type, unsigned &length, const uni_char **original_name)
{
    BOOL unused;
    return WGL_ShaderVariables::LookupListByName(varyings, name, TRUE, FALSE, constant_type, length, unused, original_name);
}

/* static */ VEGA3D_CONSTANT_TYPE
WGL_ShaderVariables::GetShaderType(WGL_Type *type)
{
    switch (type->GetType())
    {
    case WGL_Type::Basic:
    {
        WGL_BasicType *basic_type = static_cast<WGL_BasicType *>(type);
        switch (basic_type->type)
        {
        case WGL_BasicType::Float:
            return VEGA3D_CONSTANT(SHDCONST_FLOAT);
        case WGL_BasicType::Int:
        case WGL_BasicType::UInt:
            return VEGA3D_CONSTANT(SHDCONST_INT);
        case WGL_BasicType::Bool:
            return VEGA3D_CONSTANT(SHDCONST_BOOL);
        default:
            OP_ASSERT(!"Unexpected basic type");
            return VEGA3D_CONSTANT(SHDCONST_NONE);
        }
    }
    case WGL_Type::Vector:
    {
        WGL_VectorType *vector_type = static_cast<WGL_VectorType *>(type);
        switch (vector_type->type)
        {
        case WGL_VectorType::Vec2:
            return VEGA3D_CONSTANT(SHDCONST_FLOAT2);
        case WGL_VectorType::Vec3:
            return VEGA3D_CONSTANT(SHDCONST_FLOAT3);
        case WGL_VectorType::Vec4:
            return VEGA3D_CONSTANT(SHDCONST_FLOAT4);
        case WGL_VectorType::IVec2:
            return VEGA3D_CONSTANT(SHDCONST_INT2);
        case WGL_VectorType::IVec3:
            return VEGA3D_CONSTANT(SHDCONST_INT3);
        case WGL_VectorType::IVec4:
            return VEGA3D_CONSTANT(SHDCONST_INT4);
        case WGL_VectorType::BVec2:
            return VEGA3D_CONSTANT(SHDCONST_BOOL2);
        case WGL_VectorType::BVec3:
            return VEGA3D_CONSTANT(SHDCONST_BOOL3);
        case WGL_VectorType::BVec4:
            return VEGA3D_CONSTANT(SHDCONST_BOOL4);
        default:
            OP_ASSERT(!"Unexpected vector type");
            return VEGA3D_CONSTANT(SHDCONST_NONE);
        }
    }
    case WGL_Type::Matrix:
    {
        WGL_MatrixType *matrix_type = static_cast<WGL_MatrixType *>(type);
        switch (matrix_type->type)
        {
        case WGL_MatrixType::Mat2:
            return VEGA3D_CONSTANT(SHDCONST_MAT2);
        case WGL_MatrixType::Mat3:
            return VEGA3D_CONSTANT(SHDCONST_MAT3);
        case WGL_MatrixType::Mat4:
            return VEGA3D_CONSTANT(SHDCONST_MAT4);
        default:
            OP_ASSERT(!"Unexpected matrix type");
            return VEGA3D_CONSTANT(SHDCONST_NONE);
        }
    }
    case WGL_Type::Sampler:
    {
        WGL_SamplerType *sampler_type = static_cast<WGL_SamplerType *>(type);
        switch (sampler_type->type)
        {
        case WGL_SamplerType::Sampler2D:
            return VEGA3D_CONSTANT(SHDCONST_SAMP2D);
        case WGL_SamplerType::SamplerCube:
            return VEGA3D_CONSTANT(SHDCONST_SAMPCUBE);
        default:
            OP_ASSERT(!"Unexpected sampler type");
            return VEGA3D_CONSTANT(SHDCONST_NONE);
        }
    }
    default:
        OP_ASSERT(!"Unexpected type");
        return VEGA3D_CONSTANT(SHDCONST_NONE);
    }
}

/* static */ OP_BOOLEAN
WGL_ShaderVariables::LookupListByName(List<WGL_ShaderVariable> &list,
                                      const char               *name,
                                      BOOL                     allow_array,
                                      BOOL                     allow_struct,
                                      VEGA3D_CONSTANT_TYPE     &constant_type,
                                      unsigned                 &length,
                                      BOOL                     &is_array,
                                      const uni_char           **original_name)
{
    const char *name_rest = name;
    unsigned name_length = 0;
    while (*name_rest && *name_rest != '.' && *name_rest != '[')
    {
        name_rest++;
        name_length++;
    }
    if (name_length == 0)
        return OpBoolean::IS_FALSE;

    WGL_ShaderVariable *v = list.First();
    WGL_Type *result_type = NULL;
    while (v && !result_type)
    {
        BOOL non_aliased_name_matches = uni_strncmp(v->name, name, name_length) == 0;
        if (non_aliased_name_matches ||
            v->alias_name != NULL && uni_strncmp(v->alias_name, name, name_length) == 0)
        {
            /* We have found a variable in 'list' whose name (or alias) matches
               the first (and usually only) component of 'name'. */

            if (original_name)
                *original_name = non_aliased_name_matches ? v->alias_name : v->name;

            BOOL is_resolved = FALSE;
            WGL_Type *v_type = v->type;
            while (!is_resolved)
            {
                /* For compound names (e.g. "foo[2]" or "foo.bar"), go through
                   each component of 'name' and make sure it matches a
                   component of 'v', looking up types in the process. */

                if (*name_rest == '.' && allow_struct && v_type->GetType() == WGL_Type::Struct)
                {
                    WGL_StructType *struct_type = static_cast<WGL_StructType *>(v_type);
                    name = ++name_rest;
                    name_length = 0;
                    while (*name_rest && *name_rest != '.' && *name_rest != '[')
                    {
                        name_rest++;
                        name_length++;
                    }
                    is_resolved = TRUE;
                    if (struct_type->fields)
                        for (WGL_Field *f = struct_type->fields->list.First(); f; f = f->Suc())
                            if (uni_strncmp(f->name->value, name, name_length) == 0)
                            {
                                v_type = f->type;
                                is_resolved = FALSE;
                                break;
                            }
                }
                else if (*name_rest == '[' && allow_array && v_type->GetType() == WGL_Type::Array)
                {
                    WGL_ArrayType *array_type = static_cast<WGL_ArrayType *>(v_type);
                    while (*name_rest && *name_rest != ']')
                        name_rest++;
                    if (!*name_rest)
                    {
                        // Malformed string: '[' without matching ']'
                        return OpBoolean::IS_FALSE;
                    }
                    name_rest++;
                    if (*name_rest == '.' && allow_struct && array_type->element_type->GetType() == WGL_Type::Struct)
                        v_type = array_type->element_type;
                    else if (*name_rest == '[' && allow_array && array_type->element_type->GetType() == WGL_Type::Array)
                        v_type = array_type->element_type;
                    else
                    {
                        result_type = array_type->element_type;
                        is_resolved = TRUE;
                    }
                }
                else
                {
                    result_type = v_type;
                    is_resolved = TRUE;
                }
            }
        }
        if (!result_type)
            v = v->Suc();
    }

    if (!result_type)
        return OpBoolean::IS_FALSE;

    length = 1;
    is_array = FALSE;
    switch (result_type->GetType())
    {
    case WGL_Type::Basic:
    case WGL_Type::Vector:
    case WGL_Type::Matrix:
    case WGL_Type::Sampler:
        constant_type = GetShaderType(result_type);
        return OpBoolean::IS_TRUE;

    case WGL_Type::Array:
        {
            if (!allow_array)
            {
                OP_ASSERT(!"Unexpected array type");
                return OpBoolean::IS_FALSE;
            }

            WGL_ArrayType *array_type = static_cast<WGL_ArrayType *>(result_type);
            length = array_type->length_value;
            constant_type = GetShaderType(array_type->element_type);
            is_array = TRUE;
            return OpBoolean::IS_TRUE;
        }

    case WGL_Type::Struct:
        {
            if (!allow_struct)
            {
                OP_ASSERT(!"Unexpected struct type");
                return OpBoolean::IS_FALSE;
            }

            WGL_StructType *struct_type = static_cast<WGL_StructType *>(v->type);
            WGL_Field *f = struct_type->fields ? struct_type->fields->list.First() : NULL;
            if (f)
            {
                constant_type = GetShaderType(f->type);
                return OpBoolean::IS_TRUE;
            }
        }

    default:
        OP_ASSERT(!"Unsupported variable type");
        return OpBoolean::IS_FALSE;
    }
}

/* static */ WGL_ShaderVariable *
WGL_ShaderVariables::FindInList(WGL_VarName *var, List<WGL_ShaderVariable> &list)
{
    for (WGL_ShaderVariable *v = list.First(); v; v = v->Suc())
        if (var->Equals(v->name))
            return v;

    return NULL;
}

WGL_ShaderVariable *
WGL_ShaderVariables::FindAttribute(WGL_VarName *var)
{
    return FindInList(var, attributes);
}

WGL_ShaderVariable *
WGL_ShaderVariables::FindUniform(WGL_VarName *var)
{
    return FindInList(var, uniforms);
}

WGL_ShaderVariable *
WGL_ShaderVariables::FindVarying(WGL_VarName *var)
{
    return FindInList(var, varyings);
}

unsigned
WGL_ShaderVariables::GetAttributeCount()
{
    if (attributes_count == UINT_MAX)
        attributes_count = attributes.Cardinal();

    return attributes_count;
}

OP_STATUS
WGL_ShaderVariables::GetAttributeData(unsigned &attr_count, VEGA3D_SHADER_ATTRIBUTE_DATA *&data)
{
    if (attributes_count == UINT_MAX)
        attributes_count = attributes.Cardinal();

    if (attributes_count == 0)
    {
        attr_count = 0;
        data = NULL;
        return OpStatus::OK;
    }
    else
    {
        VEGA3D_SHADER_ATTRIBUTE_DATA *block = OP_NEWA(VEGA3D_SHADER_ATTRIBUTE_DATA, attributes_count);
        if (!block)
            return OpStatus::ERR_NO_MEMORY;
        op_memset(block, 0, attributes_count * sizeof(VEGA3D_SHADER_ATTRIBUTE_DATA));

        WGL_ShaderVariable *v = attributes.First();
        for (unsigned i = 0; i < attributes_count; i++)
        {
            block[i].name = NULL;
            block[i].size = 0;
            if (v->name)
            {
                block[i].name = OP_NEWA(char, uni_strlen(v->name) + 1);
                OpString8 cstr;
                if (!block[i].name || OpStatus::IsError(cstr.Set(v->name)))
                {
                    ReleaseAttributeData(i, block);
                    return OpStatus::ERR_NO_MEMORY;
                }
                else
                    op_strcpy(block[i].name, cstr.CStr());

                block[i].size = WGL_Type::GetTypeSize(v->type);
            }
            v = v->Suc();
        }
        data = block;
        attr_count = attributes_count;
        return OpStatus::OK;
    }
}

/* static */ void
WGL_ShaderVariables::ReleaseAttributeData(unsigned attribute_count, VEGA3D_SHADER_ATTRIBUTE_DATA *attributes)
{
    for (unsigned i = 0; i < attribute_count; i++)
        OP_DELETEA(attributes[i].name);
    OP_DELETEA(attributes);
}

unsigned
WGL_ShaderVariables::GetUniformCount()
{
    if (uniforms_count == UINT_MAX)
    {
        unsigned count = 0;
        for (WGL_ShaderVariable *v = uniforms.First(); v; v = v->Suc())
        {
            count++;
            BOOL is_array = v->type->GetType() == WGL_Type::Array;
            if (is_array || v->type->GetType() == WGL_Type::Struct)
            {
                if (is_array)
                {
                    WGL_ArrayType *array_type = static_cast<WGL_ArrayType *>(v->type);
                    count += array_type->is_constant_value ? array_type->length_value : 0;
                }
                else
                {
                    /* The 'struct' cannot be accessed on its own (but arrays can..) */
                    count--;
                    WGL_StructType *struct_type = static_cast<WGL_StructType *>(v->type);
                    count += struct_type->fields ? struct_type->fields->list.Cardinal() : 0;
                }
            }
        }
        uniforms_count = count;
    }
    return uniforms_count;
}

unsigned
WGL_ShaderVariables::GetVaryingCount()
{
    if (varyings_count == UINT_MAX)
    {
        unsigned count = 0;
        for (WGL_ShaderVariable *v = varyings.First(); v; v = v->Suc())
        {
            count++;
            OP_ASSERT(v->type->GetType() != WGL_Type::Struct);
            if (v->type->GetType() == WGL_Type::Array)
            {
                WGL_ArrayType *array_type = static_cast<WGL_ArrayType *>(v->type);
                count += array_type->is_constant_value ? array_type->length_value : 0;
            }
        }
        varyings_count = count;
    }
    return varyings_count;
}

/* static */ BOOL
WGL_Validator::ValidateLinkageL(Configuration &config, WGL_ShaderVariables *vars1, WGL_ShaderVariables *vars2)
{
    OpMemGroup region;

    WGL_Context *context;
    LEAVE_IF_ERROR(WGL_Context::Make(context, &region, config.support_oes_derivatives));

#ifdef WGL_STANDALONE
    WGL_StdioPrinter printer(stdout, config.output_format == VEGA3D_SHADER_LANGUAGE(SHADER_GLSL_ES), config.support_higp_fragment);
#else
    WGL_ConsolePrinter printer(UNI_L("GLSL linker"), config.output_log, config.console_logging, config.output_format == VEGA3D_SHADER_LANGUAGE(SHADER_GLSL_ES), config.support_higp_fragment);
#endif // WGL_STANDALONE

    if (!vars2)
    {
        /* Perform additional link-time checks on 'vars1', checking that
           overall size is within supported limits. */
        return TRUE;
    }
    else
    {
        WGL_ValidationState::CheckVariableConsistency(context, vars1, vars2);
        if (WGL_Error::HaveErrors(context->GetFirstError()))
        {
            WGL_Error::ReportErrorsL(&printer, context->GetFirstError());
            return FALSE;
        }
        else
        {
            /* Emit the warnings */
            WGL_Error::ReportErrorsL(&printer, context->GetFirstError());
            return TRUE;
        }
    }
}

BOOL
WGL_Validator::ValidateSource(const uni_char *source)
{
    BOOL in_comment = FALSE;
    while (*source)
    {
        if (!in_comment && *source == '/')
        {
            if (source[1] == '*')
            {
                source++;
                in_comment = TRUE;
            }
            else if (source[1] == '/')
            {
                while (*source)
                {
                    if (*source == '\n' || *source == '\r')
                        break;
                    source++;
                }
                if (!*source)
                    break;
            }
        }
        else if (in_comment)
        {
            if (*source == '*' && source[1] == '/')
            {
                source++;
                in_comment = FALSE;
            }
        }
        else
        {
            char ch = static_cast<char>(*source);
            if (!(ch >= 'a' && ch <= 'z' || ch >= 'A' && ch <= 'Z' || ch >= '0' && ch <= '9' || op_strchr("\t\n\v\f\r _.+-/*%<>[](){}^|&~=!:;,?#", ch)))
                return FALSE;
        }
        ++source;
    }

    return TRUE;
}

/* static */ void
WGL_Validator::PrintVersionAndExtensionPrelude(WGL_Printer *printer, unsigned version, WGL_Context *context)
{
    if (version > 100)
    {
        printer->OutString("#version ");
        printer->OutInt(version);
        printer->OutNewline();
    }
    if (printer->ForGLESTarget())
    {
        for (unsigned int i = 0; i < context->GetExtensionCount(); ++i)
        {
            const uni_char *extensionDefine = NULL;
            if (context->IsExtensionEnabled(i, TRUE, extensionDefine))
            {
                printer->OutString("#extension ");
                printer->OutString(extensionDefine);
                printer->OutString(" : enable");
                printer->OutNewline();
            }
        }
    }
}


#endif // CANVAS3D_SUPPORT
