/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_GLOBAL_BUILTINS_H
#define ES_GLOBAL_BUILTINS_H

class ES_GlobalBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION parseInt(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION parseFloat(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION isNaN(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION isFinite(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION decodeURI(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION decodeURIComponent(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION encodeURI(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION encodeURIComponent(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION eval(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION escape(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION unescape(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static BOOL Eval(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, ES_Code *code, ES_CodeWord *instruction);

    static BOOL ES_CALLING_CONVENTION ThrowTypeError(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void PopulateGlobalObject(ES_Context *context, ES_Global_Object *global_object);

private:
    static JString *Decode(ES_Context *context, const uni_char *string, int length, const uni_char *reservedSet);
    static JString *Encode(ES_Context *context, const uni_char *string, int length, const uni_char *unescapedSet);
    static JString *Escape(ES_Context *context, const uni_char *string, int length, const uni_char *unescapedSet);
    static JString *Unescape(ES_Context *context, const uni_char *string, int length);
};

#endif // ES_GLOBAL_BUILTINS_H
