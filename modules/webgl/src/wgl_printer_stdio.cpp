/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009 - 2011
 *
 * WebGL GLSL compiler -- pretty printer for AST.
 *
 */

#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT
#ifdef WGL_STANDALONE
#include "modules/webgl/src/wgl_printer_stdio.h"
#include "modules/webgl/src/wgl_string.h"

#include "modules/util/opstring.h"

/* virtual */ void
WGL_StdioPrinter::OutInt(int i)
{
    fprintf(fp, "%d", i);
}

/* virtual */ void
WGL_StdioPrinter::OutDouble(double d)
{
    if (op_isintegral(d))
        fprintf(fp, "%g.", d);
    else
        fprintf(fp, "%g", d);
}

/* virtual */ void
WGL_StdioPrinter::OutBool(BOOL b)
{
    fprintf(fp, "%s", b ? "true" : "false");
}

/* virtual */ void
WGL_StdioPrinter::OutString(const char *s)
{
    fprintf(fp, "%s", s);

}

/* virtual */ void
WGL_StdioPrinter::OutString(uni_char *s)
{
    OpString8 cstr;
    LEAVE_IF_ERROR(cstr.Set(s));
    fprintf(fp, "%s", cstr.CStr());
}

/* virtual */ void
WGL_StdioPrinter::OutString(const uni_char *s)
{
    OpString8 cstr;
    LEAVE_IF_ERROR(cstr.Set(s));
    fprintf(fp, "%s", cstr.CStr());
}

/* virtual */ void
WGL_StdioPrinter::OutStringId(const uni_char *s)
{
    OpString8 cstr;
    LEAVE_IF_ERROR(cstr.Set(s));
    fprintf(fp, "%s", cstr.CStr());
}

/* virtual */ void
WGL_StdioPrinter::OutString(WGL_VarName *i)
{
    OpString8 cstr;
    LEAVE_IF_ERROR(cstr.Set(i->value));
    fprintf(fp, "%s", cstr.CStr());
}

/* virtual */ void
WGL_StdioPrinter::OutNewline()
{
    fprintf(fp, "\n");
    for (unsigned i = 0; i < indent; i++)
        fputc(' ', fp);
}

/* virtual */ void
WGL_StdioPrinter::Flush(BOOL as_error)
{
    fflush(fp);
}
#endif // WGL_STANDALONE

#endif // CANVAS3D_SUPPORT
