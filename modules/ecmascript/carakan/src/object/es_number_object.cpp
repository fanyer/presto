/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/builtins/es_number_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_number_object.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"

/* static */ ES_Number_Object *
ES_Number_Object::Make(ES_Context *context, ES_Global_Object *global_object, double value)
{
    GC_STACK_ANCHOR(context, global_object);
    ES_Number_Object *number;

    GC_ALLOCATE(context, number, ES_Number_Object, (number, global_object->GetNumberClass(), value));
    ES_CollectorLock gclock(context);

    number->AllocateProperties(context);

    return number;
}

/* static */ ES_Number_Object *
ES_Number_Object::MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance)
{
    GC_STACK_ANCHOR(context, global_object);
    JString **idents = context->rt_data->idents;

    ES_Number_Object *prototype_object;
    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "Number", idents[ESID_Number], ES_NumberBuiltins::ES_NumberBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);

    ES_CollectorLock gclock(context);

    ES_NumberBuiltins::PopulatePrototypeClass(context, prototype_class);

    GC_ALLOCATE(context, prototype_object, ES_Number_Object, (prototype_object, prototype_class, 0));
    prototype_class->AddInstance(context, prototype_object);

    prototype_object->AllocateProperties(context);

    ES_NumberBuiltins::PopulatePrototype(context, global_object, prototype_object);

    instance = ES_Class::MakeRoot(context, prototype_object, "Number", idents[ESID_Number], TRUE);
    prototype_object->SetSubObjectClass(context, instance);

    return prototype_object;
}
