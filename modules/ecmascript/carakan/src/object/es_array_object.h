/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#ifndef ES_ARRAY_OBJECT_H
#define ES_ARRAY_OBJECT_H

#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/ecmascript/carakan/src/object/es_indexed.h"

class ES_Array
//: public ES_Object
{
public:
    static ES_Object *Make(ES_Context *context, ES_Global_Object *global_object, unsigned capacity = 0, unsigned length = 0);

    static ES_Object *MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance);

    static void SetLength(ES_Context *context, ES_Object *array, const ES_Value_Internal &length);
};

#endif // ES_ARRAY_OBJECT_H
