/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/builtins/es_string_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_string_object.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"

/* static */ ES_String_Object *
ES_String_Object::Make(ES_Context *context, ES_Global_Object *global_object, JString *value)
{
    ES_String_Object *string;

    GC_ALLOCATE(context, string, ES_String_Object, (string, global_object->GetStringClass(), value));
    ES_CollectorLock gclock(context);

    string->AllocateProperties(context);

    ES_Value_Internal length;
    length.SetUInt32(value->length);
    string->PutCachedAtIndex(ES_PropertyIndex(0), length);

    return string;
}

/* static */ ES_String_Object *
ES_String_Object::MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance)
{
    ES_String_Object *prototype_object;
    JString **idents = context->rt_data->idents;

    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "String", idents[ESID_String], ES_StringBuiltins::ES_StringBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);
    ES_StringBuiltins::PopulatePrototypeClass(context, prototype_class);

    GC_ALLOCATE(context, prototype_object, ES_String_Object, (prototype_object, prototype_class, context->rt_data->strings[STRING_empty]));
    prototype_class->AddInstance(context, prototype_object);

    prototype_object->AllocateProperties(context);
    ES_StringBuiltins::PopulatePrototype(context, global_object, prototype_object);

    ES_Class_Node *sub_object_class = ES_Class::MakeRoot(context, prototype_object, "String", idents[ESID_String], TRUE);
    prototype_object->SetSubObjectClass(context, sub_object_class);

    instance = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_length], ES_Property_Info(RO|DE|DD), ES_STORAGE_INT32);

    return prototype_object;
}

/* static */ BOOL
ES_String_Object::GetSubString(ES_Context *context, ES_String_Object *object, unsigned index, unsigned length, JString *&result)
{
    if (index + length <= Length(object->GetValue()) && index + length > index)
    {
        result = JString::Make(context, object->GetValue(), index, length);
        return TRUE;
    }
    else
        return FALSE;
}
