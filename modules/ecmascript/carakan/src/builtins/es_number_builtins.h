/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_NUMBER_BUILTINS_H
#define ES_NUMBER_BUILTINS_H

class ES_NumberBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'Number()' */
    static BOOL ES_CALLING_CONVENTION constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'new Number()' */

    static BOOL ES_CALLING_CONVENTION toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toLocaleString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toFixed(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toExponential(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toPrecision(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_klass);
    static void PopulateConstructor(ES_Context *context, ES_Object *constructor);

    enum
    {
        ES_NumberBuiltins_constructor = 0,
        ES_NumberBuiltins_toString,
        ES_NumberBuiltins_toLocaleString,
        ES_NumberBuiltins_valueOf,
        ES_NumberBuiltins_toFixed,
        ES_NumberBuiltins_toExponential,
        ES_NumberBuiltins_toPrecision,

        ES_NumberBuiltinsCount,
        ES_NumberBuiltins_LAST_PROPERTY = ES_NumberBuiltins_toPrecision
    };

private:
    static BOOL ProcessThis(ES_Execution_Context *context, ES_Value_Internal &this_value);
    static BOOL toStringInternal(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, BOOL toLocale);

};

#endif // ES_NUMBER_BUILTINS_H
