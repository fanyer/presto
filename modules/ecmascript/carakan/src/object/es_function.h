/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_FUNCTION_H
#define ES_FUNCTION_H

#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_special_property.h"
#include "modules/memory/src/memory_executable.h"

class ES_FunctionCode;
class ES_Execution_Context;

#define MAKE_BUILTIN(length, name) ES_Function::Make(context, global_object, length, name, ESID_##name, TRUE)
#define MAKE_BUILTIN_NO_PROTOTYPE(length, name) ES_Function::Make(context, global_object, length, name, ESID_##name, TRUE)
#define MAKE_BUILTIN_WITH_NAME(length, name, sname) ES_Function::Make(context, global_object, length, name, ESID_##sname, TRUE)
#define MAKE_BUILTIN_ANONYMOUS(length, name) ES_Function::Make(context, global_object, length, name, 0, TRUE)

class ES_Function : public ES_Object
{
public:
    enum ClassPropertyIndex
    {
        LENGTH = 0,
        NAME   = 1,
        PROTOTYPE = 2,

        /* use prefixes to distinguish between native and builtin functions; latter being without a prototype property. */
        NATIVE_PROTOTYPE = 2,
        NATIVE_ARGUMENTS = 3,
        NATIVE_CALLER = 4,

        BUILTIN_ARGUMENTS = 2,
        BUILTIN_CALLER = 3
    };

    typedef BOOL (ES_CALLING_CONVENTION BuiltIn)(ES_Execution_Context *, unsigned, ES_Value_Internal *, ES_Value_Internal *);

    static ES_Object *MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance);

    static ES_Function *Make(ES_Context *context, ES_Global_Object *global_object, ES_FunctionCode *function_code, unsigned scope_chain_length);

    static ES_Function *Make(ES_Context *context, ES_Global_Object *global_object, unsigned length, BuiltIn *call, unsigned builtin_name, BOOL is_builtin_function = FALSE, ES_Class *klass = NULL);

    static ES_Function *Make(ES_Context *context, ES_Class *function_class, ES_Global_Object *global_object, unsigned nformals, BuiltIn *call, BuiltIn *construct, unsigned builtin_name, JString *name, ES_Object *prototype_property);
    /**< The provided ES_Class has to have 'length' at index 0 and 'prototype' at index 1
    */

    static void CreateFunctionClasses(ES_Context *context, ES_Class *function_class, ES_Class *&native_function_class, ES_Class *&native_function_with_proto_class, ES_Class *&builtin_function_class);

    static void Destroy(ES_Function *function);

    ES_Global_Object *&GetGlobalObject() { return global_object; }
    ES_FunctionCode *&GetFunctionCode() { return function_code; }
    BuiltIn *GetCall() { return data.builtin.call; }
    BuiltIn *GetConstruct() { return data.builtin.construct; }
    unsigned GetBuiltInInformation() { return data.builtin.information; }
    JString *GetBuiltInName(ES_Context *context);
    JString *GetName(ES_Context *context);
    inline unsigned GetLength();

    void GetLexical(ES_Value_Internal &dst, unsigned scope_index, ES_PropertyIndex variable_index)
    {
        ES_Object *scope = scope_chain[scope_index];

        if (!scope->GetPropertyValueAtIndex(variable_index).IsBoxed())
            scope->GetCachedAtIndex(variable_index, dst);
        else
        {
            ES_Special_Aliased *alias = static_cast<ES_Special_Aliased *>(scope->GetPropertyValueAtIndex(variable_index).GetBoxed());
            dst = *alias->property;
        }
    }

    void PutLexical(unsigned scope_index, ES_PropertyIndex variable_index, const ES_Value_Internal &src)
    {
        ES_Object *scope = scope_chain[scope_index];

        if (!scope->GetPropertyValueAtIndex(variable_index).IsBoxed())
            scope->PutCachedAtIndex(variable_index, src);
        else
        {
            ES_Special_Aliased *alias = static_cast<ES_Special_Aliased *>(scope->GetPropertyValueAtIndex(variable_index).GetBoxed());
            *alias->property = src;
        }
    }

    ES_Object **GetScopeChain() { return scope_chain; }
    unsigned GetScopeChainLength() { return scope_chain_length; }
    void SetScopeChainLength(unsigned length) { scope_chain_length = length; }

    static void Initialize(ES_Function *function, ES_Class *function_class, ES_Global_Object *global_object, ES_FunctionCode *function_code, BuiltIn *call, BuiltIn *construct);

#ifdef ES_NATIVE_SUPPORT
    void SetHasCreatedArgumentsArray(ES_Context *context);
    void ResetNativeConstructorDispatcher();

    static unsigned GetPropertyOffset(ClassPropertyIndex property);
    /* Return the byte offset into the properties array where
       the given property value is stored. */
#endif // ES_NATIVE_SUPPORT

    inline BOOL IsBoundFunction();

protected:
    friend class ES_Native;
    friend class ES_Execution_Context;
    friend class ESMM;

    ES_Global_Object *global_object;
    ES_FunctionCode *function_code;

    union
    {
        struct
        {
            ES_Arguments_Object *unused_arguments;
#ifdef ES_NATIVE_SUPPORT
            void *native_ctor_dispatcher;
            ES_NativeCodeBlock *native_ctor_code_block;
#endif // ES_NATIVE_SUPPORT
        } native;

        struct
        {
            BuiltIn *call, *construct;
            unsigned information; ///< 0-15: name (ESID_*), 16-31: nformals
        } builtin;
    } data;

    unsigned scope_chain_length;
    /**< Length of scope chain. */
    ES_Object *scope_chain[1];
    /**< Scope chain excluding the global object (NULL if that makes it empty.)
         Local scopes of enclosing functions are assumed to be variable objects
         by being accessed by generated ESI_{GET,PUT}_LEXICAL instructions. */
};

class ES_BoundFunction : public ES_Function
{
public:
    static ES_BoundFunction *Make(ES_Execution_Context *context, ES_Object *target_function, const ES_Value_Internal &bound_this, const ES_Value_Internal *bound_args, unsigned bound_args_count, unsigned length);

    inline ES_Object *GetTargetFunction() { return target_function; }
    inline unsigned GetBoundArgsCount() { return bound_args_count; }
    inline const ES_Value_Internal &GetBoundThis() { return bound_this; }
    inline const ES_Value_Internal *GetBoundArgs() { return bound_args; }

private:
    static void Initialize(ES_BoundFunction *function, ES_Class *function_class, ES_Global_Object *global_object, ES_Object *target_function, const ES_Value_Internal &bound_this, const ES_Value_Internal *bound_args, unsigned bound_args_count, unsigned length);

    ES_Object *target_function;
    unsigned bound_args_count;
    ES_Value_Internal bound_this;
    ES_Value_Internal bound_args[1];
};

#endif // ES_FUNCTION_H
