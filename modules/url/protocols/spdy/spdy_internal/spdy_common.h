/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#ifndef _SPDY_COMMON_H_
#define _SPDY_COMMON_H_

#ifdef _DEBUG
# define SPDY_LEAVE(err) \
	{ \
		OP_ASSERT(FALSE); \
		LEAVE(err); \
	}
#else
# define SPDY_LEAVE(err) LEAVE(err)
#endif //_DEBUG

#define SPDY_LEAVE_IF_ERROR(expr) \
	do { \
		OP_STATUS LEAVE_IF_ERROR_TMP = expr; \
		if (OpStatus::IsError(LEAVE_IF_ERROR_TMP)) \
			SPDY_LEAVE(LEAVE_IF_ERROR_TMP); \
	} while (0)

#define SPDY_LEAVE_IF_NULL(expr) \
	do { \
		if (NULL == (expr)) \
			SPDY_LEAVE(OpStatus::ERR_NO_MEMORY); \
	} while (0)


#endif // _SPDY_COMMON_H_

