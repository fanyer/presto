/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2012
 *
 * @author Andreas Farre
 */

#ifndef ES_ALLOCATION_CONTEXT_H
#define ES_ALLOCATION_CONTEXT_H

#include "modules/ecmascript/carakan/src/vm/es_context.h"

class ES_Allocation_Context
    : public ES_Context
{
public:
    ES_Allocation_Context(ES_Runtime *runtime);
    ES_Allocation_Context(ESRT_Data *rt_data);

    ~ES_Allocation_Context();

    void SetHeap(ES_Heap *new_heap);
    void Remove();
    void MoveToHeap(ES_Heap *new_heap);
private:

    ES_Allocation_Context *next;
};
#endif // ES_ALLOCATION_CONTEXT_H
