/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_error_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_error_object.h"

/* static */ BOOL
ES_ErrorBuiltins::CommonErrorConstructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value, ES_Class *klass)
{
    ES_Object *object;
    ES_Global_Object *global_object = ES_GET_GLOBAL_OBJECT();
    if (argc != 0 && !argv[0].IsUndefined())
    {
        if (!argv[0].ToString(context))
            return FALSE;

        object = ES_Error::Make(context, global_object, klass, TRUE);
        object->PutCachedAtIndex(ES_PropertyIndex(ES_Error::PROP_message), argv[0].GetString());
    }
    else
        object = ES_Error::Make(context, global_object, klass->GetParent(), TRUE);

    return_value->SetObject(object);
    return TRUE;
}

/* static */ void
ES_ErrorBuiltins::CommonPopulatePrototype(ES_Context *context, ES_Object *prototype, JString *error_name, BOOL with_empty_message)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_CommonErrorBuiltins);

    APPEND_PROPERTY(ES_CommonErrorBuiltins, constructor, undefined);
    APPEND_PROPERTY(ES_CommonErrorBuiltins, name, error_name);
    if (with_empty_message)
        APPEND_PROPERTY(ES_CommonErrorBuiltins, message, context->rt_data->strings[STRING_empty]);
    else
        APPEND_PROPERTY(ES_CommonErrorBuiltins, message, undefined);
}

/* static */ void
ES_ErrorBuiltins::CommonPopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_CommonErrorBuiltinsCount);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_CommonErrorBuiltins, constructor, DE | CP, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_CommonErrorBuiltins, name,        DE | CP, ES_STORAGE_STRING);
    DECLARE_PROPERTY(ES_CommonErrorBuiltins, message,     DE | CP, ES_STORAGE_WHATEVER);
}

/* static */ void
ES_ErrorBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_ErrorBuiltins);

    CommonPopulatePrototype(context, prototype, context->rt_data->idents[ESID_Error], TRUE);

    APPEND_PROPERTY(ES_ErrorBuiltins, toString, MAKE_BUILTIN(0, toString));

    ASSERT_OBJECT_COUNT(ES_ErrorBuiltins);
}

/* static */ void
ES_ErrorBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
//    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_ErrorBuiltinsCount);

    CommonPopulatePrototypeClass(context, prototype_class);

    JString **idents = context->rt_data->idents;
    ES_Layout_Info layout;

    DECLARE_PROPERTY(ES_ErrorBuiltins, toString, DE, ES_STORAGE_OBJECT);
}

/* static */ BOOL
ES_ErrorBuiltins::toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    const ES_Value_Internal &this_value = argv[-2];

    if (this_value.IsObject())
    {
        ES_Object *object = this_value.GetObject(context);
        JString *error_string = ES_Error::GetErrorString(context, object, TRUE/*allow getter invocations*/);
        if (error_string)
        {
            return_value->SetString(error_string);
            return TRUE;
        }
        else
            return FALSE;
    }

    return_value->SetString(context->rt_data->strings[STRING_empty]);
    return TRUE;
}


/* static */ BOOL
ES_ErrorBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_Global_Object *global_object = ES_GET_GLOBAL_OBJECT();
    return CommonErrorConstructor(context, argc, argv, return_value, global_object->GetErrorClass());
}

/* static */ BOOL
ES_EvalErrorBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_Global_Object *global_object = ES_GET_GLOBAL_OBJECT();
    return CommonErrorConstructor(context, argc, argv, return_value, global_object->GetEvalErrorClass());
}

/* static */ BOOL
ES_RangeErrorBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_Global_Object *global_object = ES_GET_GLOBAL_OBJECT();
    return CommonErrorConstructor(context, argc, argv, return_value, global_object->GetRangeErrorClass());
}

/* static */ BOOL
ES_ReferenceErrorBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_Global_Object *global_object = ES_GET_GLOBAL_OBJECT();
    return CommonErrorConstructor(context, argc, argv, return_value, global_object->GetReferenceErrorClass());
}

/* static */ BOOL
ES_SyntaxErrorBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_Global_Object *global_object = ES_GET_GLOBAL_OBJECT();
    return CommonErrorConstructor(context, argc, argv, return_value, global_object->GetSyntaxErrorClass());
}

/* static */ BOOL
ES_TypeErrorBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_Global_Object *global_object = ES_GET_GLOBAL_OBJECT();
    return CommonErrorConstructor(context, argc, argv, return_value, global_object->GetTypeErrorClass());
}

/* static */ BOOL
ES_URIErrorBuiltins::constructor(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    ES_Global_Object *global_object = ES_GET_GLOBAL_OBJECT();
    return CommonErrorConstructor(context, argc, argv, return_value, global_object->GetURIErrorClass());
}
