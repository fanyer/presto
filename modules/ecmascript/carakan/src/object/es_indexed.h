/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_INDEXED_H
#define ES_INDEXED_H

#include "modules/ecmascript/carakan/src/kernel/es_boxed.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"

#ifdef _DEBUG
# define inline
#endif // _DEBUG

class ES_Compact_Indexed_Properties;
class ES_Sparse_Indexed_Properties;

class ES_Indexed_Properties : public ES_Boxed
{
public:
    /** Identifies the representation used.  Actually stored using pointer
        tagging of ES_Object::indexed_properties. */
    enum Type
    {
        TYPE_COMPACT = GCTAG_ES_Compact_Indexed_Properties,
        /**< Stored as an ES_Value_Internal[] where slots 0 through capacity-1
             are represented.  Slots may still be absent, in which case they are
             stored as ESTYPE_UNDEFINED with ES_ValueUnion::boolean saying
             whether the slot is actually present. */

        TYPE_SPARSE = GCTAG_ES_Sparse_Indexed_Properties,
        /**< Stored as a index->value lookup table.  (Probably sorted with
             binary searching, since that makes array primitives easier to
             implement.) */

        TYPE_BYTE_ARRAY = GCTAG_ES_Byte_Array_Indexed,
        /**< Property values stored as bytes, and capacity fixed at creation.
             Not kept on a GCed heap. */

        TYPE_TYPE_ARRAY = GCTAG_ES_Type_Array_Indexed,
        /**< Property values stored as bytes, accessed at a fixed type.
             Capacity also fixed at creation. */

        TYPE_DUMMY_LAST_UNUSED
    };

    enum TypeBits
    {
        TYPE_BITS_COMPACT    = 1
    ,   TYPE_BITS_SPARSE     = 2
    ,   TYPE_BITS_STRING     = 4
    ,   TYPE_BITS_BYTE_ARRAY = 8
    ,   TYPE_BITS_TYPE_ARRAY = 16
    ,   TYPE_BITS_TYPE_MASK  = 31
    };

    static Type GetType(ES_Indexed_Properties *properties) { return static_cast<Type>(properties->GCTag()); }

    static inline unsigned GetTypeBits(ES_Indexed_Properties *properties);
    /**< Return a bit pattern representation of the type of indexed property
         storage in the following format:

         bit  0 -  4: ES_Indexed_Properties::Type
         bit  5 - 13: 1 << ES_Type_Array_Indexed::TypeArrayKind
         bit 14: 1 if storage has been indexed using a double (CORE-45075.)
         bit 15: 0
         */

    static unsigned char ToTypeBits(unsigned char type_bits) { return (type_bits & TYPE_BITS_TYPE_MASK); }

    static inline unsigned ToTypeArrayBits(unsigned char byte1, unsigned char byte2);

    static BOOL HasGettersOrSetters(ES_Indexed_Properties *properties) { return properties && properties->HasSpecialProperties(); }

    static ES_Indexed_Properties *Make(ES_Context *context, unsigned initial_index, BOOL with_attributes = FALSE);

    static inline BOOL HasProperty(ES_Indexed_Properties *properties, unsigned index);
    static inline BOOL HasProperty(ES_Indexed_Properties *properties, unsigned index, ES_Property_Info &info);

    static inline GetResult GetL(ES_Context *context, ES_Object *this_object, ES_Indexed_Properties *properties, unsigned index, ES_Value_Internal &value, const ES_Value_Internal &this_value);
    /**< @param this_object The object that owns this ES_Indexed_Properties
                            object.

         @param this_value The 'this' value for calls to accessor functions.
                           This is the Reference's base value, and can be a
                           primitive.  If it is an object, it's not necessarily
                           the same as 'this_object' (which can be an object
                           from the prototype chain.) */

    static inline ES_Value_Internal *GetStorage(ES_Indexed_Properties *properties, unsigned index);
    /**< Return a pointer to the actual storage of the index:th property.  If
         the property does not exist, NULL is returned.  If there's a getter or
         setter defined, the caller will need to act accordingly. */

    static inline PutResult PutL(ES_Context *context, ES_Object *this_object, unsigned index, const ES_Value_Internal &value, const ES_Value_Internal &this_value);
    static inline PutResult PutL(ES_Context *context, ES_Object *this_object, unsigned index, unsigned *attributes, const ES_Value_Internal &value, const ES_Value_Internal &this_value);
    static inline PutResult PutNoLockL(ES_Context *context, ES_Object *this_object, unsigned index, unsigned *attributes, const ES_Value_Internal &value, const ES_Value_Internal &this_value);
    /**< @param this_object The object that owns this ES_Indexed_Properties
                            object.

         @param this_value The 'this' value for calls to accessor functions.
                           This is the Reference's base value, and can be a
                           primitive.  If it is an object, it's not necessarily
                           the same as 'this_object' (which can be an object
                           from the prototype chain.) */

    static inline BOOL PutManyL(ES_Object *this_object, unsigned index, unsigned argc, ES_Value_Internal *argv);

    static inline ES_Indexed_Properties *PutL(ES_Context *context, ES_Indexed_Properties *properties, unsigned index, ES_Value_Internal *&value);
    static inline ES_Indexed_Properties *PutL(ES_Context *context, ES_Indexed_Properties *properties, unsigned index, unsigned *attributes, ES_Value_Internal *&value);
    static inline ES_Indexed_Properties *InitPropertyL(ES_Context *context, ES_Indexed_Properties *properties, unsigned index, ES_Value_Internal *&value);

    static inline BOOL DeleteL(ES_Context *context, ES_Object *this_object, ES_Indexed_Properties *properties, unsigned index);

    static inline unsigned TruncateL(ES_Context *context, ES_Object *this_object, ES_Indexed_Properties *properties, unsigned start, unsigned end);
    /**< Delete properties with indeces higher than or equal to 'start' and
         lower than 'end'.  Primarily used for setting the 'length' property on
         an array object, in which case 'start' will be the new length and 'end'
         will be the old length.  For array objects, there will never be any
         properties in the array with indeces higher than or equal to 'end'. */

    static PutResult DefineGetterL(ES_Context *context, ES_Object *self, unsigned index, ES_Function *function);
    static PutResult DefineSetterL(ES_Context *context, ES_Object *self, unsigned index, ES_Function *function);

    static void LookupGetter(ES_Indexed_Properties *properties, unsigned index, ES_Function *&function);
    static void LookupSetter(ES_Indexed_Properties *properties, unsigned index, ES_Function *&function);

    static inline unsigned GetUsed(ES_Indexed_Properties *properties);
    static inline BOOL HasUsed(ES_Indexed_Properties *properties);

    static inline BOOL RenumberL(ES_Execution_Context *context, ES_Object *this_object, unsigned index, unsigned length, int delta, unsigned array_length);
    /**< Renumber indeces 'index' through 'index+length' by adding delta to
         their indeces.  It is an error if this causes two elements to have the
         same index.  Typically, this will not happen because the caller will
         just have deleted elements whose indeces are about to be reused by the
         renumbering (not needed if getters or setters are involved).  It is
         also an error if one or more of the renumbered elements' index becomes
         lower than zero or higher than UINT_MAX-1.

         This function will never change representation, unless there are getters
         or setters defined for in the range [index, index + length] in which
         case it is possible for renumber to change representation. */

    static inline void AdjustTop(ES_Indexed_Properties *properties, unsigned length);

    BOOL HasSpecialProperties() { return (hdr.header & ES_Header::MASK_HAS_SPECIAL) != 0; }
    void SetHasSpecialProperties() { hdr.header |= ES_Header::MASK_HAS_SPECIAL; }

    BOOL HasReadOnlyProperties() { return (hdr.header & ES_Header::MASK_HAS_READ_ONLY) != 0; }
    void SetHasReadOnlyProperties() { hdr.header |= ES_Header::MASK_HAS_READ_ONLY; }

    BOOL NeedsCopyOnWrite() { return (hdr.header & ES_Header::MASK_COPY_ON_WRITE) != 0; }
    void SetNeedsCopyOnWrite() { hdr.header |= ES_Header::MASK_COPY_ON_WRITE; }

    static ES_Indexed_Properties *ChangeAttribute(ES_Context *context, ES_Indexed_Properties *properties, unsigned index, const ES_Property_Info &info);

protected:
    static BOOL RenumberSpecialL(ES_Context *context, ES_Object *this_object, unsigned index, unsigned length, int delta);
    static BOOL RenumberFromPrototypeL(ES_Execution_Context *context, ES_Object *this_object, unsigned index, unsigned length, int delta, unsigned array_length);
};

class ES_Indexed_Property_Iterator;

class ES_Compact_Indexed_Properties
    : public ES_Indexed_Properties
{
public:
    enum { MINIMUM_CAPACITY = 4 };

    static inline unsigned AppropriateCapacity(unsigned length);
    static inline unsigned AllocationSize(unsigned capacity);

    static ES_Compact_Indexed_Properties *Make(ES_Context *context, unsigned capacity = 8, unsigned initialize_from = 0, BOOL with_attributes = FALSE);

    static ES_Compact_Indexed_Properties *Make(ES_Context *context, ES_Code *code, ES_CodeStatic::ConstantArrayLiteral &cal);

    unsigned Capacity() { return capacity; }
    /**< The current capacity.  This is not the same as the array's length. */

    unsigned Top() { return top; }
    /**< MIN(capacity, length) for arrays, equals capacity for non-arrays. */

    BOOL Get(unsigned index, ES_Value_Internal &value) { value = values[index]; return IsUsed(index); }
    /**< Used for property reads and property writes of existing properties. */

    inline ES_Indexed_Properties *PutL(ES_Context *context, unsigned index, unsigned *attributes, ES_Value_Internal *&value);
    /**< Used for property writes when the property is known not to exist, or
         not known to exist.  May change representation to TYPE_SPARSE. */

    inline BOOL PutManyL(unsigned index, unsigned argc, ES_Value_Internal *argv);
    /**< Used for attempted block write of a set of properties. Returns TRUE if successfully performed; FALSE if
         not attempted. The write is atomic, either the whole block is put or nothing. */

    inline void PutSimpleNew(unsigned index, const ES_Value_Internal &value);
    /**< Used for property writes when the property is known not to exist yet
         and known not to be a setter.  The index must be lower than the current
         capacity.  Never changes representation. */

    ES_Indexed_Properties *DeleteL(ES_Context *context, unsigned index, BOOL &result);
    /**< Delete property.  May reallocate, and may change representation to
         TYPE_SPARSE. */

    ES_Indexed_Properties *Share(ES_Context *context);
    /**< Prepare for in-place modifications, that is, copy if copy-on-write flag
         is set.  Returns a tagged pointer. */

    void Truncate(unsigned start, unsigned &end);

    ES_Value_Internal *GetValues() { return values; }

    ES_Indexed_Properties *RenumberL(ES_Context *context, unsigned index, unsigned length, int delta);
    /**< See documentation in ES_Indexed_Properties. */

    unsigned GetUsed();
    BOOL HasUsed();

    BOOL HasAttributes() { return (hdr.header & ES_Header::MASK_HAS_ATTRIBUTES) != 0; }
    void SetHasAttributes() { hdr.header |= ES_Header::MASK_HAS_ATTRIBUTES; }

    inline unsigned GetAttributes(unsigned index);
    inline void SetAttributes(unsigned index, unsigned attributes);

private:
    friend class ES_Object;

    static void Initialize(ES_Compact_Indexed_Properties *properties, unsigned capacity)
    {
        OP_ASSERT(capacity > 0);

        properties->InitGCTag(GCTAG_ES_Compact_Indexed_Properties);

        properties->capacity = capacity;
        properties->top = capacity;
    }

    ES_Indexed_Properties *GrowAndPutL(ES_Context *context, unsigned index, unsigned *attributes, ES_Value_Internal *&value);

    BOOL IsUsed(unsigned index) { OP_ASSERT(index < capacity); return !values[index].IsUndefined() || values[index].GetHiddenBoolean(); }

    friend class ESMM;
    friend class ES_Indexed_Properties;
    friend class ES_Sparse_Indexed_Properties;
    friend class ES_Indexed_Property_Iterator;

    ES_Sparse_Indexed_Properties *MakeSparse(ES_Context *context);

    void ClearAndPut(unsigned clear_from_index, unsigned put_index, const ES_Value_Internal &value);

    static unsigned AttributesSize(unsigned capacity) { return sizeof(unsigned) * ((capacity + 9) / 10); }

    inline unsigned *GetAttributes();

#ifdef ES_NATIVE_SUPPORT
public:
#endif // ES_NATIVE_SUPPORT
    unsigned capacity;
    /**< Allocated capacity (actual length of 'values'.) */
    unsigned top;
    /**< MIN(capacity, length) for arrays, equals capacity for non-arrays. */

    ES_Value_Internal values[1]; // ARRAY OK 2011-06-08 sof
};

/* NOTE: The sparse representation is considered a special case, and is designed
   to perform reasonably well in all ways; memory effiency, random access
   complexity and ease of implementing array primitives.  It is not designed to
   be optimally performing in either area. */

class ES_Sparse_Indexed_Properties
    : public ES_Indexed_Properties
{
public:
    static ES_Sparse_Indexed_Properties *Make(ES_Context *context);

    BOOL Get(unsigned index, ES_Value_Internal &value);
    /**< Used for property reads and property writes of properties known to
         exist. */

    ES_Indexed_Properties *PutL(ES_Context *context, unsigned index, unsigned *attributes, ES_Value_Internal *&value, BOOL force_sparse = FALSE);
    /**< Used for property writes when the property is known not to exist, or
         not known to exist.  May change representation to TYPE_COMPACT. */

    ES_Indexed_Properties *DeleteL(unsigned index, BOOL &result);
    /**< Delete property.  May reallocate, and may change representation to
         TYPE_COMPACT. */

    ES_Indexed_Properties *TruncateL(ES_Context *context, unsigned start, unsigned &end);

    void Renumber(unsigned index, unsigned length, int delta);
    /**< See documentation in ES_Indexed_Properties.  Also: since this function
         is never used to remove elements, only to renumber them, it never needs
         to switch to compact representation.  (If it could remove elements,
         size might go below the "always compact" limit, or even reach zero, and
         then we would be forced to switch to compact representation to not
         break assumptions made elsewhere.) */

    unsigned GetUsed() { return used; }

private:
    static void Initialize(ES_Sparse_Indexed_Properties *properties)
    {
        properties->InitGCTag(GCTAG_ES_Sparse_Indexed_Properties);

        properties->used = 0;
        properties->blocks_used = 0;
        properties->root = NULL;
        properties->free = NULL;
        properties->blocks = NULL;
    }

    friend class ESMM;
    friend class ES_Indexed_Properties;
    friend class ES_Compact_Indexed_Properties;
    friend class ES_Indexed_Property_Iterator;

    enum { NODES_PER_BLOCK = 32 };

    void PutFirst(ES_Context *context, unsigned index, unsigned *attributes, ES_Value_Internal *&value)
    {
        OP_ASSERT(free != NULL);
        Node *node = root = AllocateNode(context);

        node->value.SetUndefined();
        node->parent = node->less = node->greater = NULL;
        node->index = index;
        node->attributes = attributes ? *attributes : 0;

        used = 1;

        value = &node->value;
    }

    class Node
    {
    public:
        enum Color { BLACK, RED };

        static Node *Colorize(Node *n, Color c) { return reinterpret_cast<Node *>(reinterpret_cast<UINTPTR>(n) | c); }
        static Color GetColor(Node *n) { return n ? static_cast<Color>(reinterpret_cast<UINTPTR>(n->parent) & 1) : BLACK; }

        Node *GetParent() { return reinterpret_cast<Node *>(reinterpret_cast<UINTPTR>(parent) & ~static_cast<UINTPTR>(1)); }
        Node *GetGrandParent() { return parent ? GetParent()->GetParent() : NULL; }
        Node *GetSibling() { if (Node *p = GetParent()) return this == p->less ? p->greater : p->less; else return NULL; }
        Node *GetUncle() { if (Node *p = GetParent()) return p->GetSibling(); else return NULL; }

        Node *GetNext()
        {
            if (greater)
                return greater->Minimum();
            else
            {
                Node *iter = this;
                while (Node *parent = iter->GetParent())
                    if (iter == parent->less)
                        return parent;
                    else
                        iter = parent;
                return NULL;
            }
        }

        Node *GetPrevious()
        {
            if (less)
                return less->Maximum();
            else
            {
                Node *iter = this;
                while (Node *parent = iter->GetParent())
                    if (iter == parent->greater)
                        return parent;
                    else
                        iter = parent;
                return NULL;
            }
        }

        void SetParent(Node *p) { parent = Colorize(p, GetColor(this)); }
        void SetColor(Color c) { parent = Colorize(GetParent(), c); }

        Node *Maximum() { Node *iter = this; while (iter->greater) iter = iter->greater; return iter; }
        Node *Minimum() { Node *iter = this; while (iter->less) iter = iter->less; return iter; }

#ifdef RBTREE_DEBUG
        BOOL RecursiveCheck(unsigned black_count, unsigned black_above);
#endif // RBTREE_DEBUG

        ES_Value_Internal value;
        Node *parent, *less, *greater;
        unsigned index, attributes;
    };

    void AllocateBlock(ES_Context *context);

    Node *AllocateNode(ES_Context *context)
    {
        if (!free)
            AllocateBlock(context);

        Node *node = free;
        free = free->less;
        return node;
    }

    void FreeNode(Node *node)
    {
        node->index = UINT_MAX;
        node->less = free;
        free = node;
    }

    BOOL Find(unsigned index, Node *&node, Node *&parent)
    {
        Node *iter_node = root;

        while (TRUE)
            if (index == iter_node->index)
            {
                node = iter_node;
                return TRUE;
            }
            else if (index < iter_node->index)
            {
                if (iter_node->less)
                    iter_node = iter_node->less;
                else
                {
                    parent = iter_node;
                    return FALSE;
                }
            }
            else if (iter_node->greater)
                iter_node = iter_node->greater;
            else
            {
                parent = iter_node;
                return FALSE;
            }
    }

#ifdef RBTREE_DEBUG
    BOOL Check();
#endif // RBTREE_DEBUG

    unsigned used, blocks_used;
    Node *root, *free;
    ES_Box *blocks;
};

class ES_Byte_Array_Indexed
    : public ES_Indexed_Properties
{
public:
    static ES_Byte_Array_Indexed *Make(ES_Context *context, unsigned capacity, unsigned char *bytes);
    /**< Attempt to allocate an external byte array holding 'capacity' bytes.
         If 'bytes' is non-NULL, it will be the external storage of the object.
         It muat have been allocated using op_malloc() / op_realloc().

         If 'bytes' is NULL, 'capacity' bytes is attempted allocated using
         op_malloc(). If that request is not satisifiable by the
         storage manager, NULL is returned. */

    unsigned Capacity() { return capacity; }
    unsigned char *Storage() { return byte_array; }

    ES_Value_Internal *Value(unsigned index) { value.SetInt32(Storage()[index]); return &value; }
    BOOL PutL(ES_Context *context, unsigned index, const ES_Value_Internal &value);

    static void Transfer(ES_Byte_Array_Indexed *source, ES_Byte_Array_Indexed *target)
    {
        Destroy(target);
        target->byte_array = source->byte_array;
        target->capacity = source->capacity;
        source->byte_array = NULL;
        source->capacity = 0;
    }

private:
    static void Initialize(ES_Byte_Array_Indexed *array, unsigned capacity, unsigned char *byte_array)
    {
        array->InitGCTag(GCTAG_ES_Byte_Array_Indexed);
        array->capacity = capacity;
        array->byte_array = byte_array;
        array->SetNeedDestroy();
    }

    static void Destroy(ES_Byte_Array_Indexed *array)
    {
        op_free(array->byte_array);
        array->byte_array = NULL;
    }

    friend class ES_Native;
    friend class ESMM;

    unsigned capacity;
    unsigned char *byte_array;
    ES_Value_Internal value;
};

class ES_Type_Array_Indexed
    : public ES_Indexed_Properties
{
public:
    enum TypeArrayKind
    {
        Int8Array = 0,
        Uint8Array,
        Uint8ClampedArray,
        Int16Array,
        Uint16Array,
        Int32Array,
        Uint32Array,
        Float32Array,
        Float64Array
    };

    enum TypeArrayBits
    {
        AnyIntArray = 0x7f,
        AnyInt8Array = 0x07,
        AnyInt16Array = 0x18,
        AnyInt32Array = 0x60,
        AnyFloatArray = 0x180,
        Float32ArrayBit = 0x80,
        Float64ArrayBit = 0x100
    };

    static ES_Type_Array_Indexed *Make(ES_Context *context, unsigned byte_offset, unsigned capacity, TypeArrayKind kind, ES_Byte_Array_Indexed *byte_array);
    /**< Allocate a static type array of 'capacity' elements of type 'kind', kept in 'byte_array'.  */

    unsigned Capacity() { return capacity; }
    unsigned Offset() { return byte_offset; }
    unsigned char *Storage() { return byte_array->Storage() + byte_offset; }
    TypeArrayKind Kind() const { return kind; }
    ES_Byte_Array_Indexed *GetByteArray() { return byte_array; }

    ES_Value_Internal *Value(unsigned index)
    {
        GetValue(index, value);
        return &value;
    }

    void GetValue(unsigned index, ES_Value_Internal &value)
    {
        unsigned char *values = Storage();
        switch (kind)
        {
        default:
            OP_ASSERT(!"Incomplete switch statement; cannot happen");
        case Int8Array:
            value.SetInt32(reinterpret_cast<signed char *>(values)[index]);
            break;
        case Uint8Array:
        case Uint8ClampedArray:
            value.SetInt32(reinterpret_cast<unsigned char *>(values)[index]);
            break;
        case Int16Array:
            value.SetInt32(reinterpret_cast<signed short *>(values)[index]);
            break;
        case Uint16Array:
            value.SetInt32(reinterpret_cast<unsigned short *>(values)[index]);
            break;
        case Int32Array:
            value.SetInt32(reinterpret_cast<int *>(values)[index]);
            break;
        case Uint32Array:
            value.SetUInt32(reinterpret_cast<unsigned *>(values)[index]);
            break;
        case Float32Array:
            value.SetNumber(reinterpret_cast<float *>(values)[index]);
            break;
        case Float64Array:
            value.SetNumber(reinterpret_cast<double *>(values)[index]);
            break;
        }
    }

    BOOL PutL(ES_Context *context, unsigned index, const ES_Value_Internal &value);

    void Neuter()
    {
        byte_array = NULL;
        byte_offset = 0;
        capacity = 0;
    }


private:
    static void Initialize(ES_Type_Array_Indexed *array, unsigned byte_offset, unsigned capacity, TypeArrayKind kind, ES_Byte_Array_Indexed *byte_array)
    {
        array->InitGCTag(GCTAG_ES_Type_Array_Indexed);
        array->capacity = capacity;
        array->byte_offset = byte_offset;
        array->kind = kind;
        array->byte_array = byte_array;
    }

    friend class ES_Native;
    friend class ESMM;

    unsigned capacity;
    unsigned byte_offset;
    TypeArrayKind kind;
    ES_Value_Internal value;
    ES_Byte_Array_Indexed *byte_array;
};

class ES_Indexed_Property_Iterator
{
public:
    ES_Indexed_Property_Iterator(ES_Context *context, ES_Object *object, ES_Indexed_Properties *&properties, BOOL skip_dont_enum = FALSE)
        : context(context),
          object(object),
          properties(properties),
          index(UINT_MAX),
          cached_node(NULL),
          skip_dont_enum(skip_dont_enum)
    {
        ResetPropertyFlag();
    }

    BOOL Previous(unsigned &index);
    BOOL Next(unsigned &index);

    BOOL LowerBound(unsigned &index, unsigned limit);
    /**< Move iterator to smallest present index greater than or equal to
         'limit'. */

    BOOL UpperBound(unsigned &index, unsigned limit);
    /**< Move iterator to largest present index less than or equal to
         'limit'. */

    inline BOOL GetValue(ES_Value_Internal &value);
    inline BOOL IsConfigurable();
    inline unsigned GetAttributes();

    BOOL DeleteL(ES_Context *context);
    /**< Deletes current index, and keeps the iterator updated (or flushes its
         cache automatically if it can't.) */

    void Reset() { index = UINT_MAX; cached_node = NULL; }
    void FlushCache() { cached_node = NULL; ResetPropertyFlag(); }

    unsigned GetCount() { return ES_Indexed_Properties::GetUsed(properties); }

    unsigned GetCurrentIndex() { return index; }

private:
    BOOL GetValueSlow(ES_Value_Internal &value);

    void ResetPropertyFlag()
    {
        is_plain_compact = properties && ES_Indexed_Properties::GetType(properties) == ES_Indexed_Properties::TYPE_COMPACT && !(object && (object->HasIndexedGettersOrSetters() || object->IsStringObject() || object->IsHostObject()));
    }

    ES_Context *context;

    ES_Object *object;
    /**< The object whose properties are iterated. */

    ES_Indexed_Properties *&properties;
    /**< Reference to pointer; typically to ES_Object::indexed_properties.
         Needs to be indirect, since modifications of the object may reallocate
         the storage. */

    unsigned index;
    /**< Current index (last one returned) or UINT_MAX for initial state. */

    ES_Sparse_Indexed_Properties::Node *cached_node;
    /**< Cached node for binary tree traversal. */

    BOOL skip_dont_enum;
    /**< Skip properties that aren't enumerable. */

    BOOL is_plain_compact;
    /**< The properties being iterated are of the compact variety. */
};

#ifdef _DEBUG
# undef inline
#endif // _DEBUG

#endif // ES_INDEXED_H
