/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */
/*
 * Copyright (C) Opera Software ASA  1995-2010
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/object/es_clone_object.h"
#include "modules/ecmascript/carakan/src/object/es_regexp_object.h"
#include "modules/ecmascript/carakan/src/object/es_typedarray.h"

#undef GROW
#undef PUSH
#undef POP

#define GROW() do { if (stack_used == stack_size) GrowStackL(); } while (0)
#define PUSH() stack[stack_used++]
#define POP() stack[--stack_used]

ES_CloneToObject::ES_CloneToObject(ES_Runtime *runtime, ES_Context *context)
    : runtime(runtime),
      context(context),
      global(static_cast<ES_Global_Object *>(runtime->GetGlobalObject())),
      stack(NULL),
      stack_used(0),
      stack_size(0)
{
}

/* virtual */
ES_CloneToObject::~ES_CloneToObject()
{
    OP_DELETEA(stack);
}

/* virtual */ void
ES_CloneToObject::PushNullL()
{
    GROW();
    PUSH().SetNull();
}

/* virtual */ void
ES_CloneToObject::PushUndefinedL()
{
    GROW();
    PUSH().SetUndefined();
}

/* virtual */ void
ES_CloneToObject::PushBooleanL(BOOL value)
{
    GROW();
    PUSH().SetBoolean(value);
}

/* virtual */ void
ES_CloneToObject::PushNumberL(double value)
{
    GROW();
    PUSH().SetNumber(value);
}

/* virtual */ void
ES_CloneToObject::PushStringL(const uni_char *value, unsigned length)
{
    JString *string = JString::Make(context, value, length);

    GROW();
    PUSH().SetString(string);

    LEAVE_IF_ERROR(strings.Add(string));
}

/* virtual */ void
ES_CloneToObject::PushStringL(unsigned index)
{
    GROW();
    PUSH().SetString(strings.Get(index));
}

/* virtual */ void
ES_CloneToObject::PushObjectL(unsigned index)
{
    GROW();
    PUSH().SetObject(objects.Get(index));
}

/* virtual */ void
ES_CloneToObject::StartObjectL()
{
    AddObjectL(ES_Object::Make(context, global->GetObjectClass()));
}

/* virtual */ void
ES_CloneToObject::StartObjectBooleanL()
{
    AddObjectL(ES_Boolean_Object::Make(context, global, POP().GetBoolean()));
}

/* virtual */ void
ES_CloneToObject::StartObjectNumberL()
{
    AddObjectL(ES_Number_Object::Make(context, global, POP().GetNumAsDouble()));
}

/* virtual */ void
ES_CloneToObject::StartObjectStringL()
{
    AddObjectL(ES_String_Object::Make(context, global, POP().GetString()));
}

/* virtual */ void
ES_CloneToObject::StartObjectArrayL()
{
    AddObjectL(ES_Array::Make(context, global));
}

/* virtual */ void
ES_CloneToObject::StartObjectRegExpL()
{
    JString *flags_source = POP().GetString();
    JString *pattern_source = POP().GetString();

    RegExpFlags flags;
    unsigned flagbits;

    ES_RegExp_Object::ParseFlags(context, flags, flagbits, flags_source);

    AddObjectL(ES_RegExp_Object::Make(context, global, pattern_source, flags, flagbits));
}

/* virtual */ void
ES_CloneToObject::StartObjectDateL()
{
    AddObjectL(ES_Date_Object::Make(context, global, POP().GetNumAsDouble()));
}

/* virtual */ void
ES_CloneToObject::AddPropertyL(int attributes)
{
    ES_Value_Internal value(POP());
    ES_Value_Internal name(POP());
    ES_Object *object = stack[stack_used - 1].GetObject();

    attributes &= ~(RO|DD);

    if (name.IsString())
        object->PutNativeL(context, name.GetString(), attributes, value);
    else
        object->PutNativeL(context, name.GetNumAsUInt32(), attributes, value);
}

/* virtual */ BOOL
ES_CloneToObject::HostObjectL(EcmaScript_Object *source_object)
{
    return HostObjectL(source_object, NULL);
}

/* virtual */ void
ES_CloneToObject::StartObjectArrayBufferL(const unsigned char *value, unsigned length)
{
    ES_ArrayBuffer *clone = ES_ArrayBuffer::Make(context, global, length, NULL, FALSE/*do not zero initialize*/);
    if (!clone)
        LEAVE(OpStatus::ERR_NO_MEMORY);

    op_memcpy(clone->GetStorage(), value, length);
    AddObjectL(clone);
}

/* virtual */ void
ES_CloneToObject::StartObjectTypedArrayL(unsigned kind, unsigned offset, unsigned size)
{
    ES_ArrayBuffer *cloned_array_buffer = static_cast<ES_ArrayBuffer *>(POP().GetObject());
    OP_ASSERT(kind <= static_cast<unsigned>(ES_TypedArray::ES_TypedArrayCount));
    ES_TypedArray *clone = ES_TypedArray::Make(context, global, static_cast<ES_TypedArray::Kind>(kind), cloned_array_buffer, offset, size);
    if (!clone)
        LEAVE(OpStatus::ERR_NO_MEMORY);
    AddObjectL(clone);
}

#ifdef ES_PERSISTENT_SUPPORT

/* virtual */ BOOL
ES_CloneToObject::HostObjectL(ES_PersistentItem *source_item)
{
    return HostObjectL(NULL, source_item);
}

#endif // ES_PERSISTENT_SUPPORT

void
ES_CloneToObject::GrowStackL()
{
    if (stack_size == 0)
        stack_size = 16;
    else
        stack_size += stack_size;

    ES_Value_Internal *new_stack = OP_NEWA_L(ES_Value_Internal, stack_size);

    if (stack)
    {
        op_memcpy(new_stack, stack, stack_used * sizeof *stack);
        OP_DELETEA(stack);
    }

    stack = new_stack;
}

void
ES_CloneToObject::AddObjectL(ES_Object *object)
{
    GROW();
    PUSH().SetObject(object);

    LEAVE_IF_ERROR(objects.Add(object));
}

OP_STATUS
ES_CloneToObject::AddTransferObject(ES_Object *object)
{
    return objects.Add(object);
}

BOOL
ES_CloneToObject::HostObjectL(EcmaScript_Object *source_object, ES_PersistentItem *source_item)
{
    ES_HostObjectCloneHandler *handler = g_ecmaManager->GetFirstHostObjectCloneHandler();

    while (handler)
    {
        EcmaScript_Object *target_object;
        OP_STATUS status;

#ifdef ES_PERSISTENT_SUPPORT
        if (source_item)
            status = handler->Clone(source_item, runtime, target_object);
        else
#endif // ES_PERSISTENT_SUPPORT
            status = handler->Clone(source_object, runtime, target_object);

        if (status == OpStatus::OK)
        {
            AddObjectL(*target_object);
            return TRUE;
        }
        else if (status != OpStatus::ERR)
            LEAVE(status);

        handler = g_ecmaManager->GetNextHostObjectCloneHandler(handler);
    }

    return FALSE;
}

#undef GROW
#undef PUSH
#undef POP

ES_CloneFromObject::ES_CloneFromObject(ES_Runtime *runtime, ES_Context *context, const ES_Value_Internal &value, ES_Runtime::CloneStatus &status, BOOL uncloned_to_null)
    : runtime(runtime),
      context(context),
      value(value),
      status(status),
      uncloned_to_null(uncloned_to_null),
      objects_count(0),
      strings(NULL)
{
}

/* virtual */
ES_CloneFromObject::~ES_CloneFromObject()
{
    if (strings)
        ES_Identifier_List::Free(context, strings);
}

/* virtual */ void
ES_CloneFromObject::CloneL(ES_CloneBackend *target0)
{
    strings = ES_Identifier_List::Make(context, 16);
    target = target0;

    OutputValueL(value);
}

void
ES_CloneFromObject::OutputValueL(const ES_Value_Internal &value)
{
    switch (value.Type())
    {
    case ESTYPE_NULL:
        target->PushNullL();
        break;

    case ESTYPE_UNDEFINED:
        target->PushUndefinedL();
        break;

    case ESTYPE_BOOLEAN:
        target->PushBooleanL(value.GetBoolean());
        break;

    case ESTYPE_INT32:
    case ESTYPE_DOUBLE:
        target->PushNumberL(value.GetNumAsDouble());
        break;

    case ESTYPE_STRING:
        OutputStringL(value.GetString());
        break;

    case ESTYPE_OBJECT:
        OutputObjectL(value.GetObject());
        break;

    default:
        OP_ASSERT(FALSE);
    }
}

void
ES_CloneFromObject::OutputStringL(JString *string)
{
    unsigned index;

    if (strings->AppendL(context, string, index))
        target->PushStringL(Storage(context, string), Length(string));
    else
        target->PushStringL(index);
}

OP_STATUS
ES_CloneFromObject::AddTransferObject(ES_Object *object)
{
    return objects.Add(object, reinterpret_cast<int *>(static_cast<UINTPTR>(objects_count++)));
}

void
ES_CloneFromObject::OutputObjectL(ES_Object *object)
{
    int *index;

    if (objects.GetData(object, &index) == OpStatus::OK)
        target->PushObjectL(reinterpret_cast<UINTPTR>(index));
    else
    {
        if (object->IsHostObject())
        {
            if (!target->HostObjectL(static_cast<ES_Host_Object *>(object)->GetHostObject()))
                FailL(ES_Runtime::CloneStatus::OBJ_IS_FOREIGN, object);
            else
                LEAVE_IF_ERROR(objects.Add(object, reinterpret_cast<int *>(static_cast<UINTPTR>(objects_count++))));

            return;
        }

        switch (object->GCTag())
        {
        case GCTAG_ES_Object_Boolean:
            target->PushBooleanL(static_cast<ES_Boolean_Object *>(object)->GetValue());
            target->StartObjectBooleanL();
            break;

        case GCTAG_ES_Object_Number:
            target->PushNumberL(static_cast<ES_Number_Object *>(object)->GetValue());
            target->StartObjectNumberL();
            break;

        case GCTAG_ES_Object_String:
            OutputStringL(static_cast<ES_String_Object *>(object)->GetValue());
            target->StartObjectStringL();
            break;

        case GCTAG_ES_Object_Date:
            target->PushNumberL(static_cast<ES_Date_Object *>(object)->GetValue());
            target->StartObjectDateL();
            break;

        case GCTAG_ES_Object_RegExp:
            OutputStringL(static_cast<ES_RegExp_Object *>(object)->GetSource());
            OutputStringL(static_cast<ES_RegExp_Object *>(object)->GetFlags(context));
            target->StartObjectRegExpL();
            break;

        case GCTAG_ES_Object:
            target->StartObjectL();
            break;

        case GCTAG_ES_Object_Array:
            target->StartObjectArrayL();
            break;

        case GCTAG_ES_Object_Function:
            FailL(ES_Runtime::CloneStatus::OBJ_IS_FUNCTION, object);
            return;

        case GCTAG_ES_Object_ArrayBuffer:
        {
            ES_ArrayBuffer *array_buffer = static_cast<ES_ArrayBuffer *>(object);
            target->StartObjectArrayBufferL(array_buffer->GetStorage(), array_buffer->GetSize());
            break;
        }

        case GCTAG_ES_Object_TypedArray:
        {
            ES_TypedArray *typed_array = static_cast<ES_TypedArray *>(object);
            OutputObjectL(typed_array->GetArrayBuffer());

            target->StartObjectTypedArrayL(typed_array->GetKind(), typed_array->GetOffset(), typed_array->GetSize());
            break;
        }

        case GCTAG_ES_Object_RegExp_CTOR:
        case GCTAG_ES_Object_Error:
        case GCTAG_ES_Object_Arguments:
        case GCTAG_ES_Object_Variables:
            FailL(ES_Runtime::CloneStatus::OBJ_IS_FOREIGN, object);
            return;
        }

        LEAVE_IF_ERROR(objects.Add(object, reinterpret_cast<int *>(static_cast<UINTPTR>(objects_count++))));

        /* Specification says to clone properties on "Object objects" and "Array
           objects". We take that to mean what's matched by the GCTAG_ES_Object
           and GCTAG_ES_Object_Array cases above. */
        if (object->GCTag() == GCTAG_ES_Object || object->GCTag() == GCTAG_ES_Object_Array)
            OutputPropertiesL(object);
    }
}

void
ES_CloneFromObject::OutputPropertiesL(ES_Object *object)
{
    if (ES_Boxed *names = object->GetOwnPropertyNamesL(context, NULL, TRUE, FALSE))
        if (names->GCTag() >= GCTAG_FIRST_CLASS && names->GCTag() <= GCTAG_LAST_CLASS)
        {
            ES_Class *cls = static_cast<ES_Class *>(names);

            JString *name;
            ES_Property_Info info;

            for (unsigned index = 0; index < object->Count(); ++index)
            {
                cls->Lookup(ES_PropertyIndex(index), name, info);

                if (info.IsDontEnum())
                    continue;

                OutputStringL(name);

                if (info.IsSpecial())
                {
                    ES_Value_Internal value;

                    if (!GET_OK(static_cast<ES_Special_Property *>(object->GetPropertyValueAtIndex(ES_PropertyIndex(index)).GetBoxed())->SpecialGetL(context, object, value, object)))
                        FailL(ES_Runtime::CloneStatus::OBJ_HAS_GETTER_SETTER, object);

                    OutputValueL(value);
                }
                else
                    OutputValueL(object->GetPropertyValueAtIndex(ES_PropertyIndex(index)));

                target->AddPropertyL(info.Attributes());
            }
        }
        else if (names->GCTag() == GCTAG_ES_Compact_Indexed_Properties ||
                 names->GCTag() == GCTAG_ES_Sparse_Indexed_Properties)
        {
            ES_Indexed_Properties *indexed = static_cast<ES_Indexed_Properties *>(names);
            ES_Indexed_Property_Iterator iterator(context, NULL, indexed);
            ES_Value_Internal name;
            ES_Value_Internal value;
            ES_Property_Info info;
            unsigned index;

            while (iterator.Next(index))
            {
                iterator.GetValue(name);

                OutputValueL(name);

                BOOL found;

                if (name.IsString())
                    found = object->GetNativeL(context, name.GetString(), value, &info);
                else
                    found = object->GetNativeL(context, name.GetNumAsUInt32(), value);

                if (!found)
                    FailL(ES_Runtime::CloneStatus::OBJ_HAS_GETTER_SETTER, object);
                else
                    OutputValueL(value);

                target->AddPropertyL(info.Attributes());
            }
        }
}

void
ES_CloneFromObject::FailL(ES_Runtime::CloneStatus::FaultReason reason, ES_Object *object)
{
    if (uncloned_to_null)
        target->PushNullL();
    else
    {
        status.fault_reason = reason;
        status.fault_object = object;

        LEAVE(OpStatus::ERR_NOT_SUPPORTED);
    }
}
