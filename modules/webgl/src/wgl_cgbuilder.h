/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- Cg/HLSL backend
 *
 */
#ifndef WGL_CGBUILDER_H
#define WGL_CGBUILDER_H

#include "modules/webgl/src/wgl_glsl.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_builder.h"
#include "modules/webgl/src/wgl_validator.h"
#include "modules/webgl/src/wgl_intrinsics.h"
#include "modules/webgl/src/wgl_pretty_printer.h"

/* Names used in HLSL shaders generated from WebGL GLSL shaders. These should
   all have a "webgl_" prefix to avoid collisions with user variables. */

/* Names of shader entry points. */
#define VS_MAIN_FN_NAME "webgl_vs"
#define PS_MAIN_FN_NAME "webgl_fs"

/* Names of struct types used for vertex/pixel shader input and output. */
#define VS_IN_STRUCT_NAME "webgl_vs_in_struct"
#define VS_OUT_STRUCT_NAME "webgl_vs_out_struct"
#define PS_OUT_STRUCT_NAME "webgl_ps_out_struct"

/* Names used for instances of the above structs.

   The variants with a "_DOT" suffix are needed as there's no reliable way to
   concatenate uni_char strings at compile time. They should always match the
   nonsuffixed variants, only ending with a period. */
#define IN_STRUCT_INSTANCE_NAME "webgl_in"
#define OUT_STRUCT_INSTANCE_NAME "webgl_out"
#define IN_STRUCT_INSTANCE_NAME_DOT UNI_L("webgl_in.")
#define OUT_STRUCT_INSTANCE_NAME_DOT UNI_L("webgl_out.")

/* The name of the cbuffer used for passing uniform data. */
#define CBUFFER_NAME "webgl_cbuffer"

class WGL_CgBuilder;

class WGL_CgTypes
{
public:
    static void OutCgType(WGL_CgBuilder *cg, WGL_Type *t);
    static void OutCgType(WGL_CgBuilder *cg, WGL_BasicType *t);
    static void OutCgType(WGL_CgBuilder *cg, WGL_VectorType *t);
    static void OutCgType(WGL_CgBuilder *cg, WGL_MatrixType *t);
    static void OutCgType(WGL_CgBuilder *cg, WGL_SamplerType *t);

    static void OutCgSemantic(WGL_CgBuilder *cg, WGL_Printer *out_sink, const uni_char *sem_name, WGL_HLSLBuiltins::DX9Semantic s);

    static WGL_Type *BuildTypeFromGL(WGL_CgBuilder *cg, WGL_GLSL::GLType t);
    /**< Constructs the Cg type from the given (enumerated) GL type. */

    static BOOL IsGlobalVertexVar(WGL_CgBuilder *cg, WGL_VarName *i, WGL_Type *&ty, WGL_ShaderVariable::Kind &kind);
    static BOOL IsGlobalFragmentVar(WGL_CgBuilder *cg, WGL_VarName *i, WGL_Type *&ty, WGL_ShaderVariable::Kind &kind);
    static BOOL OutCgKnownVar(WGL_CgBuilder *cg, WGL_Printer *outSink, const uni_char *i);
    static void SubstVars(WGL_Context *context, WGL_Expr *e, WGL_NamePair *ns, unsigned len);
};

class WGL_CgGenerator
    : public WGL_PrettyPrinter
{
public:
    WGL_CgGenerator(WGL_CgBuilder *cg, WGL_Printer *p)
        : WGL_PrettyPrinter(p)
        , cg(cg)
        , is_global(TRUE)
    {
        InitIntrinsicsMap();
    }

    virtual WGL_Expr *VisitExpr(WGL_BinaryExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_SelectExpr *e);
    virtual WGL_Expr *VisitExpr(WGL_CallExpr *e);

    enum Intrinsic
    {
        None = 0,
        ModFVec2,
        ModFVec3,
        ModFVec4,
        MinFVec2,
        MinFVec3,
        MinFVec4,
        MaxFVec2,
        MaxFVec3,
        MaxFVec4,
        ClampFVec2,
        ClampFVec3,
        ClampFVec4,
        LerpFVec2,
        LerpFVec3,
        LerpFVec4,
        StepFVec2,
        StepFVec3,
        StepFVec4,
        SmoothStepFVec2,
        SmoothStepFVec3,
        SmoothStepFVec4,
        MatCompMult2,
        MatCompMult3,
        MatCompMult4,
        EqualVec2,
        EqualVec3,
        EqualVec4,
        EqualIVec2,
        EqualIVec3,
        EqualIVec4,
        EqualBVec2,
        EqualBVec3,
        EqualBVec4,

        Texture2DBias,
        Texture2DProj3,
        Texture2DProj3Bias,
        Texture2DProj4Bias,
        Texture2DLod,
        Texture2DProj3Lod,
        Texture2DProj4Lod,
        TextureCubeBias,
        TextureCubeLod,

        Mat2ToMat3,
        Mat2ToMat4,
        Mat3ToMat4,

        LAST_INTRINSIC
    };
    /**< Complete list of custom shader functions for which the code generator
         may end up generating calls to (in order to implement GLSL's set of
         intrinsics.) */

    static const uni_char *LookupIntrinsicName(Intrinsic i);
    /**< Return the name of the HLSL stub function implementing the intrinsic for us. */

    void UsesIntrinsic(WGL_VarExpr *fun, WGL_ExprList *args);
    /**< Record the GLSL intrinsic as being used in the input shader and register any
         helper functions we will need to implement it. */

    void RegisterIntrinsicUse(Intrinsic i);
    /**< Register a helper function used in implementing the translation of a GLSL
         intrinsic or other tricky-to-translate construct (such as some type
         constructors). These will be output at the beginning of the translated
         shader. */

    virtual WGL_Decl *VisitDecl(WGL_ArrayDecl *d);
    virtual WGL_Decl *VisitDecl(WGL_PrecisionDefn *d);
    virtual WGL_Decl *VisitDecl(WGL_VarDecl *d);
    virtual WGL_Decl *VisitDecl(WGL_FunctionDefn *d);

    virtual WGL_Type *VisitType(WGL_MatrixType *t);
    virtual WGL_Type *VisitType(WGL_SamplerType *t);
    virtual WGL_Type *VisitType(WGL_VectorType *t);

    void EmitIntrinsics();

    void SetInGlobalScope(BOOL f) { is_global = f; }

private:
    WGL_CgBuilder *cg;
    BOOL is_global;

    void PrintBinaryFunctionCall(const char *fn_name, WGL_Expr *arg1, WGL_Expr *arg2);
    /**< Helper function for outputting a call to a binary function called
         'fn_name', taking the arguments 'arg1' and 'arg2'. Will only output as
         many parentheses as needed. */

    unsigned intrinsics_uses[1 + LAST_INTRINSIC / (sizeof(unsigned) * 8)];
    /**< Bitmap recording uses of our own intrinsics; marked
         as such when compiling & code included based on use. */

    typedef WGL_Expr *(WGL_CgGenerator::*IntrinsicHandler)(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    /**< This defines the type IntrinsicHandler as a pointer to
         any method in the WGL_CgGenerator class with the specified signature
         (called a "pointer-to-member"). */

    IntrinsicHandler intrinsics_map[WGL_GLSL::LAST_INTRINSIC_TAG];

    void InitIntrinsicsMap();

    /* The HLSL rewrite methods. If the 'uses_intrinsic' argument is non-NULL,
       no rewriting will be done, but rather the helper function (if any) used
       by the rewritten code will be returned.

       To add additional rewrite methods, you need to

        - declare it here (!)
        - update InitIntrinsicsMap() to activate it.
        - for any stub shader function that needs to be emitted with shaders
          that will call it, extend the WGL_CgGenerator::Intrinsic enumeration.
        - update LookupIntrinsicName() to provide a mapping to the name of this
          custom shader function.
        - extend shaders/hlsl-shaders.gs with its implementation (matching up
          enumeration tags and name, of course.)
        - provide a definition of this new Rewrite*() method.

      i.e., multiple, manual steps.
     */

    WGL_Expr *RewriteAtan(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteInversesqrt(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteClamp(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteFract(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteMatrixCompMult(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteMinMax(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteMix(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteMod(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteNot(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteVectorRelational(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteVectorEqual(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteStep(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteSmoothStep(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);
    WGL_Expr *RewriteTextureFun(WGL_VarExpr *fun, WGL_ExprList *args, Intrinsic *uses_intrinsic);

    void EmitIntrinsicFunction(Intrinsic i);
};

class WGL_ProcessVertex;

class WGL_CgBuilder
{
public:
    /** Configuration parameters for the HLSL code generator. */
    class Configuration
    {
    public:
        Configuration(unsigned dx,
                      const char *vertex = VS_MAIN_FN_NAME,
                      const char *pixel = PS_MAIN_FN_NAME)
            : vertex_shader_name(vertex)
            , pixel_shader_name(pixel)
            , code_prelude(NULL)
            , code_postlude(NULL)
            , vertex_shader_attribute_decls(NULL)
            , vertex_shader_initial_parameters(NULL)
            , vertex_semantic_info(NULL)
            , vertex_semantic_info_elements(0)
            , pixel_shader_initial_parameters(NULL)
            , pixel_semantic_info(NULL)
            , pixel_semantic_info_elements(0)
            , shader_uniforms_in_cbuffer(dx >= 10)
            , for_directx_version(dx) {}

        const char *vertex_shader_name;
        /**< Name of vertex shader main function. */

        const char *pixel_shader_name;
        /**< Name of pixel shader main function. */

        const uni_char *code_prelude;
        /**< String to put at the beginning of the generated shader. */

        const uni_char *code_postlude;
        /**< String to put at the end of the generated shader. */

        const uni_char *vertex_shader_attribute_decls;
        /**< String holding HLSL definitions to include in the generation of the attribute
             input structure. That is, a verbatim string of valid HLSL declarations to include. */

        const uni_char *vertex_shader_initial_parameters;
        /**< String to prefix the parameter list of the vertex shader function with. */

        const WGL_HLSLBuiltins::SemanticInfo *vertex_semantic_info;
        /**< A variable to semantic mapping; optionally used to annotate variables with.
             That is, if there a mapping is found for a variable, it is used. Otherwise,
             the defaulting semantic rules apply. */

        unsigned vertex_semantic_info_elements;
        /**< Number of elements in the vertex shader semantic map array. */

        const uni_char *pixel_shader_initial_parameters;
        /**< String to prefix the parameter list of the pixel shader function with. */

        const WGL_HLSLBuiltins::SemanticInfo *pixel_semantic_info;
        /**< A variable to semantic mapping; optionally used to annotate fragment shader variables
             with. That is, if there a mapping is found for a variable, it is used. Otherwise,
             the defaulting semantic rules apply. */

        unsigned pixel_semantic_info_elements;
        /**< Number of elements in the pixel shader semantic map array. */

        BOOL shader_uniforms_in_cbuffer;
        /**< If TRUE, store constant data (uniforms) in a cbuffer. */

        unsigned for_directx_version;
        /**< DirectX version to target. */
    };

    WGL_CgBuilder(Configuration *config, WGL_Context *context, WGL_Printer *p, WGL_ASTBuilder *a)
        : context(context)
        , config(config)
        , out_sink(p)
        , out_gen(WGL_CgGenerator(this, p))
        , vs_args(NULL)
        , ps_args(NULL)
        , variables(NULL)
        , builtin_variables_vertex(FALSE)
        , builtin_variables_fragment(FALSE)
        , builder(a)
    {
        ResetCounters();
    }

    void ResetCounters();

    void GenerateCode(WGL_ShaderVariables *vars, WGL_DeclList *vertex_decls, WGL_DeclList *fragment_decls);
    /**< Process and emit code for a pair of vertex and fragment shaders, code emitted
         through the associated printer. Multiple invocations of this method is possible,
         but continues to use the same printer so outputs will be appended.
     */

    int NextCounter(WGL_HLSLBuiltins::DX9Semantic s);
    /**< For the given 'semantic', return the next index to use (when declaring a variable)
         as having that semantic. Returns (-1) if the semantic is not the indexable kind. */

    WGL_CgGenerator *GetGenerator() { return &out_gen; }
    Configuration *GetConfiguration() { return config; }
    WGL_Context *GetContext() { return context; }

private:
    friend class WGL_ProcessVertex;
    friend class WGL_ProcessFragmentDecls;
    friend class WGL_ProcessPixelFunction;
    friend class WGL_EscapedVars;
    friend class WGL_CgTypes;

    void ProcessVertexShader(WGL_DeclList *ls);
    void ProcessFragmentShader(WGL_DeclList *ls, BOOL with_vertex_shader);

    void EmitVertexShaderHeader(WGL_Printer *out_sink, BOOL for_attributes);
    void EmitPixelShaderHeader(WGL_Printer *out_sink);

    void EmitVertexShaderStruct(BOOL for_pixel_shader);

    enum SemanticHint
    {
        SemanticIgnore,
        SemanticVertex,
        SemanticPixel,
        SemanticBoth
    };

    void EmitTypeDecl(WGL_Printer    *out_sink,
                      WGL_Type       *ty,
                      const uni_char *var,
                      SemanticHint   hint,
                      BOOL           is_decl = TRUE);

    BOOL OutputSemanticForBuiltin(const uni_char *var_name,
                                  const WGL_HLSLBuiltins::SemanticInfo *semantic_info,
                                  unsigned size);

    void EmitVertexMain(WGL_FunctionDefn *fun);
    void EmitVertexTypeDecls(WGL_DeclList *ls);
    void EmitUniforms(BOOL for_vertex);

    void EmitPixelMain(WGL_FunctionDefn *fun, BOOL with_vertex_shader);
    void EmitPixelTypeDecls(WGL_DeclList *ls, BOOL with_vertex_shader);

    WGL_Context *context;
    Configuration *config;
    WGL_Printer *out_sink;
    WGL_CgGenerator out_gen;

    WGL_ExprList *vs_args;
    WGL_ExprList *ps_args;
    WGL_ShaderVariables *variables;
    WGL_ShaderVariables builtin_variables_vertex;
    WGL_ShaderVariables builtin_variables_fragment;
    WGL_ASTBuilder *builder;

    unsigned semantic_counters[WGL_HLSLBuiltins::LAST_SEMANTIC];
};
#endif // WGL_CGBUILDER_H
