/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_BOOLEAN_BUILTINS_H
#define ES_BOOLEAN_BUILTINS_H

class ES_BooleanBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'Boolean()' */
    static BOOL ES_CALLING_CONVENTION constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'new Boolean()' */

    static BOOL ES_CALLING_CONVENTION toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_klass);

    enum
    {
        ES_BooleanBuiltins_constructor = 0,
        ES_BooleanBuiltins_toString,
        ES_BooleanBuiltins_valueOf,

        ES_BooleanBuiltinsCount,
        ES_BooleanBuiltins_LAST_PROPERTY = ES_BooleanBuiltins_valueOf
    };
};

#endif // ES_BOOLEAN_BUILTINS_H
