/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright 2001 - 2011 Opera Software AS.  All rights reserved.
 *
 * A simple expandable char buffer.
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/util/es_tempbuf.h"

const int alignment = 16;			// Must be power of 2

#define CHECK_INVARIANTS() \
    do { \
        OP_ASSERT( (storage == 0) == (allocated == 0) ); \
    } while (0)

ES_TempBuffer::ES_TempBuffer()
{
    cached_length = 0;
    storage = 0;
    allocated = 0;
    CHECK_INVARIANTS();
}

ES_TempBuffer::~ES_TempBuffer()
{
    OP_DELETEA(storage);
}

OP_STATUS
ES_TempBuffer::EnsureConstructed(size_t capacity)
{
    size_t nallocated;

    if (storage && allocated >= capacity)
        return OpStatus::OK;

    capacity = MAX(capacity, allocated * 2);

    nallocated = (capacity + (alignment - 1 )) & ~(alignment - 1);
    uni_char *nstorage = OP_NEWA(uni_char, nallocated);
    if (!nstorage)
        return OpStatus::ERR_NO_MEMORY;

    if (storage)
    {
        op_memcpy(nstorage, storage, allocated * sizeof(uni_char));
        OP_DELETEA(storage);
    }
    else
        nstorage[0] = 0;

    storage = nstorage;
    allocated = nallocated;

    CHECK_INVARIANTS();
    return OpStatus::OK;
}

void
ES_TempBuffer::FreeStorage()
{
    OP_DELETEA(storage);

    cached_length = 0;
    storage = 0;
    allocated = 0;

    CHECK_INVARIANTS();
}

OP_STATUS
ES_TempBuffer::Expand(size_t capacity)
{
    CHECK_INVARIANTS();

    if (capacity == 0)
        capacity = 1;

    RETURN_IF_ERROR(EnsureConstructed(capacity));

    CHECK_INVARIANTS();
    return OpStatus::OK;
}

OP_STATUS
ES_TempBuffer::AppendSlow(int ch)
{
    CHECK_INVARIANTS();

    size_t used = Length() + 1;

    RETURN_IF_ERROR(EnsureConstructed(used + 1));

    uni_char *p;
    p=storage+used - 1;
    *p++ = ch;
    *p = 0;
    cached_length++;

    CHECK_INVARIANTS();
    return OpStatus::OK;
}

OP_STATUS
ES_TempBuffer::Append(const uni_char *s, size_t n)
{
    CHECK_INVARIANTS();

    size_t len = 0;
    const uni_char* q = s;
    while (len < n && *q)
    {
        ++q; ++len;
    }

    size_t used = Length() + 1;
    size_t orig_len = len;

    RETURN_IF_ERROR(EnsureConstructed(used + len));

    uni_char *p;
    for (p = storage + used - 1; len; len--)
        *p++ = *s++;
    *p = 0;
    cached_length += orig_len;

    CHECK_INVARIANTS();
    return OpStatus::OK;
}

OP_STATUS
ES_TempBuffer::AppendUnsignedLong(unsigned long int l)
{
    uni_char buf[DECIMALS_FOR_128_BITS + 1];
    if (uni_snprintf(buf, DECIMALS_FOR_128_BITS + 1, UNI_L("%u"), l) < 1)
        return OpStatus::ERR_NO_MEMORY;
    return ES_TempBuffer::Append(buf);
}

OP_STATUS
ES_TempBuffer::Append(const char *s, size_t n)
{
    CHECK_INVARIANTS();

    size_t len = 0;

    const char* q = s;
    while (len < n && *q)
    {
        ++q; ++len;
    }

    size_t used = Length() + 1;
    size_t orig_len = len;

    RETURN_IF_ERROR(EnsureConstructed(used + len));

    uni_char *p;
    for (p = storage + used - 1; len; len--)
        *p++ = static_cast<uni_char>(*s++);
    *p = 0;
    cached_length += orig_len;

    CHECK_INVARIANTS();
    return OpStatus::OK;
}


