/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- base definitions.
 *
 */
#ifndef WGL_BASE_H
#define WGL_BASE_H

class WGL_String;
typedef WGL_String WGL_VarName;

struct WGL_ProgramText
{
    uni_char *program_text;
    unsigned program_text_length;
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif // ARRAY_SIZE

class WGL_Context;
#endif // WGL_BASE_H
