/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#include "modules/ecmascript/carakan/src/object/es_boolean_object.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/ecmascript/carakan/src/builtins/es_boolean_builtins.h"

/* static */ ES_Boolean_Object *
ES_Boolean_Object::Make(ES_Context *context, ES_Global_Object *global_object, BOOL value)
{
    ES_Boolean_Object *boolean;

    GC_ALLOCATE(context, boolean, ES_Boolean_Object, (boolean, global_object->GetBooleanClass(), value));
    ES_CollectorLock gclock(context);

    boolean->AllocateProperties(context);

    return boolean;
}

/* static */ ES_Boolean_Object *
ES_Boolean_Object::MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance)
{
    JString **idents = context->rt_data->idents;

    ES_Boolean_Object *prototype_object;
    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "Boolean", idents[ESID_Boolean], ES_BooleanBuiltins::ES_BooleanBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);

    ES_CollectorLock gclock(context);
    ES_BooleanBuiltins::PopulatePrototypeClass(context, prototype_class);

    GC_ALLOCATE(context, prototype_object, ES_Boolean_Object, (prototype_object, prototype_class, FALSE));
    prototype_class->AddInstance(context, prototype_object);

    prototype_object->AllocateProperties(context);
    ES_BooleanBuiltins::PopulatePrototype(context, global_object, prototype_object);

    instance = ES_Class::MakeRoot(context, prototype_object, "Boolean", idents[ESID_Boolean], TRUE);
    prototype_object->SetSubObjectClass(context, instance);

    return prototype_object;
}
