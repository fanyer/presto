/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#ifndef ES_NUMBER_OBJECT_H
#define ES_NUMBER_OBJECT_H

#include "modules/ecmascript/carakan/src/object/es_object.h"

class ES_Number_Object : public ES_Object
{
public:
    static ES_Number_Object *Make(ES_Context *context, ES_Global_Object *global_object, double value);

    static ES_Number_Object *MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance);

    double GetValue() { return value; }

private:
    static void Initialize(ES_Number_Object *number, ES_Class *klass, double value)
    {
        ES_Object::Initialize(number, klass);
        number->ChangeGCTag(GCTAG_ES_Object_Number);
        number->value = value;
    }

    double value;
};

#endif // ES_NUMBER_OBJECT_H
