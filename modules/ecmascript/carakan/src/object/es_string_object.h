/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#ifndef ES_STRING_OBJECT_H
#define ES_STRING_OBJECT_H

#include "modules/ecmascript/carakan/src/object/es_object.h"

class ES_String_Object : public ES_Object
{
public:
    static ES_String_Object *Make(ES_Context *context, ES_Global_Object *global_object, JString *value);

    static ES_String_Object *MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance);
    static BOOL GetSubString(ES_Context *context, ES_String_Object *this_object, unsigned index, unsigned length, JString *&result);
    JString *&GetValue() { return value; }

private:
    static void Initialize(ES_String_Object *string, ES_Class *klass, JString *value)
    {
        ES_Object::Initialize(string, klass);
        string->ChangeGCTag(GCTAG_ES_Object_String);
        string->value = value;
    }

    JString *value;
};

#endif // ES_STRING_OBJECT_H
