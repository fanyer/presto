/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/object/es_typedarray.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_indexed.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/ecmascript/carakan/src/builtins/es_typedarray_builtins.h"

/* static */ ES_ArrayBuffer *
ES_ArrayBuffer::Make(ES_Context *context, ES_Global_Object *global_object, unsigned byte_length, unsigned char *bytes, BOOL initialize_to_zero)
{
    ES_ArrayBuffer *array_buffer;
    GC_ALLOCATE(context, array_buffer, ES_ArrayBuffer, (array_buffer, global_object->GetArrayBufferClass()));
    ES_CollectorLock gclock(context);
    array_buffer->AllocateProperties(context);

    /* Attempt to create the external buffer; return NULL if it fails, but throw a
       TypeError if in an execution context. */
    ES_Byte_Array_Indexed *byte_array = ES_Byte_Array_Indexed::Make(context, byte_length, bytes);
    if (byte_array == NULL)
    {
        if (context->GetExecutionContext())
            context->GetExecutionContext()->ThrowTypeError("Unable to create ArrayBuffer of the given length");
        return NULL;
    }

    if (initialize_to_zero)
        op_memset(byte_array->Storage(), 0, byte_length);

    array_buffer->SetIndexedProperties(byte_array);

    /* Initialise .length and .byteLength. */
    array_buffer->PutCachedAtIndex(ES_PropertyIndex(0), byte_length);
    array_buffer->PutCachedAtIndex(ES_PropertyIndex(1), byte_length);
    return array_buffer;
}

/* static */ ES_ArrayBuffer *
ES_ArrayBuffer::Make(ES_Execution_Context *context, ES_Global_Object *global_object, unsigned argc, ES_Value_Internal* argv)
{
    unsigned length = 0;
    if (argc > 0)
    {
        if (!argv[0].ToNumber(context))
            return NULL;

        if (!argv[0].IsUInt32())
        {
            context->ThrowRangeError("Length value out of range");
            return NULL;
        }

        length = argv[0].GetNumAsUInt32();
    }

    return Make(context, global_object, length);
}

/* static */ void
ES_ArrayBuffer::Transfer(ES_ArrayBuffer *source, ES_ArrayBuffer *target)
{
    ES_Byte_Array_Indexed *source_properties = static_cast<ES_Byte_Array_Indexed *>(source->GetIndexedProperties());
    ES_Byte_Array_Indexed *target_properties = static_cast<ES_Byte_Array_Indexed *>(target->GetIndexedProperties());

    /* Transfer the actual storage along with lengths. */
    target->PutCachedAtIndex(ES_PropertyIndex(0), source_properties->Capacity());
    target->PutCachedAtIndex(ES_PropertyIndex(1), source_properties->Capacity());
    ES_Byte_Array_Indexed::Transfer(source_properties, target_properties);

    /* Empty the source. */
    source->PutCachedAtIndex(ES_PropertyIndex(0), 0);
    source->PutCachedAtIndex(ES_PropertyIndex(1), 0);

    /* Neuter the different views. */
    source->typed_array_view->Neuter();
    source->typed_array_view = NULL;
}

void
ES_ArrayBuffer::RegisterTypedArray(ES_Context *context, ES_TypedArray *typed_array)
{
    typed_array->next = typed_array_view;
    if (typed_array_view)
        typed_array_view->prev = typed_array;
    typed_array_view = typed_array;
}

void
ES_ArrayBuffer::RemoveTypedArray(ES_TypedArray *typed_array)
{
    if (typed_array->next)
        typed_array->next->prev = typed_array->prev;
    if (typed_array->prev)
        typed_array->prev->next = typed_array->next;
    else
        typed_array_view = typed_array->next;
}

GetResult
ES_ArrayBuffer::GetIndex(ES_Execution_Context *context, unsigned index, ES_Value_Internal &value, ES_TypedArray::Kind kind, ByteOrder byte_order)
{
    /* Verify that underlying storage matches above length */
    unsigned value_size = ES_TypedArray::GetElementSizeInBytes(kind);
    ES_Byte_Array_Indexed *array_buffer_bytearray = static_cast<ES_Byte_Array_Indexed *>(GetIndexedProperties());
    if (index + (value_size - 1) >= array_buffer_bytearray->Capacity())
    {
        context->ThrowRangeError("Index out of range");
        return PROP_GET_FAILED;
    }

    unsigned char *bytearray = array_buffer_bytearray->Storage();
    switch (kind)
    {
    case ES_TypedArray::Int8Array:
        value.SetInt32((reinterpret_cast<signed char*>(bytearray))[index]);
        break;

    case ES_TypedArray::Uint8Array:
    case ES_TypedArray::Uint8ClampedArray:
        value.SetUInt32(bytearray[index]);
        break;

    case ES_TypedArray::Int16Array:
    case ES_TypedArray::Uint16Array:
        {
            unsigned char *values = &(bytearray[index]);
            unsigned byte1 = static_cast<unsigned>(values[0]);
            unsigned byte2 = static_cast<unsigned>(values[1]);

            unsigned short val;
            if (byte_order == BigEndian)
                val = (byte1 << 8) + byte2;
            else
                val = (byte2 << 8) + byte1;

            if (kind == ES_TypedArray::Int16Array)
                value.SetInt32(static_cast<signed short>(val));
            else
                value.SetUInt32(val);
        }
        break;

    case ES_TypedArray::Int32Array:
    case ES_TypedArray::Uint32Array:
    case ES_TypedArray::Float32Array:
        {
            unsigned char *values = &(bytearray[index]);
            unsigned byte1 = static_cast<unsigned>(values[0]);
            unsigned byte2 = static_cast<unsigned>(values[1]);
            unsigned byte3 = static_cast<unsigned>(values[2]);
            unsigned byte4 = static_cast<unsigned>(values[3]);

            union { float f; unsigned u; } val;
            if (byte_order == BigEndian)
                val.u = (byte1 << 24) + (byte2 << 16) + (byte3 << 8) + byte4;
            else
                val.u = (byte4 << 24) + (byte3 << 16) + (byte2 << 8) + byte1;

            switch (kind)
            {
            case ES_TypedArray::Int32Array:
                value.SetInt32(static_cast<int>(val.u));
                break;
            case ES_TypedArray::Uint32Array:
                value.SetUInt32(val.u);
                break;
            case ES_TypedArray::Float32Array:
                value.SetNumber(val.f);
                break;
            }
        }
        break;

    case ES_TypedArray::Float64Array:
        {
            unsigned char *values = &(bytearray[index]);
            union { double d; unsigned char u[8]; } val; // ARRAY OK 2011-07-26 sof

#ifdef OPERA_BIG_ENDIAN
            BOOL need_swapping = byte_order == LittleEndian;
#else
            BOOL need_swapping = byte_order == BigEndian;
#endif // OPERA_BIG_ENDIAN

            if (need_swapping)
            {
                val.u[7] = values[0];
                val.u[6] = values[1];
                val.u[5] = values[2];
                val.u[4] = values[3];
                val.u[3] = values[4];
                val.u[2] = values[5];
                val.u[1] = values[6];
                val.u[0] = values[7];
            }
            else
                op_memcpy(val.u, values, 8);

            value.SetNumber(val.d);
            break;
        }
        break;

    default:
        OP_ASSERT(!"Indexing array buffer at illegal (and unknown) type");
        return PROP_GET_FAILED;
    }

    return PROP_GET_OK;
}

PutResult
ES_ArrayBuffer::PutIndex(ES_Execution_Context *context, unsigned byte_index, const ES_Value_Internal &value, ES_TypedArray::Kind kind, ByteOrder byte_order)
{
    ES_Byte_Array_Indexed *array_buffer_bytearray = static_cast<ES_Byte_Array_Indexed *>(GetIndexedProperties());
    if (byte_index + (ES_TypedArray::GetElementSizeInBytes(kind) - 1) >= array_buffer_bytearray->Capacity())
    {
        context->ThrowRangeError("Index argument out of range");
        return PROP_PUT_FAILED;
    }
    ES_Value_Internal temporary = value;
    if (!temporary.ToNumber(context))
        return PROP_PUT_FAILED;

    /* DataView uses this PutIndex() method (and it only); a clamped setUint8()
       is currently not supported by DataView. */
    OP_ASSERT(kind != ES_TypedArray::Uint8ClampedArray);

    unsigned char *bytearray = array_buffer_bytearray->Storage();
    unsigned char *values = &bytearray[byte_index];
    switch (kind)
    {
    case ES_TypedArray::Int8Array:
    {
        *reinterpret_cast<signed char *>(values) = static_cast<signed char>(temporary.GetNumAsInt32());
        break;
    }
    case ES_TypedArray::Uint8Array:
    {
        *values = static_cast<unsigned char>(temporary.GetNumAsUInt32());
        break;
    }
    case ES_TypedArray::Int16Array:
    case ES_TypedArray::Uint16Array:
    {
        unsigned val = temporary.GetNumAsUInt32();
        switch (byte_order)
        {
        case BigEndian:
            values[0] = static_cast<unsigned char>((val >> 8) & 0xff);
            values[1] = static_cast<unsigned char>(val & 0xff);
            break;
        default:
            OP_ASSERT(0);
        case LittleEndian:
            values[0] = static_cast<unsigned char>(val & 0xff);
            values[1] = static_cast<unsigned char>((val >> 8) & 0xff);
            break;
        }
        break;
    }
    case ES_TypedArray::Int32Array:
    case ES_TypedArray::Uint32Array:
    {
        unsigned val = temporary.GetNumAsUInt32();
        switch (byte_order)
        {
        case BigEndian:
            values[0] = static_cast<unsigned char>((val >> 24) & 0xff);
            values[1] = static_cast<unsigned char>((val >> 16) & 0xff);
            values[2] = static_cast<unsigned char>((val >>  8) & 0xff);
            values[3] = static_cast<unsigned char>(val & 0xff);
            break;
        default:
            OP_ASSERT(0);
        case LittleEndian:
            values[0] = static_cast<unsigned char>(val & 0xff);
            values[1] = static_cast<unsigned char>((val >>  8) & 0xff);
            values[2] = static_cast<unsigned char>((val >> 16) & 0xff);
            values[3] = static_cast<unsigned char>((val >> 24) & 0xff);
            break;
        }
        break;
    }

    case ES_TypedArray::Float32Array:
    {
        union { float f; unsigned char u[4]; } val; // ARRAY OK 2011-07-26 sof
        val.f = static_cast<float>(temporary.GetNumAsDouble());

#ifdef OPERA_BIG_ENDIAN
        BOOL need_swapping = byte_order == LittleEndian;
#else
        BOOL need_swapping = byte_order == BigEndian;
#endif // OPERA_BIG_ENDIAN
        if (need_swapping)
        {
            values[0] = val.u[3];
            values[1] = val.u[2];
            values[2] = val.u[1];
            values[3] = val.u[0];
        }
        else
            op_memcpy(values, val.u, 4);

        break;
    }

    case ES_TypedArray::Float64Array:
    {
        union { double d; unsigned char u[8]; } val; // ARRAY OK 2011-07-26 sof
        val.d = temporary.GetNumAsDouble();

#ifdef OPERA_BIG_ENDIAN
        BOOL need_swapping = byte_order == LittleEndian;
#else
        BOOL need_swapping = byte_order == BigEndian;
#endif // OPERA_BIG_ENDIAN
        if (need_swapping)
        {
            values[0] = val.u[7];
            values[1] = val.u[6];
            values[2] = val.u[5];
            values[3] = val.u[4];
            values[4] = val.u[3];
            values[5] = val.u[2];
            values[6] = val.u[1];
            values[7] = val.u[0];
        }
        else
            op_memcpy(values, val.u, 8);

        break;
    }

    default:
        OP_ASSERT(!"Unrecognised array type; cannot happen.");
        break;
    }

    return PROP_PUT_OK;
}

static ES_Type_Array_Indexed::TypeArrayKind
ToTypeArrayKind(ES_TypedArray::Kind k)
{
    /* These two enums could certainly be joined up, but
       not keen on using a 'low level' enum + DataView
       is the odd one out. */
    switch (k)
    {
    case ES_TypedArray::Int8Array:
        return ES_Type_Array_Indexed::Int8Array;
    case ES_TypedArray::Int16Array:
        return ES_Type_Array_Indexed::Int16Array;
    case ES_TypedArray::Int32Array:
        return ES_Type_Array_Indexed::Int32Array;
    case ES_TypedArray::Uint8Array:
        return ES_Type_Array_Indexed::Uint8Array;
    case ES_TypedArray::Uint8ClampedArray:
        return ES_Type_Array_Indexed::Uint8ClampedArray;
    case ES_TypedArray::Uint16Array:
        return ES_Type_Array_Indexed::Uint16Array;
    case ES_TypedArray::Uint32Array:
        return ES_Type_Array_Indexed::Uint32Array;
    case ES_TypedArray::Float32Array:
        return ES_Type_Array_Indexed::Float32Array;
    case ES_TypedArray::Float64Array:
        return ES_Type_Array_Indexed::Float64Array;
    default:
        OP_ASSERT(!"Type array kind does not have a corresponding indexed type; cannot happen.");
        return ES_Type_Array_Indexed::Uint8Array;
    }
}

/*== TypedArrays */

/* static */ void
ES_TypedArray::Initialize(ES_TypedArray *typed_array, ES_Class *klass, Kind kind, ES_Object *buffer)
{
    ES_Object::Initialize(typed_array, klass);
    typed_array->ChangeGCTag(GCTAG_ES_Object_TypedArray);
    typed_array->kind = kind;
    typed_array->array_buffer = static_cast<ES_ArrayBuffer *>(buffer);
    typed_array->next = NULL;
    typed_array->prev = NULL;
    typed_array->SetNeedDestroy();
}

/* static */ ES_TypedArray *
ES_TypedArray::Make(ES_Execution_Context *context, Kind kind, unsigned argc, ES_Value_Internal* argv)
{
    unsigned view_byte_offset = 0;
    unsigned view_length = 0;
    unsigned view_byte_length = 0;
    unsigned byte_size = GetElementSizeInBytes(kind);

    ES_ArrayBuffer *array_buffer = NULL;
    ES_Object *array_initialiser = NULL;
    if (argc <= 1)
    {
        if (argc == 0 || argv[0].IsNumber())
        {
            if (argc > 0 && !argv[0].IsUInt32())
            {
                context->ThrowRangeError("Length outside of supported range");
                return NULL;
            }
            else
                view_length = argc == 0 ? 0 : argv[0].GetNumAsUInt32();

            view_byte_length = view_length * byte_size;
            if (view_byte_length / byte_size != view_length)
            {
                context->ThrowRangeError("Length outside of supported range");
                return NULL;
            }

            if ((array_buffer = ES_ArrayBuffer::Make(context, context->GetGlobalObject(), view_byte_length)) == NULL)
                return NULL;
        }
        else if (argv[0].IsObject())
        {
            ES_Object *object = argv[0].GetObject(context);

            if (object->GCTag() == GCTAG_ES_Object_ArrayBuffer)
            {
                array_buffer = static_cast<ES_ArrayBuffer *>(object);

                ES_Value_Internal length_value;
                array_buffer->GetCachedAtIndex(ES_PropertyIndex(0), length_value);
                OP_ASSERT(length_value.IsNumber());
                view_byte_length = length_value.GetNumAsUInt32();
                view_length = view_byte_length / byte_size;
            }
            else if (kind == Uint8ClampedArray && object->GCTag() == GCTAG_ES_Object_TypedArray)
            {
                ES_TypedArray *typed_array = static_cast<ES_TypedArray *>(object);
                if (typed_array->GetKind() == Uint8Array || typed_array->GetKind() == Uint8ClampedArray)
                {
                    array_buffer = typed_array->GetArrayBuffer();
                    ES_Value_Internal length_value;
                    array_buffer->GetCachedAtIndex(ES_PropertyIndex(0), length_value);
                    OP_ASSERT(length_value.IsNumber());
                    view_byte_length = length_value.GetNumAsUInt32();
                    view_length = view_byte_length / byte_size;
                }
                else
                {
                    context->ThrowTypeError("Expected Uint8Array or Uint8ClampedArray argument");
                    return NULL;
                }
            }
            else
            {
                GetResult result;
                if (!GET_OK(result = object->GetLength(context, view_length)))
                {
                    if (result == PROP_GET_NOT_FOUND)
                        context->ThrowTypeError("Expected an array argument");
                    return NULL;
                }

                view_byte_length = view_length * byte_size;
                if (view_byte_length / byte_size != view_length)
                {
                    context->ThrowRangeError("Length outside of supported range");
                    return NULL;
                }

                if ((array_buffer = ES_ArrayBuffer::Make(context, context->GetGlobalObject(), view_byte_length)) == NULL)
                    /* ES_ArrayBuffer::Make() throws an exception upon failure. */
                    return NULL;

                if (view_length > 0)
                    array_initialiser = object;
            }
        }
        else
        {
            context->ThrowTypeError("Expected array, typed array or array buffer argument");
            return NULL;
        }
    }
    else
    {
        ES_Object *object;
        if (argv[0].IsObject() && (object = argv[0].GetObject(context))->GCTag() == GCTAG_ES_Object_ArrayBuffer)
        {
            array_buffer = static_cast<ES_ArrayBuffer *>(object);
            array_buffer->GetLength(context, view_byte_length);

            if (!argv[1].ToNumber(context))
                return NULL;

            if (!argv[1].IsUInt32())
            {
                context->ThrowRangeError("Offset outside of supported range");
                return NULL;
            }
            else
                view_byte_offset = argv[1].GetNumAsUInt32();

            if (view_byte_offset > view_byte_length)
            {
                context->ThrowRangeError("Offset outside of ArrayBuffer range");
                return NULL;
            }

            if (view_byte_offset % byte_size != 0)
            {
                context->ThrowTypeError("Byte offset not aligned with element size");
                return NULL;
            }

            if (argc >= 3)
            {
                if (!argv[2].ToNumber(context))
                    return NULL;

                if (!argv[2].IsUInt32())
                {
                    context->ThrowRangeError("Length outside of supported range");
                    return NULL;
                }

                unsigned requested_length = argv[2].GetNumAsUInt32();

                if ((requested_length * byte_size) / byte_size != requested_length)
                {
                    context->ThrowRangeError("Length outside of supported range");
                    return NULL;
                }

                requested_length *= byte_size;

                if (requested_length > view_byte_length ||
                    requested_length + view_byte_offset < requested_length ||
                    requested_length + view_byte_offset > view_byte_length)
                {
                    context->ThrowRangeError("Offset and length outside of ArrayBuffer's supported range");
                    return NULL;
                }

                if (requested_length % byte_size != 0)
                {
                    context->ThrowTypeError("Length not aligned with element size");
                    return NULL;
                }
                view_byte_length = requested_length;
                view_length = requested_length / byte_size;
            }
            else
            {
                if ((view_byte_length - view_byte_offset) % byte_size != 0)
                {
                    context->ThrowTypeError("Length not aligned with element size");
                    return NULL;
                }
                view_byte_length -= view_byte_offset;
                view_length = view_byte_length / byte_size;
            }
        }
        else
        {
            context->ThrowTypeError("Expected an ArrayBuffer as first argument");
            return NULL;
        }
    }

    ES_CollectorLock gclock(context);
    ES_TypedArray *typed_array = NULL;
    ES_Class *klass = kind == DataViewArray ? context->GetGlobalObject()->GetDataViewClass() : context->GetGlobalObject()->GetTypedArrayClass(kind);
    GC_ALLOCATE(context, typed_array, ES_TypedArray, (typed_array, klass, kind, array_buffer));

    if (array_buffer)
        array_buffer->RegisterTypedArray(context, typed_array);

    typed_array->AllocateProperties(context);
    typed_array->PutCachedAtIndex(ES_PropertyIndex(0), view_length);

    /* A TypedArray is indexable, excepts for DataViews, create the
       indexed properties representation of it. */
    if (kind != DataViewArray)
    {
        ES_Type_Array_Indexed *indexed = ES_Type_Array_Indexed::Make(context, view_byte_offset, view_length, ToTypeArrayKind(kind), static_cast<ES_Byte_Array_Indexed *>(array_buffer->GetIndexedProperties()));
        typed_array->SetIndexedProperties(indexed);
    }

    /* Initialise .buffer, .byteLength and .byteOffset. */
    typed_array->PutCachedAtIndex(ES_PropertyIndex(1), array_buffer);
    typed_array->PutCachedAtIndex(ES_PropertyIndex(2), view_byte_length);
    typed_array->PutCachedAtIndex(ES_PropertyIndex(3), view_byte_offset);

    if (array_initialiser)
    {
        ES_StackPointerAnchor typed_array_anchor(context, array_buffer);

        ES_Value_Internal value;
        GetResult result;
        for (unsigned i = 0; i < view_length; i++)
        {
            if (!GET_OK(result = array_initialiser->GetL(context, i, value)))
                if (result == PROP_GET_NOT_FOUND)
                    value.SetUndefined();
                else
                    return NULL;

            if (!PUT_OK(ES_Indexed_Properties::PutNoLockL(context, typed_array, i, NULL, value, ES_Value_Internal())))
                return NULL;
        }
    }
    return typed_array;
}

/* static */ ES_TypedArray *
ES_TypedArray::Make(ES_Context *context, ES_Global_Object *global_object, Kind kind, ES_ArrayBuffer* array_buffer, unsigned view_byte_offset, unsigned view_byte_length)
{
    unsigned byte_size = GetElementSizeInBytes(kind);

    ES_Value_Internal length_value;
    array_buffer->GetCachedAtIndex(ES_PropertyIndex(0), length_value);
    OP_ASSERT(length_value.IsNumber());
    if (view_byte_length == 0)
    {
        view_byte_length = length_value.GetNumAsUInt32();
        OP_ASSERT(view_byte_offset <= view_byte_length);
        view_byte_length -= view_byte_offset;
    }

    unsigned view_length = view_byte_length / byte_size;

    ES_CollectorLock gclock(context);
    ES_TypedArray *typed_array = NULL;
    ES_Class *klass = kind == DataViewArray ? global_object->GetDataViewClass() : global_object->GetTypedArrayClass(kind);
    GC_ALLOCATE(context, typed_array, ES_TypedArray, (typed_array, klass, kind, array_buffer));
    array_buffer->RegisterTypedArray(context, typed_array);

    typed_array->AllocateProperties(context);
    typed_array->PutCachedAtIndex(ES_PropertyIndex(0), view_length);

    /* A TypedArray is indexable, excepts for DataViews, create the
       indexed properties representation of it. */
    if (kind != DataViewArray)
    {
        ES_Type_Array_Indexed *indexed = ES_Type_Array_Indexed::Make(context, view_byte_offset, view_length, ToTypeArrayKind(kind), static_cast<ES_Byte_Array_Indexed *>(array_buffer->GetIndexedProperties()));
        typed_array->SetIndexedProperties(indexed);
    }

    /* Initialise .buffer, .byteLength and .byteOffset. */
    typed_array->PutCachedAtIndex(ES_PropertyIndex(1), array_buffer);
    typed_array->PutCachedAtIndex(ES_PropertyIndex(2), view_byte_length);
    typed_array->PutCachedAtIndex(ES_PropertyIndex(3), view_byte_offset);

    return typed_array;
}

/* static */ unsigned
ES_TypedArray::GetElementSizeInBytes(Kind k)
{
    switch (k)
    {
    case Int8Array:
    case Uint8Array:
    case Uint8ClampedArray:
    case DataViewArray:
        return 1;

    case Int16Array:
    case Uint16Array:
        return 2;

    case Int32Array:
    case Uint32Array:
    case Float32Array:
        return 4;

    case Float64Array:
        return 8;
    default:
        OP_ASSERT(!"Not matching all enum tags; cannot happen.");
        return 0;
    }
}

/* static */ const char *
ES_TypedArray::GetKindClassName(Kind k)
{
    switch (k)
    {
    case Int8Array:
        return "Int8Array";
    case Uint8Array:
        return "Uint8Array";
    case Uint8ClampedArray:
        return "Uint8ClampedArray";
    case DataViewArray:
        return "DataView";

    case Int16Array:
        return "Int16Array";
    case Uint16Array:
        return "Uint16Array";

    case Int32Array:
        return "Int32Array";
    case Uint32Array:
        return "Uint32Array";
    case Float32Array:
        return "Float32Array";

    case Float64Array:
        return "Float64Array";
    default:
        OP_ASSERT(!"Not matching all enum tags; cannot happen.");
        return "";
    }
}

void *
ES_TypedArray::GetStorage()
{
    ES_Value_Internal view_byte_offset;
    GetCachedAtIndex(ES_PropertyIndex(3), view_byte_offset);
    OP_ASSERT(view_byte_offset.IsUInt32());
    return array_buffer->GetStorage() + view_byte_offset.GetNumAsUInt32();
}

unsigned
ES_TypedArray::GetSize()
{
    ES_Value_Internal view_byte_length;
    GetCachedAtIndex(ES_PropertyIndex(2), view_byte_length);
    OP_ASSERT(view_byte_length.IsUInt32());
    return view_byte_length.GetNumAsUInt32();
}

unsigned
ES_TypedArray::GetOffset()
{
    ES_Value_Internal view_byte_offset;
    GetCachedAtIndex(ES_PropertyIndex(3), view_byte_offset);
    OP_ASSERT(view_byte_offset.IsUInt32());
    return view_byte_offset.GetNumAsUInt32();
}

void
ES_TypedArray::Neuter()
{
    ES_Value_Internal null_value;
    null_value.SetNull();

    ES_TypedArray *iter = this;
    while (iter)
    {
        iter->PutCachedAtIndex(ES_PropertyIndex(0), 0);

        iter->PutCachedAtIndex(ES_PropertyIndex(1), null_value);
        iter->PutCachedAtIndex(ES_PropertyIndex(2), 0);
        iter->PutCachedAtIndex(ES_PropertyIndex(3), 0);

        static_cast<ES_Type_Array_Indexed *>(iter->GetIndexedProperties())->Neuter();
        ES_TypedArray *next = iter->next;
        iter->next = NULL;
        iter->prev = NULL;
        iter = next;
    }
}

/* static */ void
ES_TypedArray::Destroy(ES_TypedArray *typed_array)
{
    if (typed_array->array_buffer && typed_array->array_buffer->GCTag() == GCTAG_ES_Object_ArrayBuffer)
        typed_array->array_buffer->RemoveTypedArray(typed_array);
}

/*== DataView */

/* static */ GetResult
ES_DataView::GetIndex(ES_Execution_Context *context, ES_Object *this_object, unsigned index, ES_Value_Internal &value, ES_TypedArray::Kind kind, ES_ArrayBuffer::ByteOrder byte_order)
{
    OP_ASSERT(this_object->IsTypedArrayObject());
    ES_TypedArray *typed_array = static_cast<ES_TypedArray *>(this_object);

    ES_Value_Internal length_value;
    typed_array->GetCachedAtIndex(ES_PropertyIndex(0), length_value);
    OP_ASSERT(length_value.IsNumber());
    unsigned length = length_value.GetNumAsUInt32();

    unsigned byte_size = ES_TypedArray::GetElementSizeInBytes(typed_array->GetKind());
    if (index * byte_size < index)
        if (typed_array->GetKind() == ES_TypedArray::DataViewArray)
        {
            context->ThrowRangeError("Index argument out of range");
            return PROP_GET_FAILED;
        }
        else
        {
            value.SetUndefined();
            return PROP_GET_OK;
        }

    if ((index * byte_size + (byte_size - 1)) >= length * byte_size)
        if (typed_array->GetKind() == ES_TypedArray::DataViewArray)
        {
            context->ThrowRangeError("Index argument out of range");
            return PROP_GET_FAILED;
        }
        else
        {
            value.SetUndefined();
            return PROP_GET_OK;
        }


    index = index * byte_size;
    typed_array->GetCachedAtIndex(ES_PropertyIndex(ES_TypedArrayBuiltins::ES_TypedArrayBuiltins_byteOffset), length_value);
    if (!length_value.ToNumber(context))
        return PROP_GET_FAILED;

    index += length_value.GetNumAsUInt32();
    return typed_array->GetArrayBuffer()->GetIndex(context, index, value, kind, byte_order);
}

/* static */ PutResult
ES_DataView::PutIndex(ES_Execution_Context *context, ES_Object *this_object, unsigned index, const ES_Value_Internal &value, ES_TypedArray::Kind kind, ES_ArrayBuffer::ByteOrder byte_order)
{
    OP_ASSERT(this_object->IsTypedArrayObject());
    ES_TypedArray *typed_array = static_cast<ES_TypedArray *>(this_object);

    ES_Value_Internal length_value;
    typed_array->GetCachedAtIndex(ES_PropertyIndex(0), length_value);
    OP_ASSERT(length_value.IsNumber());
    unsigned length = length_value.GetNumAsUInt32();

    unsigned byte_size = ES_TypedArray::GetElementSizeInBytes(typed_array->GetKind());
    if (index * byte_size < index || (index * byte_size + (byte_size - 1)) >= length * byte_size)
    {
        context->ThrowRangeError("Index argument out of range");
        return PROP_PUT_FAILED;
    }
    index = index * byte_size;

    typed_array->GetCachedAtIndex(ES_PropertyIndex(ES_TypedArrayBuiltins::ES_TypedArrayBuiltins_byteOffset), length_value);
    if (!length_value.ToNumber(context))
        return PROP_PUT_FAILED;

    index += length_value.GetNumAsUInt32();
    return typed_array->GetArrayBuffer()->PutIndex(context, index, value, kind, byte_order);
}
