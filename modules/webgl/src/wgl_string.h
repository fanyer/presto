/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL compiler -- string representations (JString derived.)
 *
 */
#ifndef WGL_STRING_H
#define WGL_STRING_H
#include "modules/webgl/src/wgl_base.h"

/** A JString-like/lite implementation for WebGL purposes. */
class WGL_String
{
public:
    static WGL_String *Make(WGL_Context *context, const uni_char *s, unsigned l);
    /**< Create an immutable string representing 's' (of length 'l'), shared
         if equal strings have been created previously. The string object is
         owned by 'context', which handles eventual release.

         Cannot return NULL, but will leave upon OOM. */

    static WGL_String *Make(WGL_Context *context, const uni_char *s) { return Make(context, s, static_cast<unsigned>(uni_strlen(s))); };
    /**< Create an immutable string representing 's', shared if equal strings
         have been created previously. The string object is owned by 'context',
         which handles eventual release.

         Cannot return NULL, but will leave upon OOM. */

    inline unsigned CalculateHash();
    static inline unsigned CalculateHash(const uni_char *str, unsigned length);

    static WGL_String *CopyL(WGL_String *s);
    static void Clear(WGL_String *s);
    static void Release(WGL_String *s);

    static WGL_String *AppendL(WGL_Context *context, WGL_String *&s1, WGL_String *s2);
    static uni_char *Storage(WGL_Context *context, WGL_String *s);
    static unsigned Length(const WGL_String *s);

    inline BOOL Equals(const WGL_String *other) const;
    inline BOOL Equals(const uni_char *str, unsigned len) const;
    inline BOOL Equals(const uni_char *str) const;
    inline BOOL Equals(const char *str, unsigned len) const;
    inline BOOL Equals(const char *str) const;

    unsigned Hash() { if (!hash) hash = CalculateHash(); return hash; }

    uni_char *value;
    unsigned length;
    unsigned hash;
};

#include "modules/webgl/src/wgl_string_inlines.h"

#endif // WGL_STRING_H
