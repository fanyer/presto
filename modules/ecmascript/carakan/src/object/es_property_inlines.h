/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999-2008
 */

#ifndef ES_PROPERTY_INLINES_H
#define ES_PROPERTY_INLINES_H

/* static */ inline void
ES_Properties::Initialize(ES_Properties *self, unsigned size, unsigned used, unsigned &serial)
{
    self->InitGCTag(GCTAG_ES_Properties);

    self->capacity = size;
    self->used     = used;
    for (unsigned i = 0; i < used; ++i)
    {
        self->slots[i].SetUndefined();
        self->SetSerialNr(i, serial++);
    }
}

inline void
ES_Properties::Get(unsigned index, ES_Value_Internal &value)
{
    OP_ASSERT(index < used);
    value = slots[index];
}

inline void
ES_Properties::Put(unsigned index, const ES_Value_Internal &value)
{
    OP_ASSERT(index < used);
    slots[index] = value;
}

#endif // ES_PROPERTY_INLINES_H
