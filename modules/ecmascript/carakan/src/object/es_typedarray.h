/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

/** Typed Arrays - as specified Khronos (WebGL being the main motivator);

    http://www.khronos.org/registry/typedarray/specs/latest/

    (a variant may end up as a EcmaScript TC39 specification at some point.)

    ArrayBuffer is the basic byte array view, with <Type>Array providing a uniformly typed
    view on top of it (instantiated at an integral or floating point type.) Or if a
    dynamic, heterogeneous view is required of the buffer, DataView is provided. It lets you
    index the array buffer at any of the above basic types. An ArrayBuffer can be shared
    by multiple DataViews and TypeArrays. */

#ifndef ES_TYPEDARRAY_H
#define ES_TYPEDARRAY_H

class ES_ArrayBuffer;
class ES_DataView;

class ES_TypedArray
    : public ES_Object
{
public:
    enum Kind
    {
        Int8Array = 0,
        Int16Array,
        Int32Array,
        Uint8Array,
        Uint8ClampedArray,
        Uint16Array,
        Uint32Array,
        Float32Array,
        Float64Array,

        DataViewArray,

        ES_TypedArrayCount
    };

    static ES_TypedArray *Make(ES_Execution_Context *context, Kind kind, unsigned argc, ES_Value_Internal* argv);
    static ES_TypedArray *Make(ES_Context *context, ES_Global_Object *global_object, Kind kind, ES_ArrayBuffer* array_buffer, unsigned view_byte_offset = 0, unsigned view_byte_length = 0);

    Kind GetKind() { return kind; }
    ES_ArrayBuffer *GetArrayBuffer() { return array_buffer; }

    static unsigned GetElementSizeInBytes(Kind k);
    static const char *GetKindClassName(Kind k);

    void *GetStorage();
    /**< Returns the storage (ES_ArrayBuffer::GetStorage() + offset) for this typed array. */

    unsigned GetSize();
    /**< Returns the size, in bytes, of the storage for this typed array. */

    unsigned GetOffset();
    /**< Returns the view byte offset for this typed array. */

    void Neuter();
    /**< Render this typed array unusable after transfer of its
         underlying ArrayBuffer. */

private:
    friend class ESMM;
    friend class ES_DataViewBuiltins;
    friend class ES_TypedArrayBuiltins;
    friend class ES_ArrayBuffer;

    static void Initialize(ES_TypedArray *typed_array, ES_Class *klass, Kind kind, ES_Object *buffer);

    static void Destroy(ES_TypedArray *typed_array);

    Kind kind;
    ES_ArrayBuffer *array_buffer;

    ES_TypedArray *next;
    ES_TypedArray *prev;
    /**< Typed arrays sharing the same ArrayBuffer are chained together. */
};

class ES_ArrayBuffer
    : public ES_Object
{
public:
    enum ByteOrder
    {
        LittleEndian = 0,
        BigEndian
    };

    static ES_ArrayBuffer *Make(ES_Execution_Context *context, ES_Global_Object *global_object, unsigned argc, ES_Value_Internal *argv);

    static ES_ArrayBuffer *Make(ES_Context *context, ES_Global_Object *global_object, unsigned byte_length, unsigned char *bytes = NULL, BOOL initialize_to_zero = TRUE);
    /**< Create a new ArrayBuffer in the given execution context, optionally
         initializing it to hold an external buffer.

         @param context The execution context.
         @param global_object The global object.
         @param byte_length The requested length of this buffer.
         @param bytes If non-NULL, the external buffer that holds the storage and
                initial contents for this ArrayBuffer. Must be least 'byte_length'
                bytes large. If the allocation of the ArrayBuffer object is successful,
                ownership of 'bytes' is transferred to the object and further buffer
                accesses should not be performed. The buffer must have been allocated
                using the system allocator (op_malloc() / op_realloc()),,
                and will be released using that allocator's destructor (op_free) upon
                destruction of the ES_ArrayBuffer object.
         @param initialize_to_zero If TRUE, zero initialize the ArrayBuffer's
                storage. Passing in FALSE is appropriate if the buffer is
                created from external storage ('bytes' is non-NULL) or the storage
                will be initialized just after creation.
         @return If successful, returns the ArrayBuffer instance. If allocating
                 the external buffer encounters OOM, a TypeError is thrown
                 and NULL is returned. Callers are obliged to NULL check
                 the return value. */

    static void Transfer(ES_ArrayBuffer *source, ES_ArrayBuffer *target);
    /**< Transfer an ArrayBuffer to another. Transferring entails re-assigning
         the underlying storage to the other, leaving the source as empty. */

    GetResult GetIndex(ES_Execution_Context *context, unsigned index, ES_Value_Internal &value, ES_TypedArray::Kind kind, ByteOrder byte_order);
    PutResult PutIndex(ES_Execution_Context *context, unsigned byte_index, const ES_Value_Internal &value, ES_TypedArray::Kind kind, ByteOrder byte_order);

    unsigned char *GetStorage() { return static_cast<ES_Byte_Array_Indexed *>(GetIndexedProperties())->Storage(); }
    unsigned GetSize() { return static_cast<ES_Byte_Array_Indexed *>(GetIndexedProperties())->Capacity(); }

    void RegisterTypedArray(ES_Context *context, ES_TypedArray *typed_array);
    /**< Register the given typed array as having a view of this ArrayBuffer.
         Needed for invalidation of the different views in case the ArrayBuffer
         is transferred (and, so-called, neutered in the process.) */

    void RemoveTypedArray(ES_TypedArray *typed_array);
    /**< Upon destruction of a typed array, diassociate from its ArrayBuffer. */

private:
    friend class ESMM;

    static void Initialize(ES_ArrayBuffer *array_buffer, ES_Class *klass)
    {
        ES_Object::Initialize(array_buffer, klass);
        array_buffer->ChangeGCTag(GCTAG_ES_Object_ArrayBuffer);
        array_buffer->typed_array_view = NULL;
    }

    ES_TypedArray *typed_array_view;
    /**< Head of list of dependent typed array views, iterated over
         when neutering the array buffer upon transfer.
         @see RegisterTypedArray(). */
};

/** ES_DataView is a utility class for the DataView builtins, allowing
    reads and writes with a given byte order. Typed Arrays do not use
    this, but operate on the indexed representation directly. */
class ES_DataView
{
public:
    static GetResult GetIndex(ES_Execution_Context *context, ES_Object *this_object, unsigned index, ES_Value_Internal &value, ES_TypedArray::Kind kind, ES_ArrayBuffer::ByteOrder byte_order);
    static PutResult PutIndex(ES_Execution_Context *context, ES_Object *this_object, unsigned index, const ES_Value_Internal &value, ES_TypedArray::Kind kind, ES_ArrayBuffer::ByteOrder byte_order);
};

#endif // ES_TYPEDARRAY_H
