/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- dumping output.
 *
 */
#ifndef WGL_PRINTER_STDIO_H
#define WGL_PRINTER_STDIO_H

#ifdef WGL_STANDALONE
#include "modules/webgl/src/wgl_printer.h"

class WGL_StdioPrinter
    : public WGL_Printer
{
public:
    WGL_StdioPrinter(FILE *f, BOOL gl_es, BOOL highp, BOOL flag = FALSE)
        : WGL_Printer(gl_es, highp)
        , fp(f)
        , owns_file(flag)
    {
    }

    virtual ~WGL_StdioPrinter()
    {
        if (owns_file)
            fclose(fp);
    }

    virtual void OutInt(int i);
    virtual void OutDouble(double d);
    virtual void OutBool(BOOL b);
    virtual void OutString(const char *s);
    virtual void OutString(uni_char *s);
    virtual void OutString(const uni_char *s);
    virtual void OutStringId(const uni_char *s);
    virtual void OutString(WGL_VarName *s);

    virtual void OutNewline();

    virtual void Flush(BOOL as_error);

private:
    FILE *fp;

    BOOL owns_file;
    /**< TRUE => printer has ownership of FILE and must
         close on delete. */
};

#endif // WGL_STANDALONE
#endif // WGL_PRINTER_STDIO_H
