/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_STRING_BUILTINS_H
#define ES_STRING_BUILTINS_H

#undef small

class ES_StringBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'String()' */
    static BOOL ES_CALLING_CONVENTION constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    /**< 'new String()' */

    static BOOL ES_CALLING_CONVENTION toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toLowerCase(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toLocaleLowerCase(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toUpperCase(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION toLocaleUpperCase(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION charCodeAt(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION charAt(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION substring(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION substr(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION slice(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION concat(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION indexOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION lastIndexOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION localeCompare(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION replace(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION match(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION split(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION search(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION trim(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION anchor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION fontcolor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION fontsize(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION link(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION big(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION blink(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION bold(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION fixed(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION italics(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION small(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION strike(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION sub(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION sup(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL ES_CALLING_CONVENTION fromCharCode(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_klass);
    static void PopulateConstructor(ES_Context *context, ES_Global_Object *global_object, ES_Object *constructor);

    enum
    {
        ES_StringBuiltins_length,
        ES_StringBuiltins_constructor,
        ES_StringBuiltins_toString,
        ES_StringBuiltins_valueOf,
        ES_StringBuiltins_toLowerCase,
        ES_StringBuiltins_toLocaleLowerCase,
        ES_StringBuiltins_toUpperCase,
        ES_StringBuiltins_toLocaleUpperCase,
        ES_StringBuiltins_charCodeAt,
        ES_StringBuiltins_charAt,
        ES_StringBuiltins_substring,
        ES_StringBuiltins_substr,
        ES_StringBuiltins_slice,
        ES_StringBuiltins_concat,
        ES_StringBuiltins_indexOf,
        ES_StringBuiltins_lastIndexOf,
        ES_StringBuiltins_localeCompare,
        ES_StringBuiltins_replace,
        ES_StringBuiltins_match,
        ES_StringBuiltins_split,
        ES_StringBuiltins_search,
        ES_StringBuiltins_trim,
        ES_StringBuiltins_link,
        ES_StringBuiltins_anchor,
        ES_StringBuiltins_fontcolor,
        ES_StringBuiltins_fontsize,
        ES_StringBuiltins_big,
        ES_StringBuiltins_blink,
        ES_StringBuiltins_bold,
        ES_StringBuiltins_fixed,
        ES_StringBuiltins_italics,
        ES_StringBuiltins_small,
        ES_StringBuiltins_strike,
        ES_StringBuiltins_sub,
        ES_StringBuiltins_sup,

        ES_StringBuiltinsCount,
        ES_StringBuiltins_LAST_PROPERTY = ES_StringBuiltins_sup
    };

private:
    static BOOL ProcessThis(ES_Execution_Context *context, ES_Value_Internal &this_value);

    static BOOL htmlify(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, const char *tag_name);
    static BOOL htmlify(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, const char *tag_name, const char *attr_name);
};

#endif // ES_STRING_BUILTINS_H
