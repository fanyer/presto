/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_JSON_BUILTINS_H
#define ES_JSON_BUILTINS_H

#include "modules/ecmascript/carakan/src/kernel/es_string_appender.h"

class ES_JsonBuiltins
{
public:
    static BOOL ES_CALLING_CONVENTION parse(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);
    static BOOL ES_CALLING_CONVENTION stringify(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value);

    static void ParseL(ES_Context *context, ES_Value_Internal &value, const uni_char *string, unsigned length = UINT_MAX);
    static void StringifyL(ES_Context *context, const uni_char *&string, const ES_Value_Internal &value);

    static void PopulateJson(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype);
    static void PopulateJsonClass(ES_Context *context, ES_Class_Singleton *prototype_class);

    enum
    {
        ES_JsonBuiltins_stringify,
        ES_JsonBuiltins_parse,

        ES_JsonBuiltinsCount,
        ES_JsonBuiltins_LAST_PROPERTY = ES_JsonBuiltins_parse
    };

private:

    static BOOL ExportValue(
        ES_Context           *context,
        ES_Value_Internal    *subject_val,
        ES_Object            *replacer_obj,
        JString              *space_str,
        class JSON_Appender  &result);

    static BOOL WalkObject(
        ES_Execution_Context *context,
        ES_Object            *holder_obj,
        ES_Value_Internal    &key_val,
        ES_Function          *reviver,
        ES_Value_Internal    *result);

    static BOOL GetProperties(
        ES_Context           *context,
        ES_Object            *obj,
        ES_Object            *allprops,
        UINT32               *nallprops);
};

#define ES_JSON_PUSH_FRAME(CONTEXT, TOP_FRAME, SAVE_REG) \
do \
{ \
    ES_Boxed_Array *next_frame; \
    if (TOP_FRAME && TOP_FRAME->slots[IDX_NEXT_FRAME_ARR]) \
    { \
        next_frame = static_cast<ES_Boxed_Array *>(TOP_FRAME->slots[IDX_NEXT_FRAME_ARR]); \
        OP_ASSERT(TOP_FRAME == next_frame->slots[IDX_PREV_FRAME_ARR]); \
    } \
    else if (!TOP_FRAME && !SAVE_REG.IsUndefined()) \
    { \
        next_frame = static_cast<ES_Boxed_Array *>(SAVE_REG.GetBoxed()); \
    } \
    else \
    { \
        next_frame = ES_Boxed_Array::Make(CONTEXT, NSLOTS, NUSED); \
        next_frame->slots[IDX_PREV_FRAME_ARR] = TOP_FRAME; \
        next_frame->slots[IDX_NEXT_FRAME_ARR] = NULL; \
        if (TOP_FRAME) \
            TOP_FRAME->slots[IDX_NEXT_FRAME_ARR] = next_frame; \
        else \
            SAVE_REG.SetBoxed(next_frame); \
    } \
    TOP_FRAME = next_frame; \
} while(0)

#endif // ES_JSON_BUILTINS_H
