/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/object/es_array_object.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/ecmascript/carakan/src/builtins/es_array_builtins.h"

/* static */ ES_Object *
ES_Array::Make(ES_Context *context, ES_Global_Object *global_object, unsigned capacity, unsigned length)
{
    ES_Object *array = ES_Object::MakeArray(context, global_object->GetArrayClass(), capacity);

    array->ChangeGCTag(GCTAG_ES_Object_Array);

    SetLength(context, array, length);

    return array;
}

/* static */ ES_Object *
ES_Array::MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance)
{
    JString **idents = context->rt_data->idents;
    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "Array", idents[ESID_Array], ES_ArrayBuiltins::ES_ArrayBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);

    ES_ArrayBuiltins::PopulatePrototypeClass(context, prototype_class);
    ES_Object *prototype_object = ES_Object::Make(context, prototype_class);
    prototype_class->AddInstance(context, prototype_object);
    prototype_object->ChangeGCTag(GCTAG_ES_Object_Array);
    prototype_object->AllocateProperties(context);

    ES_ArrayBuiltins::PopulatePrototype(context, global_object, prototype_object);

    SetLength(context, prototype_object, 0);

    ES_Class *sub_object_class = ES_Class::MakeRoot(context, prototype_object, "Array", idents[ESID_Array], TRUE);
    prototype_object->SetSubObjectClass(context, sub_object_class);
    instance = ES_Class::ExtendWithL(context, sub_object_class, idents[ESID_length], ES_Property_Info(DE|DD), ES_STORAGE_INT32);

    return prototype_object;
}
