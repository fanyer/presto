/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/builtins/es_date_builtins.h"
#include "modules/ecmascript/carakan/src/object/es_date_object.h"
#include "modules/ecmascript/carakan/src/object/es_global_object.h"
#include "modules/ecmascript/carakan/src/vm/es_suspended_call.h"

class ES_SuspendedGetLocalTime
    : public ES_SuspendedCall
{
public:
    ES_SuspendedGetLocalTime(ES_Execution_Context *context, double u)
        : result(-1)
        , utc(u)
    {
        context->SuspendedCall(this);
    }

    double result;
private:
    double utc;

    virtual void DoCall(ES_Execution_Context *context)
    {
        result = OpDate::LocalTime(utc);
    }
};

/* static */ ES_Date_Object *
ES_Date_Object::Make(ES_Context *context, ES_Global_Object *global_object, double utc)
{
    ES_Date_Object *date;

    GC_ALLOCATE(context, date, ES_Date_Object, (date, global_object->GetDateClass(), utc));
    ES_CollectorLock gclock(context);

    date->AllocateProperties(context);

    return date;
}

/* static */ ES_Date_Object *
ES_Date_Object::Make(ES_Context *context, ES_Global_Object *global_object, double utc, double local)
{
    ES_Date_Object *date;

    GC_ALLOCATE(context, date, ES_Date_Object, (date, global_object->GetDateClass(), utc, local));
    ES_CollectorLock gclock(context);

    date->AllocateProperties(context);

    return date;
}

/* static */ ES_Date_Object *
ES_Date_Object::MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance)
{
    JString **idents = context->rt_data->idents;

    ES_Date_Object *prototype_object;
    ES_Class_Singleton *prototype_class = ES_Class::MakeSingleton(context, global_object->GetObjectPrototype(), "Date", idents[ESID_Date], ES_DateBuiltins::ES_DateBuiltinsCount);
    prototype_class->Prototype()->AddInstance(context, prototype_class, TRUE);

    ES_CollectorLock gclock(context);
    ES_DateBuiltins::PopulatePrototypeClass(context, prototype_class);

    GC_ALLOCATE(context, prototype_object, ES_Date_Object, (prototype_object, prototype_class, op_nan(0)));
    prototype_class->AddInstance(context, prototype_object);

    prototype_object->AllocateProperties(context);
    ES_DateBuiltins::PopulatePrototype(context, global_object, prototype_object);

    instance = ES_Class::MakeRoot(context, prototype_object, "Date", idents[ESID_Date], TRUE);
    prototype_object->SetSubObjectClass(context, instance);

    return prototype_object;
}

/* static */ double
ES_Date_Object::GetLocalTime(ES_Execution_Context *context, double utc)
{
    return ES_SuspendedGetLocalTime(context, utc).result;
}
