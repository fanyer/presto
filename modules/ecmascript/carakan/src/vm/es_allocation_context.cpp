/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2012
 *
 * @author Andreas Farre
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/vm/es_allocation_context.h"

ES_Allocation_Context::ES_Allocation_Context(ES_Runtime *runtime)
    : ES_Context(runtime->GetRuntimeData(), runtime->GetHeap(), runtime, FALSE),
      next(NULL)
{
    SetHeap(heap);
}

ES_Allocation_Context::ES_Allocation_Context(ESRT_Data *rt_data)
    : ES_Context(rt_data, rt_data->heap, NULL, FALSE),
      next(NULL)
{
    SetHeap(heap);
}

ES_Allocation_Context::~ES_Allocation_Context()
{
    if (heap)
        heap->DecContexts();
    Remove();
}

void
ES_Allocation_Context::SetHeap(ES_Heap *new_heap)
{
    OP_ASSERT(!new_heap->InCollector());
    next = new_heap->GetAllocationContext();
    new_heap->SetAllocationContext(this);
    heap = new_heap;
}

void
ES_Allocation_Context::Remove()
{
    heap->SetAllocationContext(next);
    next = NULL;
    heap = NULL;
}

void
ES_Allocation_Context::MoveToHeap(ES_Heap *new_heap)
{
    Remove();
    SetHeap(new_heap);
}
