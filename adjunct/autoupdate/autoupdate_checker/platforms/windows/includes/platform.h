/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: s; c-basic-offset: 2 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef OAUC_WIN_INCLUDES_H
# define OAUC_WIN_INCLUDES_H

# include <windows.h>
# include <WinDef.h>

typedef DWORD OAUCTime;
const OAUCTime RETURN_IMMEDIATELY = 0;
const OAUCTime WAIT_INFINITELY = static_cast<DWORD>(-1);
const OAUCTime OAUC_PREFFERED_SLEEP_TIME_BETWEEN_RETRIES = static_cast<DWORD>(5 * 1000);
typedef WCHAR uni_char;
typedef DWORD PidType;

# ifndef NULLPTR
# define NULLPTR nullptr
# endif

# include <new>
# include <assert.h>

# ifndef OAUC_NEW
# define OAUC_NEW(obj, args) new obj args
# endif

# ifndef OAUC_NEWA
# define OAUC_NEWA(obj, count) new obj[count]
# endif

# ifndef OAUC_DELETE
# define OAUC_DELETE(ptr) delete ptr
# endif

# ifndef OAUC_DELETEA
# define OAUC_DELETEA(ptr) delete [] ptr
# endif

# ifndef OAUC_ASSERT
# define OAUC_ASSERT(expr) assert(expr)
# endif

# ifndef ADAPTATION_LAYER_IMPL_HEADERS
# define ADAPTATION_LAYER_IMPL_HEADERS "platforms/universal_adaptation_layer/all_includes.h"
# endif

#endif // OAUC_WIN_INCLUDES_H
