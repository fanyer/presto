/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_math_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_math_builtins_support.h"

#ifndef _STANDALONE
# include "modules/libcrypto/include/OpRandomGenerator.h"
#endif // !_STANDALONE

/* static */ BOOL
ES_MathBuiltins::abs(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        if (argv[0].IsInt32())
        {
            if (argv[0].GetInt32() != INT_MIN)
                return_value->SetInt32(::op_abs(argv[0].GetInt32()));
            else
                return_value->SetDouble(INT2DOUBLE(INT_MAX)+1);
        }
        else if (argv[0].IsDouble())
        {
            double d = argv[0].GetNumAsDouble();
            if (!op_isnan(d))
                d = op_isfinite(d) ? ::op_fabs(d) : context->rt_data->PositiveInfinity;
            return_value->SetDouble(d);
        }
    }
    else
    {
        return_value->SetNan();
    }
    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::sqrt(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        return_value->SetNumber(::op_sqrt(argv[0].GetNumAsDouble()));
    }
    else
    {
        return_value->SetNan();
    }

    return TRUE;
}

/* static */ double
ES_MathBuiltinSupport::HandlePowSpecial(double r, double x, double y)
{
    /* The conditions for reporting special values (NaN and Infinities) are long and varied -- cf. ES5 spec, section 15.8.2.12 for
       pow(), and simply using a platform's libc implementation of pow() (via op_pow()) exposes cross-platform
       variations, which isn't good enough. Hence, we post process and tidy up the result from op_pow() whenever
       the platform routine reports a NaN.

       Code is closely based on Futhark's ESLibrarySupport::MathPow(). */
    OP_ASSERT(op_isnan(r));

    /* 'bullet' references below refer to the ES5 spec for pow() (section 15.8.2.12.) */
    if (op_isnan(y)) // (bullet 1)
        return RT_DATA.NaN;
    else if (op_fabs(y) == 0) // (bullet 2, 3)
        return 1;
    else if (op_isnan(x)) // y is non-zero (bullet 4)
        return RT_DATA.NaN;
    else if (op_isinf(y)) // bullets 5-10
    {
        if (op_fabs(x) > 1)
            return op_signbit(y) == 0 ? RT_DATA.PositiveInfinity : 0.0;
        else if (op_fabs(x) == 1)
            return RT_DATA.NaN;
        else
            return op_signbit(y) == 0 ? 0.0 : RT_DATA.PositiveInfinity;
    }
    else if (op_isinf(x))
    {
        if (op_signbit(x) == 0)
            return y > 0 ? RT_DATA.PositiveInfinity : 0.0;
        else if (op_isintegral(y))
            if (op_signbit(y) == 0)
                return ((long)op_floor(y) % 2) ? RT_DATA.NegativeInfinity : RT_DATA.PositiveInfinity;
            else
                return ((long)op_floor(y) % 2) ? RT_DATA.NegativeZero : 0.0;
    }
    else if (op_fabs(x) == 0)
    {
        if (op_signbit(x) == 0)
            return op_signbit(y) == 0 ? 0.0 : RT_DATA.PositiveInfinity;
        else if (op_isintegral(y))
            if (op_signbit(y) == 0)
                return ((long)op_floor(y) % 2) ? RT_DATA.NegativeZero : 0.0;
            else
                return ((long)op_floor(y) % 2) ? RT_DATA.NegativeInfinity : RT_DATA.PositiveInfinity;
    }
    /* last special case for finite(x) && x < 0: if platform reported NaN w/ an integral exponent,
     * that's incorrect, should be 0. Otherwise, NaN. */
    else if (x < 0 && op_isfinite(x) && op_isintegral(y))
        return (x > -1.0 ? 0.0 : RT_DATA.PositiveInfinity);
    return r;
}

/* static */ BOOL
ES_MathBuiltins::pow(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 2)
    {
        if (!argv[0].ToNumber(context) || !argv[1].ToNumber(context))
            return FALSE;

        double number = argv[0].GetNumAsDouble();
        double exp = argv[1].GetNumAsDouble();

        if (op_isnan(exp) || (op_fabs(number) == 1.0 && op_isinf(exp)))
            return_value->SetNan();
        else
        {
            double r = ::op_pow(number, exp);
            if (op_isnan(r))
                return_value->SetNumber(ES_MathBuiltinSupport::HandlePowSpecial(r, number, exp));
            else
                return_value->SetNumber(r);
        }
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::sin(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        double argument = argv[0].GetNumAsDouble();

#ifdef USE_CUSTOM_DOUBLE_OPS
        double result = context->Sin(argument);
#else // USE_CUSTOM_DOUBLE_OPS
        double result = ::op_sin(argument);
#endif // USE_CUSTOM_DOUBLE_OPS

        return_value->SetNumber(result);
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::asin(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        return_value->SetNumber(::op_asin(argv[0].GetNumAsDouble()));
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::cos(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        double argument = argv[0].GetNumAsDouble();

#ifdef USE_CUSTOM_DOUBLE_OPS
        double result = context->Cos(argument);
#else // USE_CUSTOM_DOUBLE_OPS
        double result = ::op_cos(argument);
#endif // USE_CUSTOM_DOUBLE_OPS

        return_value->SetNumber(result);
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::acos(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        return_value->SetNumber(::op_acos(argv[0].GetNumAsDouble()));
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::tan(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        double argument = argv[0].GetNumAsDouble();

#ifdef USE_CUSTOM_DOUBLE_OPS
        double result = context->Tan(argument);
#else // USE_CUSTOM_DOUBLE_OPS
        double result = ::op_tan(argument);
#endif // USE_CUSTOM_DOUBLE_OPS

        return_value->SetNumber(result);
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::atan(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        return_value->SetNumber(::op_atan(argv[0].GetNumAsDouble()));
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ double
ES_MathBuiltinSupport::HandleAtan2Special(double r, double y, double x)
{
    /* Implementing atan2() reliably across platforms requires some back patching to handle imprecise results from platform's libc impl;
       i.e., implement ES spec for Math.atan2() 15.8.2.5 */
    OP_ASSERT(op_isnan(r) || op_isinf(r));

    /* 23 cases to consider: */
    if (op_isnan(x) || op_isnan(y)) /* case: 1 */
        return RT_DATA.NaN;
    else if (y > 0 && op_fabs(x) == 0) /* case: 2,3 */
        return (M_PI/2);
    else if (op_fabs(y) == 0) /* case: 4,5,6,7,8,9,10,11 */
        return op_copysign(op_signbit(x)*M_PI, y);
    else if (y < 0 && op_fabs(x) == 0) /* case: 12, 13 */
        return (-M_PI/2);
    else if (op_isinf(x) && op_isfinite(y)) /* case: 14-17 */
        return (op_signbit(x) == 0 ? op_copysign(0, y) : op_copysign(M_PI, y));
    else if (op_isinf(y) && op_isfinite(x)) /* case: 18,19 */
        return op_copysign(M_PI/2, y);
    else if (op_isinf(y) && op_isinf(x)) /* case: 20-23 */
        if (op_signbit(x) == 0)
            return op_copysign(M_PI/4, y);
        else
            return op_copysign(3*M_PI/4, y);
    else
        return r;
}

/* static */ BOOL
ES_MathBuiltins::atan2(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 2)
    {
        if (!argv[0].ToNumber(context) || !argv[1].ToNumber(context))
            return FALSE;

        double d1 = argv[0].GetNumAsDouble();
        double d2 = argv[1].GetNumAsDouble();

        double r  = ::op_atan2(d1, d2);

        if (op_isnan(r) || op_isinf(r))
            return_value->SetNumber(ES_MathBuiltinSupport::HandleAtan2Special(r, d1, d2));
        else
            return_value->SetNumber(r);
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::max(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    unsigned i;
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        double ret = argv[0].GetNumAsDouble();
        if (op_isnan(ret))
        {
            i = 1;
            goto check_remaining_and_return;
        }
        for (i = 1; i < argc; i++)
        {
            if (!argv[i].ToNumber(context))
                return FALSE;
            double d = argv[i].GetNumAsDouble();
            if (op_isnan(d))
            {
                i++;
                goto check_remaining_and_return;
            }
            // Check for the case max(-0, 0)
            if (ret != d || op_signbit(ret) == 1)
                ret = ret > d ? ret : d;
        }
        return_value->SetNumber(ret);
    }
    else
        return_value->SetNumber(context->rt_data->NegativeInfinity);

    return TRUE;

check_remaining_and_return:
    /* Parse all arguments, even if one is NaN. It is possible that ToNumber
       will fail for one of them, and then we should not return a value. */
    for (; i < argc; i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    return_value->SetNan();
    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::min(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    unsigned i;
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;
        double ret = argv[0].GetNumAsDouble();
        if (op_isnan(ret))
        {
            i = 1;
            goto check_remaining_and_return;
        }
        for (i = 1; i < argc; i++)
        {
            if (!argv[i].ToNumber(context))
                return FALSE;
            double d = argv[i].GetNumAsDouble();
            if (op_isnan(d))
            {
                i++;
                goto check_remaining_and_return;
            }
            if (ret != d || op_signbit(ret) == 0)
                ret = ret < d ? ret : d;
        }
        return_value->SetNumber(ret);
    }
    else
        return_value->SetNumber(context->rt_data->PositiveInfinity);

    return TRUE;

check_remaining_and_return:
    /* Parse all arguments, even if one is NaN. It is possible that ToNumber
       will fail for one of them, and then we should not return a value. */
    for (; i < argc; i++)
        if (!argv[i].ToNumber(context))
            return FALSE;

    return_value->SetNan();
    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::round(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        double number = argv[0].GetNumAsDouble();

#ifdef HAS_ROUND
        number = number > 0 ? round(number) : rint(number);
#else
        if (!op_isnan(number))
        {
            int exponent;
            op_frexp(number, &exponent);

            /* >= 2^52, the doubles are integral values; keep the number.
               Adding 0.5 will either be a no-op or add 1, which is undesirable. */
            if (exponent < 52)
                number = number > 0 || number < -0.5 ? ::op_floor(number + 0.5) : ::op_copysign(0, number);
        }
#endif

        return_value->SetNumber(number);
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::floor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        return_value->SetNumber(::op_floor(argv[0].GetNumAsDouble()));
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::ceil(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        return_value->SetNumber(::op_ceil(argv[0].GetNumAsDouble()));
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::exp(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        return_value->SetNumber(::op_exp(argv[0].GetNumAsDouble()));
    }
    else
        return_value->SetNan();

    return TRUE;
}

/* static */ BOOL
ES_MathBuiltins::log(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc >= 1)
    {
        if (!argv[0].ToNumber(context))
            return FALSE;

        return_value->SetNumber(::op_log(argv[0].GetNumAsDouble()));
    }
    else
        return_value->SetNan();

    return TRUE;
}

#ifndef _STANDALONE

class ES_SuspendedRandom
    : public ES_SuspendedCall
{
public:
    ES_SuspendedRandom(ES_Execution_Context *context)
    {
        context->SuspendedCall(this);
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        result = g_libcrypto_random_generator->GetDouble();
    }

    double result;

    operator double () { return result; }
};

#endif // !_STANDALONE

/* static */ BOOL
ES_MathBuiltins::random(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    double ret;
	/* The spec does not say how many bits of random number we should
	   generate.  With MSVC++, rand() generates only 15 bits (RAND_MAX
	   is 0x7FFF), which is a travesty.

	   To fix this, I would like to include the Mersenne Twister RNG
	   (http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/emt.html or
	   http://en.wikipedia.org/wiki/Mersenne_twister) but it is LGPL,
	   and it is now four days before Opera 7.0 code freeze, and I do
	   not want to deal with the hassle right now.  So I will instead
	   hack around it and try to provide 32 bits of random data, even
	   though the randomness properties may not be exciting (given that
	   the stream delivered from rand() has an unknown, and possibly short,
	   period).

	   (...years later...)

	   Apparently we *do* use the Mersenne Twister in the stdlib module.
	   */
#ifndef _STANDALONE
	/*
	   (...1 1/2 years later...)

	   The libcrypto random generator is cryptographically secure, in
	   addition to having very good random properties.
		*/
    ret = ES_SuspendedRandom(context);

#else // _STANDALONE

#ifdef RAND_MAX_IS_64_BIT
	UINT64 r = op_rand64();

	ret = double(r)/18446744073709551616.0;
#elif defined USE_RANDOM_NOT_RAND
	ret = random();		// Must not return 1.0
#else
	unsigned int r=0;

# if   RAND_MAX == 0xFFFFFFFFU
	r = op_rand();
# elif RAND_MAX >= 0xFFFFFFF
	r = (op_rand() << 4) ^ op_rand();
# elif RAND_MAX >= 0xFFFFFF
	r = (op_rand() << 8) ^ op_rand();
# elif RAND_MAX >= 0xFFFFF
	r = (op_rand() << 12) ^ op_rand();
# elif RAND_MAX >= 0xFFFF
	r = (op_rand() << 16) ^ op_rand();
# elif RAND_MAX >= 0xFFF
	r = (((op_rand() << 12) ^ op_rand()) << 8) ^ op_rand();
# elif RAND_MAX >= 0xFF
	r = (((((op_rand() << 8) ^ op_rand()) << 8) ^ op_rand()) << 8) ^ op_rand();
# else
#   error "Man, your compiler sucks rocks."
# endif

	// This makes sense on systems with more than 32 bits of int
	r = r & 0xFFFFFFFFU;

    ret = double(r)/4294967296.0;	// Must not return 1.0
#endif
#endif // _STANDALONE

    return_value->SetDouble(ret);
    return TRUE;
}

/* static */ void
ES_MathBuiltins::PopulateMath(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_MathBuiltins);

    ES_Function *fn_abs = MAKE_BUILTIN_NO_PROTOTYPE(1, abs);
    ES_Function *fn_ceil = MAKE_BUILTIN_NO_PROTOTYPE(1, ceil);
    ES_Function *fn_cos = MAKE_BUILTIN_NO_PROTOTYPE(1, cos);
    ES_Function *fn_floor = MAKE_BUILTIN_NO_PROTOTYPE(1, floor);
    ES_Function *fn_log = MAKE_BUILTIN_NO_PROTOTYPE(1, log);
    ES_Function *fn_pow = MAKE_BUILTIN_NO_PROTOTYPE(2, pow);
    ES_Function *fn_sin = MAKE_BUILTIN_NO_PROTOTYPE(1, sin);
    ES_Function *fn_sqrt = MAKE_BUILTIN_NO_PROTOTYPE(1, sqrt);
    ES_Function *fn_tan = MAKE_BUILTIN_NO_PROTOTYPE(1, tan);

#ifdef ES_NATIVE_SUPPORT
    fn_abs->SetFunctionID(ES_BUILTIN_FN_abs);
    fn_ceil->SetFunctionID(ES_BUILTIN_FN_ceil);
    fn_cos->SetFunctionID(ES_BUILTIN_FN_cos);
    fn_floor->SetFunctionID(ES_BUILTIN_FN_floor);
    fn_log->SetFunctionID(ES_BUILTIN_FN_log);
    fn_pow->SetFunctionID(ES_BUILTIN_FN_pow);
    fn_sin->SetFunctionID(ES_BUILTIN_FN_sin);
    fn_sqrt->SetFunctionID(ES_BUILTIN_FN_sqrt);
    fn_tan->SetFunctionID(ES_BUILTIN_FN_tan);
#endif // ES_NATIVE_SUPPORT

    APPEND_PROPERTY(ES_MathBuiltins, abs,       fn_abs);
    APPEND_PROPERTY(ES_MathBuiltins, acos,      MAKE_BUILTIN_NO_PROTOTYPE(1, acos));
    APPEND_PROPERTY(ES_MathBuiltins, asin,      MAKE_BUILTIN_NO_PROTOTYPE(1, asin));
    APPEND_PROPERTY(ES_MathBuiltins, atan,      MAKE_BUILTIN_NO_PROTOTYPE(1, atan));
    APPEND_PROPERTY(ES_MathBuiltins, atan2,     MAKE_BUILTIN_NO_PROTOTYPE(2, atan2));
    APPEND_PROPERTY(ES_MathBuiltins, ceil,      fn_ceil);
    APPEND_PROPERTY(ES_MathBuiltins, cos,       fn_cos);
    APPEND_PROPERTY(ES_MathBuiltins, exp,       MAKE_BUILTIN_NO_PROTOTYPE(1, exp));
    APPEND_PROPERTY(ES_MathBuiltins, floor,     fn_floor);
    APPEND_PROPERTY(ES_MathBuiltins, log,       fn_log);
    APPEND_PROPERTY(ES_MathBuiltins, max,       MAKE_BUILTIN_NO_PROTOTYPE(2, max));
    APPEND_PROPERTY(ES_MathBuiltins, min,       MAKE_BUILTIN_NO_PROTOTYPE(2, min));
    APPEND_PROPERTY(ES_MathBuiltins, pow,       fn_pow);
    APPEND_PROPERTY(ES_MathBuiltins, random,    MAKE_BUILTIN_NO_PROTOTYPE(0, random));
    APPEND_PROPERTY(ES_MathBuiltins, round,     MAKE_BUILTIN_NO_PROTOTYPE(1, round));
    APPEND_PROPERTY(ES_MathBuiltins, sin,       fn_sin);
    APPEND_PROPERTY(ES_MathBuiltins, sqrt,      fn_sqrt);
    APPEND_PROPERTY(ES_MathBuiltins, tan,       fn_tan);
    APPEND_PROPERTY(ES_MathBuiltins, E,         M_E);
    APPEND_PROPERTY(ES_MathBuiltins, LN10,      M_LN10);
    APPEND_PROPERTY(ES_MathBuiltins, LN2,       M_LN2);
    APPEND_PROPERTY(ES_MathBuiltins, LOG2E,     M_LOG2E);
    APPEND_PROPERTY(ES_MathBuiltins, LOG10E,    M_LOG10E);
    APPEND_PROPERTY(ES_MathBuiltins, PI,        M_PI);
    APPEND_PROPERTY(ES_MathBuiltins, SQRT1_2,   M_SQRT1_2);
    APPEND_PROPERTY(ES_MathBuiltins, SQRT2,     M_SQRT2);

    ASSERT_OBJECT_COUNT(ES_MathBuiltins);
}

/* static */ void
ES_MathBuiltins::PopulateMathClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_MathBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_MathBuiltins, abs,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, acos,     DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, asin,     DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, atan,     DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, atan2,    DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, ceil,     DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, cos,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, exp,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, floor,    DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, log,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, max,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, min,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, pow,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, random,   DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, round,    DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, sin,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, sqrt,     DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, tan,      DE,             ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_MathBuiltins, E,        RO | DE | DD,   ES_STORAGE_DOUBLE);
    DECLARE_PROPERTY(ES_MathBuiltins, LN10,     RO | DE | DD,   ES_STORAGE_DOUBLE);
    DECLARE_PROPERTY(ES_MathBuiltins, LN2,      RO | DE | DD,   ES_STORAGE_DOUBLE);
    DECLARE_PROPERTY(ES_MathBuiltins, LOG2E,    RO | DE | DD,   ES_STORAGE_DOUBLE);
    DECLARE_PROPERTY(ES_MathBuiltins, LOG10E,   RO | DE | DD,   ES_STORAGE_DOUBLE);
    DECLARE_PROPERTY(ES_MathBuiltins, PI,       RO | DE | DD,   ES_STORAGE_DOUBLE);
    DECLARE_PROPERTY(ES_MathBuiltins, SQRT1_2,  RO | DE | DD,   ES_STORAGE_DOUBLE);
    DECLARE_PROPERTY(ES_MathBuiltins, SQRT2,    RO | DE | DD,   ES_STORAGE_DOUBLE);
}
