/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_VALUE_INLINES_H
#define ES_VALUE_INLINES_H

#ifdef ES_VALUES_AS_NANS
#define private_type other.type
#define private_value other.value
#endif

inline ES_ValueType
ES_Value_Internal::TypeTag() const
{
    return private_type;
}

inline ES_ValueType
ES_Value_Internal::Type() const
{
#if defined(ES_VALUES_AS_NANS)
    if (private_type <= ESTYPE_DOUBLE)
        return ESTYPE_DOUBLE;
#endif
    return private_type;
}

/* static */ inline unsigned
ES_Value_Internal::TypeBits(ES_ValueType type)
{
#ifdef ES_VALUES_AS_NANS
    if (type <= ESTYPE_DOUBLE)
        return ESTYPE_BITS_DOUBLE;
    else if (type == ESTYPE_INT32_OR_DOUBLE)
        return ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE;
    else
        return static_cast<ES_ValueTypeBits>(1 << (ESTYPE_OBJECT - type));
#else // ES_VALUES_AS_NANS
    if (type == ESTYPE_UNDEFINED)
        return ESTYPE_BITS_UNDEFINED;
    else
        return static_cast<ES_ValueTypeBits>(type << 1);
#endif // ES_VALUES_AS_NANS
}

/* static */ inline ES_ValueType
ES_Value_Internal::TypeFromBits(unsigned bits)
{
    switch (bits)
    {
    case ESTYPE_BITS_OBJECT: return ESTYPE_OBJECT;
    case ESTYPE_BITS_STRING: return ESTYPE_STRING;
    case ESTYPE_BITS_BOXED: return ESTYPE_BOXED;
    case ESTYPE_BITS_BOOLEAN: return ESTYPE_BOOLEAN;
    case ESTYPE_BITS_NULL: return ESTYPE_NULL;
    default: OP_ASSERT(FALSE);
    case ESTYPE_BITS_UNDEFINED: return ESTYPE_UNDEFINED;
    case ESTYPE_BITS_INT32: return ESTYPE_INT32;
    case ESTYPE_BITS_DOUBLE: return ESTYPE_DOUBLE;
    }
}

inline unsigned
ES_Value_Internal::TypeBits() const
{
    return TypeBits(private_type);
}

/* static */ inline BOOL
ES_Value_Internal::IsMonoTypeBits(unsigned bits)
{
    switch (bits)
    {
    case ESTYPE_BITS_OBJECT:
    case ESTYPE_BITS_STRING:
    case ESTYPE_BITS_BOXED:
    case ESTYPE_BITS_BOOLEAN:
    case ESTYPE_BITS_NULL:
    case ESTYPE_BITS_UNDEFINED:
    case ESTYPE_BITS_INT32:
    case ESTYPE_BITS_DOUBLE:
        return TRUE;
    default:
        return (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE) == bits;
    }
}

inline BOOL
ES_Value_Internal::IsUndefined() const
{
    return TypeTag() == ESTYPE_UNDEFINED;
}

inline BOOL
ES_Value_Internal::IsNull() const
{
    return TypeTag() == ESTYPE_NULL;
}

inline BOOL
ES_Value_Internal::IsNullOrUndefined() const
{
    return TypeTag() == ESTYPE_NULL || TypeTag() == ESTYPE_UNDEFINED;
}

inline BOOL
ES_Value_Internal::IsBoolean() const
{
    return TypeTag() == ESTYPE_BOOLEAN;
}

inline BOOL
ES_Value_Internal::IsNumber() const
{
#if defined(ES_VALUES_AS_NANS)
    // Tests for DOUBLE or INT32:
    return private_type <= ESTYPE_INT32;
#else
    return (private_type & ESTYPE_INT32_OR_DOUBLE) != 0;
#endif
}

inline BOOL
ES_Value_Internal::IsDouble() const
{
#ifdef ES_VALUES_AS_NANS
    return private_type <= ESTYPE_DOUBLE;
#else
    return private_type == ESTYPE_DOUBLE;
#endif
}

inline BOOL
ES_Value_Internal::IsInt32() const
{
    return TypeTag() == ESTYPE_INT32;
}

inline BOOL
ES_Value_Internal::IsUInt32() const
{
    if (IsInt32())
        return GetInt32() >= 0;
    else if (IsDouble())
    {
        double d = GetDouble();
        return op_isintegral(d) && d >= 0.0 && d < 4294967296.0;
    }
    else
        return FALSE;
}

inline BOOL
ES_Value_Internal::IsString() const
{
    return TypeTag() == ESTYPE_STRING;
}

inline BOOL
ES_Value_Internal::IsObject() const
{
    return TypeTag() == ESTYPE_OBJECT;
}

inline BOOL
ES_Value_Internal::IsHiddenObject(ES_Execution_Context *context) const
{
    if (TypeTag() == ESTYPE_OBJECT)
    {
        ES_Object *object = GetObject();
        if (object->HasMultipleIdentities())
            return object->IsHiddenObject() || GetObject(context)->IsHiddenObject();
        else
            return object->IsHiddenObject();
    }
    else
        return FALSE;
}

inline BOOL
ES_Value_Internal::IsCallable(ES_Execution_Context *context) const
{
    if (TypeTag() == ESTYPE_OBJECT)
        switch (context ? GetObject(context)->GCTag() : GetObject()->GCTag())
        {
        case GCTAG_ES_Object_Function:
#ifdef ES_REGEXP_IS_CALLABLE
        case GCTAG_ES_Object_RegExp:
#endif // ES_REGEXP_IS_CALLABLE
        case GCTAG_ES_Object_RegExp_CTOR:
            return TRUE;
        }

    return FALSE;
}

inline BOOL
ES_Value_Internal::IsBoxed() const
{
    return TypeTag() == ESTYPE_BOXED;
}

inline BOOL
ES_Value_Internal::IsDecodedBoxed() const
{
    return TypeTag() >= ESTYPE_BOXED;
}

inline BOOL
ES_Value_Internal::IsArgumentsCookie() const
{
    return private_type == ESTYPE_BOOLEAN && private_value.boolean == 666;
}

inline BOOL
ES_Value_Internal::IsNoExceptionCookie() const
{
    return private_type == ESTYPE_BOOLEAN && private_value.boolean == 667;
}

inline BOOL
ES_Value_Internal::IsGetterSetterCookie() const
{
#if 0
    return private_type == ESTYPE_OBJECT && GetObject()->hdr.getter_setter_cookie;
#else // 0
    return FALSE;
#endif // 0
}

inline BOOL
ES_Value_Internal::IsSpecial() const
{
    return IsDecodedBoxed() && GetDecodedBoxed()->IsSpecial();
}

inline BOOL
ES_Value_Internal::IsGetterOrSetter() const
{
    return IsDecodedBoxed() && GetDecodedBoxed()->GCTag() == GCTAG_ES_Special_Mutable_Access;
}

inline void
ES_Value_Internal::SetType(ES_ValueType type)
{
    private_type = type;
}

inline void
ES_Value_Internal::SetUndefined()
{
    private_type = ESTYPE_UNDEFINED;
#ifdef _DEBUG
    // To detect when GetHiddenBoolean() is used on an undefined value
    // that wasn't set by SetUndefined(BOOL hidden_boolean).
    private_value.boolean = 0xdeadbeef;
#endif // _DEBUG
}

inline void
ES_Value_Internal::SetUndefined(BOOL hidden_boolean)
{
    private_type = ESTYPE_UNDEFINED;
#ifdef _DEBUG
    // Make sure the stored value is exactly TRUE or FALSE.
    private_value.boolean = hidden_boolean ? TRUE : FALSE;
#else // _DEBUG
    private_value.boolean = hidden_boolean;
#endif // _DEBUG
}

inline void
ES_Value_Internal::SetNull()
{
    private_type = ESTYPE_NULL;
}

inline void
ES_Value_Internal::SetBoolean(BOOL boolean)
{
    private_type = ESTYPE_BOOLEAN;
    private_value.boolean = boolean;
}

inline void
ES_Value_Internal::SetTrue()
{
    private_type = ESTYPE_BOOLEAN;
    private_value.boolean = TRUE;
}

inline void
ES_Value_Internal::SetFalse()
{
    private_type= ESTYPE_BOOLEAN;
    private_value.boolean = FALSE;
}

inline void
ES_Value_Internal::SetNumber(double number)
{
    INT32 as_int32 = static_cast<INT32>(number);

    // TODO: test with 0x8000000000000000 instead
#ifdef NAN_EQUALS_EVERYTHING
    if (number == as_int32 && (as_int32 != 0 || 1/number > 0) && !(number == 0 && number == 1))
#else // NAN_EQUALS_EVERYTHING
    if (number == as_int32 && (as_int32 != 0 || 1/number > 0))
#endif // NAN_EQUALS_EVERYTHING
    {
        SetInt32(as_int32);
        return;
    }

    SetDouble(number);
}

inline void
ES_Value_Internal::SetDouble(double number)
{
#ifdef ES_VALUES_AS_NANS
    double_value = number;
    if (op_isnan(number))
    //if (private_type > ESTYPE_DOUBLE)
    {
        // NaN conversion
        private_type = ESTYPE_DOUBLE;
    }
#else
    private_type = ESTYPE_DOUBLE;
    private_value.number = number;
#endif
}

inline void
ES_Value_Internal::SetUInt32(UINT32 number)
{
    if (number < 2147483648U)
    {
        SetInt32(static_cast<INT32> (number));
    }
    else
    {
# ifdef ES_VALUES_AS_NANS
        // We know this is not NaN
        double_value = UNSIGNED2DOUBLE(number);
# else
        SetDouble(UNSIGNED2DOUBLE(number));
# endif
    }
}

inline void
ES_Value_Internal::SetInt32(INT32 number)
{
    private_type = ESTYPE_INT32;
    private_value.i32 = number;
}

inline void
ES_Value_Internal::SetNan()
{
    SetDouble(op_nan(NULL));
}

inline void
ES_Value_Internal::SetString(JString* string)
{
    private_type = ESTYPE_STRING;
    private_value.string = string;
}

inline void
ES_Value_Internal::SetObject(ES_Object* object)
{
    private_type = ESTYPE_OBJECT;
    private_value.object = object;
}

inline void
ES_Value_Internal::SetArgumentsCookie()
{
    private_type = ESTYPE_BOOLEAN;
    private_value.boolean = 666;
}

inline void
ES_Value_Internal::SetNoExceptionCookie()
{
    private_type = ESTYPE_BOOLEAN;
    private_value.boolean = 667;
}

inline void
ES_Value_Internal::SetBoxed(ES_Boxed* boxed)
{
    private_type = ESTYPE_BOXED;
    private_value.boxed = boxed;
}

inline BOOL
ES_Value_Internal::GetBoolean() const
{
    OP_ASSERT(IsBoolean());
    return private_value.boolean != 0;
}

inline BOOL
ES_Value_Internal::GetHiddenBoolean() const
{
    OP_ASSERT(IsUndefined());
#ifdef _DEBUG
    OP_ASSERT(private_value.boolean == 1 || private_value.boolean == 0);
#endif // _DEBUG
    return private_value.boolean != 0;
}

inline double
ES_Value_Internal::GetNumAsDouble() const
{
    OP_ASSERT(IsNumber());
    if (IsInt32())
        return INT2DOUBLE(GetInt32());
    else
        return GetDouble();
}

inline INT32
ES_Value_Internal::GetNumAsInt32() const
{
    OP_ASSERT(IsNumber());
    if (IsInt32())
        return GetInt32();
    else
        return DOUBLE2INT32(GetDouble());
}

inline UINT32
ES_Value_Internal::GetNumAsUInt32() const
{
    OP_ASSERT(IsNumber());
    if (IsInt32())
        return static_cast<UINT32> (GetInt32());
    else
    {
#ifdef INT_CAST_IS_ES262_COMPLIANT
        return static_cast<UINT32>(GetDouble());
#else // INT_CAST_IS_ES262_COMPLIANT
        return DOUBLE2UINT32(GetDouble());
#endif // INT_CAST_IS_ES262_COMPLIANT
    }
}

inline double
ES_Value_Internal::GetNumAsInteger() const
{
    OP_ASSERT(IsNumber());
    if (IsInt32())
    {
        return INT2DOUBLE(GetInt32());  // Bad -- but caller fixes this by only calling GetInteger on double values
    }
    else if (op_isnan(GetDouble()))
        return 0.0;
    else if (!op_isfinite(GetDouble()))
        return GetDouble();
    else
        return op_truncate(GetDouble());
}

inline INT32
ES_Value_Internal::GetNumAsBoundedInt32(INT32 nan_value) const
{
    OP_ASSERT(IsNumber());
    if (IsInt32())
        return GetInt32();
    else
        return GetNumAsBoundedInt32Slow(nan_value);
}

inline double
ES_Value_Internal::GetDouble() const
{
    OP_ASSERT(IsDouble());
#ifdef ES_VALUES_AS_NANS
    return double_value;
#else
    return private_value.number;
#endif
}

inline INT32
ES_Value_Internal::GetInt32() const
{
    OP_ASSERT(IsInt32());
    return private_value.i32;
}

inline ES_Object*
ES_Value_Internal::GetObject(ES_Context *context) const
{
    OP_ASSERT(IsObject());

    if (private_value.object->HasMultipleIdentities())
        return ES_Host_Object::Identity(context, static_cast<ES_Host_Object *>(private_value.object));
    else
        return private_value.object;
}

inline ES_Object*
ES_Value_Internal::GetObject() const
{
    OP_ASSERT(IsObject());
    return private_value.object;
}


inline ES_Object*&
ES_Value_Internal::GetObject_ref()
{
    OP_ASSERT(IsObject());
    return private_value.object;
}

inline JString*
ES_Value_Internal::GetString() const
{
    OP_ASSERT(IsString());
    return private_value.string;
}

inline JString*&
ES_Value_Internal::GetString_ref()
{
    OP_ASSERT(IsString());
    return private_value.string;
}

inline ES_Boxed*
ES_Value_Internal::GetBoxed() const
{
    OP_ASSERT(IsBoxed());
    return private_value.boxed;
}

inline ES_Boxed*&
ES_Value_Internal::GetBoxed_ref()
{
    OP_ASSERT(IsBoxed());
    return private_value.boxed;
}

#ifndef ES_VALUES_AS_NANS

inline unsigned
ES_Value_Internal::GetExtra() const
{
    return private_extra;
}

inline void
ES_Value_Internal::SetExtra(unsigned extra)
{
    private_extra = extra;
}

#endif

inline ES_Boxed*
ES_Value_Internal::GetDecodedBoxed() const
{
    OP_ASSERT(IsDecodedBoxed());

    if (IsString())
        return GetString();
    else if (IsObject())
        return GetObject();
    else
        return GetBoxed();
}

#if 0

inline ES_Boxed*&
ES_Value_Internal::GetDecodedBoxed_ref()
{
    OP_ASSERT(IsDecodedBoxed());

    if (IsString())
        return CAST_TO_BOXED_REF(GetString_ref());
    else if (IsObject())
        return CAST_TO_BOXED_REF(GetObject_ref());
    else
        return GetBoxed_ref();
}

inline ES_Boxed*&
ES_Value_Internal::GetForcedDecodedBoxed_ref()
{
    return CAST_TO_BOXED_REF(private_value.boxed);
}

#endif // 0

// FIXME: Inefficient value copying in To* functions.

inline BOOL
ES_Value_Internal::ToNumber(ES_Execution_Context *context)
{
    // sect 9.3
    if (!IsNumber())
        if (!IsObject())
            *this = AsNumber(context);
        else
            return ToNumberSlow(context);
    return TRUE;
}

inline BOOL
ES_Value_Internal::ToString(ES_Execution_Context *context)
{
    // sect 9.8
    if (!IsString())
        if (!IsObject())
            *this = AsString(context);
        else
            return ToStringSlow(context);
    return TRUE;
}

inline BOOL
ES_Value_Internal::ToString(ES_Context *context)
{
    if (!IsString())
    {
        OP_ASSERT(!IsObject());
        *this = AsString(context);
    }
    return TRUE;
}

inline void
ES_Value_Internal::ToBoolean()
{
    // sect 9.2
    if (!IsBoolean())
        *this = AsBoolean();
}

inline BOOL
ES_Value_Internal::ToObject(ES_Execution_Context *context, BOOL throw_exception)
{
    // sect 9.9

    if (IsObject())
        return TRUE;
    else
        return AsObject(context, *this, throw_exception);
}

inline BOOL
ES_Value_Internal::SimulateToObject(ES_Execution_Context *context) const
{
    // sect 9.9

    if (!IsNullOrUndefined())
        return TRUE;
    else
    {
        ES_Value_Internal dest(*this);
        return AsObject(context, dest, TRUE);
    }
}

inline ES_Value_Internal
ES_Value_Internal::AsString(ES_Context *context) const
{
    unsigned index;

    switch (Type())
    {
    case ESTYPE_UNDEFINED:
        index = STRING_undefined;
        break;

    case ESTYPE_NULL:
        index = STRING_null;
        break;

    case ESTYPE_BOOLEAN:
        index = GetBoolean() ? STRING_true : STRING_false;
        break;

    case ESTYPE_STRING:
        return *this;

    case ESTYPE_INT32:
        {
            int value = GetInt32();

            if (value >= 0 && value < STRING_NUMNUMBERS)
            {
                index = STRING_0 + value;
                break;
            }
        }

    default:
        return AsStringSlow(context);
    }

    return ES_Value_Internal(context->rt_data->strings[index]);
}

inline BOOL
ES_Value_Internal::FastEqual(ES_Value_Internal &lhs)
{
    return Type() == lhs.Type() && private_value.i32 == lhs.private_value.i32;
}

inline void
ES_Value_Internal::ExportL(ES_Context *context, ES_Value& dst, char format, ES_ValueString *dst_string) const
{
    switch (Type())
    {
    default:
    case ESTYPE_UNDEFINED:
        dst.type = VALUE_UNDEFINED;
        break;

    case ESTYPE_NULL:
        dst.type = VALUE_NULL;
        break;

    case ESTYPE_BOOLEAN:
        dst.type = VALUE_BOOLEAN;
        dst.value.boolean = !!GetBoolean();
        break;

    case ESTYPE_INT32:
    case ESTYPE_DOUBLE:
        dst.type = VALUE_NUMBER;
        dst.value.number = GetNumAsDouble();
        break;

    case ESTYPE_STRING:
    {
        unsigned length = Length(GetString());
        if (format == 'z' || format == 'Z')
        {
            dst.type = VALUE_STRING_WITH_LENGTH;
            dst.value.string_with_length = dst_string;
            dst_string->string = StorageZ(context, GetString());
            dst_string->length = length;
        }
        else
        {
            dst.type = VALUE_STRING;
            dst.value.string = StorageZ(context, GetString());
        }
        dst.string_length = length < 0x1000000u ? length : 0xffffffu;
        break;
    }

    case ESTYPE_OBJECT:
        dst.type = VALUE_OBJECT;
        dst.value.object = GetObject();
        break;
    }
}

/* n number
   s string
   S string or null
   z string with length
   Z string with length or null
   b boolean
   {everything else} do not typecast the argument
*/
inline BOOL
ES_Value_Internal::ConvertL(ES_Execution_Context *context, char format)
{
    switch (format)
    {
    case 'n':
        return ToNumber(context);
    case 's':
    case 'z':
        return ToString(context);
    case 'S':
    case 'Z':
        if (!IsNull() && !IsUndefined())
            return ToString(context);
        else
            return TRUE;
    case 'b':
        ToBoolean();
    default:
        return TRUE;
    }
}

/* static */ inline void
ES_Value_Internal::Memcpy(ES_Value_Internal &dst, char *src, ES_StorageType type)
{
#ifdef ES_VALUES_AS_NANS
    if (type == ES_STORAGE_WHATEVER || type == ES_STORAGE_DOUBLE)
#else
    if (type == ES_STORAGE_WHATEVER)
#endif // ES_VALUES_AS_NANS
        dst = *reinterpret_cast<ES_Value_Internal*>(src);
    else
    {
        OP_ASSERT(type != ES_STORAGE_NULL || *reinterpret_cast<UINTPTR*>(src) == 0);

        if (ES_Layout_Info::IsNullable(type) && *reinterpret_cast<UINTPTR*>(src) == 0)
            dst.private_type = ESTYPE_NULL;
        else
        {
#ifndef ES_VALUES_AS_NANS
            if (type > LAST_SIZE_4)
                dst.private_value = *reinterpret_cast<ES_ValueUnion*>(src);
            else
#endif // ES_VALUES_AS_NANS
                *reinterpret_cast<unsigned*>(&dst.private_value) = *reinterpret_cast<unsigned *>(src);

            dst.private_type = ConvertToValueType(type);
        }
    }

    OP_ASSERT(type != ES_STORAGE_NULL || (!dst.IsObject() && !dst.IsObject()));
    OP_ASSERT(!dst.IsObject() || dst.GetObject() != NULL);
    OP_ASSERT(!dst.IsString() || dst.GetString() != NULL);
}

/* static */ inline void
ES_Value_Internal::Memcpy(char *dst, const ES_Value_Internal &src, ES_StorageType type)
{
#ifdef DEBUG_ENABLE_OPASSERT
    BOOL ok_copy = ES_Layout_Info::IsSpecialType(type) || type == ES_STORAGE_WHATEVER || ES_Value_Internal::ConvertToStorageType(src.Type()) == type ||
        ES_Layout_Info::SuperType(ES_Value_Internal::ConvertToStorageType(src.Type())) == type ||
        (ES_Value_Internal::ConvertToStorageType(src.Type()) == ES_STORAGE_NULL && ES_Layout_Info::IsNullable(type));
    OP_ASSERT(ok_copy);
#endif

    OP_ASSERT(!src.IsObject() || src.GetObject() != NULL);
    OP_ASSERT(!src.IsString() || src.GetString() != NULL);

#ifdef ES_VALUES_AS_NANS
    if (type == ES_STORAGE_WHATEVER || type == ES_STORAGE_DOUBLE)
#else
    if (type == ES_STORAGE_WHATEVER)
#endif // ES_VALUES_AS_NANS
        *reinterpret_cast<ES_Value_Internal*>(dst) = src;
    else if (type == ES_STORAGE_NULL || ES_Layout_Info::IsNullable(type) && src.IsNull())
        *reinterpret_cast<UINTPTR*>(dst) = 0;
    else if (type == ES_STORAGE_DOUBLE && src.IsInt32())
        *reinterpret_cast<double *>(dst) = INT2DOUBLE(src.GetInt32());
    else if (type > LAST_SIZE_4)
        *reinterpret_cast<ES_ValueUnion *>(dst) = src.private_value;
    else
        *reinterpret_cast<unsigned *>(dst) = *reinterpret_cast<const unsigned *>(&src.private_value);
}

/* static */ inline unsigned
ES_Value_Internal::ConvertToValueTypeBits(ES_StorageType type)
{
    unsigned type_bits = 0;
    switch(type)
    {
    case ES_STORAGE_WHATEVER:
        type_bits |= ESTYPE_BITS_ALL_TYPES;
        break;
    case ES_STORAGE_NULL:
        type_bits |= ESTYPE_BITS_NULL;
        break;
    case ES_STORAGE_UNDEFINED:
        type_bits |= ESTYPE_BITS_UNDEFINED;
        break;
    case ES_STORAGE_BOOLEAN:
        type_bits |= ESTYPE_BITS_BOOLEAN;
        break;
    case ES_STORAGE_INT32:
        type_bits |= ESTYPE_BITS_INT32;
        break;
    case ES_STORAGE_DOUBLE:
        type_bits |= ESTYPE_BITS_DOUBLE;
        break;
    case ES_STORAGE_STRING_OR_NULL:
        type_bits |= ESTYPE_BITS_NULL;
        /* fall through */
    case ES_STORAGE_STRING:
        type_bits |= ESTYPE_BITS_STRING;
        break;
    case ES_STORAGE_OBJECT_OR_NULL:
        type_bits |= ESTYPE_BITS_NULL;
        /* fall through */
    case ES_STORAGE_OBJECT:
        type_bits |= ESTYPE_BITS_OBJECT;
        break;
    default:
        OP_ASSERT(!"This should not happen");
        break;
    }
    return type_bits;
}

/* static */ inline ES_ValueType
ES_Value_Internal::ConvertToValueType(ES_StorageType type)
{
    OP_ASSERT(type != ES_STORAGE_WHATEVER);
    switch(type)
    {
    case ES_STORAGE_NULL: return ESTYPE_NULL;
    case ES_STORAGE_UNDEFINED: return ESTYPE_UNDEFINED;
    case ES_STORAGE_BOOLEAN: return ESTYPE_BOOLEAN;
    case ES_STORAGE_INT32: return ESTYPE_INT32;
    case ES_STORAGE_DOUBLE: return ESTYPE_DOUBLE;
    case ES_STORAGE_INT32_OR_DOUBLE: return ESTYPE_INT32_OR_DOUBLE;
    case ES_STORAGE_STRING_OR_NULL:
    case ES_STORAGE_STRING: return ESTYPE_STRING;
    case ES_STORAGE_SPECIAL_PRIVATE:
    case ES_STORAGE_OBJECT_OR_NULL:
    case ES_STORAGE_OBJECT: return ESTYPE_OBJECT;
    case ES_STORAGE_BOXED: return ESTYPE_BOXED;
    }
    OP_ASSERT(!"Not reached");
    return ESTYPE_BOXED;
}

/* static */ inline ES_StorageType
ES_Value_Internal::ConvertToStorageType(ES_ValueType type)
{
    switch(type)
    {
    case ESTYPE_NULL: return ES_STORAGE_NULL;
    case ESTYPE_UNDEFINED: return ES_STORAGE_UNDEFINED;
    case ESTYPE_DOUBLE: return ES_STORAGE_DOUBLE;
    case ESTYPE_INT32: return ES_STORAGE_INT32;
    case ESTYPE_INT32_OR_DOUBLE: return ES_STORAGE_INT32_OR_DOUBLE;
    case ESTYPE_BOOLEAN: return ES_STORAGE_BOOLEAN;
    case ESTYPE_STRING: return ES_STORAGE_STRING;
    case ESTYPE_OBJECT: return ES_STORAGE_OBJECT;
    case ESTYPE_BOXED: return ES_STORAGE_BOXED;
    }

    return ES_STORAGE_WHATEVER;
}

/* static */ inline ES_StorageTypeBits
ES_Value_Internal::StorageTypeBits(ES_StorageType type)
{
    switch(type)
    {
    case ES_STORAGE_WHATEVER: return ES_STORAGE_BITS_WHATEVER;
    case ES_STORAGE_BOOLEAN: return ES_STORAGE_BITS_BOOLEAN;
    case ES_STORAGE_INT32: return ES_STORAGE_BITS_INT32;
    case ES_STORAGE_UNDEFINED: return ES_STORAGE_BITS_UNDEFINED;
    case ES_STORAGE_NULL: return ES_STORAGE_BITS_NULL;
    case ES_STORAGE_DOUBLE: return ES_STORAGE_BITS_DOUBLE;
    case ES_STORAGE_STRING:
    case ES_STORAGE_STRING_OR_NULL: return ES_STORAGE_BITS_STRING;
    case ES_STORAGE_OBJECT:
    case ES_STORAGE_OBJECT_OR_NULL: return ES_STORAGE_BITS_OBJECT;
    case ES_STORAGE_BOXED: return ES_STORAGE_BITS_BOXED;
    default:
        OP_ASSERT(!"This should not happen");
        return ES_STORAGE_BITS_WHATEVER;
    }
}

/* static */ inline ES_StorageType
ES_Value_Internal::StorageTypeFromBits(ES_StorageTypeBits bits, BOOL is_null)
{
    switch(bits)
    {
    case ES_STORAGE_BITS_WHATEVER: return ES_STORAGE_WHATEVER;
    case ES_STORAGE_BITS_BOOLEAN: return ES_STORAGE_BOOLEAN;
    case ES_STORAGE_BITS_INT32: return ES_STORAGE_INT32;
    case ES_STORAGE_BITS_UNDEFINED: return ES_STORAGE_UNDEFINED;
    case ES_STORAGE_BITS_NULL: return ES_STORAGE_NULL;
    case ES_STORAGE_BITS_DOUBLE: return ES_STORAGE_DOUBLE;
    case ES_STORAGE_BITS_STRING: return is_null ? ES_STORAGE_STRING_OR_NULL : ES_STORAGE_STRING;
    case ES_STORAGE_BITS_OBJECT: return is_null ? ES_STORAGE_OBJECT_OR_NULL : ES_STORAGE_OBJECT;
    case ES_STORAGE_BITS_BOXED: return ES_STORAGE_BOXED;
    default:
        OP_ASSERT(!"This should not happen");
        return ES_STORAGE_WHATEVER;
    }
}

/* static */ inline unsigned
ES_Value_Internal::CachedTypeBits(ES_StorageType type)
{
    unsigned cached_type = 0;
    if (type != ES_STORAGE_WHATEVER)
        cached_type |= static_cast<unsigned>(ES_Value_Internal::ConvertToValueType(type)) & ES_VALUE_TYPE_MASK;
    cached_type |= static_cast<unsigned>(ES_Value_Internal::StorageTypeBits(type)) << ES_STORAGE_TYPE_SHIFT;
    cached_type |= ES_Layout_Info::SizeOf(type) << ES_STORAGE_SIZE_SHIFT;
    if (type == ES_STORAGE_STRING_OR_NULL || type == ES_STORAGE_OBJECT_OR_NULL || type == ES_STORAGE_NULL)
        cached_type |= ES_STORAGE_NULL_MASK;
    return cached_type;
}

/* static */ inline ES_ValueType
ES_Value_Internal::ValueTypeFromCachedTypeBits(unsigned type)
{
    return static_cast<ES_ValueType>((type & ES_VALUE_TYPE_MASK) | ES_VALUE_TYPE_INIT_MASK);
}

/* static */ inline ES_StorageType
ES_Value_Internal::StorageTypeFromCachedTypeBits(unsigned type)
{
    return ES_Value_Internal::StorageTypeFromBits(static_cast<ES_StorageTypeBits>((type & ES_STORAGE_TYPE_MASK) >> ES_STORAGE_TYPE_SHIFT), ES_Value_Internal::IsNullableFromCachedTypeBits(type));
}

/* static */ inline BOOL
ES_Value_Internal::IsNullableFromCachedTypeBits(unsigned type)
{
    return (type & ES_STORAGE_NULL_MASK) != 0;
}

/* static */ inline unsigned
ES_Value_Internal::SizeFromCachedTypeBits(unsigned type)
{
    return (type & ES_STORAGE_SIZE_MASK) >> ES_STORAGE_SIZE_SHIFT;
}

inline ES_StorageType
ES_Value_Internal::GetStorageType() const
{
    return ES_Value_Internal::ConvertToStorageType(Type());
}

inline BOOL
ES_Value_Internal::CheckType(ES_StorageType type) const
{
    return ES_Layout_Info::CheckType(ES_Value_Internal::ConvertToStorageType(Type()), type);
}

inline BOOL
EqDouble(double a, double b)
{
#if defined ES_NEED_GUARD_AGAINST_NAN || defined NAN_EQUALS_EVERYTHING
    if (op_isnan(a) || op_isnan(b))
        return FALSE;
    else
#endif // defined ES_NEED_GUARD_AGAINST_NAN || defined NAN_EQUALS_EVERYTHING
        return a == b;
}

#ifdef ES_VALUES_AS_NANS
#undef private_type
#undef private_value
#endif

#endif // ES_VALUE_INLINES_H
