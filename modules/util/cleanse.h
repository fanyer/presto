/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_CLEANSE_H
#define MODULES_UTIL_CLEANSE_H

#ifdef __cplusplus
extern "C"
{
#endif

	void OPERA_cleanse_heap(void *ptr, size_t len)
#ifndef CLEAR_PASSWD_FROM_MEMORY
	{
		// Do nothing
	}
#endif // !CLEAR_PASSWD_FROM_MEMORY
	;

	void OPERA_cleanse(void *ptr, size_t len)
#ifndef CLEAR_PASSWD_FROM_MEMORY
	{
		// Do nothing
	}
#endif // !CLEAR_PASSWD_FROM_MEMORY
	;

#ifdef __cplusplus
}
#endif

#endif // !MODULES_UTIL_CLEANSE_H
