/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- dumping outputs.
 *
 */
#ifndef WGL_PRINTER_CONSOLE_H
#define WGL_PRINTER_CONSOLE_H

#include "modules/webgl/src/wgl_printer.h"
#include "modules/util/opstring.h"

class WGL_ConsolePrinter
    : public WGL_Printer
{
public:
    WGL_ConsolePrinter(const uni_char *url, OpString *log_output, BOOL use_console, BOOL gl_es, BOOL high_prec)
        : WGL_Printer(gl_es, high_prec)
        , log_output(log_output)
        , use_console(use_console)
        , url(url)
    {
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
    OpString out_buffer;
    OpString *log_output;
    BOOL use_console;
    const uni_char *url;
};

#endif // WGL_PRINTER_CONSOLE_H
