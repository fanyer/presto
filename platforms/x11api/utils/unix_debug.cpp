/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @brief Implementation of Opera's debug functions that doesn't need posix dependencies */

#include "core/pch.h"

#if defined(X11API) && defined(NO_CORE_COMPONENTS)

#include "platforms/posix/posix_native_util.h"
#include "modules/opdata/UniString.h"

# ifdef DEBUG_ENABLE_SYSTEM_OUTPUT
extern "C" void dbg_systemoutput(const uni_char* str)
{
	OpString8 output;
	RETURN_VOID_IF_ERROR(PosixNativeUtil::ToNative(str, &output));

	if (output.HasContent())
		fprintf(stderr, "%s\n", output.CStr());
}

extern "C" void dbg_logfileoutput(const char* txt, int len)
{
	if (len)
	{
		// '%', '.', digits, 's', '\0'
		char format[INT_STRSIZE(len) + 4]; // ARRAY OK 2010-08-12 markuso
		op_sprintf(format, "%%.%ds", len);
		fprintf(stderr, format, txt);
	}
}
# endif // DEBUG_ENABLE_SYSTEM_OUTPUT

# ifdef DEBUG_ENABLE_OPASSERT
extern "C" void Debug_OpAssert(const char* expression, const char* file, int line)
{
	fprintf(stderr, "Failed OP_ASSERT(%s) at %s:%d\n", expression, file, line);
}
# endif // DEBUG_ENABLE_OPASSERT

#endif // X11API && NO_CORE_COMPONENTS
