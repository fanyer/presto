/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL compiler -- compilation context.
 *
 */
#ifndef WGL_CONTEXT_H
#define WGL_CONTEXT_H
#include "modules/webgl/src/wgl_glsl.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_error.h"
#include "modules/webgl/src/wgl_validator.h"
#include "modules/webgl/src/wgl_intrinsics.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"

#define WGL_UNIQUE_VAR_PREFIX UNI_L("webgl_op_uniq")

class OpMemGroup;
class WGL_Identifier_List;
class WGL_ASTBuilder;
class WGL_ShaderVariables;

class WGL_IdBuilder
{
public:
    virtual WGL_Type *NewTypeName(const uni_char *nm) = 0;
    virtual WGL_Type *NewTypeName(WGL_String *s) = 0;

    virtual BOOL IsTypeName(WGL_String *s) = 0;
};

class WGL_SourceLoc
{
public:
    WGL_SourceLoc(const uni_char *loc, unsigned l)
        : location(loc)
        , line_number(l)
    {
    }

    WGL_SourceLoc(WGL_SourceLoc &loc)
        : location(loc.location)
        , line_number(loc.line_number)
    {
    }

    const uni_char *location;
    /**< File/ URL / source context of the location. NOTE: shared and not owned by the value. */
    unsigned line_number;
};

class WGL_VariableUse;

class WGL_FunctionDecl
    : public ListElement<WGL_FunctionDecl>
{
public:
    WGL_FunctionDecl(WGL_SourceLoc location, WGL_Type *return_type, WGL_ParamList *p, BOOL proto)
        : location(location)
        , return_type(return_type)
        , parameters(p)
        , callers(NULL)
        , is_proto(proto)
    {
    }

    virtual ~WGL_FunctionDecl()
    {
    }

    WGL_SourceLoc location;
    WGL_Type *return_type;
    WGL_ParamList *parameters;
    List<WGL_VariableUse> *callers;
    /**< List of user-defined functions directly called by
         this function. Used for tracking dependencies and
         catch out recursion (which is illegal.) */

    BOOL is_proto;
};

class WGL_FunctionData
    : public ListElement<WGL_FunctionData>
{
public:
    WGL_FunctionData(WGL_String *name)
        : name(name)
    {
    }

    WGL_String *name;
    List<WGL_FunctionDecl> decls;
};

class WGL_Attribute
    : public ListElement<WGL_Attribute>
{
public:
    WGL_Attribute(WGL_SourceLoc &loc)
        : type(NULL)
        , var(NULL)
        , location(loc)
        , usages(0)
    {
    }

    WGL_Type *type;
    WGL_VarName *var;
    WGL_SourceLoc location;

    unsigned usages;
    /**< The number of uses of this attribute. */
};

/** Uniforms are shared across vertex and fragment shaders. Track
    and record uses of them during processing of the vertex shader,
    so as to be able to verify matching decls when processing the
    fragment shader. Reused for varying variables --
    Varying variables are written by the vertex shader, read by
    the fragment shader (wrt the fragment's position within
    the primitive.) */
class WGL_VaryingOrUniform
    : public ListElement<WGL_VaryingOrUniform>
{
public:
    WGL_VaryingOrUniform(WGL_Type *t, WGL_VarName *v, WGL_Type::Precision p, WGL_SourceLoc &loc)
        : type(t)
        , var(v)
        , precision(p)
        , location(loc)
        , usages(0)
    {
    }

    WGL_Type *type;
    WGL_VarName *var;
    WGL_Type::Precision precision;
    WGL_SourceLoc location;

    unsigned usages;
    /**< The number of uses of this uniform/varying. */
};

template<class E>
class WGL_BindingScope
    : public ListElement<WGL_BindingScope<E> >
{
public:
    WGL_BindingScope()
        : list(NULL)
    {
    }

    List<E> *list;
};

class WGL_Precision
    : public ListElement<WGL_Precision>
{
public:
    WGL_Precision(WGL_Type *t, WGL_Type::Precision p)
        : type(t)
        , precision(p)
    {
    }

    WGL_Type *type;
    WGL_Type::Precision precision;
};

class WGL_BuiltinType
    : public ListElement<WGL_BuiltinType>
{
public:
    enum Type
    {
        Basic = 1,
        Vector,
        Matrix,
        Sampler,
        // collection of types; see intrinsics list.
        Gen,
        Mat,
        Vec,
        IVec,
        BVec
    };

    union Detail
    {
        WGL_BasicType::Type basic_type;
        WGL_VectorType::Type vector_type;
        WGL_MatrixType::Type matrix_type;
        WGL_SamplerType::Type sampler_type;
    };

    BOOL Matches(WGL_BasicType::Type type, BOOL strict);
    BOOL Matches(WGL_VectorType::Type type, BOOL strict);
    BOOL Matches(WGL_MatrixType::Type type, BOOL strict);
    BOOL Matches(WGL_SamplerType::Type type, BOOL strict);

    Type type;
    Detail value;
};

class WGL_Builtin
    : public ListElement<WGL_Builtin>
{
public:
    virtual BOOL IsBuiltinVar() = 0;
    virtual BOOL IsBuiltinConstant() = 0;
};

class WGL_BuiltinFun
    : public WGL_Builtin
{
public:
    virtual ~WGL_BuiltinFun()
    {
    }

    virtual BOOL IsBuiltinVar() { return FALSE; }
    virtual BOOL IsBuiltinConstant() { return FALSE; }

    const uni_char *name;
    WGL_GLSL::Intrinsic intrinsic;
    unsigned arity;
    WGL_BuiltinType *return_type;
    List <WGL_BuiltinType> parameters;
};

class WGL_ArraySize
{
public:
    enum Kind
    {
        NoArray = 0,
        SizeConstant,
        SizeSymbolic
    };

    union Value
    {
        unsigned size_constant;
        const char *size_symbolic;
    };

    WGL_ArraySize()
        : kind(NoArray)
    {
    }

    WGL_ArraySize(unsigned l)
        : kind(l > 0 ? SizeConstant : NoArray)
    {
        value.size_constant = l;
    }

    WGL_ArraySize(const char *s)
        : kind(s ? SizeSymbolic : NoArray)
    {
        value.size_symbolic = s;
    }

    Kind kind;
    Value value;
};

class WGL_BuiltinVar
    : public WGL_Builtin
{
public:
    virtual BOOL IsBuiltinVar() { return TRUE; }
    virtual BOOL IsBuiltinConstant() { return is_const; }

    const uni_char *name;
    /**< The name of the builtin; a statically allocated string. */

    WGL_Type::Precision precision;
    /**< The precision of the builtin, as supported by the implementation/platform. */

    WGL_BuiltinType type;

    WGL_ArraySize array_size;
    /**< A non-array, constant-sized array, or an array of some unknown symbolic size. */

    BOOL is_read_only;
    /**< TRUE if assignments to variable should be reported as errors. */

    BOOL is_output;
    /**< TRUE if passed as output from this shader. */

    BOOL is_const;
    /**< TRUE if a constant value (see 'const_value' for what that is.) */

    int const_value;

    BOOL for_vertex;
};

class WGL_VarBinding
    : public ListElement<WGL_VarBinding>
{
public:
    WGL_VarBinding(WGL_Type *t, WGL_VarName *i, BOOL is_read_only)
        : var(i)
        , type(t)
        , value(NULL)
        , is_read_only(is_read_only)
    {
    }

    WGL_VarName *var;
    WGL_Type *type;

    WGL_Expr *value;
    /**< 'value' is the currently known value for 'var'; it may not be known. */

    BOOL is_read_only;
    /**< TRUE if the binding may not be assigned to (or passed as a reference to a function.) */
};

/* Assoc list entry for variable names. */
class WGL_VarAlias
    : public ListElement<WGL_VarAlias>
{
public:
    WGL_VarAlias(WGL_VarName *a, WGL_VarName *b)
        : var1(a)
        , var2(b)
    {
    }

    WGL_VarName *var1;
    WGL_VarName *var2;
};

/* Variable list element. */
class WGL_VariableUse
    : public ListElement<WGL_VariableUse>
{
public:
    WGL_VariableUse(WGL_VarName *a)
        : var(a)
    {
    }

    static BOOL IsIn(List<WGL_VariableUse> &list, WGL_VarName *v);

    WGL_VarName *var;
};

class WGL_ValidationState
{
public:
    WGL_ValidationState()
        : context(NULL)
        , scope_level(0)
        , current_function(NULL)
        , current_callers(NULL)
    {
    }

    void Release();

    void EnterScope(WGL_Type *return_type);
    void LeaveScope();
    unsigned GetScopeLevel() { return scope_level; }
    void ResetL();

    void AddAttribute(WGL_Type *type, WGL_VarName *i, unsigned line_number);
    void AddUniform(WGL_Type *type, WGL_VarName *v, WGL_Type::Precision p, unsigned line_number);
    void AddVarying(WGL_Type *type, WGL_VarName *v, WGL_Type::Precision p, unsigned line_number);
    void AddPrecision(WGL_Type *type, WGL_Type::Precision p);
    WGL_VarBinding *AddVariable(WGL_Type *t, WGL_VarName *i, BOOL is_read_only);
    void AddAlias(WGL_VarName *old_name, WGL_VarName *new_name);
    WGL_String *AddStructType(WGL_String *str, WGL_Type *type);
    void AddFunction(WGL_VarName *fun_name, WGL_Type *return_type, WGL_ParamList *params, BOOL is_proto, WGL_SourceLoc &loc);

    WGL_Type *LookupTypeName(WGL_String *type_name);
    WGL_Type *LookupVarType(WGL_String *name, WGL_VarName **aliased_var, BOOL current_scope_only = FALSE, BOOL deref_name = TRUE);
    WGL_VarBinding *LookupVarBinding(WGL_String *name);
    WGL_Type *LookupLocalVar(WGL_String *name, BOOL current_scope_only = FALSE);
    WGL_Type *LookupGlobal(WGL_String *name);
    WGL_VaryingOrUniform *LookupUniform(WGL_String *name, BOOL record_use = FALSE);
    WGL_VaryingOrUniform *LookupVarying(WGL_String *name, BOOL record_use = FALSE);
    WGL_Attribute *LookupAttribute(WGL_String *name, BOOL record_use = FALSE);
    WGL_FunctionData *LookupFunction(WGL_String *name);
    List<WGL_Builtin> *LookupBuiltin(WGL_String *i);
    WGL_Type::Precision LookupPrecision(WGL_Type *t);

    WGL_Field *LookupField(WGL_FieldList *field_list, WGL_String *field);

    WGL_BindingScope<WGL_Precision> *GetPrecisionScope() { return precision_environment.First(); }

    WGL_FunctionData *GetFirstFunction() { return function_scope.First(); }

    OpPointerHashTable<WGL_String, List<WGL_Builtin> > &GetBuiltins() { return builtins_map; }

    BOOL IsOverlappingOverload(WGL_ParamList *params, WGL_Type *return_type, BOOL is_proto, WGL_FunctionDecl *&overlap, BOOL &duplicate, List<WGL_FunctionDecl> &existing);

    WGL_Type *NormaliseType(WGL_Type *t);
    static BOOL IsSameType(WGL_Type *t1, WGL_Type *t2, BOOL type_equality);
    BOOL IsConstantExpression(WGL_Expr *e);
    BOOL HasExpressionType(WGL_Type *t, WGL_Expr *e, BOOL type_equality);
    WGL_Literal *EvalConstExpression(WGL_Expr::Operator op, WGL_Literal::Type t1, WGL_Literal::Value v1, WGL_Literal::Type t2, WGL_Literal::Value v2);
    WGL_Literal *EvalConstExpression(WGL_Expr *e);

    WGL_Type *GetExpressionType(WGL_Expr *e);
    BOOL IsConstantBuiltin(const uni_char *s);
    BOOL MatchesBuiltinType(WGL_BuiltinType *t, WGL_Type *type);
    BOOL ConflictsWithBuiltin(WGL_VarName *fun_name, WGL_ParamList *params, WGL_Type *return_type);
    WGL_Type *FromBuiltinType(WGL_BuiltinType *t);

    WGL_BuiltinFun *ResolveBuiltinFunction(List<WGL_Builtin> *builtins, WGL_ExprList *args);
    /**< Builtin overload resolution.

         Given a list of candidate overloads for a builtin (either
         functions or variables), attempt to resolve the overload
         of an application using 'args' as arguments.

         Returns NULL if no match or the matching builtin function. */

    WGL_Type *ResolveBuiltinReturnType(WGL_BuiltinFun *builtin, WGL_ExprList *args, WGL_Type *arg1_type);
    /**< Builtin return type check resolution.

         Resolve the return type of a builtin application. 'arg1_type' is
         the already resolved type of the first argument of the application. */

    WGL_Literal *EvalBuiltinFunction(WGL_VarExpr *fun, List<WGL_Builtin> *builtins, WGL_ExprList *args);
    /**< Builtin constant folding.

         Evaluate a constant application of a builtin function. Used when
         reducing loop bounds and computing the static sizes of arrays.

         Returns the literal result or NULL if not computable or supported. */

    BOOL IsLegalReferenceArg(WGL_Expr *e);
    /**< TRUE if expression can be passed as an 'out' or 'inout' parameter */

    void ValidateEqualityOperation(WGL_Type *t);
    /**< Check if a type can be legally used in a (==) or (!=) application. */

    BOOL LiteralToInt(WGL_Literal *l, int &val);
    BOOL LiteralToDouble(WGL_Literal *l, double &val);

    WGL_VarName *SetFunction(WGL_VarName *new_function, List<WGL_VariableUse> *&callers)
    {
        WGL_VarName *old = current_function;
        List<WGL_VariableUse> *old_calls = current_callers;
        current_function = new_function;
        current_callers = callers;
        callers = old_calls;
        return old;
    }

    WGL_VarName *CurrentFunction() { return current_function; }

    void AddFunctionCall(WGL_VarName *f);

    List<WGL_VariableUse> *GetFunctionCalls() { return current_callers; }

    static void CheckVariableConsistency(WGL_Context *context, WGL_ShaderVariables *v1, WGL_ShaderVariables *v2);

    static void CheckVariableAliases(WGL_Context *context, WGL_ShaderVariables *vs, const WGL_ShaderVariable *v, WGL_ShaderVariable::Kind kind);

    static void CheckVariableAliasOverlap(WGL_Context *context, WGL_ShaderVariables *v1, WGL_ShaderVariables *v2);

private:
    friend class WGL_Context;

    WGL_Context *context;
    unsigned scope_level;

    List<WGL_Attribute> attributes;
    /**< List of vertex shader 'attribute' declarations. */

    List<WGL_VaryingOrUniform> uniforms;
    /**< List of vertex shader uniform declarations. */

    List<WGL_VaryingOrUniform> varyings;
    /**< List of vertex + fragment varying declarations. */

    List< WGL_BindingScope<WGL_VarBinding> > variable_environment;
    /**< The variables in scope. */

    List< WGL_BindingScope<WGL_Precision> > precision_environment;
    /**< The (scoped) environment of precision declarations. */

    List<WGL_VarBinding> global_scope;
    /**< The variables at global scope. */

    List<WGL_FunctionData> function_scope;
    /**< The functions in scope, including prototypes. */

    OpPointerHashTable<WGL_String, List<WGL_Builtin> > builtins_map;
    /**< The builtins known for the current language (vertex/fragment.)
         Builtin names may have more than one instantiation and a
         given instantiation might be polymorphic. */

    OpPointerHashTable<WGL_String, List<WGL_Type> > types_map;
    /**< The name to type mapping; all types in GLSL are at the same scope, but
     *   more than one overloading may exist for a given name. */

    List< WGL_BindingScope<WGL_VarAlias> > alias_scope;
    /**< Scoped alias environment. Used to provide a remapping of names in
         the input to something else (and more appropriate) in the output. */

    WGL_VarName *current_function;
    /**< If non-NULL, the function definition being validated. */

    List<WGL_VariableUse> *current_callers;
    /**< If validation a function definition, the list
         user-defined functions being called. */

    BOOL GetTypeForSwizzle(WGL_VectorType *vec_type, WGL_SelectExpr *sel_expr, WGL_Type *&result);
    /**< GetExpressionType() helper function. Sets 'result' to the type of the result of the
         swizzle operation in 'sel_expr' with associated vector type 'vec_type'
         upon success, and returns TRUE. Returns FALSE if an internal error
         occurs. */

    static void CheckVariableConsistency(WGL_Context *context, WGL_ShaderVariable::Kind kind, List<WGL_ShaderVariable> &l1, List<WGL_ShaderVariable> &l2);
};

class WGL_Context
   : public WGL_IdBuilder
{
public:
    WGL_Context(OpMemGroup *region)
        : arena(region)
        , strings_table(NULL)
        , unique_table(NULL)
        , builder(NULL)
        , file_name(NULL)
        , line_number(1)
        , id_unique(0)
        , configuration(NULL)
        , extensions_enabled(0)
        , extensions_supported(0)
        , extensions_supported_count(0)
        , processing_fragment(FALSE)
        , uses_clamp(FALSE)
    {
        validation_state.context = this;
    }

    virtual ~WGL_Context();

    void Release();

    static OP_STATUS Make(WGL_Context *&new_context, OpMemGroup *region, BOOL support_oes_extension);

    OP_STATUS RecordResult(BOOL for_vertex_shader, WGL_ShaderVariables *result);
    /**< After the completion of shader validation, copy and record the
         input/output variables that the shader program declared. */

    void SetASTBuilder(WGL_ASTBuilder *b) { builder = b; }

    BOOL IsTypeName(WGL_String *s);
    BOOL IsUnique(WGL_String *s);

    void ResetLocalsL();
    void ResetGlobalsL();

    void ResetL(BOOL for_vertex);
    /**< If for_vertex is TRUE, then consider the context to be processing a vertex shader.
         If FALSE, then switching to processing a fragment shader. */

    void EnterLocalScope(WGL_Type *return_type);
    /**< When entering a new local scope, install a new scope context. If entering
         the scope of a function, supply its return type. */

    void LeaveLocalScope();
    /**< Remove and delete the innermost local scope. */

    OpMemGroup *Arena() { return arena; }
    /**< Return the arena that can be used for allocating temporary objects that
         won't live past the lifetime of this context. */

    WGL_Identifier_List *GetStrings() { return strings_table; }
    WGL_Identifier_List *GetUniques() { return unique_table; }

    WGL_ASTBuilder *GetASTBuilder() { return builder; }

    void AddError(WGL_Error::Type t, const uni_char *msg);
    void AddError(WGL_Error *error) { error->Into(&errors); }

    WGL_Error *GetFirstError() { return errors.First(); }
    void ClearErrors() { errors.RemoveAll(); }

    virtual WGL_Type *NewTypeName(const uni_char *name);
    virtual WGL_Type *NewTypeName(WGL_String *name);

    WGL_VarName *NewUniqueVar(BOOL record_name);
    /**< Create a new unique variable name; guaranteed to not
         clash with any variables in the input nor other unique
         variables for this shader.

         @param record_name Controls if the unique name can be
                used in the program being validated. If TRUE,
                the unique variable name is recorded as an internally
                constructed name that can be legally used. */

    WGL_VarName *NewUniqueHashVar(WGL_VarName *existing);
    /**< Create a new unique variable name, including an
         existing variable's hash value as the unique value.
         Used when a shader needs an alias for a long variable
         name, but that unique alias must be the same across
         multiple shader inputs -- GLSL "varying" and "uniform"
         variables are the two kind of inter-shader variables
         requiring this. */

    WGL_VarName *FindAlias(WGL_VarName *name);
    WGL_VarName *FindAlias(const uni_char *name);

    WGL_Literal *BuildLiteral(WGL_Literal::Type t);

    WGL_ValidationState &GetValidationState() { return validation_state; }
    WGL_Validator::Configuration *GetConfiguration() { return configuration; }

    void SetSourceContextL(const char *s);
    const uni_char *GetSourceContext() { return file_name; }

    void Initialise(WGL_Validator::Configuration *config);

    List<WGL_Builtin> *NewBuiltinVar(const uni_char *name, WGL_BasicType::Type t, WGL_Type::Precision prec, BOOL is_read_only, BOOL for_out, BOOL for_vertex);
    List<WGL_Builtin> *NewBuiltinVar(const uni_char *name, WGL_VectorType::Type vt, WGL_Type::Precision prec, BOOL is_read_only, BOOL for_out, BOOL for_vertex, const char *symbolic_size = NULL);
    List<WGL_Builtin> *NewBuiltinConstant(const uni_char *name, WGL_BasicType::Type t, WGL_Type::Precision prec, int i);

    /* TODO: move somewhere else */
    static BOOL IsVectorType(unsigned t);
    static WGL_VectorType::Type ToVectorType(unsigned t);
    static BOOL IsMatrixType(unsigned t);
    static WGL_MatrixType::Type ToMatrixType(unsigned t);
    static BOOL IsSamplerType(unsigned t);
    static WGL_SamplerType::Type ToSamplerType(unsigned t);

    void SetLineNumber(unsigned l) { line_number = l; }
    unsigned GetLineNumber() { return line_number; }

    BOOL IsSupportedExtension(const uni_char *extension, unsigned extension_length);
    BOOL IsEnabledExtension(unsigned idx, WGL_GLSLBuiltins::FunctionInfo **functions, unsigned *functions_count);
    unsigned GetExtensionCount();
    BOOL EnableExtension(const uni_char *e, unsigned extension_length, const uni_char **cpp_define);
    BOOL EnableExtension(unsigned idx, const uni_char **cpp_define);
    BOOL DisableExtension(const uni_char *e, unsigned extension_length, const uni_char **cpp_define);
    BOOL DisableExtension(unsigned idx, const uni_char **cpp_define);

    BOOL GetExtensionIndex(unsigned idx, WGL_GLSLBuiltins::Extension &id);
    BOOL GetExtensionInfo(WGL_GLSLBuiltins::Extension id, const uni_char **vertex_name, const uni_char **fragment_name, const uni_char **supported_define);

    void SetUsedClamp() { uses_clamp = TRUE; }
    BOOL GetUsedClamp() { return uses_clamp; }

    BOOL IsExtensionEnabled(unsigned int index, BOOL fragment, const uni_char *&define);

private:
    void RecordResultL(BOOL for_vertex_shader, WGL_ShaderVariables *result);
    /**< After the completion of shader validation, copy and record the
         input/output variables that the shader program declared. */

protected:
    OpMemGroup *arena;

    WGL_Identifier_List *strings_table;
    /**< All strings are interned and shared; kept in the strings table. */

    WGL_Identifier_List *unique_table;
    /**< Internal, unique names generated during processing. */

    WGL_Identifier_List *types_table;
    /**< The names of types defined (used by the frontend only, validation
         step maintains a mapping that contains extra type information.) */

    WGL_ASTBuilder *builder;

    List <WGL_Error> errors;

    WGL_ValidationState validation_state;
    /**< The split between the context and validator is currently a bit fluid, but
         'validation_state' holds information that is (intended) to keep during
         validation of the vertex and fragment shaders. */

    const uni_char *file_name;
    unsigned line_number;

    unsigned id_unique;
    /**< Unique value source; used to generated new identifiers and such
         as part of validation/static analysis. */

    WGL_Validator::Configuration *configuration;
    /**< If supplied, the configuration settings that validation and code generation
          should use. */

    unsigned extensions_enabled;
    unsigned extensions_supported;
    unsigned extensions_supported_count;

    BOOL processing_fragment;

    BOOL uses_clamp;
};

#endif // WGL_CONTEXT_H
