/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_BOX_H
#define ES_BOX_H

class ES_Box : public ES_Boxed
{
public:
    static inline ES_Box *Make(ES_Context *context, unsigned size);
    /**< Create ES_Box object with a capacity of at least 'size' bytes.  The
         allocator might find a slightly larger free object than we need, and
         return it, so ES_Box::Size() on the returned object might be bigger
         than 'size'. */

    static ES_Box *MakeAligned(ES_Context *context, unsigned size, unsigned alignment);
    /**< Create ES_Box object with a capacity of at least 'size' bytes, and
         whatever extra capacity is required to ensure that a block of 'size'
         bytes with the requested alignment is contained within.

         UnboxAligned() must be used to actually retrieve a pointer to the
         aligned block of 'size' bytes; Unbox() will return the beginning of the
         allocated block, which will generally be an address that is a multiple
         of 8, plus 4. */

    static ES_Box *MakeStrict(ES_Context *context, unsigned size);
    /**< Create ES_Box object with a capacity of exactly 'size' bytes.  The
         expression 'sizeof(ES_Box) + size' must be a multiple of
         ES_LIM_ARENA_ALIGN, since otherwise the request can't possibly be
         satisfied.  */

    char *Unbox() { return reinterpret_cast<char*>(this) + sizeof(ES_Box); }
    inline char *UnboxAligned(unsigned alignment);
    inline unsigned Size() const { return ObjectSize(this) - sizeof(ES_Box); }

    static void Initialize(ES_Box *box);
};

#endif // ES_BOX_H
