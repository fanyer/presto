/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file stdlib_printf.cpp Printf and scanf */

#include "core/pch.h"

#ifdef EBERHARD_MATTES_PRINTF /* Requires third-party code */

#include "modules/stdlib/src/thirdparty_printf/uni_printf.h"
#include "modules/stdlib/src/thirdparty_printf/uni_scanf.h"

extern "C" {

#ifndef HAVE_SPRINTF

int op_sprintf(char *buffer, const char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int result = op_vsnprintf(buffer, INT_MAX, format, arguments);
	va_end(arguments);
	return result;
}

#endif

#ifndef HAVE_VSPRINTF

int op_vsprintf(char *buffer, const char *format, va_list argptr)
{
	return op_vsnprintf(buffer, INT_MAX, format, argptr);
}

#endif

#ifndef HAVE_SNPRINTF

int op_snprintf(char *buffer, size_t count, const char *format, ...)
{
	va_list arguments;
	va_start(arguments, format);
	int result = op_vsnprintf(buffer, count, format, arguments);
	va_end(arguments);
	return result;
}

#endif

#ifndef HAVE_VSNPRINTF

/* If the buffer is too small then op_vsnprintf must (according to ANSI C) return
   how large the buffer needs to be (not including \0)  */

int op_vsnprintf(char *buffer, size_t count, const char *format, va_list argptr)
{
	OpPrintf printer(OpPrintf::PRINTF_ASCII);
	return printer.Format(buffer, count, format, argptr);
}

#endif

#ifndef HAVE_SSCANF

int op_sscanf(const char *input, const char *format, ...)
{
	OpScanf scanner(OpScanf::SCANF_ASCII);
	va_list arg_ptr;
	va_start(arg_ptr, format);
	int rc = scanner.Parse(input, format, arg_ptr);
	va_end(arg_ptr);
	return rc;
}

#endif // HAVE_SSCANF

#ifndef HAVE_VSSCANF

int op_vsscanf(const char *input, const char *format, va_list arg_ptr)
{
	OpScanf scanner(OpScanf::SCANF_ASCII);
	return scanner.Parse(input, format, arg_ptr);
}

#endif // HAVE_VSSCANF

} // extern "C"

#ifndef HAVE_UNI_SPRINTF
int uni_sprintf(uni_char * buffer, const uni_char * format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);
	int result = uni_vsnprintf(buffer, INT_MAX, format, arg_ptr);
	va_end(arg_ptr);
	return result;
}
#endif // HAVE_UNI_SPRINTF

#ifndef HAVE_UNI_VSPRINTF
int uni_vsprintf(uni_char * buffer, const uni_char * format, va_list arg_ptr)
{
	return uni_vsnprintf(buffer, INT_MAX, format, arg_ptr);
}
#endif // HAVE_UNI_VSPRINTF

#ifndef HAVE_UNI_SNPRINTF
int uni_snprintf(uni_char * buffer, size_t count, const uni_char * format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);
	int result = uni_vsnprintf(buffer, count, format, arg_ptr);
	va_end(arg_ptr);
	return result;
}
#endif // HAVE_UNI_SNPRINTF

#ifndef HAVE_UNI_VSNPRINTF
int uni_vsnprintf(uni_char * buffer, size_t count, const uni_char * format, va_list arg_ptr)
{
	OpPrintf printer(OpPrintf::PRINTF_UNICODE);
	return printer.Format(buffer, count, format, arg_ptr);
}
#endif // HAVE_UNI_VSNPRINTF

#ifndef HAVE_UNI_SSCANF
int uni_sscanf(const uni_char * input, const uni_char * format, ...)
{
	OpScanf scanner(OpScanf::SCANF_UNICODE);
	va_list arg_ptr;
	va_start(arg_ptr, format);
	int rc = scanner.Parse(input, format, arg_ptr);
	va_end(arg_ptr);
	return rc;
}
#endif

#endif // EBERHARD_MATTES_PRINTF
