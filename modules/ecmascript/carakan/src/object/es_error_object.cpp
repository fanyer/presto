/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/builtins/es_error_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_error_object.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"

/* static */ ES_Error *
ES_Error::Make(ES_Context *context, ES_Global_Object *global_object, ES_Class *prototype_class, BOOL via_constructor, BOOL is_proto)
{
    ES_Error *error;

    GC_ALLOCATE(context, error, ES_Error, (error, prototype_class));

    ES_CollectorLock gclock(context);

    error->AllocateProperties(context);
    error->via_constructor = !!via_constructor;

    if (!is_proto)
    {
        error->PutCachedAtIndex(ES_PropertyIndex(PROP_stacktrace), ES_Value_Internal(global_object->GetSpecialStacktrace()));
        error->PutCachedAtIndex(ES_PropertyIndex(PROP_stack), ES_Value_Internal(global_object->GetSpecialStack()));
    }

    if (via_constructor)
        context->GetExecutionContext()->CaptureStackTrace(error);

    return error;
}

/* static */ ES_Object *
ES_Error::MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance)
{
    JString **idents = context->rt_data->idents;
    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "Error", idents[ESID_Error], ES_ErrorBuiltins::ES_CommonErrorBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);
    ES_ErrorBuiltins::PopulatePrototypeClass(context, prototype_class);

    ES_Object *prototype_object = ES_Error::Make(context, global_object, prototype_class, FALSE, TRUE);
    prototype_class->AddInstance(context, prototype_object);
    ES_ErrorBuiltins::PopulatePrototype(context, global_object, prototype_object);

    ES_Class *sub_object_class = ES_Class::MakeRoot(context, prototype_object, "Error", idents[ESID_Error], TRUE);
    prototype_object->SetSubObjectClass(context, sub_object_class);

    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_stacktrace], ES_Property_Info(DE|SP), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_stack], ES_Property_Info(DE|SP), ES_STORAGE_WHATEVER);
    instance = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_message], ES_Property_Info(), ES_STORAGE_WHATEVER);

    return prototype_object;
}

/* static */ ES_Object *
ES_Error::MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance, int id)
{
    JString **idents = context->rt_data->idents;
    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, global_object->GetErrorPrototype(), "Error", idents[ESID_Error], ES_ErrorBuiltins::ES_CommonErrorBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);
    ES_ErrorBuiltins::CommonPopulatePrototypeClass(context, prototype_class);

    ES_Object *prototype_object = ES_Error::Make(context, global_object, prototype_class, FALSE, TRUE);
    prototype_class->AddInstance(context, prototype_object);
    ES_ErrorBuiltins::CommonPopulatePrototype(context, prototype_object, idents[id], TRUE);

    ES_Class *sub_object_class = ES_Class::MakeRoot(context, prototype_object, "Error", idents[ESID_Error], TRUE);
    prototype_object->SetSubObjectClass(context, sub_object_class);

    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_stacktrace], ES_Property_Info(DE|SP), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_stack], ES_Property_Info(DE|SP), ES_STORAGE_WHATEVER);
    instance = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_message], ES_Property_Info(), ES_STORAGE_WHATEVER);

    return prototype_object;
}

/* static */ ES_Class *
ES_Error::MakeErrorInstanceClass(ES_Context *context, ES_Class *klass)
{
    /* CT-659 requires that NativeObject.prototype has a class such that
       if a native object is set with that prototype, the class and
       its properties won't be that object's. That means we cannot
       directly make use of the sub-object class of an Error prototype
       object any longer when constructing Error object instances, but
       need to extend that bare class first.

       That's my theory, anyway. Sounds too expensive, so there could be
       alternative solutions that work better. */
    JString **idents = context->rt_data->idents;
    ES_Class *sub_object_class = klass;

    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_stacktrace], ES_Property_Info(DE|SP), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_stack], ES_Property_Info(DE|SP), ES_STORAGE_WHATEVER);
    sub_object_class = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_message], ES_Property_Info(), ES_STORAGE_STRING);

    return sub_object_class;
}

/* static */ ES_Object *
ES_Error::MakeErrorPrototype(ES_Context *context, ES_Global_Object *global_object, ES_Object *error_prototype_prototype, const char *error_name)
{
    JString *error_name_string = context->rt_data->GetSharedString(error_name, op_strlen(error_name));

    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, error_prototype_prototype, error_name, error_name_string, ES_ErrorBuiltins::ES_CommonErrorBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);
    ES_ErrorBuiltins::CommonPopulatePrototypeClass(context, prototype_class);
    ES_Object *prototype_object = ES_Error::Make(context, global_object, prototype_class, FALSE, TRUE);
    prototype_class->AddInstance(context, prototype_object);

    ES_ErrorBuiltins::CommonPopulatePrototype(context, prototype_object, error_name_string, FALSE);

    ES_Class *sub_object_class = ES_Class::MakeRoot(context, prototype_object, error_name, error_name_string, TRUE);
    prototype_object->SetSubObjectClass(context, sub_object_class);

    return prototype_object;
}

class ES_SuspendedGetStackTraceString
    : public ES_SuspendedCall
{
public:
    ES_SuspendedGetStackTraceString(ES_Execution_Context *context, ES_Execution_Context *error_context, ES_Error *error, ES_Error::StackTraceFormat format, unsigned prefix_linebreaks)
        : context(context),
          error_context(error_context),
          error(error),
          format(format),
          prefix_linebreaks(prefix_linebreaks),
          result(NULL)
    {
    }

    virtual void DoCall(ES_Execution_Context *context)
    {
        JString *string;

        TRAPD(status,
              if (!context->GenerateStackTraceL(string, error_context, error, format, prefix_linebreaks))
                  string = context->rt_data->strings[STRING_empty];);

        if (OpStatus::IsSuccess(status))
            result = string;
    }

    ES_Execution_Context *context;
    ES_Execution_Context *error_context;
    ES_Error *error;
    ES_Error::StackTraceFormat format;
    unsigned prefix_linebreaks;
    JString *result;
};

JString *
ES_Error::GetStackTraceString(ES_Execution_Context *context, ES_Execution_Context *error_context, StackTraceFormat format, unsigned prefix_linebreaks)
{
    if (stacktrace_strings[format])
        return stacktrace_strings[format];
    else
    {
        ES_SuspendedGetStackTraceString call(context, error_context, this, format, prefix_linebreaks);

        context->SuspendedCall(&call);

        if (!call.result)
            context->AbortOutOfMemory();

        return stacktrace_strings[format] = call.result;
    }
}

/* static */ JString *
ES_Error::GetErrorString(ES_Execution_Context *context, ES_Object *error_object, BOOL allow_getters)
{
    ES_Value_Internal name, message;
    JString **idents = context->rt_data->idents, *empty = context->rt_data->strings[STRING_empty];
    ES_Object::PropertyDescriptor desc;

    if (!error_object->GetPropertyL(context, desc, idents[ESID_name], 0, TRUE, FALSE))
        return NULL;
    else if (ES_Object::IsDataDescriptor(desc))
        name = desc.value;

    if (!error_object->GetPropertyL(context, desc, idents[ESID_message], 0, TRUE, FALSE))
        return NULL;
    else if (ES_Object::IsDataDescriptor(desc))
        message = desc.value;

    if (name.IsUndefined())
        name.SetString(idents[ESID_Error]);
    if (!allow_getters && name.IsObject())
        name.SetString(empty);
    else if (!name.ToString(context))
        return NULL;

    if (message.IsUndefined())
        message.SetString(empty);
    else if (!allow_getters && name.IsObject())
        message.SetString(empty);
    else if (!message.ToString(context))
        return NULL;

    if (Length(name.GetString()) == 0)
        return message.GetString();
    else if (Length(message.GetString()) == 0)
        return name.GetString();

    JString *string = Share(context, name.GetString());
    ES_CollectorLock gclock(context);

    Append(context, string, ": ");
    Append(context, string, message.GetString());

    return string;
}

