/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_INDEXED_INLINES_H
#define ES_INDEXED_INLINES_H

#include "modules/ecmascript/carakan/src/object/es_indexed.h"
#include "modules/ecmascript/carakan/src/object/es_typedarray.h"

#ifdef _DEBUG
# define inline
#endif // _DEBUG

/* static */ inline unsigned
ES_Indexed_Properties::GetTypeBits(ES_Indexed_Properties *properties)
{
    Type type = static_cast<Type>(properties->GCTag());

    switch (type)
    {
    case TYPE_COMPACT:
        return TYPE_BITS_COMPACT;
    case TYPE_SPARSE:
        return TYPE_BITS_SPARSE;
    case TYPE_BYTE_ARRAY:
        return TYPE_BITS_BYTE_ARRAY;
    default:
        return TYPE_BITS_TYPE_ARRAY | ((0x1 << static_cast<ES_Type_Array_Indexed *>(properties)->Kind()) << 5);
    }
}

/* static */ inline unsigned
ES_Indexed_Properties::ToTypeArrayBits(unsigned char byte1, unsigned char byte2)
{
    OP_ASSERT((byte1 & TYPE_BITS_TYPE_MASK) == TYPE_BITS_TYPE_ARRAY);
    return (static_cast<unsigned>(byte1) >> 5) | (static_cast<unsigned>(byte2 & 0x3f) << 3);
}

/* static */ inline BOOL
ES_Indexed_Properties::HasProperty(ES_Indexed_Properties *properties, unsigned index)
{
    ES_Property_Info info;
    return HasProperty(properties, index, info);
}

/* static */ inline BOOL
ES_Indexed_Properties::HasProperty(ES_Indexed_Properties *properties, unsigned index, ES_Property_Info &info)
{
    if (properties)
    {
        switch (GetType(properties))
        {
        default:
        case TYPE_COMPACT:
        {
            ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(properties);
            if (index < compact->Capacity() && compact->IsUsed(index))
            {
                if (compact->HasAttributes())
                    info.Set(compact->GetAttributes(index));
                return TRUE;
            }
            else
                return FALSE;
        }

        case TYPE_SPARSE:
        {
            ES_Sparse_Indexed_Properties::Node *node, *parent;
            if (static_cast<ES_Sparse_Indexed_Properties *>(properties)->Find(index, node, parent))
            {
                info.Set(node->attributes);
                return TRUE;
            }
            else
                return FALSE;
        }

        case TYPE_BYTE_ARRAY:
        {
            ES_Byte_Array_Indexed *byte_array = static_cast<ES_Byte_Array_Indexed *>(properties);
            if (index < byte_array->Capacity())
            {
                info.SetConfigurable(FALSE);
                return TRUE;
            }
            else
                return FALSE;
        }
        case TYPE_TYPE_ARRAY:
        {
            ES_Type_Array_Indexed *type_array = static_cast<ES_Type_Array_Indexed *>(properties);
            if (index < type_array->Capacity())
            {
                info.SetConfigurable(FALSE);
                return TRUE;
            }
            else
                return FALSE;
        }
        }
    }
    else
        return FALSE;
}

/* static */ inline GetResult
ES_Indexed_Properties::GetL(ES_Context *context, ES_Object *this_object, ES_Indexed_Properties *properties, unsigned index, ES_Value_Internal &value, const ES_Value_Internal &this_value)
{
    if (this_object->IsStringObject())
    {
        JString *result = NULL;
        if (ES_String_Object::GetSubString(context, static_cast<ES_String_Object *>(this_object), index, 1, result))
        {
            value.SetString(result);
            return PROP_GET_OK;
        }
    }

    if (properties)
        if (ES_Value_Internal *storage = GetStorage(properties, index))
            if (storage->IsSpecial())
                return static_cast<ES_Special_Property *>(storage->GetDecodedBoxed())->SpecialGetL(context, this_object, value, this_value);
            else
            {
                value = *storage;
                return PROP_GET_OK;
            }

    value.SetUndefined();
    return PROP_GET_NOT_FOUND;
}

/* static */ inline ES_Value_Internal *
ES_Indexed_Properties::GetStorage(ES_Indexed_Properties *properties, unsigned index)
{
    if (properties)
        switch (GetType(properties))
        {
        default:
        case TYPE_COMPACT:
        {
            ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(properties);

            if (index < compact->Capacity() && compact->IsUsed(index))
                return &compact->values[index];
            else
                return NULL;
        }

        case TYPE_SPARSE:
        {
            ES_Sparse_Indexed_Properties *sparse = static_cast<ES_Sparse_Indexed_Properties *>(properties);
            ES_Sparse_Indexed_Properties::Node *node, *parent;

            if (sparse->Find(index, node, parent))
                return &node->value;
            else
                return NULL;
        }
        case TYPE_BYTE_ARRAY:
        {
            ES_Byte_Array_Indexed *byte_array = static_cast<ES_Byte_Array_Indexed *>(properties);

            if (index < byte_array->Capacity())
                return byte_array->Value(index);
            else
                return NULL;
        }
        case TYPE_TYPE_ARRAY:
        {
            ES_Type_Array_Indexed *type_array = static_cast<ES_Type_Array_Indexed *>(properties);

            if (index < type_array->Capacity())
                return type_array->Value(index);
            else
                return NULL;
        }
        }

    return NULL;
}

/* static */ inline PutResult
ES_Indexed_Properties::PutNoLockL(ES_Context *context, ES_Object *this_object, unsigned index, unsigned *attributes, const ES_Value_Internal &value, const ES_Value_Internal &this_value)
{
    ES_Value_Internal *ptr;

    if (this_object->IsStringObject())
    {
        unsigned int slen = Length(static_cast<ES_String_Object *>(this_object)->GetValue());
        if (index < slen)
            return PROP_PUT_READ_ONLY;
    }

    ES_Indexed_Properties *properties = this_object->GetIndexedProperties(), *new_properties = properties;
    BOOL is_readonly = FALSE;

    if (properties && properties->HasReadOnlyProperties())
    {
        ES_Property_Info info;
        if (HasProperty(properties, index, info) && info.IsReadOnly())
            is_readonly = TRUE;
    }

    if (!this_object->IsExtensible())
        if (!properties || !HasProperty(properties, index))
            return PROP_PUT_READ_ONLY;

    if (properties)
    {
        switch (GetType(properties))
        {
        default:
        case TYPE_COMPACT:
            new_properties = static_cast<ES_Compact_Indexed_Properties *>(properties)->PutL(context, index, attributes, ptr);
            break;

        case TYPE_SPARSE:
            new_properties = static_cast<ES_Sparse_Indexed_Properties *>(properties)->PutL(context, index, attributes, ptr);
            break;

        case TYPE_BYTE_ARRAY:
            return static_cast<ES_Byte_Array_Indexed *>(properties)->PutL(context, index, value) ? PROP_PUT_OK : PROP_PUT_FAILED;
        case TYPE_TYPE_ARRAY:
            return static_cast<ES_Type_Array_Indexed *>(properties)->PutL(context, index, value) ? PROP_PUT_OK : PROP_PUT_FAILED;
        }

        if (ptr->IsSpecial() && !attributes)
        {
            if (new_properties != properties)
                this_object->SetIndexedProperties(new_properties);

            return static_cast<ES_Special_Property *>(ptr->GetDecodedBoxed())->SpecialPutL(context, this_object, value, this_value);
        }
    }
    else
    {
        new_properties = ES_Indexed_Properties::Make(context, index);

        OP_ASSERT(GetType(new_properties) != TYPE_BYTE_ARRAY && GetType(new_properties) != TYPE_TYPE_ARRAY);

        if (GetType(new_properties) == TYPE_COMPACT)
            new_properties = static_cast<ES_Compact_Indexed_Properties *>(new_properties)->PutL(context, index, attributes, ptr);
        else
            static_cast<ES_Sparse_Indexed_Properties *>(new_properties)->PutFirst(context, index, attributes, ptr);
    }

    BOOL need_to_set = new_properties != properties;

    if (value.IsSpecial())
    {
        if (!new_properties->HasSpecialProperties())
        {
            new_properties->SetHasSpecialProperties();
            need_to_set = TRUE;
        }
    }
    else if (is_readonly && !attributes)
        return PROP_PUT_READ_ONLY;

    if (value.IsUndefined())
        ptr->SetUndefined(TRUE);
    else
        *ptr = value;

    if (need_to_set)
        this_object->SetIndexedProperties(new_properties);

    return PROP_PUT_OK;
}

/* static */ inline PutResult
ES_Indexed_Properties::PutL(ES_Context *context, ES_Object *this_object, unsigned index, const ES_Value_Internal &value, const ES_Value_Internal &this_value)
{
    ES_CollectorLock gclock(context, TRUE);
    return PutNoLockL(context, this_object, index, 0, value, this_value);
}

/* static */ inline PutResult
ES_Indexed_Properties::PutL(ES_Context *context, ES_Object *this_object, unsigned index, unsigned *attributes, const ES_Value_Internal &value, const ES_Value_Internal &this_value)
{
    ES_CollectorLock gclock(context, TRUE);
    return PutNoLockL(context, this_object, index, attributes, value, this_value);
}

/* static */ inline BOOL
ES_Indexed_Properties::PutManyL(ES_Object *this_object, unsigned index, unsigned argc, ES_Value_Internal *argv)
{
    if (this_object->IsStringObject())
        return FALSE;

    ES_Indexed_Properties *properties = this_object->GetIndexedProperties();

    if (properties)
    {
        switch (GetType(properties))
        {
        default:
        case TYPE_COMPACT:
            return static_cast<ES_Compact_Indexed_Properties *>(properties)->PutManyL(index, argc, argv);
        case TYPE_SPARSE:
        case TYPE_BYTE_ARRAY:
        case TYPE_TYPE_ARRAY:
            return FALSE;
        }
    }
    else
        return FALSE;
}


/* static */ inline BOOL
ES_Indexed_Properties::DeleteL(ES_Context *context, ES_Object *this_object, ES_Indexed_Properties *properties, unsigned index)
{
    BOOL result = TRUE;

    if (properties)
    {
        ES_Indexed_Properties *new_properties = properties;

        switch (GetType(properties))
        {
        default:
        case TYPE_COMPACT:
            new_properties = static_cast<ES_Compact_Indexed_Properties *>(properties)->DeleteL(context, index, result);
            break;

        case TYPE_SPARSE:
            new_properties = static_cast<ES_Sparse_Indexed_Properties *>(properties)->DeleteL(index, result);
            break;

        case TYPE_BYTE_ARRAY:
            return index >= static_cast<ES_Byte_Array_Indexed *>(properties)->Capacity();
        case TYPE_TYPE_ARRAY:
            return index >= static_cast<ES_Type_Array_Indexed *>(properties)->Capacity();
        }

        if (new_properties != properties)
            this_object->SetIndexedProperties(new_properties);
    }

    return result;
}

/* static */ inline unsigned
ES_Indexed_Properties::TruncateL(ES_Context *context, ES_Object *this_object, ES_Indexed_Properties *properties, unsigned start, unsigned end)
{
    if (properties)
        switch (GetType(properties))
        {
        default:
        case TYPE_COMPACT:
            /* Truncating a compact array will never make it sparse. */
            static_cast<ES_Compact_Indexed_Properties *>(properties)->Truncate(start, end);
            break;

        case TYPE_SPARSE:
            ES_Indexed_Properties *new_properties = static_cast<ES_Sparse_Indexed_Properties *>(properties)->TruncateL(context, start, end);
            if (new_properties != properties)
                this_object->SetIndexedProperties(new_properties);
            break;
        }
    return end;
}

/* static */ inline unsigned
ES_Indexed_Properties::GetUsed(ES_Indexed_Properties *properties)
{
    if (properties)
        switch (GetType(properties))
        {
        default:
        case TYPE_COMPACT:
            return static_cast<ES_Compact_Indexed_Properties *>(properties)->GetUsed();

        case TYPE_SPARSE:
            return static_cast<ES_Sparse_Indexed_Properties *>(properties)->GetUsed();

        case TYPE_BYTE_ARRAY:
            return static_cast<ES_Byte_Array_Indexed *>(properties)->Capacity();
        case TYPE_TYPE_ARRAY:
            return static_cast<ES_Type_Array_Indexed *>(properties)->Capacity();
        }

    return 0;
}

/* static */ inline BOOL
ES_Indexed_Properties::HasUsed(ES_Indexed_Properties *properties)
{
    if (properties)
    {
        switch (GetType(properties))
        {
        default:
        case TYPE_COMPACT:
            return static_cast<ES_Compact_Indexed_Properties *>(properties)->HasUsed();

        case TYPE_SPARSE:
        case TYPE_BYTE_ARRAY:
        case TYPE_TYPE_ARRAY:
            return GetUsed(properties) != 0;
        }
    }
    else
        return FALSE;
}

/* static */ inline BOOL
ES_Indexed_Properties::RenumberL(ES_Execution_Context *context, ES_Object *this_object, unsigned index, unsigned length, int delta, unsigned array_length)
{
    ES_Indexed_Properties *properties = this_object->GetIndexedProperties();
    GC_STACK_ANCHOR(context, this_object);

    if (properties)
        if (properties->HasSpecialProperties())
            return RenumberSpecialL(context, this_object, index, length, delta);
        else if (delta != 0)
            switch (GetType(properties))
            {
            default:
            case TYPE_COMPACT:
            {
                ES_Indexed_Properties *new_properties = static_cast<ES_Compact_Indexed_Properties *>(properties)->RenumberL(context, index, length, delta);
                if (new_properties != properties)
                    this_object->SetIndexedProperties(new_properties);
                break;
            }

            case TYPE_SPARSE:
                static_cast<ES_Sparse_Indexed_Properties *>(properties)->Renumber(index, length, delta);
                break;

            case TYPE_BYTE_ARRAY:
            case TYPE_TYPE_ARRAY:
                break;
            }

    if (ES_Object *prototype = this_object->Class()->Prototype())
        if (prototype->HasIndexedProperties())
            return RenumberFromPrototypeL(context, this_object, index, length, delta, array_length);

    return TRUE;
}

/* static */ inline void
ES_Indexed_Properties::AdjustTop(ES_Indexed_Properties *properties, unsigned length)
{
    if (properties && GetType(properties) == TYPE_COMPACT)
    {
        ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(properties);

        compact->top = es_minu(length, compact->Capacity());
    }
}

/* static */ inline ES_Indexed_Properties *
ES_Indexed_Properties::PutL(ES_Context *context, ES_Indexed_Properties *properties, unsigned index, ES_Value_Internal *&value)
{
    return PutL(context, properties, index, 0, value);
}

/* static */ inline ES_Indexed_Properties *
ES_Indexed_Properties::PutL(ES_Context *context, ES_Indexed_Properties *properties, unsigned index, unsigned *attributes, ES_Value_Internal *&value)
{
    if (properties)
    {
        /* Used only for internal purposes, and never on byte arrays. */
        OP_ASSERT(GetType(properties) != TYPE_BYTE_ARRAY && GetType(properties) != TYPE_TYPE_ARRAY);

        if (GetType(properties) == TYPE_COMPACT)
            return static_cast<ES_Compact_Indexed_Properties *>(properties)->PutL(context, index, attributes, value);
        else
            return static_cast<ES_Sparse_Indexed_Properties *>(properties)->PutL(context, index, attributes, value);
    }
    else
    {
        properties = ES_Indexed_Properties::Make(context, index, attributes && *attributes != 0);

        /* same as above, byte arrays aren't created via Make(). Internal consistency check. */
        OP_ASSERT(GetType(properties) != TYPE_BYTE_ARRAY && GetType(properties) != TYPE_TYPE_ARRAY);

        if (GetType(properties) == TYPE_COMPACT)
            static_cast<ES_Compact_Indexed_Properties *>(properties)->PutL(context, index, attributes, value);
        else
            static_cast<ES_Sparse_Indexed_Properties *>(properties)->PutFirst(context, index, attributes, value);

        return properties;
    }
}

/* static */ inline unsigned
ES_Compact_Indexed_Properties::AppropriateCapacity(unsigned length)
{
    return es_maxu(length, MINIMUM_CAPACITY);
}

/* static */ inline unsigned
ES_Compact_Indexed_Properties::AllocationSize(unsigned capacity)
{
    return sizeof(ES_Compact_Indexed_Properties) + (capacity - 1) * sizeof(ES_Value_Internal);
}

inline ES_Indexed_Properties *
ES_Compact_Indexed_Properties::PutL(ES_Context *context, unsigned new_index, unsigned *new_attributes, ES_Value_Internal *&new_value)
{
    if (new_index < capacity && !NeedsCopyOnWrite() && (!new_attributes || *new_attributes == 0 || HasAttributes()))
    {
        if (new_attributes && (*new_attributes != 0 || HasAttributes()))
        {
            SetAttributes(new_index, *new_attributes);
            if ((*new_attributes & RO) == RO)
                SetHasReadOnlyProperties();
        }

        new_value = &values[new_index];
        return this;
    }
    else
        return GrowAndPutL(context, new_index, new_attributes, new_value);
}

inline BOOL
ES_Compact_Indexed_Properties::PutManyL(unsigned new_index, unsigned argc, ES_Value_Internal *argv)
{
    if (new_index < capacity && (new_index + argc) <= capacity && !NeedsCopyOnWrite() && !HasSpecialProperties())
    {
        ES_Value_Internal *dst = values + new_index, *stop = argv + argc;
        while (argv != stop)
        {
            if (!argv->IsUndefined())
                *dst = *argv;
            else
                dst->SetUndefined(TRUE);

            ++argv, ++dst;
        }
        return TRUE;
    }
    else
        return FALSE;
}


inline void
ES_Compact_Indexed_Properties::PutSimpleNew(unsigned index, const ES_Value_Internal &value)
{
    OP_ASSERT(index < capacity);

    if (!value.IsUndefined())
        values[index] = value;
    else
        values[index].SetUndefined(TRUE);
}

inline unsigned *
ES_Compact_Indexed_Properties::GetAttributes()
{
    if (HasAttributes())
        return reinterpret_cast<unsigned *>(&values[capacity]);
    else
        return NULL;
}

inline unsigned
ES_Compact_Indexed_Properties::GetAttributes(unsigned index)
{
    if (unsigned *attributes = GetAttributes())
    {
        unsigned shift = (index % 10) * 3;
        return attributes[index / 10] >> shift & 7;
    }
    else
        return 0;
}

inline void
ES_Compact_Indexed_Properties::SetAttributes(unsigned index, unsigned attributes)
{
    unsigned &word = GetAttributes()[index / 10], shift = (index % 10) * 3;
    word = (word & ~(7 << shift)) | (attributes & 7) << shift;
}

inline BOOL
ES_Indexed_Property_Iterator::GetValue(ES_Value_Internal &value)
{
    if (is_plain_compact)
    {
        OP_ASSERT(ES_Indexed_Properties::GetType(properties) == ES_Indexed_Properties::TYPE_COMPACT);
        value = static_cast<ES_Compact_Indexed_Properties *>(properties)->values[index];
        OP_ASSERT(!value.IsSpecial());
        return TRUE;
    }
    else
        return GetValueSlow(value);
}

inline BOOL
ES_Indexed_Property_Iterator::IsConfigurable()
{
    if (object && (object->IsStringObject() || object->IsHostObject()))
        return FALSE;

    switch (ES_Indexed_Properties::GetType(properties))
    {
    case ES_Indexed_Properties::TYPE_COMPACT:
        return (static_cast<ES_Compact_Indexed_Properties *>(properties)->GetAttributes(index) & DD) == 0;

    case ES_Indexed_Properties::TYPE_SPARSE:
        return (cached_node->attributes & DD) == 0;
    }

    return FALSE;
}

inline unsigned
ES_Indexed_Property_Iterator::GetAttributes()
{
    switch (ES_Indexed_Properties::GetType(properties))
    {
    case ES_Indexed_Properties::TYPE_COMPACT:
        return static_cast<ES_Compact_Indexed_Properties *>(properties)->GetAttributes(index);

    case ES_Indexed_Properties::TYPE_SPARSE:
        return cached_node->attributes;
    }

    return RO | DD;
}

#ifdef _DEBUG
# undef inline
#endif // _DEBUG

#endif // ES_INDEXES_INLINES_H
