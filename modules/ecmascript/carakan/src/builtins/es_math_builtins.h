/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_MATH_BUILTINS_H
#define ES_MATH_BUILTINS_H

#undef max
#undef min

class ES_MathBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION abs(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION sqrt(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION pow(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION sin(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION asin(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION cos(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION acos(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION tan(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION atan(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION atan2(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION max(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION min(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION round(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION floor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION ceil(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION exp(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION log(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION random(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulateMath(ES_Context *context, ES_Global_Object *global_object, ES_Object *math);
    static void PopulateMathClass(ES_Context *context, ES_Class_Singleton *math_class);

    enum
    {
        ES_MathBuiltins_abs = 0,
        ES_MathBuiltins_acos,
        ES_MathBuiltins_asin,
        ES_MathBuiltins_atan,
        ES_MathBuiltins_atan2,
        ES_MathBuiltins_ceil,
        ES_MathBuiltins_cos,
        ES_MathBuiltins_exp,
        ES_MathBuiltins_floor,
        ES_MathBuiltins_log,
        ES_MathBuiltins_max,
        ES_MathBuiltins_min,
        ES_MathBuiltins_pow,
        ES_MathBuiltins_random,
        ES_MathBuiltins_round,
        ES_MathBuiltins_sin,
        ES_MathBuiltins_sqrt,
        ES_MathBuiltins_tan,
        ES_MathBuiltins_E,
        ES_MathBuiltins_LN10,
        ES_MathBuiltins_LN2,
        ES_MathBuiltins_LOG2E,
        ES_MathBuiltins_LOG10E,
        ES_MathBuiltins_PI,
        ES_MathBuiltins_SQRT1_2,
        ES_MathBuiltins_SQRT2,

        ES_MathBuiltinsCount,
        ES_MathBuiltins_LAST_PROPERTY = ES_MathBuiltins_SQRT2
    };

};

#endif // ES_MATH_BUILTINS_H
