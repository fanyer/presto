/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009 - 2010
 *
 * WebGL GLSL compiler -- representing GL intrinsics functions and
 * state variables.
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_context.h"
#include "modules/webgl/src/wgl_intrinsics.h"

/* Mapping between GL types and HLSL ones */

/* static */ WGL_HLSLBuiltins::SamplerNamePair
WGL_HLSLBuiltins::sampler_type_map[]
  = { {WGL_SamplerType::Sampler1D, UNI_L("sampler1D")},
      {WGL_SamplerType::Sampler2D, UNI_L("sampler2D")},
      {WGL_SamplerType::Sampler3D, UNI_L("sampler3D")},
      {WGL_SamplerType::SamplerCube, UNI_L("samplerCUBE")},
      {WGL_SamplerType::Sampler1DShadow, UNI_L("sampler1DShadow")},
      {WGL_SamplerType::Sampler2DShadow, UNI_L("sampler2DShadow")},
      {WGL_SamplerType::SamplerCubeShadow, UNI_L("samplerCubeShadow")},
      {WGL_SamplerType::Sampler1DArray, UNI_L("sampler1DArray")},
      {WGL_SamplerType::Sampler2DArray, UNI_L("sampler2DArray")},
      {WGL_SamplerType::Sampler1DArrayShadow, UNI_L("sampler1DArrayShadow")},
      {WGL_SamplerType::Sampler2DArrayShadow, UNI_L("sampler2DArrayShadow")},
      {WGL_SamplerType::SamplerBuffer, UNI_L("samplerBuffer")},
      {WGL_SamplerType::Sampler2DMS,   UNI_L("sampler2DMS")},
      {WGL_SamplerType::Sampler2DMSArray,  UNI_L("sampler2DMSArray")},
      {WGL_SamplerType::Sampler2DRect,  UNI_L("sampler2DRect")},
      {WGL_SamplerType::Sampler2DRectShadow,  UNI_L("sampler2DRectShadow")},
      {WGL_SamplerType::ISampler1D, UNI_L("iSampler1D")},
      {WGL_SamplerType::ISampler2D, UNI_L("iSampler2D")},
      {WGL_SamplerType::ISampler3D, UNI_L("iSampler3D")},
      {WGL_SamplerType::ISamplerCube, UNI_L("iSamplerCube")},
      {WGL_SamplerType::ISampler1DArray, UNI_L("iSampler1DArray")},
      {WGL_SamplerType::ISampler2DArray, UNI_L("iSampler2DArray")},
      {WGL_SamplerType::ISampler2DRect, UNI_L("iSampler2DRect")},
      {WGL_SamplerType::ISamplerBuffer, UNI_L("iSamplerBuffer")},
      {WGL_SamplerType::ISampler2DMS,   UNI_L("iSampler2DMS")},
      {WGL_SamplerType::ISampler2DMSArray, UNI_L("iSampler2DMSArray")},
      {WGL_SamplerType::USampler1D, UNI_L("uSampler1D")},
      {WGL_SamplerType::USampler2D, UNI_L("uSampler2D")},
      {WGL_SamplerType::USampler3D, UNI_L("uSampler3D")},
      {WGL_SamplerType::USamplerCube, UNI_L("uSamplerCube")},
      {WGL_SamplerType::USampler1DArray, UNI_L("uSampler1DArray")},
      {WGL_SamplerType::USampler2DArray, UNI_L("uSampler2DArray")},
      {WGL_SamplerType::USampler2DRect, UNI_L("uSampler2DRect")},
      {WGL_SamplerType::USamplerBuffer, UNI_L("uSamplerBuffer")},
      {WGL_SamplerType::USampler2DMS,   UNI_L("uSampler2DMS")},
      {WGL_SamplerType::USampler2DMSArray, UNI_L("uSampler2DMSArray")},
  };

/* static */
unsigned WGL_HLSLBuiltins::sampler_type_map_elements = ARRAY_SIZE(WGL_HLSLBuiltins::sampler_type_map);

/* static */ WGL_HLSLBuiltins::MatrixNamePair
WGL_HLSLBuiltins::matrix_type_map[]
  = { {WGL_MatrixType::Mat2, UNI_L("float2x2")},
      {WGL_MatrixType::Mat3, UNI_L("float3x3")},
      {WGL_MatrixType::Mat4, UNI_L("float4x4")},
      {WGL_MatrixType::Mat2x2, UNI_L("float2x2")},
      {WGL_MatrixType::Mat2x3, UNI_L("float2x3")},
      {WGL_MatrixType::Mat2x4, UNI_L("float2x4")},
      {WGL_MatrixType::Mat3x2, UNI_L("float3x2")},
      {WGL_MatrixType::Mat3x3, UNI_L("float3x3")},
      {WGL_MatrixType::Mat3x4, UNI_L("float3x4")},
      {WGL_MatrixType::Mat4x2, UNI_L("float4x2")},
      {WGL_MatrixType::Mat4x3, UNI_L("float4x3")},
      {WGL_MatrixType::Mat4x4, UNI_L("float4x4")}
  };

/* static */
unsigned WGL_HLSLBuiltins::matrix_type_map_elements = ARRAY_SIZE(WGL_HLSLBuiltins::matrix_type_map);

/* static */ WGL_GLSLBuiltins::VectorNamePair
WGL_HLSLBuiltins::vector_type_map[]
  = { {WGL_VectorType::Vec2,  UNI_L("float2")},
      {WGL_VectorType::Vec3,  UNI_L("float3")},
      {WGL_VectorType::Vec4,  UNI_L("float4")},
      {WGL_VectorType::BVec2, UNI_L("bool2")},
      {WGL_VectorType::BVec3, UNI_L("bool3")},
      {WGL_VectorType::BVec4, UNI_L("bool4")},
      {WGL_VectorType::IVec2, UNI_L("int2")},
      {WGL_VectorType::IVec3, UNI_L("int3")},
      {WGL_VectorType::IVec4, UNI_L("int4")},
      {WGL_VectorType::UVec2, UNI_L("uint2")},
      {WGL_VectorType::UVec3, UNI_L("uint3")},
      {WGL_VectorType::UVec4, UNI_L("uint4")}
  };

/* static */ unsigned
WGL_HLSLBuiltins::vector_type_map_elements = ARRAY_SIZE(WGL_HLSLBuiltins::vector_type_map);

/* static */ WGL_GLSLBuiltins::BasicTypeAndName
WGL_HLSLBuiltins::base_type_map[]
  = { {WGL_BasicType::Void,  UNI_L("void")},
      {WGL_BasicType::Int,   UNI_L("int")},
      {WGL_BasicType::UInt,  UNI_L("uint")},
      {WGL_BasicType::Float, UNI_L("float")},
      {WGL_BasicType::Bool,  UNI_L("bool")}
  };

/* static */ unsigned
WGL_HLSLBuiltins::base_type_map_elements = ARRAY_SIZE(WGL_HLSLBuiltins::base_type_map);

#define BUILTIN_FUN(t) WGL_GLSL::t
#define FUN1(a, t, ret, arg1) { UNI_L(a), BUILTIN_FUN(t), 1, ret, arg1, 0, 0 }
#define FUN2(a, t, ret, arg1, arg2) { UNI_L(a), BUILTIN_FUN(t), 2, ret, arg1, arg2, 0 }
#define FUN3(a, t, ret, arg1, arg2, arg3) { UNI_L(a), BUILTIN_FUN(t), 3, ret, arg1, arg2, arg3 }

/** GLSL intrinsics (Section 8), encoding name and types supported.
  *
  *  G  - genType (float, vec2, vec3, vec4)
  *  M  - mat2, mat3, mat4
  *  F  - float
  *  B  - bool
  *  V  - vec2, vec3, vec4
  *  V2 - vec2
  *  V3 - vec3
  *  V3 - vec4
  *  BV - bvec2, bvec3, bvec4
  *  IV - ivec2, ivec3, ivec4
  *  S2D - sampler2D
  *  SC  - samplerCube
  */

/* static */ WGL_GLSLBuiltins::FunctionInfo
WGL_GLSLBuiltins::shader_intrinsics[] =
  {
    // trig: section 8.1
    FUN1("radians", Radians, "G", "G"),
    FUN1("degrees", Degrees, "G", "G"),
    FUN1("sin", Sin, "G", "G"),
    FUN1("cos", Cos, "G", "G"),
    FUN1("tan", Tan, "G", "G"),
    FUN1("asin", Asin, "G", "G"),
    FUN1("acos", Acos, "G", "G"),
    FUN2("atan", Atan, "G", "G", "G"),
    FUN1("atan", Atan, "G", "G"),
    // exponential intrinsics: section 8.2
    FUN2("pow", Pow, "G", "G", "G"),
    FUN1("exp", Exp, "G", "G"),
    FUN1("log", Log, "G", "G"),
    FUN1("exp2", Exp2, "G", "G"),
    FUN1("log2", Log2, "G", "G"),
    FUN1("sqrt", Sqrt, "G", "G"),
    FUN1("inversesqrt", Inversesqrt, "G", "G"),

    // common intrinsics: section 8.3
    // (some of these are overloaded.)
    FUN1("abs", Abs, "G", "G"),
    FUN1("sign", Sign, "G", "G"),
    FUN1("floor", Floor, "G", "G"),
    FUN1("ceil", Ceil, "G", "G"),
    FUN1("fract", Fract, "G", "G"),
    FUN2("mod", Mod, "G", "G", "G"),
    FUN2("mod", Mod, "G", "G", "F"),
    FUN2("min", Min, "G", "G", "G"),
    FUN2("min", Min, "G", "G", "F"),
    FUN2("max", Max, "G", "G", "G"),
    FUN2("max", Max, "G", "G", "F"),
    FUN3("clamp", Clamp, "G", "G", "G", "G"),
    FUN3("clamp", Clamp, "G", "G", "F", "F"),
    FUN3("mix", Mix, "G", "G", "G", "G"),
    FUN3("mix", Mix, "G", "G", "G", "F"),
    FUN2("step", Step, "G", "G", "G"),
    FUN2("step", Step, "G", "F", "G"),
    FUN3("smoothstep", Smoothstep, "G", "G", "G", "G"),
    FUN3("smoothstep", Smoothstep, "G", "F", "F", "G"),

    // geometry intrinsics: section 8.4
    FUN1("length", Length, "F", "G"),
    FUN2("distance", Distance, "F", "G", "G"),
    FUN2("dot", Dot, "F", "G", "G"),
    FUN2("cross", Cross, "V", "V", "V"),
    FUN1("normalize", Normalize,"G", "G"),
    FUN3("faceforward", Faceforward, "G", "G", "G", "G"),
    FUN2("reflect", Reflect, "G", "G", "G"),
    FUN3("refract", Refract, "G", "G", "G", "F"),

    // matrix intrinsics: Section 8.5
    FUN2("matrixCompMult", MatrixCompMult, "M", "M", "M"),

    // vector relational intrinsics: Section 8.6
    FUN2("lessThan", LessThan, "BV", "V", "V"),
    FUN2("lessThan", LessThan, "BV", "IV", "IV"),
    FUN2("lessThanEqual", LessThanEqual, "BV", "V", "V"),
    FUN2("lessThanEqual", LessThanEqual, "BV", "IV", "IV"),
    FUN2("greaterThan", GreaterThan, "BV", "V", "V"),
    FUN2("greaterThan", GreaterThan, "BV", "IV", "IV"),
    FUN2("greaterThanEqual", GreaterThanEqual, "BV", "V", "V"),
    FUN2("greaterThanEqual", GreaterThanEqual, "BV", "IV", "IV"),
    FUN2("equal", Equal, "BV", "V", "V"),
    FUN2("equal", Equal, "BV", "IV", "IV"),
    FUN2("equal", Equal, "BV", "BV", "BV"),
    FUN2("notEqual", NotEqual, "BV", "V", "V"),
    FUN2("notEqual", NotEqual, "BV", "IV", "IV"),
    FUN2("notEqual", NotEqual, "BV", "BV", "BV"),
    FUN1("any", Any, "B", "BV"),
    FUN1("all", All, "B", "BV"),
    FUN1("not", Not, "BV", "BV"),

    // texture lookup intrinsics: Section 8.7
    FUN2("texture2D", Texture2D, "V4", "S2D", "V2"),
    FUN3("texture2D", Texture2D, "V4", "S2D", "V2", "F"),
    FUN2("texture2DProj", Texture2DProj, "V4", "S2D", "V3"),
    FUN3("texture2DProj", Texture2DProj, "V4", "S2D", "V3", "F"),
    FUN2("texture2DProj", Texture2DProj, "V4", "S2D", "V4"),
    FUN3("texture2DProj", Texture2DProj, "V4", "S2D", "V4", "F"),
    FUN3("texture2DLod", Texture2DLod, "V4", "S2D", "V2", "F"),
    FUN3("texture2DProjLod", Texture2DProjLod, "V4", "S2D", "V3", "F"),
    FUN3("texture2DProjLod", Texture2DProjLod, "V4", "S2D", "V4", "F"),
    FUN2("textureCube", TextureCube, "V4", "SC", "V3"),
    FUN3("textureCube", TextureCube, "V4", "SC", "V3", "F"),
    FUN3("textureCubeLod", TextureCubeLod, "V4", "SC", "V3", "F")
  };

/* static */ unsigned
WGL_GLSLBuiltins::shader_intrinsics_elements = ARRAY_SIZE(WGL_GLSLBuiltins::shader_intrinsics);

static WGL_GLSLBuiltins::FunctionInfo oes_standard_derivatives[] =
  {
    FUN1("dFdx", DFdx, "G", "G"),
    FUN1("dFdy", DFdy, "G", "G"),
    FUN1("fwidth", Fwidth, "G", "G")
  };

static unsigned oes_standard_derivatives_elements = ARRAY_SIZE(oes_standard_derivatives);

/* static */ WGL_GLSLBuiltins::ExtensionDefine
WGL_GLSLBuiltins::extension_info[] =
  { { WGL_GLSLBuiltins::Extension_OES_standard_derivatives,
      /*vertex name*/ NULL,
      /*fragment name*/ UNI_L("GL_OES_standard_derivatives"),
      /*supported define*/ UNI_L("GL_OES_standard_derivatives"),
      /*enabled define*/ UNI_L("GL_OES_standard_derivatives"),
      NULL,
      0,
      oes_standard_derivatives,
      oes_standard_derivatives_elements}
  };

/* static */ unsigned
WGL_GLSLBuiltins::extension_info_elements = ARRAY_SIZE(WGL_GLSLBuiltins::extension_info);

/* static */ WGL_GLSLBuiltins::ConstantInfo
WGL_GLSLBuiltins::vertex_shader_constant[] =
{ {GL_MaxVertexAttribs, UNI_L("gl_MaxVertexAttribs"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxVertexUniformVectors, UNI_L("gl_MaxVertexUniformVectors"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxVaryingVectors, UNI_L("gl_MaxVaryingVectors"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxVertexTextureImageUnits, UNI_L("gl_MaxVertexTextureImageUnits"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxCombinedTextureImageUnits, UNI_L("gl_MaxCombinedTextureImageUnits"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxTextureImageUnits, UNI_L("gl_MaxTextureImageUnits"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxFragmentUniformVectors, UNI_L("gl_MaxFragmentUniformVectors"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxDrawBuffers, UNI_L("gl_MaxDrawBuffers"), WGL_BasicType::Int, WGL_Type::Medium}
};

/* static */ unsigned
WGL_GLSLBuiltins::vertex_shader_constant_elements = ARRAY_SIZE(WGL_GLSLBuiltins::vertex_shader_constant);

/* static */ int
WGL_GLSLBuiltins::GetConstantValue(const WGL_Validator::Configuration &config, GLSLConstant c)
{
    /* For now, just follow the GLSL ES2.0 spec(section 7.4) */
    switch (c)
    {
    case GL_MaxVertexAttribs:
        return config.gl_MaxVertexAttribs;
    case GL_MaxVertexUniformVectors:
        return config.gl_MaxVertexUniformVectors;
    case GL_MaxVaryingVectors:
        return config.gl_MaxVaryingVectors;
    case GL_MaxVertexTextureImageUnits:
        return config.gl_MaxVertexTextureImageUnits;
    case GL_MaxCombinedTextureImageUnits:
        return config.gl_MaxCombinedTextureImageUnits;
    case GL_MaxTextureImageUnits:
        return config.gl_MaxTextureImageUnits;
    case GL_MaxFragmentUniformVectors:
        return config.gl_MaxFragmentUniformVectors;
    case GL_MaxDrawBuffers:
        return config.gl_MaxDrawBuffers;
    default:
        OP_ASSERT(!"Unhandled constant value");
        return 0;
    }
}

/* static */ WGL_NamePair
WGL_GLSLBuiltins::fragment_shader_intrinsics[] =
  { {UNI_L("texture2D"), UNI_L("tex2D")},
    {UNI_L("vec4"),      UNI_L("float4")},
    {UNI_L("st"),        UNI_L("xy")}
  };

/* static */ unsigned
WGL_GLSLBuiltins::fragment_shader_intrinsics_elements = ARRAY_SIZE(WGL_GLSLBuiltins::fragment_shader_intrinsics);

/* static */ WGL_GLSLBuiltins::GLName
WGL_GLSLBuiltins::vertex_shader_input[] =
  { { UNI_L("gl_VertexID"),       WGL_GLSL::GL_Int}
  , { UNI_L("gl_InstanceID"),     WGL_GLSL::GL_Int}
  , { UNI_L("gl_Color"),          WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_SecondaryColor"), WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_Normal"),         WGL_GLSL::GL_Vec3}
  , { UNI_L("gl_Vertex"),         WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_MultTexCoord0"),  WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_MultTexCoord1"),  WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_MultTexCoord2"),  WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_MultTexCoord3"),  WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_MultTexCoord4"),  WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_MultTexCoord5"),  WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_MultTexCoord6"),  WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_MultTexCoord7"),  WGL_GLSL::GL_Vec4}
  , { UNI_L("gl_FogCoord"),       WGL_GLSL::GL_Float}
  };

/* static */ unsigned
WGL_GLSLBuiltins::vertex_shader_input_elements = ARRAY_SIZE(WGL_GLSLBuiltins::vertex_shader_input);

/* TODO: go through and weed out the desktop-specific ones; not supported. */

/* static */ WGL_GLSLBuiltins::GLName
WGL_GLSLBuiltins::shader_constant[] =
  { { UNI_L("gl_ModelViewMatrix"),  WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ProjectionMatrix"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ProjectionMatrix"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ModelViewProjectionMatrix"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_TextureMatrix"), WGL_GLSL::GL_Mat4},  // [gl_MaxTextureCoords]
    { UNI_L("gl_NormalMatrix"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ModelViewMatrixInverse"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ProjectionMatrixInverse"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ModelViewProjectionMatrixInverse"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_TextureMatrixInverse"), WGL_GLSL::GL_Mat4}, // [gl_MaxTextureCoords]
    { UNI_L("gl_ModelViewMatrixTranspose"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ProjectionMatrixTranspose"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ModelViewProjectionMatrixTranspose"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_TextureMatrixTranspose"), WGL_GLSL::GL_Mat4}, // [gl_MaxTextureCoords]
    { UNI_L("gl_ModelViewMatrixInverseTranspose"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ProjectionMatrixInverseTranspose"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_ModelViewProjectionMatrixInverseTranspose"), WGL_GLSL::GL_Mat4},
    { UNI_L("gl_TextureMatrixInverseTranspose"), WGL_GLSL::GL_Mat4}, // [gl_MaxTextureCoords]
    { UNI_L("gl_NormalScale"), WGL_GLSL::GL_Float},
    { UNI_L("gl_ClipPlane"), WGL_GLSL::GL_Array_Vec4},
    { UNI_L("gl_Point"), WGL_GLSL::GL_PointParams},
    { UNI_L("gl_FrontMaterial"), WGL_GLSL::GL_MatParams},
    { UNI_L("gl_BackMaterial"), WGL_GLSL::GL_MatParams},
    { UNI_L("gl_LightSourceParameters"), WGL_GLSL::GL_LightSourceParams},
    { UNI_L("gl_LightModelParameters"), WGL_GLSL::GL_LightModelParams},
    { UNI_L("gl_FrontLightModelProduct"), WGL_GLSL::GL_LightModelProducts},
    { UNI_L("gl_FrontLightModelProduct"), WGL_GLSL::GL_LightModelProducts},
    { UNI_L("gl_BackLightModelProduct"), WGL_GLSL::GL_LightModelProducts},
    { UNI_L("gl_FrontLightProduct"), WGL_GLSL::GL_LightProducts},
    { UNI_L("gl_BackLightProduct"), WGL_GLSL::GL_LightProducts},
    { UNI_L("gl_LightSourceProduct"), WGL_GLSL::GL_LightSourceParams},
    { UNI_L("gl_LightSourceProduct"), WGL_GLSL::GL_LightSourceParams}
  };

/* static */ unsigned
WGL_GLSLBuiltins::shader_constant_elements = ARRAY_SIZE(WGL_GLSLBuiltins::shader_constant);

/* static */ WGL_GLSLBuiltins::GLName
WGL_GLSLBuiltins::vertex_shader_output[] =
     // per vertex begin
  { {UNI_L("gl_Position"),            WGL_GLSL::GL_Vec4}
  , {UNI_L("gl_PointSize"),           WGL_GLSL::GL_Int}
  , {UNI_L("gl_ClipDistance"),        WGL_GLSL::GL_Array_Float}
  , {UNI_L("gl_ClipVertex"),          WGL_GLSL::GL_Vec4}
     // per vertex end
  , {UNI_L("gl_FrontColor"),          WGL_GLSL::GL_Vec4}
  , {UNI_L("gl_BackColor"),           WGL_GLSL::GL_Vec4}
  , {UNI_L("gl_FrontSecondaryColor"), WGL_GLSL::GL_Vec4}
  , {UNI_L("gl_BackSecondaryColor"),  WGL_GLSL::GL_Vec4}
  , {UNI_L("gl_TexCoord"),            WGL_GLSL::GL_Array_Float}
  , {UNI_L("gl_FogFragCoord"),        WGL_GLSL::GL_Float}
  };

/* static */
unsigned WGL_GLSLBuiltins::vertex_shader_output_elements = ARRAY_SIZE(WGL_GLSLBuiltins::vertex_shader_output);

/* static */ WGL_GLSLBuiltins::GLName
WGL_GLSLBuiltins::fragment_shader_input[] =
 { {UNI_L("gl_FragCoord"), WGL_GLSL::GL_Vec4}
 , {UNI_L("gl_PointCoord"), WGL_GLSL::GL_Vec2}
 , {UNI_L("gl_FrontFacing"), WGL_GLSL::GL_Bool}
 };

/* static */ unsigned
WGL_GLSLBuiltins::fragment_shader_input_elements = ARRAY_SIZE(WGL_GLSLBuiltins::fragment_shader_input);

/* static */ WGL_GLSLBuiltins::GLName
WGL_GLSLBuiltins::fragment_shader_output[] =
 { {UNI_L("gl_FragColor"), WGL_GLSL::GL_Vec4}
 , {UNI_L("gl_FragData"), WGL_GLSL::GL_Array_Vec4}
 };

/* static */ unsigned
WGL_GLSLBuiltins::fragment_shader_output_elements = ARRAY_SIZE(WGL_GLSLBuiltins::fragment_shader_output);

/* static */ WGL_GLSLBuiltins::ConstantInfo
WGL_GLSLBuiltins::fragment_shader_constant[] =
{ {GL_MaxVertexAttribs, UNI_L("gl_MaxVertexAttribs"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxVertexUniformVectors, UNI_L("gl_MaxVertexUniformVectors"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxVaryingVectors, UNI_L("gl_MaxVaryingVectors"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxVertexTextureImageUnits, UNI_L("gl_MaxVertexTextureImageUnits"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxCombinedTextureImageUnits, UNI_L("gl_MaxCombinedTextureImageUnits"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxTextureImageUnits, UNI_L("gl_MaxTextureImageUnits"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxFragmentUniformVectors, UNI_L("gl_MaxFragmentUniformVectors"), WGL_BasicType::Int, WGL_Type::Medium},
  {GL_MaxDrawBuffers, UNI_L("gl_MaxDrawBuffers"), WGL_BasicType::Int, WGL_Type::Medium}
};

/* static */ unsigned
WGL_GLSLBuiltins::fragment_shader_constant_elements = ARRAY_SIZE(WGL_GLSLBuiltins::fragment_shader_constant);

/* static */ WGL_HLSLBuiltins::WGL_HLSLName
WGL_HLSLBuiltins::glsl_hlsl_type_map[] =
{ {UNI_L("gl_Position"), UNI_L("gl_Position"), UNI_L("float4"), UNI_L("POSITION"), WGL_HLSLBuiltins::POSITION},
  {UNI_L("gl_FragColor"), UNI_L("gl_FragColor"), UNI_L("float4"), UNI_L("COLOR"), WGL_HLSLBuiltins::COLOR},
  {UNI_L("gl_FragData"), UNI_L("gl_FragData[1]"), UNI_L("float4"), UNI_L("COLOR"), WGL_HLSLBuiltins::COLOR},
  {UNI_L("gl_FrontFacing"), UNI_L("gl_FrontFacing"), UNI_L("bool"), UNI_L("SV_IsFrontFace"), WGL_HLSLBuiltins::VFACE},
  {UNI_L("gl_FragCoord"), UNI_L("gl_FragCoord"), UNI_L("float4"), UNI_L("SV_Position"), WGL_HLSLBuiltins::POSITION},
  {UNI_L("gl_PointCoord"), UNI_L("gl_PointCoord"), UNI_L("float2"), UNI_L("SV_Position"), WGL_HLSLBuiltins::POSITION},
};

/* static */ unsigned
WGL_HLSLBuiltins::glsl_hlsl_type_map_elements = ARRAY_SIZE(WGL_HLSLBuiltins::glsl_hlsl_type_map);

/* static */ BOOL
WGL_HLSLBuiltins::IsIndexedSemantic(DX9Semantic s)
{
    return (!(s == NO_SEMANTIC || s == POSITIONT || s == FOG || s == VFACE || s == VPOS));
}

/* MSDN claims that the HLSL keywords are case-insensitively reserved. Based
   on local testing, that doesn't appear to be a true statement, only a few
   are. So, we only case-insensitively reserve those that have to.

   Notice that the vector types (float1x1, uint4x4) do not appear to be
   treated as reserved names, so do not extend the treatment to those. */

/* static */ WGL_HLSLBuiltins::WGL_HLSLKeyword
WGL_HLSLBuiltins::hlsl_keywords[] =
  { {"AppendStructuredBuffer", FALSE},
    {"asm", TRUE},
    {"asm_fragment", TRUE},
    {"BlendState", FALSE},
    {"Buffer", FALSE},
    {"ByteAddressBuffer", FALSE},
    {"cbuffer", FALSE},
    {"centroid", FALSE},
    {"column_major", TRUE},
    {"compile", FALSE},
    {"compile_fragment", FALSE},
    {"CompileShader", FALSE},
    {"ComputeShader", FALSE},
    {"ConsumeStructuredBuffer", FALSE},
    {"DepthStencilState", FALSE},
    {"DepthStencilView", FALSE},
    {"double", FALSE},
    {"DomainShader", FALSE},
    {"dword", FALSE},
    {"extern", FALSE},
    {"fxgroup", FALSE},
    {"GeometryShader", FALSE},
    {"groupshared", FALSE},
    {"half", FALSE},
    {"HullShader", FALSE},
    {"inline", FALSE},
    {"InputPatch", FALSE},
    {"interface", FALSE},
    {"line", FALSE},
    {"lineadj", FALSE},
    {"linear", FALSE},
    {"LineStream", FALSE},
    {"namespace", FALSE},
    {"NULL", FALSE},
    {"nointerpolation", FALSE},
    {"noperspective", FALSE},
    {"OutputPatch", FALSE},
    {"packoffset", FALSE},
    {"pass", TRUE},
    {"pixelfragment", TRUE},
    {"pixelshader", FALSE},
    {"point", FALSE},
    {"RenderTargetView", FALSE},
    {"register", FALSE},
    {"Row_major", TRUE},
    {"RWBuffer", FALSE},
    {"RWByteAddressBuffer", FALSE},
    {"RWStructuredBuffer", FALSE},
    {"RWTexture1D", FALSE},
    {"RWTexture1DArray", FALSE},
    {"RWTexture2D", FALSE},
    {"RWTexture2DArray", FALSE},
    {"samplerCUBE", FALSE},
    {"sampler_state", FALSE},
    {"SamplerComparisonState", FALSE},
    {"SamplerState", FALSE},
    {"sampler", FALSE},
    {"shared", FALSE},
    {"snorm", FALSE},
    {"stateblock", FALSE},
    {"stateblock_state", FALSE},
    {"static", FALSE},
    {"string", FALSE},
    {"StructuredBuffer", FALSE},
    {"tbuffer", FALSE},
    {"technique10", FALSE},
    {"technique", TRUE},
    {"texture", FALSE},
    {"Texture1D", FALSE},
    {"Texture1DArray", FALSE},
    {"Texture2D", FALSE},
    {"Texture2DArray", FALSE},
    {"Texture2DMS", FALSE},
    {"Texture2DMSArray", FALSE},
    {"Texture3D", FALSE},
    {"TextureCube", FALSE},
    {"TextureCubeArray", FALSE},
    {"triangle", FALSE},
    {"triangleadj", FALSE},
    {"TriangleStream", FALSE},
    {"unorm", FALSE},
    {"vector", FALSE},
    {"vertexfragment", FALSE},
    {"volatile", FALSE}};

/* static */ unsigned
WGL_HLSLBuiltins::hlsl_keywords_elements = ARRAY_SIZE(WGL_HLSLBuiltins::hlsl_keywords);

/* static */ BOOL
WGL_HLSLBuiltins::IsHLSLKeyword(const uni_char *name)
{
    int low = 0, high = static_cast<int>(hlsl_keywords_elements) - 1;
    int i;

    while (low <= high)
    {
        i = (low + high) / 2;
        int c = uni_stricmp(name, hlsl_keywords[i].name);
        if (c == 0)
        {
            if (!hlsl_keywords[i].case_insensitive)
                c = uni_strcmp(name, hlsl_keywords[i].name);

            return (c == 0);
        }
        if (c < 0)
            high = i - 1;
        else
            low = i + 1;
    }

    return FALSE;
}

#undef BUILTIN_FUN
#undef FUN1
#undef FUN2
#undef FUN3

#endif // CANVAS3D_SUPPORT
