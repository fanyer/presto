/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_typedarray_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_typedarray.h"

#define ES_THIS_ARRAYBUFFER() \
    if (!argv[-2].IsObject() || !argv[-2].GetObject(context)->IsArrayBufferObject()) { context->ThrowTypeError("Expected an ArrayBuffer as 'this'"); return FALSE; } \
    ES_ArrayBuffer *this_object = static_cast<ES_ArrayBuffer *>(argv[-2].GetObject(context));

#define ES_THIS_TYPEARRAY() \
    if (!argv[-2].IsObject() || !argv[-2].GetObject(context)->IsTypedArrayObject() || static_cast<ES_TypedArray *>(argv[-2].GetObject(context))->kind == ES_TypedArray::DataViewArray) { context->ThrowTypeError("Expected a TypeArray as 'this'"); return FALSE; } \
    ES_TypedArray *this_object = static_cast<ES_TypedArray *>(argv[-2].GetObject(context));

#define ES_THIS_DATAVIEW() \
    if (!argv[-2].IsObject() || !argv[-2].GetObject(context)->IsTypedArrayObject() || static_cast<ES_TypedArray *>(argv[-2].GetObject(context))->kind != ES_TypedArray::DataViewArray) { context->ThrowTypeError("Expected a DataView as 'this'"); return FALSE; } \
    ES_TypedArray *this_object = static_cast<ES_TypedArray *>(argv[-2].GetObject(context));

#define ES_CHECK_DATAVIEW_OFFSET(offset, length) \
    if ((offset) < 0 || static_cast<unsigned>(offset) >= (length)) \
    { \
        context->ThrowRangeError("Offset argument out of range"); \
        return FALSE; \
    }

#define ES_CHECK_ARITY(n) \
    if (argc < (n)) \
    { \
        context->ThrowTypeError("Not enough arguments"); \
        return FALSE; \
    }

#define ES_GET_DATAVIEW_OFFSET(OFF) \
    if (!argv[0].ToNumber(context)) \
        return FALSE; \
    int OFF = argv[0].GetNumAsInt32()

#define ES_GET_DATAVIEW_LENGTH(LEN) \
    ES_Value_Internal length_value; \
    this_object->GetCachedAtIndex(ES_PropertyIndex(0), length_value); \
    if (!length_value.ToNumber(context)) \
    { \
        context->ThrowInternalError("Not a valid DataView object"); \
        return FALSE; \
    } \
    unsigned LEN = length_value.GetNumAsUInt32()

#define ES_CONSTRUCT_TYPEARRAY(ty) \
    if (ES_TypedArray *array = ES_TypedArray::Make(context, ty, argc, argv)) \
    { \
        return_value->SetObject(array); \
        return TRUE; \
    } \
    else \
        return FALSE

static void
ClampRange(int length, int& begin, int& end)
{
    if (begin < 0)
        begin = length + begin;
    if (begin > length)
        begin = length;
    if (begin < 0)
        begin = 0;
    if (end < 0)
        end = length + end;
    if (end > length)
        end = length;
    if (end < 0)
        end = 0;
    if (end < begin)
        end = begin;
}

/* static */ BOOL
ES_ArrayBufferBuiltins::constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (ES_ArrayBuffer *array = ES_ArrayBuffer::Make(context, context->GetGlobalObject(), argc, argv))
    {
        return_value->SetObject(array);
        return TRUE;
    }
    else
        return FALSE;
}

/* static */ void
ES_ArrayBufferBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_ArrayBufferBuiltinsCount);

    APPEND_PROPERTY(ES_ArrayBufferBuiltins, length, 0);
    APPEND_PROPERTY(ES_ArrayBufferBuiltins, byteLength, 0);
    APPEND_PROPERTY(ES_ArrayBufferBuiltins, constructor, undefined);
    APPEND_PROPERTY(ES_ArrayBufferBuiltins, slice, MAKE_BUILTIN(1, slice));

    ASSERT_OBJECT_COUNT(ES_ArrayBufferBuiltins);
}

/* static */ void
ES_ArrayBufferBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_ArrayBufferBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_ArrayBufferBuiltins, length, RO | DD, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_ArrayBufferBuiltins, byteLength, RO | DD, ES_STORAGE_INT32);
    DECLARE_PROPERTY(ES_ArrayBufferBuiltins, constructor, DE, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_ArrayBufferBuiltins, slice, 0, ES_STORAGE_OBJECT);
}

/* static */ void
ES_ArrayBufferBuiltins::PopulateGlobalObject(ES_Context *context, ES_Global_Object *global_object)
{
    JString **idents = context->rt_data->idents;

    ES_Class_Singleton *arraybuffer_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "ArrayBuffer", idents[ESID_ArrayBuffer], ES_ArrayBufferBuiltinsCount);
    PopulatePrototypeClass(context, arraybuffer_class);

    ES_Object *prototype = ES_Object::Make(context, arraybuffer_class);
    arraybuffer_class->AddInstance(context, prototype);

    PopulatePrototype(context, global_object, prototype);

    ES_Object *constructor = ES_Function::Make(context, global_object->GetBuiltInConstructorClass(), global_object, 1, constructor_construct, constructor_construct, ESID_ArrayBuffer, NULL, prototype);
    prototype->InitPropertyL(context, idents[ESID_constructor], constructor, DE);
    global_object->InitPropertyL(context, idents[ESID_ArrayBuffer], constructor, DE);

    ES_Class *sub_object_class = ES_Class::MakeRoot(context, prototype, "ArrayBuffer", idents[ESID_ArrayBuffer], TRUE);
    prototype->SetSubObjectClass(context, sub_object_class);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_length], ES_Property_Info(RO|DD), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_byteLength], ES_Property_Info(RO|DD), ES_STORAGE_INT32);
    global_object->classes[ES_Global_Object::CI_ARRAYBUFFER] = sub_object_class;
}

/* static */ BOOL
ES_ArrayBufferBuiltins::slice(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_ARRAYBUFFER();
    ES_CHECK_ARITY(1);
    if (!argv[0].ToNumber(context))
        return FALSE;

    int begin = argv[0].GetNumAsInt32();

    unsigned array_length;
    if (!GET_OK(this_object->GetLength(context, array_length, TRUE)))
        return FALSE;

    int end;
    if (argc > 1)
        if (argv[1].ToNumber(context))
            end = argv[1].GetNumAsInt32();
        else
            return FALSE;
    else
        end = static_cast<int>(array_length);

    ClampRange(static_cast<int>(array_length), begin, end);

    ES_Value_Internal length_value;
    length_value.SetInt32(end - begin);
    if (ES_ArrayBuffer *array_slice = ES_ArrayBuffer::Make(context, context->GetGlobalObject(), 1, &length_value))
    {
        op_memcpy(array_slice->GetStorage(), this_object->GetStorage() + begin, end - begin);
        return_value->SetObject(array_slice);
        return TRUE;
    }
    else
        return FALSE;
}

/* static */ ES_TypedArrayBuiltins::ConstructorFun *
ES_TypedArrayBuiltins::GetConstructorFunction(ES_TypedArray::Kind kind)
{
    switch (kind)
    {
    default:
        OP_ASSERT(!"Unhandled array type, cannot happen");
    case ES_TypedArray::Int8Array:
        return constructor_construct_Int8;
    case ES_TypedArray::Int16Array:
        return constructor_construct_Int16;
    case ES_TypedArray::Int32Array:
        return constructor_construct_Int32;
    case ES_TypedArray::Uint8Array:
        return constructor_construct_Uint8;
    case ES_TypedArray::Uint8ClampedArray:
        return constructor_construct_Uint8Clamped;
    case ES_TypedArray::Uint16Array:
        return constructor_construct_Uint16;
    case ES_TypedArray::Uint32Array:
        return constructor_construct_Uint32;
    case ES_TypedArray::Float32Array:
        return constructor_construct_Float32;
    case ES_TypedArray::Float64Array:
        return constructor_construct_Float64;
    }
}

/* static */ void
ES_TypedArrayBuiltins::PopulateGlobalObject(ES_Context *context, ES_Global_Object *global_object)
{
    JString **idents = context->rt_data->idents;
    ES_TypedArray::Kind kinds[] = { ES_TypedArray::Int8Array, ES_TypedArray::Int16Array, ES_TypedArray::Int32Array, ES_TypedArray::Uint8Array, ES_TypedArray::Uint8ClampedArray, ES_TypedArray::Uint16Array, ES_TypedArray::Uint32Array, ES_TypedArray::Float32Array, ES_TypedArray::Float64Array};
    unsigned ids[] = { ESID_Int8Array, ESID_Int16Array, ESID_Int32Array, ESID_Uint8Array, ESID_Uint8ClampedArray, ESID_Uint16Array, ESID_Uint32Array, ESID_Float32Array, ESID_Float64Array};

    for (unsigned i = 0; i < ARRAY_SIZE(kinds); i++)
    {
        const char *typed_array_name = ES_TypedArray::GetKindClassName(kinds[i]);
        ES_Object *prototype_prototype = kinds[i] == ES_TypedArray::Uint8ClampedArray ? global_object->GetTypedArrayClass(ES_TypedArray::Uint8Array)->Prototype() : global_object->GetObjectPrototype();
        ES_Class_Singleton *typed_array_class = ES_Class::MakeSingleton(context, prototype_prototype, typed_array_name, idents[ids[i]], ES_TypedArrayBuiltinsCount);
        PopulatePrototypeClass(context, typed_array_class);

        ES_Object *prototype = ES_Object::Make(context, typed_array_class);
        PopulatePrototype(context, global_object, prototype, kinds[i]);
        ES_Class *constructor_class = global_object->GetBuiltInConstructorClass();
        constructor_class = ES_Class::ExtendWithL(context, constructor_class, idents[ESID_BYTES_PER_ELEMENT], ES_Property_Info(RO|DD), ES_STORAGE_INT32);

        ConstructorFun *constructor_fun = GetConstructorFunction(kinds[i]);
        ES_Object *constructor = ES_Function::Make(context, constructor_class, global_object, 1, constructor_fun, constructor_fun, ids[i], NULL, prototype);
        constructor->InitPropertyL(context, idents[ESID_BYTES_PER_ELEMENT], ES_TypedArray::GetElementSizeInBytes(kinds[i]));

        prototype->InitPropertyL(context, idents[ESID_constructor], constructor, DE);
        global_object->InitPropertyL(context, idents[ids[i]], constructor, DE);

        ES_Class *sub_object_class = ES_Class::MakeRoot(context, prototype, typed_array_name, idents[ids[i]], TRUE);
        prototype->SetSubObjectClass(context, sub_object_class);
        sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_length], ES_Property_Info(RO|DD), ES_STORAGE_WHATEVER);
        sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_buffer], ES_Property_Info(RO|DD), ES_STORAGE_WHATEVER);
        sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_byteLength], ES_Property_Info(RO|DD), ES_STORAGE_INT32);
        sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_byteOffset], ES_Property_Info(RO|DD), ES_STORAGE_INT32);
        global_object->classes[ES_Global_Object::CI_INT8ARRAY + i] = sub_object_class;
    }
}

/* static */ void
ES_TypedArrayBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype, ES_TypedArray::Kind kind)
{
    ASSERT_CLASS_SIZE(ES_TypedArrayBuiltinsCount);
    ES_Value_Internal undefined;

    APPEND_PROPERTY(ES_TypedArrayBuiltins, length, 0);
    APPEND_PROPERTY(ES_TypedArrayBuiltins, buffer, undefined);
    APPEND_PROPERTY(ES_TypedArrayBuiltins, byteLength, 0);
    APPEND_PROPERTY(ES_TypedArrayBuiltins, byteOffset, 0);

    APPEND_PROPERTY(ES_TypedArrayBuiltins, constructor, undefined);
    APPEND_PROPERTY(ES_TypedArrayBuiltins, set, MAKE_BUILTIN(1, set));
    APPEND_PROPERTY(ES_TypedArrayBuiltins, subarray, MAKE_BUILTIN(1, subarray));
}

/* static */ void
ES_TypedArrayBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_TypedArrayBuiltinsCount);

    JString **idents = context->rt_data->idents;

    DECLARE_PROPERTY(ES_TypedArrayBuiltins, length, RO | DD, ES_STORAGE_INT32);
    DECLARE_PROPERTY(ES_TypedArrayBuiltins, buffer, RO | DD, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_TypedArrayBuiltins, byteLength, RO | DD, ES_STORAGE_INT32);
    DECLARE_PROPERTY(ES_TypedArrayBuiltins, byteOffset, RO | DD, ES_STORAGE_INT32);

    DECLARE_PROPERTY(ES_TypedArrayBuiltins, constructor, DE, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_TypedArrayBuiltins, set, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_TypedArrayBuiltins, subarray, 0, ES_STORAGE_OBJECT);
}

/* static */ BOOL
ES_TypedArrayBuiltins::constructor_construct_Int8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CONSTRUCT_TYPEARRAY(ES_TypedArray::Int8Array);
}

/* static */ BOOL
ES_TypedArrayBuiltins::constructor_construct_Uint8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CONSTRUCT_TYPEARRAY(ES_TypedArray::Uint8Array);
}

/* static */ BOOL
ES_TypedArrayBuiltins::constructor_construct_Uint8Clamped(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CONSTRUCT_TYPEARRAY(ES_TypedArray::Uint8ClampedArray);
}

/* static */ BOOL
ES_TypedArrayBuiltins::constructor_construct_Int16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CONSTRUCT_TYPEARRAY(ES_TypedArray::Int16Array);
}

/* static */ BOOL
ES_TypedArrayBuiltins::constructor_construct_Uint16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CONSTRUCT_TYPEARRAY(ES_TypedArray::Uint16Array);
}

/* static */ BOOL
ES_TypedArrayBuiltins::constructor_construct_Int32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CONSTRUCT_TYPEARRAY(ES_TypedArray::Int32Array);
}

/* static */ BOOL
ES_TypedArrayBuiltins::constructor_construct_Uint32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CONSTRUCT_TYPEARRAY(ES_TypedArray::Uint32Array);
}

/* static */ BOOL
ES_TypedArrayBuiltins::constructor_construct_Float32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CONSTRUCT_TYPEARRAY(ES_TypedArray::Float32Array);
}

/* static */ BOOL
ES_TypedArrayBuiltins::constructor_construct_Float64(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CONSTRUCT_TYPEARRAY(ES_TypedArray::Float64Array);
}

static BOOL
IsInArrayRange(ES_Execution_Context *context, unsigned array_length, unsigned offset, unsigned input_length)
{
    if (input_length + offset < input_length)
    {
        context->ThrowRangeError("Input array and offset too large");
        return FALSE;
    }
    else if (input_length + offset > array_length)
    {
        context->ThrowRangeError("Input array and offset extends beyond end of array");
        return FALSE;
    }
    else
        return TRUE;
}

/* static */ BOOL
ES_TypedArrayBuiltins::set(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_TYPEARRAY();
    ES_CHECK_ARITY(1);
    if (!argv[0].IsObject())
    {
        context->ThrowTypeError("Expected object as first argument");
        return FALSE;
    }
    ES_Object *object = argv[0].GetObject(context);

    unsigned array_length = 0;
    if (!GET_OK(this_object->GetLength(context, array_length, TRUE)))
        return FALSE;

    unsigned offset;
    if (argc > 1)
        if (argv[1].ToNumber(context))
            offset = argv[1].GetNumAsUInt32();
        else
            return FALSE;
    else
        offset = 0;

    if (offset >= array_length)
    {
        context->ThrowRangeError("Offset larger than array size");
        return FALSE;
    }

    if (object->IsArrayObject())
    {
        unsigned input_length;
        if (!GET_OK(object->GetLength(context, input_length, TRUE)))
            return FALSE;

        if (!IsInArrayRange(context, array_length, offset, input_length))
            return FALSE;

        if (input_length > 0)
        {
            ES_Value_Internal value;
            GetResult result;
            for (unsigned i = 0; i < input_length; i++)
            {
                if (!GET_OK(result = object->GetL(context, i, value)))
                    if (result == PROP_GET_NOT_FOUND)
                        value.SetUndefined();
                    else
                        return FALSE;

                if (!PUT_OK(ES_Indexed_Properties::PutNoLockL(context, this_object, offset + i, NULL, value, ES_Value_Internal())))
                    return FALSE;
            }
        }
    }
    else if (object->IsTypedArrayObject())
    {
        ES_TypedArray *array_object = static_cast<ES_TypedArray *>(object);
        if (array_object->GetKind() == ES_TypedArray::DataViewArray)
        {
            context->ThrowTypeError("DataView argument not supported as first argument");
            return FALSE;
        }

        unsigned input_length;
        if (!GET_OK(array_object->GetLength(context, input_length, TRUE)))
            return FALSE;

        if (!IsInArrayRange(context, array_length, offset, input_length))
            return FALSE;

        if (input_length > 0)
        {
            ES_TypedArray::Kind this_kind = this_object->GetKind();
            unsigned byte_size = ES_TypedArray::GetElementSizeInBytes(this_kind);
            unsigned this_byte_offset = 0;
            ES_Value_Internal length_value;
            this_object->GetCachedAtIndex(ES_PropertyIndex(ES_TypedArrayBuiltins_byteOffset), length_value);
            if (length_value.ToNumber(context))
                this_byte_offset = length_value.GetNumAsUInt32();
            else
                return FALSE;

            unsigned that_byte_offset = 0;
            array_object->GetCachedAtIndex(ES_PropertyIndex(ES_TypedArrayBuiltins_byteOffset), length_value);
            if (length_value.ToNumber(context))
                that_byte_offset = length_value.GetNumAsUInt32();
            else
                return FALSE;

            unsigned char *storage = this_object->array_buffer->GetStorage();
            ES_TypedArray::Kind that_kind = array_object->GetKind();
            if (this_kind == that_kind)
            {
                /* Same type, just copy the bytes over. */
                if (this_object->array_buffer == array_object->array_buffer)
                {
                    /* Same underlying buffer, move the memory block for correct behaviour. */
                    op_memmove(storage + this_byte_offset + offset * byte_size, storage + that_byte_offset, input_length * byte_size);
                }
                else
                {
                    unsigned char *that_storage = array_object->array_buffer->GetStorage();
                    op_memcpy(storage + this_byte_offset + offset * byte_size, that_storage + that_byte_offset, input_length * byte_size);
                }
                return TRUE;
            }
            else
            {
                BOOL forward = TRUE;

                if (this_object->array_buffer == array_object->array_buffer)
                {
                    /* Sharing the same underlying buffer, check if the views overlap. */
                    unsigned char *this_array_start = storage + this_byte_offset + offset;
                    unsigned char *this_array_end = this_array_start + input_length * byte_size;
                    unsigned char *that_array_start = static_cast<unsigned char *>(array_object->GetStorage());
                    unsigned char *that_array_end = that_array_start + input_length * ES_TypedArray::GetElementSizeInBytes(that_kind);

                    /* Note: 'that_array' will never be larger than 'this_array'. */

                    BOOL end_overlap = this_array_end >= that_array_start && this_array_end <= that_array_end;
                    BOOL start_overlap = this_array_start >= that_array_start && this_array_start <= that_array_end;

                    /* Copying direction: must do it backwards if the start of the target will be
                       visited later when copying from source in a forward direction. If the
                       array buffer views match up, no need (end_overlap == start_overlap == TRUE.) */
                    if (start_overlap)
                        forward = end_overlap;

                    /* If the overlap of the portion copied from isn't sufficiently close, it's
                       safe to sequentially copy. Safe == the first write wouldn't be observable
                       by any of the reads from the source. */
                    if (end_overlap && (that_array_end - this_array_end) > 8)
                        end_overlap = FALSE;

                    if (start_overlap && (this_array_start - that_array_start) > 8)
                        start_overlap = FALSE;

                    if (start_overlap || end_overlap)
                    {
                        /* Avoid overlap by keeping a circular buffer. The maximum number
                           of elements we need to delay writing is when a Float64 array
                           is set() using a Int8/Uint8 view of the same array buffer. */
                        ES_Value_Internal values[8];

                        unsigned j = 0;
                        for (unsigned i = 0; i < input_length; i++)
                        {
                            if (i >= ARRAY_SIZE(values))
                            {
                                unsigned index = offset + (forward ? j : (input_length - 1 - j));
                                ES_Indexed_Properties::PutNoLockL(context, this_object, index, NULL, values[j % ARRAY_SIZE(values)], ES_Value_Internal());
                                j++;
                            }
                            values[i % ARRAY_SIZE(values)] = *ES_Indexed_Properties::GetStorage(array_object->GetIndexedProperties(), forward ? i : (input_length - 1 - i));
                        }
                        for (; j < input_length; j++)
                        {
                            unsigned index = offset + (forward ? j : (input_length - 1 - j));
                            ES_Indexed_Properties::PutNoLockL(context, this_object, index, NULL, values[j % ARRAY_SIZE(values)], ES_Value_Internal());
                        }
                        return TRUE;
                    }
                }
                for (unsigned i = 0; i < input_length; i++)
                {
                    unsigned index = forward ? i : (input_length - 1 - i);
                    ES_Value_Internal *value = ES_Indexed_Properties::GetStorage(array_object->GetIndexedProperties(), index);
                    ES_Indexed_Properties::PutNoLockL(context, this_object, offset + index, NULL, *value, ES_Value_Internal());
                }
            }
        }
    }
    else
    {
        context->ThrowTypeError("Expected array as first argument");
        return FALSE;
    }

    return TRUE;
}

/* static */ BOOL
ES_TypedArrayBuiltins::subarray(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_TYPEARRAY();
    ES_CHECK_ARITY(1);
    if (!argv[0].ToNumber(context))
        return FALSE;

    int begin = argv[0].GetNumAsInt32();

    unsigned array_length;
    if (!GET_OK(this_object->GetLength(context, array_length, TRUE)))
        return FALSE;

    int end;
    if (argc > 1)
        if (argv[1].ToNumber(context))
            end = argv[1].GetNumAsInt32();
        else
            return FALSE;
    else
        end = static_cast<int>(array_length);

    ClampRange(static_cast<int>(array_length), begin, end);

    ES_Value_Internal array_argv[3];
    array_argv[0] = this_object->array_buffer;
    array_argv[1].SetNumber(this_object->GetOffset() + static_cast<unsigned>(begin) * ES_TypedArray::GetElementSizeInBytes(this_object->GetKind()));
    array_argv[2].SetInt32(end - begin);
    if (ES_TypedArray *subarray_object = ES_TypedArray::Make(context, this_object->GetKind(), 3, array_argv))
    {
        return_value->SetObject(subarray_object);
        return TRUE;
    }
    else
        return FALSE;
}

/* static */ BOOL
ES_DataViewBuiltins::constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_CHECK_ARITY(1);
    if (!argv[0].IsObject())
        return FALSE;

    ES_Object *array_buffer = argv[0].GetObject(context);

    if (!array_buffer->IsArrayBufferObject())
    {
        context->ThrowTypeError("Expected ArrayBuffer as first argument");
        return FALSE;
    }

    unsigned byte_offset = 0;
    unsigned byte_length = 0;

    if (argc > 1)
        if (!argv[1].ToNumber(context))
            return FALSE;
        else
            byte_offset = argv[1].GetNumAsUInt32();

    unsigned length;
    if (!GET_OK(array_buffer->GetLength(context, length, TRUE)))
        return FALSE;

    if (byte_offset > length)
    {
        context->ThrowRangeError("Offset extends past end of ArrayBuffer");
        return FALSE;
    }

    if (argc > 2)
    {
        if (!argv[2].ToNumber(context))
            return FALSE;
        else
            byte_length = argv[2].GetNumAsUInt32();
    }
    else
        byte_length = length - byte_offset;

    if (byte_length > length || byte_offset + byte_length < byte_offset || byte_offset + byte_length > length)
    {
        context->ThrowTypeError("View length extends past end of ArrayBuffer");
        return FALSE;
    }

    ES_Value_Internal array_argv[3];
    array_argv[0] = argv[0];
    array_argv[1].SetNumber(byte_offset);
    array_argv[2].SetNumber(byte_length);
    if (ES_TypedArray *dataview_object = ES_TypedArray::Make(context, ES_TypedArray::DataViewArray, 3, array_argv))
    {
        return_value->SetObject(dataview_object);
        return TRUE;
    }
    else
        return FALSE;
}

/* static */ BOOL
ES_DataViewBuiltins::constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    context->ThrowTypeError("DataView: cannot be called as a function");
    return FALSE;
}

static ES_ArrayBuffer::ByteOrder
WithByteOrder(int argc, ES_Value_Internal *argv)
{
    ES_ArrayBuffer::ByteOrder byte_order = ES_ArrayBuffer::BigEndian;
    if (argc > 1)
    {
        argv[1].ToBoolean();
        if (argv[1].GetBoolean())
            byte_order = ES_ArrayBuffer::LittleEndian;
    }
    return byte_order;
}

/* static */ BOOL
ES_DataViewBuiltins::getInt8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(1);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset, length);
    return GET_OK(ES_DataView::GetIndex(context, this_object, offset, *return_value, ES_TypedArray::Int8Array, ES_ArrayBuffer::BigEndian));
}

/* static */ BOOL
ES_DataViewBuiltins::getUint8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(1);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset, length);
    return GET_OK(ES_DataView::GetIndex(context, this_object, offset, *return_value, ES_TypedArray::Uint8Array, ES_ArrayBuffer::BigEndian));
}

/* static */ BOOL
ES_DataViewBuiltins::getInt16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(1);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 1, length);
    return GET_OK(ES_DataView::GetIndex(context, this_object, offset, *return_value, ES_TypedArray::Int16Array, WithByteOrder(argc, argv)));
}

/* static */ BOOL
ES_DataViewBuiltins::getUint16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(1);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 1, length);
    return GET_OK(ES_DataView::GetIndex(context, this_object, offset, *return_value, ES_TypedArray::Uint16Array, WithByteOrder(argc, argv)));
}

/* static */ BOOL
ES_DataViewBuiltins::getInt32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(1);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 3, length);
    return GET_OK(ES_DataView::GetIndex(context, this_object, offset, *return_value, ES_TypedArray::Int32Array, WithByteOrder(argc, argv)));
}

/* static */ BOOL
ES_DataViewBuiltins::getUint32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(1);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 3, length);
    return GET_OK(ES_DataView::GetIndex(context, this_object, offset, *return_value, ES_TypedArray::Uint32Array, WithByteOrder(argc, argv)));
}

/* static */ BOOL
ES_DataViewBuiltins::getFloat32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(1);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 3, length);
    return GET_OK(ES_DataView::GetIndex(context, this_object, offset, *return_value, ES_TypedArray::Float32Array, WithByteOrder(argc, argv)));
}

/* static */ BOOL
ES_DataViewBuiltins::getFloat64(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(1);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 7, length);
    return GET_OK(ES_DataView::GetIndex(context, this_object, offset, *return_value, ES_TypedArray::Float64Array, WithByteOrder(argc, argv)));
}

/* static */ BOOL
ES_DataViewBuiltins::setInt8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(2);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset, length);
    return_value->SetUndefined();
    return PUT_OK(ES_DataView::PutIndex(context, this_object, offset, argv[1], ES_TypedArray::Int8Array, ES_ArrayBuffer::BigEndian));
}

/* static */ BOOL
ES_DataViewBuiltins::setUint8(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(2);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset, length);
    return_value->SetUndefined();
    return PUT_OK(ES_DataView::PutIndex(context, this_object, offset, argv[1], ES_TypedArray::Uint8Array, ES_ArrayBuffer::BigEndian));
}

/* static */ BOOL
ES_DataViewBuiltins::setInt16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(2);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 1, length);

    return_value->SetUndefined();
    return PUT_OK(ES_DataView::PutIndex(context, this_object, offset, argv[1], ES_TypedArray::Int16Array, WithByteOrder(argc - 1, argv + 1)));
}

/* static */ BOOL
ES_DataViewBuiltins::setUint16(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(2);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 1, length);

    return_value->SetUndefined();
    return PUT_OK(ES_DataView::PutIndex(context, this_object, offset, argv[1], ES_TypedArray::Uint16Array, WithByteOrder(argc - 1, argv + 1)));
}

/* static */ BOOL
ES_DataViewBuiltins::setInt32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(2);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 3, length);

    return_value->SetUndefined();
    return PUT_OK(ES_DataView::PutIndex(context, this_object, offset, argv[1], ES_TypedArray::Int32Array, WithByteOrder(argc - 1, argv + 1)));
}

/* static */ BOOL
ES_DataViewBuiltins::setUint32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(2);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 3, length);

    return_value->SetUndefined();
    return PUT_OK(ES_DataView::PutIndex(context, this_object, offset, argv[1], ES_TypedArray::Uint32Array, WithByteOrder(argc - 1, argv + 1)));
}

/* static */ BOOL
ES_DataViewBuiltins::setFloat32(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(2);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 3, length);

    return_value->SetUndefined();
    return PUT_OK(ES_DataView::PutIndex(context, this_object, offset, argv[1], ES_TypedArray::Float32Array, WithByteOrder(argc - 1, argv + 1)));
}

/* static */ BOOL
ES_DataViewBuiltins::setFloat64(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_DATAVIEW();
    ES_CHECK_ARITY(2);
    ES_GET_DATAVIEW_OFFSET(offset);
    ES_GET_DATAVIEW_LENGTH(length);
    ES_CHECK_DATAVIEW_OFFSET(offset + 7, length);

    return_value->SetUndefined();
    return PUT_OK(ES_DataView::PutIndex(context, this_object, offset, argv[1], ES_TypedArray::Float64Array, WithByteOrder(argc - 1, argv + 1)));
}

/* static */ void
ES_DataViewBuiltins::PopulateGlobalObject(ES_Context *context, ES_Global_Object *global_object)
{
    JString **idents = context->rt_data->idents;
    ES_Class_Singleton *dataview_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "DataView", idents[ESID_DataView], ES_DataViewBuiltinsCount);
    PopulatePrototypeClass(context, dataview_class);

    ES_Object *prototype = ES_Object::Make(context, dataview_class);
    PopulatePrototype(context, global_object, prototype);

    ES_Object *constructor = ES_Function::Make(context, global_object->GetBuiltInConstructorClass(), global_object, 1, constructor_call, constructor_construct, ESID_DataView, NULL, prototype);
    prototype->InitPropertyL(context, idents[ESID_constructor], constructor, DE);
    global_object->InitPropertyL(context, idents[ESID_DataView], constructor, DE);

    ES_Class *sub_object_class = ES_Class::MakeRoot(context, prototype, "DataView", idents[ESID_DataView], TRUE);
    prototype->SetSubObjectClass(context, sub_object_class);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_length], ES_Property_Info(RO|DD), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_buffer], ES_Property_Info(RO|DD), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_byteLength], ES_Property_Info(RO|DD), ES_STORAGE_INT32);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_byteOffset], ES_Property_Info(RO|DD), ES_STORAGE_INT32);
    global_object->classes[ES_Global_Object::CI_DATAVIEW] = sub_object_class;
}

/* static */ void
ES_DataViewBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ASSERT_CLASS_SIZE(ES_DataViewBuiltinsCount);
    ES_Value_Internal undefined;

    APPEND_PROPERTY(ES_DataViewBuiltins, length, 0);
    APPEND_PROPERTY(ES_DataViewBuiltins, buffer, undefined);
    APPEND_PROPERTY(ES_DataViewBuiltins, byteOffset, 0);
    APPEND_PROPERTY(ES_DataViewBuiltins, byteLength, 0);

    APPEND_PROPERTY(ES_DataViewBuiltins, constructor, undefined);
    APPEND_PROPERTY(ES_DataViewBuiltins, getInt8, MAKE_BUILTIN(1, getInt8));
    APPEND_PROPERTY(ES_DataViewBuiltins, getUint8, MAKE_BUILTIN(1, getUint8));
    APPEND_PROPERTY(ES_DataViewBuiltins, getInt16, MAKE_BUILTIN(1, getInt16));
    APPEND_PROPERTY(ES_DataViewBuiltins, getUint16, MAKE_BUILTIN(1, getUint16));
    APPEND_PROPERTY(ES_DataViewBuiltins, getInt32, MAKE_BUILTIN(1, getInt32));
    APPEND_PROPERTY(ES_DataViewBuiltins, getUint32, MAKE_BUILTIN(1, getUint32));
    APPEND_PROPERTY(ES_DataViewBuiltins, getFloat32, MAKE_BUILTIN(1, getFloat32));
    APPEND_PROPERTY(ES_DataViewBuiltins, getFloat64, MAKE_BUILTIN(1, getFloat64));
    APPEND_PROPERTY(ES_DataViewBuiltins, setInt8, MAKE_BUILTIN(2, setInt8));
    APPEND_PROPERTY(ES_DataViewBuiltins, setUint8, MAKE_BUILTIN(2, setUint8));
    APPEND_PROPERTY(ES_DataViewBuiltins, setInt16, MAKE_BUILTIN(2, setInt16));
    APPEND_PROPERTY(ES_DataViewBuiltins, setUint16, MAKE_BUILTIN(2, setUint16));
    APPEND_PROPERTY(ES_DataViewBuiltins, setInt32, MAKE_BUILTIN(2, setInt32));
    APPEND_PROPERTY(ES_DataViewBuiltins, setUint32, MAKE_BUILTIN(2, setUint32));
    APPEND_PROPERTY(ES_DataViewBuiltins, setFloat32, MAKE_BUILTIN(2, setFloat32));
    APPEND_PROPERTY(ES_DataViewBuiltins, setFloat64, MAKE_BUILTIN(2, setFloat64));
}

/* static */ void
ES_DataViewBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_DataViewBuiltinsCount);

    JString **idents = context->rt_data->idents;

    DECLARE_PROPERTY(ES_DataViewBuiltins, length, RO | DD, ES_STORAGE_INT32);
    DECLARE_PROPERTY(ES_DataViewBuiltins, buffer, RO | DD, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_DataViewBuiltins, byteOffset, RO | DD, ES_STORAGE_INT32);
    DECLARE_PROPERTY(ES_DataViewBuiltins, byteLength, RO | DD, ES_STORAGE_INT32);

    DECLARE_PROPERTY(ES_DataViewBuiltins, constructor, DE, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_DataViewBuiltins, getInt8, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, getUint8, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, getInt16, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, getUint16, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, getInt32, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, getUint32, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, getFloat32, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, getFloat64, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, setInt8, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, setUint8, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, setInt16, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, setUint16, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, setInt32, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, setUint32, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, setFloat32, 0, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_DataViewBuiltins, setFloat64, 0, ES_STORAGE_OBJECT);
}

#undef ES_THIS_TYPEARRAY
#undef ES_THIS_DATAVIEW
#undef ES_CHECK_DATAVIEW_OFFSET
#undef ES_CHECK_ARGUMENTS
#undef ES_GET_DATAVIEW_OFFSET
#undef ES_GET_DATAVIEW_LENGTH
