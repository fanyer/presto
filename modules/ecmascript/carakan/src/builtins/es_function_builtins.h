/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_FUNCTION_BUILTINS_H
#define ES_FUNCTION_BUILTINS_H

class ES_FunctionBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'Function()' and 'new Function()' */

    static BOOL ES_CALLING_CONVENTION toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION apply(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION bind(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION bound_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION bound_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_klass);

    enum
    {
        ES_FunctionBuiltins_length,
        ES_FunctionBuiltins_name,
        ES_FunctionBuiltins_constructor,
        ES_FunctionBuiltins_toString,
        ES_FunctionBuiltins_apply,
        ES_FunctionBuiltins_call,
        ES_FunctionBuiltins_bind,

        ES_FunctionBuiltinsCount,
        ES_FunctionBuiltins_LAST_PROPERTY = ES_FunctionBuiltins_call
    };
};

#endif // ES_FUNCTION_BUILTINS_H
