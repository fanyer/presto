/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_DATE_BUILTINS_H
#define ES_DATE_BUILTINS_H

class ES_DateBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'Date()' */
    static BOOL ES_CALLING_CONVENTION constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'new Date()' */

    static BOOL ES_CALLING_CONVENTION valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toDateString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toTimeString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toLocaleString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toLocaleDateString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toLocaleTimeString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getTime(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getFullYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUTCFullYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getMonth(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUTCMonth(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getDate(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUTCDate(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getDay(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUTCDay(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getHours(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUTCHours(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getMinutes(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUTCMinutes(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getSeconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUTCSeconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getMilliseconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getUTCMilliseconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION getTimezoneOffset(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setTime(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setMilliseconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUTCMilliseconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setSeconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUTCSeconds(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setMinutes(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUTCMinutes(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setHours(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUTCHours(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setDate(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUTCDate(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setMonth(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUTCMonth(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setFullYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION setUTCFullYear(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toUTCString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toISOString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toJSON(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION parse(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION UTC(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION now(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class);
    static void PopulateConstructor(ES_Context *context, ES_Global_Object *global_object, ES_Object *constructor);

    enum
    {
        ES_DateBuiltins_constructor = 0,
        ES_DateBuiltins_valueOf,
        ES_DateBuiltins_toString,
        ES_DateBuiltins_toDateString,
        ES_DateBuiltins_toTimeString,
        ES_DateBuiltins_toLocaleString,
        ES_DateBuiltins_toLocaleDateString,
        ES_DateBuiltins_toLocaleTimeString,
        ES_DateBuiltins_getTime,
        ES_DateBuiltins_getYear,
        ES_DateBuiltins_getFullYear,
        ES_DateBuiltins_getUTCFullYear,
        ES_DateBuiltins_getMonth,
        ES_DateBuiltins_getUTCMonth,
        ES_DateBuiltins_getDate,
        ES_DateBuiltins_getUTCDate,
        ES_DateBuiltins_getDay,
        ES_DateBuiltins_getUTCDay,
        ES_DateBuiltins_getHours,
        ES_DateBuiltins_getUTCHours,
        ES_DateBuiltins_getMinutes,
        ES_DateBuiltins_getUTCMinutes,
        ES_DateBuiltins_getSeconds,
        ES_DateBuiltins_getUTCSeconds,
        ES_DateBuiltins_getMilliseconds,
        ES_DateBuiltins_getUTCMilliseconds,
        ES_DateBuiltins_getTimezoneOffset,
        ES_DateBuiltins_setTime,
        ES_DateBuiltins_setMilliseconds,
        ES_DateBuiltins_setUTCMilliseconds,
        ES_DateBuiltins_setSeconds,
        ES_DateBuiltins_setUTCSeconds,
        ES_DateBuiltins_setMinutes,
        ES_DateBuiltins_setUTCMinutes,
        ES_DateBuiltins_setHours,
        ES_DateBuiltins_setUTCHours,
        ES_DateBuiltins_setDate,
        ES_DateBuiltins_setUTCDate,
        ES_DateBuiltins_setMonth,
        ES_DateBuiltins_setUTCMonth,
        ES_DateBuiltins_setYear,
        ES_DateBuiltins_setFullYear,
        ES_DateBuiltins_setUTCFullYear,
        ES_DateBuiltins_toUTCString,
        ES_DateBuiltins_toGMTString,
        ES_DateBuiltins_toISOString,
        ES_DateBuiltins_toJSON,

        ES_DateBuiltinsCount,

        ES_DateBuiltinsCount_LAST_PROPERTY = ES_DateBuiltins_toJSON
    };

private:
    static JString* TimeToString(ES_Context *context, double time);
    static JString* TimeToDateString(ES_Context *context, double time);
    static JString* TimeToTimeString(ES_Context *context, double time);
    static JString* TimeToLocaleString(ES_Context *context, double time);
    static JString* TimeToLocaleDateString(ES_Context *context, double time);
    static JString* TimeToLocaleTimeString(ES_Context *context, double time);
    static JString* TimeToUTCString(ES_Context *context, double time);
    static JString* TimeToISOString(ES_Context *context, double time);
};

#endif // ES_DATE_BUILTINS_H
