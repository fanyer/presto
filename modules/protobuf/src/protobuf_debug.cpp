/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#include "core/pch.h"

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/protobuf_debug.h"

/* PROTOBUF_DEBUG */

#if defined(DEBUG_PROTOBUF) && defined(DEBUG_ENABLE_PRINTF)

void PROTOBUF_DUMP(const char* data, size_t bytes)
{
	PROTOBUF_DEBUG_PROC();
	dbg_printf("[");
	for (size_t n = 0; n < bytes; n++)
	{
		if (isprint(data[n]))
			dbg_printf("%c", data[n]);
		else
			dbg_printf("\\x%.2x", (unsigned char)data[n]);
	}
	dbg_printf("]\n");
}

#endif // defined(DEBUG_PROTOBUF) && defined(DEBUG_ENABLE_PRINTF)

#endif // PROTOBUF_SUPPORT
