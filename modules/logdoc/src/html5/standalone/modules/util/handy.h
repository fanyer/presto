/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_HANDY_H
#define MODULES_UTIL_HANDY_H

#ifdef ARRAY_SIZE
#undef ARRAY_SIZE
#endif // ARRAY_SIZE

#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))

BOOL UpdateOfflineModeL(BOOL toggle);
OP_BOOLEAN UpdateOfflineMode(BOOL toggle);

#endif // !MODULES_UTIL_HANDY_H
