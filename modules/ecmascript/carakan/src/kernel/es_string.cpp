/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright 1999 - 2011 Opera Software ASA.  All rights reserved.
 *
 * @author Lars T Hansen
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

/** Maximum string length that can safely be used and passed to the allocator. */
#define ES_JSTRINGSTORAGE_MAX_LENGTH ((ES_LIM_OBJECT_SIZE - sizeof(JStringStorage) - ES_LIM_ARENA_ALIGN) / sizeof(uni_char))

/* JStringStorage does not know about 1, that must be handled in the client */

inline void
Initialize(JStringStorage* self, unsigned allocate, unsigned nchars, BOOL no_zero_tail)
{
    OP_ASSERT(allocate > nchars);

    self->InitGCTag(GCTAG_JStringStorage);
    self->allocated = allocate;
    self->length = nchars;

    if (allocate - nchars == 1 || no_zero_tail)
        self->storage[nchars] = 0;
    else
        op_memset(self->storage + nchars, 0, (allocate - nchars) * sizeof(uni_char));
}

/* static */ JStringStorage*
JStringStorage::Make(ES_Context *context, const uni_char* X, unsigned allocate, unsigned nchars, BOOL no_zero_tail)
{
    OP_ASSERT(nchars != UINT_MAX && allocate > nchars);
    if (allocate > ES_JSTRINGSTORAGE_MAX_LENGTH)
        context->AbortOutOfMemory();

    JStringStorage *s;

    unsigned extra = allocate ? sizeof(uni_char) * (allocate - 1) : 0;
    GC_ALLOCATE_WITH_EXTRA(context, s, extra, JStringStorage, (s, allocate, nchars, no_zero_tail));
    op_memcpy(s->storage, X, nchars*sizeof(uni_char));
#ifdef ES_DBG_GC
    GC_DDATA.demo_JStringStorage[ilog2(sizeof(JStringStorage)+extra)]++;
#endif
    return s;
}

/* static */ JStringStorage*
JStringStorage::Make(ES_Context *context, const char* X, unsigned allocate, unsigned nchars)
{
    OP_ASSERT(nchars != UINT_MAX && allocate > nchars);
    if (allocate > ES_JSTRINGSTORAGE_MAX_LENGTH)
        context->AbortOutOfMemory();

    JStringStorage *s;

    unsigned extra = allocate ? sizeof(uni_char) * (allocate - 1) : 0;
    GC_ALLOCATE_WITH_EXTRA(context, s, extra, JStringStorage, (s, allocate, nchars, FALSE));
    if (X)
    {
        op_memcpy(s->storage, X, nchars);
        make_doublebyte_in_place(s->storage, nchars-1);
    }
    OP_ASSERT(!extra || s->storage[nchars] == 0);
#ifdef ES_DBG_GC
    GC_DDATA.demo_JStringStorage[ilog2(sizeof(JStringStorage)+extra)]++;
#endif
    return s;
}

/* static */ JStringStorage *
JStringStorage::Make(ES_Context *context, JString *string)
{
    unsigned length = Length(string);

    // Reuse the string storage in string if doing so will not cost us much in extra memory kept alive.
    // We put the limit at 80% (4/5) here, but it is possible other values give better performance and
    // memory usage so that number is a candidate for future tweaking.
    if (!IsSegmented(string) && string->offset == 0 && (length + 1) * 5 >= string->value->allocated * 4)
        return string->value;
    else if (IsSegmented(string))
        return GetSegmented(string)->Realize(context, string->offset, length);
    else
        return JStringStorage::Make(context, Storage(context, string), length + 1, length, FALSE);
}

/* static */ JStringStorage *
JStringStorage::MakeStatic(ES_Context *context, unsigned nchars)
{
    unsigned bytesize = sizeof(JStringStorage) + nchars * sizeof(uni_char);

    ES_PageHeader *page = context->heap->GetPageAllocator()->AllocateLarge(context, GC_ALIGN(bytesize));

    if (!page)
        context->AbortOutOfMemory();

    JStringStorage *s = static_cast<JStringStorage *>(page->GetFirst());

    s->InitHeader(bytesize);
    s->InitGCTag(GCTAG_JStringStorage);
    s->SetAllocatedOnLargePage();

    s->allocated = nchars + 1;
    s->length = nchars;
    s->storage[nchars] = 0;

    return s;
}

/* static */ void
JStringStorage::FreeStatic(JStringStorage *storage)
{
    if (storage)
        ES_PageAllocator::FreeLarge(NULL, storage->GetPage());
}

inline void
Initialize(JStringSegmented *self, unsigned nsegments)
{
    self->InitGCTag(GCTAG_JStringSegmented);
    self->nsegments = self->nallocated = nsegments;
    op_memset(self->Bases(), 0, nsegments * sizeof(JStringStorage *));
}

/* static */ JStringSegmented *
JStringSegmented::Make(ES_Context *context, unsigned nsegments)
{
    OP_ASSERT(nsegments > 0);

    JStringSegmented *s;

    unsigned extra = nsegments * (sizeof(JStringStorage *) + 2 * sizeof(unsigned));
    GC_ALLOCATE_WITH_EXTRA(context, s, extra, JStringSegmented, (s, nsegments));
    return s;
}

JStringStorage *
JStringSegmented::Realize(ES_Context *context, unsigned soffset, unsigned slength, unsigned extra)
{
    JStringStorage *s = JStringStorage::Make(context, (const char *) NULL, slength + 1 + extra, slength);
    uni_char *ptr = s->storage;

    JSegmentIterator iter(this, soffset, slength);
    while (iter.Next())
    {
        op_memcpy(ptr, iter.GetBase()->storage + iter.GetOffset(), iter.GetLength() * sizeof(uni_char));
        ptr += iter.GetLength();
    }

    return s;
}

JStringStorage *
JStringSegmented::MaybeRealize(ES_Context *context, unsigned &soffset, unsigned slength)
{
    JSegmentIterator iter(this, soffset, slength);

    if (!iter.Next() && slength == 0)
    {
        soffset = 0;
        return JStringStorage::Make(context, UNI_L(""), 1, 0);
    }
    OP_ASSERT(iter.IsValid() && slength);

    JStringStorage *s;

    if (slength > iter.GetLength())
    {
        s = JStringStorage::Make(context, (const char *) NULL, slength + 1, slength);
        uni_char *ptr = s->storage;

        do
        {
            op_memcpy(ptr, iter.GetBase()->storage + iter.GetOffset(), iter.GetLength() * sizeof(uni_char));
            ptr += iter.GetLength();
        }
        while (iter.Next());

        soffset = 0;
    }
    else
    {
        s = iter.GetBase();
        soffset = iter.GetOffset();
    }

    return s;
}

JSegmentIterator::JSegmentIterator(JStringSegmented *segmented, unsigned soffset, unsigned slength)
    : index(0), current_base(NULL), current_offset(0), current_length(0)
{
    InitSegmented(segmented, soffset, slength);
}


JSegmentIterator::JSegmentIterator(const JString *string)
    : index(0), bases(NULL), offsets(NULL), lengths(NULL), soffset(0), slength(0)
{
    if (IsSegmented(string))
    {
        InitSegmented(GetSegmented(string), string->offset, Length(string));
        current_base = NULL;
        current_offset = 0;
        current_length = 0;
    }
    else
    {
        segmented = NULL;
        current_base = string->value;
        current_offset = string->offset;
        current_length = Length(string);
    }
}

JSegmentIterator::JSegmentIterator(const JSegmentIterator &other)
{
    op_memcpy(this, &other, sizeof(other));
}

void
JSegmentIterator::InitSegmented(JStringSegmented *segmented0, unsigned soffset0, unsigned slength0)
{
    segmented = segmented0;
    soffset = soffset0;
    slength = slength0;
    bases = segmented->Bases();
    offsets = segmented->Offsets();
    lengths = segmented->Lengths();

    while (soffset >= lengths[index])
        soffset -= lengths[index++];

    index--;
}

BOOL
JSegmentIterator::Next()
{
    if (segmented)
        if (++index < segmented->nsegments && slength)
        {
            OP_ASSERT(soffset <= lengths[index]);

            current_base = bases[index];

            current_length = es_minu(slength, lengths[index] - soffset);
            slength -= current_length;

            current_offset = offsets[index] + soffset;
            soffset = 0;

#ifdef _DEBUG
            if (slength)
                OP_ASSERT(index + 1 < segmented->nsegments);
#endif // _DEBUG

            return TRUE;
        }
        else
        {
            current_base   = 0;
            current_length = 0;
            current_offset = 0;
            return FALSE;
        }
    else if (!index)
    {
        ++index;
        return TRUE;
    }
    else
    {
        ++index;
        return FALSE;
    }
}

inline void
Initialize(JString *self)
{
    self->InitGCTag(GCTAG_JString);
    self->value = NULL;
    self->length = 0;
    self->offset = 0;
    self->hash = 0;
    self->host_code_or_numval = 0;
}

BOOL
JString::StaticIndex(unsigned &index)
{
    if (length == 1)
    {
        uni_char c = Element(this, 0);
        if (c < STRING_NUMCHARS)
        {
            index = STRING_nul + c;
            return TRUE;
        }
    }
    return FALSE;
}

/* static */ JString*
JString::Make(ES_Context *context, const uni_char* X, unsigned nchars)
{
    if (nchars == UINT_MAX)
        nchars = uni_strlen(X);

    if (nchars <= 1)
    {
        if (nchars == 0 && context->rt_data->strings[STRING_empty])
            return context->rt_data->strings[STRING_empty];
        else
        {
            uni_char c = *X;
            if (c < STRING_NUMCHARS)
                return context->rt_data->strings[STRING_nul + c];
        }
    }

    JString *s;
    JStringStorage *ss = NULL;

#ifndef ES_FAST_COLLECTOR_ALLOCATION
    unsigned main_bytes = GC_ALIGN(sizeof(JString));
    unsigned extra_bytes = GC_ALIGN(sizeof(JStringStorage) + nchars * sizeof(uni_char));

    if (main_bytes + extra_bytes < LARGE_OBJECT_LIMIT)
    {
        GC_ALLOCATE_WITH_EXTRA_ALIGNED(context, s, ss, extra_bytes, JString, JStringStorage, (s), (ss, nchars+1, nchars, TRUE));
        op_memcpy(ss->storage, X, nchars * sizeof(uni_char));
    }
    else
#endif // ES_FAST_COLLECTOR_ALLOCATION
    {
        GC_ALLOCATE(context, s, JString, (s));
        ES_CollectorLock gclock(context);

        ss = JStringStorage::Make(context, X, nchars + 1, nchars);
    }

    s->length = nchars;
    s->value = ss;

    return s;
}

/* static */ JString*
JString::Make(ES_Context *context, const char* X, unsigned nchars)
{
    JString *s = NULL;
    if (nchars == UINT_MAX)
        nchars = op_strlen(X);
    if (nchars == 1)
    {
        int c = *(const unsigned char*)X;
        if (c < STRING_NUMCHARS)
            s = context->rt_data->strings[STRING_nul + c];
    }
    if (s == NULL)
    {
        GC_ALLOCATE(context, s, JString, (s));
        ES_CollectorLock gclock(context);

        s->length = nchars;
        s->value = JStringStorage::Make(context, X,nchars+1,nchars);
    }
    return s;
}

/* static */ JString*
JString::Make(ES_Context *context, unsigned reserve)
{
    JString *s;

    GC_ALLOCATE(context, s, JString, (s));
    ES_CollectorLock gclock(context);

    s->length = reserve;
    s->value = JStringStorage::Make(context, (const char *) NULL, reserve + 1, reserve);
    return s;
}

/* static */ JString*
JString::Make(ES_Context *context, JString *X, unsigned offset, unsigned length)
{
    JString *s = NULL;
    if (length <= 1)
    {
        if (length == 0)
        {
            if (context->rt_data->strings[STRING_empty])
                return context->rt_data->strings[STRING_empty];
            else
            {
                GC_ALLOCATE(context, s, JString, (s));
                s->value = JStringStorage::Make(context, UNI_L(""), 1, 0);
                s->offset = 0;
                s->length = 0;
                return s;
            }
        }
        else
        {
            uni_char c = Element(X, offset);
            if (c < STRING_NUMCHARS)
                return context->rt_data->strings[STRING_nul + c];
        }
    }

    GC_STACK_ANCHOR(context, X);
    GC_ALLOCATE(context, s, JString, (s));
    s->value = X->value;
    s->offset = X->offset + offset;
    s->length = length;

    return s;
}

/* static */ JString*
JString::Make(ES_Context *context, JStringStorage *X, unsigned offset, unsigned length)
{
    JString *s = NULL;
    if (length == 1)
    {
        uni_char c = X->storage[offset];
        if (c < STRING_NUMCHARS)
            return context->rt_data->strings[STRING_nul + c];
    }

    GC_STACK_ANCHOR(context, X);
    GC_ALLOCATE(context, s, JString, (s));
    s->value = X;
    s->offset = offset;
    s->length = length;

    return s;
}

/* static */ JString*
JString::MakeTemporary(ES_Context *context)
{
    JString *s;

    GC_ALLOCATE(context, s, JString, (s));

    return s;
}

/* static */ JString*
JString::Make(ES_Context *context, JStringSegmented *segmented, unsigned length)
{
    OP_ASSERT(segmented->nsegments > 0);

#ifdef _DEBUG
    for (unsigned i = 0; i < segmented->nsegments; ++i)
    {
        OP_ASSERT(segmented->Bases()[i] != NULL);
        OP_ASSERT(segmented->Lengths()[i] != 0);
    }
#endif // _DEBUG

    if (length == 1)
    {
        unsigned *lengths = segmented->Lengths();

        for (unsigned index = 0; index < segmented->nsegments; ++index)
            if (lengths[index] == 1)
            {
                int c = segmented->Bases()[index]->storage[segmented->Offsets()[index]];

                if (c < STRING_NUMCHARS)
                    return context->rt_data->strings[STRING_nul + c];
                else
                    break;
            }
    }
    else if (length == 0)
        return JString::Make(context, UNI_L(""), 0);

    JString *s;

    GC_STACK_ANCHOR(context, segmented);

    GC_ALLOCATE(context, s, JString, (s));

    s->length = length;
    s->value = reinterpret_cast<JStringStorage *>(reinterpret_cast<UINTPTR>(segmented) ^ 1);
    return s;
}

unsigned
JString::CalculateHashSegmented()
{
    unsigned hash;

    ES_HASH_INIT(hash);

    JSegmentIterator iter(this);

    while (iter.Next())
        CalculateHashInner(hash, iter.GetBase()->storage + iter.GetOffset(), iter.GetLength());

    return hash;
}

int
JString::TranslatePropertyName()
{
    if (g_ecmaManager->property_name_translator)
        if (length < 64)
        {
            if (IsSegmented(this) || value->storage[offset + length] != 0)
            {
                uni_char buffer[64]; // ARRAY OK 2011-06-08 sof

                if (!IsSegmented(this))
                    op_memcpy(buffer, value->storage + offset, UNICODE_SIZE(length));
                else
                {
                    uni_char *ptr = buffer;
                    JSegmentIterator iter(this);

                    while (iter.Next())
                    {
                        op_memcpy(ptr, iter.GetBase()->storage + iter.GetOffset(), UNICODE_SIZE(iter.GetLength()));
                        ptr += iter.GetLength();
                    }
                }

                buffer[length] = 0;

                return g_ecmaManager->property_name_translator->TranslatePropertyName(buffer);
            }
            else
                return g_ecmaManager->property_name_translator->TranslatePropertyName(value->storage + offset);
        }
        else
            return g_ecmaManager->property_name_translator->TranslatePropertyName(NULL);
    else
        return -1;
}

BOOL
JString::EqualsSegmented(const uni_char *str) const
{
    JSegmentIterator iter(this);

    while (iter.Next())
        if (op_memcmp(iter.GetBase()->storage + iter.GetOffset(), str, UNICODE_SIZE(iter.GetLength())) != 0)
            return FALSE;
    return TRUE;
}

BOOL
JString::EqualsSegmented(const char *str) const
{
    JSegmentIterator iter(this);

    unsigned j = 0;
    while (iter.Next())
    {
        unsigned segment_length = UNICODE_SIZE(iter.GetLength());
        const uni_char *storage = iter.GetBase()->storage + iter.GetOffset();
        for (unsigned index = 0; index < segment_length; ++index)
            if (storage[index] != str[j++])
                return FALSE;
    }
    return TRUE;
}

JString *
JTemporaryString::Allocate(ES_Context *context, JString *base)
{
    if (base)
    {
        OP_ASSERT(static_cast<unsigned>(string.value->storage - Storage(context, base)) < Length(base));
        return JString::Make(context, base, string.value->storage - Storage(context, base), string.length);
    }
    else
        return JString::Make(context, string.value->storage, string.length);
}

JString *
JTemporaryString::Allocate(ES_Context *context, JStringStorage *base)
{
    OP_ASSERT(static_cast<unsigned>(string.value->storage - base->storage) < base->length);
    return JString::Make(context, base, string.value->storage - base->storage, string.length);
}

JString*
Share(ES_Context *context, JString *X)
{
    JString *s;
    GC_STACK_ANCHOR(context, X);
    GC_ALLOCATE(context, s, JString, (s));
    s->value = X->value;
    s->offset = X->offset;
    s->length = X->length;
    return s;
}

JString*
Finalize(ES_Context *context, JString *s)
{
    if (Length(s) == 1)
    {
        int c = Element(s, 0);

        if (c < STRING_NUMCHARS)
            return context->rt_data->strings[STRING_nul + c];
    }

    return s;
}

int
Compare(ES_Context *context, JString* X, JString* Y)
{
    unsigned lX = X->length;
    unsigned lY = Y->length;

    unsigned charsleft=es_minz(lX, lY);
    const uni_char *a=Storage(context, X), *b=Storage(context, Y);

    // op_memcmp does not work well with uni_char.
    while (charsleft && *a == *b)
    {
        --charsleft;
        ++a;
        ++b;
    }
    if (charsleft == 0)
        return lX < lY ? -1
             : lX > lY ? +1
             : 0;
    else
        return *a - *b;
}

uni_char
Element(JStringSegmented* s, unsigned i)
{
    JStringStorage **bases = s->Bases();
    unsigned *offsets = s->Offsets();
    unsigned *lengths = s->Lengths();
    unsigned j = 0;

    while (i >= lengths[j])
        i -= lengths[j++];

    return bases[j]->storage[i + offsets[j]];
}

BOOL
SegmentedEquals(const JString* x, const JString* y)
{
    unsigned length = Length(x), index = 0;
    unsigned xoffset = 0, yoffset = 0;

    if (length == 0 && Length(y) == 0)
        return TRUE;

    JSegmentIterator xiter(x);
    JSegmentIterator yiter(y);

    xiter.Next();
    yiter.Next();

    while (index < length)
    {
        OP_ASSERT(xiter.IsValid());
        OP_ASSERT(yiter.IsValid());

        unsigned local_length = es_minu(xiter.GetLength() - xoffset, yiter.GetLength() - yoffset);

        if (op_memcmp(xiter.GetBase()->storage + xiter.GetOffset() + xoffset, yiter.GetBase()->storage + yiter.GetOffset() + yoffset, UNICODE_SIZE(local_length)) != 0)
            return FALSE;

        if ((xoffset += local_length) == xiter.GetLength())
        {
            xiter.Next();
            xoffset = 0;
        }

        if ((yoffset += local_length) == yiter.GetLength())
        {
            yiter.Next();
            yoffset = 0;
        }

        index += local_length;
    }

    OP_ASSERT(!xiter.IsValid() && !yiter.IsValid());

    return TRUE;
}

int ES_CALLING_CONVENTION
Equals(const JString* X, const JString* Y)
{
    if (X == Y)
        return TRUE;
    else if (X->length != Y->length || X->length == 1 && (X->IsBuiltinString() || Y->IsBuiltinString()))
        return FALSE;
    else if (!IsSegmented(X) && !IsSegmented(Y))
        return op_memcmp(Storage(NULL, const_cast<JString *>(X)), Storage(NULL, const_cast<JString *>(Y)), UNICODE_SIZE(X->length)) == 0;
    else
        return SegmentedEquals(X, Y);
}

uni_char*
StorageZ(ES_Context *context, JString* s)
{
    if (IsSegmented(s))
        s->value = GetSegmented(s)->MaybeRealize(context, s->offset, Length(s));

    OP_ASSERT(s->value->length >= s->offset + s->length);
    OP_ASSERT(s->value->allocated > s->offset + s->length);

    if (s->value->storage[s->offset + s->length] != 0)
    {
        OP_ASSERT(!s->IsBuiltinString());
        ES_CollectorLock gclock(context);
        Append(context, s, "", 1);
        s->length--;
        s->value->length--;
    }
    else
    {
        OP_ASSERT(s->value->length < s->value->allocated);
        s->value->storage[s->value->length] = 0;
    }
    return Storage(context, s);
}

/** Prepare 's' for additional 'nchars' characters by allocating a new
    JStringStorage if necessary. */
static void
PrepareForAppend(ES_Context *context, JString *s, unsigned nchars)
{
    unsigned new_nchars = es_maxz(s->length + nchars, s->length * 2) + 1;
    if (new_nchars * sizeof(uni_char) + 100 > ES_LIM_OBJECT_SIZE)
        /* Approaching the limits of the system; attempt to fall back on something
           smaller to allow us to keep running, but the performance implications
           of this may not be good. */
        new_nchars = s->length + nchars + 1001;

    if (IsSegmented(s))
    {
        GC_STACK_ANCHOR(context, s);
        s->value = GetSegmented(s)->Realize(context, s->offset, s->length, new_nchars - s->length);
        s->offset = 0;
    }
    else if (s->value->length != s->length || s->value->length+nchars+1 > s->value->allocated)
    {
        /* Must use new storage */
        GC_STACK_ANCHOR(context, s);
        JStringStorage *new_value = JStringStorage::Make(context, Storage(context, s), new_nchars, s->length, TRUE);
        s->value = new_value;
        s->offset = 0;
    }
    s->host_code_or_numval = 0;
}

JString*
Append(ES_Context *context, JString *s, const uni_char* X, unsigned nchars)
{
    if (nchars == UINT_MAX)
        nchars = uni_strlen(X);

    if (nchars)
    {
        PrepareForAppend(context, s, nchars);

        uni_char *target = s->value->storage + s->length;
        op_memcpy(target, X, nchars*sizeof(uni_char));
        target[nchars] = 0;
        s->length += nchars;
        s->value->length += nchars;
    }

    return s;
}

JString*
Append(ES_Context *context, JString *s, const char* X, unsigned nchars)
{
    if (nchars == UINT_MAX)
        nchars = op_strlen(X);

    if (nchars)
    {
        PrepareForAppend(context, s, nchars);

        uni_char *target = s->value->storage + s->length;
        op_memcpy(target, X, nchars);
        make_doublebyte_in_place(target, nchars);
        target[nchars] = 0;
        s->length += nchars;
        s->value->length += nchars;
    }

    return s;
}

void
Append(ES_Context *context, JString *s, JString *X, unsigned nchars)
{
    ES_CollectorLock gclock(context, TRUE);

    nchars = nchars == UINT_MAX ? X->length : nchars;

    if (!IsSegmented(X))
        Append(context, s, Storage(context, X), nchars);
    else if (nchars)
    {
        PrepareForAppend(context, s, nchars);

        uni_char *target = s->value->storage + s->length;

        s->length += nchars;
        s->value->length += nchars;

        JSegmentIterator iter(X);
        while (iter.Next() && nchars)
        {
            unsigned llength = es_minu(iter.GetLength(), nchars);
            op_memcpy(target, iter.GetBase()->storage + iter.GetOffset(), llength * sizeof(uni_char));
            target += llength;
            nchars -= llength;
        }

        *target = 0;
    }
}

JString*
Append(ES_Context *context, JString *s, uni_char c)
{
    return Append(context, s, &c, 1);
}

static BOOL
IsLowerCase(int ch)
{
    return ch >= 'a' && ch <= 'z' || ch > 127 && uni_islower(ch);
}

static BOOL
IsUpperCase(int ch)
{
    return ch >= 'A' && ch <= 'Z' || ch > 127 && uni_isupper(ch);
}

JString *
ConvertCase(ES_Context *context, JString *src, BOOL lower)
{
    size_t length = Length(src);

    if (length == 0)
        return src;
    else
    {
        const uni_char *srcstr = Storage(context, src), *srcend = srcstr + length;

        if (length == 1)
        {
            int c = lower ? uni_tolower(*srcstr) : uni_toupper(*srcstr);
            if (c < STRING_NUMCHARS)
                return context->rt_data->strings[STRING_nul + c];
        }

        const uni_char *start = srcstr;

        if (lower)
            while (start != srcend && !IsUpperCase(*start)) ++start;
        else
            while (start != srcend && !IsLowerCase(*start)) ++start;

        if (start == srcend)
            return src;

        JString *dst = JString::Make(context, length);
        uni_char *dststr = Storage(context, dst);
        unsigned static_prefix_length = start - srcstr;

        op_memcpy(dststr, srcstr, UNICODE_SIZE(static_prefix_length));
        dststr += static_prefix_length;
        srcstr += static_prefix_length;

        if (lower)
            do
                *dststr++ = uni_tolower(*srcstr++);
            while (srcstr != srcend);
        else
            do
                *dststr++ = uni_toupper(*srcstr++);
            while (srcstr != srcend);

        return dst;
    }
}

#ifdef _DEBUG
# include "modules/ecmascript/carakan/src/kernel/es_string_inlines.h"
#endif // _DEBUG
