/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL validator.
 *
 */
#ifndef WGL_VALIDATOR_H
#define WGL_VALIDATOR_H

#include "modules/util/opstring.h"
#include "modules/util/simset.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_printer.h"

/* GLES2.0 shader parameters (see Section A.8 of spec); minimum values. */
#define WGL_GL_MAXVERTEXATTRIBS 8
#define WGL_GL_MAXVERTEXUNIFORMVECTORS 128
#define WGL_GL_MAXVARYINGVECTORS 8
#define WGL_GL_MAXVERTEXTEXTUREIMAGEUNITS 0
#define WGL_GL_MAXCOMBINEDTEXTUREIMAGEUNITS 8
#define WGL_GL_MAXTEXTUREIMAGEUNITS 8
#define WGL_GL_MAXFRAGMENTUNIFORMVECTORS 16
#define WGL_GL_MAXDRAWBUFFERS 1

#ifndef WGL_STANDALONE
# include "modules/libvega/vega3ddevice.h"

# define VEGA3D_CONSTANT(x) VEGA3dShaderProgram::x
# define VEGA3D_CONSTANT_TYPE VEGA3dShaderProgram::ConstantType
# define VEGA3D_SHADER_LANGUAGE_TYPE VEGA3dShaderProgram::ShaderLanguage
# define VEGA3D_SHADER_LANGUAGE(x) VEGA3dShaderProgram::x
# define VEGA3D_SHADER_ATTRIBUTE_DATA VEGA3dDevice::AttributeData

#else // !WGL_STANDALONE

# define VEGA3D_CONSTANT(x) WGL_Standalone_VEGAConstants::x
# define VEGA3D_CONSTANT_TYPE WGL_Standalone_VEGAConstants::ConstantType
# define VEGA3D_SHADER_LANGUAGE_TYPE WGL_Standalone_VEGAConstants::ShaderLanguage
# define VEGA3D_SHADER_LANGUAGE(x) WGL_Standalone_VEGAConstants::x
# define VEGA3D_SHADER_ATTRIBUTE_DATA WGL_Standalone_VEGAConstants::AttributeData

class WGL_Standalone_VEGAConstants
{
public:
    enum ConstantType
    {
        SHDCONST_FLOAT = 0,
        SHDCONST_FLOAT2,
        SHDCONST_FLOAT3,
        SHDCONST_FLOAT4,
        SHDCONST_INT,
        SHDCONST_INT2,
        SHDCONST_INT3,
        SHDCONST_INT4,
        SHDCONST_BOOL,
        SHDCONST_BOOL2,
        SHDCONST_BOOL3,
        SHDCONST_BOOL4,
        SHDCONST_MAT2,
        SHDCONST_MAT3,
        SHDCONST_MAT4,
        SHDCONST_SAMP2D,
        SHDCONST_SAMPCUBE,
        SHDCONST_NONE
    };

    enum ShaderLanguage
    {
        SHADER_GLSL = 1,
        SHADER_GLSL_ES,
        SHADER_HLSL_9,
        SHADER_HLSL_10
    };

    struct AttributeData
    {
        char* name;
        unsigned int size;
    };
};

#endif // !WGL_STANDALONE

/** WGL_ShaderVariable represent an input/output variable declared in
    the shader source. */
class WGL_ShaderVariable
    : public ListElement<WGL_ShaderVariable>
{
public:
    enum Kind
    {
        Attribute = 1,
        Uniform,
        Varying
    };

    WGL_ShaderVariable(Kind k, const uni_char *name, WGL_Type::Precision p, WGL_Type *type, BOOL owned)
        : kind(k)
        , name(name)
        , alias_name(NULL)
        , type(type)
        , precision(p)
        , owned(owned)
    {
    }

    ~WGL_ShaderVariable();

    void SetAliasL(const uni_char *new_alias, BOOL copy = FALSE);
    /**< The variable may have an alias/unique name in the generated source;
         associate it with the source variable here. */

public:
#ifdef WGL_STANDALONE
    void ShowL(WGL_PrettyPrinter *p);
#endif // WGL_STANDALONE

    Kind kind;
    /**< An attribute, uniform or varying kind. */

    const uni_char *name;
    /**< The original source name of the variable. */

    const uni_char *alias_name;
    /**< If not NULL, the unique alias used in generated source. */

    WGL_Type *type;
    /**< The type of the shader variable. Owned by the shader variable. */

    WGL_Type::Precision precision;
    /**< The effective precision for this declaration. */

    BOOL owned;
    /**< The variable owns its name and type values and must delete
         these upon destruction. */
};


/** The result of validation is pass/fail along with a set
    of variables representing the uniforms, varyings and attribute
    declarations found in the shader source. The WGL_ShaderVariables
    collect these together, and is used by the WebGL context
    to later on query for name and type information. */
class WGL_ShaderVariables
{
public:
    WGL_ShaderVariables(BOOL is_owned = TRUE)
        : uniforms_count(UINT_MAX)
        , varyings_count(UINT_MAX)
        , attributes_count(UINT_MAX)
        , owned(is_owned)
    {
    }

    ~WGL_ShaderVariables();

    static WGL_ShaderVariables *MakeL(WGL_ShaderVariables *copy);

    /* The three lists maintained by this variable set: */
    List<WGL_ShaderVariable> uniforms;
    List<WGL_ShaderVariable> varyings;
    List<WGL_ShaderVariable> attributes;

    OP_BOOLEAN LookupAttribute(const char *name, VEGA3D_CONSTANT_TYPE &constant_type, unsigned &length, const uni_char** original_name);
    /**< Does the shader program declare an attribute with the given 'name'?
         @param name of the source attribute. Identifier names (e.g., 'my_variable')
                are recognised, along with array references ('attrib_array[2]') and
                struct field references ('attrib_struct.x'). Both such references
                can be nested, referring to attribute array and struct variables types.
         @param [out]constant_type The VEGA3dShaderProgram::ConstantType of the
                referred to attribute storage.
         @param [out]length For array types, the number of elements in the
                array. Set to 1 for other types.
         @param [out]original_name If supplied, the original source variable
                name of 'name'. The validator may rename variables in its output,
                but with this providing the mapping back to the user-supplied
                variable name.
         @return TRUE if the attribute name was successfully resolved, FALSE if not. */

    OP_BOOLEAN LookupUniform(const char *name, VEGA3D_CONSTANT_TYPE &constant_type, unsigned &length, BOOL &is_array, const uni_char** original_name);
    /**< Does the shader program declare a uniform with the given 'name'?
         @param name of the source attribute. Identifier names (e.g., 'my_variable')
                are recognised, along with array references ('uniform_array[2]') and
                struct field references ('uniform_struct.x'). Both such references
                can be nested, referring to uniform variables with array or
                struct types.
         @param [out]constant_type The VEGA3dShaderProgram::ConstantType of the
                referred-to uniform storage.
         @param [out]length For array types, the number of elements in the array. Set to
                1 for other types.
         @param [out]original_name If supplied, the original source variable
                name of 'name'. The validator may rename variables in its output,
                but with this providing the mapping back to the user-supplied
                variable name.
         @return TRUE if the uniform name was successfully resolved, FALSE if not. */

    OP_BOOLEAN LookupVarying(const char *name, VEGA3D_CONSTANT_TYPE &constant_type, unsigned &length, const uni_char** original_name);
    /**< Does the shader program declare a varying with the given 'name'?
         @param name of the source attribute. Identifier names (e.g., 'my_variable')
                are recognised, along with array references ('varying_array[2]'.)
                Array references cannot be nested, nor are struct references allowed.
         @param [out]constant_type The VEGA3dShaderProgram::ConstantType of the
                referred-to varying storage.
         @param [out]length For array types, the number of elements in the array. Set to
                1 for other types.
         @param [out]original_name If supplied, the original source variable
                name of 'name'. The validator may rename variables in its output,
                with this providing the mapping back to the user-supplied variable name.
         @return TRUE if the varying name was successfully resolved, FALSE if not. */

    /* Indexed-based lookups of uniform/varying/attribute information. The WebGL canvas context
       code in libvega is using by-name, hence currently unused. */

    WGL_ShaderVariable *FindAttribute(WGL_VarName *var);
    /**< Return the attribute variable with the given (generated) name. */

    WGL_ShaderVariable *FindUniform(WGL_VarName *var);
    /**< Return the uniform variable with the given (generated) name. */

    WGL_ShaderVariable *FindVarying(WGL_VarName *var);
    /**< Return the varying variable with the given (generated) name. */

    unsigned GetAttributeCount();
    /**< Compute and return the total number of attributes in this
         variable set. */

    unsigned GetUniformCount();
    /**< Compute and return the total number of uniforms in this
         variable set. Uniforms are counted by expanding any
         array and struct types, and counting their elements
         and fields separately. The struct isn't counted
         on its own, but the array is. */

    unsigned GetVaryingCount();
    /**< Compute and return the total number of varyings in this
         variable set. Varying are counted by expanding any
         array varyings, counting their elements separately. */

    OP_STATUS GetAttributeData(unsigned &attribute_count, VEGA3D_SHADER_ATTRIBUTE_DATA *&attributes);
    /**< Return an VEGA3dDevice::AttributeData array representing the declared attributes. If successful
         the caller is responsible for releasing the array by calling ReleaseAttributeData().

         @param [out]attribute_count number of elements in the attribute array.
         @param [out]attributes the allocated array. The empty array is represented
                as NULL.
         @return OpStatus::OK if attribute data successfully created; OpStatus::ERR_NO_MEMORY
                 on OOM. */

    static void ReleaseAttributeData(unsigned attribute_count, VEGA3D_SHADER_ATTRIBUTE_DATA *attributes);
    /**< Release attribute data returned by GetAttributeData(). Must be called upon release, and
         only once. */

    WGL_ShaderVariable *GetFirstAttribute() { return attributes.First(); }
    WGL_ShaderVariable *GetFirstVarying() { return varyings.First(); }
    WGL_ShaderVariable *GetFirstUniform() { return uniforms.First(); }

    BOOL AddAttribute(WGL_ShaderVariable *var);
    /**< Insert the given attribute variable into the current set.
         @return FALSE if an attribute with the given name already exists,
                 TRUE if successfully added. */

    BOOL AddAttributeL(WGL_Context *context, WGL_VarName *name, WGL_Type *type, WGL_Type::Precision prec = WGL_Type::NoPrecision);
    /**< Add a new attribute variable to the current set. If successful,
         the name and type arguments will be owned by the current set.

         @param context The validator context.
         @param name Name of the attribute variable.
         @param type The attribute type.
         @param prec The effective precision of the attribute.

         @return TRUE if successfully added, FALSE if attribute variable
                 name already exists. */

    BOOL AddAttributeL(WGL_Context *context, const uni_char *name, WGL_Type *type, WGL_Type::Precision prec = WGL_Type::NoPrecision, const uni_char *alias_name = NULL);
    /**< Add a new attribute variable to the current set. If successful,
         the name and type arguments will be owned by the current set.

         @param context The validator context.
         @param name Name of the attribute variable in validated shader.
         @param type The attribute type.
         @param prec The effective precision of the attribute.
         @param alias_name If provided, the original source name of this
                variable.

         @return TRUE if successfully added, FALSE if attribute with
                 the given variable name already exists. */

    BOOL AddUniform(WGL_ShaderVariable *var);
    /**< Insert the given uniform variable into the current set.
         @return FALSE if a uniform with the given name already exists,
                 TRUE if successfully added. */

    BOOL AddUniformL(WGL_Context *context, WGL_VarName *name, WGL_Type *type, WGL_Type::Precision prec = WGL_Type::NoPrecision);
    /**< Add a new uniform variable to the current set. If successful,
         the name and type arguments will be owned by the current set.

         @param context The validator context.
         @param name Name of the uniform variable.
         @param type The uniform's type.
         @param prec The effective precision of the variable.

         @return TRUE if successfully added, FALSE if uniform with
                 the given variable name already exists. */

    BOOL AddUniformL(WGL_Context *context, const uni_char *name, WGL_Type *type, WGL_Type::Precision prec = WGL_Type::NoPrecision, const uni_char *alias_name = NULL);
    /**< Add a new uniform variable to the current set. If successful,
         the name and type arguments will be owned by the current set.

         @param context The validator context.
         @param name Name of the uniform variable in validated shader.
         @param type The uniform type.
         @param prec The effective precision of the variable.
         @param alias_name If provided, the original source name of this
                variable.

         @return TRUE if successfully added, FALSE if uniform with
                 the given variable name already exists. */

    BOOL AddVarying(WGL_ShaderVariable *var);
    /**< Insert the given varying variable into the current set.
         @return FALSE if a uniform with the given name already exists,
                 TRUE if successfully added. */

    BOOL AddVaryingL(WGL_Context *context, WGL_VarName *name, WGL_Type *type, WGL_Type::Precision prec = WGL_Type::NoPrecision);
    /**< Add a new varying variable to the current set. If successful,
         the name and type arguments will be owned by the current set.

         @param context The validator context.
         @param name Name of the varying variable.
         @param type The varying's type.
         @param prec The effective precision of the variable.

         @return TRUE if successfully added, FALSE if a varying with
                 the given variable name already exists. */

    BOOL AddVaryingL(WGL_Context *context, const uni_char *name, WGL_Type *type, WGL_Type::Precision prec = WGL_Type::NoPrecision, const uni_char *alias_name = NULL);
    /**< Add a new varying variable to the current set. If successful,
         the name and type arguments will be owned by the current set.

         @param context The validator context.
         @param name Name of the varying variable in the validated shader.
         @param type The varying's type.
         @param prec The effective precision of the variable.
         @param alias_name If provided, the original source name of this
                variable.

         @return TRUE if successfully added, FALSE if a varying with
                 the given variable name already exists. */

    BOOL AddVariableL(WGL_Context *context, WGL_ShaderVariable::Kind kind, WGL_VarName *name, WGL_Type *type);

    void MergeWithL(WGL_ShaderVariables *vars);
    /**< Merge a shader variable set with another. The merging is
         mere concatenation of the individual variable kinds, copying
         the variables in 'vars'. Will leave on OOM. */

    void Release();
    /**< Empty the variable set, releasing the contained WGL_ShaderVariable values. */

#ifdef WGL_STANDALONE
    OP_STATUS Show(WGL_PrettyPrinter *p);
    static void ShowListL(WGL_PrettyPrinter *p, const char *label, List<WGL_ShaderVariable> &list);
#endif // WGL_STANDALONE

private:
    unsigned uniforms_count;
    /**< The number of uniform variables in this set; UINT_MAX if
         not yet computed. See GetUniformCount(). */

    unsigned varyings_count;
    /**< The number of varying variables in this set; UINT_MAX if
         not yet computed. See GetVaryingCount(). */

    unsigned attributes_count;
    /**< The number of attribute variables in this set; UINT_MAX if
         not yet computed. See GetVaryingCount(). */

    void CopyVariableL(WGL_ShaderVariable *v);

    static VEGA3D_CONSTANT_TYPE GetShaderType(WGL_Type *type);

    static OP_BOOLEAN LookupListByName(List<WGL_ShaderVariable> &list, const char *name, BOOL allow_array, BOOL allow_struct, VEGA3D_CONSTANT_TYPE &constant_type, unsigned &length, BOOL &is_array, const uni_char** original_name);

    static WGL_ShaderVariable *FindInList(WGL_VarName *var, List<WGL_ShaderVariable> &list);

    BOOL owned;
    /**< The variables in the set are assumed owned and has to
         deleted upon destruction. */
};

/** WGL_Validator processes a vertex or fragment shader, validating
    it for safety and well-formedness per spec. The external interface
    to WebGL validation. */
class WGL_Validator
{
public:
    /** Configuration parameters to optionally use when validating. */
    class Configuration
    {
    public:
        /* GLES2.0 shader parameters (see Section A.8 of spec) */
        Configuration()
            : gl_MaxVertexAttribs(WGL_GL_MAXVERTEXATTRIBS)
            , gl_MaxVertexUniformVectors(WGL_GL_MAXVERTEXUNIFORMVECTORS)
            , gl_MaxVaryingVectors(WGL_GL_MAXVARYINGVECTORS)
            , gl_MaxVertexTextureImageUnits(WGL_GL_MAXVERTEXTEXTUREIMAGEUNITS)
            , gl_MaxCombinedTextureImageUnits(WGL_GL_MAXCOMBINEDTEXTUREIMAGEUNITS)
            , gl_MaxTextureImageUnits(WGL_GL_MAXTEXTUREIMAGEUNITS)
            , gl_MaxFragmentUniformVectors(WGL_GL_MAXFRAGMENTUNIFORMVECTORS)
            , gl_MaxDrawBuffers(WGL_GL_MAXDRAWBUFFERS)
            , for_vertex(FALSE)
            , output_log(NULL)
            , console_logging(TRUE)
            , output_source(NULL)
            , output_format(VEGA3D_SHADER_LANGUAGE(SHADER_GLSL))
            , fragment_shader_constant_uniform_array_indexing(TRUE)
            , clamp_out_of_bound_uniform_array_indexing(TRUE)
            , support_oes_derivatives(FALSE)
            , support_higp_fragment(TRUE)
            , output_version(100)
        {
        }

        unsigned gl_MaxVertexAttribs;
        unsigned gl_MaxVertexUniformVectors;
        unsigned gl_MaxVaryingVectors;
        unsigned gl_MaxVertexTextureImageUnits;
        unsigned gl_MaxCombinedTextureImageUnits;
        unsigned gl_MaxTextureImageUnits;
        unsigned gl_MaxFragmentUniformVectors;
        unsigned gl_MaxDrawBuffers;

        BOOL for_vertex;

        OpString *output_log;
        /**< If non-NULL, append validator errors and warnings to this string. */

        BOOL console_logging;
        /**< If TRUE, also log errors/warning to the error console. */

        OpString8 *output_source;
        /**< If non-NULL and program passes validation, emit the resulting
             program into string. */

        VEGA3D_SHADER_LANGUAGE_TYPE output_format;
        /**< The language to generate as output. */

        BOOL fragment_shader_constant_uniform_array_indexing;
        /**< If FALSE, permit fragment shaders to do uniform array indexing using
             non-constant (run-time determined) expressions. Default is TRUE,
             making the validator report an error if encountered. */

        BOOL clamp_out_of_bound_uniform_array_indexing;
        /**< If TRUE, wrap all non-constant uniform array index operations with
             a check wrt upper bound, preventing out-of-bound accesses. Default
             is TRUE. */

        BOOL support_oes_derivatives;
        /**< If TRUE, then support the OES_standard_derivatives extension. */

        BOOL support_higp_fragment;
        /**< If TRUE, then support high precision in fragment shaders. */

        unsigned int output_version;
        /**< Specifies the version number to use in the output (via the #version directive if > 100.) */

        /* TODO: allow extension intrinsic functions and constants to be specified. */
    };

    static OP_BOOLEAN Validate(Configuration &config, const uni_char *url, const uni_char *source, unsigned length, WGL_ShaderVariables **variables);
    /**< Given the source code for a shader, parse and validate it for conformance
         to the GLES 2.0 specification + WebGL constraints.

         The validator's behaviour can be adjusted via the 'config' argument.

         The reported outcome is binary; OpBoolean::IS_FALSE meaning that
         the source did not pass validation, OpBoolean::IS_TRUE that it is
         acceptable (but warnings may have been emitted.)
         Errors/warnings are recorded via the output log supplied as
         'config.output_log'; they may optionally also be logged to the
         error console. */

    static BOOL ValidateSource(const uni_char *source);
    /**< Validate the shader source for lexical correctness as required by the WebGL
         specification. */

    static BOOL ValidateLinkageL(Configuration &config, WGL_ShaderVariables *vars, WGL_ShaderVariables *other_vars = NULL);
    /**< Verify consistency of the input/output variables for a pair of
         shader programs or perform overall link-time checks on one set.
         If 'other_vars' is supplied, then both sets are assumed to be from an
         otherwise valid shader program, and consistency checking means verifying
         that variables with same names have identical types.

         By interleaving consistency validation with merging variable sets,
         N programs can be validated as making up a consistent whole.

         ValidateL()'s configuration type is re-purposed here for convenience,
         but only the info_log and console_logging are looked at and used. */

    static void PrintVersionAndExtensionPrelude(WGL_Printer *printer, unsigned version, WGL_Context *context);
    /**< Print a "#version <version>" directive (if 'version' > 100),
         and print an "#extension <foo> : enable" directive for each
         extension enabled in 'context'.

         (This method is public so that the standalone compiler can access it.
         We could also make it conditionally private, but that's probably not
         worth the obfuscation.) */

    static BOOL IsGL(VEGA3D_SHADER_LANGUAGE_TYPE lang)
    {
        return (lang == VEGA3D_SHADER_LANGUAGE(SHADER_GLSL)) ||
               (lang == VEGA3D_SHADER_LANGUAGE(SHADER_GLSL_ES));
    }

private:
    static BOOL ValidateL(WGL_Context *context, Configuration &config, const uni_char *url, const uni_char *source, unsigned length, WGL_ShaderVariables **variables);
    /**< The 'leaving' internal version of Validate(). See it for documentation. */
};

#endif // WGL_VALIDATOR_H
