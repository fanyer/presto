/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_array_builtins.h"
#include "modules/ecmascript/carakan/src/kernel/es_string_appender.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_array_object.h"
#include "modules/ecmascript/carakan/src/object/es_indexed.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/ecmascript/carakan/src/vm/es_suspended_call.h"

static BOOL
IsSimpleArray(ES_Object *object)
{
    if (object->HasIndexedGettersOrSetters())
        return FALSE;

    if (ES_Object *prototype = object->Class()->Prototype())
        if (prototype->HasIndexedProperties())
            return FALSE;

    ES_Indexed_Properties *indexed = object->GetIndexedProperties();

    return indexed && ES_Indexed_Properties::GetType(indexed) == ES_Indexed_Properties::TYPE_COMPACT;
}

BOOL
ES_Array_Property_Iterator_Elm::Previous(unsigned &index)
{
    return iterator.Previous(index);
}

BOOL
ES_Array_Property_Iterator_Elm::Next(unsigned &index)
{
    return iterator.Next(index);
}

BOOL
ES_Array_Property_Iterator_Elm::LowerBound(unsigned &index, unsigned limit)
{
    return iterator.LowerBound(index, limit);
}

BOOL
ES_Array_Property_Iterator_Elm::UpperBound(unsigned &index, unsigned limit)
{
    return iterator.UpperBound(index, limit);
}

BOOL
ES_Array_Property_Iterator_Elm::GetValue(ES_Value_Internal &value)
{
    return iterator.GetValue(value);
}

unsigned
ES_Array_Property_Iterator_Elm::GetCurrentIndex()
{
    return iterator.GetCurrentIndex();
}

void
ES_Array_Property_Iterator_Elm::FlushCache()
{
    iterator.FlushCache();
}

ES_Array_Property_Iterator::ES_Array_Property_Iterator(ES_Execution_Context *context, ES_Object *object, unsigned length)
    : context(context)
    , first(context, object)
    , current_index(UINT_MAX)
    , length(length)
    , started(FALSE)
{
    first.Into(this);

    BOOL include_prototypes = FALSE;

    if (object->HasIndexedGettersOrSetters())
        /* Even if the prototypes don't have indexed properties now, getters on
           the instance object might set indexed properties on the prototype
           objects during iteration. */
        include_prototypes = TRUE;
    else if (ES_Object *prototype = object->Class()->Prototype())
        if (prototype->HasIndexedProperties())
            include_prototypes = TRUE;

    if (include_prototypes)
        for (ES_Object *prototype = object->Class()->Prototype(); prototype; prototype = prototype->Class()->Prototype())
        {
            ES_Suspended_NEW2<ES_Array_Property_Iterator_Elm, ES_Context*, ES_Object*> suspended(context, prototype);
            context->SuspendedCall(&suspended);
            if (!suspended.storage)
                context->AbortOutOfMemory();
            suspended.storage->Into(this);
        }
}

ES_Array_Property_Iterator::~ES_Array_Property_Iterator()
{
    first.Out();

    if (First())
        ES_SuspendedClearHead(context, this);
}

void
ES_Array_Property_Iterator::Reset()
{
    for (ES_Array_Property_Iterator_Elm *iter = First(); iter; iter = iter->Suc())
        iter->Reset();

    started = FALSE;
}

BOOL
ES_Array_Property_Iterator::Next(unsigned &index)
{
    unsigned next_candidate;
    unsigned best_candidate = length;
    current = NULL;

    if (!started)
    {
        for (ES_Array_Property_Iterator_Elm *iter = First(); iter != NULL; iter = iter->Suc())
        {
            BOOL has_elm = iter->Next(next_candidate);
            if (has_elm && next_candidate < best_candidate)
            {
                best_candidate = next_candidate;
                current = iter;
            }
        }

        started = TRUE;
        current_index = index = best_candidate;
        return current != NULL;
    }

    current_index++;

    if (current_index >= length)
        return FALSE;

    for (ES_Array_Property_Iterator_Elm *iter = First(); iter != NULL; iter = iter->Suc())
    {
        next_candidate = iter->GetCurrentIndex();

        BOOL has_next = TRUE;
        if (next_candidate >= current_index)
            has_next = iter->LowerBound(next_candidate, current_index);
        else
            while ((next_candidate == UINT_MAX || next_candidate < current_index) && has_next)
                has_next = iter->Next(next_candidate);

        if (!has_next)
            continue;

        if (next_candidate == current_index)
        {
            current = iter;
            best_candidate = next_candidate;
            break;
        }
        else if (next_candidate < best_candidate)
        {
            best_candidate = next_candidate;
            current = iter;
        }
    }
    current_index = index = best_candidate;
    return current != NULL;
}


BOOL
ES_Array_Property_Iterator::Previous(unsigned &index)
{
    // If we have the max value, find the last in each.
    unsigned next_candidate;
    unsigned best_candidate = UINT_MAX;
    current = NULL;

    if (!started)
    {
        for (ES_Array_Property_Iterator_Elm *iter = First(); iter != NULL; iter = iter->Suc())
        {
            BOOL has_elm = iter->Previous(next_candidate);
            while (has_elm && next_candidate >= length)
                has_elm = iter->Previous(next_candidate);
            if (has_elm && (best_candidate == UINT_MAX || next_candidate > best_candidate))
            {
                best_candidate = next_candidate;
                current = iter;
            }
        }

        started = TRUE;
        current_index = index = best_candidate;
        return current != NULL;
    }

    if (current_index <= 0)
        return FALSE;

    current_index--;

    if (current_index >= length)
        current_index = length - 1;

    for (ES_Array_Property_Iterator_Elm *iter = First(); iter != NULL; iter = iter->Suc())
    {
        next_candidate = iter->GetCurrentIndex();

        BOOL has_next = TRUE;
        if (next_candidate <= current_index)
            has_next = iter->UpperBound(next_candidate, current_index);
        else
            while (next_candidate > current_index && has_next)
                has_next = iter->Previous(next_candidate);

        if (!has_next)
            continue;

        if (next_candidate == current_index)
        {
            current = iter;
            best_candidate = next_candidate;
            break;
        }
        else if (best_candidate == UINT_MAX || next_candidate > best_candidate)
        {
            best_candidate = next_candidate;
            current = iter;
        }
    }
    current_index = index = best_candidate;
    return current != NULL;
}

BOOL
ES_Array_Property_Iterator::LowerBound(unsigned &index, unsigned limit)
{
    unsigned next_candidate;
    unsigned best_candidate = length;
    current = NULL;

    if (limit >= length)
        return FALSE;

    for (ES_Array_Property_Iterator_Elm *iter = First(); iter != NULL; iter = iter->Suc())
    {
        if (!iter->LowerBound(next_candidate, limit))
            continue;

        if (next_candidate == limit)
        {
            current = iter;
            best_candidate = next_candidate;
            break;
        }
        if (next_candidate < best_candidate)
        {
            current = iter;
            best_candidate = next_candidate;
        }
    }
    current_index = index = best_candidate;
    started = TRUE;
    return current != NULL;
}

BOOL
ES_Array_Property_Iterator::UpperBound(unsigned &index, unsigned limit)
{
    unsigned next_candidate;
    unsigned best_candidate = UINT_MAX;
    current = NULL;
    for (ES_Array_Property_Iterator_Elm *iter = First(); iter != NULL; iter = iter->Suc())
    {
        if (!iter->UpperBound(next_candidate, limit))
            continue;

        if (next_candidate == limit)
        {
            current = iter;
            best_candidate = next_candidate;
            break;
        }
        if (best_candidate == UINT_MAX || next_candidate > best_candidate)
        {
            current = iter;
            best_candidate = next_candidate;
        }
    }
    current_index = index = best_candidate;
    started = TRUE;
    return current != NULL;
}

#define ES_THIS_OBJECT() if (!argv[-2].ToObject(context)) return FALSE; ES_Object *this_object = argv[-2].GetObject(context);
#define ES_CHECK_CALLABLE(index, msg) do { if (index >= argc || !argv[index].IsObject() || !argv[index].GetObject(context)->IsFunctionObject()) { context->ThrowTypeError(msg); return FALSE; } } while (0)

#define ES_GET_LENGTH(length) unsigned length; do { if (this_object->GetLength(context, length) == PROP_GET_FAILED) return FALSE; } while (0)
#define ES_PUT_LENGTH(length) do { if (this_object->PutLength(context, length, TRUE) == PROP_PUT_FAILED) return FALSE; } while (0)

/* static */ BOOL
ES_ArrayBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    unsigned length;

    BOOL param_is_length = argc == 1 && argv[0].IsNumber();

    if (param_is_length)
    {
        if (!argv[0].IsUInt32())
        {
            context->ThrowRangeError("Array constructor: length must be a 32-bit unsigned integer");
            return FALSE;
        }
        length = argv[0].GetNumAsUInt32();
    }
    else
        length = argc;

    unsigned capacity = 0;

    /* Bogus limit: access-nsieve uses 80001 as length, and benefits greatly
       from preallocating the array. */
    if (length <= 131072)
        capacity = ES_Compact_Indexed_Properties::AppropriateCapacity(length);

    ES_Object *array = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT(), capacity, length);

    return_value->SetObject(array);

    unsigned dummy;

    if (!param_is_length && array->PutManyL(context, dummy, 0, argc, argv) != PROP_PUT_OK)
        return FALSE;

    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();

    ES_Value_Internal fn;
    GetResult result = this_object->GetL(context, context->rt_data->idents[ESID_join], fn);
    if (!GET_OK(result) || !fn.IsObject() || !fn.GetObject()->IsFunctionObject())
    {
        ES_Object *obj = context->GetGlobalObject()->GetObjectPrototype();
        GetResult result = obj->GetL(context, context->rt_data->idents[ESID_toString], fn);
        if (!GET_OK(result) || !fn.IsObject() || !fn.GetObject()->IsFunctionObject())
        {
            context->ThrowReferenceError("Array.prototype.toString: failed to find Object.prototype.toString");
            return FALSE;
        }
    }

    ES_Value_Internal value;
    ES_Value_Internal *registers = context->SetupFunctionCall(fn.GetObject(), 0);
    registers[0].SetObject(this_object);
    registers[1].SetObject(fn.GetObject());
    if (!context->CallFunction(registers, 0, &value) || !value.ToString(context))
        return FALSE;
    *return_value = value;

    return TRUE;
}

static BOOL toLocaleStringInternal(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, ES_Object *this_object)
{
    ES_GET_LENGTH(length);

    if (length != 0)
    {
        TempBuffer buffer;
        buffer.SetExpansionPolicy(TempBuffer::AGGRESSIVE);

        const uni_char *separator = UNI_L(",");
        unsigned separator_length = 1;

        ES_Array_Property_Iterator iter(context, this_object, length);
        unsigned index, previous = 0;

        ES_Value_Internal *registers = context->AllocateRegisters(1), &value = registers[0];

        unsigned class_id = 0;
        ES_Object *prototype_object = NULL;
        ES_PropertyIndex cached_index(0);

        while (iter.Next(index))
        {
            while (previous++ < index)
                buffer.Append(separator, separator_length);

            if (!iter.GetValue(value))
            {
                context->FreeRegisters(1);
                return FALSE;
            }

            if (!value.IsNullOrUndefined())
            {
                value.ToObject(context);

                ES_Value_Internal fn;
                ES_Object *object = value.GetObject(context);
                GetResult result;

                if (class_id == object->Class()->Id())
                    if (prototype_object)
                        result = prototype_object->GetCachedAtIndex(cached_index, fn);
                    else
                        result = object->GetCachedAtIndex(ES_PropertyIndex(cached_index), fn);
                else
                {
                    result = object->GetL(context, object, context->rt_data->idents[ESID_toLocaleString], fn, prototype_object, cached_index);

                    if (result == PROP_GET_OK_CAN_CACHE)
                        class_id = object->Class()->GetId(context);
                    else
                        class_id = UINT_MAX;
                }

                if (GET_OK(result) && fn.IsObject() && fn.GetObject()->IsFunctionObject())
                {
                    ES_Value_Internal *call_registers = context->SetupFunctionCall(fn.GetObject(), 0);
                    call_registers[0].SetObject(object);
                    call_registers[1].SetObject(fn.GetObject());
                    if (!context->CallFunction(call_registers, 0, &value) || !value.ToString(context))
                    {
                        context->FreeRegisters(1);
                        return FALSE;
                    }
                }
                else
                {
                    if (result != PROP_GET_FAILED)
                        context->ThrowTypeError("Array.prototype.toLocaleString: failed to call toLocaleString on member");
                    context->FreeRegisters(1);
                    return FALSE;
                }

                buffer.Append(Storage(context, value.GetString()), Length(value.GetString()));
            }

            iter.FlushCache(); // Anything could have happened during the function call.

            previous = index;
        }

        while (++previous < length)
            buffer.Append(separator, separator_length);

        context->FreeRegisters(1);

        const uni_char *storage = buffer.GetStorage();
        if (storage)
            return_value->SetString(JString::Make(context, storage));
        else
            return_value->SetString(context->rt_data->strings[STRING_empty]);
    }
    else
        return_value->SetString(context->rt_data->strings[STRING_empty]);

    return TRUE;
}

class ES_SuspendedLocaleString
    : public ES_SuspendedCall
{
public:
    ES_SuspendedLocaleString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, ES_Object *this_object)
        : result(FALSE)
        , argc(argc)
        , argv(argv)
        , return_value(return_value)
        , this_object(this_object)
    {
        context->SuspendedCall(this);
    }

    BOOL result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        this_object->SetInToString();
        result = toLocaleStringInternal(context, argc, argv, return_value, this_object);
        this_object->ResetInToString();
    }

    unsigned argc;
    ES_Value_Internal *argv;
    ES_Value_Internal *return_value;
    ES_Object *this_object;
};

/* static */ BOOL
ES_ArrayBuiltins::toLocaleString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();

    if (this_object->IsInToString())
    {
        return_value->SetString(context->rt_data->strings[STRING_empty]);
        return TRUE;
    }
    else
        return ES_SuspendedLocaleString(context, argc, argv, return_value, this_object).result;
}

/* This isn't really the logarithm function since we adjust the value
   with one and for input 0 we return 1. But in lack of a better name
   we call it ilog10. */
static unsigned
ilog10(unsigned n)
{
    if (n < 100)
    {
        if (n < 10)
            return 1;
        else
            return 2;
    }
    else
    {
        if (n < 1000000)
        {
            if (n < 10000)
            {
                if (n < 1000)
                    return 3;
                else
                    return 4;
            }
            else
            {
                if (n < 100000)
                    return 5;
                else
                    return 6;
            }
        }
        else
        {
            if (n < 100000000)
            {
                if (n < 10000000)
                    return 7;
                else
                    return 8;
            }
            else
            {
                if (n < 1000000000)
                    return 9;
                else
                    return 10;
            }
        }
    }
}

/* static */ BOOL
ES_FastJoin(ES_Execution_Context *context, ES_Compact_Indexed_Properties *compact, unsigned length, JString *separator, ES_Value_Internal *return_value)
{
    ES_Value_Internal element;
    unsigned string_length = 0;
    unsigned top = es_minu(compact->Top(), length);

    for (unsigned index = 0; index < top; ++index)
        if (compact->Get(index, element) && !element.IsNullOrUndefined())
        {
            unsigned element_length = 0;
            switch (element.Type())
            {
            case ESTYPE_BOOLEAN:
                element_length += element.GetBoolean() ? 4 : 5;
                break;
            case ESTYPE_STRING:
                element_length += Length(element.GetString());
                break;
            case ESTYPE_INT32:
                {
                    int value = element.GetInt32();

                    if (value < 0)
                    {
                        value *= -1;
                        element_length++;
                    }

                    element_length += ilog10(value);
                }
                break;
            default:
                return FALSE;
            }

            // overflow check
            if (UINT_MAX - string_length <= element_length)
                return FALSE;

            string_length += element_length;
        }

    if (top)
    {
        const uni_char *separator_storage = Storage(context, separator);
        unsigned separator_length = Length(separator);

        // overflow check
        if (separator_length)
        {
            if (UINT_MAX / separator_length < length)
                return FALSE;

            unsigned total_separator_length = separator_length * (length - 1);

            if (UINT_MAX - string_length < total_separator_length)
                return FALSE;

            string_length += total_separator_length;
        }

        JString *string = JString::Make(context, string_length);
        ES_CollectorLock gclock(context);

        uni_char *storage = Storage(context, string);
        unsigned count = 0;

        for (unsigned index = 0; index < top; ++index)
        {
            if (count < index)
                do
                {
                    op_memcpy(storage, separator_storage, UNICODE_SIZE(separator_length));
                    storage += separator_length;
                } while (++count < index);

            if (compact->Get(index, element) && !element.IsNullOrUndefined())
            {
                element.ToString(context);

                JString *element_string = element.GetString();
                unsigned element_length = Length(element_string);

                op_memcpy(storage, Storage(context, element_string), UNICODE_SIZE(element_length));
                storage += element_length;
            }
        }

        while (++count < length)
        {
            op_memcpy(storage, separator_storage, UNICODE_SIZE(separator_length));
            storage += separator_length;
        }

        return_value->SetString(Finalize(context, string));
    }
    else
        return_value->SetString(context->rt_data->strings[STRING_empty]);

    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::join(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    if (this_object->IsInToString())
    {
        return_value->SetString(context->rt_data->strings[STRING_empty]);
        return TRUE;
    }

    JString *separator;
    unsigned separator_length;

    if (argc != 0 && !argv[0].IsUndefined())
    {
        if (!argv[0].ToString(context))
            return FALSE;

        separator = argv[0].GetString();
        separator_length = Length(separator);
    }
    else
    {
        separator = context->rt_data->strings[STRING_nul + ','];
        separator_length = 1;
    }

    this_object->SetInToString();

    if (length != 0)
    {
        ES_CollectorLock gclock(context);

        if (!this_object->IsHostObject() && !ES_Indexed_Properties::HasGettersOrSetters(this_object->GetIndexedProperties()) && (!this_object->Class()->Prototype() || !this_object->Class()->Prototype()->HasIndexedProperties()))
        {
            unsigned nused;
            if (this_object->IsStringObject())
                nused = length;
            else
            {
                ES_Indexed_Properties *indexed = this_object->GetIndexedProperties();

                if (indexed && ES_Indexed_Properties::GetType(indexed) == ES_Indexed_Properties::TYPE_COMPACT)
                    if (ES_FastJoin(context, static_cast<ES_Compact_Indexed_Properties *>(indexed), length, separator, return_value))
                        goto return_true;

                nused = es_minu(ES_Indexed_Properties::GetUsed(indexed), length);
            }

            unsigned nsegments = nused;
            unsigned total_length = 0;

            if (separator_length != 0)
                nsegments += length - 1;

            if (nsegments == 0)
                return_value->SetString(context->rt_data->strings[STRING_empty]);
            else if (nsegments > 64)
                goto not_segmented;
            else
            {
                JStringSegmented *segmented = JStringSegmented::Make(context, nsegments);
                JStringStorage **bases = segmented->Bases();
                unsigned *offsets = segmented->Offsets();
                unsigned *lengths = segmented->Lengths();
                unsigned segment_index = 0;

                ES_Value_Internal *registers = context->AllocateRegisters(1), &value = registers[0];

                if (separator_length == 0)
                {
                    ES_Indexed_Property_Iterator iter(context, this_object, this_object->GetIndexedPropertiesRef());
                    unsigned index;

                    while (iter.Next(index) && index < length)
                    {
                        if (!iter.GetValue(value))
                        {
                            context->FreeRegisters(1);
                            goto return_false;
                        }

                        BOOL need_flush = FALSE;

                        if (!value.IsNull() && !value.IsUndefined())
                        {
                            if (value.IsObject())
                                need_flush = TRUE;

                            if (!value.ToString(context))
                            {
                                context->FreeRegisters(1);
                                goto return_false;
                            }

                            JString *string = value.GetString();
                            unsigned string_length = Length(string);

                            if (string_length != 0)
                            {
                                offsets[segment_index] = 0;
                                bases[segment_index] = StorageSegment(context, string, offsets[segment_index], string_length);
                                lengths[segment_index] = string_length;
                                ++segment_index;

                                total_length += string_length;
                            }
                        }

                        if (need_flush)
                            iter.FlushCache(); // Anything could have happened during the function call.
                    }
                }
                else
                {
                    unsigned nseparators;
                    ES_Indexed_Property_Iterator iter(context, this_object, this_object->GetIndexedPropertiesRef());
                    unsigned index, previous_index = 0;
                    unsigned separator_offset = 0;
                    JStringStorage *separator_storage = StorageSegment(context, separator, separator_offset, separator_length);

                    if (iter.LowerBound(index, 0) && index < length)
                    {
                        do
                        {
                            nseparators = index - previous_index;
                            previous_index = index;
                            total_length += separator_length * nseparators;
                            while (nseparators-- != 0)
                            {
                                bases[segment_index] = separator_storage;
                                offsets[segment_index] = separator_offset;
                                lengths[segment_index] = separator_length;

                                ++segment_index;
                            }

                            if (!iter.GetValue(value))
                            {
                                context->FreeRegisters(1);
                                goto return_false;
                            }

                            BOOL need_flush = FALSE;

                            if (!value.IsNull() && !value.IsUndefined())
                            {
                                if (value.IsObject())
                                    need_flush = TRUE;

                                if (!value.ToString(context))
                                {
                                    context->FreeRegisters(1);
                                    goto return_false;
                                }

                                JString *string = value.GetString();
                                unsigned slength = Length(string);

                                if (slength != 0)
                                {
                                    offsets[segment_index] = 0;
                                    bases[segment_index] = StorageSegment(context, string, offsets[segment_index], slength);
                                    lengths[segment_index] = slength;

                                    ++segment_index;

                                    total_length += slength;
                                }
                            }

                            if (need_flush)
                                iter.FlushCache(); // Anything could have happened during the function call.
                        } while (iter.Next(index) && index < length);
                        nseparators = length < 1 + index ? 0 : length - index - 1;
                    }
                    else
                        nseparators = length - 1;

                    total_length += separator_length * nseparators;
                    while (nseparators-- != 0)
                    {
                        bases[segment_index] = separator_storage;
                        offsets[segment_index] = separator_offset;
                        lengths[segment_index] = separator_length;

                        ++segment_index;
                    }
                }

                context->FreeRegisters(1);

                if (segment_index > 0)
                {
                    OP_ASSERT(segment_index <= nsegments);
                    segmented->nsegments = segment_index;
                    return_value->SetString(JString::Make(context, segmented, total_length));
                }
                else
                    return_value->SetString(context->rt_data->strings[STRING_empty]);
            }
        }
        else
        {
        not_segmented:
            /* Reserve space for the result, pre-allocating the known lower bound. */
            unsigned reserve_chars = (length - 1) * separator_length;

            /* If the result array is too big to possibly satisfy, fail early. */
            if (reserve_chars > ES_LIM_OBJECT_SIZE / sizeof(uni_char))
            {
                this_object->ResetInToString();
                context->ThrowRangeError("Array.prototype.join: result string too long");
                return FALSE;
            }
            JString *result = JString::Make(context, reserve_chars);
            result->length = result->value->length = 0;

            ES_JString_Appender appender(context, result);

            ES_Array_Property_Iterator iter(context, this_object, length);
            unsigned index, previous = 0;

            ES_Value_Internal *registers = context->AllocateRegisters(1), &value = registers[0];

            const uni_char *separator_string = Storage(context, separator);

            while (iter.Next(index))
            {
                while (previous++ < index)
                    appender.Append(separator_string, separator_length);

                BOOL need_flush = FALSE; // FIXME: also needed if GetValue() calls a getter

                if (!iter.GetValue(value))
                {
                    context->FreeRegisters(1);
                    goto return_false;
                }

                if (!value.IsNull() && !value.IsUndefined())
                {
                    if (value.IsObject())
                        need_flush = TRUE;

                    if (!value.ToString(context))
                    {
                        context->FreeRegisters(1);
                        goto return_false;
                    }

                    appender.Append(value.GetString());
                }

                if (need_flush)
                    iter.FlushCache(); // Anything could have happened during the function call.

                previous = index;
            }

            while (++previous < length)
                appender.Append(separator_string, separator_length);

            context->FreeRegisters(1);

            return_value->SetString(Finalize(context, appender.GetString()));
        }
    }
    else
        return_value->SetString(context->rt_data->strings[STRING_empty]);

return_true:
    this_object->ResetInToString();
    return TRUE;
return_false:
    this_object->ResetInToString();
    return FALSE;
}

/* static */ BOOL
ES_ArrayBuiltins::forEach(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_CHECK_CALLABLE(0, "Array.prototype.forEach: callbackfn is not a callable object");

    ES_Object *callbackfn = argv[0].GetObject(context);
    ES_Value_Internal thisArg;

    if (argc >= 2)
        thisArg = argv[1];

    ES_Array_Property_Iterator iter(context, this_object, length);

    if (callbackfn->IsFunctionObject())
    {
        ES_FunctionCall call(context, thisArg, callbackfn, 3);
        unsigned index;

        call.Initialize();

        ES_Code *stored_code = context->Code();

        while (iter.Next(index))
        {
            call.Setup(stored_code);

            if (!iter.GetValue(call.argv[0]))
            {
                call.Abort();
                return FALSE;
            }

            call.argv[1].SetUInt32(index);
            call.argv[2].SetObject(this_object);

            if (!call.Execute())
                return FALSE;

            iter.FlushCache();
        }
    }

    return_value->SetUndefined();
    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::every(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_CHECK_CALLABLE(0, "Array.prototype.every: callbackfn is not a callable object");

    ES_Object *callbackfn = argv[0].GetObject(context);
    ES_Value_Internal thisArg;

    if (argc >= 2)
        thisArg = argv[1];

    ES_Array_Property_Iterator iter(context, this_object, length);
    unsigned index;

    while (iter.Next(index))
    {
        ES_Value_Internal *registers = context->SetupFunctionCall(callbackfn, /* argc = */ 3);

        registers[0] = thisArg;
        registers[1].SetObject(callbackfn);

        if (!iter.GetValue(registers[2]))
        {
            context->AbortFunctionCall(callbackfn, 3);
            return FALSE;
        }

        registers[3].SetUInt32(index);
        registers[4].SetObject(this_object);

        ES_Value_Internal ret_val;

        if (!context->CallFunction(registers, /* argc = */ 3, &ret_val))
            return FALSE;

        ret_val.ToBoolean();
        if (!ret_val.GetBoolean())
        {
            return_value->SetBoolean(FALSE);
            return TRUE;
        }

        iter.FlushCache(); // Anything could have happened during the function call.
    }

    return_value->SetBoolean(TRUE);
    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::some(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_CHECK_CALLABLE(0, "Array.prototype.some: callbackfn is not a callable object");

    ES_Object *callbackfn = argv[0].GetObject(context);
    ES_Value_Internal thisArg;

    if (argc >= 2)
        thisArg = argv[1];

    ES_Array_Property_Iterator iter(context, this_object, length);
    unsigned index;

    while (iter.Next(index))
    {
        ES_Value_Internal *registers = context->SetupFunctionCall(callbackfn, /* argc = */ 3);

        registers[0] = thisArg;
        registers[1].SetObject(callbackfn);

        if (!iter.GetValue(registers[2]))
        {
            context->AbortFunctionCall(callbackfn, 3);
            return FALSE;
        }

        registers[3].SetUInt32(index);
        registers[4].SetObject(this_object);

        ES_Value_Internal ret_val;

        if (!context->CallFunction(registers, /* argc = */ 3, &ret_val))
            return FALSE;

        ret_val.ToBoolean();
        if (ret_val.GetBoolean())
        {
            return_value->SetBoolean(TRUE);
            return TRUE;
        }

        iter.FlushCache(); // Anything could have happened during the function call.
    }

    return_value->SetBoolean(FALSE);
    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::map(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_CHECK_CALLABLE(0, "Array.prototype.map: callbackfn is not a callable object");

    ES_Object *callbackfn = argv[0].GetObject(context);
    ES_Value_Internal thisArg;

    if (argc >= 2)
        thisArg = argv[1];

    ES_Object *array = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT());

    ES_Value_Internal *values = context->AllocateRegisters(2);
    values[0].SetObject(array);

    ES_Array::SetLength(context, array, length);

    ES_Array_Property_Iterator iter(context, this_object, length);
    unsigned index;

    while (iter.Next(index))
    {
        ES_Value_Internal *registers = context->SetupFunctionCall(callbackfn, 3);

        registers[0] = thisArg;
        registers[1].SetObject(callbackfn);

        if (!iter.GetValue(registers[2]))
        {
            context->AbortFunctionCall(callbackfn, 3);
            goto return_false;
        }

        registers[3].SetUInt32(index);
        registers[4].SetObject(this_object);

        if (!context->CallFunction(registers, 3, &values[1]))
            goto return_false;

        if (array->PutL(context, index, values[1]) == PROP_PUT_FAILED)
            goto return_false;

        iter.FlushCache(); // Anything could have happened during the function call.
    }

    return_value->SetObject(array);

    context->FreeRegisters(2);
    return TRUE;

return_false:
    context->FreeRegisters(2);
    return FALSE;
}

/* static */ BOOL
ES_ArrayBuiltins::filter(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_CHECK_CALLABLE(0, "Array.prototype.filter: callbackfn is not a callable object");

    ES_Object *callbackfn = argv[0].GetObject(context);
    ES_Value_Internal thisArg;

    if (argc >= 2)
        thisArg = argv[1];

    ES_Object *array = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT());

    ES_Value_Internal *values = context->AllocateRegisters(2);
    values[0].SetObject(array);

    ES_Array_Property_Iterator iter(context, this_object, length);
    unsigned index;

    int ret_len = 0;

    while (iter.Next(index))
    {
        if (!iter.GetValue(values[1]))
            goto return_FALSE;

        ES_Value_Internal *registers = context->SetupFunctionCall(callbackfn, /* argc = */ 3);

        registers[0] = thisArg;
        registers[1].SetObject(callbackfn);
        registers[2] = values[1];
        registers[3].SetUInt32(index);
        registers[4].SetObject(this_object);

        ES_Value_Internal ret_val;

        if (!context->CallFunction(registers, 3, &ret_val))
            goto return_FALSE;

        ret_val.ToBoolean();

        if (ret_val.GetBoolean())
            if (array->PutL(context, ret_len++, values[1]) == PROP_PUT_FAILED)
                goto return_FALSE;

        iter.FlushCache(); // Anything could have happened during the function call.
    }

    ES_Array::SetLength(context, array, ret_len);
    return_value->SetObject(array);

    context->FreeRegisters(2);
    return TRUE;

return_FALSE:
    context->FreeRegisters(2);
    return FALSE;
}

/* static */ BOOL
ES_ArrayBuiltins::reduce(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_CHECK_CALLABLE(0, "Array.prototype.reduce: callbackfn is not a callable object");

    ES_Object *callbackfn = argv[0].GetObject(context);

    ES_Array_Property_Iterator iter(context, this_object, length);
    unsigned index;

    ES_Value_Internal *values = context->AllocateRegisters(1);

    if (argc >= 2)
        values[0] = argv[1];
    else
    {
        if (!iter.Next(index))
        {
            context->ThrowTypeError("Array.prototype.reduce: no initial value provided for empty array");
            goto return_FALSE;
        }

        if (!iter.GetValue(values[0]))
            goto return_FALSE;
    }

    while (iter.Next(index))
    {
        ES_Value_Internal *registers = context->SetupFunctionCall(callbackfn, /* argc = */ 4);

        registers[0].SetUndefined();
        registers[1].SetObject(callbackfn);
        registers[2] = values[0];

        if (!iter.GetValue(registers[3]))
        {
            context->AbortFunctionCall(callbackfn, 4);
            goto return_FALSE;
        }

        registers[4].SetUInt32(index);
        registers[5].SetObject(this_object);

        if (!context->CallFunction(registers, /* argc = */ 4, &values[0]))
            goto return_FALSE;

        iter.FlushCache(); // Anything could have happened during the function call.
    }

    *return_value = values[0];

    context->FreeRegisters(1);
    return TRUE;

return_FALSE:
    context->FreeRegisters(1);
    return FALSE;
}

/* static */ BOOL
ES_ArrayBuiltins::reduceRight(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_CHECK_CALLABLE(0, "Array.prototype.reduceRight: callbackfn is not a callable object");

    ES_Object *callbackfn = argv[0].GetObject(context);

    ES_Array_Property_Iterator iter(context, this_object, length);
    unsigned index;

    ES_Value_Internal *values = context->AllocateRegisters(1);

    if (argc >= 2)
        values[0] = argv[1];
    else
    {
        if (!iter.Previous(index))
        {
            context->ThrowTypeError("Array.prototype.reduceRight: no initial value provided for empty array");
            goto return_FALSE;
        }

        if (!iter.GetValue(values[0]))
            goto return_FALSE;
    }

    while (iter.Previous(index))
    {
        ES_Value_Internal *registers = context->SetupFunctionCall(callbackfn, /* argc = */ 4);

        registers[0].SetUndefined();
        registers[1].SetObject(callbackfn);
        registers[2] = values[0];

        if (!iter.GetValue(registers[3]))
        {
            context->AbortFunctionCall(callbackfn, 4);
            goto return_FALSE;
        }

        registers[4].SetUInt32(index);
        registers[5].SetObject(this_object);

        if (!context->CallFunction(registers, /* argc = */ 4, &values[0]))
            goto return_FALSE;

        iter.FlushCache(); // Anything could have happened during the function call.
    }

    *return_value = values[0];

    context->FreeRegisters(1);
    return TRUE;

return_FALSE:
    context->FreeRegisters(1);
    return FALSE;
}

/* static */ BOOL
ES_ArrayBuiltins::push(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    unsigned index;

    if (this_object->PutManyL(context, index, length, argc, argv) == PROP_PUT_FAILED)
        return FALSE;

    length = (length + argc) < length ? UINT_MAX : length + argc;

    if (index < argc)
    {
        ES_CollectorLock gclock(context);

        ES_Value_Internal *registers = context->AllocateRegisters(1), &name = registers[0];
        double dlength = length;

        for (; index < argc; ++index, ++dlength)
        {
            name.SetNumber(dlength);
            if (!name.ToString(context))
                return FALSE;

            if (this_object->PutL(context, name.GetString(), argv[index]) == PROP_PUT_FAILED)
                return FALSE;
        }

        context->FreeRegisters(1);

        if (this_object->IsArrayObject())
        {
            context->ThrowRangeError("Array.[[Put]]: 'length' out of range");
            return FALSE;
        }

        return_value->SetDouble(dlength);

        PutResult result;
        if ((result = this_object->PutL(context, context->rt_data->idents[ESID_length], *return_value)) == PROP_PUT_READ_ONLY)
            context->ThrowTypeError("Failed to update length");
        if (!PUT_OK(result))
            return FALSE;

        return TRUE;
    }
    else
    {
        ES_PUT_LENGTH(length);

        return_value->SetUInt32(length);
        return TRUE;
    }
}

/* static */ BOOL
ES_ArrayBuiltins::pop(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    if (length == 0)
    {
        ES_PUT_LENGTH(length);
        return_value->SetUndefined();
    }
    else
    {
        --length;
        for (ES_Object *iter = this_object; iter != NULL; iter = iter->Class()->Prototype())
        {
            ES_Indexed_Properties *&properties = iter->GetIndexedPropertiesRef();

            GetResult result = ES_Indexed_Properties::GetL(context, iter, properties, length, *return_value, this_object);
            if (result == PROP_GET_OK)
            {
                if (iter == this_object)
                    ES_Indexed_Properties::DeleteL(context, this_object, properties, length);
                break;
            }
            else if (iter->IsHostObject())
            {
                ES_Host_Object *host_object = static_cast<ES_Host_Object *>(iter);
                result = host_object->GetHostL(context, iter, length, *return_value);
                if (result == PROP_GET_OK)
                {
                    if (iter == this_object)
                    {
                        unsigned delete_result;
                        host_object->DeleteHostL(context, length, delete_result);
                    }
                    break;
                }
            }
            if (GET_NOT_FOUND(result))
                continue;
            else if (result == PROP_GET_FAILED)
                return FALSE;
        }
        ES_PUT_LENGTH(length);
    }

    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::shift(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    if (length == 0)
    {
        ES_PUT_LENGTH(0);
        return_value->SetUndefined();
    }
    else
    {
        if (this_object->GetL(context, static_cast<unsigned>(0), argv[-1]) == PROP_GET_FAILED)
            return FALSE;

        ES_Indexed_Properties *properties = this_object->GetIndexedProperties();
        if (!properties || !ES_Indexed_Properties::HasGettersOrSetters(properties))
        {
            unsigned delete_result;
            if (!this_object->DeleteL(context, static_cast<unsigned>(0), delete_result))
                return FALSE;
        }

        if (!ES_Indexed_Properties::RenumberL(context, this_object, 1, length - 1, -1, length))
            return FALSE;

        *return_value = argv[-1];

        ES_PUT_LENGTH(length - 1);
    }

    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::unshift(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_CollectorLock gclock(context);

    if (length != 0)
    {
        if (UINT_MAX - length < argc)
        {
            /* Some elements will (or might; if they are there at all) spill out
               into the named property representation. */

            ES_Value_Internal *registers = context->AllocateRegisters(2);
            ES_Value_Internal &value = registers[0], &name = registers[1];

            for (unsigned index = UINT_MAX - argc; index < length; ++index)
            {
                name.SetNumber(static_cast<double>(index) + argc);
                if (!name.ToString(context))
                    goto failed;

                GetResult result = this_object->GetL(context, index, value);

                if (GET_OK(result))
                {
                    if (this_object->PutL(context, name.GetString(), value) == PROP_PUT_FAILED)
                        goto failed;

                    ES_Indexed_Properties *properties = this_object->GetIndexedProperties();
                    if (!properties || !ES_Indexed_Properties::HasGettersOrSetters(properties))
                    {
                        unsigned delete_result;
                        if (!this_object->DeleteL(context, index, delete_result))
                            goto failed;
                    }
                }
                else if (GET_NOT_FOUND(result))
                    this_object->DeleteOwnPropertyL(context, name.GetString());
                else
                    goto failed;
            }

            context->FreeRegisters(2);
        }

        if (!ES_Indexed_Properties::RenumberL(context, this_object, 0, es_minu(length, UINT_MAX - argc), argc, length))
            return FALSE;
    }

    for (unsigned index = 0; index < argc; ++index)
        if (this_object->PutNoLockL(context, index, argv[index]) == PROP_PUT_FAILED)
            return FALSE;

    if (UINT_MAX - length < argc)
    {
        context->ThrowRangeError("Array.[[Put]]: 'length' out of range");
        return FALSE;
    }
    else
    {
        length += argc;

        ES_PUT_LENGTH(length);

        return_value->SetUInt32(length);

        return TRUE;
    }

failed:
    context->FreeRegisters(2);
    return FALSE;
}

/* static */ BOOL
ES_ArrayBuiltins::indexOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_Array_Property_Iterator iter(context, this_object, length);
    ES_Value_Internal search_value;

    if (length == 0)
        goto return_not_found;

    if (argc >= 1)
        search_value = argv[0];
    else
        search_value.SetUndefined();

    double n;
    if (argc >= 2)
    {
        if (!argv[1].ToNumber(context))
            return FALSE;
        n = argv[1].GetNumAsInteger();
    }
    else
        n = 0;

    if (n >= length)
        goto return_not_found;

    unsigned k;
    if (n >= 0)
        k = DOUBLE2UINT32(n);
    else
    {
        double abs_n = op_fabs(n);
        if (abs_n <= length)
            k = DOUBLE2UINT32(op_floor(length - abs_n));
        else
            k = 0;
    }

    unsigned index;
    if (iter.LowerBound(index, k))
        do
        {
            ES_Value_Internal element_k;

            if (!iter.GetValue(element_k))
                return FALSE;

            if (ES_Execution_Context::EqStrict(context, &element_k, &search_value))
            {
                return_value->SetUInt32(index);
                return TRUE;
            }
        }
        while (iter.Next(index));

return_not_found:
    return_value->SetInt32(-1);
    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::lastIndexOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_Array_Property_Iterator iter(context, this_object, length);
    ES_Value_Internal search_value;

    if (length == 0)
        goto return_not_found;

    if (argc >= 1)
        search_value = argv[0];
    else
        search_value.SetUndefined();

    double n;
    if (argc >= 2)
    {
        if (!argv[1].ToNumber(context))
            return FALSE;
        n = argv[1].GetNumAsInteger();
    }
    else
        n = length - 1;

    unsigned k;
    if (n >= 0)
        if (n <= length - 1)
            k = DOUBLE2UINT32(n);
        else
            k = length - 1;
    else
    {
        double abs_n = op_fabs(n);
        if (abs_n <= length)
            k = length - DOUBLE2UINT32(abs_n);
        else
            goto return_not_found;
    }

    unsigned index;
    if (iter.UpperBound(index, k))
        do
        {
            ES_Value_Internal element_k;

            if (!iter.GetValue(element_k))
                return FALSE;

            if (ES_Execution_Context::EqStrict(context, &element_k, &search_value))
            {
                return_value->SetUInt32(index);
                return TRUE;
            }
        }
        while (iter.Previous(index));

return_not_found:
    return_value->SetInt32(-1);
    return TRUE;
}

static BOOL
ComputeSliceOrSpliceStart(ES_Execution_Context *context, unsigned &start, unsigned argc, ES_Value_Internal *argv, unsigned length)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        int istart;

        if (argv[0].IsInt32() && (istart = argv[0].GetInt32()) >= 0 && length <= INT_MAX)
            start = es_mini(istart, length);
        else
        {
            double dstart = argv[0].GetNumAsInteger();

            if (dstart < 0)
                dstart = es_maxd(length + dstart, 0);
            else
                dstart = es_mind(dstart, length);

            start = static_cast<unsigned>(DOUBLE2INT32(dstart));
        }
    }
    else
        start = 0;

    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::slice(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    unsigned start, end;

    if (!ComputeSliceOrSpliceStart(context, start, argc, argv, length))
        return FALSE;

    if (argc >= 2 && !argv[1].IsUndefined())
    {
        if (!argv[1].ToNumber(context))
            return FALSE;

        if (argv[1].IsInt32() && argv[1].GetInt32() >= 0 && length < INT_MAX)
            end = es_mini(argv[1].GetInt32(), length);
        else
        {
            double dend = argv[1].GetNumAsInteger();

            if (dend < 0)
                dend = es_maxd(length + dend, 0);
            else
                dend = es_mind(dend, length);

            end = static_cast<unsigned>(DOUBLE2INT32(dend));
       }
    }
    else
        end = length;

    unsigned result_length = static_cast<unsigned>(end > start ? end - start : 0);

    ES_Value_Internal *registers = context->AllocateRegisters(2), &value = registers[0], &result = registers[1];
    ES_Object *array = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT(), es_minu(ES_Compact_Indexed_Properties::AppropriateCapacity(result_length), 0xffu), 0);

    result.SetObject(array);

    if (result_length <= 0xffu)
    {
        ES_CollectorLock gclock(context);

        /* Attempt a [Get] directly, and step on if not there. When working with indexed host objects, saves a round-trip per element. */
        for (unsigned index = start; index < end; index++)
             switch (this_object->GetL(context, index, value))
             {
             case PROP_GET_FAILED:
                 context->FreeRegisters(2);
                 return FALSE;

             case PROP_GET_NOT_FOUND:
             case PROP_GET_NOT_FOUND_CAN_CACHE:
                 continue;

             default:
                 if (value.IsUndefined() && !this_object->HasOwnProperty(context, index))
                     continue;

                 if (array->PutNoLockL(context, index - start, value) == PROP_PUT_FAILED)
                 {
                     context->FreeRegisters(2);
                     return FALSE;
                 }
             }
    }
    else
    {
        ES_Array_Property_Iterator iter(context, this_object, length);
        unsigned index;

        if (iter.LowerBound(index, start) && index < end)
            do
                if (!iter.GetValue(value) || array->PutL(context, index - start, value) == PROP_PUT_FAILED)
                {
                    context->FreeRegisters(2);
                    return FALSE;
                }
            while (iter.Next(index) && index < end);
    }

    context->FreeRegisters(2);

    if (array->PutLength(context, result_length, FALSE) == PROP_PUT_FAILED)
        return FALSE;
    else
    {
        return_value->SetObject(array);
        return TRUE;
    }
}

/* static */ BOOL
ES_ArrayBuiltins::splice(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    unsigned start;

    if (argc >= 1)
    {
        if (!ComputeSliceOrSpliceStart(context, start, argc, argv, length))
            return FALSE;
    }
    else
        start = length;

    unsigned deleteCount, itemsToAdd = argc > 2 ? argc - 2 : 0;

    if (argc >= 2)
        if (argv[1].IsUndefined())
            deleteCount = 0;
        else
        {
            if (!argv[1].ToNumber(context))
                return FALSE;

            deleteCount = static_cast<unsigned>(DOUBLE2INT32(es_mind(es_maxd(argv[1].GetNumAsInteger(), 0), length - start)));
        }
    else
        deleteCount = length - start;

    ES_Value_Internal *registers = context->AllocateRegisters(2), &value = registers[0], &result = registers[1];
    ES_Object *array = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT(), ES_Compact_Indexed_Properties::AppropriateCapacity(es_minu(deleteCount, 0xffu)), 0);

    result.SetObject(array);

    ES_CollectorLock gclock(context);

    if (deleteCount != 0)
    {
        ES_Array_Property_Iterator iter(context, this_object, length);
        unsigned index, end = start + deleteCount;

        if (iter.LowerBound(index, start) && index < end)
            do
                if (!iter.GetValue(value) || array->PutNoLockL(context, index - start, value) == PROP_PUT_FAILED)
                    goto return_false;
            while (iter.Next(index) && index < end);
    }

    if (array->PutLength(context, deleteCount, FALSE) == PROP_PUT_FAILED)
        goto return_false;

    if (itemsToAdd != deleteCount)
    {
        ES_Indexed_Properties *properties = this_object->GetIndexedProperties();
        if (properties && itemsToAdd < deleteCount && !ES_Indexed_Properties::HasGettersOrSetters(properties))
        {
            ES_Indexed_Property_Iterator iter(context, this_object, this_object->GetIndexedPropertiesRef());
            unsigned end = start + deleteCount, index;

            if (iter.LowerBound(index, start + itemsToAdd) && index < end)
                do
                    iter.DeleteL(context);
                while (iter.Next(index) && index < end);
        }

        if (!ES_Indexed_Properties::RenumberL(context, this_object, start + deleteCount, length - (start + deleteCount), static_cast<int>(itemsToAdd) - static_cast<int>(deleteCount), length))
            goto return_false;
    }

    for (unsigned i = 0; i < itemsToAdd; ++i)
        if (ES_Indexed_Properties::PutNoLockL(context, this_object, start + i, 0, argv[i + 2], this_object) == PROP_PUT_FAILED)
            goto return_false;

    if (length - deleteCount <= UINT_MAX - itemsToAdd)
    {
        if (!PUT_OK(this_object->PutLength(context, length - deleteCount + itemsToAdd, TRUE)))
            goto return_false;
    }
    else
    {
        context->ThrowRangeError("Array.[[Put]]: 'length' out of range");
        goto return_false;
    }

    context->FreeRegisters(2);

    return_value->SetObject(array);
    return TRUE;

return_false:
    context->FreeRegisters(2);
    return FALSE;
}

/* static */ BOOL
ES_ArrayBuiltins::concat(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);
    GC_STACK_ANCHOR(context, this_object);

    ES_Value_Internal *registers = context->AllocateRegisters(2), &value = registers[1];

    ES_Object *A = ES_Array::Make(context, ES_GET_GLOBAL_OBJECT());
    ES_Object *E = this_object;
    GC_STACK_ANCHOR(context, E);

    unsigned n = 0, argi = 0;

    registers[0].SetObject(A);

    ES_CollectorLock gclock(context);

    do
    {
        unsigned E_length;

        GetResult result = E->GetLength(context, E_length);

        if (result == PROP_GET_FAILED)
            goto failed;
        else if (GET_OK(result))
        {
            if (E->IsArrayObject())
            {
                if (E_length > 0)
                {
                    if (IsSimpleArray(E))
                    {
                        ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(E->GetIndexedProperties());
                        ES_Value_Internal *values = compact->GetValues();

                        for (unsigned k = 0, top = compact->Top(); k < top; ++k)
                            if (!values[k].IsUndefined() || values[k].GetHiddenBoolean())
                                if (A->PutNoLockL(context, n + k, values[k]) == PROP_PUT_FAILED)
                                    goto failed;
                    }
                    else
                    {
                        ES_Array_Property_Iterator iter(context, E, E_length);
                        unsigned k;

                        while (iter.Next(k))
                        {
                            if (!iter.GetValue(value) || A->PutNoLockL(context, n + k, value) == PROP_PUT_FAILED)
                                goto failed;
                        }
                    }

                    n += E_length;
                }
            }
            else
            {
                if (A->PutNoLockL(context, n, this_object) == PROP_PUT_FAILED)
                    goto failed;

                n++;
            }
        }

        E = NULL;

        while (argi < argc)
            if (argv[argi].IsObject() && argv[argi].GetObject(context)->IsArrayObject())
            {
                E = argv[argi++].GetObject(context);
                break;
            }
            else if (A->PutNoLockL(context, n++, argv[argi++]) == PROP_PUT_FAILED)
                goto failed;
    }
    while (E);

    if (A->PutLength(context, n, FALSE) != PROP_PUT_FAILED)
    {
        context->FreeRegisters(2);
        return_value->SetObject(A);
        return TRUE;
    }

failed:
    context->FreeRegisters(2);
    return FALSE;
}

static BOOL
SwapIndexed(ES_Execution_Context *context, ES_Object *self, ES_Indexed_Properties *&properties, unsigned index1, unsigned index2, ES_Value_Internal *registers)
{
    ES_Value_Internal &value1 = registers[0], &value2 = registers[1];

    GetResult result_low = ES_Indexed_Properties::GetL(context, self, properties, index1, value1, self);
    if (result_low == PROP_GET_FAILED)
        return FALSE;

    GetResult result_high = ES_Indexed_Properties::GetL(context, self, properties, index2, value2, self);
    if (result_high == PROP_GET_FAILED)
        return FALSE;

    if (GET_OK(result_low))
    {
        if (ES_Indexed_Properties::PutL(context, self, index2, value1, self) == PROP_PUT_FAILED)
            return FALSE;
    }
    else
        ES_Indexed_Properties::DeleteL(context, self, properties, index2);

    if (GET_OK(result_high))
    {
        if (ES_Indexed_Properties::PutL(context, self, index1, value2, self) == PROP_PUT_FAILED)
            return FALSE;
    }
    else
        ES_Indexed_Properties::DeleteL(context, self, properties, index1);

    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::reverse(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_Object *proto = this_object;
    while (true) // check for the evil case of indexed props in the prototype chain (see CARAKAN-695)
    {
        ES_Value_Internal proto_val;
        if (!GET_OK(proto->GetPrototypeL(context, proto_val)))
            break;
        if (proto_val.IsObject())
        {
            proto = proto_val.GetObject(context);
            if (proto->HasIndexedProperties())
            {
                for (unsigned k = 0; k < length / 2; k++)
                {
                    unsigned yidx = length - k - 1;
                    ES_Value_Internal xval, yval;
                    BOOL has_x, has_y;

                    GetResult result = this_object->GetL(context, k, xval);
                    if (result == PROP_GET_FAILED)
                        return FALSE;
                    has_x = !GET_NOT_FOUND(result);

                    result = this_object->GetL(context, yidx, yval);
                    if (result == PROP_GET_FAILED)
                        return FALSE;
                    has_y = !GET_NOT_FOUND(result);

                    unsigned res;
                    if (has_y)
                    {
                        if (!PUT_OK(this_object->PutL(context, k, yval)))
                            return FALSE;
                    }
                    else if (!this_object->DeleteL(context, k, res))
                        return FALSE;

                    if (has_x)
                    {
                        if (!PUT_OK(this_object->PutL(context, yidx, xval)))
                            return FALSE;
                    }
                    else if (!this_object->DeleteL(context, yidx, res))
                        return FALSE;
                }
                return_value->SetObject(this_object);
                return TRUE;
            }
        }
        else // regular case (no indexed props in one of the prototypes)
            break;
    }

    if (ES_Indexed_Properties *&properties = this_object->GetIndexedPropertiesRef())
    {
        ES_Value_Internal *registers = context->AllocateRegisters(2);

        switch (ES_Indexed_Properties::GetType(properties))
        {
        default:
        case ES_Indexed_Properties::TYPE_COMPACT:
        {
            /* If it's not sparse, we can safely use the algorithm outlined in
               the standard.  Even if some elements are missing, the iterator
               can't really do it more efficiently that just looking at every
               index either. */

            unsigned k = 0, half = length >> 1;

            while (k < half)
            {
                if (!SwapIndexed(context, this_object, properties, k, length - k - 1, registers))
                {
                    context->FreeRegisters(2);
                    return FALSE;
                }

                ++k;
            }
            break;
        }

        case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
        {
            ES_Byte_Array_Indexed *byte_array = static_cast<ES_Byte_Array_Indexed *>(properties);
            unsigned char *start = byte_array->Storage();
            unsigned char *end   = start + (byte_array->Capacity() - 1);

            while (start < end)
            {
                unsigned char ch = *start;
                *start++ = *end;
                *end--   = ch;
            }
            break;
        }

        case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
        {
            ES_Type_Array_Indexed *type_array = static_cast<ES_Type_Array_Indexed *>(properties);

            switch (type_array->Kind())
            {
            case ES_Type_Array_Indexed::Int8Array:
            case ES_Type_Array_Indexed::Uint8Array:
            case ES_Type_Array_Indexed::Uint8ClampedArray:
            {
                unsigned char *start = type_array->Storage();
                unsigned char *end = start + (type_array->Capacity() - 1);

                while (start != end)
                {
                    unsigned char ch = *start;
                    *start++ = *end;
                    *end--   = ch;
                }
                break;
            }
            case ES_Type_Array_Indexed::Int16Array:
            case ES_Type_Array_Indexed::Uint16Array:
            {
                unsigned short *start = reinterpret_cast<unsigned short *>(type_array->Storage());
                unsigned short *end = start + (type_array->Capacity() - 1);

                while (start != end)
                {
                    unsigned short ch = *start;
                    *start++ = *end;
                    *end--   = ch;
                }
                break;
            }
            case ES_Type_Array_Indexed::Int32Array:
            case ES_Type_Array_Indexed::Uint32Array:
            case ES_Type_Array_Indexed::Float32Array:
            {
                unsigned *start = reinterpret_cast<unsigned *>(type_array->Storage());
                unsigned *end = start + (type_array->Capacity() - 1);

                while (start != end)
                {
                    unsigned elem = *start;
                    *start++ = *end;
                    *end--   = elem;
                }
                break;
            }
            case ES_Type_Array_Indexed::Float64Array:
                double *start = reinterpret_cast<double *>(type_array->Storage());
                double *end = start + (type_array->Capacity() - 1);

                while (start != end)
                {
                    double elem = *start;
                    *start++ = *end;
                    *end--   = elem;
                }
                break;
            }
            break;
        }

        case ES_Indexed_Properties::TYPE_SPARSE:
        {
            ES_Indexed_Property_Iterator iter_low(context, this_object, this_object->GetIndexedPropertiesRef());
            ES_Indexed_Property_Iterator iter_high(context, this_object, this_object->GetIndexedPropertiesRef());

            unsigned index_low = 0, index_high = 0, assigned_low = UINT_MAX, assigned_high = UINT_MAX;
            BOOL used_low = TRUE, used_high = TRUE, new_low, new_high;

            while (((new_low = (used_low && iter_low.Next(index_low))), (new_high = (used_high && iter_high.Previous(index_high))), new_low || new_high) && index_low <= index_high)
            {
                OP_ASSERT(!(index_low == assigned_low) || !(index_high == assigned_high));

                if (length - index_low - 1 >= index_high)
                {
                    assigned_low = index_low;
                    assigned_high = length - index_low - 1;
                    used_low = TRUE;
                    used_high = index_high == assigned_high;
                }
                else
                {
                    assigned_low = length - index_high - 1;
                    assigned_high = index_high;
                    used_low = FALSE;
                    used_high = TRUE;
                }

                if (!SwapIndexed(context, this_object, properties, assigned_low, assigned_high, registers))
                {
                    context->FreeRegisters(2);
                    return FALSE;
                }

                if (!used_low || !used_high)
                {
                    iter_low.FlushCache();
                    iter_high.FlushCache();
                }
            }
            break;
        }
        }
        context->FreeRegisters(2);
    }
    else if (length > 0)
    {
        context->ThrowTypeError("Array.prototype.reverse: unable to reverse type");
        return FALSE;
    }

    return_value->SetObject(this_object);
    return TRUE;
}

static void
MergeSortStrings(ES_Context *context, JString **strings, unsigned *indeces, unsigned *storage, unsigned length, BOOL &unchanged)
{
    if (length == 2)
    {
        if (Compare(context, strings[indeces[0]], strings[indeces[1]]) > 0)
        {
            unchanged = FALSE;
            unsigned index = indeces[0];
            indeces[0] = indeces[1];
            indeces[1] = index;
        }
    }
    else if (length > 2)
    {
        unsigned nleft = length / 2, nright = length - nleft;

        MergeSortStrings(context, strings, indeces, storage + nleft, nleft, unchanged);
        MergeSortStrings(context, strings, indeces + nleft, storage + nleft, nright, unchanged);

        int result = Compare(context, strings[indeces[nleft - 1]], strings[indeces[nleft]]);

        if (result > 0)
        {
            unchanged = FALSE;

            unsigned *ileft = storage, *iright = indeces + nleft, *ifinal = indeces;
            op_memcpy(storage, indeces, nleft * sizeof(unsigned));

            while (nleft != 0 && nright != 0)
            {
                int compare_result = Compare(context, strings[*ileft], strings[*iright]);
                if (compare_result <= 0)
                {
                    *ifinal++ = *ileft++;
                    --nleft;
                }
                if (compare_result >= 0)
                {
                    *ifinal++ = *iright++;
                    --nright;
                }
            }

            while (nleft-- != 0)
                *ifinal++ = *ileft++;

            while (nright-- != 0)
                *ifinal++ = *iright++;
        }
    }
}

static BOOL
CallCompare(int &result, ES_Execution_Context *context, ES_Code *code, ES_FunctionCall *compare, ES_Value_Internal &left, ES_Value_Internal &right, ES_Value_Internal &temporary)
{
    compare->Setup(code);

    compare->argv[0] = left;
    compare->argv[1] = right;

    if (!compare->Execute())
        return FALSE;

    if (compare->return_value->IsNull() || compare->return_value->IsUndefined())
        result = -1;
    else if (compare->return_value->IsInt32())
        result = compare->return_value->GetInt32();
    else
    {
        temporary = *compare->return_value;

        if (!temporary.ToNumber(context))
            return FALSE;

        double dresult = temporary.GetNumAsDouble();
        if (op_isnan(dresult))
            result = -1;
        else
            result = dresult < 0 ? -1 : dresult > 0 ? 1 : 0;
    }

    return TRUE;
}

static BOOL
MergeSort(ES_Execution_Context *context, ES_Code *code, ES_FunctionCall *compare, ES_Value_Internal *values, unsigned *indeces, unsigned *storage, unsigned length, ES_Value_Internal &temporary, BOOL &unchanged)
{
    if (length == 2)
    {
        int result;

        if (!CallCompare(result, context, code, compare, values[indeces[0]], values[indeces[1]], temporary))
            return FALSE;

        if (result > 0)
        {
            unchanged = FALSE;
            unsigned index = indeces[0];
            indeces[0] = indeces[1];
            indeces[1] = index;
        }
    }
    else if (length > 2)
    {
        unsigned nleft = length / 2, nright = length - nleft;

        op_memcpy(storage, indeces, nleft * sizeof(unsigned));

        if (!MergeSort(context, code, compare, values, indeces, storage + nleft, nleft, temporary, unchanged) ||
            !MergeSort(context, code, compare, values, indeces + nleft, storage + nleft, nright, temporary, unchanged))
            return FALSE;

        int result;

        if (!CallCompare(result, context, code, compare, values[indeces[nleft - 1]], values[indeces[nleft]], temporary))
            return FALSE;

        if (result > 0)
        {
            unchanged = FALSE;

            unsigned *ileft = storage, *iright = indeces + nleft, *ifinal = indeces;
            op_memcpy(storage, indeces, nleft * sizeof(unsigned));

            while (nleft != 0 && nright != 0)
            {
                if (!CallCompare(result, context, code, compare, values[*ileft], values[*iright], temporary))
                    return FALSE;

                if (result <= 0)
                {
                    *ifinal++ = *ileft++;
                    --nleft;
                }
                if (result >= 0)
                {
                    *ifinal++ = *iright++;
                    --nright;
                }
            }

            while (nleft-- != 0)
                *ifinal++ = *ileft++;

            while (nright-- != 0)
                *ifinal++ = *iright++;
        }
    }

    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::sort(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_THIS_OBJECT();
    ES_GET_LENGTH(length);

    ES_Indexed_Properties *&indexed_properties = this_object->GetIndexedPropertiesRef();

    if (length != 0 && this_object->HasIndexedProperties())
    {
        ES_Function *comparefn;

        if (argc >= 1 && !argv[0].IsUndefined())
            if (argv[0].IsObject() && argv[0].GetObject(context)->IsFunctionObject())
                comparefn = static_cast<ES_Function *>(argv[0].GetObject(context));
            else
            {
                context->ThrowTypeError("Array.prototype.sort: comparefn argument is not a function");
                return FALSE;
            }
        else
            comparefn = NULL;

        unsigned used = 0;
        ES_Object *prototype = this_object->Class()->Prototype();

        if (!prototype || !prototype->HasIndexedProperties())
            used = ES_Indexed_Properties::GetUsed(indexed_properties);
        else
        {
            ES_Array_Property_Iterator iterator(context, this_object, length);
            unsigned index;

            while (iterator.Next(index))
                ++used;
        }

        AutoRegisters regs(context, 4);
        ES_Value_Internal &vstrings = regs[0], &temporary = regs[1], &vvalues = regs[2];
        unsigned *old_index, *new_index, *tmp_index, undefineds = 0, count = 0, index;
        BOOL unchanged = TRUE;

  #ifdef ES_MOVABLE_OBJECTS
  # error "These (old|new|tmp)_index don't work!"
  #endif // ES_MOVABLE_OBJECTS

        regs[3].SetBoxed(ES_Box::Make(context, used * 3 * sizeof(unsigned)));
        old_index = reinterpret_cast<unsigned*>(static_cast<ES_Box*>(regs[3].GetBoxed())->Unbox());
        new_index = old_index + used;
        tmp_index = new_index + used;

        unsigned dummy_serial = 0;
        ES_Properties *properties = ES_Properties::Make(context, used, 0, this_object, dummy_serial);
        ES_Value_Internal *values = properties->slots;

        vvalues.SetBoxed(properties);

        if (!comparefn)
        {
            ES_Boxed_Array *boxed_array = ES_Boxed_Array::Make(context, used, 0);
            ES_Boxed **slot;
            JString **strings;

            vstrings.SetBoxed(boxed_array);
            strings = reinterpret_cast<JString **>(slot = boxed_array->slots);

            ES_Array_Property_Iterator iter(context, this_object, length);
            ES_Value_Internal *value = values;
            unsigned *oldi = old_index, *newi = new_index;

            while (iter.Next(index))
            {
                if (!iter.GetValue(*value))
                    goto return_false;
                if (value->IsUndefined())
                    ++undefineds;
                else
                {
                    temporary = *value;
                    if (!temporary.ToString(context))
                        goto return_false;
                    *slot++ = temporary.GetString();
                    *oldi++ = *newi++ = count++;
                    ++properties->used;
                    ++boxed_array->nused;
                    ++value;
                }
            }

            count = boxed_array->nused;

            MergeSortStrings(context, strings, new_index, tmp_index, count, unchanged);
        }
        else
        {
            ES_Array_Property_Iterator iter(context, this_object, length);
            ES_Value_Internal *value = values;
            unsigned *oldi = old_index, *newi = new_index;

            while (iter.Next(index))
            {
                if (!iter.GetValue(*value))
                    goto return_false;
                if (value->IsUndefined())
                    ++undefineds;
                else
                {
                    *oldi++ = *newi++ = count++;
                    ++properties->used;
                    ++value;
                }
            }

            ES_FunctionCall compare(context, ES_Value_Internal(), comparefn, 2);

            compare.Initialize();

            if (!MergeSort(context, context->Code(), &compare, values, new_index, tmp_index, count, temporary, unchanged))
                goto return_false;
        }

        if (indexed_properties && ES_Indexed_Properties::GetType(indexed_properties) == ES_Indexed_Properties::TYPE_SPARSE)
        {
            ES_Indexed_Property_Iterator iter(context, this_object, indexed_properties);

            if (!iter.LowerBound(index, length))
            {
                temporary.SetBoxed(indexed_properties);

                unsigned capacity = 8, needed = count + undefineds;

                while (capacity < needed)
                    capacity += capacity;

                this_object->SetIndexedProperties(indexed_properties = ES_Compact_Indexed_Properties::Make(context, capacity));
            }
        }

        if (count == length)
        {
            if (!unchanged)
                for (index = 0; index < count; ++index)
                    if (index != new_index[index] && this_object->PutL(context, index, values[new_index[index]]) == PROP_PUT_FAILED)
                        goto return_false;
        }
        else
            for (index = 0; index < count; ++index)
                if (this_object->PutL(context, index, values[new_index[index]]) == PROP_PUT_FAILED)
                    goto return_false;

        if (undefineds != 0)
        {
            ES_Value_Internal undefined;

            for (; index < count + undefineds; ++index)
                if (this_object->PutL(context, index, undefined) == PROP_PUT_FAILED)
                    goto return_false;
        }

        if (count + undefineds != length)
        {
            ES_Indexed_Property_Iterator iter(context, this_object, indexed_properties);

            if (iter.LowerBound(index, count + undefineds) && index < length)
                do
                    iter.DeleteL(context);
                while (iter.Next(index) && index < length);
        }

        return_value->SetObject(this_object);
        return TRUE;

    return_false:
        return FALSE;
    }

    return_value->SetObject(this_object);
    return TRUE;
}

/* static */ BOOL
ES_ArrayBuiltins::isArray(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (argv[0].IsObject() && argv[0].GetObject(context)->IsArrayObject())
        {
            return_value->SetTrue();
            return TRUE;
        }
    }
    return_value->SetFalse();
    return TRUE;
}

/* static */ void
ES_ArrayBuiltins::PopulateConstructor(ES_Context *context, ES_Global_Object *global_object, ES_Object *constructor)
{
    constructor->InitPropertyL(context, context->rt_data->idents[ESID_isArray], MAKE_BUILTIN_NO_PROTOTYPE(1, isArray), DE);
}

/* static */ void
ES_ArrayBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_ArrayBuiltins);

    ES_Function *fn_push = MAKE_BUILTIN(1, push);

#ifdef ES_NATIVE_SUPPORT
    fn_push->SetFunctionID(ES_BUILTIN_FN_push);
#endif // ES_NATIVE_SUPPORT

    APPEND_PROPERTY(ES_ArrayBuiltins, length,           undefined);
    APPEND_PROPERTY(ES_ArrayBuiltins, constructor,      undefined);

    APPEND_PROPERTY(ES_ArrayBuiltins, toString,         MAKE_BUILTIN(0, toString));
    APPEND_PROPERTY(ES_ArrayBuiltins, toLocaleString,   MAKE_BUILTIN(0, toLocaleString));
    APPEND_PROPERTY(ES_ArrayBuiltins, join,             MAKE_BUILTIN(1, join));

    APPEND_PROPERTY(ES_ArrayBuiltins, forEach,          MAKE_BUILTIN(1, forEach));
    APPEND_PROPERTY(ES_ArrayBuiltins, every,            MAKE_BUILTIN(1, every));
    APPEND_PROPERTY(ES_ArrayBuiltins, some,             MAKE_BUILTIN(1, some));
    APPEND_PROPERTY(ES_ArrayBuiltins, map,              MAKE_BUILTIN(1, map));
    APPEND_PROPERTY(ES_ArrayBuiltins, filter,           MAKE_BUILTIN(1, filter));
    APPEND_PROPERTY(ES_ArrayBuiltins, reduce,           MAKE_BUILTIN(1, reduce));
    APPEND_PROPERTY(ES_ArrayBuiltins, reduceRight,      MAKE_BUILTIN(1, reduceRight));

    APPEND_PROPERTY(ES_ArrayBuiltins, push,             fn_push);
    APPEND_PROPERTY(ES_ArrayBuiltins, pop,              MAKE_BUILTIN(0, pop));
    APPEND_PROPERTY(ES_ArrayBuiltins, shift,            MAKE_BUILTIN(0, shift));
    APPEND_PROPERTY(ES_ArrayBuiltins, unshift,          MAKE_BUILTIN(1, unshift));

    APPEND_PROPERTY(ES_ArrayBuiltins, indexOf,          MAKE_BUILTIN(1, indexOf));
    APPEND_PROPERTY(ES_ArrayBuiltins, lastIndexOf,      MAKE_BUILTIN(1, lastIndexOf));

    APPEND_PROPERTY(ES_ArrayBuiltins, slice,            MAKE_BUILTIN(2, slice));
    APPEND_PROPERTY(ES_ArrayBuiltins, splice,           MAKE_BUILTIN(2, splice));
    APPEND_PROPERTY(ES_ArrayBuiltins, concat,           MAKE_BUILTIN(1, concat));
    APPEND_PROPERTY(ES_ArrayBuiltins, reverse,          MAKE_BUILTIN(0, reverse));

    APPEND_PROPERTY(ES_ArrayBuiltins, sort,             MAKE_BUILTIN(1, sort));

    ASSERT_OBJECT_COUNT(ES_ArrayBuiltins);
}

/* static */ void
ES_ArrayBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_ArrayBuiltinsCount);

    JString **idents = context->rt_data->idents;
    DECLARE_PROPERTY(ES_ArrayBuiltins, length,          DE | DD,    ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_ArrayBuiltins, constructor,     DE,         ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_ArrayBuiltins, toString,        DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, toLocaleString,  DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, join,            DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, forEach,         DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, every,           DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, some,            DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, map,             DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, filter,          DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, reduce,          DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, reduceRight,     DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, push,            DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, pop,             DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, shift,           DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, unshift,         DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, indexOf,         DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, lastIndexOf,     DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, slice,           DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, splice,          DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, concat,          DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, reverse,         DE,         ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_ArrayBuiltins, sort,            DE,         ES_STORAGE_OBJECT);
}

#undef ES_THIS_OBJECT
#undef ES_CHECK_CALLABLE
#undef ES_GET_LENGTH
#undef ES_PUT_LENGTH
