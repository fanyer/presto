/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_number_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_number_object.h"
#include "modules/stdlib/include/double_format.h"

#define ES_THIS_NUMBER() if (!ProcessThis(context, argv[-2])) return FALSE; double this_number = argv[-2].GetNumAsDouble();

/* FIXME: there are obvious similarities between constructor_call and constructor_construct... */

/* static */ BOOL
ES_NumberBuiltins::constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc == 0)
    {
        return_value->SetInt32(0);
        return TRUE;
    }
    else
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        *return_value = argv[0];
        return TRUE;
    }
}

/* static */ BOOL
ES_NumberBuiltins::constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double value_number;

    if (argc == 0)
        value_number = 0;
    else
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        value_number = argv[0].GetNumAsDouble();
    }

    return_value->SetObject(ES_Number_Object::Make(context, ES_GET_GLOBAL_OBJECT(), value_number));
    return TRUE;
}

static BOOL
StrictProcessThisNumber(double &this_number, const ES_Value_Internal &this_value)
{
    if (this_value.IsNumber())
        this_number = this_value.GetNumAsDouble();
    else if (this_value.IsObject() && this_value.GetObject()->IsNumberObject())
        this_number = static_cast<ES_Number_Object *>(this_value.GetObject())->GetValue();
    else
        return FALSE;

    return TRUE;
}

/* static */ BOOL
ES_NumberBuiltins::toStringInternal(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, BOOL toLocale)
{
    double this_number;

    if (!StrictProcessThisNumber(this_number, argv[-2]))
    {
        context->ThrowTypeError(toLocale
                                ? "Number.prototype.toLocaleString: this is not a Number object"
                                : "Number.prototype.toString: this is not a Number object");
        return FALSE;
    }

    unsigned radix = 10;

    if (argc != 0 && !argv[0].IsUndefined() && !toLocale)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        radix = argv[0].GetNumAsUInt32();
        if (radix < 2 || radix > 36)
        {
            context->ThrowRangeError("Number.prototype.toString: radix out of range");
            return FALSE;
        }
    }

    if (radix == 10 || op_isnan(this_number) || op_isinf(this_number))
    {
        return_value->SetNumber(this_number);
        return return_value->ToString(context);
    }

    int ivalue = static_cast<int>(this_number);

    if (this_number == ivalue && !toLocale && (ivalue != 0 || 1/this_number > 0))
    {
        BOOL is_negative = FALSE;
        unsigned value;

        if (ivalue < 0)
        {
            value = static_cast<unsigned>(-ivalue);
            is_negative = TRUE;
        }
        else
            value = static_cast<unsigned>(ivalue);

        /* lowercase important -- spec doesn't require it but sites depend on it, see #239699 */
        static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

        /* 32 bits + sign (radix == 2) */
        uni_char buf[33]; // ARRAY OK 2012-02-03 sof
        unsigned index = ARRAY_SIZE(buf);

        while (value > 0)
        {
            buf[--index] = digits[value%radix];
            value /= radix;
        }

        if (index == ARRAY_SIZE(buf))
            buf[--index] = '0';

        if (is_negative)
            buf[--index] = '-';

        OP_ASSERT(index < ARRAY_SIZE(buf));

        JString *result = JString::Make(context, buf + index, ARRAY_SIZE(buf) - index);
        return_value->SetString(result);
    }
    else
        /* Using a suspended call here appears conservative, the callee is not too stack hungry, but will leave as-is until a known hotspot. */
        return_value->SetString(ecma_suspended_numbertostring(context, this_number, radix));

    return TRUE;
}

/* static */ BOOL
ES_NumberBuiltins::toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return toStringInternal(context, argc, argv, return_value, FALSE);
}

/* static */ BOOL
ES_NumberBuiltins::toLocaleString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    return toStringInternal(context, argc, argv, return_value, TRUE);
}

/* static */ BOOL
ES_NumberBuiltins::valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double this_number;

    if (!StrictProcessThisNumber(this_number, argv[-2]))
    {
        context->ThrowTypeError("Number.prototype.valueOf: this is not a Number object");
        return FALSE;
    }

    return_value->SetNumber(this_number);

    return TRUE;
}

class ES_SuspendedToFixed
    : public ES_SuspendedCall
{
public:
    ES_SuspendedToFixed(ES_Execution_Context *context, char *buffer, double value, int precision, size_t buffer_size)
        : buffer(buffer)
        , value(value)
        , precision(precision)
        , buffer_size(buffer_size)
        , result(NULL)
    {
        context->SuspendedCall(this);
    }

    char *buffer;
    double value;
    int precision;
    size_t buffer_size;
    char *result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        result = OpDoubleFormat::ToFixed(buffer, value, precision, buffer_size);
    }
};

/* static */ BOOL
ES_NumberBuiltins::toFixed(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double this_number;

    if (!StrictProcessThisNumber(this_number, argv[-2]))
    {
        context->ThrowTypeError("Number.prototype.toFixed: this is not a Number object");
        return FALSE;
    }

    ES_CollectorLock gclock(context);

    double fractions = 0;
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        fractions = argv[0].GetNumAsInteger();
    }
    if (fractions < 0 || fractions > 20)
    {
        context->ThrowRangeError("Number.prototype.toFixed: fractionDigits out of range");
        return FALSE;
    }
    int ifractions = static_cast<int>(fractions);
    if (op_isnan(this_number) || !op_isfinite(this_number) || op_fabs(this_number) >= 1E21)
    {
        return_value->SetNumber(this_number);
        if (!return_value->ToString(context))
            return FALSE;
        return TRUE;
    }

    char s[43]; // ARRAY OK 2011-06-08 sof
    return_value->SetString(JString::Make(context, ES_SuspendedToFixed(context, s, this_number, ifractions, ARRAY_SIZE(s)).result));

    return TRUE;
}

class ES_SuspendedToExponential
    : public ES_SuspendedCall
{
public:
    ES_SuspendedToExponential(ES_Execution_Context *context, char *buffer, double value, int precision)
        : buffer(buffer)
        , value(value)
        , precision(precision)
        , result(NULL)
    {
        context->SuspendedCall(this);
    }

    char *buffer;
    double value;
    int precision;
    char *result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        result = OpDoubleFormat::ToExponential(buffer, value, precision);
    }
};

/* static */ BOOL
ES_NumberBuiltins::toExponential(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double this_number;

    if (!StrictProcessThisNumber(this_number, argv[-2]))
    {
        context->ThrowTypeError("Number.prototype.toExponential: this is not a Number object");
        return FALSE;
    }

    ES_CollectorLock gclock(context);

    BOOL undefined = FALSE;

    if (op_isnan(this_number) || !op_isfinite(this_number))
    {
        return_value->SetNumber(this_number);
        if (!return_value->ToString(context))
            return FALSE;
        return TRUE;
    }

    if (argc == 0)
        undefined = TRUE;
    else
        undefined = argv[0].IsUndefined();

    if (undefined)
    {
        char s[43]; // ARRAY OK 2011-06-08 sof
        return_value->SetString(JString::Make(context, ES_SuspendedToExponential(context, s, this_number, -1).result));
        return TRUE;
    }

    if (!argv[0].ToNumber(context))
        return FALSE;
    double fractions = argv[0].GetNumAsInteger();

    if (fractions < 0 || fractions > 20)
    {
        context->ThrowRangeError("Number.prototype.toExponential: fractionDigits out of range");
        return FALSE;
    }

    int ifractions = static_cast<int>(fractions);

    char s[43]; // ARRAY OK 2011-06-08 sof
    return_value->SetString(JString::Make(context, ES_SuspendedToExponential(context, s, this_number, ifractions).result));

    return TRUE;
}

class ES_SuspendedToPrecision
    : public ES_SuspendedCall
{
public:
    ES_SuspendedToPrecision(ES_Execution_Context *context, char *buffer, double value, int precision, size_t buffer_size)
        : buffer(buffer)
        , value(value)
        , precision(precision)
        , buffer_size(buffer_size)
        , result(NULL)
    {
        context->SuspendedCall(this);
    }

    char *buffer;
    double value;
    int precision;
    size_t buffer_size;
    char *result;

private:
    virtual void DoCall(ES_Execution_Context *context)
    {
        result = OpDoubleFormat::ToPrecision(buffer, value, precision, buffer_size);
    }
};

/* static */ BOOL
ES_NumberBuiltins::toPrecision(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double this_number;

    if (!StrictProcessThisNumber(this_number, argv[-2]))
    {
        context->ThrowTypeError("Number.prototype.toPrecision: this is not a Number object");
        return FALSE;
    }

    ES_CollectorLock gclock(context);

    BOOL undefined = FALSE;

    if (op_isnan(this_number) || !op_isfinite(this_number))
    {
        return_value->SetNumber(this_number);
        if (!return_value->ToString(context))
            return FALSE;
        return TRUE;
    }

    if (argc == 0)
        undefined = TRUE;
    else
        undefined = argv[0].IsUndefined();

    if (undefined)
    {
        return_value->SetNumber(this_number);
        if (!return_value->ToString(context))
            return FALSE;
        return TRUE;
    }

    double fractions = 0;
    if (!argv[0].ToNumber(context))
        return FALSE;
    fractions = argv[0].GetNumAsInteger();

    if (fractions < 1 || fractions > 21)
    {
        context->ThrowRangeError("Number.prototype.toPrecision: fractionDigits out of range");
        return FALSE;
    }
    int ifractions = static_cast<int>(fractions);

    char s[43]; // ARRAY OK 2011-06-08 sof
    return_value->SetString(JString::Make(context, ES_SuspendedToPrecision(context, s, this_number, ifractions, ARRAY_SIZE(s)).result));

    return TRUE;
}

/* static */ void
ES_NumberBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_NumberBuiltins);

    APPEND_PROPERTY(ES_NumberBuiltins, constructor,     ES_Value_Internal());
    APPEND_PROPERTY(ES_NumberBuiltins, toString,        MAKE_BUILTIN(1, toString));
    APPEND_PROPERTY(ES_NumberBuiltins, toLocaleString,  MAKE_BUILTIN(0, toLocaleString));
    APPEND_PROPERTY(ES_NumberBuiltins, valueOf,         MAKE_BUILTIN(0, valueOf));
    APPEND_PROPERTY(ES_NumberBuiltins, toFixed,         MAKE_BUILTIN(1, toFixed));
    APPEND_PROPERTY(ES_NumberBuiltins, toExponential,   MAKE_BUILTIN(1, toExponential));
    APPEND_PROPERTY(ES_NumberBuiltins, toPrecision,     MAKE_BUILTIN(1, toPrecision));

    ASSERT_OBJECT_COUNT(ES_NumberBuiltins);
}
/* static */ void
ES_NumberBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_NumberBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_NumberBuiltins, constructor,    DE, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_NumberBuiltins, toString,       DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_NumberBuiltins, toLocaleString, DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_NumberBuiltins, valueOf,        DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_NumberBuiltins, toFixed,        DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_NumberBuiltins, toExponential,  DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_NumberBuiltins, toPrecision,    DE, ES_STORAGE_OBJECT);
}

/* static */ void
ES_NumberBuiltins::PopulateConstructor(ES_Context *context, ES_Object *constructor)
{
    JString **idents = context->rt_data->idents;
    constructor->InitPropertyL(context, idents[ESID_MAX_VALUE], DBL_MAX, DD|DE|RO);
#ifdef IEEE_DENORMALS
    double min_val= op_implode_double(0x00000000UL, 0x00000001UL);
#else
    double min_val = DBL_MIN;
#endif
    constructor->InitPropertyL(context, idents[ESID_MIN_VALUE], min_val, DD|DE|RO);
    ES_Value_Internal value;
    value.SetNan();
    constructor->InitPropertyL(context, idents[ESID_NaN], value, DD|DE|RO);
    value.SetNumber(context->rt_data->NegativeInfinity);
    constructor->InitPropertyL(context, idents[ESID_NEGATIVE_INFINITY], value, DD|DE|RO);
    value.SetNumber(context->rt_data->PositiveInfinity);
    constructor->InitPropertyL(context, idents[ESID_POSITIVE_INFINITY], value, DD|DE|RO);
}

/* static */ BOOL
ES_NumberBuiltins::ProcessThis(ES_Execution_Context *context, ES_Value_Internal &this_value)
{
    /* We don't convert string primitives to String objects for calls to builtin
       functions.  But if we had, we would have called String.prototype.toString
       to convert it back.  And since that function can be overridden by a
       script, we need to make sure it isn't, since then it might return
       something else than the original string primitive, invalidating our
       optimization.  Odd special case that probably never happens, but still
       what we should do according to the specification. */

    if (this_value.IsNumber())
    {
        // FIXME: is this the correct global object?
        ES_Global_Object *global_object = context->GetGlobalObject();
        ES_Value_Internal value;

        GetResult result = global_object->Number_prototype_valueOf.GetL(context, value);
        if (result == PROP_GET_OK && value.IsObject() && value.GetObject()->IsFunctionObject() && static_cast<ES_Function *>(value.GetObject())->GetCall() == ES_NumberBuiltins::valueOf)
            return TRUE;

        if (!this_value.ToObject(context))
            return FALSE;
    }

    if (!this_value.ToNumber(context))
        return FALSE;

    return TRUE;
}
