/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/object/es_property.h"

ES_Properties *ES_Properties::Make(ES_Context *context, unsigned size, unsigned used, ES_Object *ideal, unsigned &serial)
{
    ES_Properties *properties;
    if (used > size)
        size = used;
#ifdef ES_VALUES_AS_NANS
    unsigned extra = size*sizeof(ES_Value_Internal) + size*sizeof(unsigned);
#else // ES_VALUES_AS_NANS
    unsigned extra = size*sizeof(ES_Value_Internal);
#endif // ES_VALUES_AS_NANS
    GC_ALLOCATE_WITH_EXTRA_ON_SAME_GEN(context, properties, extra, ES_Properties, (properties, size, used, serial), ideal);

    return properties;
}

BOOL ES_Properties::GetLocation(unsigned index, ES_Value_Internal *&location)
{
    if (index < used)
    {
        location = &slots[index];
        return TRUE;
    }
    return FALSE;
}

ES_Properties *ES_Properties::AppendValueL(ES_Context *context, const ES_Value_Internal &value, unsigned &index, unsigned serial, ES_Object *ideal)
{
    index = used++;
    if (index < capacity)
    {
        slots[index] = value;
        SetSerialNr(index, serial);
        return this;
    }
    else
    {
        --used;
        ES_CollectorLock gclock(context);
        unsigned dummy_serial = 0;
        ES_Properties *new_properties = ES_Properties::Make(context, used != 0 ? used * 2 : 4, 0, ideal, dummy_serial);
        new_properties->used = used + 1;
#ifdef ES_VALUES_AS_NANS
        op_memcpy(new_properties->GetSerials(), GetSerials(), capacity * sizeof(unsigned));
#endif
        op_memcpy(new_properties->slots, slots, capacity * sizeof(ES_Value_Internal));
        new_properties->slots[index] = value;
        new_properties->SetSerialNr(index, serial);
        return new_properties;
    }
}

void ES_Properties::Delete(unsigned index)
{
    --used;
    op_memmove(slots + index, slots + index + 1,  (used - index) * sizeof(ES_Value_Internal));
    slots[used].SetUndefined();

    return;
}

void ES_Properties::SetSerialNr(unsigned index, unsigned serial)
{
#ifdef ES_VALUES_AS_NANS
    GetSerials()[index] = serial;
#else // ES_VALUES_AS_NANS
    slots[index].SetExtra(serial);
#endif // ES_VALUES_AS_NANS
}

void ES_Properties::GetSerialNr(unsigned index, unsigned &serial)
{
#ifdef ES_VALUES_AS_NANS
    serial = GetSerials()[index];
#else // ES_VALUES_AS_NANS
    serial = slots[index].GetExtra();
#endif // ES_VALUES_AS_NANS
}
