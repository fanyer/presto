/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2010-2011
 *
 * WebGL HLSL intrinsics (source)
 *
 */

/*** THIS IS A GENERATED HEADER. PLEASE EDIT hlsl-shaders.gs AND RUN 'MAKE' TO REGENERATE IT. ***/

#ifndef WGL_CGBUILDER_SHADERS_H
#define WGL_CGBUILDER_SHADERS_H

#define FUN_ModFVec2 UNI_L("float2 webgl_op_mod_fvec2(float2 v, float y){ return float2(v.x % y, v.y % y);}")
#define FUN_ModFVec3 UNI_L("float3 webgl_op_mod_fvec3(float3 v, float y){ return float3(v.x % y, v.y % y, v.z % y);}")
#define FUN_ModFVec4 UNI_L("float4 webgl_op_mod_fvec4(float4 v, float y){ return float4(v.x % y, v.y % y, v.z % y, v.w % y);}")
#define FUN_MinFVec2 UNI_L("float2 webgl_op_min_fvec2(float2 v, float y){ return float2(v.x < y ? v.x : y, v.y < y ? v.y : y);}")
#define FUN_MinFVec3 UNI_L("float3 webgl_op_min_fvec3(float3 v, float y){ return float3(v.x < y ? v.x : y, v.y < y ? v.y : y, v.z < y ? v.z : y);}")
#define FUN_MinFVec4 UNI_L("float4 webgl_op_min_fvec4(float4 v, float y){ return float4(v.x < y ? v.x : y, v.y < y ? v.y : y, v.z < y ? v.z : y, v.w < y ? v.w : y);}")
#define FUN_MaxFVec2 UNI_L("float2 webgl_op_max_fvec2(float2 v, float y){ return float2(v.x > y ? v.x : y, v.y > y ? v.y : y);}")
#define FUN_MaxFVec3 UNI_L("float3 webgl_op_max_fvec3(float3 v, float y){ return float3(v.x > y ? v.x : y, v.y > y ? v.y : y, v.z > y ? v.z : y);}")
#define FUN_MaxFVec4 UNI_L("float4 webgl_op_max_fvec4(float4 v, float y){ return float4(v.x > y ? v.x : y, v.y > y ? v.y : y, v.z > y ? v.z : y, v.w > y ? v.w : y);}")
#define FUN_ClampFVec2 UNI_L("float2 webgl_op_clamp_fvec2(float2 v, float minVal, float maxVal){ return float2(clamp(v.x, minVal, maxVal), clamp(v.y, minVal, maxVal));}")
#define FUN_ClampFVec3 UNI_L("float3 webgl_op_clamp_fvec3(float3 v, float minVal, float maxVal){ return float3(clamp(v.x, minVal, maxVal), clamp(v.y, minVal, maxVal), clamp(v.z, minVal, maxVal));}")
#define FUN_ClampFVec4 UNI_L("float4 webgl_op_clamp_fvec4(float4 v, float minVal, float maxVal){ return float4(clamp(v.x, minVal, maxVal), clamp(v.y, minVal, maxVal), clamp(v.z, minVal, maxVal), clamp(v.w, minVal, maxVal));}")
#define FUN_LerpFVec2 UNI_L("float2 webgl_op_lerp_fvec2(float2 x, float2 y, float a){ return lerp(x, y, float2(a, a));}")
#define FUN_LerpFVec3 UNI_L("float3 webgl_op_lerp_fvec3(float3 x, float3 y, float a){ return lerp(x, y, float3(a, a, a));}")
#define FUN_LerpFVec4 UNI_L("float4 webgl_op_lerp_fvec4(float4 x, float4 y, float a){ return lerp(x, y, float4(a, a, a, a));}")
#define FUN_StepFVec2 UNI_L("float2 webgl_op_step_fvec2(float edge, float2 x){ return step(float2(edge, edge), x);}")
#define FUN_StepFVec3 UNI_L("float3 webgl_op_step_fvec3(float edge, float3 x){ return step(float3(edge, edge, edge), x);}")
#define FUN_StepFVec4 UNI_L("float4 webgl_op_step_fvec4(float edge, float4 x){ return step(float4(edge, edge, edge, edge), x);}")
#define FUN_SmoothStepFVec2 UNI_L("float2 webgl_op_smoothstep_fvec2(float edge0, float edge1, float2 x){ return smoothstep(float2(edge0, edge0), float2(edge1, edge1), x);}")
#define FUN_SmoothStepFVec3 UNI_L("float3 webgl_op_smoothstep_fvec3(float edge0, float edge1, float3 x){ return smoothstep(float3(edge0, edge0, edge0), float3(edge1, edge1, edge1), x);}")
#define FUN_SmoothStepFVec4 UNI_L("float4 webgl_op_smoothstep_fvec4(float edge0, float edge1, float4 x){ return smoothstep(float4(edge0, edge0, edge0, edge0), float4(edge1, edge1, edge1, edge1), x);}")
#define FUN_MatCompMult2 UNI_L("float2x2 webgl_op_matcompmult_mat2(float2x2 a, float2x2 b){ return float2x2(a[0][0] * b[0][0], a[0][1] * b[0][1], a[1][0] * b[1][0], a[1][1] * b[1][1]);}")
#define FUN_MatCompMult3 UNI_L("float3x3 webgl_op_matcompmult_mat3(float3x3 a, float3x3 b){ return float3x3(a[0][0] * b[0][0], a[0][1] * b[0][1], a[0][2] * b[0][2], a[1][0] * b[1][0], a[1][1] * b[1][1], a[1][2] * b[1][2], a[2][0] * b[2][0], a[2][1] * b[2][1], a[2][2] * b[2][2]);}")
#define FUN_MatCompMult4 UNI_L("float4x4 webgl_op_matcompmult_mat4(float4x4 a, float4x4 b){ return float4x4(a[0][0] * b[0][0], a[0][1] * b[0][1], a[0][2] * b[0][2], a[0][3] * b[0][3], a[1][0] * b[1][0], a[1][1] * b[1][1], a[1][2] * b[1][2], a[1][3] * b[1][3], a[2][0] * b[2][0], a[2][1] * b[2][1], a[2][2] * b[2][2], a[2][3] * b[2][3], a[3][0] * b[3][0], a[3][1] * b[3][1], a[3][2] * b[3][2], a[3][3] * b[3][3]);}")
#define FUN_EqualVec2 UNI_L("bool2 webgl_op_equal_fvec2(float2 a, float2 b){ return bool2(a.x == b.x, a.y == b.y);}")
#define FUN_EqualVec3 UNI_L("bool3 webgl_op_equal_fvec3(float3 a, float3 b){ return bool3(a.x == b.x, a.y == b.y, a.z == b.z);}")
#define FUN_EqualVec4 UNI_L("bool4 webgl_op_equal_fvec4(float4 a, float4 b){ return bool4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);}")
#define FUN_EqualIVec2 UNI_L("bool2 webgl_op_equal_ivec2(int2 a, int2 b){ return bool2(a.x == b.x, a.y == b.y);}")
#define FUN_EqualIVec3 UNI_L("bool3 webgl_op_equal_ivec3(int3 a, int3 b){ return bool3(a.x == b.x, a.y == b.y, a.z == b.z);}")
#define FUN_EqualIVec4 UNI_L("bool4 webgl_op_equal_ivec4(int4 a, int4 b){ return bool4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);}")
#define FUN_EqualBVec2 UNI_L("bool2 webgl_op_equal_bvec2(bool2 a, bool2 b){ return bool2(a.x == b.x, a.y == b.y);}")
#define FUN_EqualBVec3 UNI_L("bool3 webgl_op_equal_bvec3(bool3 a, bool3 b){ return bool3(a.x == b.x, a.y == b.y, a.z == b.z);}")
#define FUN_EqualBVec4 UNI_L("bool4 webgl_op_equal_bvec4(bool4 a, bool4 b){ return bool4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);}")
#define FUN_Texture2DBias UNI_L("float4 webgl_op_tex2Dbias(sampler2D sampler, float2 coord, float bias){ return tex2Dbias(sampler, float4(coord.x, coord.y, 0.0, bias));}")
#define FUN_Texture2DProj3 UNI_L("float webgl_op_tex2Dproj3(sampler2D sampler, float3 coord){ return tex2Dproj(sampler, float4(coord.x, coord.y, 0.0, coord.z));}")
#define FUN_Texture2DProj3Bias UNI_L("float webgl_op_tex2Dproj3bias(sampler2D sampler, float3 coord, float bias){ return tex2Dbias(sampler, float4(coord.x / coord.z, coord.y / coord.z, 0.0, bias));}")
#define FUN_Texture2DProj4Bias UNI_L("float webgl_op_tex2Dproj4bias(sampler2D sampler, float4 coord, float bias){ return tex2Dbias(sampler, float4(coord.x / coord.w, coord.y / coord.w, 0.0, bias));}")
#define FUN_Texture2DLod UNI_L("float webgl_op_tex2Dlod(sampler2D sampler, float2 coord, float lod){ return tex2Dlod(sampler, float4(coord.x, coord.y, 0.0, lod));}")
#define FUN_Texture2DProj3Lod UNI_L("float webgl_op_tex2Dproj3lod(sampler2D sampler, float3 coord, float lod){ return tex2Dlod(sampler, float4(coord.x / coord.z, coord.y / coord.z, 0.0, lod));}")
#define FUN_Texture2DProj4Lod UNI_L("float webgl_op_tex2Dproj4lod(sampler2D sampler, float4 coord, float lod){ return tex2Dlod(sampler, float4(coord.x / coord.w, coord.y / coord.w, 0.0, lod));}")
#define FUN_TextureCubeBias UNI_L("float webgl_op_texCUBEbias(samplerCube sampler, float3 coord, float bias){ return texCUBEbias(sampler, float4(coord.x, coord.y, coord.z, bias));}")
#define FUN_TextureCubeLod UNI_L("float webgl_op_texCUBElod(samplerCube sampler, float3 coord, float lod){ return texCUBElod(sampler, float4(coord.x, coord.y, coord.z, bias));}")
#define FUN_Mat2ToMat3 UNI_L("float3x3 webgl_op_mat2ToMat3(float2x2 s){ return float3x3(s._m00, s._m01, 0, s._m10, s._m11, 0, 0 , 0 , 1 );}")
#define FUN_Mat2ToMat4 UNI_L("float4x4 webgl_op_mat2ToMat4(float2x2 s){ return float4x4(s._m00, s._m01, 0, 0, s._m10, s._m11, 0, 0, 0 , 0 , 1, 0, 0 , 0 , 0, 1 );}")
#define FUN_Mat3ToMat4 UNI_L("float4x4 webgl_op_mat3ToMat4(float3x3 s){ return float4x4(s._m00, s._m01, s._m02, 0, s._m10, s._m11, s._m12, 0, s._m20, s._m21, s._m22, 0, 0 , 0 , 0 , 1 );}")
static const uni_char *glsl_intrinsic_functions[] = {
      FUN_ModFVec2,
      FUN_ModFVec3,
      FUN_ModFVec4,
      FUN_MinFVec2,
      FUN_MinFVec3,
      FUN_MinFVec4,
      FUN_MaxFVec2,
      FUN_MaxFVec3,
      FUN_MaxFVec4,
      FUN_ClampFVec2,
      FUN_ClampFVec3,
      FUN_ClampFVec4,
      FUN_LerpFVec2,
      FUN_LerpFVec3,
      FUN_LerpFVec4,
      FUN_StepFVec2,
      FUN_StepFVec3,
      FUN_StepFVec4,
      FUN_SmoothStepFVec2,
      FUN_SmoothStepFVec3,
      FUN_SmoothStepFVec4,
      FUN_MatCompMult2,
      FUN_MatCompMult3,
      FUN_MatCompMult4,
      FUN_EqualVec2,
      FUN_EqualVec3,
      FUN_EqualVec4,
      FUN_EqualIVec2,
      FUN_EqualIVec3,
      FUN_EqualIVec4,
      FUN_EqualBVec2,
      FUN_EqualBVec3,
      FUN_EqualBVec4,
      FUN_Texture2DBias,
      FUN_Texture2DProj3,
      FUN_Texture2DProj3Bias,
      FUN_Texture2DProj4Bias,
      FUN_Texture2DLod,
      FUN_Texture2DProj3Lod,
      FUN_Texture2DProj4Lod,
      FUN_TextureCubeBias,
      FUN_TextureCubeLod,
      FUN_Mat2ToMat3,
      FUN_Mat2ToMat4,
      FUN_Mat3ToMat4,
    NULL};
#endif // WGL_CGBUILDER_SHADERS_H
