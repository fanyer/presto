/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_builtins.h"
#include "modules/ecmascript/carakan/src/builtins/es_boolean_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_boolean_object.h"

static BOOL
StrictProcessThis(BOOL &this_bool, const ES_Value_Internal &this_value)
{
    if (this_value.IsObject() && this_value.GetObject()->IsBooleanObject())
        this_bool = static_cast<ES_Boolean_Object *>(this_value.GetObject())->GetValue();
    else if (this_value.IsBoolean())
        this_bool = this_value.GetBoolean();
    else
        return FALSE;

    return TRUE;
}

/* static */ BOOL
ES_BooleanBuiltins::constructor_call(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    if (argc == 0)
    {
        return_value->SetFalse();
    }
    else
    {
        argv[0].ToBoolean();
        *return_value = argv[0];
    }
    return TRUE;
}

/* static */ BOOL
ES_BooleanBuiltins::constructor_construct(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    BOOL b;

    if (argc == 0)
        b = FALSE;
    else
    {
        argv[0].ToBoolean();
        b = argv[0].GetBoolean();
    }

    return_value->SetObject(ES_Boolean_Object::Make(context, ES_GET_GLOBAL_OBJECT(), b));

    return TRUE;
}

/* static */ BOOL
ES_BooleanBuiltins::toString(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    BOOL this_bool;

    if (!StrictProcessThis(this_bool, argv[-2]))
    {
        context->ThrowTypeError("Boolean.prototype.toString: this is not a Boolean object");
        return FALSE;
    }

    if (this_bool)
        return_value->SetTrue();
    else
        return_value->SetFalse();

    if (!return_value->ToString(context))
        return FALSE;
    return TRUE;
}

/* static */ BOOL
ES_BooleanBuiltins::valueOf(ES_Execution_Context *context, unsigned argc, ES_Value_Internal *argv, ES_Value_Internal *return_value)
{
    BOOL this_bool;

    if (!StrictProcessThis(this_bool, argv[-2]))
    {
        context->ThrowTypeError("Boolean.prototype.valueOf: this is not a Boolean object");
        return FALSE;
    }

    if (this_bool)
        return_value->SetTrue();
    else
        return_value->SetFalse();

    return TRUE;
}

/* static */ void
ES_BooleanBuiltins::PopulatePrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *prototype)
{
    ES_Value_Internal undefined;

    ASSERT_CLASS_SIZE(ES_BooleanBuiltins);

    APPEND_PROPERTY(ES_BooleanBuiltins, constructor, undefined);

    APPEND_PROPERTY(ES_BooleanBuiltins, toString,   MAKE_BUILTIN(0, toString));
    APPEND_PROPERTY(ES_BooleanBuiltins, valueOf,    MAKE_BUILTIN(0, valueOf));

    ASSERT_OBJECT_COUNT(ES_BooleanBuiltins);
}

/* static */ void
ES_BooleanBuiltins::PopulatePrototypeClass(ES_Context *context, ES_Class_Singleton *prototype_class)
{
    OP_ASSERT(prototype_class->GetPropertyTable()->Capacity() >= ES_BooleanBuiltinsCount);

    JString **idents = context->rt_data->idents;

    DECLARE_PROPERTY(ES_BooleanBuiltins, constructor,   DE, ES_STORAGE_WHATEVER);
    DECLARE_PROPERTY(ES_BooleanBuiltins, toString,      DE, ES_STORAGE_OBJECT);
    DECLARE_PROPERTY(ES_BooleanBuiltins, valueOf,       DE, ES_STORAGE_OBJECT);
}
