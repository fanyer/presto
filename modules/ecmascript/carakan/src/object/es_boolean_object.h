/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#ifndef ES_BOOLEAN_OBJECT_H
#define ES_BOOLEAN_OBJECT_H

#include "modules/ecmascript/carakan/src/object/es_object.h"

class ES_Boolean_Object : public ES_Object
{
public:
    static ES_Boolean_Object *Make(ES_Context *context, ES_Global_Object *global_object, BOOL value);

    static ES_Boolean_Object *MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance);

    BOOL GetValue() { return value; }

private:
    static void Initialize(ES_Boolean_Object *boolean, ES_Class *klass, BOOL value)
    {
        ES_Object::Initialize(boolean, klass);
        boolean->ChangeGCTag(GCTAG_ES_Object_Boolean);
        boolean->value = value;
    }

    BOOL value;
};

#endif // ES_BOOLEAN_OBJECT_H
