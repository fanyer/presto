/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_REGEXP_BUILTINS_H
#define ES_REGEXP_BUILTINS_H

#include "modules/regexp/include/regexp_advanced_api.h"

class ES_RegExpBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'RegExp()' */
    static BOOL ES_CALLING_CONVENTION constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'new RegExp()' */

    static BOOL ES_CALLING_CONVENTION exec(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION test(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION compile(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_klass);

    enum
    {
        ES_RegExpBuiltins_lastIndex = 0,
        ES_RegExpBuiltins_source,
        ES_RegExpBuiltins_global,
        ES_RegExpBuiltins_ignoreCase,
        ES_RegExpBuiltins_multiline,

        ES_RegExpBuiltins_constructor,
        ES_RegExpBuiltins_toString,
        ES_RegExpBuiltins_test,
        ES_RegExpBuiltins_exec,
        ES_RegExpBuiltins_compile,

        ES_RegExpBuiltinsCount,
        ES_RegExpBuiltins_LAST_PROPERTY = ES_RegExpBuiltins_compile
    };
};

static inline BOOL
IsEmpty(const RegExpMatch &match)
{
    return match.length == 0;
}

static inline JString *
MatchSubString(ES_Context *context, JString *base, const RegExpMatch &match)
{
    if (IsEmpty(match))
        return context->rt_data->strings[STRING_empty];
    else
        return JString::Make(context, base, match.start, match.length);
}

static inline void
SetMatchValue(ES_Context *context, ES_Value_Internal &value, JString *base, const RegExpMatch &match)
{
    if (match.length != UINT_MAX)
        value.SetString(MatchSubString(context, base, match));
    else
        value.SetUndefined(TRUE);
}

#endif // ES_REGEXP_BUILTINS_H
