/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_OBJECT_BUILTINS_H
#define ES_OBJECT_BUILTINS_H

class ES_ObjectBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'Object()' and 'new Object()' */

    static BOOL ES_CALLING_CONVENTION toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toLocaleString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION hasOwnProperty(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION isPrototypeOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION propertyIsEnumerable(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION defineGetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION defineSetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION lookupGetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION lookupSetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION protoGetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION protoSetter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_klass);

    enum
    {
        ES_ObjectBuiltins_constructor,
        ES_ObjectBuiltins_toString,
        ES_ObjectBuiltins_toLocaleString,
        ES_ObjectBuiltins_valueOf,
        ES_ObjectBuiltins_hasOwnProperty,
        ES_ObjectBuiltins_isPrototypeOf,
        ES_ObjectBuiltins_propertyIsEnumerable,
        ES_ObjectBuiltins_defineGetter,
        ES_ObjectBuiltins_defineSetter,
        ES_ObjectBuiltins_lookupGetter,
        ES_ObjectBuiltins_lookupSetter,
        ES_ObjectBuiltins_proto,

        ES_ObjectBuiltinsCount,
        ES_ObjectBuiltins_LAST_PROPERTY = ES_ObjectBuiltins_proto
    };
};

class ES_ObjectConstructorBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION getPrototypeOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getOwnPropertyDescriptor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getOwnPropertyNames(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION create(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION defineProperty(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION defineProperties(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION seal(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION freeze(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION preventExtensions(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION isSealed(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION isFrozen(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION isExtensible(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION keys(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulateConstructor(ES_Context *context, ES_Global_Object *global_object, ES_Object *constructor);

    enum
    {
        ES_ObjectConstructorBuiltins_getPrototypeOf,
        ES_ObjectConstructorBuiltins_getOwnPropertyDescriptor,
        ES_ObjectConstructorBuiltins_getOwnPropertyNames,
        ES_ObjectConstructorBuiltins_create,
        ES_ObjectConstructorBuiltins_defineProperty,
        ES_ObjectConstructorBuiltins_defineProperties,
        ES_ObjectConstructorBuiltins_seal,
        ES_ObjectConstructorBuiltins_freeze,
        ES_ObjectConstructorBuiltins_preventExtensions,
        ES_ObjectConstructorBuiltins_isSealed,
        ES_ObjectConstructorBuiltins_isFrozen,
        ES_ObjectConstructorBuiltins_isExtensible,
        ES_ObjectConstructorBuiltins_keys,

        ES_ObjectConstructorBuiltinsCount,
        ES_ObjectConstructorBuiltins_LAST_PROPERTY = ES_ObjectConstructorBuiltins_keys
    };
};

#endif // ES_OBJECT_BUILTINS_H
