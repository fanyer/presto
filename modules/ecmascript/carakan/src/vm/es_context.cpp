/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/vm/es_context.h"

ES_Context::ES_Context(ESRT_Data *rt_data, ES_Heap *heap, ES_Runtime *runtime/* = NULL*/, BOOL use_native_dispatcher/* = FALSE*/)
  : rt_data(rt_data),
    heap(heap),
    runtime(runtime),
    use_native_dispatcher(use_native_dispatcher),
    eval_status(ES_NORMAL)
{
    if (heap)
        heap->IncContexts();
}

/*virtual*/ ES_Context::~ES_Context()
{
    if (heap)
        heap->DecContexts();
}

void
ES_Context::AbortOutOfMemory()
{
    SetOutOfMemory();
    heap->Unlock();
    YieldImpl();
}

void
ES_Context::AbortError()
{
    SetError();
    heap->Unlock();
    YieldImpl();
}
