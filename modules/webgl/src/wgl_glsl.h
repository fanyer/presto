/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- Cg/HLSL backend
 *
 */
#ifndef WGL_GLSL_H
#define WGL_GLSL_H
class WGL_GLSL
{
public:

    /* Enumerating the possible types used by GLSL intrinsics (Chapter 8.)
       Required when keeping track over what specific overloads are used
       by a shader, and consequently have to be emitted/included during
       code generation. */
    enum IntrinsicTypes
    {
        Float = 0,
        Vec2,
        Vec3,
        Vec4,
        IVec2,
        IVec3,
        IVec4,
        BVec2,
        BVec3,
        BVec4,
        Mat2,
        Mat3,
        Mat4,
        Sampler2D,

        /* type categories; used for intrinsics that are overloaded on the HLSL side. */
        Vec,
        IVec,
        BVec,
        Mat,
        GenType,
        AnyType,
        LAST_INTRINSIC_TYPE = Mat4
    };

    /* The intrinsics defined by the GLSL ES2.0 spec (Chapter 8.) */
    enum Intrinsic
    {
        NotAnIntrinsic = 0,
        Radians,
        Degrees,
        Sin,
        Cos,
        Tan,
        Asin,
        Acos,
        Atan,
        Pow,
        Exp,
        Log,
        Exp2,
        Log2,
        Sqrt,
        Inversesqrt,
        Abs,
        Sign,
        Floor,
        Ceil,
        Fract,
        Mod,
        Min,
        Max,
        Clamp,
        Mix,
        Step,
        Smoothstep,
        Length,
        Distance,
        Dot,
        Cross,
        Normalize,
        Faceforward,
        Reflect,
        Refract,
        MatrixCompMult,
        LessThan,
        LessThanEqual,
        GreaterThan,
        GreaterThanEqual,
        Equal,
        NotEqual,
        Any,
        All,
        Not,
        Texture2D,
        Texture2DProj,
        Texture2DLod,
        Texture2DProjLod,
        TextureCube,
        TextureCubeLod,

        /* Extensions */

        /* OES_standard_derivatives */
        DFdx,
        DFdy,
        Fwidth,

        LAST_INTRINSIC_TAG
    };

    // Type used in decls for various GL global variables:
    enum GLType
    {
	GL_Void = 0,
	GL_Float,
	GL_Int,
	GL_UInt,
	GL_Bool,
	GL_Array_Float,
	GL_Array_Vec4,
	GL_Vec2,
	GL_Vec3,
	GL_Vec4,
	GL_Mat2,
	GL_Mat3,
	GL_Mat4,
	GL_PointParams,
	GL_MatParams,
	GL_LightSourceParams,
	GL_LightModelParams,
	GL_LightModelProducts,
	GL_LightProducts
    };
};

#endif // WGL_GLSL_H
