/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/kernel/es_collector.h"

/* static */ ES_Box *
ES_Box::MakeAligned(ES_Context *context, unsigned size, unsigned alignment)
{
    OP_ASSERT((size & (alignment - 1)) == 0);

    /* This implementation assumes that the ES_Box is allocated at an
       ES_LIM_ARENA_ALIGN byte boundary, that its payload is offset by 4, and
       that the total size is a multiple of ES_LIM_ARENA_ALIGN.

       In that case, given the assumption that the requested size is a multiple
       of the requested alignment, and that ES_LIM_ARENA_ALIGN is at least 8, an
       alignment less than or equal to ES_LIM_ARENA_ALIGN will always be met; an
       alignment of 4 or less automatically since that's less than
       ES_LIM_ARENA_ALIGN, and 8 because if size is a multiple of 8, 4 extra
       bytes will always be allocated, and that's all the extra that is
       needed.

       For alignments above ES_LIM_ARENA_ALIGN we add space for an extra object
       minus the ES_LIM_ARENA_ALIGN bytes we're guaranteed to be aligned at. */

    if (alignment > ES_LIM_ARENA_ALIGN)
        size += alignment - ES_LIM_ARENA_ALIGN;

    return ES_Box::Make(context, size);
}

/* static */ ES_Box *
ES_Box::MakeStrict(ES_Context *context, unsigned size)
{
    /* We can't request a size that is exactly an odd multiple of four, since
       the allocator will never allocate something like that. */
    OP_ASSERT(ES_IS_SIZE_PAGE_ALIGNED(sizeof(ES_Box) + size));

    ES_Box *box;
    GC_ALLOCATE_WITH_EXTRA(context, box, size, ES_Box, (box));

    if (box->Size() != size)
    {
        ES_CollectorLock gclock(context);
        ES_Free *discarded = NULL;

        do
        {
            if (ObjectSize(box) > sizeof(ES_Free))
            {
                ES_Free *discard = static_cast<ES_Free *>(static_cast<ES_Boxed *>(box));
                discard->next = discarded;
                discarded = discard;
            }

            GC_ALLOCATE_WITH_EXTRA(context, box, size, ES_Box, (box));
        }
        while (box->Size() != size);

        while (discarded)
        {
            ES_Free *next = discarded->next;
            context->heap->Free(discarded);
            discarded = next;
        }
    }

    return box;
}
