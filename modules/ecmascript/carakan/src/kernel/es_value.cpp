/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/object/es_boolean_object.h"
#include "modules/ecmascript/carakan/src/object/es_number_object.h"
#include "modules/ecmascript/carakan/src/object/es_string_object.h"
#include "modules/ecmascript/carakan/src/object/es_date_object.h"
#include "modules/ecmascript/carakan/src/builtins/es_object_builtins.h"

INT32
ES_Value_Internal::GetNumAsBoundedInt32Slow(INT32 nan_value) const
{
    OP_ASSERT(IsNumber());
    double d = GetDouble();
    if (op_isnan(d))
        return nan_value;
    if (!op_isfinite(d))
        return d < 0 ? INT_MIN : INT_MAX;
    INT32 i = 0;
    if (d > (double)INT_MAX)
        i = INT_MAX;
    else if (d < (double)INT_MIN)
        i = INT_MIN;
    else
        i = (INT32)d;
    return i;
}

double
ES_Value_Internal::GetNumAsIntegerRound() const
{
    OP_ASSERT(IsNumber());
    if (IsInt32())
    {
        return INT2DOUBLE(GetInt32());
    }
    else if (op_isnan(GetDouble()))
        return 0.0;
    else if (!op_isfinite(GetDouble()))
        return GetDouble();
    else
    {
        /* The rounding mode here is to round half-to-even. */
        double d = GetDouble();
        double n = op_floor(d);
        d -= n;
        if (d < 0.5)
            return n;
        if (d > 0.5)
            return (n + 1.0);
        op_modf(n / 2.0, &d);
        if (2.0 * d == n)
            return n;
        else
            return (n + 1.0);
    }
}

unsigned
ES_Value_Internal::GetNumAsUint8Clamped() const
{
    if (IsInt32())
    {
        int ivalue = GetInt32();
        return (ivalue < 0) ? 0 : (ivalue > 0xff) ? 0xff : static_cast<unsigned>(ivalue);
    }
    else
    {
        double dvalue = GetNumAsIntegerRound();
        return (dvalue < 0) ? 0 : (dvalue > 255.) ? 0xff : static_cast<unsigned>(dvalue);
    }
}

JString*
ES_Value_Internal::TypeString(ES_Context *context) const
{
    unsigned stringid;
    switch (Type())
    {
    case ESTYPE_UNDEFINED:
        stringid = STRING_undefined;
        break;
    case ESTYPE_NULL:
        stringid = STRING_object;
        break;
    case ESTYPE_BOOLEAN:
        stringid = STRING_boolean;
        break;
    case ESTYPE_INT32:
    case ESTYPE_DOUBLE:
        stringid = STRING_number;
        break;
    case ESTYPE_STRING:
        stringid = STRING_string;
        break;
    case ESTYPE_OBJECT:
        return GetObject()->GetTypeOf(context);
    default:
        stringid = STRING_undefined;
        break;
    }

    return context->rt_data->strings[stringid];
}

BOOL
ES_Value_Internal::ToPrimitive(ES_Execution_Context *context, HintType hint)
{
    ES_Object *object = GetObject(context);
    ES_Global_Object *global_object = context->GetGlobalObject();
    GC_Tag tag = object->GCTag();

    if (tag >= GCTAG_ES_Object_Number && tag <= GCTAG_ES_Object_Boolean)
        if (global_object->IsSimpleStringObject(object))
        {
            SetString(static_cast<ES_String_Object *>(object)->GetValue());
            return TRUE;
        }
        else if (global_object->IsSimpleNumberObject(object))
        {
            SetNumber(static_cast<ES_Number_Object *>(object)->GetValue());
            if (hint == HintString)
                *this = AsString(context);
            return TRUE;
        }
        else if (tag == GCTAG_ES_Object_Date)
        {
            if (hint == HintNumber)
            {
                if (global_object->IsSimpleDateObject(object))
                {
                    SetNumber(static_cast<ES_Date_Object *>(object)->GetValue());
                    return TRUE;
                }
            }
            else
                hint = HintString;
        }
        else if (global_object->IsSimpleBooleanObject(object))
        {
            BOOL value = static_cast<ES_Boolean_Object *>(object)->GetValue();

            if (hint == HintString)
            {
                JString **strings = context->rt_data->strings;
                SetString(value ? strings[STRING_true] : strings[STRING_false]);
            }
            else
                SetBoolean(value);
            return TRUE;
        }

    return ToPrimitiveInternal(context, hint);
}

static BOOL
IsBuiltinFunction(ES_Object *fn, ES_Function::BuiltIn *call)
{
    return !fn->IsHostObject() && static_cast<ES_Function *>(fn)->GetCall() == call;
}

BOOL
ES_Value_Internal::ToPrimitiveInternal(ES_Execution_Context *context, HintType hint)
{
    OP_ASSERT(IsObject());

    ES_Object *object = GetObject(context);
    ES_Value_Internal fn_value;

    for (unsigned index = 0; index < 2; ++index)
    {
        GetResult result;

        if (hint == HintString ? index == 0 : index == 1)
            result = context->GetToString(object, fn_value);
        else
            result = context->GetValueOf(object, fn_value);

        if (GET_OK(result))
        {
            if (fn_value.IsObject())
            {
                ES_Object *fn = fn_value.GetObject(context);

                if (fn->IsFunctionObject())
                {
                    if (IsBuiltinFunction(fn, ES_ObjectBuiltins::valueOf))
                        /* This just returns the this object, so not much point
                           in calling it here. */
                        continue;

                    ES_Value_Internal *registers = context->SetupFunctionCall(fn, 0, ES_VirtualStackFrame::VIA_TOPRIMITIVE);
                    registers[0].SetObject(object);
                    registers[1].SetObject(fn);
                    if (!context->CallFunction(registers, 0, this))
                        return FALSE;
                    if (!IsObject())
                        return TRUE;
                }
                else
                {
                    if (hint == HintString ? index == 0 : index == 1)
                        context->ThrowTypeError("when converting to primitive: toString is not callable");
                    else
                        context->ThrowTypeError("when converting to primitive: valueOf is not callable");

                    return FALSE;
                }
            }
        }
        else if (result == PROP_GET_FAILED)
            return FALSE;
    }

    context->ThrowTypeError("when converting to primitive: result is not a primitive");
    return FALSE;
}

ES_Value_Internal
ES_Value_Internal::AsNumber(ES_Context *context) const
{
    // sect 9.3

    ES_Value_Internal number;

    switch (Type())
    {
    case ESTYPE_STRING:
    {
        JString* str = GetString();
        int v = str->NumericValue();
        if (v != INT_MIN)
            number.SetInt32(v);
        else
            number.SetNumber(tonumber(context, Storage(context, str), Length(str), FALSE, FALSE));
        break;
    }

    case ESTYPE_NULL:
        number.SetInt32(0);
        break;

    case ESTYPE_BOOLEAN:
        number.SetInt32(GetBoolean() ? 1 : 0);
        break;

    case ESTYPE_INT32:
        number.SetInt32(GetInt32());
        break;

    case ESTYPE_DOUBLE:
        number.SetDouble(GetDouble());
        break;

    default:
        OP_ASSERT(!"Illegal value type in value; numbers are handled in-line");
        // FALLTHROUGH
    case ESTYPE_UNDEFINED:
        number.SetNan();
        break;
    }

    return number;
}

ES_Value_Internal
ES_Value_Internal::AsBoolean() const
{
    // sect 9.2

    ES_Value_Internal boolean;

    switch (Type())
    {
#ifndef ES_VALUES_AS_NANS
    case ESTYPE_DOUBLE:
        boolean.SetBoolean(!(op_isnan(GetDouble()) || GetDouble() == 0));
        break;

    default:
        OP_ASSERT(!"Illegal value type in value; booleans handled in-line");
        // FALLTHROUGH
#else
    default: //case ESTYPE_DOUBLE:
        boolean.SetBoolean(!(op_isnan(GetDouble()) || GetDouble() == 0));
        break;
#endif
    case ESTYPE_UNDEFINED:
    case ESTYPE_NULL:
        boolean.SetFalse();
        break;

    case ESTYPE_INT32:
        boolean.SetBoolean(!(GetInt32() == 0));
        break;

    case ESTYPE_BOOLEAN:
        boolean.SetBoolean(GetBoolean());
        break;

    case ESTYPE_STRING:
        boolean.SetBoolean(Length(GetString()) > 0);
        break;

    case ESTYPE_OBJECT:
        // FIXME: IsHiddenObject
        boolean.SetBoolean(!GetObject()->IsHiddenObject());
        break;
    }

    return boolean;
}

ES_Value_Internal
ES_Value_Internal::AsStringSlow(ES_Context *context) const
{
    // sect 9.8

    JString* string = NULL;
#ifndef ES_VALUES_AS_NANS
    JString** strings = context->rt_data->strings;
#endif

    switch (Type())
    {
    case ESTYPE_INT32:
    {
        INT32 i = GetInt32();

        OP_ASSERT(!(i >= 0 && i < STRING_NUMNUMBERS));

        uni_char buf[16]; // ARRAY OK 2008-06-09 stanislavj
        unsigned index = ARRAY_SIZE(buf);

        BOOL is_negative = FALSE;
        unsigned value;

        if (i < 0)
        {
            value = static_cast<unsigned>(-i);
            is_negative = TRUE;
        }
        else
            value = static_cast<unsigned>(i);

        static const char digits[] = "0123456789";
        while (value > 0)
        {
            buf[--index] = digits[value % 10];
            value /= 10;
        }

        if (index == ARRAY_SIZE(buf))
            buf[--index] = '0';

        if (is_negative)
            buf[--index] = '-';

        string = JString::Make(context, buf + index, ARRAY_SIZE(buf) - index);
        JString::SetValue(string, i);
        break;
    }

    default:
#ifndef ES_VALUES_AS_NANS
        OP_ASSERT(!"Illegal value type in value, strings are handled inline");

        // Don't crash, this happens if eg a toString() returns an Object, as
        // in the case of the escape() function.  That's probably a logic bug
        // elsewhere, but guard against it here.

        string = strings[STRING_empty];
        break;

    case ESTYPE_DOUBLE:
#endif
        string = tostring(context, GetDouble());
        break;
    }

    ES_Value_Internal result;
    result.SetString(string);
    return result;
}

ES_Value_Internal
ES_Value_Internal::AsStringSpecial(ES_Execution_Context *context) const
{
    switch (Type())
    {
    case ESTYPE_NULL:
    {
        // For a host object's functions/properties convert null into "" instead of "null"
        JString** strings = context->rt_data->strings;
        ES_Value_Internal result;
        result.SetString(strings[STRING_empty]);
        return result;
    }

    default:
        return AsString(context);
    }

}


// AsObject is the only piece of C++ code in the entire run-time system
// that knows how to create certain basic object types from primitive values.
// It duplicates functionality available in the libraries, but since it must
// sometimes be called synchronously it exists also as C++.

BOOL
ES_Value_Internal::AsObject(ES_Execution_Context *context, ES_Value_Internal& dest, BOOL throw_exception) const
{
    // sect 9.9

    OP_ASSERT(!IsObject());

    ES_Global_Object* global_object = context->GetGlobalObject();

    switch (Type())
    {
    case ESTYPE_STRING:
        dest.SetObject(ES_String_Object::Make(context, global_object, GetString()));
        break;

    case ESTYPE_BOOLEAN:
        // Avoid using ES_Boolean
        dest.SetObject(ES_Boolean_Object::Make(context, global_object, GetBoolean()));
        break;

    case ESTYPE_DOUBLE:
    case ESTYPE_INT32:
        dest.SetObject(ES_Number_Object::Make(context, global_object, GetNumAsDouble()));
        break;

    default:
        // FIXME: missing conversions here.
        OP_ASSERT(FALSE);
    case ESTYPE_UNDEFINED:
    case ESTYPE_NULL:
        if (throw_exception)
            context->ThrowTypeError("Cannot convert undefined or null to Object");
        return FALSE;
    }

    return TRUE;
}

BOOL
ES_Value_Internal::ToNumberSlow(ES_Execution_Context *context)
{
    if (!ToPrimitive(context, HintNumber))
        return FALSE;
    if (!IsNumber())
        *this = AsNumber(context);
    return TRUE;
}

BOOL
ES_Value_Internal::ToStringSlow(ES_Execution_Context *context)
{
    if (!ToPrimitive(context, HintString))
        return FALSE;
    if (!IsString())
        *this = AsString(context);
    return TRUE;
}

OP_STATUS
ES_Value_Internal::Import(ES_Context *context, const ES_Value& src)
{
    TRAPD(r, ImportL(context, src));
    return r;
}

void
ES_Value_Internal::ImportL(ES_Context *context, const ES_Value& src)
{
    switch (src.type)
    {
    case VALUE_UNDEFINED:
        SetUndefined();
        break;

    case VALUE_NULL:
        SetNull();
        break;

    case VALUE_BOOLEAN:
        SetBoolean(src.value.boolean ? TRUE : FALSE);        // In case other values than TRUE or FALSE occur
        break;

    case VALUE_NUMBER:
        SetNumber(src.value.number);
        break;

    case VALUE_STRING:
        OP_ASSERT(src.value.string != NULL);
        SetString(JString::Make(context, src.value.string, src.GetStringLength()));
        break;

    case VALUE_STRING_WITH_LENGTH:
        OP_ASSERT(src.value.string_with_length->string != NULL);
        SetString(JString::Make(context, src.value.string_with_length->string,
                                     src.value.string_with_length->length));
        break;

    case VALUE_OBJECT:
        OP_ASSERT(src.value.object != NULL);

        if (src.value.object->IsHostObject() && static_cast<ES_Host_Object *>(src.value.object)->GetRuntime()->GetHeap() != context->heap)
        {
            if (context->IsUsingStandardStack())
                LEAVE_IF_ERROR(context->heap->MergeWith(static_cast<ES_Host_Object *>(src.value.object)->GetRuntime()->GetHeap()));
            else
            {
                ES_SuspendedMergeHeapWith call(context->heap, static_cast<ES_Host_Object *>(src.value.object)->GetRuntime()->GetHeap());
                static_cast<ES_Execution_Context *>(context)->SuspendedCall(&call);
                if (OpStatus::IsError(call.result))
                    context->AbortOutOfMemory();
            }
        }

        SetObject(src.value.object);
        break;
    }
}

void
ES_Value_Internal::ImportUnlockedL(ES_Context *context, const ES_Value& src)
{
    switch (src.type)
    {
    case VALUE_UNDEFINED:
        SetUndefined();
        break;

    case VALUE_NULL:
        SetNull();
        break;

    case VALUE_BOOLEAN:
        SetBoolean(src.value.boolean ? TRUE : FALSE);        // In case other values than TRUE or FALSE occur
        break;

    case VALUE_NUMBER:
        SetNumber(src.value.number);
        break;

    case VALUE_STRING:
        {
            ES_CollectorLock gclock(context);

            OP_ASSERT(src.value.string != NULL);
            SetString(JString::Make(context, src.value.string, src.GetStringLength()));
            break;
        }

    case VALUE_STRING_WITH_LENGTH:
        {
            ES_CollectorLock gclock(context);

            OP_ASSERT(src.value.string_with_length->string != NULL);
            SetString(JString::Make(context, src.value.string_with_length->string, src.value.string_with_length->length));
            break;
        }

    case VALUE_OBJECT:
        OP_ASSERT(src.value.object != NULL);

        if (src.value.object->IsHostObject() && static_cast<ES_Host_Object *>(src.value.object)->GetRuntime()->GetHeap() != context->heap)
        {
            ES_CollectorLock gclock(context);

            if (context->IsUsingStandardStack())
                LEAVE_IF_ERROR(context->heap->MergeWith(static_cast<ES_Host_Object *>(src.value.object)->GetRuntime()->GetHeap()));
            else
            {
                ES_SuspendedMergeHeapWith call(context->heap, static_cast<ES_Host_Object *>(src.value.object)->GetRuntime()->GetHeap());
                static_cast<ES_Execution_Context *>(context)->SuspendedCall(&call);
                if (OpStatus::IsError(call.result))
                    context->AbortOutOfMemory();
            }
        }

        SetObject(src.value.object);
        break;
    }
}

OP_STATUS
ES_Value_Internal::Export(ES_Context *context, ES_Value& dst) const
{
    TRAPD(r, ExportL(context, dst));
    return r;
}

#ifdef ES_NATIVE_SUPPORT

/* static */ int ES_CALLING_CONVENTION
ES_Value_Internal::ReturnAsBoolean(ES_Value_Internal *value)
{
    if (value->IsObject())
        if (value->GetObject()->IsHiddenObject())
        {
            value->SetUndefined();
            return FALSE;
        }
        else
            return TRUE;
    else
        return value->AsBoolean().GetBoolean();
}

#endif // ES_NATIVE_SUPPORT

/* static */ BOOL
ES_Value_Internal::IsSameValue(ES_Context *context, const ES_Value_Internal &first0, const ES_Value_Internal &second0)
{
    const ES_Value_Internal *first = &first0, *second = &second0;
    ES_Value_Internal first1, second1;

    if (first->Type() != second->Type())
        if (first->IsNumber() && second->IsNumber())
        {
            /* Might be the same number, stored as ESTYPE_INT32 in one value and
               as ESTYPE_DOUBLE in the other. */
            first1.SetDouble(first->GetNumAsDouble());
            first = &first1;

            second1.SetDouble(second->GetNumAsDouble());
            second = &second1;
        }
        else
            return FALSE;

    switch (first->Type())
    {
    case ESTYPE_NULL:
    case ESTYPE_UNDEFINED:
        return TRUE;

    case ESTYPE_INT32:
        return first->GetInt32() == second->GetInt32();

    case ESTYPE_DOUBLE:
    {
        double d1(first->GetDouble()), d2(second->GetDouble());
        if (op_isnan(d1) && op_isnan(d2))
            return TRUE;
        else if (op_isnan(d1) || op_isnan(d2))
            return FALSE;
        else if (op_signbit(d1) != op_signbit(d2))
            return FALSE;
        else
            return d1 == d2;
    }

    case ESTYPE_BOOLEAN:
        return first->GetBoolean() == second->GetBoolean();

    case ESTYPE_STRING:
        return first->GetString()->Equals(second->GetString());

    case ESTYPE_OBJECT:
        return first->GetObject(context) == second->GetObject(context);

    default:
        OP_ASSERT(!"Incorrect use!");
        return FALSE;
    }
}

#ifdef _DEBUG
# include "modules/ecmascript/carakan/src/kernel/es_value_inlines.h"
#endif // _DEBUG
