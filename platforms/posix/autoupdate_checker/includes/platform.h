// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2012 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#ifndef PLATFORMS_POSIX_AUTOUPDATE_CHECKER_INCLUDES_PLATFORM_H_
#define PLATFORMS_POSIX_AUTOUPDATE_CHECKER_INCLUDES_PLATFORM_H_

#include <stdint.h>
#include <ctime>
#include <new>
#include <cassert>
#include <limits>

/* If we're building for Opera 12, we need to use OP_NEW/OP_DELETE for
 * allocation and deletion, otherwise we will break MemGuard. These macros
 * are defined in "modules/memory/memory_fundamentals.h" */
#ifndef OAUC_STANDALONE
#include "modules/memory/memory_fundamentals.h"
#endif

typedef time_t OAUCTime;
const OAUCTime RETURN_IMMEDIATELY = 0;
const OAUCTime WAIT_INFINITELY = std::numeric_limits<OAUCTime>::max();
const OAUCTime OAUC_PREFFERED_SLEEP_TIME_BETWEEN_RETRIES = 5 * 1000;
typedef pid_t PidType;

#ifndef NULLPTR
# define NULLPTR NULL
#endif

/* The Standalone component will allocate/delete using a standard, non-throwing
 * new/delete, whereas if the files are compiled within Opera 12, we'll need
 * OP_NEW/OP_DELETE. */
#ifndef OAUC_NEW
#ifdef OAUC_STANDALONE
# define OAUC_NEW(obj, args) new (std::nothrow) obj args
#else
# define OAUC_NEW(obj, args) OP_NEW(obj, args)
#endif
#endif

#ifndef OAUC_NEWA
#ifdef OAUC_STANDALONE
# define OAUC_NEWA(obj, count) new (std::nothrow) obj[count]
#else
# define OAUC_NEWA(obj, args) OP_NEWA(obj, args)
#endif
#endif

#ifndef OAUC_DELETE
#ifdef OAUC_STANDALONE
# define OAUC_DELETE(ptr) delete ptr
#else
# define OAUC_DELETE(ptr) OP_DELETE(ptr)
#endif
#endif

#ifndef OAUC_DELETEA
#ifdef OAUC_STANDALONE
# define OAUC_DELETEA(ptr) delete [] ptr
#else
# define OAUC_DELETEA(ptr) OP_DELETEA(ptr)
#endif
#endif

#ifndef OAUC_ASSERT
# define OAUC_ASSERT(expr) assert(expr)
#endif

#endif  // PLATFORMS_POSIX_AUTOUPDATE_CHECKER_INCLUDES_PLATFORM_H_
