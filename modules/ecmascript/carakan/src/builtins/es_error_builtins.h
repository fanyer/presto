/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_ERROR_BUILTINS_H
#define ES_ERROR_BUILTINS_H

class ES_ErrorBuiltins
{
public:
    static BOOL CommonErrorConstructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, ES_Class *klass);

    static void CommonPopulatePrototype(ES_Context *context, ES_Object *prototype, JString *error_name, BOOL with_empty_message);
    static void CommonPopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class);

    static BOOL ES_CALLING_CONVENTION toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class);

    enum
    {
        ES_CommonErrorBuiltins_constructor = 0,
        ES_CommonErrorBuiltins_name,
        ES_CommonErrorBuiltins_message,

        ES_CommonErrorBuiltinsCount,
        ES_CommonErrorBuiltins_LAST_PROPERTY = ES_CommonErrorBuiltins_message
    };

    enum
    {
        ES_ErrorBuiltins_toString = ES_CommonErrorBuiltinsCount,

        ES_ErrorBuiltinsCount,
        ES_ErrorBuiltins_LAST_PROPERTY = ES_ErrorBuiltins_toString
    };
};

class ES_EvalErrorBuiltins : public ES_ErrorBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
};

class ES_RangeErrorBuiltins : public ES_ErrorBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
};

class ES_ReferenceErrorBuiltins : public ES_ErrorBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
};

class ES_SyntaxErrorBuiltins : public ES_ErrorBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
};

class ES_TypeErrorBuiltins : public ES_ErrorBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
};

class ES_URIErrorBuiltins : public ES_ErrorBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
};

#endif // ES_ERROR_BUILTINS_H
