/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_TYPEDARRAY_BUILTINS_H
#define ES_TYPEDARRAY_BUILTINS_H

#include "modules/ecmascript/carakan/src/object/es_typedarray.h"

class ES_ArrayBufferBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    enum
    {
        ES_ArrayBufferBuiltins_length = 0,
        ES_ArrayBufferBuiltins_byteLength,
        ES_ArrayBufferBuiltins_constructor,
        ES_ArrayBufferBuiltins_slice,

        ES_ArrayBufferBuiltinsCount,
        ES_ArrayBufferBuiltinsCount_LAST_PROPERTY = ES_ArrayBufferBuiltinsCount
    };

    static void PopulateGlobalObject(ES_Context *context, ES_Global_Object *global_object);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class);
    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);

    static BOOL ES_CALLING_CONVENTION slice(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
};

class ES_TypedArrayBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor_construct_Int8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION constructor_construct_Uint8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION constructor_construct_Uint8Clamped(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION constructor_construct_Int16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION constructor_construct_Uint16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION constructor_construct_Int32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION constructor_construct_Uint32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION constructor_construct_Float32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION constructor_construct_Float64(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    enum
    {
        ES_TypedArrayBuiltins_length = 0,
        ES_TypedArrayBuiltins_buffer,
        ES_TypedArrayBuiltins_byteLength,
        ES_TypedArrayBuiltins_byteOffset,
        ES_TypedArrayBuiltins_constructor,
        ES_TypedArrayBuiltins_set,
        ES_TypedArrayBuiltins_subarray,

        ES_TypedArrayBuiltinsCount,
        ES_TypedArrayBuiltinsCount_LAST_PROPERTY = ES_TypedArrayBuiltinsCount
    };

    static void PopulateGlobalObject(ES_Context *context, ES_Global_Object *global_object);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class);
    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype, ES_TypedArray::Kind kind);

    static BOOL ES_CALLING_CONVENTION set(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION subarray(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

private:
    typedef BOOL (ES_CALLING_CONVENTION ConstructorFun)(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static ConstructorFun *GetConstructorFunction(ES_TypedArray::Kind kind);

};

/** DataView is the heterogeneous view of an ArrayBuffer, providing methods
    for indexing the buffer at any of the supported basic types. */
class ES_DataViewBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'DataView()' */
    static BOOL ES_CALLING_CONVENTION constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'new DataView()' */

    static void PopulateGlobalObject(ES_Context *context, ES_Global_Object *global_object);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class);
    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);

    enum
    {
        ES_DataViewBuiltins_length = 0,
        ES_DataViewBuiltins_buffer,
        ES_DataViewBuiltins_byteOffset,
        ES_DataViewBuiltins_byteLength,
        ES_DataViewBuiltins_constructor,
        ES_DataViewBuiltins_getInt8,
        ES_DataViewBuiltins_getUint8,
        ES_DataViewBuiltins_getInt16,
        ES_DataViewBuiltins_getUint16,
        ES_DataViewBuiltins_getInt32,
        ES_DataViewBuiltins_getUint32,
        ES_DataViewBuiltins_getFloat32,
        ES_DataViewBuiltins_getFloat64,
        ES_DataViewBuiltins_setInt8,
        ES_DataViewBuiltins_setUint8,
        ES_DataViewBuiltins_setInt16,
        ES_DataViewBuiltins_setUint16,
        ES_DataViewBuiltins_setInt32,
        ES_DataViewBuiltins_setUint32,
        ES_DataViewBuiltins_setFloat32,
        ES_DataViewBuiltins_setFloat64,

        ES_DataViewBuiltinsCount,
        ES_DataViewBuiltinsCount_LAST_PROPERTY = ES_DataViewBuiltinsCount
    };

    static BOOL ES_CALLING_CONVENTION getInt8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUint8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getInt16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUint16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getInt32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUint32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getFloat32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getFloat64(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setInt8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUint8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setInt16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUint16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setInt32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUint32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setFloat32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setFloat64(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
};

#endif // ES_TYPEDARRAY_BUILTINS_H
