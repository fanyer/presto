/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2010
 */

#ifndef ES_OBJECT_INLINES_H
#define ES_OBJECT_INLINES_H

#include "modules/ecmascript/carakan/src/kernel/es_boxed.h"
#include "modules/ecmascript/carakan/src/object/es_host_object.h"
#include "modules/ecmascript/carakan/src/object/es_indexed.h"
#include "modules/ecmascript/carakan/src/object/es_special_property.h"
#include "modules/ecmascript/carakan/src/vm/es_execution_context.h"

#ifdef _DEBUG
# define inline
#endif // _DEBUG

inline ES_Class *ES_Object::Class() const
{
    return klass;
}

inline void ES_Object::SetClass(ES_Class *new_klass)
{
    OP_ASSERT(klass->Prototype() == new_klass->Prototype());
    OP_ASSERT(klass->Count() >= new_klass->Count());

    klass = new_klass;

    property_count = es_minu(property_count, klass->Count());
}

inline BOOL ES_Object::IsArrayObject() const
{
    return GCTag() == GCTAG_ES_Object_Array;
}

inline BOOL ES_Object::IsBooleanObject() const
{
    return GCTag() == GCTAG_ES_Object_Boolean;
}

inline BOOL ES_Object::IsDateObject() const
{
    return GCTag() == GCTAG_ES_Object_Date;
}

inline BOOL ES_Object::IsErrorObject() const
{
    return GCTag() == GCTAG_ES_Object_Error;
}

inline BOOL ES_Object::IsFunctionObject() const
{
    return GCTag() == GCTAG_ES_Object_Function || GCTag() == GCTAG_ES_Object_RegExp_CTOR;
}

inline BOOL ES_Object::IsNativeFunctionObject()
{
    return GCTag() == GCTAG_ES_Object_Function && !IsHostObject() && static_cast<ES_Function *>(this)->GetFunctionCode();
}

inline BOOL ES_Object::IsNativeFunctionObject(BOOL &is_strict_mode)
{
    ES_FunctionCode *fncode;
    if (GCTag() == GCTAG_ES_Object_Function && !IsHostObject() && (fncode = static_cast<ES_Function *>(this)->GetFunctionCode()))
    {
        is_strict_mode = fncode->data->is_strict_mode;
        return TRUE;
    }
    else
        return FALSE;
}

inline BOOL ES_Object::IsGlobalObject() const
{
    return (object_bits & MASK_IS_GLOBAL_OBJECT) != 0;
}

inline BOOL ES_Object::IsHostObject() const
{
    return (object_bits & MASK_IS_HOST_OBJECT) != 0 && static_cast<const ES_Host_Object *>(this)->GetHostObject() != NULL;
}

inline BOOL ES_Object::IsHostObject(IncludeInactiveTag include_inactive) const
{
    return (object_bits & MASK_IS_HOST_OBJECT) != 0;
}

inline void ES_Object::SetIsHostObject()
{
    object_bits |= MASK_IS_HOST_OBJECT;
}

inline BOOL ES_Object::IsHiddenObject() const
{
    return (object_bits & MASK_HIDDEN_OBJECT) != 0;
}

inline void ES_Object::SetIsHiddenObject()
{
    object_bits |= MASK_HIDDEN_OBJECT;
}

inline BOOL ES_Object::HasMultipleIdentities() const
{
    return (object_bits & MASK_MULTIPLE_IDENTITIES) != 0;
}

inline BOOL ES_Object::IsProxyInstanceAdded() const
{
    return (object_bits & MASK_PROXY_INSTANCE_ADDED) != 0;
}

inline void ES_Object::SetProxyInstanceAdded()
{
    object_bits |= MASK_PROXY_INSTANCE_ADDED;
}

inline BOOL ES_Object::IsProxyCachingDisabled() const
{
    return (object_bits & MASK_PROXY_CACHING_DISABLED) != 0;
}

inline void ES_Object::SetProxyCachingDisabled()
{
    object_bits |= MASK_PROXY_CACHING_DISABLED;
}

inline void ES_Object::ClearProxyInstanceAdded()
{
    object_bits &= ~MASK_PROXY_INSTANCE_ADDED;
}

inline BOOL ES_Object::HasVolatilePropertySet() const
{
    return (object_bits & MASK_HAS_VOLATILE_PROPERTIES) != 0;
}

inline void ES_Object::SetHasVolatilePropertySet()
{
    object_bits |= MASK_HAS_VOLATILE_PROPERTIES;
}

inline BOOL ES_Object::HasUnorderedProperties() const
{
    return (object_bits & MASK_HAS_UNORDERED_PROPERTIES) != 0;
}

inline void ES_Object::SetHasUnorderedProperties()
{
    object_bits |= MASK_HAS_UNORDERED_PROPERTIES;
}

inline BOOL ES_Object::HasBeenInlined() const
{
    return (object_bits & MASK_HAS_BEEN_INLINED) != 0;
}

inline void ES_Object::SetHasBeenInlined()
{
    object_bits |= MASK_HAS_BEEN_INLINED;
}

inline void ES_Object::ClearHasBeenInlined()
{
    object_bits &= ~MASK_HAS_BEEN_INLINED;
}

inline BOOL
ES_Object::IsCrossDomainAccessible() const
{
    return (object_bits & MASK_ALLOW_CROSS_DOMAIN_ACCESS) != 0;
}

inline void
ES_Object::SetIsCrossDomainAccessible()
{
    object_bits |= MASK_ALLOW_CROSS_DOMAIN_ACCESS;
}

inline BOOL
ES_Object::IsCrossDomainHostAccessible() const
{
    return (object_bits & MASK_ALLOW_CROSS_DOMAIN_HOST_ACCESS) != 0;
}

inline void
ES_Object::SetIsCrossDomainHostAccessible()
{
    object_bits |= MASK_ALLOW_CROSS_DOMAIN_HOST_ACCESS;
}

inline BOOL
ES_Object::HasGetterOrSetter() const
{
    return (object_bits & MASK_HAS_GETTER_OR_SETTER) != 0;
}

inline void ES_Object::SignalPropertySetChanged()
{
    klass->InvalidateCurrentClass();
    klass->InvalidateInstances();
}

inline BOOL ES_Object::IsHostFunctionObject() const
{
    return IsHostObject() && IsFunctionObject();
}

inline BOOL ES_Object::IsNumberObject() const
{
    return GCTag() == GCTAG_ES_Object_Number;
}

inline BOOL ES_Object::IsRegExpObject() const
{
    return GCTag() == GCTAG_ES_Object_RegExp;
}

inline BOOL ES_Object::IsRegExpConstructor() const
{
    return GCTag() == GCTAG_ES_Object_RegExp_CTOR;
}

inline BOOL ES_Object::IsStringObject() const
{
    return GCTag() == GCTAG_ES_Object_String;
}

inline BOOL ES_Object::IsArgumentsObject() const
{
    return GCTag() == GCTAG_ES_Object_Arguments;
}

inline BOOL ES_Object::IsVariablesObject() const
{
    return GCTag() == GCTAG_ES_Object_Variables;
}

inline BOOL ES_Object::IsArrayBufferObject() const
{
    return GCTag() == GCTAG_ES_Object_ArrayBuffer;
}

inline BOOL ES_Object::IsTypedArrayObject() const
{
    return GCTag() == GCTAG_ES_Object_TypedArray;
}

inline BOOL ES_Object::IsScopeObject() const
{
    return (object_bits & MASK_IS_SCOPE_OBJECT) != 0;
}

inline void ES_Object::SetIsScopeObject()
{
    object_bits |= MASK_IS_SCOPE_OBJECT;
}

inline BOOL ES_Object::IsInToString() const
{
    return (object_bits & MASK_IS_IN_TOSTRING) != 0;
}

inline void ES_Object::SetInToString()
{
    object_bits |= MASK_IS_IN_TOSTRING;
}

inline void ES_Object::ResetInToString()
{
    object_bits &= ~MASK_IS_IN_TOSTRING;
}

inline BOOL ES_Object::IsBuiltInFunction() const
{
    return (object_bits & MASK_IS_BUILTIN_FN) != 0;
}

#ifdef ES_NATIVE_SUPPORT

inline ES_BuiltInFunction ES_Object::GetFunctionID() const
{
    return (ES_BuiltInFunction) ((object_bits & MASK_FUNCTION_ID) >> FUNCTION_ID_SHIFT);
}

inline void ES_Object::SetFunctionID(ES_BuiltInFunction id)
{
    OP_ASSERT(IsBuiltInFunction());

    object_bits = (object_bits & ~MASK_FUNCTION_ID) | (id << FUNCTION_ID_SHIFT);

    if (id == ES_BUILTIN_FN_eval)
        object_bits |= MASK_IS_EVAL_FN;
    else if (id == ES_BUILTIN_FN_apply)
        object_bits |= MASK_IS_APPLY_FN;
}

#endif // ES_NATIVE_SUPPORT

inline BOOL ES_Object::IsSingleton() const
{
    return klass->IsSingleton();
}

inline BOOL ES_Object::HasHashTableProperties() const
{
    return klass->HasHashTableProperties();
}

inline void ES_Object::SetIndexedProperties(ES_Indexed_Properties *properties)
{
#ifdef ES_NATIVE_SUPPORT
    indexed_properties = properties;

    if (properties)
    {
        unsigned indexed_bits = 0;

        switch (ES_Indexed_Properties::GetType(properties))
        {
        case ES_Indexed_Properties::TYPE_COMPACT:
        {
            ES_Compact_Indexed_Properties *compact = static_cast<ES_Compact_Indexed_Properties *>(properties);

            if (!compact->HasSpecialProperties())
                if (!compact->HasReadOnlyProperties() && !compact->NeedsCopyOnWrite() && IsExtensible())
                    indexed_bits = MASK_SIMPLE_COMPACT_INDEXED | MASK_MUTABLE_COMPACT_INDEXED;
                else
                    indexed_bits = MASK_SIMPLE_COMPACT_INDEXED;
        }
        break;

        case ES_Indexed_Properties::TYPE_BYTE_ARRAY:
            indexed_bits = MASK_BYTE_ARRAY_INDEXED;
            break;
        case ES_Indexed_Properties::TYPE_TYPE_ARRAY:
            indexed_bits = MASK_TYPE_ARRAY_INDEXED;
            break;
        }

        object_bits = (object_bits & ~MASK_INDEXED) | indexed_bits;
    }
#else // ES_NATIVE_SUPPORT
    indexed_properties = properties;
#endif // ES_NATIVE_SUPPORT
}

inline void
ES_Object::SetPlainCompactIndexedProperties(ES_Indexed_Properties *properties)
{
    indexed_properties = properties;
#ifdef ES_NATIVE_SUPPORT
    object_bits = (object_bits & ~MASK_INDEXED) | MASK_SIMPLE_COMPACT_INDEXED | MASK_MUTABLE_COMPACT_INDEXED;
#endif // ES_NATIVE_SUPPORT
}

inline void
ES_Object::AllocateProperties(ES_Context *context)
{
    property_count = klass->Count();

    ES_Box *props = ES_Box::Make(context, klass->SizeOf(property_count));

    Initialize(props);

    SetProperties(props);
}

inline void
ES_Object::AllocatePropertyStorage(ES_Context *context, unsigned size)
{
    ES_Box *props = ES_Box::Make(context, size);
    SetProperties(props);
}

inline BOOL
ES_Object::SecurityCheck(ES_Execution_Context *context)
{
    return !IsHostObject() || static_cast<ES_Host_Object *>(this)->SecurityCheck(context);
}

inline PutResult
ES_Object::PutL(ES_Execution_Context *context, unsigned index, const ES_Value_Internal &value)
{
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->PutHostL(context, index, 0, value);
    return ES_Indexed_Properties::PutL(context, this, index, value, this);
}

inline PutResult
ES_Object::PutCachedAtOffset(unsigned offset, unsigned type, const ES_Value_Internal &value)
{
    OP_ASSERT(IsVariablesObject() || (offset + ES_Value_Internal::SizeFromCachedTypeBits(type)) <= Capacity());

    ES_Value_Internal::Memcpy(properties + offset, value, ES_Value_Internal::StorageTypeFromCachedTypeBits(type));

    return PROP_PUT_OK;
}

inline PutResult
ES_Object::PutCachedAtIndex(ES_PropertyIndex index, const ES_Value_Internal &value)
{
    OP_ASSERT(index < property_count);

    ES_Layout_Info layout = klass->GetLayoutInfoAtInfoIndex(index);

    OP_ASSERT(IsVariablesObject() || layout.GetNextOffset() <= Capacity());

    ES_Value_Internal::Memcpy(properties + layout.GetOffset(), value, layout.GetStorageType());

    return PROP_PUT_OK;
}

inline PutResult
ES_Object::PutNoLockL(ES_Execution_Context *context, unsigned index, const ES_Value_Internal &value)
{
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->PutHostL(context, index, 0, value);
    return ES_Indexed_Properties::PutNoLockL(context, this, index, 0, value, this);
}

inline PutResult
ES_Object::PutNoLockL(ES_Execution_Context *context, unsigned index, unsigned *attributes, const ES_Value_Internal &value)
{
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->PutHostL(context, index, 0, value);
    return ES_Indexed_Properties::PutNoLockL(context, this, index, attributes, value, this);
}

inline PutResult
ES_Object::PutManyL(ES_Execution_Context *context, unsigned &args_put, unsigned index, unsigned argc, ES_Value_Internal *argv)
{
    args_put = argc;
    PutResult result = PROP_PUT_OK;

    if (IsHostObject() || !ES_Indexed_Properties::PutManyL(this, index, argc, argv))
    {
        ES_CollectorLock gclock(context);

        unsigned length = index;
        unsigned i;

        for (i = 0; i < argc && length < UINT_MAX; ++i, ++length)
            if ((result = PutNoLockL(context, index + i, argv[i])) != PROP_PUT_OK)
                break;

        args_put = i;
    }

    return result;
}

inline PutResult
ES_Object::PutCached(ES_Layout_Info layout, const ES_Value_Internal &value)
{
    OP_ASSERT(IsVariablesObject() || layout.GetNextOffset() <= Capacity());

    ES_Value_Internal::Memcpy(properties + layout.GetOffset(), value, layout.GetStorageType());

    return PROP_PUT_OK;
}

inline GetResult
ES_Object::GetOwnPropertyL(ES_Execution_Context *context, ES_Object *this_object, const ES_Value_Internal &this_value, unsigned index, ES_Value_Internal &value)
{
    return ES_Indexed_Properties::GetL(context, this, indexed_properties, index, value, this_value);
}

inline GetResult
ES_Object::GetL(ES_Execution_Context *context, JString *name, ES_Value_Internal &value)
{
    ES_Object *dummy_prototype;
    ES_PropertyIndex dummy_index;

    return GetL(context, this, name, value, dummy_prototype, dummy_index);
}

inline GetResult
ES_Object::GetL(ES_Execution_Context *context, unsigned index, ES_Value_Internal &value)
{
    return GetL(context, this, index, value);
}

inline GetResult
ES_Object::GetCachedAtIndex(ES_PropertyIndex index, ES_Value_Internal &value) const
{
    ES_Layout_Info layout = klass->GetLayoutInfoAtInfoIndex(index);
    ES_Value_Internal::Memcpy(value, properties + layout.GetOffset(), layout.GetStorageType());
    return PROP_GET_OK;
}

inline GetResult
ES_Object::GetCached(unsigned offset, unsigned type, ES_Value_Internal &value) const
{
    ES_Value_Internal::Memcpy(value, properties + offset, ES_Value_Internal::StorageTypeFromCachedTypeBits(type));
    return PROP_GET_OK;
}

inline GetResult
ES_Object::GetCached(ES_Layout_Info layout, ES_Value_Internal &value) const
{
    ES_Value_Internal::Memcpy(value, properties + layout.GetOffset(), layout.GetStorageType());
    return PROP_GET_OK;
}

inline GetResult
ES_Object::GetLength(ES_Execution_Context *context, unsigned &length, BOOL must_exist)
{
    if (HasIntrinsicLength())
    {
        ES_Value_Internal value;
        GetCachedAtIndex(ES_PropertyIndex(0), value);
        length = value.GetNumAsUInt32();
        return PROP_GET_OK;
    }
    else
        return GetLengthSlow(context, length, must_exist);
}

inline PutResult
ES_Object::PutL(ES_Execution_Context *context, JString *name, const ES_Value_Internal &value)
{
    ES_PropertyIndex dummy_index;
    return PutL(context, this, name, value, dummy_index);
}

inline void
ES_Object::UnsafePutL(ES_Context *context, JString *name, const ES_Value_Internal &value)
{
    ES_Property_Info info;
    ES_Value_Internal_Ref value_ref;

#ifdef DEBUG_ENABLE_OPASSERT
    BOOL res =
#endif // DEBUG_ENABLE_OPASSERT
    GetOwnLocation(name, info, value_ref);

    OP_ASSERT(res);

    OP_ASSERT(!info.IsFunction());
    OP_ASSERT(!info.IsSpecial());

    if (!value_ref.CheckType(value.Type()))
        ConvertProperty(context, value_ref, value.GetStorageType());

    value_ref.Write(value);
}

inline PutResult
ES_Object::PutLength(ES_Execution_Context *context, unsigned length, BOOL throw_on_read_only)
{
    if (IsArrayObject())
    {
        ES_Array::SetLength(context, this, length);
        return PROP_PUT_OK;
    }
    else
    {
        ES_Value_Internal value(length);
        PutResult result = PutL(context, context->rt_data->idents[ESID_length], length);
        if (throw_on_read_only && result == PROP_PUT_READ_ONLY)
        {
            context->ThrowTypeError("Failed to update length");
            result = PROP_PUT_FAILED;
        }
        return result;
    }
}

inline GetResult
ES_Object::GetPrototypeL(ES_Execution_Context *context, ES_Value_Internal &value)
{
    if (ES_Object *proto = Class()->Prototype())
        value.SetObject(proto);
    else
        value.SetNull();
    return PROP_GET_OK;
}

inline BOOL
ES_Object::HasPropertyWithInfo(ES_Execution_Context *context, JString *name, ES_Property_Info &info, ES_Object *&owner, BOOL &is_secure)
{
    BOOL dummy_can_cache = TRUE;
    return HasPropertyWithInfo(context, name, info, owner, is_secure, dummy_can_cache);
}

inline BOOL
ES_Object::HasOwnNativeProperty(JString *name)
{
    ES_Property_Info info;
    return HasOwnNativeProperty(name, info);
}

inline BOOL
ES_Object::HasOwnNativeProperty(JString *name, ES_Property_Info &info)
{
    return ES_Class::Find(klass, name, info, Count());
}

inline BOOL
ES_Object::HasOwnNativeProperty(unsigned index)
{
    ES_Property_Info info;
    return HasOwnNativeProperty(index, info);
}

inline BOOL
ES_Object::HasOwnNativeProperty(unsigned index, ES_Property_Info &info)
{
    if (ES_Indexed_Properties::HasProperty(indexed_properties, index, info))
        return TRUE;
    else if (IsStringObject())
    {
        unsigned length = Length(static_cast<ES_String_Object *>(this)->GetValue());
        if (index < length)
        {
            info.SetWritable(FALSE);
            info.SetConfigurable(FALSE);

            return TRUE;
        }
    }

    return FALSE;
}

inline BOOL
ES_Object::HasOwnProperty(ES_Context *context, JString *name)
{
    ES_Property_Info info;
    BOOL is_secure = TRUE;
    return HasOwnProperty(context, name, info, is_secure);
}

inline BOOL
ES_Object::HasOwnProperty(ES_Context *context, JString *name, ES_Property_Info &info, BOOL &is_secure)
{
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->HasOwnHostProperty(context, name, info, is_secure);

    is_secure = TRUE;

    return HasOwnNativeProperty(name, info);
}

inline BOOL
ES_Object::HasOwnProperty(ES_Context *context, unsigned index)
{
    ES_Property_Info info;
    BOOL is_secure = TRUE;
    return HasOwnProperty(context, index, info, is_secure);
}

inline BOOL
ES_Object::HasOwnProperty(ES_Context *context, unsigned index, ES_Property_Info &info, BOOL &is_secure)
{
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->HasOwnHostProperty(context, index, info, is_secure);

    is_secure = TRUE;

    return HasOwnNativeProperty(index, info);
}

inline void
ES_Object::PutPrivateL(ES_Context *context, unsigned private_name, const ES_Value_Internal &value, ES_Class *private_klass)
{
    ES_Layout_Info layout;
    ES_Object *private_properties;
    if (klass->LookupExtra(ES_STORAGE_SPECIAL_PRIVATE, layout))
    {
        ES_Value_Internal private_properties_value;
        GetCached(layout, private_properties_value);
        OP_ASSERT(private_properties_value.IsObject());
        private_properties = private_properties_value.GetObject();
    }
    else
    {
        private_properties = ES_Object::Make(context, private_klass);
        ES_Class *new_klass = ES_Class::ExtendWithL(context, klass, ES_STORAGE_SPECIAL_PRIVATE, ES_LayoutIndex(Count()));
        layout = new_klass->GetLayoutInfoAtIndex(ES_LayoutIndex(Count()));
        PutCachedNew(context, layout, private_properties, new_klass);
    }

    OP_ASSERT_PROPERTY_COUNT(this);
    OP_ASSERT(layout.GetStorageType() == ES_STORAGE_SPECIAL_PRIVATE);

    private_properties->PutNativeL(context, private_name, 0, value);
}

inline BOOL
ES_Object::GetPrivateL(ES_Context *context, unsigned private_name, ES_Value_Internal &value)
{
    ES_Layout_Info layout;
    if (klass->LookupExtra(ES_STORAGE_SPECIAL_PRIVATE, layout))
    {
        ES_Value_Internal private_properties_value;
        GetCached(layout, private_properties_value);
        OP_ASSERT(private_properties_value.IsObject());
        return private_properties_value.GetObject()->GetNativeL(context, private_name, value);
    }

    OP_ASSERT_PROPERTY_COUNT(this);
    return FALSE;
}

inline BOOL
ES_Object::DeletePrivateL(ES_Context *context, unsigned private_name)
{
    ES_Layout_Info layout;
    if (klass->LookupExtra(ES_STORAGE_SPECIAL_PRIVATE, layout))
    {
        ES_Value_Internal private_properties_value;
        GetCached(layout, private_properties_value);
        OP_ASSERT(private_properties_value.IsObject());
        return private_properties_value.GetObject()->DeleteOwnPropertyL(context, private_name);
    }

    return FALSE;
}

inline void ES_Object::PutNativeL(ES_Context *context, unsigned index, const ES_Property_Info &new_info, const ES_Value_Internal &new_value)
{
    if (IsArrayObject())
    {
        ES_Value_Internal length_val;
        GetCachedAtIndex(ES_PropertyIndex(0), length_val);
        OP_ASSERT(length_val.IsNumber());
        UINT32 length = length_val.GetNumAsUInt32();
        if (!(index < length))
            ES_Array::SetLength(context, this, index + 1);
    }

    unsigned attributes = new_info.Attributes();
    ES_Indexed_Properties::PutL(context, this, index, &attributes, new_value, this);
}

inline BOOL ES_Object::GetNativeL(ES_Context *context, unsigned index, ES_Value_Internal &value)
{
    return GET_OK(ES_Indexed_Properties::GetL(context, this, indexed_properties, index, value, this));
}

inline BOOL ES_Object::DeleteNativeL(ES_Context *context, JString *name)
{
    return DeleteOwnPropertyL(context, name);
}

inline DeleteResult ES_Object::DeleteL(ES_Execution_Context *context, JString *name, unsigned &result)
{
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->DeleteHostL(context, name, result);
    result = DeleteOwnPropertyL(context, name);
    return TRUE;
}

inline DeleteResult ES_Object::DeleteL(ES_Execution_Context *context, unsigned index, unsigned &result)
{
    if (IsHostObject())
        return static_cast<ES_Host_Object *>(this)->DeleteHostL(context, index, result);

    result = DeleteOwnPropertyL(context, index);
    return TRUE;
}

inline BOOL ES_Object::DeleteOwnPropertyL(ES_Context *context, unsigned index)
{
    return ES_Indexed_Properties::DeleteL(context, this, indexed_properties, index);
}

inline void ES_Object::SimpleCachedPropertyAccess::Initialize(ES_Object *new_object, JString *new_name)
{
    object = new_object;
    name = new_name;
    class_id = UINT_MAX;
}

inline BOOL ES_Object::HasEnumerableProperties() const
{
    if (klass->HasEnumerableProperties())
        return TRUE;

    if (indexed_properties && ES_Indexed_Properties::HasUsed(indexed_properties))
        return TRUE;

    if (HasHashTableProperties())
        return TRUE;

    if (IsHostObject())
        return TRUE;

    if (IsStringObject())
        return TRUE;

    return FALSE;
}

inline const char *ES_Object::ObjectName() const
{
    return klass->ObjectName();
}

inline BOOL
ES_Object::IsShadowed(ES_Context *context, ES_Object *object, JString *name)
{
    ES_Object *o = this;
    while (o != object)
    {
        if (o->HasOwnProperty(context, name))
            return TRUE;
        o = o->Class()->Prototype();
    }

    return FALSE;
}

inline BOOL
ES_Object::IsShadowed(ES_Context *context, ES_Object *object, unsigned index)
{
    ES_Object *o = this;
    while (o != object)
    {
        if (o->HasOwnProperty(context, index))
            return TRUE;
        o = o->Class()->Prototype();
    }

    return FALSE;
}

inline void
ES_Object::UpdateProperties(ES_Context *context, ES_Box *props)
{
    if (properties != props->Unbox())
    {
        if (properties && !IsFunctionObject())
            context->heap->Free(GetProperties());

        properties = props->Unbox();
    }
}

inline void
ES_Object::Grow(ES_Context *context, ES_Class *new_klass, unsigned capacity, unsigned minimum_capacity)
{
    unsigned size = new_klass->IsCompact() && static_cast<ES_Class_Compact_Node *>(new_klass)->ChildCount() ? new_klass->SizeOf(new_klass->LayoutLevel()) : es_maxu(capacity * 2, minimum_capacity);
    ES_Box *new_properties = ES_Box::Make(context, size);

    op_memcpy(new_properties->Unbox(), properties, capacity);
    UpdateProperties(context, new_properties);
}

inline ES_Box *
ES_Object::AllocateExtraPropertyStorage(ES_Context *context, ES_Class *klass, unsigned capacity, unsigned minimum_capacity)
{
    unsigned size = klass->IsCompact() && static_cast<ES_Class_Compact_Node *>(klass)->ChildCount() ? (klass->SizeOf(klass->LayoutLevel()) + sizeof(ES_Value_Internal)) : es_maxu(capacity * 2, minimum_capacity);
    return ES_Box::Make(context, size);
}

inline void
ES_Object::ReplaceProperties(ES_Context *context, ES_Box *new_properties, unsigned capacity)
{
    op_memcpy(new_properties->Unbox(), properties, capacity);
    UpdateProperties(context, new_properties);
}

inline void
ES_Object::AppendValueL(ES_Context *context, JString *name, ES_PropertyIndex &index, const ES_Value_Internal &value, unsigned attributes, ES_StorageType type)
{
    /* Pre-allocate property storage. Done so that if class extension
       succeeds (!= OOM), appending the property won't fail either.
       Thereby keeping the class and object synchronized. */
    ES_Box *extended_properties = NULL;
    unsigned needed_capacity = SizeOf() + sizeof(ES_Value_Internal);
    unsigned capacity = properties ? Capacity() : 0;
    if (needed_capacity > capacity)
        extended_properties = AllocateExtraPropertyStorage(context, klass, capacity, needed_capacity);

    ES_CollectorLock gclock(context);

    klass = ES_Class::ExtendWithL(context, klass, name, attributes, type, ES_LayoutIndex(property_count));
    index = klass->IndexOf(name);
    ES_Layout_Info layout = klass->GetLayoutInfoAtInfoIndex(index);

    if (!properties)
    {
        Initialize(extended_properties);

        SetProperties(extended_properties);
        extended_properties = NULL;
    }
    else if (extended_properties)
        ReplaceProperties(context, extended_properties, capacity);

    OP_ASSERT(layout.GetNextOffset() <= Capacity());

    PutCached(layout, value);

    property_count++;
}

inline BOOL
ES_Object::IsPrototypeObject() const
{
    return IsSingleton() && static_cast<ES_Class_Singleton *>(klass)->HasInstances();
}

inline void
ES_Object::Invalidate()
{
    if (IsPrototypeObject())
        InvalidateInstances();

    if (Class()->HasInstances())
        Class()->InvalidateInstances();
}

#ifdef ECMASCRIPT_DEBUGGER
inline void
ES_Object::TrackedByDebugger(BOOL tracked)
{
    if (tracked)
    {
        object_bits |= MASK_TRACKED_BY_DEBUGGER;
        SetNeedDestroy();
    }
    else
        object_bits &= ~MASK_TRACKED_BY_DEBUGGER;
}

inline BOOL
ES_Object::IsTrackedByDebugger() const
{
    return (object_bits & MASK_TRACKED_BY_DEBUGGER) != 0;
}
#endif // ECMASCRIPT_DEBUGGER

inline BOOL ES_Object::IsExtensible() const
{
    return (object_bits & MASK_IS_NOT_EXTENSIBLE) == 0;
}

inline void ES_Object::SetIsNotExtensible()
{
    object_bits |= MASK_IS_NOT_EXTENSIBLE;
    if (indexed_properties)
        SetIndexedProperties(indexed_properties);
}

inline ES_Boxed_List *
ES_Object::GetPropertyNamesSafeL(ES_Context *context, BOOL enum_dont_enum)
{
    return static_cast<ES_Boxed_List *>(GetPropertyNamesL(context, FALSE, enum_dont_enum));
}

inline ES_Indexed_Properties *
ES_Object::GetOwnPropertyNamesSafeL(ES_Context *context, ES_Object *shadow, BOOL enum_dont_enum)
{
    return static_cast<ES_Indexed_Properties *>(GetOwnPropertyNamesL(context, shadow, FALSE, enum_dont_enum));
}

inline ES_Boxed *
ES_Object::GetBoxed(ES_Layout_Info layout) const
{
    OP_ASSERT(layout.IsBoxedType());
    return *reinterpret_cast<ES_Boxed **>(properties + layout.GetOffset());
}

inline ES_Value_Internal *
ES_Object::GetValue_loc(ES_Layout_Info layout) const
{
    OP_ASSERT(layout.GetStorageType() == ES_STORAGE_WHATEVER);
    return reinterpret_cast<ES_Value_Internal *>(properties + layout.GetOffset());
}

inline ES_Value_Internal *
ES_Object::GetValueAtIndex_loc(ES_PropertyIndex index) const
{
    ES_Layout_Info layout = klass->GetLayoutInfoAtInfoIndex(index);
    OP_ASSERT(layout.GetStorageType() == ES_STORAGE_WHATEVER);
    return reinterpret_cast<ES_Value_Internal *>(properties + layout.GetOffset());
}

inline ES_Value_Internal
ES_Object::GetPropertyValueAtIndex(ES_PropertyIndex index) const
{
    return *GetValue_loc(klass->GetLayoutInfoAtInfoIndex(index));
}

inline /* static */ unsigned
ES_Object::AllocationSize(ES_Class *klass, unsigned used, unsigned size)
{
    size = es_maxu(size, 1);
    used = es_maxu(klass->LayoutLevel(), used);
    return sizeof(ES_Box) + GC_ALIGN((used != 0 ? klass->SizeOf(used) : sizeof(UINTPTR) * size));
}

inline BOOL
ES_Object::HasStorage() const
{
    return properties != NULL;
}

/* static */ inline void
ES_Array::SetLength(ES_Context *context, ES_Object *array, const ES_Value_Internal &length)
{
    ES_Layout_Info layout = array->Class()->GetLayoutInfoAtInfoIndex(ES_PropertyIndex(0));
    ES_Object::ES_Value_Internal_Ref value_ref(array->properties, layout, ES_PropertyIndex(0));

    if (!value_ref.CheckType(length.Type()))
        array->ConvertProperty(context, value_ref, length.GetStorageType());

    value_ref.Write(length);

    ES_Indexed_Properties::AdjustTop(array->GetIndexedProperties(), length.GetNumAsUInt32());
}

inline BOOL
ES_Object::HasIntrinsicLength() const
{
    switch (GCTag())
    {
    case GCTAG_ES_Object_String:
    case GCTAG_ES_Object_Array:
    case GCTAG_ES_Object_TypedArray:
    case GCTAG_ES_Object_ArrayBuffer:
        return TRUE;
    default:
        return FALSE;
    }
}

#ifdef _DEBUG
# undef inline
#endif // _DEBUG

#endif // ES_OBJECT_INLINES_H
