/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/* This file contains includes and defines that are commonly
 * used in the plugins/plugin*.h files.
 */

#ifndef _PLUGIN_COMMON_H_INC
#define _PLUGIN_COMMON_H_INC

#define RETURN_NPERR_IF_ERROR(expr) \
do { \
	OP_STATUS RETURN_IF_ERROR_TMP = expr; \
	if (OpStatus::IsError(RETURN_IF_ERROR_TMP)) \
		if (OpStatus::IsMemoryError(RETURN_IF_ERROR_TMP)) \
			return NPERR_OUT_OF_MEMORY_ERROR; \
		else \
			return NPERR_GENERIC_ERROR; \
} while (0)

#ifdef _PLUGIN_SUPPORT_

/* These (with includes and undefs) are only used to
 *avoid X11 name clashes, I believe
 */
#define String XTString

#ifdef USE_XP_UNIX
# define XP_UNIX
# define MOZ_X11
#endif // USE_XP_UNIX

#undef RAND_MAX
#include "modules/ns4plugins/src/plug-inc/npfunctions.h"

#undef Always
#undef Window
#undef String
#undef CurrentTime
#undef Unsorted
#undef RectangleOut
#undef Status
#undef CursorShape
#undef KeyRelease
#undef KeyPress
#undef FocusIn
#undef FocusOut
#undef Enter
#undef Leave
#undef Paint
#undef Move
#undef Resize
#undef Create
#undef Destroy
#undef Show
#undef Hide
#undef Close
#undef Quit
#undef Reparent
#undef Bool
#undef None
#undef BadRequest
#undef ProtocolVersion

#if defined(_OPL_)
#define XP_UNIX
#endif

// The following definition is used by NPP_ClearSiteData to delete all data
// since the beginning of time (basically).
#define NP_CLEAR_MAX_AGE_ALL ~0u

#endif // _PLUGIN_SUPPORT_
#endif // !_PLUGIN_COMMON_H_INC
