/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009 - 2010
 *
 * WebGL GLSL compiler -- pretty printer for AST.
 *
 */

#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_printer_console.h"
#include "modules/webgl/src/wgl_string.h"

#ifndef WGL_STANDALONE
#include "modules/console/opconsoleengine.h"
#endif


/* virtual */ void
WGL_ConsolePrinter::OutInt(int i)
{
    if (use_console)
        LEAVE_IF_ERROR(out_buffer.AppendFormat(UNI_L("%d"), i));
    if (log_output)
        LEAVE_IF_ERROR(log_output->AppendFormat(UNI_L("%d"), i));
}

/* virtual */ void
WGL_ConsolePrinter::OutDouble(double d)
{
    if (op_isintegral(d))
    {
        if (use_console)
            LEAVE_IF_ERROR(out_buffer.AppendFormat(UNI_L("%g."), d));
        if (log_output)
            LEAVE_IF_ERROR(log_output->AppendFormat(UNI_L("%g."), d));
    }
    else
    {
        if (use_console)
            LEAVE_IF_ERROR(out_buffer.AppendFormat(UNI_L("%g"), d));
        if (log_output)
            LEAVE_IF_ERROR(log_output->AppendFormat(UNI_L("%g"), d));
    }
}

/* virtual */ void
WGL_ConsolePrinter::OutBool(BOOL b)
{
    const uni_char *val = b ? UNI_L("true") : UNI_L("false");
    if (use_console)
        LEAVE_IF_ERROR(out_buffer.Append(val));
    if (log_output)
        LEAVE_IF_ERROR(log_output->Append(val));
}

/* virtual */ void
WGL_ConsolePrinter::OutString(const char *s)
{
    if (use_console)
        LEAVE_IF_ERROR(out_buffer.Append(s));
    if (log_output)
        LEAVE_IF_ERROR(log_output->Append(s));
}

/* virtual */ void
WGL_ConsolePrinter::OutString(uni_char *s)
{
    if (use_console)
        LEAVE_IF_ERROR(out_buffer.Append(s));
    if (log_output)
        LEAVE_IF_ERROR(log_output->Append(s));
}

/* virtual */ void
WGL_ConsolePrinter::OutString(const uni_char *s)
{
    if (use_console)
        LEAVE_IF_ERROR(out_buffer.Append(s));
    if (log_output)
        LEAVE_IF_ERROR(log_output->Append(s));
}

/* virtual */ void
WGL_ConsolePrinter::OutStringId(const uni_char *s)
{
    if (use_console)
        LEAVE_IF_ERROR(out_buffer.Append(s));
    if (log_output)
        LEAVE_IF_ERROR(log_output->Append(s));
}

/* virtual */ void
WGL_ConsolePrinter::OutString(WGL_VarName *i)
{
    if (use_console)
        LEAVE_IF_ERROR(out_buffer.Append(i->value));
    if (log_output)
        LEAVE_IF_ERROR(log_output->Append(i->value));
}

/* virtual */ void
WGL_ConsolePrinter::OutNewline()
{
    OutString("\n");
    for (unsigned i = 0; i < indent; i++)
        OutString(" ");
}

/* virtual */ void
WGL_ConsolePrinter::Flush(BOOL as_error)
{
    if (use_console)
#ifndef WGL_STANDALONE
    {
        OpConsoleEngine::Message message(OpConsoleEngine::EcmaScript, as_error ? OpConsoleEngine::Error : OpConsoleEngine::Information);

        if (url)
            LEAVE_IF_ERROR(message.url.Set(url));

        LEAVE_IF_ERROR(message.context.Set(as_error ? "WebGL shader validation" : "WebGL shader validation warning"));
        LEAVE_IF_ERROR(message.message.Set(out_buffer.CStr()));
        message.window = 0;

        g_console->PostMessageL(&message);
        LEAVE_IF_ERROR(out_buffer.Set(""));
    }
#else
    {
        OpString8 cstr;
        LEAVE_IF_ERROR(cstr.Set(out_buffer));
        fprintf(stderr, "%s", cstr.CStr());
    }
#endif

    if (log_output)
        LEAVE_IF_ERROR(log_output->Append("\n"));
}

#endif // CANVAS3D_SUPPORT

