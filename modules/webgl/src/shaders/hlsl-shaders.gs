// HLSL shader functions for impedance matching between
// the GLSL range of intrinsics and what HLSL doesn't
// provide as equivalents.

// This file is processed to construct an .h file for use
// by the HLSL code generator.

BEGIN ModFVec2
float2 webgl_op_mod_fvec2(float2 v, float y)
{
    return float2(v.x % y, v.y % y);
}
END

BEGIN ModFVec3
float3 webgl_op_mod_fvec3(float3 v, float y)
{
    return float3(v.x % y, v.y % y, v.z % y);
}
END

BEGIN ModFVec4
float4 webgl_op_mod_fvec4(float4 v, float y)
{
    return float4(v.x % y, v.y % y, v.z % y, v.w % y);
}
END

BEGIN MinFVec2
float2 webgl_op_min_fvec2(float2 v, float y)
{
    return float2(v.x < y ? v.x : y, v.y < y ? v.y : y);
}
END

BEGIN MinFVec3
float3 webgl_op_min_fvec3(float3 v, float y)
{
    return float3(v.x < y ? v.x : y, v.y < y ? v.y : y, v.z < y ? v.z : y);
}
END

BEGIN MinFVec4
float4 webgl_op_min_fvec4(float4 v, float y)
{
    return float4(v.x < y ? v.x : y, v.y < y ? v.y : y, v.z < y ? v.z : y, v.w < y ? v.w : y);
}
END

BEGIN MaxFVec2
float2 webgl_op_max_fvec2(float2 v, float y)
{
    return float2(v.x > y ? v.x : y, v.y > y ? v.y : y);
}
END

BEGIN MaxFVec3
float3 webgl_op_max_fvec3(float3 v, float y)
{
    return float3(v.x > y ? v.x : y, v.y > y ? v.y : y, v.z > y ? v.z : y);
}
END

BEGIN MaxFVec4
float4 webgl_op_max_fvec4(float4 v, float y)
{
    return float4(v.x > y ? v.x : y, v.y > y ? v.y : y, v.z > y ? v.z : y, v.w > y ? v.w : y);
}
END

BEGIN ClampFVec2
float2 webgl_op_clamp_fvec2(float2 v, float minVal, float maxVal)
{
    return float2(clamp(v.x, minVal, maxVal), clamp(v.y, minVal, maxVal));
}
END

BEGIN ClampFVec3
float3 webgl_op_clamp_fvec3(float3 v, float minVal, float maxVal)
{
    return float3(clamp(v.x, minVal, maxVal), clamp(v.y, minVal, maxVal), clamp(v.z, minVal, maxVal));
}
END

BEGIN ClampFVec4
float4 webgl_op_clamp_fvec4(float4 v, float minVal, float maxVal)
{
    return float4(clamp(v.x, minVal, maxVal), clamp(v.y, minVal, maxVal), clamp(v.z, minVal, maxVal), clamp(v.w, minVal, maxVal));
}
END

BEGIN LerpFVec2
float2 webgl_op_lerp_fvec2(float2 x, float2 y, float a)
{
    return lerp(x, y, float2(a, a));
}
END

BEGIN LerpFVec3
float3 webgl_op_lerp_fvec3(float3 x, float3 y, float a)
{
    return lerp(x, y, float3(a, a, a));
}
END

BEGIN LerpFVec4
float4 webgl_op_lerp_fvec4(float4 x, float4 y, float a)
{
    return lerp(x, y, float4(a, a, a, a));
}
END

BEGIN StepFVec2
float2 webgl_op_step_fvec2(float edge, float2 x)
{
    return step(float2(edge, edge), x);
}
END

BEGIN StepFVec3
float3 webgl_op_step_fvec3(float edge, float3 x)
{
    return step(float3(edge, edge, edge), x);
}
END

BEGIN StepFVec4
float4 webgl_op_step_fvec4(float edge, float4 x)
{
    return step(float4(edge, edge, edge, edge), x);
}
END

BEGIN SmoothStepFVec2
float2 webgl_op_smoothstep_fvec2(float edge0, float edge1, float2 x)
{
    return smoothstep(float2(edge0, edge0), float2(edge1, edge1), x);
}
END

BEGIN SmoothStepFVec3
float3 webgl_op_smoothstep_fvec3(float edge0, float edge1, float3 x)
{
    return smoothstep(float3(edge0, edge0, edge0), float3(edge1, edge1, edge1), x);
}
END

BEGIN SmoothStepFVec4
float4 webgl_op_smoothstep_fvec4(float edge0, float edge1, float4 x)
{
    return smoothstep(float4(edge0, edge0, edge0, edge0), float4(edge1, edge1, edge1, edge1), x);
}
END

BEGIN MatCompMult2
float2x2 webgl_op_matcompmult_mat2(float2x2 a, float2x2 b)
{
    return float2x2(a[0][0] * b[0][0], a[0][1] * b[0][1], a[1][0] * b[1][0], a[1][1] * b[1][1]);
}
END

BEGIN MatCompMult3
float3x3 webgl_op_matcompmult_mat3(float3x3 a, float3x3 b)
{
    return float3x3(a[0][0] * b[0][0], a[0][1] * b[0][1], a[0][2] * b[0][2],
                    a[1][0] * b[1][0], a[1][1] * b[1][1], a[1][2] * b[1][2],
                    a[2][0] * b[2][0], a[2][1] * b[2][1], a[2][2] * b[2][2]);
}
END

BEGIN MatCompMult4
float4x4 webgl_op_matcompmult_mat4(float4x4 a, float4x4 b)
{
    return float4x4(a[0][0] * b[0][0], a[0][1] * b[0][1], a[0][2] * b[0][2], a[0][3] * b[0][3],
                    a[1][0] * b[1][0], a[1][1] * b[1][1], a[1][2] * b[1][2], a[1][3] * b[1][3],
                    a[2][0] * b[2][0], a[2][1] * b[2][1], a[2][2] * b[2][2], a[2][3] * b[2][3],
                    a[3][0] * b[3][0], a[3][1] * b[3][1], a[3][2] * b[3][2], a[3][3] * b[3][3]);
}
END

BEGIN EqualVec2
bool2 webgl_op_equal_fvec2(float2 a, float2 b)
{
    return bool2(a.x == b.x, a.y == b.y);
}
END

BEGIN EqualVec3
bool3 webgl_op_equal_fvec3(float3 a, float3 b)
{
    return bool3(a.x == b.x, a.y == b.y, a.z == b.z);
}
END

BEGIN EqualVec4
bool4 webgl_op_equal_fvec4(float4 a, float4 b)
{
    return bool4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);
}
END

BEGIN EqualIVec2
bool2 webgl_op_equal_ivec2(int2 a, int2 b)
{
    return bool2(a.x == b.x, a.y == b.y);
}
END

BEGIN EqualIVec3
bool3 webgl_op_equal_ivec3(int3 a, int3 b)
{
    return bool3(a.x == b.x, a.y == b.y, a.z == b.z);
}
END

BEGIN EqualIVec4
bool4 webgl_op_equal_ivec4(int4 a, int4 b)
{
    return bool4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);
}
END

BEGIN EqualBVec2
bool2 webgl_op_equal_bvec2(bool2 a, bool2 b)
{
    return bool2(a.x == b.x, a.y == b.y);
}
END

BEGIN EqualBVec3
bool3 webgl_op_equal_bvec3(bool3 a, bool3 b)
{
    return bool3(a.x == b.x, a.y == b.y, a.z == b.z);
}
END

BEGIN EqualBVec4
bool4 webgl_op_equal_bvec4(bool4 a, bool4 b)
{
    return bool4(a.x == b.x, a.y == b.y, a.z == b.z, a.w == b.w);
}
END

BEGIN Texture2DBias
float4 webgl_op_tex2Dbias(sampler2D sampler, float2 coord, float bias)
{
    return tex2Dbias(sampler, float4(coord.x, coord.y, 0.0, bias));
}
END

BEGIN Texture2DProj3
float webgl_op_tex2Dproj3(sampler2D sampler, float3 coord)
{
    return tex2Dproj(sampler, float4(coord.x, coord.y, 0.0, coord.z));
}
END

BEGIN Texture2DProj3Bias
float webgl_op_tex2Dproj3bias(sampler2D sampler, float3 coord, float bias)
{
    return tex2Dbias(sampler, float4(coord.x / coord.z, coord.y / coord.z, 0.0, bias));
}
END

BEGIN Texture2DProj4Bias
float webgl_op_tex2Dproj4bias(sampler2D sampler, float4 coord, float bias)
{
    return tex2Dbias(sampler, float4(coord.x / coord.w, coord.y / coord.w, 0.0, bias));
}
END

BEGIN Texture2DLod
float webgl_op_tex2Dlod(sampler2D sampler, float2 coord, float lod)
{
    return tex2Dlod(sampler, float4(coord.x, coord.y, 0.0, lod));
}
END

BEGIN Texture2DProj3Lod
float webgl_op_tex2Dproj3lod(sampler2D sampler, float3 coord, float lod)
{
    return tex2Dlod(sampler, float4(coord.x / coord.z, coord.y / coord.z, 0.0, lod));
}
END

BEGIN Texture2DProj4Lod
float webgl_op_tex2Dproj4lod(sampler2D sampler, float4 coord, float lod)
{
    return tex2Dlod(sampler, float4(coord.x / coord.w, coord.y / coord.w, 0.0, lod));
}
END

BEGIN TextureCubeBias
float webgl_op_texCUBEbias(samplerCube sampler, float3 coord, float bias)
{
    return texCUBEbias(sampler, float4(coord.x, coord.y, coord.z, bias));
}
END

BEGIN TextureCubeLod
float webgl_op_texCUBElod(samplerCube sampler, float3 coord, float lod)
{
    return texCUBElod(sampler, float4(coord.x, coord.y, coord.z, bias));
}
END

BEGIN Mat2ToMat3
float3x3 webgl_op_mat2ToMat3(float2x2 s)
{
    return float3x3(s._m00, s._m01, 0,
                    s._m10, s._m11, 0,
                    0     , 0     , 1 );
}
END

BEGIN Mat2ToMat4
float4x4 webgl_op_mat2ToMat4(float2x2 s)
{
    return float4x4(s._m00, s._m01, 0, 0,
                    s._m10, s._m11, 0, 0,
                    0     , 0     , 1, 0,
                    0     , 0     , 0, 1 );
}
END

BEGIN Mat3ToMat4
float4x4 webgl_op_mat3ToMat4(float3x3 s)
{
    return float4x4(s._m00, s._m01, s._m02, 0,
                    s._m10, s._m11, s._m12, 0,
                    s._m20, s._m21, s._m22, 0,
                    0     , 0     , 0     , 1 );
}
END
