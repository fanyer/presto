/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  1999-2006
 *
 * @author Lars T Hansen
 */

#ifndef ES_STRING_INLINES_H
#define ES_STRING_INLINES_H

inline BOOL
JString::Equals(const JString *other) const
{
    if (length == other->length)
        if (!IsSegmented(this) && !IsSegmented(other))
        {
            const uni_char *x = value->storage + offset, *y = other->value->storage + other->offset;
            for (unsigned index = 0; index < length; ++index)
                if (x[index] != y[index])
                    return FALSE;
            return TRUE;
        }
        else
            return !!::Equals(this, other);
    else
        return FALSE;
}

inline BOOL
JString::Equals(const uni_char *str, unsigned len) const
{
    if (length == len)
        if (!IsSegmented(this))
        {
            const uni_char *storage = &value->storage[offset];
            for (unsigned index = 0; index < len; ++index)
                if (storage[index] != str[index])
                    return FALSE;
            return TRUE;
        }
        else
            return EqualsSegmented(str);
    else
        return FALSE;
}

inline BOOL
JString::Equals(const char *str, unsigned len) const
{
    if (length == len)
        if (!IsSegmented(this))
        {
            const uni_char *storage = &value->storage[offset];
            for (unsigned index = 0; index < len; ++index)
                if (storage[index] != str[index])
                    return FALSE;
            return TRUE;
        }
        else
        {
            return EqualsSegmented(str);
        }
    else
        return FALSE;
}

static inline void
CalculateHashInner(unsigned &hash, const uni_char *string, unsigned length)
{
    unsigned h = hash;
    unsigned i = 0, l = length;
    while (i < l)
        ES_HASH_UPDATE(h, string[i++]);
    hash = h;
}

inline unsigned
JString::CalculateHash()
{
    if (!IsSegmented(this))
    {
        unsigned hash;
        ES_HASH_INIT(hash);
        CalculateHashInner(hash, &value->storage[offset], length);
        return hash;
    }
    else
        return CalculateHashSegmented();
}

inline void
JString::SetTemporary(JString *base, unsigned new_offset, unsigned new_length)
{
    OP_ASSERT(!IsSegmented(base));

    value = base->value;
    offset = new_offset;
    length = new_length;
    hash = 0;
}

inline /* static */ void
JString::SetValue(JString *s, int v)
{
    if (v > -(1 << 29) && v < (1 << 29))
        s->host_code_or_numval = (static_cast<unsigned>(v) << 2) | 1;
}

inline /* static */ void
JString::SetHostCode(JString *s, unsigned h)
{
    s->host_code_or_numval = (h << 2) | 2;
}

inline unsigned
Length(const JString* s)
{
	return s->length;
}

inline uni_char
Element(const JString* s, unsigned i)
{
    if (!IsSegmented(s))
        return s->value->storage[i + s->offset];
    else
        return Element(GetSegmented(s), i + s->offset);
}

inline uni_char*
Storage(ES_Context *context, JString* s)
{
    if (IsSegmented(s))
    {
        const_cast<JString *>(s)->value = GetSegmented(s)->Realize(context, s->offset, Length(s));
        const_cast<JString *>(s)->offset = 0;
    }

	return s->value->storage + s->offset;
}

inline JStringStorage*
StorageSegment(ES_Context *context, JString *s, unsigned &soffset, unsigned slength)
{
    soffset += s->offset;
    if (IsSegmented(s))
        return GetSegmented(s)->MaybeRealize(context, soffset, slength);
    else
        return s->value;
}

inline BOOL
IsSegmented(const JString* s)
{
    return (reinterpret_cast<UINTPTR>(s->value) & 1) != 0;
}

inline BOOL
IsSegmented(const JStringStorage* s)
{
    return (reinterpret_cast<UINTPTR>(s) & 1) != 0;
}

inline JStringSegmented *
GetSegmented(const JString* s)
{
    OP_ASSERT(IsSegmented(s));
    return reinterpret_cast<JStringSegmented *>(reinterpret_cast<UINTPTR>(s->value) ^ 1);
}

inline void
SetSegmented(JString* s, JStringSegmented *seg)
{
    OP_ASSERT(IsSegmented(s));
    s->value = reinterpret_cast<JStringStorage *>(reinterpret_cast<UINTPTR>(seg) | 1);
}

inline unsigned
GetSegmentCount(JString *s)
{
    if (IsSegmented(s))
    {
        JSegmentIterator iter(s);
        unsigned segments = 0;
        while (iter.Next())
            segments++;
        return segments;
    }
    else
        return Length(s) != 0;
}

#endif // ES_STRING_INLINES_H
