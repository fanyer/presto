/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL compiler -- string representations (JString derived.)
 *
 */
#ifndef WGL_STRING_INLINES_H
#define WGL_STRING_INLINES_H
/* DJ Bernstein's hashing function: simple and effective. */
#define WGL_HASH_INIT(x) x = 5381
#define WGL_HASH_UPDATE(x, c) x = ((x << 5) + x) + c

inline uni_char*
Storage(WGL_Context *context, WGL_String* s)
{
	return s->value;
}

inline BOOL
WGL_String::Equals(const WGL_String *other) const
{
    if (length == other->length)
    {
        const uni_char *x = value, *y = other->value;
        for (unsigned index = 0; index < length; ++index)
            if (x[index] != y[index])
               return FALSE;
        return TRUE;
    }
    else
        return FALSE;
}

inline BOOL
WGL_String::Equals(const uni_char *str, unsigned len) const
{
    if (length == len)
    {
        const uni_char *storage = value;
        for (unsigned index = 0; index < len; ++index)
            if (storage[index] != str[index])
                return FALSE;
        return TRUE;
    }
    else
        return FALSE;
}

inline BOOL
WGL_String::Equals(const char *str, unsigned len) const
{
    if (length == len)
    {
        const uni_char *storage = value;
        for (unsigned index = 0; index < len; ++index)
            if (storage[index] != static_cast<uni_char>(str[index]))
                return FALSE;
        return TRUE;
    }
    else
        return FALSE;
}

inline BOOL
WGL_String::Equals(const char *str) const
{
    return Equals(str, static_cast<unsigned>(op_strlen(str)));
}

inline BOOL
WGL_String::Equals(const uni_char *str) const
{
    return Equals(str, static_cast<unsigned>(uni_strlen(str)));
}

static inline void
CalculateHashInner(unsigned &hash, const uni_char *string, unsigned length)
{
    unsigned h = hash;
    unsigned i = 0, l = length;
    while (i < l)
        WGL_HASH_UPDATE(h, string[i++]);
    hash = h;
}

inline unsigned
WGL_String::CalculateHash()
{
    unsigned hash;
    WGL_HASH_INIT(hash);
    CalculateHashInner(hash, value, length);
    return hash;
}

/*static */inline unsigned
WGL_String::CalculateHash(const uni_char *str, unsigned length)
{
    unsigned hash;
    WGL_HASH_INIT(hash);
    CalculateHashInner(hash, str, length);
    return hash;
}

#endif // WGL_STRING_INLINES_H
