/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/object/es_indexed.h"
#include "modules/ecmascript/carakan/src/builtins/es_array_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_indexed_inlines.h"

/** For arrays not bigger than this, we always use the compact model, regardless
    of usage level. */
#define ES_ARRAY_ALWAYS_COMPACT_LIMIT 256

/** For arrays larger than ES_ARRAY_ALWAYS_COMPACT_LIMIT, use the compact model
    if the total number of slots divided by the number of slots used is less
    than or equal to this number. */
#define ES_ARRAY_COMPACT_USAGE_LEVEL_LIMIT 2

/* static */ ES_Indexed_Properties *
ES_Indexed_Properties::Make(ES_Context *context, unsigned initial_index, BOOL with_attributes)
{
    if (initial_index < ES_ARRAY_ALWAYS_COMPACT_LIMIT)
        /* No need to tag here; there will be no getters or setters. */
        return ES_Compact_Indexed_Properties::Make(context, ES_Compact_Indexed_Properties::AppropriateCapacity(initial_index + 1), 0, with_attributes);
    else
        return ES_Sparse_Indexed_Properties::Make(context);
}

/* static */ PutResult
ES_Indexed_Properties::DefineGetterL(ES_Context *context, ES_Object *self, unsigned index, ES_Function *function)
{
    ES_Value_Internal value;
    ES_Indexed_Properties *properties = self->GetIndexedProperties();

    if (properties)
    {
        BOOL res;

        switch (ES_Indexed_Properties::GetType(properties))
        {
        default:
        case ES_Indexed_Properties::TYPE_COMPACT:
            if (index < static_cast<ES_Compact_Indexed_Properties *>(properties)->capacity)
                res = static_cast<ES_Compact_Indexed_Properties *>(properties)->Get(index, value);
            else
                res = FALSE;
            break;

        case ES_Indexed_Properties::TYPE_SPARSE:
            res = static_cast<ES_Sparse_Indexed_Properties *>(properties)->Get(index, value);
            break;

        case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
        case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
            return PROP_PUT_OK;
        }

        if (res)
            if (value.IsGetterOrSetter())
            {
                static_cast<ES_Special_Mutable_Access *>(value.GetDecodedBoxed())->getter = function;
                return PROP_PUT_OK;
            }
    }

    value.SetBoxed(ES_Special_Mutable_Access::Make(context, function, NULL));

    ES_CollectorLock gclock(context);

    return PUT_CANNOT_CACHE(ES_Indexed_Properties::PutL(context, self, index, value, self));
}

/* static */ PutResult
ES_Indexed_Properties::DefineSetterL(ES_Context *context, ES_Object *self, unsigned index, ES_Function *function)
{
    ES_Value_Internal value;
    ES_Indexed_Properties *properties = self->GetIndexedProperties();

    if (properties)
    {
        BOOL res;

        switch (ES_Indexed_Properties::GetType(properties))
        {
        default:
        case ES_Indexed_Properties::TYPE_COMPACT:
            if (index < static_cast<ES_Compact_Indexed_Properties *>(properties)->capacity)
                res = static_cast<ES_Compact_Indexed_Properties *>(properties)->Get(index, value);
            else
                res = FALSE;
            break;

        case ES_Indexed_Properties::TYPE_SPARSE:
            res = static_cast<ES_Sparse_Indexed_Properties *>(properties)->Get(index, value);
            break;

        case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
        case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
            return PROP_PUT_OK;
        }

        if (res)
            if (value.IsGetterOrSetter())
            {
                static_cast<ES_Special_Mutable_Access *>(value.GetDecodedBoxed())->setter = function;
                return PROP_PUT_OK;
            }
    }

    value.SetBoxed(ES_Special_Mutable_Access::Make(context, NULL, function));

    ES_CollectorLock gclock(context);

    return PUT_CANNOT_CACHE(ES_Indexed_Properties::PutL(context, self, index, value, self));
}

/* static */ void
ES_Indexed_Properties::LookupGetter(ES_Indexed_Properties *properties, unsigned index, ES_Function *&function)
{
    function = NULL;

    if (HasGettersOrSetters(properties))
    {
        ES_Value_Internal value;
        BOOL res;

        switch (ES_Indexed_Properties::GetType(properties))
        {
        default:
        case ES_Indexed_Properties::TYPE_COMPACT:
            if (index < static_cast<ES_Compact_Indexed_Properties *>(properties)->capacity)
                res = static_cast<ES_Compact_Indexed_Properties *>(properties)->Get(index, value);
            else
                res = FALSE;
            break;

        case ES_Indexed_Properties::TYPE_SPARSE:
            res = static_cast<ES_Sparse_Indexed_Properties *>(properties)->Get(index, value);
            break;

        case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
        case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
            return;
        }

        if (res && value.IsGetterOrSetter())
            function = static_cast<ES_Special_Mutable_Access *>(value.GetDecodedBoxed())->getter;
    }
}

/* static */ void
ES_Indexed_Properties::LookupSetter(ES_Indexed_Properties *properties, unsigned index, ES_Function *&function)
{
    function = NULL;

    if (HasGettersOrSetters(properties))
    {
        ES_Value_Internal value;
        BOOL res;

        switch (ES_Indexed_Properties::GetType(properties))
        {
        default:
        case ES_Indexed_Properties::TYPE_COMPACT:
            if (index < static_cast<ES_Compact_Indexed_Properties *>(properties)->capacity)
                res = static_cast<ES_Compact_Indexed_Properties *>(properties)->Get(index, value);
            else
                res = FALSE;
            break;

        case ES_Indexed_Properties::TYPE_SPARSE:
            res = static_cast<ES_Sparse_Indexed_Properties *>(properties)->Get(index, value);
            break;

        case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
        case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
            return;
        }

        if (res && value.IsGetterOrSetter())
            function = static_cast<ES_Special_Mutable_Access *>(value.GetDecodedBoxed())->setter;
    }
}

/* static */ BOOL
ES_Indexed_Properties::RenumberSpecialL(ES_Context *context, ES_Object *this_object, unsigned index, unsigned length, int delta)
{
    unsigned current_index, previous_index, udelta;
    ES_Value_Internal value;
    ES_Indexed_Properties *&properties = this_object->GetIndexedPropertiesRef();
    ES_Indexed_Property_Iterator iterator(context, this_object, properties);

    if (delta > 0)
    {
        udelta = static_cast<unsigned>(delta);

        OP_ASSERT(UINT_MAX - udelta > index + length - 1);

        /* if delta is greater than 0 we need to move elements starting from the back
           to make sure that we don't overwrite anything, i.e. previous_index >= current_index*/
        previous_index = index + length;

        for (BOOL has_previous = iterator.UpperBound(current_index, index + length - 1);
             current_index >= index  && has_previous;
             has_previous = iterator.Previous(current_index))
        {
            /* if there was a hole, delete the element at the previous index */
            if (previous_index - current_index > 1)
                ES_Indexed_Properties::DeleteL(context, this_object, properties, previous_index);

            if (!iterator.GetValue(value))
                return FALSE;

            if (!PUT_OK(ES_Indexed_Properties::PutL(context, this_object, current_index + udelta, value, this_object)))
                return FALSE;

            previous_index = current_index;
        }
    }
    else if (BOOL has_next = iterator.LowerBound(current_index, index))
    {
        udelta = static_cast<unsigned>(-delta);

        OP_ASSERT(udelta <= index);

        /* if delta is less than 0 we need to move elements starting from the front
           to make sure that we don't overwrite anything, i.e. previous_index <= current_index*/
        previous_index = index;

        for (; current_index < index + length && has_next; has_next = iterator.Next(current_index))
        {
            /* if there was a hole, delete the element at the previous index */
            if (current_index - previous_index > 1)
                ES_Indexed_Properties::DeleteL(context, this_object, properties, previous_index);

            if (!iterator.GetValue(value))
                return FALSE;

            if (!PUT_OK(ES_Indexed_Properties::PutL(context, this_object, current_index - udelta, value, this_object)))
                return FALSE;

            /* NOTE: This makes renumbering degrade to O(n*log(n)) for the sparse representation.
               It's needed since ES_Indexed_Properties::PutL could be a special property potentially
               calling other code. If this is a problem then there are ways to fix it. */
            iterator.FlushCache();

            previous_index = current_index;
        }
    }

    return TRUE;
}

/* static */ BOOL
ES_Indexed_Properties::RenumberFromPrototypeL(ES_Execution_Context *context, ES_Object *this_object, unsigned index, unsigned length, int delta, unsigned array_length)
{
    ES_Object *prototype = this_object->Class()->Prototype();
    ES_Indexed_Properties *&properties = this_object->GetIndexedPropertiesRef();
    ES_Array_Property_Iterator iterator(context, prototype, array_length);
    unsigned pindex;

    GC_STACK_ANCHOR(context, this_object);

    ES_CollectorLock gclock(context, TRUE);

    while (iterator.Next(pindex))
        if (pindex >= index && pindex < index + length)
        {
            pindex += delta;

            if (!ES_Indexed_Properties::HasProperty(properties, pindex))
            {
                ES_Value_Internal *value;

                properties = PutL(context, properties, pindex, value);

                if (!iterator.GetValue(*value))
                {
                    value->SetUndefined();

                    DeleteL(context, this_object, properties, pindex);

                    return FALSE;
                }

                if (value->IsUndefined())
                    value->SetUndefined(TRUE);
            }
        }

    return TRUE;
}

/* static */ ES_Indexed_Properties *
ES_Indexed_Properties::ChangeAttribute(ES_Context *context, ES_Indexed_Properties *properties, unsigned index, const ES_Property_Info &info)
{
    if (properties)
    {
        switch (GetType(properties))
        {
        case TYPE_COMPACT:
        {
            ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(properties);

            OP_ASSERT(compact->IsUsed(index));

            if (!compact->HasAttributes())
            {
                unsigned capacity = compact->Capacity();
                ES_Compact_Indexed_Properties *new_compact = ES_Compact_Indexed_Properties::Make(context, capacity, capacity, TRUE);

                op_memcpy(new_compact->values, compact->values, capacity * sizeof(ES_Value_Internal));

                if (compact->HasSpecialProperties())
                    new_compact->SetHasSpecialProperties();

                properties = compact = new_compact;
            }

            compact->SetAttributes(index, info.Attributes());
        }
        break;

        case TYPE_SPARSE:
            ES_Sparse_Indexed_Properties::Node *node, *parent;
            static_cast<ES_Sparse_Indexed_Properties *>(properties)->Find(index, node, parent);
            node->attributes = info.Attributes();
        }

        if (info.IsReadOnly())
            properties->SetHasReadOnlyProperties();
    }

    return properties;
}

/* static */ ES_Compact_Indexed_Properties *
ES_Compact_Indexed_Properties::Make(ES_Context *context, unsigned capacity, unsigned initialize_from, BOOL with_attributes)
{
    ES_Compact_Indexed_Properties *properties;
    unsigned attributes_size;

    if (with_attributes)
        attributes_size = AttributesSize(capacity);
    else
        attributes_size = 0;

    if (capacity >= (ES_LIM_OBJECT_SIZE - sizeof(ES_Compact_Indexed_Properties) - attributes_size) / sizeof(ES_Value_Internal))
        context->AbortOutOfMemory();

    GC_ALLOCATE_WITH_EXTRA(context, properties, AllocationSize(capacity) - sizeof(ES_Compact_Indexed_Properties) + attributes_size, ES_Compact_Indexed_Properties, (properties, capacity));

    for (unsigned index = initialize_from; index < capacity; ++index)
        properties->values[index].SetUndefined(FALSE);

    if (with_attributes)
    {
        properties->SetHasAttributes();
        op_memset(properties->GetAttributes(), 0, attributes_size);
    }

    return properties;
}

/* static */ ES_Compact_Indexed_Properties *
ES_Compact_Indexed_Properties::Make(ES_Context *context, ES_Code *code, ES_CodeStatic::ConstantArrayLiteral &cal)
{
    ES_Compact_Indexed_Properties *properties;
    unsigned capacity = AppropriateCapacity(cal.array_length);

    GC_ALLOCATE_WITH_EXTRA(context, properties, AllocationSize(capacity) - sizeof(ES_Compact_Indexed_Properties), ES_Compact_Indexed_Properties, (properties, capacity));

    unsigned empty_index = 0;
    for (unsigned index = 0; index < cal.elements_count; ++index)
    {
        unsigned element_index = cal.indeces[index];

        while (empty_index < element_index)
            properties->values[empty_index++].SetUndefined(FALSE);

        ES_Value_Internal value;

        switch (cal.values[index].type)
        {
        case ESTYPE_NULL:
            value.SetNull();
            break;

        case ESTYPE_UNDEFINED:
            value.SetUndefined();
            break;

        case ESTYPE_BOOLEAN:
            value.SetBoolean(cal.values[index].value != 0);
            break;

        case ESTYPE_INT32:
            value.SetInt32(cal.values[index].value);
            break;

        case ESTYPE_DOUBLE:
            value.SetDouble(code->data->doubles[cal.values[index].value]);
            break;

        case ESTYPE_STRING:
            value.SetString(code->GetString(cal.values[index].value));
            break;
        }

        properties->PutSimpleNew(element_index, value);

        empty_index = element_index + 1;
    }

    while (empty_index < cal.array_length)
        properties->values[empty_index++].SetUndefined(FALSE);

    properties->SetNeedsCopyOnWrite();
    return properties;
}

ES_Indexed_Properties *
ES_Compact_Indexed_Properties::GrowAndPutL(ES_Context *context, unsigned new_index, unsigned *new_attributes, ES_Value_Internal *&new_value)
{
    BOOL convert_to_sparse = FALSE;
    unsigned new_capacity = capacity;

#ifdef ES_MOVABLE_OBJECTS
    ES_CollectorLock gclock(context, TRUE);
#endif // ES_MOVABLE_OBJECTS

    if (new_index >= INT_MAX)
        convert_to_sparse = TRUE;
    else
    {
        if (capacity >= 131072)
        {
            unsigned found = 0, searched = 0;

            for (unsigned start = 16; start < capacity; start += 1024, searched += 16)
                for (unsigned index = start - 16; index < start; ++index)
                    if (IsUsed(index))
                        ++found;

            if (found * 4 < searched)
                convert_to_sparse = TRUE;
        }

        if (!convert_to_sparse)
        {
            while (new_index >= new_capacity)
                new_capacity += new_capacity;

            if (new_capacity > ES_ARRAY_ALWAYS_COMPACT_LIMIT && new_capacity / capacity > 2)
                convert_to_sparse = TRUE;
        }
    }

    if (convert_to_sparse)
    {
        ES_Sparse_Indexed_Properties *sparse = MakeSparse(context);
        if (sparse)
        {
#ifndef ES_MOVABLE_OBJECTS
            ES_CollectorLock gclock(context);
#endif // ES_MOVABLE_OBJECTS

            sparse->PutL(context, new_index, new_attributes, new_value);
        }
        else
        {
            sparse = ES_Sparse_Indexed_Properties::Make(context);
#ifndef ES_MOVABLE_OBJECTS
            ES_CollectorLock gclock(context);
#endif // ES_MOVABLE_OBJECTS

            sparse->PutFirst(context, new_index, new_attributes, new_value);
        }
        return sparse;
    }
    else
    {
        ES_Compact_Indexed_Properties *new_representation = ES_Compact_Indexed_Properties::Make(context, new_capacity, capacity, new_attributes && *new_attributes != 0 || HasAttributes());

        op_memcpy(new_representation->values, values, capacity * sizeof(ES_Value_Internal));
        new_value = &new_representation->values[new_index];

        if (HasSpecialProperties())
            new_representation->SetHasSpecialProperties();
        if (HasReadOnlyProperties() || new_attributes && (*new_attributes & RO) == RO)
            new_representation->SetHasReadOnlyProperties();
        if (HasAttributes())
            op_memcpy(new_representation->GetAttributes(), GetAttributes(), AttributesSize(capacity));

        if (new_attributes && *new_attributes != 0)
            new_representation->SetAttributes(new_index, *new_attributes);

        return new_representation;
    }
}

ES_Indexed_Properties *
ES_Compact_Indexed_Properties::DeleteL(ES_Context *context, unsigned delete_index, BOOL &result)
{
    if (delete_index < capacity && IsUsed(delete_index))
    {
        if ((GetAttributes(delete_index) & DD) == DD)
        {
            result = FALSE;
            return this;
        }

        result = TRUE;

        if (NeedsCopyOnWrite())
            goto reallocate;

        values[delete_index].SetUndefined(FALSE);

        if (capacity > ES_ARRAY_ALWAYS_COMPACT_LIMIT && delete_index == capacity / 2)
        {
        reallocate:
            unsigned new_capacity = capacity / 2;

            /* Cut array in half if top half is now empty. */
            for (unsigned index = capacity - 1, stop = capacity / 2; index >= stop; --index)
                if (IsUsed(index))
                    if (NeedsCopyOnWrite())
                    {
                        new_capacity = capacity;
                        break;
                    }
                    else
                        return this;

            ES_Compact_Indexed_Properties *new_representation = ES_Compact_Indexed_Properties::Make(context, new_capacity);

            op_memcpy(new_representation->values, values, new_capacity * sizeof(ES_Value_Internal));

            if (new_capacity > delete_index)
                new_representation->values[delete_index].SetUndefined(FALSE);

            if (HasSpecialProperties())
                new_representation->SetHasSpecialProperties();
            if (HasReadOnlyProperties())
                new_representation->SetHasReadOnlyProperties();

            return new_representation;
        }
    }

    return this;
}

ES_Indexed_Properties *
ES_Compact_Indexed_Properties::Share(ES_Context *context)
{
    if (NeedsCopyOnWrite())
    {
        ES_Compact_Indexed_Properties *new_representation = ES_Compact_Indexed_Properties::Make(context, capacity);

        op_memcpy(new_representation->values, values, capacity * sizeof(ES_Value_Internal));

        if (HasSpecialProperties())
            new_representation->SetHasSpecialProperties();
        if (HasReadOnlyProperties())
            new_representation->SetHasReadOnlyProperties();

        return new_representation;
    }
    else
        return this;
}

void
ES_Compact_Indexed_Properties::Truncate(unsigned start, unsigned &end)
{
    OP_ASSERT(start <= end);

    if (start < capacity)
    {
        end = es_minu(end, capacity);

        if (!HasAttributes())
        {
            ES_Value_Internal *ptr = values + start;
            ES_Value_Internal *ptr_end = values + end;

            while (ptr != ptr_end)
            {
                ptr->SetUndefined(FALSE);
                ++ptr;
            }

            end = start;
        }
        else if (start < end)
            do
                if ((GetAttributes(end - 1) & DD) == DD)
                    return;
                else
                    values[--end].SetUndefined(FALSE);
            while (end != start);
    }
    else
        /* None of the "truncated" properties actually exist. */
        end = start;
}

ES_Indexed_Properties *
ES_Compact_Indexed_Properties::RenumberL(ES_Context *context, unsigned index, unsigned length, int delta)
{
    if (index >= capacity)
        return this;

    if (delta < 0)
    {
        unsigned udelta = static_cast<unsigned>(-delta), tail;

        length = es_minu(length, capacity - index);

        /* Find highest used index after the elements we're renumbering. */
        for (tail = capacity - 1; tail >= index + length; --tail)
            if (IsUsed(tail))
                break;

        if (tail < index + length && capacity - (index + length) >= udelta)
        {
            unsigned new_capacity = capacity;

            while (new_capacity / 2 >= index + length)
                new_capacity /= 2;

            if (new_capacity != capacity)
            {
                ES_Compact_Indexed_Properties *new_representation = ES_Compact_Indexed_Properties::Make(context, new_capacity, (index + length) - udelta);

                op_memcpy(new_representation->values, values, (index - udelta) * sizeof(ES_Value_Internal));
                op_memcpy(new_representation->values + (index - udelta), values + index, length * sizeof(ES_Value_Internal));

                if (HasSpecialProperties())
                    new_representation->SetHasSpecialProperties();
                if (HasReadOnlyProperties())
                    new_representation->SetHasReadOnlyProperties();

                return new_representation;
            }
            else
            {
                /* There's nothing above the moved segment, and there are enough
                   currently unused elements after it that we can simply copy
                   those unused elements on top of elements that now become
                   unused. */
                op_memmove(values + index - udelta, values + index, (length + udelta) * sizeof(ES_Value_Internal));
            }
        }
        else
        {
            op_memmove(values + index - udelta, values + index, length * sizeof(ES_Value_Internal));

            for (unsigned newly_unused = index + length - udelta; newly_unused < index + length; ++newly_unused)
                values[newly_unused].SetUndefined(FALSE);
        }
    }
    else
    {
        unsigned actual_length = es_minu(capacity - index, length);

        if (actual_length == capacity - index)
            while (actual_length > 0 && !IsUsed(index + actual_length - 1))
                --actual_length;

        unsigned udelta = static_cast<unsigned>(delta), needed_capacity = index + actual_length + udelta;

        if (needed_capacity > capacity)
        {
            /* Need to grow. */
            unsigned new_capacity = capacity;

            while (needed_capacity > new_capacity)
                new_capacity += new_capacity;

            ES_Compact_Indexed_Properties *new_representation = ES_Compact_Indexed_Properties::Make(context, new_capacity);

            op_memcpy(new_representation->values, values, index * sizeof(ES_Value_Internal));

            for (ES_Value_Internal *newly_unused = new_representation->values + index, *stop = newly_unused + udelta; newly_unused != stop; ++newly_unused)
                newly_unused->SetUndefined(FALSE);

            op_memcpy(new_representation->values + index + udelta, values + index, actual_length * sizeof(ES_Value_Internal));

            if (HasSpecialProperties())
                new_representation->SetHasSpecialProperties();
            if (HasReadOnlyProperties())
                new_representation->SetHasReadOnlyProperties();

            return new_representation;
        }
        else
        {
            op_memmove(values + index + udelta, values + index, actual_length * sizeof(ES_Value_Internal));

            for (ES_Value_Internal *newly_unused = values + index, *stop = newly_unused + udelta; newly_unused != stop; ++newly_unused)
                newly_unused->SetUndefined(FALSE);
        }
    }

    return this;
}

unsigned
ES_Compact_Indexed_Properties::GetUsed()
{
    unsigned used = 0;

    for (unsigned index = 0; index < capacity; ++index)
        if (IsUsed(index))
            ++used;

    return used;
}

BOOL
ES_Compact_Indexed_Properties::HasUsed()
{
    for (unsigned index = 0; index < capacity; ++index)
        if (IsUsed(index))
            return TRUE;

    return FALSE;
}

ES_Sparse_Indexed_Properties *
ES_Compact_Indexed_Properties::MakeSparse(ES_Context *context)
{
    ES_Value_Internal *value;

    for (unsigned index = 0; index < capacity; ++index)
        if (IsUsed(index))
        {
            ES_Sparse_Indexed_Properties *new_representation = ES_Sparse_Indexed_Properties::Make(context);
            ES_CollectorLock gclock(context);

            unsigned attributes = GetAttributes(index);

            new_representation->PutFirst(context, index, &attributes, value);
            *value = values[index];

            for (++index; index < capacity; ++index)
                if (IsUsed(index))
                {
                    attributes = GetAttributes(index);
                    new_representation->PutL(context, index, &attributes, value, TRUE);
                    *value = values[index];
                }

            if (HasSpecialProperties())
                new_representation->SetHasSpecialProperties();
            if (HasReadOnlyProperties())
                new_representation->SetHasReadOnlyProperties();

            return new_representation;
        }

    return NULL;
}

void
ES_Compact_Indexed_Properties::ClearAndPut(unsigned clear_from_index, unsigned put_index, const ES_Value_Internal &value)
{
    for (unsigned index = clear_from_index; index < put_index; ++index)
        values[index].SetUndefined(FALSE);

    values[put_index] = value;
}

/* static */ ES_Sparse_Indexed_Properties *
ES_Sparse_Indexed_Properties::Make(ES_Context *context)
{
    ES_Sparse_Indexed_Properties *properties;

    GC_ALLOCATE(context, properties, ES_Sparse_Indexed_Properties, (properties));

    ES_CollectorLock gclock(context);
    properties->AllocateBlock(context);

    return properties;
}

BOOL
ES_Sparse_Indexed_Properties::Get(unsigned index, ES_Value_Internal &value)
{
    Node *node = root;

    while (node)
        if (index == node->index)
        {
            value = node->value;
            return TRUE;
        }
        else if (index < node->index)
            node = node->less;
        else
            node = node->greater;

    value.SetUndefined();
    return FALSE;
}

ES_Indexed_Properties *
ES_Sparse_Indexed_Properties::PutL(ES_Context *context, unsigned new_index, unsigned *attributes, ES_Value_Internal *&new_value, BOOL force_sparse)
{
    Node *node = root, *parent;

    OP_ASSERT(used != 0);

    if (attributes && (*attributes & RO) == RO)
        SetHasReadOnlyProperties();

    while (TRUE)
        if (new_index == node->index)
        {
            new_value = &node->value;
            if (attributes)
                node->attributes = *attributes;
            return this;
        }
        else if (new_index < node->index)
        {
            if (node->less)
                node = node->less;
            else
            {
                parent = node;
                break;
            }
        }
        else if (node->greater)
            node = node->greater;
        else
        {
            parent = node;
            break;
        }

    ++used;

    if (!free)
    {
        if (!force_sparse)
        {
            node = root;

            while (node->greater)
                node = node->greater;

            unsigned highest_index = node->index;
            unsigned compact_capacity = ES_Compact_Indexed_Properties::AppropriateCapacity(es_maxu(highest_index, new_index) + 1);

            if ((compact_capacity / used) < ES_ARRAY_COMPACT_USAGE_LEVEL_LIMIT)
            {
                ES_Indexed_Properties *properties = this;
                ES_Indexed_Property_Iterator iter(context, NULL, properties);

                unsigned index;
                BOOL with_attributes = FALSE;

                while (!with_attributes && iter.Next(index))
                    if (iter.GetAttributes() != 0)
                        with_attributes = TRUE;

                iter.Reset();

                ES_Compact_Indexed_Properties *new_representation = ES_Compact_Indexed_Properties::Make(context, compact_capacity, 0, with_attributes);
                ES_Value_Internal *new_values = new_representation->values;

                while (iter.Next(index))
                {
                    iter.GetValue(new_values[index]);

                    if (new_values[index].IsUndefined())
                        new_values[index].SetUndefined(TRUE);

                    if (unsigned attributes = iter.GetAttributes())
                        new_representation->SetAttributes(index, attributes);
                }

                new_value = &new_values[new_index];

                if (HasSpecialProperties())
                    new_representation->SetHasSpecialProperties();
                if (HasReadOnlyProperties())
                    new_representation->SetHasReadOnlyProperties();

                return new_representation;
            }
        }

        AllocateBlock(context);
    }

    Node *new_node = free;
    free = free->less;

    if (new_index < parent->index)
        parent->less = new_node;
    else
        parent->greater = new_node;

    new_node->value.SetUndefined();
    new_node->parent = Node::Colorize(parent, Node::RED);
    new_node->less = new_node->greater = NULL;
    new_node->index = new_index;
    new_node->attributes = attributes ? *attributes : 0;
    new_value = &new_node->value;

    while (TRUE)
    {
        if (!parent)
        {
            new_node->parent = NULL; // actually: new_node->SetColor(Node::BLACK);
            break;
        }
        else if (Node::GetColor(parent) == Node::RED)
        {
            Node *grandparent = parent->GetParent();
            Node *uncle = parent->GetSibling();

            OP_ASSERT(grandparent != NULL); // guaranteed by 'parent' being red

            if (uncle && Node::GetColor(uncle) == Node::RED)
            {
                parent->parent = uncle->parent = grandparent; // actually: parent->SetColor(Node::BLACK); uncle->SetColor(Node::BLACK)
                grandparent->SetColor(Node::RED);
                new_node = grandparent;
                parent = grandparent->GetParent();
            }
            else
            {
                Node *n, *p;

                // "case 4"
                if (new_node == parent->greater && parent == grandparent->less)
                {
                    grandparent->less = new_node;

                    new_node->parent = grandparent;
                    parent->SetParent(new_node);

                    if ((parent->greater = new_node->less) != NULL)
                        parent->greater->SetParent(parent);
                    new_node->less = parent;

                    n = parent;
                    p = new_node;
                }
                else if (new_node == parent->less && parent == grandparent->greater)
                {
                    grandparent->greater = new_node;

                    new_node->parent = grandparent;
                    parent->SetParent(new_node);

                    if ((parent->less = new_node->greater) != NULL)
                        parent->less->SetParent(parent);
                    new_node->greater = parent;

                    n = parent;
                    p = new_node;
                }
                else
                {
                    parent->parent = grandparent; // actually: parent->SetColor(Node::BLACK);

                    n = new_node;
                    p = parent;
                }

                // "case 5"
                OP_ASSERT(n->GetParent() == p);
                OP_ASSERT(Node::GetColor(p) == Node::BLACK);

                grandparent->SetColor(Node::RED);

                Node *ggp = grandparent->GetParent(), **pgrandparent;
                if (!ggp)
                    pgrandparent = &root;
                else if (grandparent == ggp->less)
                    pgrandparent = &ggp->less;
                else
                    pgrandparent = &ggp->greater;

                *pgrandparent = p;

                grandparent->SetParent(p);
                p->parent = ggp;

                if (n == p->less && p == grandparent->less)
                {
                    if ((grandparent->less = p->greater) != NULL)
                        grandparent->less->SetParent(grandparent);
                    p->greater = grandparent;
                }
                else
                {
                    if ((grandparent->greater = p->less) != NULL)
                        grandparent->greater->SetParent(grandparent);
                    p->less = grandparent;
                }

                break;
            }
        }
        else
            break;
    }

    return this;
}

ES_Indexed_Properties *
ES_Sparse_Indexed_Properties::DeleteL(unsigned index, BOOL &result)
{
    Node *node, *dummy, *todelete;

    result = TRUE;

    if (Find(index, node, dummy))
    {
        if ((node->attributes & DD) == DD)
        {
            result = FALSE;
            return this;
        }

        --used;

        if (used == 0)
            return NULL;

        if (node->less || node->greater)
        {
            if (!node->less || node->greater && index - node->less->index < node->greater->index - index)
                todelete = node->greater->Minimum();
            else
                todelete = node->less->Maximum();

            node->index = todelete->index;
            node->value = todelete->value;
        }
        else
            todelete = node;

        OP_ASSERT(!todelete->less || !todelete->greater);

        Node *parent = todelete->GetParent();
        Node *child = todelete->less ? todelete->less : todelete->greater;

        if (child)
            if (todelete == parent->less)
                parent->less = child;
            else
                parent->greater = child;

        BOOL either_is_red = Node::GetColor(todelete) == Node::RED || Node::GetColor(child) == Node::RED;

        if (child)
        {
            child->parent = parent; // also sets child's color to black if it was red
            todelete->parent = NULL;
        }
        else
            child = todelete;

        if (!either_is_red)
        {
            Node *n = child;

            while ((parent = n->GetParent()) != NULL)
            {
                Node *sibling = n->GetSibling();
                Node *sl = sibling->less, *sg = sibling->greater;

                if (Node::GetColor(sibling) == Node::RED)
                {
                    sibling->parent = parent->GetParent(); // also: sibling->SetColor(Node::BLACK)
                    if (!sibling->parent)
                        root = sibling;
                    else if (parent == sibling->parent->less)
                        sibling->parent->less = sibling;
                    else
                        sibling->parent->greater = sibling;
                    parent->parent = Node::Colorize(sibling, Node::RED);

                    if (n == parent->less)
                    {
                        parent->greater = sl;
                        if (sl)
                            sl->parent = parent;
                        sibling->less = parent;
                        sibling = sl;
                        sl = sibling->less;
                        sg = sibling->greater;
                    }
                    else
                    {
                        parent->less = sg;
                        if (sg)
                            sg->parent = parent;
                        sibling->greater = parent;
                        sibling = sg;
                        sl = sibling->less;
                        sg = sibling->greater;
                    }

                    OP_ASSERT(parent == n->GetParent());
                    OP_ASSERT(sibling == n->GetSibling());
                    OP_ASSERT(sl == sibling->less);
                    OP_ASSERT(sg == sibling->greater);
                }

                if (Node::GetColor(sibling) == Node::BLACK)
                {
                    if (Node::GetColor(sl) == Node::BLACK &&
                        Node::GetColor(sg) == Node::BLACK)
                    {
                        sibling->SetColor(Node::RED);

                        if (Node::GetColor(parent) == Node::BLACK)
                        {
                            n = parent;
                            continue;
                        }

                        parent->SetColor(Node::BLACK);
                        break;
                    }
                    else
                    {
                        if (n == parent->less &&
                            Node::GetColor(sl) == Node::RED &&
                            Node::GetColor(sg) == Node::BLACK)
                        {
                            sl->parent = sibling->parent; // also sets sl's color to black
                            if (sibling == sibling->parent->less)
                                sibling->parent->less = sl;
                            else
                                sibling->parent->greater = sl;
                            sibling->parent = Node::Colorize(sl, Node::RED);
                            sibling->less = sl->greater;
                            if (sibling->less)
                                sibling->less->parent = sibling;
                            sl->greater = sibling;
                            sibling = sl;
                        }
                        else if (n == parent->greater &&
                                 Node::GetColor(sl) == Node::BLACK &&
                                 Node::GetColor(sg) == Node::RED)
                        {
                            sg->parent = sibling->parent; // also sets sg's color to black
                            if (sibling == sibling->parent->less)
                                sibling->parent->less = sg;
                            else
                                sibling->parent->greater = sg;
                            sibling->parent = Node::Colorize(sg, Node::RED);
                            sibling->greater = sg->less;
                            if (sibling->greater)
                                sibling->greater->parent = sibling;
                            sg->less = sibling;
                            sibling = sg;
                        }
                    }

                    OP_ASSERT(parent == n->GetParent());
                    OP_ASSERT(sibling == n->GetSibling());
                    OP_ASSERT(Node::GetColor(sibling) == Node::BLACK);
                }

                if (n == parent->less)
                {
                    sibling->greater->parent = sibling; // actually: sibling->greater->SetColor(Node::BLACK)
                    sibling->parent = parent->parent; // also sets sibling's color to same as parent's
                    if (!sibling->parent)
                        root = sibling;
                    else if (parent == sibling->GetParent()->less)
                        sibling->GetParent()->less = sibling;
                    else
                        sibling->GetParent()->greater = sibling;
                    parent->parent = sibling; // also sets parent's color to black
                    parent->greater = sibling->less;
                    if (parent->greater)
                        parent->greater->SetParent(parent);
                    sibling->less = parent;
                }
                else
                {
                    sibling->less->parent = sibling; // actually: sibling->greater->SetColor(Node::BLACK)
                    sibling->parent = parent->parent; // also sets sibling's color to same as parent's
                    if (!sibling->parent)
                        root = sibling;
                    else if (parent == sibling->GetParent()->less)
                        sibling->GetParent()->less = sibling;
                    else
                        sibling->GetParent()->greater = sibling;
                    parent->parent = sibling; // also sets parent's color to black
                    parent->less = sibling->greater;
                    if (parent->less)
                        parent->less->SetParent(parent);
                    sibling->greater = parent;
                }

                break;
            }
        }

        if (Node *parent = todelete->GetParent())
        {
            OP_ASSERT(!todelete->less && !todelete->greater);
            OP_ASSERT(todelete == parent->less || todelete == parent->greater);

            if (todelete == parent->less)
                parent->less = NULL;
            else
                parent->greater = NULL;
        }
        else if (used == 0)
            root = NULL;

        FreeNode(todelete);
    }

    return this;
}

ES_Indexed_Properties *
ES_Sparse_Indexed_Properties::TruncateL(ES_Context *context, unsigned start, unsigned &end)
{
    ES_Indexed_Properties *properties = this;
    ES_Indexed_Property_Iterator iter(context, NULL, properties);
    unsigned index, left = 0, last = 0, truncate_below = end;

    if (iter.Next(index) && index < start)
        do
        {
            last = index;
            ++left;
        }
        while (iter.Next(index) && index < start);

    iter.Reset();

    while (iter.Previous(index) && index >= start)
    {
        if (!iter.IsConfigurable())
        {
            if (index > last)
                last = index;
        }
        if (index <= last)
            ++left;
    }

    end = es_maxu(start, last);

    if (left == 0)
        return NULL;
    else
    {
        unsigned compact_capacity = ES_Compact_Indexed_Properties::AppropriateCapacity(last + 1);

        if (last < ES_ARRAY_ALWAYS_COMPACT_LIMIT || (left << 1) > compact_capacity)
        {
            ES_Compact_Indexed_Properties *compact = ES_Compact_Indexed_Properties::Make(context, compact_capacity, last + 1);
            ES_CollectorLock gclock(context);

            iter.Reset();
            iter.Next(index);

            do
            {
                ES_Value_Internal &value = compact->values[index];
                iter.GetValue(value);
                if (value.IsUndefined())
                    value.SetUndefined(TRUE);
            }
            while (iter.Next(index) && index < start);

            if (HasSpecialProperties())
                compact->SetHasSpecialProperties();
            if (HasReadOnlyProperties())
                compact->SetHasReadOnlyProperties();

            return compact;
        }
        else
        {
            iter.Reset();

            if (iter.LowerBound(index, start) && index < truncate_below)
                do
                    iter.DeleteL(context);
                while (iter.Next(index) && index < truncate_below);
        }

        return properties;
    }
}

void
ES_Sparse_Indexed_Properties::Renumber(unsigned index, unsigned length, int delta)
{
    Node *iter = root;

    while (TRUE)
        if (index == iter->index)
            break;
        else if (index < iter->index)
            if (iter->less)
                iter = iter->less;
            else
                break;
        else if (iter->greater)
            iter = iter->greater;
        else
        {
            while (Node *parent = iter->GetParent())
                if (index > parent->index)
                    iter = parent;
                else
                    break;
            break;
        }

    OP_ASSERT(index <= iter->index);

    while (iter && iter->index < index + length)
    {
        Node *next = iter->GetNext();
        iter->index += delta;
        iter = next;
    }

#ifdef RBTREE_DEBUG
    Check();
#endif // _DEBUG
}

void
ES_Sparse_Indexed_Properties::AllocateBlock(ES_Context *context)
{
    if ((blocks_used & (blocks_used - 1)) == 0)
    {
        unsigned new_blocks_allocated = blocks_used == 0 ? 8 : blocks_used + blocks_used;
        ES_Box *new_blocks = ES_Box::Make(context, new_blocks_allocated * sizeof(ES_Box *));
        op_memcpy(new_blocks->Unbox(), blocks->Unbox(), blocks_used * sizeof(ES_Box *));
        blocks = new_blocks;
    }

    ES_Box *box = reinterpret_cast<ES_Box **>(blocks->Unbox())[blocks_used] = ES_Box::Make(context, NODES_PER_BLOCK * sizeof(Node));
    ++blocks_used;

    Node *nodes = free = reinterpret_cast<Node *>(box->Unbox());

    for (unsigned index = 0; index < NODES_PER_BLOCK - 1; ++index)
    {
        nodes[index].index = UINT_MAX;
        nodes[index].less = &nodes[index + 1];
    }

    nodes[NODES_PER_BLOCK - 1].index = UINT_MAX;
    nodes[NODES_PER_BLOCK - 1].less = NULL;
}

#ifdef RBTREE_DEBUG

BOOL
ES_Sparse_Indexed_Properties::Check()
{
    if (Node::GetColor(root) != Node::BLACK)
    {
        printf("root not black\n");
        return FALSE;
    }

    if (root)
    {
        Node *iter = root;
        unsigned black_count = 0;

        while (iter)
        {
            if (Node::GetColor(iter) == Node::BLACK)
                ++black_count;
            iter = iter->less;
        }

        return root->RecursiveCheck(black_count, 0);
    }
    else
        return TRUE;
}

BOOL
ES_Sparse_Indexed_Properties::Node::RecursiveCheck(unsigned black_count, unsigned black_above)
{
    if (GetColor(this) == Node::RED)
    {
        if (GetColor(less) == Node::RED || GetColor(greater) == Node::RED)
        {
            printf("red node has red child\n");
            return FALSE;
        }
    }
    else
        ++black_above;

    if (less)
    {
        if (less->GetParent() != this)
        {
            printf("invalid tree: node has wrong parent\n");
            return FALSE;
        }
        else if (less->index >= index)
        {
            printf("invalid tree: less is more\n");
            return FALSE;
        }
        else if (!less->RecursiveCheck(black_count, black_above))
            return FALSE;
    }

    if (greater)
    {
        if (greater->GetParent() != this)
        {
            printf("invalid tree: node has wrong parent\n");
            return FALSE;
        }
        else if (greater->index <= index)
        {
            printf("invalid tree: more is less\n");
            return FALSE;
        }
        else if (!greater->RecursiveCheck(black_count, black_above))
            return FALSE;
    }

    if ((!less || !greater) && black_above != black_count)
    {
        printf("paths has different black count\n");
        return FALSE;
    }

    return TRUE;
}

#endif // RBTREE_DEBUG

/* static */ ES_Byte_Array_Indexed *
ES_Byte_Array_Indexed::Make(ES_Context *context, unsigned capacity, unsigned char *bytes)
{
    ES_Byte_Array_Indexed *array;
    GC_ALLOCATE(context, array, ES_Byte_Array_Indexed, (array, capacity, NULL));

    if (bytes)
        array->byte_array = bytes;
    else if (ES_Execution_Context *exec_context = context->GetExecutionContext())
    {
        ES_SuspendedMalloc call(capacity);
        exec_context->SuspendedCall(&call);
        array->byte_array = static_cast<unsigned char *>(call.storage);
    }
    else
        array->byte_array = static_cast<unsigned char *>(op_malloc(capacity));

    context->heap->RecordExternalBytes(capacity);
    return array->byte_array ? array : NULL;
}

BOOL
ES_Byte_Array_Indexed::PutL(ES_Context *context0, unsigned index, const ES_Value_Internal &value)
{
    if (index < capacity)
    {
        int ivalue;

        if (value.IsInt32())
            ivalue = value.GetInt32();
        else if (value.IsDouble())
            ivalue = value.GetNumAsInt32();
        else if (ES_Execution_Context *context = context0->GetExecutionContext())
        {
            ES_Value_Internal *temporary = context->AllocateRegisters(1);

            *temporary = value;

            if (!temporary->ToNumber(context))
            {
                context->FreeRegisters(1);
                return FALSE;
            }

            ivalue = temporary->GetNumAsInt32();

            context->FreeRegisters(1);
        }
        else
            return TRUE;

        Storage()[index] = static_cast<unsigned char>(ivalue);
    }

    return TRUE;
}

/* static */ ES_Type_Array_Indexed *
ES_Type_Array_Indexed::Make(ES_Context *context, unsigned byte_offset, unsigned capacity, TypeArrayKind kind, ES_Byte_Array_Indexed *byte_array)
{
    ES_Type_Array_Indexed *array;
    GC_ALLOCATE(context, array, ES_Type_Array_Indexed, (array, byte_offset, capacity, kind, byte_array));
    return array;
}

BOOL
ES_Type_Array_Indexed::PutL(ES_Context *context0, unsigned index, const ES_Value_Internal &value)
{
    if (index < capacity)
        if (ES_Execution_Context *context = context0->GetExecutionContext())
        {
            ES_Value_Internal *temporary = context->AllocateRegisters(1);
            *temporary = value;
            if (!temporary->ToNumber(context))
            {
                context->FreeRegisters(1);
                return FALSE;
            }

            unsigned char *values = Storage();
            unsigned val;
            if (kind == Uint8ClampedArray)
                val = temporary->GetNumAsUint8Clamped();
            else
                val = temporary->GetNumAsUInt32();

            switch (kind)
            {
            case Int8Array:
                reinterpret_cast<signed char *>(values)[index] = static_cast<signed char>(val);
                break;
            case Uint8Array:
            case Uint8ClampedArray:
                reinterpret_cast<unsigned char *>(values)[index] = static_cast<unsigned char>(val);
                break;
            case Int16Array:
                reinterpret_cast<signed short *>(values)[index] = static_cast<signed short>(val);
                break;
            case Uint16Array:
                reinterpret_cast<unsigned short *>(values)[index] = static_cast<unsigned short>(val);
                break;
            case Int32Array:
                reinterpret_cast<int *>(values)[index] = static_cast<int>(val);
                break;
            case Uint32Array:
                reinterpret_cast<unsigned *>(values)[index] = val;
                break;
            case Float32Array:
                reinterpret_cast<float *>(values)[index] = static_cast<float>(temporary->GetNumAsDouble());
                break;
            case Float64Array:
                reinterpret_cast<double *>(values)[index] = temporary->GetNumAsDouble();
                break;
            }
            context->FreeRegisters(1);
        }

    return TRUE;
}

BOOL
ES_Indexed_Property_Iterator::Previous(unsigned &index_out)
{
    if (properties && index != 0)
    {
        switch (ES_Indexed_Properties::GetType(properties))
        {
        default:
        case ES_Indexed_Properties::TYPE_COMPACT:
        {
            ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(properties);

            unsigned index_iter;
            if (index == UINT_MAX || index >= compact->capacity)
                index_iter = compact->capacity - 1;
            else
                index_iter = index - 1;

            do
                if (compact->IsUsed(index_iter) && (!skip_dont_enum || (compact->GetAttributes(index_iter) & DE) != DE))
                {
                    index = index_iter;
                    goto found;
                }
            while (index_iter-- != 0);

            index = 0;
            return FALSE;
        }

        case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
        {
            ES_Byte_Array_Indexed *byte_array = static_cast<ES_Byte_Array_Indexed *>(properties);

            if (index == UINT_MAX || index >= byte_array->Capacity())
                index = byte_array->Capacity() - 1;
            else
                index = index - 1;

            goto found;
        }

        case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
        {
            ES_Type_Array_Indexed *type_array = static_cast<ES_Type_Array_Indexed *>(properties);

            if (index == UINT_MAX || index >= type_array->Capacity())
                index = type_array->Capacity() - 1;
            else
                index = index - 1;

            goto found;
        }

        case ES_Indexed_Properties::TYPE_SPARSE:
            ES_Sparse_Indexed_Properties *sparse = static_cast<ES_Sparse_Indexed_Properties *>(properties);

            do
            {
                ES_Sparse_Indexed_Properties::Node *node = cached_node;

                if (index-- == 0)
                    return FALSE;

                if (node)
                {
                find_previous:
                    OP_ASSERT(node->index > index);
                    if (ES_Sparse_Indexed_Properties::Node *previous = node->GetPrevious())
                    {
                        OP_ASSERT(previous->index <= index);
                        cached_node = previous;
                        index = previous->index;
                    }
                    else
                    {
                        index = UINT_MAX;
                        return FALSE;
                    }
                }
                else
                {
                    node = sparse->root;

                    while (TRUE)
                        if (node->index == index || node->index < index && !node->greater)
                        {
                            cached_node = node;
                            index = node->index;
                            goto found;
                        }
                        else if (node->index > index)
                            if (node->less)
                                node = node->less;
                            else
                                goto find_previous;
                        else
                            node = node->greater;
                }
            }
            while (skip_dont_enum && (cached_node->attributes & DE) == DE);
        }
    }
    else
    {
        if (object)
            if (object->IsStringObject())
            {
                if (index == UINT_MAX)
                {
                    int slen = Length(static_cast<ES_String_Object *>(object)->GetValue());
                    if (slen == 0)
                        return FALSE;
                    index = slen - 1;
                }
                else
                {
                    if (index <= 0)
                    {
                        index = UINT_MAX;
                        return FALSE;
                    }
                    index--;
                }
                goto found;
            }
            else if (object->IsHostObject())
            {
                ES_Host_Object *host_object = static_cast<ES_Host_Object *>(object);
                /* TODO: consider having a method that asks host object for the next/prev indexed
                   property instead. Reduces engine<->host object traffic. */
                unsigned capacity = host_object->GetIndexedPropertiesLength(context);

                if (capacity != 0)
                {
                    unsigned index_iter = (index == UINT_MAX ? capacity : index) - 1;

                    do
                        if (object->HasProperty(context, index_iter))
                        {
                            index = index_iter;
                            goto found;
                        }
                    while (index_iter-- != 0);

                    index = 0;
                    return FALSE;
                }
            }

        return FALSE;
    }

found:
    index_out = index;
    return TRUE;
}

BOOL
ES_Indexed_Property_Iterator::Next(unsigned &index_out)
{
    if (properties)
    {
        switch (ES_Indexed_Properties::GetType(properties))
        {
        case ES_Indexed_Properties::TYPE_COMPACT:
        {
            ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(properties);
            unsigned index_iter = index + 1;

            if (index_iter < compact->capacity)
            {
                do
                    if (compact->IsUsed(index_iter) && (!skip_dont_enum || (compact->GetAttributes(index_iter) & DE) != DE))
                    {
                        index = index_iter;
                        goto found;
                    }
                while (++index_iter != compact->capacity);
            }

            return FALSE;
        }

        case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
        {
            ES_Byte_Array_Indexed *byte_array = static_cast<ES_Byte_Array_Indexed *>(properties);

            index++;

            if (index < byte_array->Capacity())
            {
                index_out = index;
                return TRUE;
            }
            return FALSE;
        }

        case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
        {
            ES_Type_Array_Indexed *type_array = static_cast<ES_Type_Array_Indexed *>(properties);

            index++;

            if (index < type_array->Capacity())
            {
                index_out = index;
                return TRUE;
            }
            return FALSE;
        }

        case ES_Indexed_Properties::TYPE_SPARSE:
            ES_Sparse_Indexed_Properties *sparse = static_cast<ES_Sparse_Indexed_Properties *>(properties);

            do
            {
                ++index;

                ES_Sparse_Indexed_Properties::Node *node = cached_node;

                if (node)
                {
                find_next:
                    OP_ASSERT(node->index < index);

                    if (ES_Sparse_Indexed_Properties::Node *next = node->GetNext())
                    {
                        OP_ASSERT(next->index >= index);

                        cached_node = next;
                        index = next->index;
                    }
                    else
                    {
                        index = UINT_MAX;
                        return FALSE;
                    }
                }
                else
                {
                    node = sparse->root;

                    while (TRUE)
                        if (node->index == index || node->index > index && !node->less)
                        {
                            cached_node = node;
                            index = node->index;
                            break;
                        }
                        else if (node->index < index)
                            if (node->greater)
                                node = node->greater;
                            else
                                goto find_next;
                        else
                            node = node->less;
                }
            }
            while (skip_dont_enum && (cached_node->attributes & DE) == DE);
        }
    }
    else
    {
        if (object)
            if (object->IsStringObject())
            {
                unsigned int slen = Length(static_cast<ES_String_Object *>(object)->GetValue());
                if (index == UINT_MAX)
                {
                    if (slen == 0)
                        return FALSE;
                    index = 0;
                }
                else
                {
                    index++;
                    if (index >= slen)
                    {
                        index = UINT_MAX;
                        return FALSE;
                    }
                }
                goto found;
            }
            else if (object->IsHostObject())
            {
                ES_Host_Object *host_object = static_cast<ES_Host_Object *>(object);
                unsigned capacity = host_object->GetIndexedPropertiesLength(context);

                if (capacity != 0)
                {
                    unsigned index_iter = index + 1;

                    if (index_iter < capacity)
                        do
                            if (object->HasProperty(context, index_iter))
                            {
                                index = index_iter;
                                goto found;
                            }
                        while (++index_iter != capacity);
                }
            }

        return FALSE;
    }

found:
    index_out = index;
    return TRUE;
}

BOOL
ES_Indexed_Property_Iterator::LowerBound(unsigned &index_out, unsigned limit)
{
    if (limit == 0)
        Reset();
    else
        index = limit - 1;

    return Next(index_out);
}

BOOL
ES_Indexed_Property_Iterator::UpperBound(unsigned &index_out, unsigned limit)
{
    Reset();
    unsigned last_index;
    if (Previous(last_index))
    {
        Reset();

        if (limit < last_index)
            index = limit + 1;

        return Previous(index_out);
    }
    else
        return FALSE;
}

BOOL
ES_Indexed_Property_Iterator::DeleteL(ES_Context *context)
{
    BOOL result = TRUE;

    if (properties)
    {
        switch (ES_Indexed_Properties::GetType(properties))
        {
        default:
        case ES_Indexed_Properties::TYPE_COMPACT:
            properties = static_cast<ES_Compact_Indexed_Properties *>(properties)->DeleteL(context, index, result);
            break;

        case ES_Indexed_Properties::TYPE_SPARSE:
            properties = static_cast<ES_Sparse_Indexed_Properties *>(properties)->DeleteL(index, result);
            cached_node = NULL;
            break;

        case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
        case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
            /* read only */
            break;
        }
    }

    return result;
}

BOOL
ES_Indexed_Property_Iterator::GetValueSlow(ES_Value_Internal &value)
{
    if (object)
        if (object->IsStringObject())
        {
            JString *result = NULL;
            if (ES_String_Object::GetSubString(context, static_cast<ES_String_Object *>(object), index, 1, result))
            {
                value.SetString(result);
                return TRUE;
            }
            else
                return FALSE;
        }
        else if (object->IsHostObject())
        {
            OP_ASSERT(context->GetExecutionContext());
            GetResult result = static_cast<ES_Host_Object *>(object)->GetHostL(context->GetExecutionContext(), object, index, value);
            if (GET_OK(result))
                return TRUE;
            else if (result == PROP_GET_FAILED)
                return FALSE;
            else
            {
                /* Odd; the object first claimed to have the property and now
                   doesn't.  Pretend it's undefined.  This probably never
                   happens. */
                OP_ASSERT(FALSE);
                value.SetUndefined();
                return TRUE;
            }
        }

    switch (ES_Indexed_Properties::GetType(properties))
    {
    case ES_Indexed_Properties::TYPE_COMPACT:
        value = static_cast<ES_Compact_Indexed_Properties *>(properties)->values[index];
        break;

    case ES_Indexed_Properties::TYPE_SPARSE:
        value = cached_node->value;
        break;

    case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
        value.SetInt32(static_cast<ES_Byte_Array_Indexed *>(properties)->Storage()[index]);
        break;

    case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
        static_cast<ES_Type_Array_Indexed *>(properties)->GetValue(index, value);
        break;
    }

    if (context && value.IsSpecial())
        return GET_OK(static_cast<ES_Special_Property *>(value.GetDecodedBoxed())->SpecialGetL(context, object, value, object));
    else
        return TRUE;
}
