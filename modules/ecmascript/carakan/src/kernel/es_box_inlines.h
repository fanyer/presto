/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_BOX_INLINES_H
#define ES_BOX_INLINES_H

inline ES_Box *
ES_Box::Make(ES_Context *context, unsigned size)
{
    ES_Box *box;
    GC_ALLOCATE_WITH_EXTRA(context, box, size, ES_Box, (box));
    return box;
}

inline char *
ES_Box::UnboxAligned(unsigned alignment)
{
    char *unaligned = Unbox();

    if ((reinterpret_cast<UINTPTR>(unaligned) & (alignment - 1)) != 0)
        unaligned += alignment - (reinterpret_cast<UINTPTR>(unaligned) & (alignment - 1));

    return unaligned;
}

inline void
ES_Box::Initialize(ES_Box *box)
{
    box->InitGCTag(GCTAG_ES_Box);
}

#endif // ES_BOX_INLINES_H
