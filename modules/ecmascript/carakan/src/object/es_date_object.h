/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2004
 */

#ifndef ES_DATE_OBJECT_H
#define ES_DATE_OBJECT_H

#include "modules/ecmascript/carakan/src/vm/es_execution_context.h"
#include "modules/ecmascript/carakan/src/object/es_object.h"
#include "modules/stdlib/util/opdate.h"

class ES_Date_Object : public ES_Object
{
public:
    static ES_Date_Object *Make(ES_Context *context, ES_Global_Object *global_object, double utc);
    static ES_Date_Object *Make(ES_Context *context, ES_Global_Object *global_object, double utc, double local);

    static ES_Date_Object *MakePrototypeObject(ES_Context *context, ES_Global_Object *global_object, ES_Class *&instance);

    double GetValue() { return utc; }
    void SetValue(double v)
    {
        utc = v;
        local_valid = 0;
        utc_is_known_nan = op_isnan(v);
    }

    double GetLocalTime(ES_Execution_Context *context)
    {
        if (local_valid)
            return local;

        local = utc_is_known_nan ? utc : GetLocalTime(context, utc);
        local_valid = 1;
        return local;
    }

    BOOL IsInvalid() { return utc_is_known_nan; }
    void SetInvalid()
    {
        utc = op_nan(0);
        utc_is_known_nan = 1;
    }

private:
    static void Initialize(ES_Date_Object *date, ES_Class *klass, double utc)
    {
        ES_Object::Initialize(date, klass);
        date->ChangeGCTag(GCTAG_ES_Object_Date);
        date->utc = utc;
        date->local_valid = 0;
        date->utc_is_known_nan = op_isnan(utc);
    }

    static void Initialize(ES_Date_Object *date, ES_Class *klass, double utc, double local)
    {
        ES_Object::Initialize(date, klass);
        date->ChangeGCTag(GCTAG_ES_Object_Date);
        date->utc = utc;
        date->local_valid = 1;
        date->local = local;
        date->utc_is_known_nan = op_isnan(utc);
    }

    static double GetLocalTime(ES_Execution_Context *context, double utc);

    double utc;
    double local;
    unsigned local_valid:1;
    unsigned utc_is_known_nan:1;
};

#endif // ES_DATE_OBJECT_H
