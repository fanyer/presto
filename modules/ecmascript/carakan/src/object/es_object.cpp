/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_object_inlines.h"
#include "modules/ecmascript/carakan/src/object/es_host_object.h"
#include "modules/ecmascript/carakan/src/object/es_special_property.h"
#include "modules/ecmascript/carakan/src/builtins/es_object_builtins.h"

#ifdef ES_DEBUG_COMPACT_OBJECTS
unsigned g_total_type_changes = 0;

void
DebugVerifyObject(ES_Object *object)
{
    ES_Class *klass = object->klass;
    unsigned count = object->Count();

    for (unsigned i = 0; i < count; ++i)
    {
        ES_Layout_Info layout = klass->GetLayoutInfoAtIndex(ES_LayoutIndex(i));
        ES_StorageType type = layout.GetStorageType();
        unsigned offset = layout.GetOffset();

        if (i + 1 < count)
            OP_ASSERT(offset + ES_Layout_Info::SizeOf(type) <= klass->GetLayoutInfoAtIndex(ES_LayoutIndex(i + 1)).GetOffset());
        else
            OP_ASSERT(object->IsVariablesObject() || offset + ES_Layout_Info::SizeOf(type) <= object->Capacity());

        if (layout.IsBoxedType())
        {
            ES_Boxed *box = *reinterpret_cast<ES_Boxed**>(object->properties + offset);

            if (type == ES_STORAGE_OBJECT || type == ES_STORAGE_STRING)
            {
                OP_ASSERT(box != NULL);
                OP_ASSERT(box->GCTag() == GCTAG_JString || type != ES_STORAGE_STRING);
            }

            if (box == NULL)
                OP_ASSERT(type == ES_STORAGE_NULL || type == ES_STORAGE_STRING_OR_NULL || type == ES_STORAGE_OBJECT_OR_NULL);
        }
    }
}
#endif // ES_DEBUG_COMPACT_OBJECTS

ES_Object *
ES_Object::Make(ES_Context *context, ES_Class *klass, unsigned size)
{
    OP_ASSERT(!klass->Prototype() || klass->Prototype() && !klass->Prototype()->HasMultipleIdentities());

    ES_Class *main_class = klass->Level() > 0 ? klass : klass->GetMainClass();
    unsigned used = klass->Count();

    if (size == 0)
    {
        if (main_class)
            used = size = main_class->Count();
        else
            size = used;

        if (size == 0)
            size = 4;
    }

    ES_Object *object;

    GC_STACK_ANCHOR(context, klass);

    unsigned mainbytes = GC_ALIGN(sizeof(ES_Object));
    unsigned namedbytes = GC_ALIGN(ES_Object::AllocationSize(main_class ? main_class : klass, used, size));

    if (mainbytes + namedbytes < LARGE_OBJECT_LIMIT)
    {
        ES_Box *properties;
        GC_ALLOCATE_WITH_EXTRA_ALIGNED(context, object, properties, namedbytes, ES_Object, ES_Box, (object, klass), (properties));
        Initialize(properties);

        object->SetProperties(properties);
    }
    else
    {
        GC_ALLOCATE(context, object, ES_Object, (object, klass));
        ES_CollectorLock gclock(context);

        ES_Box *properties = ES_Box::Make(context, namedbytes);
        Initialize(properties);

        object->SetProperties(properties);
    }

    OP_ASSERT_PROPERTY_COUNT(object);
    return object;
}

ES_Object *
ES_Object::MakeArray(ES_Context *context, ES_Class *klass, unsigned indexed_capacity)
{
    ES_Object *object;

    //GC_STACK_ANCHOR(context, klass);

    unsigned named_capacity = klass->Count();
    unsigned mainbytes = GC_ALIGN(sizeof(ES_Object));
    unsigned namedbytes = GC_ALIGN(ES_Object::AllocationSize(klass, named_capacity, named_capacity));
    unsigned indexedbytes = indexed_capacity != 0 ? GC_ALIGN(ES_Compact_Indexed_Properties::AllocationSize(indexed_capacity)) : 0;

    unsigned extrabytes = namedbytes + indexedbytes;

    if (mainbytes + extrabytes < LARGE_OBJECT_LIMIT)
    {
        GC_ALLOCATE_WITH_EXTRA(context, object, extrabytes, ES_Object, (object, klass));

        ES_Box *properties = reinterpret_cast<ES_Box *>(reinterpret_cast<char *>(object) + mainbytes);

        object->SplitAllocation(properties);

        Initialize(properties);

        object->SetProperties(properties);

        if (indexed_capacity != 0)
        {
            ES_Compact_Indexed_Properties *indexed = reinterpret_cast<ES_Compact_Indexed_Properties *>(reinterpret_cast<char *>(properties) + namedbytes);

            properties->SplitAllocation(indexed);

            ES_Compact_Indexed_Properties::Initialize(indexed, indexed_capacity);

            object->SetPlainCompactIndexedProperties(indexed);

            for (unsigned index = 0; index < indexed_capacity; ++index)
                indexed->values[index].SetUndefined(FALSE);
        }
    }
    else
    {
        object = ES_Object::Make(context, klass);

        if (indexed_capacity != 0)
        {
            ES_CollectorLock gclock(context);

            object->SetPlainCompactIndexedProperties(ES_Compact_Indexed_Properties::Make(context, indexed_capacity));
        }
    }

    OP_ASSERT_PROPERTY_COUNT(object);
    return object;
}

/* static */ ES_Object *
ES_Object::MakePrototypeObject(ES_Context *context, ES_Class *&instance, ES_Object *parent_prototype, const char *class_name, unsigned size)
{
    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, parent_prototype, class_name, size);

    ES_CollectorLock gclock(context);

    ES_Object *prototype = ES_Object::Make(context, prototype_class, size);

    prototype_class->AddInstance(context, prototype);

    instance = ES_Class::MakeRoot(context, prototype);
    prototype->SetInstances(instance);

    if (parent_prototype)
    {
        parent_prototype->AddInstance(context, prototype_class, TRUE);

        if (parent_prototype->HasGetterOrSetter())
            prototype->SetHasGetterOrSetter();
    }

    OP_ASSERT_PROPERTY_COUNT(prototype);
    return prototype;
}

/* static */ ES_Object *
ES_Object::MakePrototypeObject(ES_Context *context, ES_Class *&instance)
{
    ES_Object *prototype_object = ES_Object::MakePrototypeObject(context, instance, NULL, "Object", ES_ObjectBuiltins::ES_ObjectBuiltinsCount);

    ES_ObjectBuiltins::PopulatePrototypeClass(context, static_cast<ES_Class_Singleton *>(prototype_object->Class()));
    ES_CollectorLock gclock(context);
    prototype_object->AllocateProperties(context);

    OP_ASSERT_PROPERTY_COUNT(prototype_object);
    return prototype_object;
}

void
ES_Object::Initialize(ES_Object *self, ES_Class *klass)
{
    self->InitGCTag(GCTAG_ES_Object);

    self->object_bits = 0;
    self->klass = klass;
    self->properties = NULL;
    self->indexed_properties = NULL;
    self->property_count = klass ? klass->Count() : 0;
}

void
ES_Object::InitPropertyL(ES_Context *context, JString *name, const ES_Value_Internal &value, unsigned attributes, ES_StorageType type, BOOL is_new)
{
    ES_Property_Info info;

    ES_Value_Internal_Ref value_ref;
    if (!is_new && GetOwnLocation(name, info, value_ref))
    {
        OP_ASSERT(attributes == 0 || info.Check(attributes | CP));

        value_ref.Write(value);
    }
    else
    {
        ES_PropertyIndex index;
        AppendValueL(context, name, index, value, attributes, type);
    }

    OP_ASSERT_PROPERTY_COUNT(this);
}

void
ES_Object::InitPropertyL(ES_Context *context, unsigned index, const ES_Value_Internal &value, unsigned attributes)
{
    ES_Value_Internal *storage;
    ES_Indexed_Properties *new_properties = ES_Indexed_Properties::PutL(context, indexed_properties, index, &attributes, storage);

    if (value.IsUndefined())
        storage->SetUndefined(TRUE);
    else
        *storage = value;

    if (value.IsSpecial())
        new_properties->SetHasSpecialProperties();
    if (attributes & RO)
        new_properties->SetHasReadOnlyProperties();

    SetIndexedProperties(new_properties);
}

PutResult
ES_Object::PutL(ES_Execution_Context *context, const ES_Value_Internal &this_value, JString *name, const ES_Value_Internal &value, ES_PropertyIndex &index)
{
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->PutHostL(context, name, value, TRUE, index);

    ES_Property_Info info;
    ES_Value_Internal_Ref value_ref;

    if (GetOwnLocation(name, info, value_ref))
    {
        if (info.IsSpecial())
            return static_cast<ES_Special_Property *>(value_ref.GetBoxed())->SpecialPutL(context, this, value, this_value);
        else if (info.IsReadOnly())
            return PROP_PUT_READ_ONLY;

        if (!info.IsClassProperty())
        {
            OP_ASSERT(HasHashTableProperties());

            unsigned serial_number;
            static_cast<ES_Class_Hash_Base *>(klass)->GetSerialNr(info, serial_number);
            static_cast<ES_Class_Hash_Base *>(klass)->Delete(name);

            AppendOwnProperty(context, name, index, value);

            static_cast<ES_Class_Hash_Base *>(klass)->SetSerialNrAtIndex(index, serial_number);

            SetHasUnorderedProperties();

            if (IsPrototypeObject() || (value.IsObject() && value.GetObject()->IsNativeFunctionObject()))
                return PROP_PUT_OK;

            return PROP_PUT_OK_CAN_CACHE;
        }

        PutResult result = PROP_PUT_OK_CAN_CACHE;

        index = value_ref.Index();

        if (info.IsFunction())
            result = ResetOwnFunction(context, name, info, value_ref, value);

        if (!value_ref.CheckType(value.Type()))
        {
            OP_ASSERT(!IsVariablesObject());
            ConvertProperty(context, value_ref, value.GetStorageType());
        }

        value_ref.Write(value);

        return result;
    }

    ES_Object *owner;
    BOOL is_secure = FALSE;
    ES_Object *prototype = klass->Prototype();
    if (prototype && prototype->HasPropertyWithInfo(context, name, info, owner, is_secure))
    {
        if (!is_secure)
        {
            context->ThrowReferenceError("Security error: attempted to write protected variable: ", Storage(context, name), Length(name));
            return PROP_PUT_FAILED;
        }

        if (owner->GetOwnLocation(name, info, value_ref) && info.IsSpecial() && value_ref.IsGetterOrSetter())
            return static_cast<ES_Special_Property *>(value_ref.GetBoxed())->SpecialPutL(context, owner, value, this_value);
        else if (info.IsReadOnly())
            return PROP_PUT_READ_ONLY;
    }

    if (!IsExtensible())
        return PROP_PUT_READ_ONLY;

    AppendOwnProperty(context, name, index, value);

    if (!IsSingleton() && property_count > ES_OBJECT_PROPERTY_COUNT_LIMIT)
    {
        ConvertToSingletonObject(context);
        return PROP_PUT_OK;
    }

    if (IsPrototypeObject())
        return PROP_PUT_OK;

    if (IsSingleton() || HasHashTableProperties())
        return PROP_PUT_OK_CAN_CACHE;
    else
        return PROP_PUT_OK_CAN_CACHE_NEW;
}

void
ES_Object::AppendOwnProperty(ES_Context *context, JString *name, ES_PropertyIndex &index, const ES_Value_Internal &value)
{
    ES_CollectorLock gclock(context, TRUE);

    unsigned attributes = 0;

    if (value.IsSpecial())
        attributes |= SP;

    if (IsSingleton())
        if ((value.IsObject() && value.GetObject()->IsNativeFunctionObject()))
            attributes |= FN;

    AppendValueL(context, name, index, value, attributes, value.GetStorageType());

    Invalidate();

    OP_ASSERT_PROPERTY_COUNT(this);
}

PutResult
ES_Object::PutNonClassL(ES_Execution_Context *context, JString *name, const ES_Value_Internal &value)
{
    ES_CollectorLock gclock(context);
    BOOL force_class = IsPrototypeObject();

    ES_PropertyIndex index;
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->PutHostL(context, name, value, force_class, index);
    else if (force_class)
        return PutL(context, name, value);

    ES_Property_Info info;
    ES_Value_Internal_Ref value_ref;

    if (GetOwnLocation(name, info, value_ref))
    {
        if (info.IsSpecial())
            return static_cast<ES_Special_Property *>(value_ref.GetBoxed())->SpecialPutL(context, this, value, this);
        else if (info.IsReadOnly())
            return PROP_PUT_READ_ONLY;

        if (info.IsFunction())
            ResetOwnFunction(context, name, info, value_ref, value);

        if (!value_ref.CheckType(value.Type()))
        {
            OP_ASSERT(!IsVariablesObject());
            ConvertProperty(context, value_ref, value.GetStorageType());
        }

        value_ref.Write(value);

        return PROP_PUT_OK;
    }

    ES_Object *owner = NULL;
    BOOL is_secure = FALSE;

    ES_Object *prototype = klass->Prototype();
    if (prototype && prototype->HasPropertyWithInfo(context, name, info, owner, is_secure))
    {
        if (!is_secure)
        {
            context->ThrowReferenceError("Security error: attempted to write protected variable: ", Storage(context, name), Length(name));
            return PROP_PUT_FAILED;
        }

        if (owner->GetOwnLocation(name, info, value_ref) && value_ref.IsGetterOrSetter())
            return static_cast<ES_Special_Property *>(value_ref.GetBoxed())->SpecialPutL(context, owner, value, this);
        else if (info.IsReadOnly())
            return PROP_PUT_READ_ONLY;
    }

    info.Reset();

    if (!HasHashTableProperties())
        klass = ES_Class::MakeHash(context, klass, Count());

    static_cast<ES_Class_Hash_Base *>(klass)->InsertL(context, name, value, info, Count());

    OP_ASSERT_PROPERTY_COUNT(this);

    return PROP_PUT_OK;
}

PutResult
ES_Object::PutCachedNew(ES_Context *context, unsigned offset, unsigned type, const ES_Value_Internal &value, ES_Class *new_class)
{
    OP_ASSERT(new_class != NULL);
    OP_ASSERT(!klass->IsSingleton() && !klass->HasHashTableProperties() && !IsPrototypeObject());

    ES_Layout_Info layout(offset, ES_Value_Internal::StorageTypeFromCachedTypeBits(type));

    return PutCachedNew(context, layout, value, new_class);
}

PutResult
ES_Object::PutCachedNew(ES_Context *context, const ES_Layout_Info &layout, const ES_Value_Internal &value, ES_Class *new_class)
{
    unsigned capacity = properties != NULL ? Capacity() : 0;
    if (layout.GetNextOffset() > capacity)
        Grow(context, new_class, capacity, layout.GetNextOffset());

    OP_ASSERT(layout.GetNextOffset() <= Capacity());

    PutCached(layout, value);

    klass = new_class;

    property_count++;

    OP_ASSERT_PROPERTY_COUNT(this);

    return PROP_PUT_OK;
}

GetResult
ES_Object::GetL(ES_Execution_Context *context, const ES_Value_Internal &this_value, JString *name, ES_Value_Internal &value, ES_Object *&prototype, ES_PropertyIndex &index)
{
    ES_Object *object = this;
    ES_Value_Internal local_value;

    prototype = NULL;

    while (TRUE)
    {
        if (object->IsHostObject())
            return static_cast<ES_Host_Object *>(object)->GetHostL(context, this, name, value, prototype, index);

        GetResult result = object->GetOwnPropertyL(context, this, this_value, name, local_value, index);
        if (GET_FOUND(result))
        {
            value = local_value;
            return result;
        }
        prototype = object = object->klass->Prototype();

        if (!object)
            break;
    }

    value.SetUndefined();
    return PROP_GET_NOT_FOUND_CAN_CACHE;
}

GetResult
ES_Object::GetL(ES_Execution_Context *context, const ES_Value_Internal &this_value, unsigned index, ES_Value_Internal &value)
{
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->GetHostL(context, this, index, value);

    ES_Value_Internal local_value;

    GetResult result = GetOwnPropertyL(context, this, this_value, index, local_value);
    if (GET_FOUND(result))
    {
        value = local_value;
        return result;
    }

    for (ES_Object *object = klass->Prototype(); object != NULL; object = object->klass->Prototype())
    {
        if (object->IsHostObject())
            return static_cast<ES_Host_Object *>(object)->GetHostL(context, this, index, value);

        result = object->GetOwnPropertyL(context, this, this_value, index, local_value);
        if (GET_FOUND(result))
        {
            value = local_value;
            return result;
        }
    }

    value.SetUndefined();
    return PROP_GET_NOT_FOUND;
}

GetResult
ES_Object::GetNonClassL(ES_Execution_Context *context, JString *name, ES_Value_Internal &value)
{
    OP_ASSERT(HasHashTableProperties());

    ES_Property_Info info;
    ES_Value_Internal *value_loc;

    if (static_cast<ES_Class_Hash_Base *>(klass)->FindLocation(name, info, value_loc))
        if (value_loc->IsSpecial())
            return static_cast<ES_Special_Property *>(value_loc->GetDecodedBoxed())->SpecialGetL(context, this, value, this);
        else
        {
            value = *value_loc;
            return PROP_GET_OK;
        }
    else
        return PROP_GET_NOT_FOUND;
}

GetResult
ES_Object::GetOwnPropertyL(ES_Execution_Context *context, ES_Object *this_object, const ES_Value_Internal &this_value, JString *name, ES_Value_Internal &value, ES_PropertyIndex &index)
{
    ES_Property_Info info;

    ES_Value_Internal_Ref value_ref;

    if (GetOwnLocation(name, info, value_ref))
    {
        if (info.IsSpecial())
            return static_cast<ES_Special_Property *>(value_ref.GetBoxed())->SpecialGetL(context, this, value, this_value);

        value_ref.Read(value);

        if (!info.IsClassProperty())
            return PROP_GET_OK;

        index = value_ref.Index();

        return PROP_GET_OK_CAN_CACHE;
    }
    return PROP_GET_NOT_FOUND_CAN_CACHE;
}

BOOL
ES_Object::GetOwnLocationL(ES_Context *context, unsigned index, ES_Property_Info &info, ES_Value_Internal &value)
{
    info.Reset();

    if (ES_Indexed_Properties::HasProperty(indexed_properties, index, info))
    {
        value = *ES_Indexed_Properties::GetStorage(indexed_properties, index);
        return TRUE;
    }

    if (IsStringObject())
    {
        info.Set(RO | DD);

        JString *result;
        if (ES_String_Object::GetSubString(context, static_cast<ES_String_Object *>(this), index, 1, result))
        {
            value.SetString(result);
            return TRUE;
        }
    }

    return FALSE;
}

BOOL
ES_Object::GetOwnLocation(JString *name, ES_Property_Info &info, ES_Value_Internal_Ref &value_ref)
{
    ES_Value_Internal *value_loc;
    if (HasHashTableProperties() && static_cast<ES_Class_Hash_Base *>(klass)->FindLocation(name, info, value_loc))
    {
        OP_ASSERT(!info.IsClassProperty());
        value_ref.Set(reinterpret_cast<char*>(value_loc), ES_STORAGE_WHATEVER, ES_PropertyIndex(UINT_MAX));
        return TRUE;
    }
    else if (klass->Find(name, info, Count()))
    {
        OP_ASSERT(info.IsClassProperty());
        ES_Layout_Info layout = klass->GetLayoutInfo(info);
        // think of specialising ES_Class::Find to set index later
        value_ref.Set(properties, layout, klass->IndexOf(name));
        return TRUE;
    }
    else
        return FALSE;
}

GetResult
ES_Object::GetWithLocationL(ES_Execution_Context *context, ES_Object *this_object, ES_Property_Info info, ES_Value_Internal_Ref &value_ref, ES_Value_Internal &value)
{
    if (info.IsSpecial())
    {
        OP_ASSERT(value_ref.IsBoxed());
        return static_cast<ES_Special_Property *>(value_ref.GetBoxed())->SpecialGetL(context, this, value, this_object);
    }

    value_ref.Read(value);
    return info.IsClassProperty() ? PROP_GET_OK_CAN_CACHE : PROP_GET_OK;
}

PutResult
ES_Object::PutWithLocation(ES_Execution_Context *context, ES_Object *this_object, ES_Property_Info info, ES_Value_Internal_Ref &value_ref, const ES_Value_Internal &value)
{
    if (info.IsSpecial())
        return static_cast<ES_Special_Property *>(value_ref.GetBoxed())->SpecialPutL(context, this, value, this_object);
    else if (info.IsReadOnly())
        return PROP_PUT_READ_ONLY;

    OP_ASSERT(!info.IsFunction());

    if (!value_ref.CheckType(value.Type()))
    {
        OP_ASSERT(!IsVariablesObject());
        ConvertProperty(context, value_ref, value.GetStorageType());
    }

    value_ref.Write(value);

    OP_ASSERT_PROPERTY_COUNT(this);

    return PROP_PUT_OK;
}

PutResult
ES_Object::ResetOwnFunction(ES_Execution_Context *context, JString *name, ES_Property_Info info, ES_Value_Internal_Ref &value_ref, const ES_Value_Internal &value)
{
    OP_ASSERT(value_ref.IsObject() && value_ref.GetObject()->IsFunctionObject());
    BOOL was_inlined = value_ref.GetObject()->HasBeenInlined();

    info.ClearIsFunction();

    BOOL needs_conversion = FALSE;

    ES_Layout_Info old_layout = klass->GetLayoutInfo(info);
    ES_Class *old_klass = ES_Class::ActualClass(klass);

    ES_Class *new_klass = ES_Class::ChangeAttribute(context, klass, name, info, Count(), needs_conversion);

    if (needs_conversion)
        ConvertObject(context, old_klass, new_klass);
    else
    {
        int diff = new_klass->GetLayoutInfoAtIndex(info.Index()).Diff(old_layout);
        ConvertObjectSimple(context, new_klass, ES_LayoutIndex(info.Index() + 1), diff, old_klass->SizeOf(Count()), Count());
    }

    value_ref.Update(properties, klass);

    if (was_inlined)
    {
        klass->InvalidateCurrentClass();

        Invalidate();
    }

    return PROP_PUT_OK;
}

GetResult
ES_Object::GetLengthSlow(ES_Execution_Context *context, unsigned &length, BOOL must_exist)
{
    ES_Value_Internal value;
    if (HasIntrinsicLength())
        GetCachedAtIndex(ES_PropertyIndex(0), value);
    else
    {
        ES_Value_Internal self(this);

        GetResult result = self.GetObject(context)->GetL(context, context->rt_data->idents[ESID_length], value);

        if (result == PROP_GET_FAILED)
            return result;
        else if (GET_NOT_FOUND(result))
        {
            length = 0;
            return (must_exist ? result : PROP_GET_OK);
        }
    }
#if 0
    if (value.IsDecodedBoxed() && value.GetDecodedBoxed()->IsSpecial())
    {
        OP_ASSERT(!"implement me: I need a context");
//        return static_cast<ES_Special_Property *>(value.GetDecodedBoxed())->SpecialGetL(context, this, value);
        return PROP_GET_FAILED;
    }
#endif
    if (!value.ToNumber(context))
        return PROP_GET_FAILED;
    length = value.GetNumAsUInt32();
    return PROP_GET_OK;
}

BOOL
ES_Object::HasCycle(ES_Object *maybe_cycle_start)
{
    while (maybe_cycle_start != NULL)
    {
        if (this == maybe_cycle_start)
            return TRUE;
        maybe_cycle_start = maybe_cycle_start->Class()->Prototype();
    }
    return FALSE;
}

PutResult
ES_Object::PutPrototypeL(ES_Execution_Context *context, const ES_Value_Internal &value)
{
    if (!IsExtensible())
    {
        context->ThrowTypeError("Writing to __proto__ on a non-extensible object");
        return PROP_PUT_FAILED;
    }

    ES_Value_Internal v = value;
    ES_Object *prototype;
    ES_CollectorLock gclock(context);
    if (v.IsNull())
        prototype = NULL;
    else if (v.ToObject(context, FALSE))
    {
        prototype = v.GetObject(context);

        if (HasCycle(prototype))
        {
            context->ThrowInternalError("Cycle detected when writing to __proto__");
            return PROP_PUT_FAILED;
        }
    }
    else
        return PROP_PUT_OK;

    klass = ChangePrototype(context, prototype);

    Invalidate();

    return PROP_PUT_OK;
}

void
ES_Object::ChangeClass(ES_Context *context, ES_Class *new_klass, BOOL needs_conversion)
{
    ES_Class *old_klass = klass;

    klass = new_klass;

    if (needs_conversion)
        ConvertObject(context, old_klass, new_klass);
}

void
ES_Object::ChangeAttribute(ES_Context *context, JString *name, const ES_Property_Info &info)
{
    BOOL needs_conversion = FALSE;
    ES_Class *new_klass = ES_Class::ChangeAttribute(context, klass, name, info, property_count, needs_conversion);
    ES_CollectorLock gclock(context);
    ChangeClass(context, new_klass, needs_conversion);
}

void
ES_Object::ChangeAttribute(ES_Context *context, unsigned index, const ES_Property_Info &info)
{
    SetIndexedProperties(ES_Indexed_Properties::ChangeAttribute(context, indexed_properties, index, info));
}

ES_Class *
ES_Object::ChangePrototype(ES_Context *context, ES_Object *prototype)
{
    ES_Object *old_prototype = klass->Prototype();
    if (prototype == old_prototype)
        return klass;

    ES_Class *new_klass;
    ES_Class *sub_object_class = NULL;

    if (!IsSingleton())
    {
        unsigned count = klass->Count();
        new_klass = ES_Class::MakeRoot(context, prototype, klass->ObjectName(), klass->ObjectName(context), FALSE, count);
        sub_object_class = new_klass;

        BOOL needs_conversion = FALSE;

        new_klass = new_klass->CopyProperties(context, klass, ES_LayoutIndex(0), ES_LayoutIndex(count), ES_PropertyIndex(0), ES_LayoutIndex(0), needs_conversion);

        if (needs_conversion)
            ConvertObject(context, klass, new_klass);

        if (HasHashTableProperties())
        {
            new_klass = ES_Class::MakeHash(context, new_klass, count);
            static_cast<ES_Class_Hash_Base *>(new_klass)->CopyHashTableFrom(static_cast<ES_Class_Hash_Base *>(klass));
        }
    }
    else
    {
        new_klass = klass;
        klass->SetPrototype(prototype);
        /* The previous prototype cannot keep 'klass' as an instance.
           Doing so maintains a path back to 'klass' such that if the
           object is reintroduced somewhere on this object's prototype
           chain, a loop is introduced via the class reference. */
        if (old_prototype)
            old_prototype->RemoveSingletonInstance(context->heap, klass);
    }

    if (prototype)
    {
        if (!prototype->IsPrototypeObject())
            prototype->ConvertToPrototypeObject(context, sub_object_class);

        if (IsSingleton())
        {
            prototype->AddInstance(context, new_klass, TRUE);

            /* If the class is added as an instance of another, it must
               be recorded as a prototype class if not already. */
            OP_ASSERT(new_klass->IsSingleton());
            ES_Class_Singleton *klass_single = static_cast<ES_Class_Singleton *>(new_klass);
            if (!context->heap->IsPrototypeClassRecorded(klass_single))
                context->heap->AddPrototypeClass(klass_single);
        }
    }

    return new_klass;
}

BOOL
ES_Object::HasProperty(ES_Context *context, unsigned index)
{
    ES_Object *object = this;

    while (object)
    {
        if (object->HasOwnProperty(context, index))
            return TRUE;
        object = object->Class()->Prototype();
    }

    return FALSE;
}

BOOL
ES_Object::HasProperty(ES_Execution_Context *context, unsigned index, BOOL &is_secure)
{
    ES_Object *self = this;
    BOOL secure = TRUE;
    ES_Property_Info dummy_info;

    while (self != NULL)
    {
        BOOL own_secure = is_secure;
        if (self->HasOwnProperty(context, index, dummy_info, own_secure))
        {
            is_secure = secure || IsCrossDomainAccessible();
            return TRUE;
        }
        self = self->Class()->Prototype();
        secure = secure && own_secure;
    }

    is_secure = secure;
    return FALSE;
}

BOOL
ES_Object::HasProperty(ES_Context *context, JString *name)
{
    ES_Object *object = this;

    while (object)
    {
        if (object->HasOwnProperty(context, name))
            return TRUE;
        object = object->Class()->Prototype();
    }

    return FALSE;
}

BOOL
ES_Object::HasProperty(ES_Execution_Context *context, JString *name, BOOL &is_secure)
{
    ES_Property_Info dummy_info;
    ES_Object *owner;

    return HasPropertyWithInfo(context, name, dummy_info, owner, is_secure);
}

BOOL
ES_Object::HasPropertyWithInfo(ES_Execution_Context *context, JString *name, ES_Property_Info &info, ES_Object *&owner, BOOL &is_secure, BOOL &can_cache)
{
    ES_Object *self = this;
    BOOL secure = TRUE;

    while (self != NULL)
    {
        BOOL own_secure = is_secure;
        if (self->HasOwnProperty(context, name, info, own_secure))
        {
            owner = self;
            is_secure = secure || IsCrossDomainAccessible();
            return TRUE;
        }
        can_cache = can_cache && !self->HasVolatilePropertySet();
        self = self->klass->Prototype();
        secure = secure && own_secure;
    }

    is_secure = secure;
    return FALSE;
}

BOOL
ES_Object::DeleteOwnPropertyL(ES_Context *context, JString *name)
{
    ES_Property_Info info;

    if (klass->Find(name, info, Count()))
    {
        if (info.IsDontDelete())
            return FALSE;

        unsigned size = klass->SizeOf(Count());
        ES_Class *old_klass = ES_Class::ActualClass(klass);

        ES_CollectorLock gclock(context);

        BOOL needs_conversion = FALSE;

        ES_Class *new_klass = ES_Class::DeleteL(context, klass, name, Count(), needs_conversion);

        if (needs_conversion)
        {
            ES_Box *new_properties = ES_Box::Make(context, new_klass->SizeOf(Count() - 1));
            ConvertObject(context, old_klass, new_klass, new_properties, 0, info.Index(), 0);
            ConvertObject(context, old_klass, new_klass, new_properties, info.Index() + 1, Count(), info.Index());
            UpdateProperties(context, new_properties);
            property_count--;
        }
        else
        {
            property_count--;

            if (info.Index() < Count())
            {
                ES_Layout_Info new_layout = klass->GetLayoutInfoAtIndex(info.Index());
                int diff = new_klass->GetPropertyTable()->CalculateAlignedOffset(new_layout.GetStorageType(), info.Index()) - new_layout.GetOffset();
                ConvertObjectSimple(context, new_klass, info.Index(), diff, size, Count());
            }
        }

        klass = new_klass;

        Invalidate();

        OP_ASSERT(!klass->Find(name, info, Count()));
    }
    else if (HasHashTableProperties() && static_cast<ES_Class_Hash_Base *>(klass)->Find(name, info))
    {
        if (info.IsDontDelete())
            return FALSE;
        static_cast<ES_Class_Hash_Base *>(klass)->Delete(name);
    }

    return TRUE;
}

GetResult
ES_Object::SimpleCachedPropertyAccess::GetL(ES_Execution_Context *context, ES_Value_Internal &value)
{
    if (class_id == object->Class()->Id() && index < object->Count())
        return object->GetCachedAtIndex(index, value);
    else
    {
        ES_Object *prototype_object;

        GetResult result = object->GetL(context, object, name, value, prototype_object, index);

        if (result == PROP_GET_OK_CAN_CACHE && !prototype_object)
        {
            class_id = object->Class()->GetId(context);
            return PROP_GET_OK;
        }
        else
            return result;
    }
}

void
ES_Object::CachedPropertyAccess::Initialize(JString *new_name)
{
    name = new_name;
    class_id = ES_Class::NOT_CACHED_CLASS_ID;
    prototype = NULL;
}

GetResult
ES_Object::CachedPropertyAccess::GetL(ES_Execution_Context *context, ES_Object *object, ES_Value_Internal &value)
{
    if (class_id == object->Class()->Id() && cached_index < object->Count())
    {
        if (prototype && ES_Class::HasHashTableProperties(class_id))
        {
            GetResult result = object->GetNonClassL(context, name, value);
            if (!GET_NOT_FOUND(result))
                return result;
        }

        return (prototype ? prototype : object)->GetCachedAtIndex(cached_index, value);
    }
    else
    {
        ES_Object *prototype_object;

        GetResult result = object->GetL(context, object, name, value, prototype_object, cached_index);

        if (result == PROP_GET_OK_CAN_CACHE)
        {
            prototype = prototype_object;
            class_id = object->Class()->GetId(context);
            return PROP_GET_OK;
        }
        else
            return result;
    }
}

BOOL
ES_Object::HasIndexedProperties()
{
    ES_Object *self = this;

    while (self != NULL)
        if (ES_Indexed_Properties::HasUsed(self->indexed_properties))
            return TRUE;
        else
            self = self->klass->Prototype();

    return FALSE;
}

BOOL
ES_Object::HasIndexedGettersOrSetters()
{
    ES_Object *self = this;

    while (self != NULL)
        if (ES_Indexed_Properties::HasGettersOrSetters(self->indexed_properties))
            return TRUE;
        else
            self = self->klass->Prototype();

    return FALSE;
}

InstanceOfResult
ES_Object::InstanceOf(ES_Execution_Context *context, ES_Object *constructor)
{
    ES_Global_Object *global_object = context->GetContextGlobalObject();
    ES_Value_Internal prototype_value;

    if (constructor->IsFunctionObject() && !constructor->IsHostObject())
        if (static_cast<ES_Function *>(constructor)->IsBoundFunction())
            constructor = static_cast<ES_BoundFunction *>(constructor)->GetTargetFunction();

    if (constructor->Class() == global_object->GetNativeFunctionClass() ||
        constructor->Class() == global_object->GetBuiltInConstructorClass() ||
        constructor->Class() == global_object->GetNativeFunctionWithPrototypeClass())
    {
        constructor->GetCachedAtIndex(ES_PropertyIndex(ES_Function::PROTOTYPE), prototype_value);

        if (prototype_value.IsSpecial())
            if (prototype_value.GetDecodedBoxed()->GCTag() == GCTAG_ES_Special_Function_Prototype)
                /* Would create a new prototype object, which wouldn't be found
                   in this object's prototype chain. */
                return INSTANCE_NO;
            else if (!GET_OK(static_cast<ES_Special_Property *>(prototype_value.GetDecodedBoxed())->SpecialGetL(context, constructor, prototype_value, constructor)))
                return INSTANCE_BAD;
    }
    else if (!GET_OK(constructor->GetL(context, context->rt_data->idents[ESID_prototype], prototype_value)))
        return INSTANCE_BAD;

    if (!prototype_value.IsObject())
        return INSTANCE_BAD;

    ES_Object *prototype = prototype_value.GetObject(context);
    ES_Object *iter = Class()->Prototype();

    while (iter)
        if (iter == prototype)
            return INSTANCE_YES;
        else
            iter = iter->Class()->Prototype();

    return INSTANCE_NO;
}

void
ES_Object::PutNativeL(ES_Context *context, JString *name, const ES_Property_Info &new_info, const ES_Value_Internal &new_value)
{
    ES_Property_Info info;
    ES_Value_Internal_Ref value_ref;

    if (GetOwnLocation(name, info, value_ref))
    {
        OP_ASSERT(!info.IsFunction());
        OP_ASSERT(!info.IsSpecial());

        if (!value_ref.CheckType(new_value.Type()))
            ConvertProperty(context, value_ref, new_value.GetStorageType());

        value_ref.Write(new_value);
    }
    else
    {
        ES_CollectorLock gclock(context, TRUE);
        ES_PropertyIndex index;
        AppendValueL(context, name, index, new_value, new_info.Attributes(), ES_STORAGE_WHATEVER);
    }

    OP_ASSERT_PROPERTY_COUNT(this);
}

BOOL ES_Object::GetNativeL(ES_Context *context, JString *name, ES_Value_Internal &value, ES_Property_Info *info, GetNativeStatus *status)
{
    ES_Property_Info local_info;
    ES_Value_Internal_Ref value_ref;

    if (GetOwnLocation(name, info ? *info : local_info, value_ref))
    {
        if (info ? info->IsSpecial() : local_info.IsSpecial())
        {
            if (status)
            {
                ES_Boxed *special = static_cast<ES_Special_Property *>(value_ref.GetBoxed());
                switch (special->GCTag())
                {
                case GCTAG_ES_Special_Mutable_Access:
                    *status = GETNATIVE_SCRIPT_GETTER;
                    break;

                case GCTAG_ES_Special_Function_Arguments:
                case GCTAG_ES_Special_Function_Caller:
                    *status = GETNATIVE_NEEDS_CALLSTACK;
                    break;

                default:
                    static_cast<ES_Special_Property *>(special)->SpecialGetL(context, this, value, this);
                    *status = GETNATIVE_SUCCESS;
                    return TRUE;
                }
            }
            return FALSE;
        }
        if (status)
            *status = GETNATIVE_SUCCESS;
        value_ref.Read(value);
        return TRUE;
    }
    if (status)
        *status = GETNATIVE_NOT_OWN_NATIVE_PROPERTY;
    return FALSE;
}

JString *
ES_Object::GetTypeOf(ES_Context *context)
{
    unsigned stringid;

    if (IsHiddenObject())
        stringid = STRING_undefined;
    else if (IsGlobalObject())
        stringid = STRING_object;
    else if (IsFunctionObject())
        if (IsHostObject() && static_cast<ES_Host_Function *>(this)->GetHostObject()->TypeofYieldsObject())
            stringid = STRING_object;
        else
            stringid = STRING_function;
    else
        stringid = STRING_object;

    return context->rt_data->strings[stringid];
}

ES_Class *
ES_Object::GetSubObjectClass() const
{
    if (GetInstances())
    {
        if (GetInstances()->IsClass())
            return static_cast<ES_Class *>(GetInstances());
        else
        {
            OP_ASSERT(GetInstances()->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table);
            ES_Identifier_Boxed_Hash_Table *sub_classes = static_cast<ES_Identifier_Boxed_Hash_Table *>(GetInstances());
            return static_cast<ES_Class *>(sub_classes->GetValues()->slots[0]);
        }
    }
    return NULL;
}

ES_Class *
ES_Object::SetSubObjectClass(ES_Execution_Context *context)
{
    if (!IsPrototypeObject())
        ConvertToPrototypeObject(context);

    return GetSubObjectClass();
}

void
ES_Object::SetSubObjectClass(ES_Context *context, ES_Class *sub_object_class)
{
    OP_ASSERT(!IsPrototypeObject());

    SetInstances(sub_object_class);
}

void
ES_Object::ConvertToSingletonObject(ES_Context *context)
{
    unsigned size = Count();

    ES_Class_Singleton *new_klass = ES_Class::MakeSingleton(context, klass->Prototype(), klass->ObjectName(), klass->ObjectName(context), size);
    ES_CollectorLock gclock(context);

    if (ES_Object *prototype = klass->Prototype())
    {
        prototype->AddInstance(context, new_klass, TRUE);
        context->heap->AddPrototypeClass(new_klass);
    }

    BOOL needs_conversion = FALSE;

    new_klass->CopyProperties(context, klass, ES_LayoutIndex(0), ES_LayoutIndex(size), ES_PropertyIndex(0), ES_LayoutIndex(0), needs_conversion);

    OP_ASSERT(!needs_conversion);

    for (unsigned i = 0; i < new_klass->CountProperties(); ++i)
    {
        ES_PropertyIndex index(i);
        ES_Property_Info &info = new_klass->GetPropertyInfoAtIndex_ref(index);
        ES_Value_Internal_Ref value_ref(properties, klass->GetLayoutInfo(info), index);

        if (value_ref.IsObject() && value_ref.GetObject()->IsNativeFunctionObject())
            info.SetIsFunction();
        else
            OP_ASSERT(!info.IsFunction());
    }

    if (HasHashTableProperties())
    {
        /* Previously properties were copied one by one, remember this if things go wrong */
        new_klass->CopyHashTableFrom(static_cast<ES_Class_Hash_Base *>(klass));
    }

    klass = new_klass;
}

void
ES_Object::ConvertToPrototypeObject(ES_Context *context, ES_Class *sub_object_class)
{
    OP_ASSERT(!IsPrototypeObject());

    ES_CollectorLock gclock(context);

    OP_ASSERT(!sub_object_class || sub_object_class->Prototype() == this);

    if (!IsSingleton())
    {
        ConvertToSingletonObject(context);
        klass->AddInstance(context, this);
    }

    SetInstances(sub_object_class ? sub_object_class : ES_Class::MakeCompactRoot(context, this));

    if (ES_Object *prototype = klass->Prototype())
        if (prototype->HasGetterOrSetter())
            SetHasGetterOrSetter();

    OP_ASSERT_PROPERTY_COUNT(this);
}

void
ES_Object::ConvertObject(ES_Context *context, ES_Class *old_klass, ES_Class *new_klass)
{
    ES_Box *new_properties = ES_Box::Make(context, new_klass->SizeOf(Count()));
    ConvertObject(context, old_klass, new_klass, new_properties , 0, Count(), 0);
    UpdateProperties(context, new_properties);
    klass = new_klass;

    OP_ASSERT_PROPERTY_COUNT(this);
}

void
ES_Object::ConvertObject(ES_Context *context, ES_Class *old_klass, ES_Class *new_klass, ES_Box *new_properties, unsigned from, unsigned to, unsigned start)
{
    OP_ASSERT(!IsSingleton());
    OP_ASSERT(!klass->HasHashTableProperties() || new_klass->HasHashTableProperties());

    ES_CollectorLock gclock(context);

    for (; from < to; ++from, ++start)
    {
        ES_Layout_Info old_layout = old_klass->GetLayoutInfoAtIndex(ES_LayoutIndex(from));
        ES_Layout_Info new_layout = new_klass->GetLayoutInfoAtIndex(ES_LayoutIndex(start));

        OP_ASSERT(old_layout.GetStorageType() == new_layout.GetStorageType() || ES_Layout_Info::IsSubType(old_layout.GetStorageType(), new_layout.GetStorageType())
            || old_layout.GetStorageType() == ES_STORAGE_NULL && ES_Layout_Info::IsNullable(new_layout.GetStorageType()));

        if (old_layout.IsSpecialType())
        {
            OP_ASSERT(new_layout.IsSpecialType());
            op_memcpy(new_properties->Unbox() + new_layout.GetOffset(), properties + old_layout.GetOffset(), ES_Layout_Info::SizeOf(old_layout.GetStorageType()));
        }
        else
        {
            ES_Value_Internal value;
            GetCached(old_layout, value);
            ES_Value_Internal::Memcpy(new_properties->Unbox() + new_layout.GetOffset(), value, new_layout.GetStorageType());
        }
    }

    OP_ASSERT_PROPERTY_COUNT(this);
}

void
ES_Object::ConvertObjectSimple(ES_Context *context, ES_Class *new_klass, ES_LayoutIndex index, int diff, unsigned size, unsigned count)
{
    OP_ASSERT(!new_klass->IsSingleton() || new_klass == klass);

    if (diff == 0)
    {
        klass = new_klass;
        return;
    }

    if (index >= count)
    {
        unsigned capacity = Capacity();
        if (size + diff > capacity)
            Grow(context, new_klass, capacity, size + diff);

        klass = new_klass;

        return;
    }

    unsigned src_unaligned, dst_unaligned, end_unaligned, src_aligned, dst_aligned, end_aligned;
    src_unaligned = dst_unaligned = end_unaligned = src_aligned = dst_aligned = end_aligned = UINT_MAX;

    ES_Layout_Info layout = klass->GetLayoutInfoAtIndex(index);
    unsigned layout_index = index;

    klass = new_klass;

    if (layout.IsAligned() && diff % 8 == 0)
    {
        if (diff != 0)
        {
            /* If this is a singleton object we need to update the offset. See ES_Class_Singleton::{ChangeType, Remove} */
            if (new_klass->IsSingleton())
                new_klass->AdjustOffset(index);

            unsigned capacity = Capacity();
            if (size + diff > capacity)
                Grow(context, new_klass, capacity, size + diff);

            op_memmove(properties + layout.GetOffset() + diff, properties + layout.GetOffset(), size - layout.GetOffset());
        }

        return;
    }
    else
    {
        src_unaligned = end_unaligned = layout.GetOffset();
        dst_unaligned = src_unaligned + diff;

        for (;layout_index < count; ++layout_index)
        {
            layout = new_klass->GetLayoutInfoAtIndex(ES_LayoutIndex(layout_index));
            if (layout.IsAligned())
                break;

            end_unaligned = layout.GetNextOffset();

            /* If this is a singleton object we need to update the offset. See ES_Class_Singleton::{ChangeType, Remove} */
            if (new_klass->IsSingleton())
                new_klass->AdjustOffsetAtIndex(ES_LayoutIndex(layout_index));
        }
    }

    if (layout_index < count)
    {
        OP_ASSERT(layout.IsAligned());
        src_aligned = layout.GetOffset();
        end_aligned = size;

        /* If this is a singleton object we need to update the offset. See ES_Class_Singleton::{ChangeType, Remove} */
        if (new_klass->IsSingleton())
            new_klass->AdjustOffset(ES_LayoutIndex(layout_index));

        dst_aligned = new_klass->GetLayoutInfoAtIndex(ES_LayoutIndex(layout_index)).GetOffset();
    }
    else
        src_aligned = dst_aligned = end_aligned = end_unaligned;

    unsigned size_unaligned = end_unaligned - src_unaligned;
    unsigned size_aligned = end_aligned - src_aligned;

    /* At this point the new klass is correct, regardless of if it is a singleton class or not. */
    unsigned capacity = Capacity();
    if (new_klass->SizeOf(count) > capacity)
        Grow(context, new_klass, capacity, new_klass->SizeOf(count));

    if (dst_unaligned + size_unaligned < src_aligned)
    {
        op_memmove(properties + dst_unaligned, properties + src_unaligned, size_unaligned);
        op_memmove(properties + dst_aligned, properties + src_aligned, size_aligned);
    }
    else
    {
        op_memmove(properties + dst_aligned, properties + src_aligned, size_aligned);
        op_memmove(properties + dst_unaligned, properties + src_unaligned, size_unaligned);
    }

    OP_ASSERT_PROPERTY_COUNT(this);
}

void
ES_Object::ConvertProperty(ES_Context *context, ES_Value_Internal_Ref &value_ref, ES_StorageType new_type)
{
    ES_PropertyIndex index = value_ref.Index();
    ES_Layout_Info old_layout = klass->GetLayoutInfoAtInfoIndex(index);

    BOOL needs_conversion = FALSE;
    ES_Class *old_klass = ES_Class::ActualClass(klass);
    unsigned old_size = old_klass->SizeOf(Count());
    ES_LayoutIndex layout_index = old_klass->GetPropertyInfoAtIndex(index).Index();
    ES_Class *new_klass = ES_Class::ChangeType(context, klass, index, new_type, Count(), needs_conversion);

    ES_CollectorLock gclock(context);

#ifdef ES_DEBUG_COMPACT_OBJECTS
#ifdef _STANDALONE
    extern unsigned g_total_type_changes;
    g_total_type_changes++;

    extern BOOL g_debug_type_changes;
    if (g_debug_type_changes)
    {
        OpString8 string8;
        JString *name = old_klass->GetNameAtIndex(value_ref.Index());
        string8.Set(Storage(context, name), Length(name));

        printf("%s ", string8.CStr());

        extern const char *StorageTypeString(ES_Class *, unsigned, ES_StorageType);
        ES_Layout_Info new_layout = new_klass->GetLayoutInfoAtInfoIndex(index);
        printf("id: %d (%p) -> %d (%p), ", klass->Id(), static_cast<void *>(klass), new_klass->Id(), static_cast<void *>(new_klass));
        printf("type: %s -> %s (%s), ", StorageTypeString(old_klass, index, old_layout.GetStorageType()),
            StorageTypeString(new_klass, index, new_layout.GetStorageType()), StorageTypeString(old_klass, index, new_type));
        if (old_layout.GetOffset() != new_layout.GetOffset())
            printf("offset: 0x%x -> 0x%x, %s cascading\n", old_layout.GetOffset(), new_layout.GetOffset(), needs_conversion ? "is" : "isn't");
        else
            printf("offset: 0x%x, %s cascading\n", new_layout.GetOffset(), needs_conversion ? "is" : "isn't");
    }
#endif // _STANDALONE
#endif // ES_DEBUG_COMPACT_OBJECTS

    OP_ASSERT(old_layout.GetStorageType() != new_klass->GetLayoutInfoAtInfoIndex(index).GetStorageType());
#ifdef DEBUG_ENABLE_OPASSERT
    ES_StorageType debug_new_type = new_klass->GetLayoutInfoAtInfoIndex(index).GetStorageType();
    ES_StorageType debug_old_type = old_klass->GetLayoutInfoAtInfoIndex(index).GetStorageType();
    OP_ASSERT(new_type ==  debug_new_type || ES_Layout_Info::IsSubType(new_type, debug_new_type) || new_type == ES_STORAGE_NULL && ES_Layout_Info::IsNullable(debug_new_type));
    OP_ASSERT(debug_old_type ==  debug_new_type || ES_Layout_Info::IsSubType(debug_old_type, debug_new_type) || debug_old_type == ES_STORAGE_NULL && ES_Layout_Info::IsNullable(debug_new_type));
#endif // DEBUG_ENABLE_OPASSERT

    if (needs_conversion)
        ConvertObject(context, old_klass, new_klass);
    else
    {
        int diff = new_klass->GetLayoutInfoAtIndex(layout_index).Diff(old_layout);
        ConvertObjectSimple(context, new_klass, ES_LayoutIndex(layout_index + 1), diff, old_size, Count());
    }

    value_ref.Update(properties, klass);

    OP_ASSERT_PROPERTY_COUNT(this);
}

void
ES_Object::InvalidateInstances()
{
    OP_ASSERT(IsPrototypeObject());

    if (GetInstances()->IsClass())
        ES_Class::InvalidateTree(static_cast<ES_Class *>(GetInstances()));
    else
    {
        OP_ASSERT(GetInstances()->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table);
        ES_Identifier_Boxed_Hash_Table *sub_classes = static_cast<ES_Identifier_Boxed_Hash_Table *>(GetInstances());
        ES_Boxed_Array *array = sub_classes->GetValues();
        for (unsigned i = 0; i < array->nused; ++i)
        {
            if (array->slots[i]->IsClass())
                ES_Class::InvalidateTree(static_cast<ES_Class *>(array->slots[i]));
            else
            {
                OP_ASSERT(array->slots[i]->GCTag() == GCTAG_ES_Boxed_List);
                for (ES_Boxed_List *instances = static_cast<ES_Boxed_List *>(array->slots[i]); instances; instances = instances->tail)
                    ES_Class::InvalidateTree(static_cast<ES_Class *>(instances->head));
            }
        }
    }
}

void
ES_Object::AddInstance(ES_Context *context, ES_Class *sub_object_class, BOOL is_singleton)
{
    OP_ASSERT(IsPrototypeObject());
    OP_ASSERT(is_singleton || sub_object_class->GCTag() == GCTAG_ES_Class_Node || sub_object_class->IsCompact());

    ES_Identifier_Boxed_Hash_Table *table;

    if (GetInstances()->IsClass())
    {
        table = ES_Identifier_Boxed_Hash_Table::Make(context, 4);
        ES_CollectorLock gclock(context, TRUE);

        // Q: what is the relationship between this class and passed in 'sub_object_class'?
        ES_Class *sub_object_class = static_cast<ES_Class *>(GetInstances());
        JString *name = sub_object_class->ObjectName(context);
        table->AddL(context, name, ES_SubClassTypeInstance, sub_object_class);
        SetInstances(table);
    }
    else
        table = static_cast<ES_Identifier_Boxed_Hash_Table *>(GetInstances());

    OP_ASSERT(GetInstances()->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table);
    OP_ASSERT(table->array->GCTag() == GCTAG_ES_Boxed_Array);

    JString *name = sub_object_class->ObjectName(context);
    ES_CollectorLock gclock(context);
    ES_Boxed *instance;

    if (is_singleton)
    {
        unsigned instance;
        if (static_cast<ES_Identifier_Hash_Table *>(table)->Find(name, instance, ES_SubClassTypeSingletonInstance))
        {
            ES_Boxed_List *instances = static_cast<ES_Boxed_List *>(table->GetValues()->slots[instance]);
            table->GetValues()->slots[instance] = ES_Boxed_List::Make(context, sub_object_class, instances);
        }
        else
            table->AddL(context, name, ES_SubClassTypeSingletonInstance, ES_Boxed_List::Make(context, sub_object_class));
    }
    else if (!table->Find(name, ES_SubClassTypeInstance, instance))
        table->AddL(context, name, ES_SubClassTypeInstance, sub_object_class);
#ifdef _DEBUG
    else
        OP_ASSERT(static_cast<ES_Class *>(instance)->GetRootClass() == sub_object_class->GetRootClass());
#endif


}

ES_Class *
ES_Object::FindInstance(ES_Context *context, JString *name) const
{
    ES_Class *sub_object_class = NULL;
    if (IsPrototypeObject())
    {
        if (GetInstances()->IsClass())
        {
            ES_Class *c = static_cast<ES_Class *>(GetInstances());
            if (name->Equals(c->ObjectName(context)))
                sub_object_class = static_cast<ES_Class *>(GetInstances());
        }
        else
        {
            OP_ASSERT(GetInstances()->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table);
            ES_Boxed *box;
            if (static_cast<ES_Identifier_Boxed_Hash_Table *>(GetInstances())->Find(name, ES_SubClassTypeInstance, box))
                sub_object_class = static_cast<ES_Class *>(box);
        }
    }

    OP_ASSERT(!sub_object_class || sub_object_class->GCTag() == GCTAG_ES_Class_Node || sub_object_class->IsCompact());

    return sub_object_class;
}

void
ES_Object::RemoveSingletonInstance(ES_Heap *heap, ES_Class *sub_object_class)
{
    OP_ASSERT(IsPrototypeObject());
    OP_ASSERT(GetInstances()->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table);

    unsigned index;
    OP_ASSERT(sub_object_class->ObjectName() != NULL);
    JString *name = sub_object_class->ObjectNameString();
    ES_Identifier_Boxed_Hash_Table *table = static_cast<ES_Identifier_Boxed_Hash_Table *>(GetInstances());
    if (static_cast<ES_Identifier_Hash_Table *>(table)->Find(name, index, ES_SubClassTypeSingletonInstance))
    {
        ES_Boxed_List *instances = static_cast<ES_Boxed_List *>(table->GetValues()->slots[index]);
        ES_Boxed_List *previous = NULL;
        while (instances)
        {
            if (instances->head == sub_object_class)
            {
                if (!previous)
                    if (instances->tail)
                        table->GetValues()->slots[index] = instances->tail;
                    else
                        instances->head = NULL;
                else
                {
                    previous->tail = instances->tail;
                    heap->Free(instances);
                }
                return;
            }
            previous = instances;
            instances = instances->tail;
        }
    }
}

ES_Boxed *
ES_Object::GetPropertyNamesL(ES_Context *context, BOOL use_special_cases, BOOL enum_dont_enum)
{
    ES_Boxed_List *list = NULL;
    ES_Boxed_List **tail = &list;

    ES_CollectorLock gclock(context);

    ES_Object *instance = this;
    ES_Object *object = instance;
    ES_Boxed *head = NULL;

    ES_Value_Internal *regs = NULL;
    for (;;)
    {
        if (object->IsHostObject() && !static_cast<ES_Host_Object *>(object)->SecurityCheck(context))
            break;

        if (enum_dont_enum || object->HasEnumerableProperties())
        {
            ES_Boxed *names = NULL;
            ES_Execution_Context *exec_context = context->GetExecutionContext();
            if (exec_context && object->IsHostObject())
            {
                if (!regs)
                {
                    regs = exec_context->AllocateRegisters(3);
                    regs[0].SetObject(instance);
                    regs[1].SetBoxed(list);
                }
                gclock.Release();
                names = object->GetOwnPropertyNamesL(context, instance, use_special_cases, enum_dont_enum);
                gclock.Acquire();
            }
            else
                names = object->GetOwnPropertyNamesL(context, instance, use_special_cases, enum_dont_enum);

            if (!head && names)
            {
                head = names;
                if (regs)
                    regs[2].SetBoxed(head);
            }
            else if (names)
            {
                *tail = ES_Boxed_List::Make(context, names);
                tail = &(*tail)->tail;
            }
        }

        object = object->Class()->Prototype();
        if (!object)
            break;
    }
    if (regs)
        context->GetExecutionContext()->FreeRegisters(3);

    return (list || !use_special_cases) ? ES_Boxed_List::Make(context, head, list) : head;
}

class ES_Object_PropertyEnumerator
    : public ES_PropertyEnumerator
{
public:
    ES_Object_PropertyEnumerator(ES_Context *context, ES_Object *object, ES_Object *shadow, unsigned offset, ES_Indexed_Properties *names, ES_Identifier_List *identifiers, ES_Value_Internal *regs)
        : context(context),
          object(object),
          shadow(shadow),
          offset(offset),
          names(names),
          identifiers(identifiers),
          regs(regs)
    {
    }

    virtual ~ES_Object_PropertyEnumerator()
    {
        ES_Identifier_List::Free(context, identifiers);
        regs[1].SetNull();
    }

    void AddPropertyInternalL(JString *name, unsigned host_code)
    {
        if (host_code != 0)
            JString::SetHostCode(name, host_code);
        if (object->HasOwnNativeProperty(name) || shadow && shadow->IsShadowed(context, object, name))
            return;

        /* Already added? */
        unsigned position;
        if (!identifiers->AppendL(context, name, position))
            return;

        ES_Value_Internal *value;
        regs[0].SetBoxed(names = ES_Indexed_Properties::PutL(context, names, offset++, value));
        value->SetString(name);
    }

    virtual void AddPropertyL(const char *name8, unsigned host_code = 0)
    {
        AddPropertyInternalL(name8 ? JString::Make(context, name8) : context->rt_data->strings[STRING_empty], host_code);
    }

    virtual void AddPropertyL(const uni_char *uni_name, unsigned host_code = 0)
    {
        AddPropertyInternalL(uni_name ? JString::Make(context, uni_name) : context->rt_data->strings[STRING_empty], host_code);
    }

    virtual void AddPropertiesL(unsigned start_index, unsigned count)
    {
        for (unsigned index = start_index; index < start_index + count; index++)
        {
            if (object->HasOwnNativeProperty(index) || shadow && shadow->IsShadowed(context, object, index))
                continue;

            ES_Value_Internal *value;
            regs[0].SetBoxed(names = ES_Indexed_Properties::PutL(context, names, offset++, value));
            value->SetUInt32(index);
        }
    }

    unsigned GetOffset() const { return offset; }
    ES_Indexed_Properties *GetIndexedProperties() const { return names; }

protected:
    ES_Context *context;
    ES_Object *object;
    ES_Object *shadow;

    unsigned offset;
    ES_Indexed_Properties *names;
    ES_Identifier_List *identifiers;
    // 2 registers: regs[0] for 'names', regs[1] for 'identifiers'.
    ES_Value_Internal *regs;
};

ES_Boxed *
ES_Object::GetOwnPropertyNamesL(ES_Context *context, ES_Object *shadow, BOOL use_special_cases, BOOL enum_dont_enum)
{
    ES_Indexed_Properties *names = NULL;
    ES_Property_Value_Table *hash_table = NULL;
    ES_CollectorLock gclock(context);

    BOOL has_klass = klass->HasEnumerableProperties() || enum_dont_enum && klass->Count() > 0;
    BOOL has_hash = HasHashTableProperties() && (hash_table = static_cast<ES_Class_Hash_Base *>(klass)->GetHashTable()) != NULL;
    BOOL has_indexed = ES_Indexed_Properties::GetUsed(GetIndexedProperties()) > 0;

    /* Properties are enumerated starting with the indexed properties in index order.
       In order to not need to renumber the ES_Indexed_Properties when starting to insert
       properties with serial number use an incrementing offset. */
    unsigned offset = 0;
    unsigned count = 0;

    /* Start with string objects. The string part of string objects are viewed as indexed
       properties and always start at zero and ends at length - 1. Any proper indexed
       properties will always be at index length or higher. */
    if (IsStringObject())
    {
        count = Length(static_cast<ES_String_Object *>(this)->GetValue());
        if (!use_special_cases || has_klass || has_hash || has_indexed)
        {
            ES_Value_Internal *value;
            for (unsigned i = 0; i < count; ++i)
            {
                names = ES_Indexed_Properties::PutL(context, names, offset++, value);
                value->SetUInt32(i);
            }
        }
        else if (count > 0)
            return this;
    }

    /* Then proper indexed properties. */
    if ((count = ES_Indexed_Properties::GetUsed(GetIndexedProperties())) != 0)
    {
        ES_Indexed_Property_Iterator indexed_iterator(context, this, GetIndexedPropertiesRef(), !enum_dont_enum);

        unsigned index;
        ES_Value_Internal *value;
        while (indexed_iterator.Next(index))
        {
            if (shadow && shadow->IsShadowed(context, this, index))
                continue;

            names = ES_Indexed_Properties::PutL(context, names, offset++, value);
            value->SetUInt32(index);
        }
    }

    /* Then (possibly) the properties of the host object. The order of the properties
       might be a bit strange but we'll have to live with that.  */
    if (IsHostObject())
    {
        ES_Execution_Context *exec_context = context->GetExecutionContext();
        ES_Value_Internal *regs = NULL;
        ES_Value_Internal local_value[2];

        enum { DEFAULT_HASHTABLE_SIZE = 32 };
        ES_Identifier_List *identifiers = ES_Identifier_List::Make(context, DEFAULT_HASHTABLE_SIZE);
        if (exec_context)
        {
            /* Release GC lock across property enumeration of host object. */
            gclock.Release();

            regs = exec_context->AllocateRegisters(2);
            regs[0].SetBoxed(names);
            regs[1].SetBoxed(identifiers);
        }
        else
            regs = local_value;

        ES_Object_PropertyEnumerator enumerator(context, this, shadow, offset, names, identifiers, regs);
        static_cast<ES_Host_Object *>(this)->FetchProperties(context, &enumerator);
        names = enumerator.GetIndexedProperties();
        offset = enumerator.GetOffset();

        if (exec_context)
        {
            exec_context->FreeRegisters(2);
            gclock.Acquire();
        }
    }

    /* Hash table properties */
    if (has_hash)
    {
        count = hash_table->Count();

        JString *name;
        unsigned serial;
        ES_Value_Internal *value;

        for (unsigned i = 0; i < count; ++i)
        {
            if (enum_dont_enum || !hash_table->GetInfo(i).IsDontEnum())
            {
                hash_table->properties->Lookup(i, name);
                if (name && (!shadow || !shadow->IsShadowed(context, this, name)))
                {
                    hash_table->GetSerialNr(i, serial);
                    names = ES_Indexed_Properties::PutL(context, names, serial + offset, value);
                    value->SetString(name);
                }
            }
        }
    }

    /* Class properties */
    if (has_klass)
    {
        if (use_special_cases && !IsSingleton() && shadow == this && !names && !HasUnorderedProperties() && Count() == klass->Count())
            return klass;

        ES_Identifier_List *klass_table = klass->GetPropertyTable()->properties;
        count = Count() - klass->CountExtra();

        JString *name;
        ES_Value_Internal *value;

        for (unsigned i = 0; i < count; ++i)
        {
            ES_PropertyIndex index(i);
            if (enum_dont_enum || !klass->GetPropertyInfoAtIndex(index).IsDontEnum())
            {
                klass_table->Lookup(index, name);

                if (!shadow || !shadow->IsShadowed(context, this, name))
                {
                    if (has_hash)
                    {
                        unsigned serial = static_cast<ES_Class_Hash_Base *>(klass)->GetSerialNrAtIndex(index);
                        names = ES_Indexed_Properties::PutL(context, names, serial + offset, value);
                    }
                    else
                        names = ES_Indexed_Properties::PutL(context, names, index + offset, value);

                    value->SetString(name);
                }
            }
        }
    }

    return names;
}

void
ES_Object::SetHasMultipleIdentities(ES_Context *context)
{
    object_bits |= MASK_MULTIPLE_IDENTITIES;

    OP_ASSERT(klass->Count() == 0);

    klass = ES_Class::MakeRoot(context, klass->Prototype(), klass->ObjectName(), klass->ObjectName(context), TRUE);
}

void
ES_Object::SetHasGetterOrSetter()
{
    if (HasGetterOrSetter())
        return;

    object_bits |= MASK_HAS_GETTER_OR_SETTER;

    if (!IsPrototypeObject())
        return;

    if (GetInstances()->IsClass())
    {
        if (ES_Object *prototype = static_cast<ES_Class *>(GetInstances())->Prototype())
            prototype->SetHasGetterOrSetter();
    }
    else
    {
        OP_ASSERT(GetInstances()->GCTag() == GCTAG_ES_Identifier_Boxed_Hash_Table);
        ES_Identifier_Boxed_Hash_Table *sub_classes = static_cast<ES_Identifier_Boxed_Hash_Table *>(GetInstances());
        ES_Boxed_Array *array = sub_classes->GetValues();
        for (unsigned i = 0; i < array->nused; ++i)
        {
            if (array->slots[i]->IsClass())
            {
                if (ES_Object *instance = static_cast<ES_Class *>(array->slots[i])->GetInstance())
                    instance->SetHasGetterOrSetter();
            }
            else
            {
                OP_ASSERT(array->slots[i]->GCTag() == GCTAG_ES_Boxed_List);
                for (ES_Boxed_List *instances = static_cast<ES_Boxed_List *>(array->slots[i]); instances; instances = instances->tail)
                    if (ES_Object *instance = instances->head ? static_cast<ES_Class *>(instances->head)->GetInstance() : NULL)
                        instance->SetHasGetterOrSetter();
            }
        }
    }
}

ES_Boxed *
ES_Object::GetInstances() const
{
    if (IsPrototypeObject())
        return static_cast<ES_Class_Singleton *>(klass)->GetInstances();

    return NULL;
}

void
ES_Object::SetInstances(ES_Boxed *instance)
{
    OP_ASSERT(klass->IsSingleton());
    OP_ASSERT(instance);

    static_cast<ES_Class_Singleton *>(klass)->SetInstances(instance);
}

void
ES_Object::SetPropertyCount(unsigned count)
{
    klass = ES_Class::SetPropertyCount(klass, count);
    property_count = count;
}

BOOL
ES_Object::SecurityCheck(ES_Execution_Context *context, const char *message)
{
    if (SecurityCheck(context))
        return TRUE;
    else
    {
        context->ThrowReferenceError(message);
        return FALSE;
    }
}

BOOL
ES_Object::GetOwnPropertyL(ES_Execution_Context *context, PropertyDescriptor &desc, JString *name, unsigned index, BOOL fetch_value, BOOL fetch_accessors)
{
    desc.is_undefined = 0;

    if (name)
    {
        ES_Object::ES_Value_Internal_Ref value_ref;

        if (!GetOwnLocation(name, desc.info, value_ref))
            goto not_found;
        else if (!SecurityCheck(context, "Security error: protected variable accessed"))
            return FALSE;

        desc.has_enumerable = desc.has_configurable = 1;

        if (value_ref.IsGetterOrSetter())
            desc.accessor = static_cast<ES_Special_Mutable_Access *>(value_ref.GetBoxed());
        else
        {
            if (fetch_value)
                if (value_ref.IsBoxed())
                {
                    ES_Special_Property *special = static_cast<ES_Special_Property *>(value_ref.GetBoxed());
                    if (special->WillCreatePropertyOnGet(this))
                        desc.info.ClearIsSpecial();
                    special->SpecialGetL(context, this, desc.value, this);
                }
                else
                    value_ref.Read(desc.value);
            desc.has_value = desc.has_writable = 1;
        }
    }
    else
    {
        ES_Value_Internal value;

        if (!GetOwnLocationL(context, index, desc.info, value))
            goto not_found;
        else if (IsHostObject() && !static_cast<ES_Host_Object *>(this)->SecurityCheck(context))
        {
            context->ThrowTypeError("Security error: protected variable accessed");
            return FALSE;
        }

        desc.has_enumerable = desc.has_configurable = 1;

        if (value.IsGetterOrSetter())
            desc.accessor = static_cast<ES_Special_Mutable_Access *>(value.GetBoxed());
        else
        {
            if (fetch_value)
                if (value.IsSpecial())
                {
                    ES_Special_Property *special = static_cast<ES_Special_Property *>(value.GetBoxed());
                    if (special->WillCreatePropertyOnGet(this))
                        desc.info.ClearIsSpecial();
                    special->SpecialGetL(context, this, desc.value, this);
                }
                else
                    desc.value = value;
            desc.has_value = desc.has_writable = 1;
        }
    }

    if (desc.accessor)
    {
        desc.has_getter = desc.accessor->getter != NULL;
        desc.has_setter = desc.accessor->setter != NULL;

        if (fetch_accessors)
        {
            desc.getter = desc.accessor->getter;
            desc.setter = desc.accessor->setter;
        }
    }

    return TRUE;

not_found:
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->GetOwnHostPropertyL(context, desc, name, index, fetch_value, fetch_accessors);
    else
    {
        desc.is_undefined = 1;
        return TRUE;
    }
}

BOOL
ES_Object::GetPropertyL(ES_Execution_Context *context, PropertyDescriptor &desc, JString *name, unsigned index, BOOL fetch_value, BOOL fetch_accessors)
{
    ES_Object *object = this;

    while (object)
        if (!object->GetOwnPropertyL(context, desc, name, index, fetch_value, fetch_accessors))
            return FALSE;
        else if (!desc.is_undefined)
            break;
        else
            object = object->Class()->Prototype();

    return TRUE;
}

BOOL
ES_Object::DefineOwnPropertyL(ES_Execution_Context *context, JString *name, unsigned index, const PropertyDescriptor &desc, const char *&message)
{
    ES_CollectorLock gclock(context);

    message = NULL;

    if (!SecurityCheck(context, "Security error: protected variable accessed"))
        return FALSE;

    PropertyDescriptor current;

    if (!GetOwnPropertyL(context, current, name, index, IsDataDescriptor(desc), IsAccessorDescriptor(desc)))
        return FALSE;

    ES_Value_Internal value;

    if (!IsAccessorDescriptor(desc))
        if (desc.has_value)
            value = desc.value;
        else if (current.has_value)
            value = current.value;

    BOOL update_array_length = FALSE;

    if (!name)
    {
        if (indexed_properties)
        {
            unsigned capacity;

            switch (ES_Indexed_Properties::GetType(indexed_properties))
            {
            case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
                capacity = static_cast<ES_Byte_Array_Indexed *>(indexed_properties)->Capacity();
                break;

            case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
                capacity = static_cast<ES_Type_Array_Indexed *>(indexed_properties)->Capacity();
                break;

            default:
                capacity = UINT_MAX;
            }

            if (index >= capacity)
            {
                message = "invalid modification of non-configurable property";
                return FALSE;
            }
        }

        if (GCTag() == GCTAG_ES_Object_Array)
        {
            PropertyDescriptor length;

            GetOwnPropertyL(context, length, context->rt_data->idents[ESID_length], 0, TRUE, FALSE);

            if (index >= length.value.GetNumAsUInt32())
            {
                if (!length.info.IsWritable())
                {
                    message = "attempt to add element to Array with read-only length";
                    return FALSE;
                }

                update_array_length = TRUE;
            }
        }
    }

    if (current.is_undefined)
    {
        if (!IsExtensible())
        {
            message = "attempt to add property to non-extensible object";
            return FALSE;
        }
        else if (name && IsHostObject() && !static_cast<ES_Host_Object *>(this)->AllowOperationOnProperty(context, name, EcmaScript_Object::ALLOW_NATIVE_OVERRIDE))
        {
            message = "invalid modification of non-configurable property";
            return FALSE;
        }

        ES_Property_Info new_info(desc.info.Attributes());

        if (IsAccessorDescriptor(desc))
        {
            new_info.SetIsSpecial();
            value.SetBoxed(ES_Special_Mutable_Access::Make(context, static_cast<ES_Function *>(desc.getter), static_cast<ES_Function *>(desc.setter)));

            if (name)
                SetHasGetterOrSetter();
        }

        if (name)
        {
            InitPropertyL(context, name, value, new_info.Attributes(), value.GetStorageType());

            Invalidate();
        }
        else
            InitPropertyL(context, index, value, new_info.Attributes());
    }
    else
    {
        BOOL truncate_array = FALSE;

        if (GCTag() == GCTAG_ES_Object_Array)
        {
            /* 15.4.5.1 [[DefineOwnProperty]] ( P, Desc, Throw ) */

            if (name && name->Equals("length", 6) && desc.has_value)
            {
                OP_ASSERT(current.has_value && current.value.IsUInt32());

                if (!value.ToNumber(context))
                    return FALSE;
                if (!value.IsUInt32())
                {
                    context->ThrowRangeError("invalid length value");
                    return FALSE;
                }
                if (value.GetNumAsUInt32() < current.value.GetNumAsUInt32())
                    truncate_array = TRUE;
            }
        }

        if (!desc.has_enumerable && !desc.has_configurable && !desc.has_value && !desc.has_writable && !desc.has_getter && !desc.has_setter)
            return TRUE;

        if (!current.info.IsConfigurable() && (desc.info.IsConfigurable() || desc.has_enumerable && current.info.IsEnumerable() != desc.info.IsEnumerable()))
            goto reject;

        ES_Property_Info new_info(current.info.Attributes());
        ES_Value_Internal *new_value = NULL;
        BOOL replacing_function_value = FALSE;

        if (!IsGenericDescriptor(desc))
            if (IsDataDescriptor(current) != IsDataDescriptor(desc))
            {
                if (!current.info.IsConfigurable())
                    goto reject;
                else if (IsAccessorDescriptor(desc))
                {
                    value.SetBoxed(ES_Special_Mutable_Access::Make(context, static_cast<ES_Function *>(desc.getter), static_cast<ES_Function *>(desc.setter)));
                    new_info.SetIsSpecial();
                }
                else
                    new_info.ClearIsSpecial();

                new_value = &value;

                /* Resetting [[Writable]] because of

                     "Preserve the existing values of the converted property`s
                      [[Configurable]] and [[Enumerable]] attributes and set the
                      rest of the property`s attributes to their default values."

                   in 8.12.9 step 9.b.i and 9.c.i. */
                new_info.SetWritable(FALSE);
            }
            else if (IsDataDescriptor(current) /* && IsDataDescriptor(desc) */)
            {
                if (desc.has_value && !ES_Value_Internal::IsSameValue(context, desc.value, current.value))
                {
                    new_value = &value;
                    replacing_function_value = current.info.IsFunction();
                    if (current.info.IsSpecial() && !new_value->IsSpecial())
                        new_info.ClearIsSpecial();
                }

                if (!current.info.IsConfigurable())
                    if (!current.info.IsWritable())
                        if (desc.info.IsWritable() || new_value)
                            goto reject;
            }
            else /* IsAccessorDescriptor(current) && IsAccessorDescriptor(desc) */
            {
                if (!current.info.IsConfigurable())
                    if (desc.has_setter && desc.setter != current.setter ||
                        desc.has_getter && desc.getter != current.getter)
                        goto reject;

                if (current.is_host_property)
                {
                    value.SetBoxed(current.accessor = ES_Special_Mutable_Access::Make(context, static_cast<ES_Function *>(current.getter), static_cast<ES_Function *>(current.setter)));
                    new_info.SetIsSpecial();
                    new_value = &value;
                }

                if (desc.has_setter)
                    current.accessor->setter = static_cast<ES_Function *>(desc.setter);
                if (desc.has_getter)
                    current.accessor->getter = static_cast<ES_Function *>(desc.getter);
            }

        if (desc.has_configurable)
            new_info.SetConfigurable(desc.info.IsConfigurable());
        if (desc.has_enumerable)
            new_info.SetEnumerable(desc.info.IsEnumerable());
        if (desc.has_writable)
            new_info.SetWritable(desc.info.IsWritable());
        if (replacing_function_value)
            new_info.ClearIsFunction();

        if (!name && GCTag() == GCTAG_ES_Object_Arguments)
        {
            /* 10.6 [[DefineOwnProperty]] on non-strict Arguments object */

            ES_Value_Internal *value = ES_Indexed_Properties::GetStorage(indexed_properties, index);

            if (value->IsSpecial() && value->GetBoxed()->GCTag() == GCTAG_ES_Special_Aliased)
            {
                if (desc.has_value)
                    *static_cast<ES_Special_Aliased *>(value->GetBoxed())->property = desc.value;

                if (IsAccessorDescriptor(desc) || desc.has_writable && !desc.info.IsWritable())
                    ES_Indexed_Properties::DeleteL(context, this, indexed_properties, index);
                else
                    new_value = NULL;
            }
        }

        if (!current.is_host_property && new_info.Attributes() != current.info.Attributes())
        {
            BOOL allowed;

            if (name)
                allowed = RedefinePropertyL(context, name, new_info, new_value);
            else
                allowed = RedefinePropertyL(context, index, new_info, new_value);
            if (!allowed)
            {
                message = "operation not allowed";
                return FALSE;
            }

            if (IsAccessorDescriptor(desc))
                SetHasGetterOrSetter();
        }
        else if (new_value)
            if (name)
                PutNativeL(context, name, new_info, *new_value);
            else
                PutNativeL(context, index, new_info, *new_value);

        if (truncate_array)
        {
            unsigned requested_length = value.GetNumAsUInt32();
            unsigned old_length = current.value.GetNumAsUInt32();
            unsigned new_length = ES_Indexed_Properties::TruncateL(context, this, GetIndexedProperties(), requested_length, old_length);

            if (requested_length < new_length)
            {
                ES_Array::SetLength(context, this, new_length);

                message = "array truncation failed";
                return FALSE;
            }
        }
    }

    if (update_array_length)
        ES_Array::SetLength(context, this, index + 1);

    return TRUE;

reject:
    message = "invalid modification of non-configurable property";
    return FALSE;
}

BOOL
ES_Object::RedefinePropertyL(ES_Execution_Context *context, JString *name, const ES_Property_Info &new_info, const ES_Value_Internal *new_value)
{
    if (IsHostObject() && new_value && new_value->IsGetterOrSetter() && !static_cast<ES_Host_Object *>(this)->AllowGetterSetterFor(context, name))
        return FALSE;

    if (!new_info.IsClassProperty())
    {
        OP_ASSERT(HasHashTableProperties());
        static_cast<ES_Class_Hash_Base *>(klass)->RedefinePropertyL(context, name, new_value, new_info);
        return TRUE;
    }

    ES_Property_Info info;
    ES_Value_Internal_Ref value_ref;

    GetOwnLocation(name, info, value_ref);

    OP_ASSERT(info.Attributes() != new_info.Attributes());

    info.Set(new_info.Attributes());

    BOOL needs_conversion = FALSE;
    ES_Layout_Info old_layout = klass->GetLayoutInfo(info);
    ES_Class *old_klass = ES_Class::ActualClass(klass);
    ES_Class *new_klass = ES_Class::ChangeAttribute(context, klass, name, info, property_count, needs_conversion);

    if (needs_conversion)
        ConvertObject(context, old_klass, new_klass);
    else
    {
        int diff = new_klass->GetLayoutInfo(info).Diff(old_layout);
        ConvertObjectSimple(context, new_klass, ES_LayoutIndex(info.Index() + 1), diff, old_klass->SizeOf(property_count), property_count);
    }

    if (new_value)
    {
        value_ref.Update(properties, klass);

        ES_StorageType type = new_value->GetStorageType();

        if (!value_ref.CheckType(type))
            ConvertProperty(context, value_ref, type);

        value_ref.Write(*new_value);
    }

    return TRUE;
}

BOOL
ES_Object::RedefinePropertyL(ES_Execution_Context *context, unsigned index, const ES_Property_Info &new_info, const ES_Value_Internal *new_value)
{
    unsigned attributes = new_info.Attributes();

    ES_Value_Internal *storage;
    ES_Indexed_Properties *new_properties = ES_Indexed_Properties::PutL(context, indexed_properties, index, &attributes, storage);

    if (new_value)
    {
        if (new_value->IsUndefined())
            storage->SetUndefined(TRUE);
        else
            *storage = *new_value;

        if (new_value->IsSpecial())
            new_properties->SetHasSpecialProperties();
    }

    SetIndexedProperties(new_properties);
    return TRUE;
}

void
ES_Variable_Object::Initialize(ES_Variable_Object *self, ES_Class *klass)
{
    self->InitGCTag(GCTAG_ES_Object_Variables);

    self->object_bits = 0;
    self->klass = klass;
    self->properties = NULL;
    self->indexed_properties = NULL;
    self->property_count = klass ? klass->Count() : 0;

    self->SetIsScopeObject();
}

ES_Variable_Object *
ES_Variable_Object::Make(ES_Context *context, ES_Class *klass, ES_Value_Internal *properties)
{
    OP_ASSERT(!klass->Prototype() || klass->Prototype() && !klass->Prototype()->HasMultipleIdentities());

    unsigned size = klass->Count();

    unsigned mainbytes = GC_ALIGN(sizeof(ES_Variable_Object));
    unsigned namedbytes = GC_ALIGN(ES_Object::AllocationSize(klass, size, size));

    ES_Variable_Object *object;

    if (mainbytes + namedbytes < LARGE_OBJECT_LIMIT)
    {
        ES_Box *spare_properties;
        GC_ALLOCATE_WITH_EXTRA_ALIGNED(context, object, spare_properties, namedbytes, ES_Variable_Object, ES_Box, (object, klass), (spare_properties));
        Initialize(spare_properties);

        object->indexed_properties = reinterpret_cast<ES_Indexed_Properties *>(spare_properties);

        object->SetProperties(properties);
    }
    else
    {
        GC_ALLOCATE(context, object, ES_Variable_Object, (object, klass));
        ES_CollectorLock gclock(context);

        ES_Box *spare_properties = ES_Box::Make(context, namedbytes);
        Initialize(spare_properties);
        object->indexed_properties = reinterpret_cast<ES_Indexed_Properties *>(spare_properties);

        object->SetProperties(properties);
    }

    OP_ASSERT_PROPERTY_COUNT(object);
    return object;
}

void
ES_Variable_Object::CopyPropertiesFrom(ES_Execution_Context *context, ES_Value_Internal *values)
{
    unsigned size = klass->SizeOf(Count());
    if (size > 0)
    {
        OP_ASSERT(indexed_properties);
        SetProperties(reinterpret_cast<ES_Box *>(indexed_properties));
        op_memcpy(properties + sizeof(ES_Box), values, size - sizeof(ES_Box));
    }
    else
        properties = NULL;

    indexed_properties = NULL;
    ChangeGCTag(GCTAG_ES_Object);
}

void
ES_Variable_Object::CreateAliasedFrom(ES_Context *context, ES_Value_Internal *values)
{
    ES_Class *stored_klass = klass;
    unsigned stored_named_count = CountProperties();
    unsigned stored_count = Count();

    unsigned size = klass->SizeOf(Count());
    klass = klass->GetRootClass();

    if (size > 0)
    {
        OP_ASSERT(indexed_properties);
        SetProperties(reinterpret_cast<ES_Box *>(indexed_properties));
        op_memcpy(properties + sizeof(ES_Box), values, size - sizeof(ES_Box));
    }
    else
        properties = NULL;

    indexed_properties = NULL;
    ChangeGCTag(GCTAG_ES_Object);

    /* The property values stored in 'properties' wouldn't be traced because the
       object's class has zero properties.  Locking GC to avoid problems. */
    ES_CollectorLock gclock(context);

    if (stored_named_count > 0)
    {
        BOOL needs_conversion = FALSE;
        ES_Class *old_klass = ES_Class::ActualClass(stored_klass);
        stored_klass = ES_Class::ChangeAttributeFromTo(context, old_klass, SP, 0, ES_PropertyIndex(0), ES_PropertyIndex(stored_named_count - 1), stored_count, needs_conversion, klass, ES_LayoutIndex(0));

        for (unsigned i = 0; i < stored_named_count; ++i)
        {
            ES_Property_Info info = stored_klass->GetPropertyInfoAtIndex(ES_PropertyIndex(i));
            ES_Layout_Info layout = stored_klass->GetLayoutInfo(info);

            /* If this assert does not hold then we have gaps in the layout table, which is bad for alias
               objects since this implies that Count() > klass->CountProperties() */
            OP_ASSERT(!layout.IsSpecialType());
            OP_ASSERT(layout.GetStorageType() == ES_STORAGE_WHATEVER);

            ES_Special_Aliased *aliased_property = ES_Special_Aliased::Make(context, values + i);
            GetValue_loc(layout)->SetBoxed(aliased_property);
        }
    }

    klass = stored_klass;
    property_count = stored_named_count;
}
