/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- representing GL intrinsics functions and
 * state variables.
 *
 */
#ifndef WGL_INTRINSICS_H
#define WGL_INTRINSICS_H
#include "modules/webgl/src/wgl_glsl.h"
#include "modules/webgl/src/wgl_validator.h"

struct WGL_NamePair
{
    const uni_char *nameA;
    const uni_char *nameB;
};

class WGL_GLSLBuiltins
{
public:
    struct FunctionInfo
    {
        const uni_char *name;
        WGL_GLSL::Intrinsic intrinsic;
        unsigned arity;
        const char *return_type;
        const char *arg1_type;
        const char *arg2_type;
        const char *arg3_type;
    };

    enum GLSLConstant
    {
        GL_MaxVertexAttribs,
        GL_MaxVertexUniformVectors,
        GL_MaxVaryingVectors,
        GL_MaxVertexTextureImageUnits,
        GL_MaxCombinedTextureImageUnits,
        GL_MaxTextureImageUnits,
        GL_MaxFragmentUniformVectors,
        GL_MaxDrawBuffers
    };

    struct BasicTypeAndName
    {
        WGL_BasicType::Type type;
        const uni_char *name;
    };

    struct VectorNamePair
    {
        WGL_VectorType::Type type;
        const uni_char *name;
    };

    struct ConstantInfo
    {
        GLSLConstant const_value;
        const uni_char *name;
        WGL_BasicType::Type basic_type;
        WGL_Type::Precision precision;
    };

    struct GLName
    {
        const uni_char *name;
        WGL_GLSL::GLType type;
    };

    /** Flags of extensions known to the validator. */
    enum Extension
    {
        Extension_OES_standard_derivatives = 0x1
    };

    /** An extension provides sets of functions (vertex, fragment) + a CPP define to recognise it. */
    struct ExtensionDefine
    {
        Extension id;
        const uni_char *vertex_name;
        const uni_char *fragment_name;
        const uni_char *cpp_supported_define;
        const uni_char *cpp_enabled_define;
        FunctionInfo *vertex_functions;
        unsigned vertex_functions_count;
        FunctionInfo *fragment_functions;
        unsigned fragment_functions_count;
    };

    static int GetConstantValue(const WGL_Validator::Configuration &config, GLSLConstant c);

    static FunctionInfo shader_intrinsics[];
    /**< Table of GLSL builtins, along with supported argument types. */

    static unsigned shader_intrinsics_elements;
    /**< Number of elements in vertex_shader_intrinsics; */

    static WGL_NamePair fragment_shader_intrinsics[];
    /**< Association list from GL fragment intrinsics to their Cg/HLSL pixel shader counterparts. */

    static unsigned fragment_shader_intrinsics_elements;
    /**< Number of elements in fragment_shader_intrinsics. */

    static GLName vertex_shader_input[];

    static unsigned vertex_shader_input_elements;
    /**< Number of elements in vertex_shader_output. */

    static GLName vertex_shader_output[];

    static unsigned vertex_shader_output_elements;
    /**< Number of elements in vertex_shader_output. */

    static ConstantInfo vertex_shader_constant[];

    static unsigned vertex_shader_constant_elements;
    /**< Number of elements in vertex_shader_uniforms. */

    static GLName fragment_shader_input[];

    static unsigned fragment_shader_input_elements;
    /**< Number of elements in fragment_shader_input. */

    static GLName fragment_shader_output[];

    static unsigned fragment_shader_output_elements;
    /**< Number of elements in fragment_shader_output. */

    static ConstantInfo fragment_shader_constant[];

    static unsigned fragment_shader_constant_elements;
    /**< Number of elements in fragment_shader_constant. */

    static GLName shader_constant[];

    static unsigned shader_constant_elements;
    /**< Number of elements in shader_constant. */

    enum TextureOp
    {
        Texture2D = 0,
        Texture2DBias,
        Texture2DProj3,
        Texture2DProj3Bias,
        Texture2DProj4,
        Texture2DProj4Bias,
        Texture2DLod,
        Texture2DProj3Lod,
        Texture2DProj4Lod,
        TextureCube,
        TextureCubeBias,
        TextureCubeLod
    };
    /**< The texture lookups supported by GLSL; enumerated
         for the benefit of HLSL generation. */

    static ExtensionDefine extension_info[];

    static unsigned extension_info_elements;

};

class WGL_HLSLBuiltins
{
public:
    struct MatrixNamePair
    {
        WGL_MatrixType::Type type;
        const uni_char *name;
    };

    struct SamplerNamePair
    {
        WGL_SamplerType::Type type;
        const uni_char *name;
    };

    enum DX9Semantic
    {
        NO_SEMANTIC = 0,
        BINORMAL,
        BLENDINDICES,
        BLENDWEIGHT,
        COLOR,
        NORMAL,
        POSITION,
        POSITIONT,
        PSIZE,
        TANGENT,
        TEXCOORD,
        FOG,
        TESSFACTOR,
        VFACE,
        VPOS,
        LAST_SEMANTIC
    };

    /** Mapping from a GLSL builtin name to the name
        used in the generated HLSL type declaration. */
    struct WGL_HLSLName
    {
        const uni_char *glsl_name;
        /**< The GLSL builtin name */

        const uni_char *hlsl_name;
        /**< The name to use in HLSL code, when
             declared as a parameter or part of
             a struct. It is the literal string
             to use in type declarations, so
             adding an array size as suffix will
             work out. */

        const uni_char *hlsl_type_name;
        /**< The HLSL type, as a string. */

        const uni_char *hlsl_semantics;
        /**< What DX semantic (as string.) */

        DX9Semantic hlsl_semantic;
        /**< What DX semantic (as enum.) */
    };

    static BOOL IsIndexedSemantic(DX9Semantic s);

    static WGL_HLSLName glsl_hlsl_type_map[];

    static unsigned glsl_hlsl_type_map_elements;
    /**< Number of elements in glsl_hlsl_type_map */

    static SamplerNamePair sampler_type_map[];

    static unsigned sampler_type_map_elements;
    /**< Number of elements in sampler_type_map */

    static MatrixNamePair matrix_type_map[];

    static unsigned matrix_type_map_elements;
    /**< Number of elements in matrix_type_map */

    static WGL_GLSLBuiltins::VectorNamePair vector_type_map[];

    static unsigned vector_type_map_elements;
    /**< Number of elements in vector_type_map */

    static WGL_GLSLBuiltins::BasicTypeAndName base_type_map[];

    static unsigned base_type_map_elements;
    /**< Number of elements in base_type_map */

    struct SemanticInfo
    {
        const uni_char *name;
        const uni_char *semantic_name;
        DX9Semantic semantic;
    };
    /**< Name -> semantic mapping for HLSL variables. */

    static BOOL IsHLSLKeyword(const uni_char *name);
    /**< Returns TRUE if the given name is a reserved HLSL keyword. */

private:
    /** Reserved HLSL keywords; cannot be used in generated code. */
    struct WGL_HLSLKeyword
    {
        const char *name;
        BOOL case_insensitive;
    };

    static WGL_HLSLKeyword hlsl_keywords[];

    static unsigned hlsl_keywords_elements;
};

#endif // WGL_INTRINSICS_H
