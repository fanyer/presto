/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2012
 *
 * WebGL GLSL compiler -- Cg/HLSL backend
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_string.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_cgbuilder.h"
#include "modules/webgl/src/wgl_visit.h"
#include "modules/webgl/src/wgl_intrinsics.h"
#include "modules/webgl/src/wgl_printer.h"
#include "modules/webgl/src/wgl_pretty_printer.h"

#include "modules/webgl/src/shaders/wgl_cgbuilder_shaders.h"

/* +++ local utils +++ */

static void
BlockBegin(WGL_Printer *out_sink)
{
    out_sink->OutNewline();
    out_sink->OutString("{");
    out_sink->IncreaseIndent();
    out_sink->OutNewline();
}

static void
BlockEnd(WGL_Printer *out_sink)
{
    out_sink->DecreaseIndent();
    out_sink->OutNewline();
    out_sink->OutString("}");
}

void
WGL_CgBuilder::EmitVertexShaderHeader(WGL_Printer *out_sink, BOOL for_attributes)
{
    ResetCounters();
    out_sink->OutNewline();
    if (for_attributes)
        out_sink->OutString("struct "VS_IN_STRUCT_NAME);
    else
        out_sink->OutString("struct "VS_OUT_STRUCT_NAME);
    BlockBegin(out_sink);
}

void
WGL_CgBuilder::EmitPixelShaderHeader(WGL_Printer *out_sink)
{
    ResetCounters();
    out_sink->OutNewline();
    out_sink->OutString("struct "PS_OUT_STRUCT_NAME);
    BlockBegin(out_sink);
}

/* -- local utils --- */

/* static */ BOOL
WGL_CgTypes::IsGlobalVertexVar(WGL_CgBuilder *cg, WGL_VarName *n, WGL_Type *&ty, WGL_ShaderVariable::Kind &kind)
{
    for (unsigned i = 0; i < WGL_GLSLBuiltins::vertex_shader_input_elements; i++)
        if (n->Equals(WGL_GLSLBuiltins::vertex_shader_input[i].name))
        {
            ty = WGL_CgTypes::BuildTypeFromGL(cg, WGL_GLSLBuiltins::vertex_shader_input[i].type);
            kind = WGL_ShaderVariable::Attribute;
            return TRUE;
        }

    for (unsigned i = 0; i < WGL_GLSLBuiltins::vertex_shader_output_elements; i++)
        if (n->Equals(WGL_GLSLBuiltins::vertex_shader_output[i].name))
        {
            ty = WGL_CgTypes::BuildTypeFromGL(cg, WGL_GLSLBuiltins::vertex_shader_output[i].type);
            kind = WGL_ShaderVariable::Varying;
            return TRUE;
        }

    for (unsigned i = 0; i < WGL_GLSLBuiltins::vertex_shader_constant_elements; i++)
        if (n->Equals(WGL_GLSLBuiltins::vertex_shader_constant[i].name))
        {
            ty = cg->builder->BuildBasicType(WGL_GLSLBuiltins::vertex_shader_constant[i].basic_type);
            kind = WGL_ShaderVariable::Uniform;
            return TRUE;
        }

    return FALSE;
}

/* static */ BOOL
WGL_CgTypes::IsGlobalFragmentVar(WGL_CgBuilder *cg, WGL_VarName *n, WGL_Type *&ty, WGL_ShaderVariable::Kind &kind)
{
    for (unsigned i = 0; i < WGL_GLSLBuiltins::fragment_shader_input_elements; i++)
        if (n->Equals(WGL_GLSLBuiltins::fragment_shader_input[i].name))
        {
            ty = WGL_CgTypes::BuildTypeFromGL(cg, WGL_GLSLBuiltins::fragment_shader_input[i].type);
            kind = WGL_ShaderVariable::Attribute;
            return TRUE;
        }

    for (unsigned i = 0; i < WGL_GLSLBuiltins::fragment_shader_output_elements; i++)
        if (n->Equals(WGL_GLSLBuiltins::fragment_shader_output[i].name))
        {
            ty = WGL_CgTypes::BuildTypeFromGL(cg, WGL_GLSLBuiltins::fragment_shader_output[i].type);
            kind = WGL_ShaderVariable::Varying;
            return TRUE;
        }

    for (unsigned i = 0; i < WGL_GLSLBuiltins::fragment_shader_constant_elements; i++)
        if (n->Equals(WGL_GLSLBuiltins::fragment_shader_constant[i].name))
        {
            ty = cg->builder->BuildBasicType(WGL_GLSLBuiltins::fragment_shader_constant[i].basic_type);
            kind = WGL_ShaderVariable::Uniform;
            return TRUE;
        }

    return FALSE;
}

/* static */ void
WGL_CgTypes::OutCgSemantic(WGL_CgBuilder *cg, WGL_Printer *out_sink, const uni_char *sem_name, WGL_HLSLBuiltins::DX9Semantic s)
{
    if (cg->config->for_directx_version > 9)
    {
        switch (s)
        {
        case WGL_HLSLBuiltins::COLOR:
            out_sink->OutString("SV_Target");
            return;
        case WGL_HLSLBuiltins::POSITION:
        case WGL_HLSLBuiltins::VPOS:
            out_sink->OutString("SV_Position");
            return;
        default:
            break;
        }
    }
    out_sink->OutString(sem_name);
}

/* static */ BOOL
WGL_CgTypes::OutCgKnownVar(WGL_CgBuilder *cg, WGL_Printer *out_sink, const uni_char *name)
{
    const WGL_HLSLBuiltins::WGL_HLSLName *const tm = WGL_HLSLBuiltins::glsl_hlsl_type_map;

    for (unsigned i = 0; i < WGL_HLSLBuiltins::glsl_hlsl_type_map_elements; i++)
        if (uni_str_eq(name, tm[i].glsl_name))
        {
            out_sink->OutString(tm[i].hlsl_type_name);
            out_sink->OutString(" ");
            out_sink->OutString(tm[i].hlsl_name);
            if (tm[i].hlsl_semantics && *tm[i].hlsl_semantics)
            {
                out_sink->OutString(": ");
                OutCgSemantic(cg, out_sink, tm[i].hlsl_semantics, tm[i].hlsl_semantic);
                int idx = cg->NextCounter(tm[i].hlsl_semantic);
                if (idx >= 0)
                    out_sink->OutInt(idx);
            }
            return TRUE;
        }

    return FALSE;
}

/* static */ void
WGL_CgTypes::OutCgType(WGL_CgBuilder *cg, WGL_SamplerType *ty)
{
    for (unsigned i = 0; i < WGL_HLSLBuiltins::sampler_type_map_elements; i++)
        if (ty->type == WGL_HLSLBuiltins::sampler_type_map[i].type)
        {
            cg->out_sink->OutString(WGL_HLSLBuiltins::sampler_type_map[i].name);
            return;
        }
}

/* static */ void
WGL_CgTypes::OutCgType(WGL_CgBuilder *cg, WGL_MatrixType *ty)
{
    for (unsigned i = 0; i < WGL_HLSLBuiltins::matrix_type_map_elements; i++)
        if (ty->type == WGL_HLSLBuiltins::matrix_type_map[i].type)
        {
            cg->out_sink->OutString(WGL_HLSLBuiltins::matrix_type_map[i].name);
            return;
        }
}

/* static */ void
WGL_CgTypes::OutCgType(WGL_CgBuilder *cg, WGL_VectorType *ty)
{
    for (unsigned i = 0; i < WGL_HLSLBuiltins::vector_type_map_elements; i++)
        if (ty->type == WGL_HLSLBuiltins::vector_type_map[i].type)
        {
            cg->out_sink->OutString(WGL_HLSLBuiltins::vector_type_map[i].name);
            return;
        }
}

/* static */ void
WGL_CgTypes::OutCgType(WGL_CgBuilder *cg, WGL_BasicType *ty)
{
    for (unsigned i = 0; i < WGL_HLSLBuiltins::base_type_map_elements; i++)
        if (ty->type == WGL_HLSLBuiltins::base_type_map[i].type)
        {
            cg->out_sink->OutString(WGL_HLSLBuiltins::base_type_map[i].name);
            return;
        }
}

/* static */ void
WGL_CgTypes::OutCgType(WGL_CgBuilder *cg, WGL_Type *ty)
{
    switch (ty->GetType())
    {
    case WGL_Type::Basic:
        WGL_CgTypes::OutCgType(cg, static_cast<WGL_BasicType *>(ty));
        break;
    case WGL_Type::Vector:
        WGL_CgTypes::OutCgType(cg, static_cast<WGL_VectorType *>(ty));
        break;
    case WGL_Type::Matrix:
        WGL_CgTypes::OutCgType(cg, static_cast<WGL_MatrixType *>(ty));
        break;
    case WGL_Type::Sampler:
        WGL_CgTypes::OutCgType(cg, static_cast<WGL_SamplerType *>(ty));
        break;
    default:
        ty->VisitType(&cg->out_gen);
        break;
    }
}

/* static */ WGL_Type *
WGL_CgTypes::BuildTypeFromGL(WGL_CgBuilder *cg, WGL_GLSL::GLType t)
{
    switch(t)
    {
    case WGL_GLSL::GL_Void:
        return cg->builder->BuildBasicType(WGL_BasicType::Void);
    case WGL_GLSL::GL_Float:
        return cg->builder->BuildBasicType(WGL_BasicType::Float);
    case WGL_GLSL::GL_Int:
        return cg->builder->BuildBasicType(WGL_BasicType::Int);
    case WGL_GLSL::GL_UInt:
        return cg->builder->BuildBasicType(WGL_BasicType::UInt);
    case WGL_GLSL::GL_Bool:
        return cg->builder->BuildBasicType(WGL_BasicType::Bool);
    case WGL_GLSL::GL_Array_Float:
        return cg->builder->BuildArrayType(cg->builder->BuildBasicType(WGL_BasicType::Float), NULL);
    case WGL_GLSL::GL_Array_Vec4:
        return cg->builder->BuildArrayType(BuildTypeFromGL(cg, WGL_GLSL::GL_Vec4), NULL);
    case WGL_GLSL::GL_Vec2:
        return cg->context->NewTypeName(UNI_L("float2"));
    case WGL_GLSL::GL_Vec3:
        return cg->context->NewTypeName(UNI_L("float3"));
    case WGL_GLSL::GL_Vec4:
        return cg->context->NewTypeName(UNI_L("float4"));
    case WGL_GLSL::GL_Mat2:
        return cg->context->NewTypeName(UNI_L("float2x2"));
    case WGL_GLSL::GL_Mat3:
        return cg->context->NewTypeName(UNI_L("float3x3"));
    case WGL_GLSL::GL_Mat4:
        return cg->context->NewTypeName(UNI_L("float4x4"));
    default:
        OP_ASSERT(!"Unhandled GL type");
        return NULL;
    }
}

void
WGL_CgGenerator::UsesIntrinsic(WGL_VarExpr *fun, WGL_ExprList *args)
{
    if (fun->intrinsic > 0)
    {
        Intrinsic uses_intrinsic = None;
        if (IntrinsicHandler handler = intrinsics_map[fun->intrinsic])
        {
            /* Call the rewrite method pointed at by the pointer-to-member
               'handler'. This uses the C++-specific ->* operator to specify
               the object ('this' pointer) to use. */
            if ((this->*handler)(fun, args, &uses_intrinsic) == NULL)
                if (uses_intrinsic != None)
                    RegisterIntrinsicUse(uses_intrinsic);
        }
    }
}

void
WGL_CgGenerator::RegisterIntrinsicUse(Intrinsic i)
{
    const unsigned idx = i / (sizeof(unsigned) * 8);
    const unsigned bit = i % (sizeof(unsigned) * 8);
    intrinsics_uses[idx] |= 0x1 << bit;
}

void
WGL_CgGenerator::EmitIntrinsicFunction(Intrinsic f)
{
    OP_ASSERT(f > 0 && f < LAST_INTRINSIC);
    printer->OutString(glsl_intrinsic_functions[f-1]);
    printer->OutNewline();
}

void
WGL_CgGenerator::EmitIntrinsics()
{
    Intrinsic used = None;
    for (unsigned i = 0; i < 1 + LAST_INTRINSIC / (sizeof(unsigned) * 8); i++)
        for (unsigned j = 0; j < sizeof(unsigned) * 8; j++)
        {
            if (used >= LAST_INTRINSIC)
                return;
            if ((intrinsics_uses[i] & (0x1 << j)) != 0)
                EmitIntrinsicFunction(used);

            used = static_cast<Intrinsic>(static_cast<unsigned>(used) + 1);
        }
}

WGL_Expr *
WGL_CgGenerator::RewriteAtan(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    if (uses_intrinsic)
        return NULL;

    const uni_char *hlsl_fun = args->list.Cardinal() == 2 ? UNI_L("atan2") : UNI_L("atan");

    WGL_VarExpr f(hlsl_fun);
    WGL_CallExpr call_expr(&f, args);
    return WGL_PrettyPrinter::VisitExpr(&call_expr);
}

WGL_Expr *
WGL_CgGenerator::RewriteFract(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    if (uses_intrinsic)
        return NULL;

    const uni_char *hlsl_fun = UNI_L("frac");

    WGL_VarExpr f(hlsl_fun);
    WGL_CallExpr call_expr(&f, args);
    return WGL_PrettyPrinter::VisitExpr(&call_expr);
}

WGL_Expr *
WGL_CgGenerator::RewriteInversesqrt(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    if (uses_intrinsic)
        return NULL;

    const uni_char *hlsl_fun = UNI_L("rsqrt");

    WGL_VarExpr f(hlsl_fun);
    WGL_CallExpr call_expr(&f, args);
    return WGL_PrettyPrinter::VisitExpr(&call_expr);
}

WGL_Expr *
WGL_CgGenerator::RewriteMod(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    OP_ASSERT(args->list.Cardinal() == 2);
    WGL_Type *arg1_type = args->list.First()->GetExprType();
    WGL_Type *arg2_type = args->list.First()->Suc()->GetExprType();

    Intrinsic use_intrinsic = None;
    if (!WGL_Type::HasBasicType(arg1_type, WGL_BasicType::Float) && WGL_Type::HasBasicType(arg2_type, WGL_BasicType::Float))
    {
        // Don't see mod(V/M,F) supported, so provide our own family..
        BOOL is_vector = WGL_Type::IsVectorType(arg1_type);
        OP_ASSERT(is_vector);
        if (is_vector)
        {
            WGL_VectorType *vec = static_cast<WGL_VectorType *>(arg1_type);
            unsigned mag = WGL_VectorType::GetMagnitude(vec);

            if (mag == 2)
                use_intrinsic = ModFVec2;
            else if (mag == 3)
                use_intrinsic = ModFVec3;
            else
                use_intrinsic = ModFVec4;
        }
    }
    if (uses_intrinsic)
    {
        *uses_intrinsic = use_intrinsic;
        return NULL;
    }

    // In GLSL mod(x,y)=x-y*floor(x/y) while in HLSL fmod(x,y)=x-y*trunc(x/y) so
    // we need to rewrite it without using the HLSL intrinsic.

    // x/y
    WGL_Expr *x = args->list.First();
    WGL_Expr *y = args->list.Last();
    WGL_BinaryExpr x_over_y(WGL_Expr::OpDiv, x, y);
    // floor(x/y)
    WGL_ExprList floor_params;
    floor_params.Add(&x_over_y);
    WGL_VarExpr floor_func(UNI_L("floor"));
    WGL_CallExpr floor_call_expr(&floor_func, &floor_params);
    // y * floor(x/y)
    WGL_BinaryExpr y_mul_floor(WGL_Expr::OpMul, y, &floor_call_expr);
    // x - y * floor(x/y)
    WGL_BinaryExpr sub(WGL_Expr::OpSub, x, &y_mul_floor);
    WGL_Expr *ret = sub.VisitExpr(this);
    x_over_y.Out();
    return ret;
}

WGL_Expr *
WGL_CgGenerator::RewriteVectorRelational(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    if (uses_intrinsic)
        return NULL;

    WGL_Expr::Operator op = WGL_Expr::OpLt;
    switch (fun->intrinsic)
    {
    case WGL_GLSL::LessThan:
        op = WGL_Expr::OpLt;
        break;
    case WGL_GLSL::LessThanEqual:
        op = WGL_Expr::OpLe;
        break;
    case WGL_GLSL::GreaterThan:
        op = WGL_Expr::OpGt;
        break;
    case WGL_GLSL::GreaterThanEqual:
        op = WGL_Expr::OpGe;
        break;
    default:
        OP_ASSERT(!"Unexpected vector relational intrinsic function");
        break;
    }

    WGL_Expr *arg1 = args->list.First();
    WGL_Expr *arg2 = arg1->Suc();
    WGL_BinaryExpr bop(op, arg1, arg2);
    return bop.VisitExpr(this);
}

static WGL_GLSLBuiltins::TextureOp
ResolveTextureFun(WGL_VarExpr *fun, WGL_ExprList *args)
{
    WGL_GLSLBuiltins::TextureOp op = WGL_GLSLBuiltins::Texture2D;
    switch (fun->intrinsic)
    {
    case WGL_GLSL::Texture2D:
        if (args->list.Cardinal() == 3)
            op = WGL_GLSLBuiltins::Texture2DBias;
        else
            op = WGL_GLSLBuiltins::Texture2D;
        break;
    case WGL_GLSL::Texture2DProj:
    {
        BOOL is_vec3 = args->list.First()->Suc()->GetExprType()->GetType() == WGL_Type::Vector &&
                       WGL_VectorType::GetMagnitude(static_cast<WGL_VectorType *>(args->list.First()->Suc()->GetExprType())) == 3;
        if (args->list.Cardinal() == 3)
        {
            if (is_vec3)
                op = WGL_GLSLBuiltins::Texture2DProj3Bias;
            else
                op = WGL_GLSLBuiltins::Texture2DProj4Bias;
        }
        else if (!is_vec3)
            op = WGL_GLSLBuiltins::Texture2DProj4;
        else
            op = WGL_GLSLBuiltins::Texture2DProj3;
        break;
    }
    case WGL_GLSL::Texture2DLod:
        op = WGL_GLSLBuiltins::Texture2DLod;
        break;
    case WGL_GLSL::Texture2DProjLod:
        if (args->list.First()->Suc()->GetExprType()->GetType() != WGL_Type::Vector ||
            WGL_VectorType::GetMagnitude(static_cast<WGL_VectorType *>(args->list.First()->Suc()->GetExprType())) != 3)
            op = WGL_GLSLBuiltins::Texture2DProj4Lod;
        else
            op = WGL_GLSLBuiltins::Texture2DProj3Lod;
        break;
    case WGL_GLSL::TextureCube:
        if (args->list.Cardinal() == 3)
            op = WGL_GLSLBuiltins::TextureCubeBias;
        else
            op = WGL_GLSLBuiltins::TextureCube;
        break;
    case WGL_GLSL::TextureCubeLod:
        op = WGL_GLSLBuiltins::TextureCubeLod;
        break;
    default:
        OP_ASSERT(!"Unexpected texture intrinsic function");
        break;
    }
    return op;
}

static const uni_char *
LookupTextureFun(WGL_GLSLBuiltins::TextureOp op, WGL_CgGenerator::Intrinsic &uses_intrinsic, BOOL &as_method, unsigned directx_version)
{
    switch (op)
    {
    case WGL_GLSLBuiltins::Texture2D:
        if (directx_version > 9)
        {
            as_method = TRUE;
            return UNI_L("Sample");
        }
        else
            return UNI_L("tex2D");
    case WGL_GLSLBuiltins::Texture2DBias:
        uses_intrinsic = WGL_CgGenerator::Texture2DBias;
        break;
    case WGL_GLSLBuiltins::Texture2DProj3:
        uses_intrinsic = WGL_CgGenerator::Texture2DProj3;
        break;
    case WGL_GLSLBuiltins::Texture2DProj3Bias:
        uses_intrinsic = WGL_CgGenerator::Texture2DProj3Bias;
        break;
    case WGL_GLSLBuiltins::Texture2DProj4:
        return UNI_L("tex2Dproj");
    case WGL_GLSLBuiltins::Texture2DProj4Bias:
        uses_intrinsic = WGL_CgGenerator::Texture2DProj4Bias;
        break;
    case WGL_GLSLBuiltins::Texture2DLod:
        uses_intrinsic = WGL_CgGenerator::Texture2DLod;
        break;
    case WGL_GLSLBuiltins::Texture2DProj3Lod:
        uses_intrinsic = WGL_CgGenerator::Texture2DProj3Lod;
        break;
    case WGL_GLSLBuiltins::Texture2DProj4Lod:
        uses_intrinsic = WGL_CgGenerator::Texture2DProj4Lod;
        break;
    case WGL_GLSLBuiltins::TextureCube:
        if (directx_version > 9)
        {
            as_method = TRUE;
            return UNI_L("Sample");
        }
        else
            return UNI_L("texCUBE");
    case WGL_GLSLBuiltins::TextureCubeBias:
        uses_intrinsic = WGL_CgGenerator::TextureCubeBias;
        break;
    case WGL_GLSLBuiltins::TextureCubeLod:
        uses_intrinsic = WGL_CgGenerator::TextureCubeLod;
        break;
    default:
        OP_ASSERT(!"Unexpected texture intrinsic");
        return NULL;
    }

    return WGL_CgGenerator::LookupIntrinsicName(uses_intrinsic);
}

#define HLSL_UNI_L_NONSENSE(x) UNI_L(#x)
#define HLSL_INTRINSIC(x) HLSL_UNI_L_NONSENSE(webgl_op_ ## x)

/* static */ const uni_char *
WGL_CgGenerator::LookupIntrinsicName(Intrinsic i)
{
    static const uni_char *names[] = {
        HLSL_INTRINSIC(mod_fvec2),
        HLSL_INTRINSIC(mod_fvec3),
        HLSL_INTRINSIC(mod_fvec4),
        HLSL_INTRINSIC(min_fvec2),
        HLSL_INTRINSIC(min_fvec3),
        HLSL_INTRINSIC(min_fvec4),
        HLSL_INTRINSIC(max_fvec2),
        HLSL_INTRINSIC(max_fvec3),
        HLSL_INTRINSIC(max_fvec4),
        HLSL_INTRINSIC(clamp_fvec2),
        HLSL_INTRINSIC(clamp_fvec3),
        HLSL_INTRINSIC(clamp_fvec4),
        HLSL_INTRINSIC(lerp_fvec2),
        HLSL_INTRINSIC(lerp_fvec3),
        HLSL_INTRINSIC(lerp_fvec4),
        HLSL_INTRINSIC(step_fvec2),
        HLSL_INTRINSIC(step_fvec3),
        HLSL_INTRINSIC(step_fvec4),
        HLSL_INTRINSIC(smoothstep_fvec2),
        HLSL_INTRINSIC(smoothstep_fvec3),
        HLSL_INTRINSIC(smoothstep_fvec4),
        HLSL_INTRINSIC(matcompmult_mat2),
        HLSL_INTRINSIC(matompmult_mat3),
        HLSL_INTRINSIC(matcompmult_mat4),
        HLSL_INTRINSIC(equal_fvec2),
        HLSL_INTRINSIC(equal_fvec3),
        HLSL_INTRINSIC(equal_fvec4),
        HLSL_INTRINSIC(equal_ivec2),
        HLSL_INTRINSIC(equal_ivec3),
        HLSL_INTRINSIC(equal_ivec4),
        HLSL_INTRINSIC(equal_bvec2),
        HLSL_INTRINSIC(equal_bvec3),
        HLSL_INTRINSIC(equal_bvec4),
        HLSL_INTRINSIC(tex2Dbias),
        HLSL_INTRINSIC(tex2Dproj3),
        HLSL_INTRINSIC(tex2Dproj3bias),
        HLSL_INTRINSIC(tex2Dproj4bias),
        HLSL_INTRINSIC(tex2Dlod),
        HLSL_INTRINSIC(tex2Dproj3lod),
        HLSL_INTRINSIC(tex2Dproj4lod),
        HLSL_INTRINSIC(texCUBEbias),
        HLSL_INTRINSIC(texCUBElod),
        HLSL_INTRINSIC(mat2ToMat3),
        HLSL_INTRINSIC(mat2ToMat4),
        HLSL_INTRINSIC(mat3ToMat4)
    };

    if (i == 0 || i >= LAST_INTRINSIC)
        return NULL;
    else
        return names[i - 1];
}

WGL_Expr *
WGL_CgGenerator::RewriteTextureFun(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    WGL_GLSLBuiltins::TextureOp op = ResolveTextureFun(fun, args);
    Intrinsic with_intrinsic = None;
    unsigned dx_version = cg->GetConfiguration()->for_directx_version;
    BOOL as_method = FALSE;
    const uni_char *tex_fun = LookupTextureFun(op, with_intrinsic, as_method, dx_version);

    if (uses_intrinsic)
    {
        *uses_intrinsic = with_intrinsic;
        return NULL;
    }

    if (dx_version > 9 && as_method)
    {
        WGL_Expr *obj = args->list.First();
        obj->Out();

        /* TODO: This does not deal with cases where there isn't a
           variable name directly in the first argument position (e.g.
           'texture2D(sampler_array[i], ...)' or
           'texture2D(foo ? tex_1 : tex_2, ...)'). */

        WGL_VarName *sampler_name = NULL;
        if (obj->GetType() == WGL_Expr::Var)
        {
            WGL_VarExpr *var = static_cast<WGL_VarExpr *>(obj);
            const uni_char *var_name = Storage(cg->GetContext(), var->name);
            const uni_char *sampler_suff = UNI_L("Sampler");
            uni_char *sampler_var_name = OP_NEWGROA_L(uni_char, uni_strlen(var_name) + uni_strlen(sampler_suff) + 1, cg->GetContext()->Arena());
            uni_strcpy(sampler_var_name, var_name);
            uni_strcpy(sampler_var_name + uni_strlen(var_name), sampler_suff);
            sampler_name = WGL_String::Make(cg->GetContext(), sampler_var_name);
        }
        if (sampler_name)
        {
            WGL_SelectExpr method_invoke(obj, WGL_String::Make(cg->GetContext(), tex_fun));
            WGL_VarExpr sampler(sampler_name);
            sampler.IntoStart(&args->list);
            WGL_CallExpr tex_method(&method_invoke, args);
            WGL_Expr *result = tex_method.VisitExpr(this);
            /* Very untidy, but restore contents of 'args'. */
            sampler.Out();
            obj->Into(&args->list);
            return result;
        }
    }

    WGL_VarExpr f(tex_fun);
    WGL_CallExpr call_expr(&f, args);
    return call_expr.VisitExpr(this);
}

WGL_Expr *
WGL_CgGenerator::RewriteNot(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    if (uses_intrinsic)
        return NULL;
    WGL_Expr *arg1 = args->list.First();
    WGL_UnaryExpr uop(WGL_Expr::OpNegate, arg1);
    return uop.VisitExpr(this);
}

WGL_Expr *
WGL_CgGenerator::RewriteVectorEqual(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    BOOL is_equal = fun->intrinsic == WGL_GLSL::Equal;
    WGL_Type *arg1_type = args->list.First()->GetExprType();
    OP_ASSERT(arg1_type->GetType() == WGL_Type::Vector);

    Intrinsic use_intrinsic = None;

    WGL_VectorType *vec = static_cast<WGL_VectorType *>(arg1_type);
    unsigned magnitude = WGL_VectorType::GetMagnitude(vec);

    switch (WGL_VectorType::GetElementType(vec))
    {
    case WGL_BasicType::Float:
        switch (magnitude)
        {
        case 2:
            use_intrinsic = EqualVec2;
            break;
        case 3:
            use_intrinsic = EqualVec3;
            break;
        default:
        case 4:
            use_intrinsic = EqualVec4;
            break;
        }
        break;
    case WGL_BasicType::UInt:
    case WGL_BasicType::Int:
        switch (magnitude)
        {
        case 2:
            use_intrinsic = EqualIVec2;
            break;
        case 3:
            use_intrinsic = EqualIVec3;
            break;
        default:
        case 4:
            use_intrinsic = EqualIVec4;
            break;
        }
        break;
    case WGL_BasicType::Bool:
        switch (magnitude)
        {
        case 2:
            use_intrinsic = EqualBVec2;
            break;
        case 3:
            use_intrinsic = EqualBVec3;
            break;
        default:
        case 4:
            use_intrinsic = EqualBVec4;
            break;
        }
        break;
    }

    if (uses_intrinsic)
    {
        *uses_intrinsic = use_intrinsic;
        return NULL;
    }

    const uni_char *hlsl_fun = LookupIntrinsicName(use_intrinsic);
    WGL_VarExpr intrinsic(hlsl_fun);
    WGL_CallExpr call_expr(&intrinsic, args);
    if (!is_equal)
    {
        WGL_UnaryExpr uop(WGL_Expr::OpNegate, &call_expr);
        return uop.VisitExpr(this);
    }
    else
        return call_expr.VisitExpr(this);
}

WGL_Expr *
WGL_CgGenerator::RewriteMatrixCompMult(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    WGL_Type *arg1_type = args->list.First()->GetExprType();

    BOOL is_matrix = WGL_Type::IsMatrixType(arg1_type);
    OP_ASSERT(is_matrix);
    if (is_matrix)
    {
        WGL_MatrixType *mat = static_cast<WGL_MatrixType *>(arg1_type);
        unsigned cols = WGL_MatrixType::GetColumns(mat);
        Intrinsic use_intrinsic = None;
        switch (cols)
        {
        case 2:
            use_intrinsic = MatCompMult2;
            break;
        case 3:
            use_intrinsic = MatCompMult3;
            break;
        default:
            OP_ASSERT(!"Unexpected matrix size");
        case 4:
            use_intrinsic = MatCompMult4;
            break;
        }

        if (uses_intrinsic)
        {
            *uses_intrinsic = use_intrinsic;
            return NULL;
        }

        const uni_char *hlsl_fun = LookupIntrinsicName(use_intrinsic);
        WGL_VarExpr fun(hlsl_fun);
        WGL_CallExpr call_expr(&fun, args);
        return call_expr.VisitExpr(this);
    }
    return NULL;
}

WGL_Expr *
WGL_CgGenerator::RewriteMix(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    WGL_Type *arg1_type = args->list.First()->GetExprType();
    WGL_Type *arg3_type = args->list.First()->Suc()->Suc()->GetExprType();

    const uni_char *hlsl_fun = UNI_L("lerp");
    Intrinsic use_intrinsic = None;
    if (!WGL_Type::HasBasicType(arg1_type, WGL_BasicType::Float) && WGL_Type::HasBasicType(arg3_type, WGL_BasicType::Float))
    {
        BOOL is_vector = WGL_Type::IsVectorType(arg1_type);
        OP_ASSERT(is_vector);
        if (is_vector)
        {
            WGL_VectorType *vec = static_cast<WGL_VectorType *>(arg1_type);
            switch (WGL_VectorType::GetMagnitude(vec))
            {
            case 2:
                use_intrinsic = LerpFVec2;
                break;
            case 3:
                use_intrinsic = LerpFVec3;
                break;
            default:
                OP_ASSERT(!"Unexpected vector magnitude");
            case 4:
                use_intrinsic = LerpFVec4;
                break;
            }
        }
    }
    if (uses_intrinsic)
    {
        *uses_intrinsic = use_intrinsic;
        return NULL;
    }
    else if (use_intrinsic != None)
        hlsl_fun = LookupIntrinsicName(use_intrinsic);

    WGL_VarExpr f(hlsl_fun);
    WGL_CallExpr call_expr(&f, args);
    return call_expr.VisitExpr(this);
}

WGL_Expr *
WGL_CgGenerator::RewriteStep(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    BOOL is_smooth = fun->intrinsic == WGL_GLSL::Smoothstep;
    WGL_Type *arg1_type = args->list.First()->GetExprType();
    WGL_Type *arg2_type = is_smooth ? args->list.First()->Suc()->Suc()->GetExprType() : args->list.First()->Suc()->GetExprType();

    if (WGL_Type::HasBasicType(arg1_type, WGL_BasicType::Float) && !WGL_Type::HasBasicType(arg2_type, WGL_BasicType::Float))
    {
        BOOL is_vector = WGL_Type::IsVectorType(arg2_type);
        OP_ASSERT(is_vector);
        if (is_vector)
        {
            WGL_VectorType *vec = static_cast<WGL_VectorType *>(arg2_type);
            Intrinsic use_intrinsic = None;
            switch (WGL_VectorType::GetMagnitude(vec))
            {
            case 2:
                use_intrinsic = is_smooth ? SmoothStepFVec2 : StepFVec2;
                break;
            case 3:
                use_intrinsic = is_smooth ? SmoothStepFVec3 : StepFVec3;
                break;
            default:
                OP_ASSERT(!"Unexpected vector magnitude");
            case 4:
                use_intrinsic = is_smooth ? SmoothStepFVec4 : StepFVec4;
                break;
            }

            if (uses_intrinsic)
            {
                *uses_intrinsic = use_intrinsic;
                return NULL;
            }

            const uni_char *hlsl_fun = LookupIntrinsicName(use_intrinsic);
            WGL_VarExpr fun(hlsl_fun);
            WGL_CallExpr call_expr(&fun, args);
            return call_expr.VisitExpr(this);

        }
    }
    if (uses_intrinsic)
        return NULL;

    WGL_CallExpr f(fun, args);
    return WGL_PrettyPrinter::VisitExpr(&f);
}

WGL_Expr *
WGL_CgGenerator::RewriteMinMax(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    BOOL is_min = fun->intrinsic == WGL_GLSL::Min;
    WGL_Type *arg1_type = args->list.First()->GetExprType();
    WGL_Type *arg2_type = args->list.First()->Suc()->GetExprType();

    if (!WGL_Type::HasBasicType(arg1_type, WGL_BasicType::Float) && WGL_Type::HasBasicType(arg2_type, WGL_BasicType::Float))
    {
        /* Same thing for min/max & (V, F) overloads. */
        BOOL is_vector = WGL_Type::IsVectorType(arg1_type);
        OP_ASSERT(is_vector);
        if (is_vector)
        {
            WGL_VectorType *vec = static_cast<WGL_VectorType *>(arg1_type);
            Intrinsic use_intrinsic = None;
            switch (WGL_VectorType::GetMagnitude(vec))
            {
            case 2:
                use_intrinsic = is_min ? MinFVec2 : MaxFVec2;
                break;
            case 3:
                use_intrinsic = is_min ? MinFVec3 : MaxFVec3;
                break;
            default:
                OP_ASSERT(!"Unexpected vector magnitude");
            case 4:
                use_intrinsic = is_min ? MinFVec4 : MaxFVec4;
                break;
            }
            if (uses_intrinsic)
            {
                *uses_intrinsic = use_intrinsic;
                return NULL;
            }

            const uni_char *hlsl_fun = LookupIntrinsicName(use_intrinsic);
            WGL_VarExpr fun(hlsl_fun);
            WGL_CallExpr call_expr(&fun, args);
            return call_expr.VisitExpr(this);
        }
    }
    if (uses_intrinsic)
        return NULL;

    WGL_CallExpr f(fun, args);
    return WGL_PrettyPrinter::VisitExpr(&f);
}

WGL_Expr *
WGL_CgGenerator::RewriteClamp(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic)
{
    WGL_Type *arg1_type = args->list.First()->GetExprType();
    WGL_Type *arg2_type = args->list.First()->Suc()->GetExprType();

    if (!WGL_Type::HasBasicType(arg1_type, WGL_BasicType::Float) && WGL_Type::HasBasicType(arg2_type, WGL_BasicType::Float))
    {
        /* Same thing for min/max & (V, F) overloads. */
        BOOL is_vector = WGL_Type::IsVectorType(arg1_type);
        OP_ASSERT(is_vector);
        if (is_vector)
        {
            WGL_VectorType *vec = static_cast<WGL_VectorType *>(arg1_type);
            Intrinsic use_intrinsic = None;

            switch (WGL_VectorType::GetMagnitude(vec))
            {
            case 2:
                use_intrinsic = ClampFVec2;
                break;
            case 3:
                use_intrinsic = ClampFVec3;
                break;
            default:
                OP_ASSERT(!"Unexpected vector magnitude");
            case 4:
                use_intrinsic = ClampFVec4;
                break;
            }
            if (uses_intrinsic)
            {
                *uses_intrinsic = use_intrinsic;
                return NULL;
            }

            const uni_char *hlsl_fun = LookupIntrinsicName(use_intrinsic);
            WGL_VarExpr fun(hlsl_fun);
            WGL_CallExpr call_expr(&fun, args);
            return call_expr.VisitExpr(this);
        }
    }
    if (uses_intrinsic)
        return NULL;

    WGL_CallExpr f(fun, args);
    return WGL_PrettyPrinter::VisitExpr(&f);
}

static const uni_char *
VectorTypeToString(WGL_VectorType *t)
{
    // TODO: type_qualifier/precision/implicit_precision
    switch(t->type)
    {
    case WGL_VectorType::Vec2: return UNI_L("float2");
    case WGL_VectorType::Vec3: return UNI_L("float3");
    case WGL_VectorType::Vec4: return UNI_L("float4");

    /* FIXME: bogus mapping, but outputting to flush the issue out. */
    case WGL_VectorType::BVec2: return UNI_L("bool2");
    case WGL_VectorType::BVec3: return UNI_L("bool3");
    case WGL_VectorType::BVec4: return UNI_L("bool4");
    case WGL_VectorType::IVec2: return UNI_L("int2");
    case WGL_VectorType::IVec3: return UNI_L("int3");
    case WGL_VectorType::IVec4: return UNI_L("int4");
    case WGL_VectorType::UVec2: return UNI_L("uint2");
    case WGL_VectorType::UVec3: return UNI_L("uint3");
    case WGL_VectorType::UVec4: return UNI_L("uint4");
    default:
        OP_ASSERT(!"Unknown vector type");
        return UNI_L("");
    }
}

static const uni_char *
VectorMagnitudeToString(WGL_BasicType::Type t, unsigned size)
{
    switch (t)
    {
    case WGL_BasicType::Float:
        if (size == 2)
            return UNI_L("float2");
        else if (size == 3)
            return UNI_L("float3");
        else
            return UNI_L("float4");

    case WGL_BasicType::Bool:
        if (size == 2)
            return UNI_L("bool2");
        else if (size == 3)
            return UNI_L("bool3");
        else
            return UNI_L("bool4");

    case WGL_BasicType::Int:
        if (size == 2)
            return UNI_L("int2");
        else if (size == 3)
            return UNI_L("int3");
        else
            return UNI_L("int4");

    case WGL_BasicType::UInt:
        if (size == 2)
            return UNI_L("uint2");
        else if (size == 3)
            return UNI_L("uint3");
        else
            return UNI_L("uint4");

    default:
        OP_ASSERT(!"Unexpected basic type");
        return UNI_L("");
    }
}

static const uni_char *
MatrixTypeToString(WGL_MatrixType *t)
{
    switch(t->type)
    {
    case WGL_MatrixType::Mat2:   return UNI_L("float2x2");
    case WGL_MatrixType::Mat3:   return UNI_L("float3x3");
    case WGL_MatrixType::Mat4:   return UNI_L("float4x4");
    case WGL_MatrixType::Mat2x2: return UNI_L("float2x2");
    case WGL_MatrixType::Mat2x3: return UNI_L("float2x3");
    case WGL_MatrixType::Mat2x4: return UNI_L("float2x4");
    case WGL_MatrixType::Mat3x2: return UNI_L("float3x2");
    case WGL_MatrixType::Mat3x3: return UNI_L("float3x3");
    case WGL_MatrixType::Mat3x4: return UNI_L("float3x4");
    case WGL_MatrixType::Mat4x2: return UNI_L("float4x2");
    case WGL_MatrixType::Mat4x3: return UNI_L("float4x3");
    case WGL_MatrixType::Mat4x4: return UNI_L("float4x4");
    default:
        OP_ASSERT(!"Unknown matrix type");
        return UNI_L("");
    }
}

WGL_Expr *
WGL_CgGenerator::VisitExpr(WGL_CallExpr *e)
{
    if (e->fun->GetType() == WGL_Expr::TypeConstructor)
    {
        WGL_TypeConstructorExpr *const ctor_expr = static_cast<WGL_TypeConstructorExpr *>(e->fun);
        WGL_Type *const ctor_type = ctor_expr->constructor;

        const BOOL is_vector_ctor = WGL_Type::IsVectorType(ctor_type);
        const BOOL is_matrix_ctor = WGL_Type::IsMatrixType(ctor_type);

        unsigned arg_width = WGL_ExprList::ComputeWidth(e->args);
        unsigned type_size = WGL_Type::GetTypeSize(ctor_type);
        unsigned argc = e->args->list.Cardinal();
        if (argc > 1 && arg_width <= type_size)
            goto no_rewrite;

        WGL_Expr *const arg = e->args->list.First();
        if (!is_vector_ctor && !is_matrix_ctor)
        {
            if (argc == 1 && !WGL_Type::HasBasicType(arg->GetExprType()))
            {
                ctor_expr->VisitExpr(this);
                printer->OutString("((");
                arg->VisitExpr(this);
                printer->OutString(").x)");
                return e;
            }
            else if (argc > 1)
            {
                OP_ASSERT(arg_width > type_size);
                ctor_expr->VisitExpr(this);
                printer->OutString("(");
                printer->OutString(VectorMagnitudeToString(WGL_BasicType::GetBasicType(ctor_type), arg_width));
                e->args->VisitExprList(this);
                printer->OutString(".x)");
                return e;
            }
            else
                goto no_rewrite;
        }

        if (argc == 1 && (arg->GetType() == WGL_Expr::Lit || WGL_Type::HasBasicType(arg->GetExprType())))
        {
            /* We have a vector or matrix constructor with a single scalar argument.
               Rewritten as follows:

               vec2(2)        => ((float2)(2))
               vec3(f(n) + 2) => ((float3)(f(n) + 2))

               mat3(f(n) + 2) => ((f(n) + 2) * float3x3(1,0,0, 0,1,0, 0,0,1)) */

            if (is_vector_ctor)
            {
                printer->OutString("((");
                printer->OutString(VectorTypeToString(static_cast<WGL_VectorType *>(ctor_type)));
                printer->OutString(")(");
                arg->VisitExpr(this);
                printer->OutString("))");
            }
            else
            {
                OP_ASSERT(is_matrix_ctor);

                WGL_MatrixType *const matrix_type = static_cast<WGL_MatrixType *>(ctor_type);
                const unsigned mat_size = WGL_MatrixType::GetColumns(matrix_type);
                OP_ASSERT(mat_size == WGL_MatrixType::GetRows(matrix_type) ||
                          !"Expected square destination matrix type");

                printer->OutString("((");
                arg->VisitExpr(this);
                printer->OutString(")*");
                printer->OutString(MatrixTypeToString(matrix_type));
                printer->OutString(mat_size == 2 ? UNI_L("(1,0, 0,1))") :
                                   mat_size == 3 ? UNI_L("(1,0,0, 0,1,0, 0,0,1))") :
                                                   UNI_L("(1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1))"));
            }
        }
        else /* Not a single-argument scalar. */
        {
            /* We have a vector or matrix constructor with single argument of
               non-scalar type. */

            /* Except for vector constructors applied to a wider vector, HLSL can handle these
               as in GLSL. */
            if (is_vector_ctor)
            {
                WGL_VectorType *vector_type = static_cast<WGL_VectorType *>(ctor_type);
                const char *swizzle = NULL;
                if (argc == 1 && WGL_Type::IsVectorType(arg->GetExprType()))
                {
                    if (type_size < WGL_VectorType::GetMagnitude(static_cast<WGL_VectorType *>(arg->GetExprType())))
                        if (type_size == 2)
                            swizzle = ".xy";
                        else if (type_size == 3)
                            swizzle = ".xyz";

                    if (swizzle)
                    {
                        printer->OutString(VectorTypeToString(vector_type));
                        printer->OutString("((");
                        arg->VisitExpr(this);
                        printer->OutString(")");
                        printer->OutString(swizzle);
                        printer->OutString(")");
                        return e;
                    }
                }
                else if (argc > 1)
                {
                    OP_ASSERT(arg_width > type_size);
                    if (type_size == 2)
                        swizzle = ".xy";
                    else if (type_size == 3)
                        swizzle = ".xyz";

                    if (swizzle)
                    {
                        printer->OutString(VectorTypeToString(vector_type));
                        printer->OutString("(");
                        printer->OutString(VectorMagnitudeToString(WGL_VectorType::GetElementType(vector_type), arg_width));
                        e->args->VisitExprList(this);
                        printer->OutString(swizzle);
                        printer->OutString(")");
                        return e;
                    }
                }
                goto no_rewrite;
            }

            /* As can a matrix constructor with a vector argument. */
            if (argc > 1 || !arg->GetExprType() || arg->GetExprType()->GetType() != WGL_Type::Matrix)
                goto no_rewrite;

            WGL_MatrixType *const dst_type = static_cast<WGL_MatrixType *>(ctor_type);
            WGL_MatrixType *const src_type = static_cast<WGL_MatrixType *>(arg->GetExprType());

            /* We have a matrix constructor with a single argument of matrix
               type. If the new matrix is of size m and the argument is of size
               n and m <= n, we can use the ((float<m>x<m>)(arg)) syntax. The m
               > n case is trickier as GLSL dictates that the extra elements be
               taken from the identity matrix, and is implemented via helper
               functions. */

            const unsigned dst_size = WGL_MatrixType::GetColumns(dst_type);
            const unsigned src_size = WGL_MatrixType::GetColumns(src_type);
            OP_ASSERT(dst_size == WGL_MatrixType::GetRows(dst_type) ||
                      !"Expected square destination matrix type");
            OP_ASSERT(src_size == WGL_MatrixType::GetRows(src_type) ||
                      !"Expected square source matrix type");

            if (dst_size <= src_size)
            {
                printer->OutString("((");
                printer->OutString(MatrixTypeToString(dst_type));
                printer->OutString(")(");
                arg->VisitExpr(this);
                printer->OutString("))");
            }
            else /* dst_size > src_size */
            {
                /* Use helper functions generated via the intrinsics system
                   (see WGL_ProcessVertex::VisitExpr(WGL_CallExpr *)). */

                if (src_size == 2)
                    if (dst_size == 3)
                        printer->OutString(HLSL_INTRINSIC(mat2ToMat3));
                    else /* dst_size == 4 */
                    {
                        OP_ASSERT(dst_size == 4);
                        printer->OutString(HLSL_INTRINSIC(mat2ToMat4));
                    }
                else /* src_size == 3 && dst_size == 4 */
                {
                    OP_ASSERT(src_size == 3 && dst_size == 4);
                    printer->OutString(HLSL_INTRINSIC(mat3ToMat4));
                }

                printer->OutString("(");
                arg->VisitExpr(this);
                printer->OutString(")");
            }
        }

        return e;
    }

    /* Approximations: really do need to look at the argument types. */
    else if (e->fun->GetType() == WGL_Expr::Var)
    {
        WGL_VarExpr *fun = static_cast<WGL_VarExpr *>(e->fun);
        if (IntrinsicHandler handler = intrinsics_map[fun->intrinsic])
        {
            /* Call the rewrite method pointed at by the pointer-to-member
               'handler'. This uses the C++-specific ->* operator to specify
               the object ('this' pointer) to use. */
            return (this->*handler)(fun, e->args, NULL);
        }
    }

no_rewrite:
    return WGL_PrettyPrinter::VisitExpr(e);
}

void
WGL_CgGenerator::InitIntrinsicsMap()
{
    /* Configuring up the table directing the rewriting of GLSL intrinsics.

       There is an opportunity for making this even more table-driven & not
       delegate to per-intrinsic Rewrite* methods, but have it be wholly
       type-driven using table information. Maybe at some later stage,
       pending time and the real need for making it be easy to configure
       up new intrinsic mappings. */
    intrinsics_map[WGL_GLSL::NotAnIntrinsic] = NULL;
    intrinsics_map[WGL_GLSL::Radians] = NULL;
    intrinsics_map[WGL_GLSL::Degrees] = NULL;
    intrinsics_map[WGL_GLSL::Sin] = NULL;
    intrinsics_map[WGL_GLSL::Cos] = NULL;
    intrinsics_map[WGL_GLSL::Tan] = NULL;
    intrinsics_map[WGL_GLSL::Asin] = NULL;
    intrinsics_map[WGL_GLSL::Acos] = NULL;
    intrinsics_map[WGL_GLSL::Atan] = &WGL_CgGenerator::RewriteAtan;
    intrinsics_map[WGL_GLSL::Pow] = NULL;
    intrinsics_map[WGL_GLSL::Exp] = NULL;
    intrinsics_map[WGL_GLSL::Log] = NULL;
    intrinsics_map[WGL_GLSL::Exp2] = NULL;
    intrinsics_map[WGL_GLSL::Log2] = NULL;
    intrinsics_map[WGL_GLSL::Sqrt] = NULL;
    intrinsics_map[WGL_GLSL::Inversesqrt] = &WGL_CgGenerator::RewriteInversesqrt;
    intrinsics_map[WGL_GLSL::Abs] = NULL;
    intrinsics_map[WGL_GLSL::Sign] = NULL;
    intrinsics_map[WGL_GLSL::Floor] = NULL;
    intrinsics_map[WGL_GLSL::Ceil] = NULL;
    intrinsics_map[WGL_GLSL::Fract] = &WGL_CgGenerator::RewriteFract;
    intrinsics_map[WGL_GLSL::Mod] = &WGL_CgGenerator::RewriteMod;
    intrinsics_map[WGL_GLSL::Min] = &WGL_CgGenerator::RewriteMinMax;
    intrinsics_map[WGL_GLSL::Max] = &WGL_CgGenerator::RewriteMinMax;
    intrinsics_map[WGL_GLSL::Clamp] = &WGL_CgGenerator::RewriteClamp;
    intrinsics_map[WGL_GLSL::Mix] = &WGL_CgGenerator::RewriteMix;
    intrinsics_map[WGL_GLSL::Step] = &WGL_CgGenerator::RewriteStep;
    intrinsics_map[WGL_GLSL::Smoothstep] = &WGL_CgGenerator::RewriteStep;
    intrinsics_map[WGL_GLSL::Length] = NULL;
    intrinsics_map[WGL_GLSL::Distance] = NULL;
    intrinsics_map[WGL_GLSL::Dot] = NULL;
    intrinsics_map[WGL_GLSL::Cross] = NULL;
    intrinsics_map[WGL_GLSL::Normalize] = NULL;
    intrinsics_map[WGL_GLSL::Faceforward] = NULL;
    intrinsics_map[WGL_GLSL::Reflect] = NULL;
    intrinsics_map[WGL_GLSL::Refract] = NULL;
    intrinsics_map[WGL_GLSL::MatrixCompMult] = &WGL_CgGenerator::RewriteMatrixCompMult;
    intrinsics_map[WGL_GLSL::LessThan] = &WGL_CgGenerator::RewriteVectorRelational;
    intrinsics_map[WGL_GLSL::LessThanEqual] = &WGL_CgGenerator::RewriteVectorRelational;
    intrinsics_map[WGL_GLSL::GreaterThan] = &WGL_CgGenerator::RewriteVectorRelational;
    intrinsics_map[WGL_GLSL::GreaterThanEqual] = &WGL_CgGenerator::RewriteVectorRelational;
    intrinsics_map[WGL_GLSL::Equal] = &WGL_CgGenerator::RewriteVectorEqual;
    intrinsics_map[WGL_GLSL::NotEqual] = &WGL_CgGenerator::RewriteVectorEqual;
    intrinsics_map[WGL_GLSL::Any] = NULL;
    intrinsics_map[WGL_GLSL::All] = NULL;
    intrinsics_map[WGL_GLSL::Not] = &WGL_CgGenerator::RewriteNot;
    intrinsics_map[WGL_GLSL::Texture2D] = &WGL_CgGenerator::RewriteTextureFun;
    intrinsics_map[WGL_GLSL::Texture2DProj] = &WGL_CgGenerator::RewriteTextureFun;
    intrinsics_map[WGL_GLSL::Texture2DLod] = &WGL_CgGenerator::RewriteTextureFun;
    intrinsics_map[WGL_GLSL::Texture2DProjLod] = &WGL_CgGenerator::RewriteTextureFun;
    intrinsics_map[WGL_GLSL::TextureCube] = &WGL_CgGenerator::RewriteTextureFun;
    intrinsics_map[WGL_GLSL::TextureCubeLod] = &WGL_CgGenerator::RewriteTextureFun;
    /* Extensions */
    intrinsics_map[WGL_GLSL::DFdx] = NULL;
    intrinsics_map[WGL_GLSL::DFdy] = NULL;
    intrinsics_map[WGL_GLSL::Fwidth] = NULL;

    for (unsigned i = 0; i <= LAST_INTRINSIC / (sizeof(unsigned) * 8); i++)
        intrinsics_uses[i] = 0;
}

/* virtual */ WGL_Decl *
WGL_CgGenerator::VisitDecl(WGL_ArrayDecl *d)
{
    if (d->type->type_qualifier &&
        d->type->type_qualifier->storage == WGL_TypeQualifier::Uniform &&
        cg->GetConfiguration()->shader_uniforms_in_cbuffer)
        return NULL;

    return WGL_PrettyPrinter::VisitDecl(d);
}

/* virtual */ WGL_Decl *
WGL_CgGenerator::VisitDecl(WGL_PrecisionDefn *d)
{
    return NULL;
}

/* virtual */ WGL_Decl *
WGL_CgGenerator::VisitDecl(WGL_VarDecl *d)
{
    if (!is_global)
        return WGL_PrettyPrinter::VisitDecl(d);

    if (!d->type)
        return NULL;

    /* Unqualified global variables are externally invisible and r/w in GLSL.
       To get the equivalent in HLSL, we need to add the 'static' qualifier. */

    if (!d->type->type_qualifier)
    {
        printer->OutString("static ");
        return WGL_PrettyPrinter::VisitDecl(d);
    }

    const WGL_TypeQualifier::Storage storage =
        d->type->type_qualifier->storage;

    if (storage != WGL_TypeQualifier::Attribute &&
        storage != WGL_TypeQualifier::Uniform   &&
        storage != WGL_TypeQualifier::Varying)
    {
        if (storage == WGL_TypeQualifier::Const)
            printer->OutString("static ");
        return WGL_PrettyPrinter::VisitDecl(d);
    }
    else if (storage == WGL_TypeQualifier::Uniform)
    {
        if (!cg->GetConfiguration()->shader_uniforms_in_cbuffer)
        {
            d->type->type_qualifier->storage = WGL_TypeQualifier::NoStorage;
            return WGL_PrettyPrinter::VisitDecl(d);
        }
        else if (d->type->GetType() == WGL_Type::Sampler)
        {
            d->type->type_qualifier->storage = WGL_TypeQualifier::NoStorage;
            WGL_Decl *const res = WGL_PrettyPrinter::VisitDecl(d);
            if (cg->GetConfiguration()->for_directx_version > 9)
            {
                printer->OutNewline();
                printer->OutString("SamplerState ");
                printer->OutString(Storage(cg->GetContext(), d->identifier));
                printer->OutString("Sampler;");
            }
            return res;
        }
    }

    return NULL;
}

/* virtual */ WGL_Decl *
WGL_CgGenerator::VisitDecl(WGL_FunctionDefn *d)
{
    is_global = FALSE;
    WGL_Decl *res = WGL_PrettyPrinter::VisitDecl(d);
    is_global = TRUE;
    return res;
}

/* virtual */ WGL_Type *
WGL_CgGenerator::VisitType(WGL_MatrixType *t)
{
    t->Show(this);
    printer->OutStringId(MatrixTypeToString(t));
    return t;
}

/* virtual */ WGL_Type *
WGL_CgGenerator::VisitType(WGL_SamplerType *t)
{
    if (cg->GetConfiguration()->for_directx_version > 9)
    {
        if (t->type == WGL_SamplerType::SamplerCube)
            printer->OutString("TextureCube");
        else
            printer->OutString("Texture2D");
        return t;
    }
    else
        return WGL_PrettyPrinter::VisitType(t);
}

/* virtual */ WGL_Type *
WGL_CgGenerator::VisitType(WGL_VectorType *t)
{
    t->Show(this);
    printer->OutStringId(VectorTypeToString(t));
    return t;
}

WGL_Expr *
WGL_CgGenerator::VisitExpr(WGL_BinaryExpr *e)
{
    switch (e->op)
    {
    case WGL_Expr::OpMul:
        if (e->GetExprType() && e->GetExprType()->GetType() != WGL_Type::Basic &&
            !(e->arg1->GetExprType()->GetType() == WGL_Type::Vector &&
              e->arg2->GetExprType()->GetType() == WGL_Type::Vector))
        {
            /* Rewrite to mul(.., ..). Instead of transposing, swap the
               arguments. (Vector arguments are interpreted as either row or column
               vectors depending on argument position, and (A^T B^T)^T = BA, so
               this works out fine.) */

            PrintBinaryFunctionCall("mul", e->arg2, e->arg1);

            return e;
        }
        break;

    case WGL_Expr::OpXor:
        /* Logical XOR can be implemented by comparing boolean values for
           equality. Add extra parentheses to prevent precedence mishaps. */

        /* TODO: Is casting needed to handle all cases here? */

        printer->OutString("((");
        e->arg1->VisitExpr(this);
        printer->OutString(") != (");
        e->arg2->VisitExpr(this);
        printer->OutString("))");

        return e;
    }

    // No rewrite
    return WGL_PrettyPrinter::VisitExpr(e);
}

WGL_Expr *
WGL_CgGenerator::VisitExpr(WGL_SelectExpr *sel_expr)
{
    WGL_Type *exprType = sel_expr->value->GetExprType();
    if (exprType && exprType->GetType() == WGL_Type::Vector)
    {
        const uni_char *fs = Storage(cg->GetContext(), sel_expr->field);
        uni_char sels[5]; // ARRAY OK 2011-07-01 wonko
        op_memset(sels, 0, 5 * sizeof(uni_char));
        OP_ASSERT(uni_strlen(fs) <= 4);
        for (unsigned i = 0; i < 4 && fs[i] != 0; i++)
        {
            if (fs[i] == 's')
                sels[i] = 'x';
            else if (fs[i] == 't')
                sels[i] = 'y';
            else if (fs[i] == 'p')
                sels[i] = 'z';
            else if (fs[i] == 'q')
                sels[i] = 'w';
            else
                sels[i] = fs[i];
        }
        sel_expr->value->VisitExpr(this);
        printer->OutString(".");
        printer->OutString(sels);
        return sel_expr;
    }
    else
        return WGL_PrettyPrinter::VisitExpr(sel_expr);
}

void
WGL_CgGenerator::PrintBinaryFunctionCall(const char *fn_name, WGL_Expr *arg1, WGL_Expr *arg2)
{
    WGL_Expr::PrecAssoc prev = printer->EnterArgListScope();
    printer->OutString(fn_name);
    printer->OutString("(");
    arg1->VisitExpr(this);
    printer->OutString(", ");
    arg2->VisitExpr(this);
    printer->OutString(")");
    printer->LeaveArgListScope(prev);
}

void
WGL_CgBuilder::EmitTypeDecl(WGL_Printer    *out_sink,
                            WGL_Type       *ty,
                            const uni_char *var_name,
                            SemanticHint   hint,
                            BOOL           is_decl /* = TRUE  */)
{
    if (var_name && !WGL_CgTypes::OutCgKnownVar(this, out_sink, var_name))
    {
        if (ty->GetType() == WGL_Type::Array)
        {
            /* This is a quick and dirty fix for outputting array declarations
               with the correct C-like syntax having "[" after the identifier.
               Both this code and the (pre-existing) code for the non-array
               case below is broken in that it never outputs any qualifiers.
               Fixing that in a good way while also making the "uniform"
               qualifier optional (for declarations within cbuffers) would
               require some potentially time-consuming reorganization. I'll use
               this for now to save time for more critical bugs.

               (We could probably get rid of WGL_CgTypes::OutCgType()
               altogether and move everything into
               WGL_CgGenerator/PrettyPrinter::VisitType().)

               /Ulf */

            WGL_ArrayType *const array_type = static_cast<WGL_ArrayType *>(ty);

            WGL_CgTypes::OutCgType(this, array_type->element_type);
            out_sink->OutString(" ");
            out_sink->OutString(var_name);
            out_sink->OutString("[");
            if (array_type->is_constant_value)
            {
                out_sink->OutInt(array_type->length_value);
            }
            else
            {
                OP_ASSERT(array_type->length);
                /* Extra check, just to be safe. */
                if (array_type->length)
                    array_type->length->VisitExpr(&out_gen);
            }
            out_sink->OutString("]");
        }
        else
        {
            /* Non-array case. */

            WGL_CgTypes::OutCgType(this, ty);
            out_sink->OutString(" ");
            if (var_name)
                out_sink->OutString(var_name);
        }

        BOOL have_semantic = FALSE;
        switch (hint)
        {
        case SemanticBoth:
        case SemanticVertex:
            have_semantic = OutputSemanticForBuiltin(var_name,
                                                     config->vertex_semantic_info,
                                                     config->vertex_semantic_info_elements);
            if (hint == SemanticVertex)
                break;
            /* fall through */
        case SemanticPixel:
            have_semantic = OutputSemanticForBuiltin(var_name,
                                                     config->pixel_semantic_info,
                                                     config->pixel_semantic_info_elements);
            break;
        case SemanticIgnore:
        default:
            break;
        }

        if (!have_semantic && hint != SemanticIgnore)
        {
            int idx = -1;

            switch (ty->GetType())
            {
            case WGL_Type::Basic:
                out_sink->OutString(": PSIZE");
                idx = NextCounter(WGL_HLSLBuiltins::PSIZE);
                break;
            case WGL_Type::Vector:
            case WGL_Type::Name:
                out_sink->OutString(": TEXCOORD");
                idx = NextCounter(WGL_HLSLBuiltins::TEXCOORD);
                break;
            }

            if (idx >= 0)
                out_sink->OutInt(idx);
        }
    }

    if (is_decl)
    {
        out_sink->OutString(";");
        out_sink->OutNewline();
    }
}

BOOL
WGL_CgBuilder::OutputSemanticForBuiltin(const uni_char *var_name,
                                        const WGL_HLSLBuiltins::SemanticInfo *semantic_info,
                                        unsigned size)
{
    for (unsigned i = 0; i < size; ++i)
    {
        if (uni_str_eq(var_name, semantic_info[i].name))
        {
            out_sink->OutString(": ");
            out_sink->OutString(semantic_info[i].semantic_name);
            const int idx = NextCounter(semantic_info[i].semantic);
            if (idx >= 0)
                out_sink->OutInt(idx);

            return TRUE;
        }
    }

    return FALSE;
}

int
WGL_CgBuilder::NextCounter(WGL_HLSLBuiltins::DX9Semantic s)
{
    if (WGL_HLSLBuiltins::IsIndexedSemantic(s))
        return static_cast<int>(semantic_counters[s]++);
    else
        return -1;
}

void
WGL_CgBuilder::ResetCounters()
{
    op_memset(semantic_counters, 0, sizeof(semantic_counters));
}


class WGL_ProcessVertex
    : public WGL_VarVisitor
{
private:
    friend class WGL_CgBuilder;

    WGL_CgBuilder *cg;
public:
    WGL_ProcessVertex(WGL_CgBuilder *c)
        : WGL_VarVisitor(c->context)
        , cg(c)
    {
    }

    virtual WGL_VarName *VisitVar(WGL_VarName *v, VarKind kind);
    virtual WGL_VarName *VisitTypeName(WGL_VarName *v, TypeKind kind);

    virtual WGL_Expr *VisitExpr(WGL_CallExpr *e);

    virtual WGL_Decl *VisitDecl(WGL_PrecisionDefn* d) { return d; }
    virtual WGL_Decl *VisitDecl(WGL_VarDecl *d);
};

/* virtual */ WGL_VarName *
WGL_ProcessVertex::VisitVar(WGL_VarName *var, VarKind kind)
{
    if (kind == VarDecl)
    {
        WGL_Type *ty;
        WGL_ShaderVariable::Kind kind;
        if (WGL_CgTypes::IsGlobalVertexVar(cg, var, ty, kind))
            cg->builtin_variables_vertex.AddVariableL(cg->GetContext(), kind, var, ty);
    }
    return var;
}

/* virtual */ WGL_VarName *
WGL_ProcessVertex::VisitTypeName(WGL_VarName *v, TypeKind kind)
{
    return v;
}

/* Used by both WGL_ProcessVertex and WGL_ProcessFragmentDecls. */
static void
ProcessCallExpr(WGL_CallExpr *e, WGL_CgGenerator *cg_gen)
{
    if (e->fun->GetType() == WGL_Expr::Var)
    {
        WGL_VarExpr *var_expr = static_cast<WGL_VarExpr *>(e->fun);
        if (var_expr->intrinsic > 0)
            cg_gen->UsesIntrinsic(var_expr, e->args);
    }
    /* Also check for the matrix(matrix) type constructor case, as we need to
       use a helper function when creating a larger matrix from a smaller one.

       There might be some nicer way to integrate this into the existing
       framework for intrinsics. */
    else if(e->fun->GetType() == WGL_Expr::TypeConstructor)
    {
        WGL_Type *const ctor_type = static_cast<WGL_TypeConstructorExpr *>(e->fun)->constructor;
        if (!WGL_Type::IsMatrixType(ctor_type))
            return;
        WGL_MatrixType *const dst_type = static_cast<WGL_MatrixType *>(ctor_type);

        /* Does the matrix type constructor have a single argument that is also
           a matrix? */

        if (e->args->list.Cardinal() != 1)
            return;

        WGL_Expr *const arg = e->args->list.First();
        if (!WGL_Type::IsMatrixType(arg->GetExprType()))
            return;
        WGL_MatrixType *const src_type = static_cast<WGL_MatrixType *>(arg->GetExprType());

        /* Is the matrix argument of a smaller size? */

        const unsigned dst_size = WGL_MatrixType::GetColumns(dst_type);
        const unsigned src_size = WGL_MatrixType::GetColumns(src_type);

        OP_ASSERT(dst_size == WGL_MatrixType::GetRows(dst_type) ||
                  !"Expected square destination matrix type");
        OP_ASSERT(src_size == WGL_MatrixType::GetRows(src_type) ||
                  !"Expected square source matrix type");

        if (src_size >= dst_size)
            return;

        /* Register a helper function. */

        WGL_CgGenerator::Intrinsic i;

        if (src_size == 2)
            if (dst_size == 3)
                i = WGL_CgGenerator::Mat2ToMat3;
            else /* dst_size == 4 */
            {
                OP_ASSERT(dst_size == 4);
                i = WGL_CgGenerator::Mat2ToMat4;
            }
        else /* src_size == 3 && dst_size == 4 */
        {
            OP_ASSERT(src_size == 3 && dst_size == 4);
            i = WGL_CgGenerator::Mat3ToMat4;
        }

        cg_gen->RegisterIntrinsicUse(i);
    }
}

/* virtual */ WGL_Expr *
WGL_ProcessVertex::VisitExpr(WGL_CallExpr *e)
{
    ProcessCallExpr(e, cg->GetGenerator());
    return WGL_VarVisitor::VisitExpr(e);
}

/* virtual */ WGL_Decl *
WGL_ProcessVertex::VisitDecl(WGL_VarDecl *d)
{
    if (d->initialiser)
        d->initialiser->VisitExpr(this);
    return d;
}

class WGL_PrefixParamList
    : public ListElement<WGL_PrefixParamList>
{
public:
    WGL_PrefixParamList(const uni_char *b, WGL_ParamList *ls)
        : prefix(b)
        , parameters(ls)
    {
    }

    const uni_char *prefix;
    WGL_ParamList *parameters;
};

class WGL_NamePairItem
    : public ListElement<WGL_NamePairItem>
{
public:
    WGL_NamePairItem(WGL_NamePair *i, unsigned sz)
        : table(i)
        , table_size(sz)
    {
    }

    WGL_NamePair *table;
    unsigned table_size;
};

class WGL_EscapedVars
    : public WGL_VarVisitor
{
public:
    WGL_EscapedVars(WGL_Context *context,
                    WGL_CgBuilder *cg,
                    BOOL for_vertex,
                    const uni_char *at_prefix,
                    const uni_char *uni_prefix,
                    const uni_char *var_prefix,
                    const uni_char *out_prefix)
        : WGL_VarVisitor(context)
        , context(context)
        , cg(cg)
        , for_vertex(for_vertex)
        , attr_prefix(at_prefix)
        , uniform_prefix(uni_prefix)
        , varying_prefix(var_prefix)
        , output_prefix(out_prefix)
    {
    }

    virtual ~WGL_EscapedVars()
    {
        /* Avoid triggering an assert in Head::~Head() - see CORE-43794. */
#ifdef _DEBUG
        names.RemoveAll();
#endif // _DEBUG
    }

    void AddSubstList(WGL_NamePair *ls, unsigned sz)
    {
        WGL_NamePairItem *i = OP_NEWGRO_L(WGL_NamePairItem, (ls,sz), context->Arena());
        i->Into(&names);
    }

    void AddVerbatimList(const uni_char **verbatim, unsigned sz)
    {
        WGL_NamePair *verbatimTable = OP_NEWGROA_L(WGL_NamePair, sz, context->Arena());
        for (unsigned j = 0; j < sz; ++j)
            verbatimTable[j].nameA =  verbatimTable[j].nameB = verbatim[j];
        WGL_NamePairItem *i = OP_NEWGRO_L(WGL_NamePairItem, (verbatimTable,sz), context->Arena());
        i->Into(&names);
    }

    virtual WGL_VarName *VisitVar(WGL_VarName *v, VarKind kind)
    {
        const uni_char *pref;
        if (IsSubstitution(v, pref))
            return WGL_String::Make(context, pref);
        else if (IsPrefixed(v, pref))
        {
            WGL_String *js = WGL_String::Make(context, pref);
            return WGL_String::AppendL(context, js, v);
        }
        return v;
    }

    virtual WGL_VarName *VisitTypeName(WGL_VarName *v, TypeKind kind)
    {
        const uni_char *pref;
        if (IsSubstitution(v, pref))
        {
            WGL_String *js = WGL_String::Make(context, pref);
            context->NewTypeName(js);
            return js;
        }
        else if (IsPrefixed(v, pref))
        {
            WGL_String *js = WGL_String::Make(context, pref);
            WGL_String::AppendL(context, js, v);
            context->NewTypeName(js);
            return js;
        }

        return v;
    }

private:
    List<WGL_NamePairItem> names;
    WGL_Context *context;
    WGL_CgBuilder *cg;
    BOOL for_vertex;

    const uni_char *attr_prefix;
    const uni_char *uniform_prefix;
    const uni_char *varying_prefix;
    const uni_char *output_prefix;

    BOOL IsPrefixed(WGL_VarName *i, const uni_char *&pref)
    {
        WGL_ShaderVariables& builtin_variables =
            for_vertex ? cg->builtin_variables_vertex :
                         cg->builtin_variables_fragment;

        if (attr_prefix && (cg->variables->FindAttribute(i) ||
                            builtin_variables.FindAttribute(i)))
        {
            pref = attr_prefix;
            return TRUE;
        }
        if (uniform_prefix && (cg->variables->FindUniform(i) ||
                               builtin_variables.FindUniform(i)))
        {
            pref = uniform_prefix;
            return TRUE;
        }
        if (varying_prefix && (cg->variables->FindVarying(i) ||
                               (!output_prefix && builtin_variables.FindVarying(i))))
        {
            pref = varying_prefix;
            return TRUE;
        }
        if (output_prefix && builtin_variables.FindVarying(i))
        {
            pref = output_prefix;
            return TRUE;
        }
        return FALSE;
    }

    BOOL IsSubstitution(WGL_VarName *i, const uni_char *&subst)
    {
        for (WGL_NamePairItem *l = names.First(); l; l = l->Suc())
        {
            unsigned ll = l->table_size;
            while (ll > 0)
            {
                if (i->Equals(l->table[ll-1].nameA))
                {
                    subst = l->table[ll-1].nameB;
                    return TRUE;
                }
                ll--;
            }
        }
        return FALSE;
    }
};

class WGL_ProcessFragmentDecls
    : public WGL_VarVisitor
{
public:
    WGL_ProcessFragmentDecls(WGL_CgBuilder *c)
        : WGL_VarVisitor(c->context)
        , cg(c)
    {
    }

    virtual WGL_VarName *VisitVar(WGL_VarName *var, VarKind kind)
    {
        if (kind == VarDecl)
        {
            WGL_Type *ty;
            WGL_ShaderVariable::Kind kind;
            if (WGL_CgTypes::IsGlobalFragmentVar(cg, var, ty, kind) &&
                cg->builtin_variables_fragment.AddVariableL(cg->GetContext(), kind, var, ty))
            {
                WGL_CgTypes::OutCgKnownVar(cg, cg->out_sink, Storage(cg->GetContext(), var));
                cg->out_sink->OutString(";");
                cg->out_sink->OutNewline();
            }
        }
        else if (kind == VarExpr)
        {
            WGL_Type *ty;
            WGL_ShaderVariable::Kind kind;
            if (WGL_CgTypes::IsGlobalFragmentVar(cg, var, ty, kind))
                cg->builtin_variables_fragment.AddVariableL(cg->GetContext(), kind, var, ty);
        }
        return var;
    }

    virtual WGL_Expr *VisitExpr(WGL_CallExpr *e);

private:
    friend class WGL_CgBuilder;

    WGL_CgBuilder *cg;
};

/* virtual */ WGL_Expr *
WGL_ProcessFragmentDecls::VisitExpr(WGL_CallExpr *e)
{
    ProcessCallExpr(e, cg->GetGenerator());
    return WGL_VarVisitor::VisitExpr(e);
}

void
WGL_CgBuilder::GenerateCode(WGL_ShaderVariables *vars, WGL_DeclList *vertex, WGL_DeclList *fragment)
{
    if (config->code_prelude)
    {
        out_sink->OutString(config->code_prelude);
        out_sink->OutNewline();
    }

    variables = vars;

    EmitUniforms(vertex != NULL);

    /* Emit HLSL variable definitions corresponding to GLSL built-in
       variables (e.g. gl_Position) being used in the program.

       Additionally, this stage records a set of helper functions that will
       need to be emitted, used when translating some GLSL intrinsics. */

    if (vertex)
        EmitVertexTypeDecls(vertex);
    if (fragment)
        EmitPixelTypeDecls(fragment, vertex != NULL);

    /* Emit helper functions used when translating some GLSL intrinsics. */

    out_gen.EmitIntrinsics();

    if (vertex)
        ProcessVertexShader(vertex);

    if (fragment)
        ProcessFragmentShader(fragment, vertex != NULL);

    if (config->code_postlude)
    {
        out_sink->OutString(config->code_postlude);
        out_sink->OutNewline();
    }
}

void
WGL_CgBuilder::EmitUniforms(BOOL for_vertex)
{
    if (config->shader_uniforms_in_cbuffer)
    {
        out_sink->OutString("cbuffer " CBUFFER_NAME " : register (b0)");

        /* No packoffsets used, assume 'default' packing. */
        BlockBegin(out_sink);

        if (for_vertex && config->for_directx_version <= 9)
        {
            out_sink->OutString("float2 webgl_op_half_pixel;");
            out_sink->OutNewline();
        }

        for (WGL_ShaderVariable *p = variables->GetFirstUniform(); p; p = p->Suc())
            if (p->type->GetType() != WGL_Type::Sampler)
                EmitTypeDecl(out_sink, p->type, p->name, SemanticIgnore, TRUE);

        for (WGL_ShaderVariable *p = builtin_variables_vertex.GetFirstUniform(); p; p = p->Suc())
            if (p->type->GetType() != WGL_Type::Sampler)
                EmitTypeDecl(out_sink, p->type, p->name, SemanticIgnore, TRUE);

        BlockEnd(out_sink);
        out_sink->OutString(";");
        out_sink->OutNewline();
    }
    else if (for_vertex && config->for_directx_version <= 9)
    {
        out_sink->OutString("float2 webgl_op_half_pixel;");
        out_sink->OutNewline();
    }
}

void
WGL_CgBuilder::EmitVertexTypeDecls(WGL_DeclList *ls)
{
    WGL_ProcessVertex pv(this);
    pv.VisitDeclList(ls);

    EmitVertexShaderHeader(out_sink, TRUE);
    if (config->vertex_shader_attribute_decls)
    {
        out_sink->OutString(config->vertex_shader_attribute_decls);
        out_sink->OutNewline();
    }
    for (WGL_ShaderVariable *p1 = variables->GetFirstAttribute(); p1; p1 = p1->Suc())
        EmitTypeDecl(out_sink, p1->type, p1->name, SemanticVertex, TRUE);
    for (WGL_ShaderVariable *p1 = builtin_variables_vertex.GetFirstAttribute(); p1; p1 = p1->Suc())
        EmitTypeDecl(out_sink, p1->type, p1->name, SemanticVertex, TRUE);

    out_sink->DecreaseIndent();
    out_sink->OutNewline();
    out_sink->OutString("};");
    out_sink->OutNewline();

    EmitVertexShaderStruct(FALSE);
}

void
WGL_CgBuilder::EmitVertexShaderStruct(BOOL for_pixel_shader)
{
    EmitVertexShaderHeader(out_sink, FALSE);
    if (for_pixel_shader)
        for (WGL_ShaderVariable *p1 = builtin_variables_fragment.GetFirstAttribute(); p1; p1 = p1->Suc())
        {
            // gl_FragCoord and gl_PointCoord are always put outside of the input struct
            // as they're a bit special and don't need indata.
            if (!uni_str_eq(p1->name, "gl_FragCoord") && !uni_str_eq(p1->name, "gl_PointCoord"))
                EmitTypeDecl(out_sink, p1->type, p1->name, SemanticPixel, TRUE);
        }

    for (WGL_ShaderVariable *p1 = builtin_variables_vertex.GetFirstVarying(); p1; p1 = p1->Suc())
        EmitTypeDecl(out_sink, p1->type, p1->name, SemanticVertex, TRUE);
    for (WGL_ShaderVariable *p1 = variables->GetFirstVarying(); p1; p1 = p1->Suc())
        EmitTypeDecl(out_sink, p1->type, p1->name, SemanticVertex, TRUE);

    BlockEnd(out_sink);
    out_sink->OutString(";");
    out_sink->OutNewline();
}

static void
CgTransformPosition(WGL_Printer *p, BOOL use_half_pixel)
{
#define OSIN OUT_STRUCT_INSTANCE_NAME
    p->OutNewline();
    if (use_half_pixel)
    {
        p->OutString(OSIN".gl_Position.x = "OSIN".gl_Position.x - webgl_op_half_pixel.x*"OSIN".gl_Position.w;");
        p->OutNewline();
        p->OutString(OSIN".gl_Position.y = -("OSIN".gl_Position.y - webgl_op_half_pixel.y*"OSIN".gl_Position.w);");
        p->OutNewline();
    }
    else
    {
        p->OutString(OSIN".gl_Position.y = -"OSIN".gl_Position.y;");
        p->OutNewline();
    }

    /* OpenGL expects the z value after the z divide (which becomes a
       division by w) to be in the range -1 -> 1, while D3D expects 0 -> 1. We
       map the first range onto the second. */
    p->OutString(OSIN".gl_Position.z = 0.5 * ("OSIN".gl_Position.z + "OSIN".gl_Position.w);");

    p->OutNewline();
#undef OSIN
}

void
WGL_CgBuilder::EmitVertexMain(WGL_FunctionDefn *fun)
{
    out_sink->OutNewline();
    out_sink->OutString(VS_OUT_STRUCT_NAME" ");
    out_sink->OutString(config->vertex_shader_name);
    out_sink->OutString("(");
    if (config->vertex_shader_initial_parameters)
        out_sink->OutString(config->vertex_shader_initial_parameters);
    out_sink->OutString(VS_IN_STRUCT_NAME" "IN_STRUCT_INSTANCE_NAME")");
    BlockBegin(out_sink);

    /* Assign an externally invisible global variable the input struct so that
       non-main() functions can access attributes. */
    out_sink->OutString("::"IN_STRUCT_INSTANCE_NAME" = "IN_STRUCT_INSTANCE_NAME";");
    out_sink->OutNewline();

    out_sink->OutString(VS_OUT_STRUCT_NAME" "OUT_STRUCT_INSTANCE_NAME";");
    out_sink->OutNewline();

    out_gen.SetInGlobalScope(FALSE);

    /* Strip away the extra scope/braces of outermost body */
    if (fun->body->GetType() == WGL_Stmt::Body)
        out_gen.VisitStmtList(static_cast<WGL_BodyStmt *>(fun->body)->body);
    else
        fun->body->VisitStmt(&out_gen);

    CgTransformPosition(out_sink, config->for_directx_version <= 9);
    out_sink->OutNewline();
    out_sink->OutString("return "OUT_STRUCT_INSTANCE_NAME";");
    BlockEnd(out_sink);
    out_sink->OutNewline();

    out_gen.SetInGlobalScope(TRUE);
}

void
WGL_CgBuilder::ProcessVertexShader(WGL_DeclList *ls)
{
    WGL_EscapedVars
        escape_vars(context,
                    this,
                    TRUE,                         /* For vertex?      */
                    IN_STRUCT_INSTANCE_NAME_DOT,  /* Attribute prefix */
                    NULL,                         /* Uniform prefix   */
                    OUT_STRUCT_INSTANCE_NAME_DOT, /* Varying prefix   */
                    NULL                          /* Output prefix    */
                    );

    /* This externally invisible global variable is used to let subroutines
       access attributes. It is assigned in the main function. */

    out_sink->OutString("static "VS_IN_STRUCT_NAME" "IN_STRUCT_INSTANCE_NAME";");
    out_sink->OutNewline();

    for (WGL_Decl *d = ls->list.First(); d; d = d->Suc())
    {
        d = d->VisitDecl(&escape_vars);
        if (WGL_FunctionDefn *fun = WGL_Decl::GetFunction(d, UNI_L("main")))
            EmitVertexMain(fun);
        else if (d->VisitDecl(&out_gen))
            out_sink->OutNewline();
    }
}

void
WGL_CgBuilder::ProcessFragmentShader(WGL_DeclList *ls, BOOL with_vertex_shader)
{
    WGL_EscapedVars
        escape_vars(context,
                    this,
                    FALSE,                       /* For vertex?      */
                    IN_STRUCT_INSTANCE_NAME_DOT, /* Attribute prefix */
                    NULL,                        /* Uniform prefix   */
                    IN_STRUCT_INSTANCE_NAME_DOT, /* Varying prefix   */
                    OUT_STRUCT_INSTANCE_NAME_DOT /* Output prefix    */
                    );

    /* This externally invisible global variable is used to let subroutines
       access attributes. It is assigned in the main function. */

    out_sink->OutString("static "VS_OUT_STRUCT_NAME" "IN_STRUCT_INSTANCE_NAME";");
    out_sink->OutNewline();

    // Some special stuff needs to stay unprefixed since they are passed as parameters outside
    // of the input struct.
    const uni_char *unprefixed[] = { UNI_L("gl_FragCoord"), UNI_L("gl_PointCoord") };
    escape_vars.AddVerbatimList(unprefixed, ARRAY_SIZE(unprefixed));

    for (WGL_Decl *d = ls->list.First(); d; d = d->Suc())
    {
        d = d->VisitDecl(&escape_vars);
        if (WGL_FunctionDefn *fun = WGL_Decl::GetFunction(d, UNI_L("main")))
            EmitPixelMain(fun, with_vertex_shader);
        else
        {
            if (d->VisitDecl(&out_gen))
                out_sink->OutNewline();
        }
    }
}

void
WGL_CgBuilder::EmitPixelTypeDecls(WGL_DeclList *ls, BOOL with_vertex_shader)
{
    WGL_ProcessFragmentDecls pd(this);
    EmitPixelShaderHeader(out_sink);
    pd.VisitDeclList(ls);

    BlockEnd(out_sink);
    out_sink->OutString(";");
    out_sink->OutNewline();

    if (!with_vertex_shader)
    {
        /* Force gl_Position to be included.. */
        WGL_Type *ty = NULL;
        WGL_ShaderVariable::Kind kind;
        WGL_VarName *gl_Pos = WGL_String::Make(GetContext(), UNI_L("gl_Position"));
        if (WGL_CgTypes::IsGlobalVertexVar(this, gl_Pos, ty, kind))
            builtin_variables_vertex.AddVariableL(GetContext(), kind, gl_Pos, ty);

        EmitVertexShaderStruct(TRUE);
    }
}

void
WGL_CgBuilder::EmitPixelMain(WGL_FunctionDefn *fun, BOOL with_vertex_shader)
{
    out_sink->OutString(PS_OUT_STRUCT_NAME" ");
    out_sink->OutString(config->pixel_shader_name);
    out_sink->OutString("(");
    if (config->pixel_shader_initial_parameters)
        out_sink->OutString(config->pixel_shader_initial_parameters);
    out_sink->OutString(VS_OUT_STRUCT_NAME" "IN_STRUCT_INSTANCE_NAME);

    // Check if the pixel shader is using gl_FragCoord or gl_PointCoord. They are always
    // put outside of the input struct.
    WGL_VarName *const gl_FragCoord = WGL_String::Make(context, UNI_L("gl_FragCoord"));
    WGL_VarName *const gl_PointCoord = WGL_String::Make(context, UNI_L("gl_PointCoord"));
    BOOL usesFragCoord = builtin_variables_fragment.FindAttribute(gl_FragCoord) != NULL;
    BOOL usesPointCoord = builtin_variables_fragment.FindAttribute(gl_PointCoord) != NULL;
    if (usesFragCoord)
        out_sink->OutString(", float4 gl_FragCoord : SV_Position");
    if (usesPointCoord)
        out_sink->OutString(", float4 gl_PointCoord : SV_Position");
    out_sink->OutString(")");
    BlockBegin(out_sink);

    /* Assign an externally invisible global variable the input struct so that
       non-main() functions can access varyings. */
    out_sink->OutString("::"IN_STRUCT_INSTANCE_NAME" = "IN_STRUCT_INSTANCE_NAME";");
    out_sink->OutNewline();

    out_sink->OutString(PS_OUT_STRUCT_NAME" "OUT_STRUCT_INSTANCE_NAME";");
    out_sink->OutNewline();

    out_gen.SetInGlobalScope(FALSE);

    /* Strip away the extra scope/braces of outermost body */
    if (fun->body->GetType() == WGL_Stmt::Body)
        out_gen.VisitStmtList(static_cast<WGL_BodyStmt *>(fun->body)->body);
    else
        fun->body->VisitStmt(&out_gen);

    out_sink->OutNewline();
    out_sink->OutString("return "OUT_STRUCT_INSTANCE_NAME";");
    BlockEnd(out_sink);
    out_sink->OutNewline();

    out_gen.SetInGlobalScope(TRUE);
}

#endif // CANVAS3D_SUPPORT
