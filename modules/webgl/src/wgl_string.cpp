/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2010 - 2011
 *
 * WebGL compiler -- string representations (JString derived.)
 *
 */
#include "core/pch.h"
#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_string.h"
#include "modules/webgl/src/wgl_context.h"

/* static */ WGL_String *
WGL_String::Make(WGL_Context *context, const uni_char *s, unsigned l)
{
    unsigned position;
    if (context->GetStrings()->IndexOf(s, l, position))
    {
        WGL_String *obj = NULL;
#ifdef DEBUG_ENABLE_OPASSERT
        BOOL found =
#endif // DEBUG_ENABLE_OPASSERT
        context->GetStrings()->Lookup(position, obj);
        OP_ASSERT(found);
        return obj;
    }

    WGL_String *obj = OP_NEWGRO_L(WGL_String, (), context->Arena());

    obj->value = const_cast<uni_char *>(s);
    obj->length = l;

    uni_char *str = OP_NEWGROA_L(uni_char, (l + 1), context->Arena());
    op_memcpy(str, s, l * sizeof(uni_char));
    str[l] = 0;
    obj->value = str;
    obj->length = l;
#ifdef DEBUG_ENABLE_OPASSERT
    BOOL was_added =
#endif // DEBUG_ENABLE_OPASSERT
    context->GetStrings()->AppendL(context, obj, position);
    OP_ASSERT(was_added);
    return obj;
}

/* static */ uni_char *
WGL_String::Storage(WGL_Context *context, WGL_String *s)
{
    return s->value;
}

/* static */ unsigned
WGL_String::Length(const WGL_String *s)
{
    return s->length;
}

/* static */ WGL_String *
WGL_String::CopyL(WGL_String *s)
{
    WGL_String *obj = OP_NEW_L(WGL_String, ());
    uni_char *str = OP_NEWA(uni_char, (s->length + 1));
    if (!str)
    {
        OP_DELETE(obj);
        LEAVE(OpStatus::ERR_NO_MEMORY);
    }

    op_memcpy(str, s->value, s->length * sizeof(uni_char));
    str[s->length] = 0;
    obj->value = str;
    obj->length = s->length;
    return obj;
}

/* static */ void
WGL_String::Clear(WGL_String *s)
{
    OP_DELETEA(s->value);
    s->value = NULL;
    s->length = 0;
}

/* static */ void
WGL_String::Release(WGL_String *s)
{
    WGL_String::Clear(s);
    OP_DELETE(s);
}

/* static */ WGL_String *
WGL_String::AppendL(WGL_Context *context, WGL_String *&s1, WGL_String *s2)
{
    //  Note: not intended for repetitive building of string values.
    unsigned int len = s1->length + s2->length;
    uni_char *str = OP_NEWGROA_L(uni_char, (len + 1), context->Arena());

    op_memcpy(str, s1->value, s1->length * sizeof(uni_char));
    op_memcpy(str + s1->length, s2->value, s2->length * sizeof(uni_char));

    str[len] = 0;
    s1->value = str;
    s1->length = len;
    return s1;
}

#ifdef _DEBUG
# include "modules/webgl/src/wgl_string_inlines.h"
#endif // _DEBUG

#endif // CANVAS3D_SUPPORT
