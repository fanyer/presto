/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_PROPERTY_H
#define ES_PROPERTY_H

#include "modules/ecmascript/carakan/src/kernel/es_boxed.h"

class ES_Properties : public ES_Boxed
{
public:
    static ES_Properties *Make(ES_Context *context, unsigned size, unsigned used, ES_Object *ideal, unsigned &serial);

    BOOL GetLocation(unsigned index, ES_Value_Internal *&location);
    void Get(unsigned index, ES_Value_Internal &value);
    void Put(unsigned index, const ES_Value_Internal &value);
    ES_Properties *AppendValueL(ES_Context *context, const ES_Value_Internal &value, unsigned &index, unsigned serial, ES_Object *ideal);
    void Delete(unsigned index);

    inline unsigned GetCount() { return used; }

    void GetSerialNr(unsigned index, unsigned &serial);

    unsigned capacity, used;

public:
    ES_Value_Internal slots[1];

private:
#ifdef ES_VALUES_AS_NANS
    unsigned *GetSerials() { return reinterpret_cast<unsigned *>(reinterpret_cast<char *>(slots) + capacity*sizeof(ES_Value_Internal)); }
#endif // ES_VALUES_AS_NANS
    void SetSerialNr(unsigned index, unsigned serial);
    /**< Record the insertion order of the property, relative the most recently
         inserted indexed property.
    */

    static inline void Initialize(ES_Properties *properties, unsigned size, unsigned used, unsigned &serial);

    friend class ESMM;
    friend class ES_Object;
};

#include "modules/ecmascript/carakan/src/object/es_property_inlines.h"

#endif // ES_PROPERTIES_H
