/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef __MWERKS__
#pragma profile off
#endif

#include "platforms/mac/memory/CarbonMemory.h"

	// Look for external allocators
#include "adjunct/embrowser/EmBrowser.h"
extern AppMallocProc	extMalloc;
extern AppFreeProc		extFree;
extern AppReallocProc	extRealloc;
extern Boolean			extMemInitialized;


#if defined(_MAC_DEBUG)
# if defined(_AM_DEBUG)
#  include "platforms/mac/debug/anders/settings.anders.h"	// rather paranoid memory debugging
# elif defined(_MINCH_DEBUG)
#  include "platforms/mac/debug/minch/settings.minch.h"
# endif
#include "platforms/mac/debug/debug_mem.h"	// public memory debugging tools
#endif //_MAC_DEBUG

#ifndef MEM_SUBROUTINE
#define MEM_SUBROUTINE(a)
#endif

#ifndef CHECK_DELETE_PROTECTION
#define CHECK_DELETE_PROTECTION(a)
#endif


// This is the simple allocator, Nothing fancy going on here.
  // X only. Use system C library functions.
#  define MAC_ALLOCATE(siz)			malloc(size)
#  define MAC_FREE(buf)				free(buf)
#  define MAC_REALLOCATE(buf,size)	realloc(buf,size)



#if defined(_MAC_DEBUG) && !defined(MEM_CAP_DECLARES_MEMORY_API)

void *operator new(size_t size) throw()
{	MEM_SUBROUTINE(777003)

	register void * buf;

#ifdef BUFFER_LIMIT_CHECK
LIMIT_CHECK_ALLOC_SIZE(size);
#endif

	buf = MAC_ALLOCATE(size);

#ifdef BUFFER_LIMIT_CHECK
SET_MARKERS(buf,size,NEW_MAGIC);
#endif
#ifdef _DEADBEEF_ON_ALLOCATE_
deadbeefset(buf, size);
#endif

	return(buf);
}

void operator delete(void *buf) throw()
{	MEM_SUBROUTINE(777004)

CHECK_DELETE_PROTECTION(buf);
#ifdef BUFFER_LIMIT_CHECK
int bufsize = BUFFER_SIZE(buf);
CHECK_MARKERS(buf, NEW_MAGIC);
#ifdef _DEADBEEF_ON_DISPOSE_
feebdaedset(((char*)buf)+LIMIT_CHECK_PRE_SIZE, bufsize);
#endif
#endif
	MAC_FREE(buf);
}

void *operator new[](size_t size) throw()
{	MEM_SUBROUTINE(777005)

	register void * buf;

#ifdef BUFFER_LIMIT_CHECK
LIMIT_CHECK_ALLOC_SIZE(size);
#endif

	buf = MAC_ALLOCATE(size);

#ifdef BUFFER_LIMIT_CHECK
SET_MARKERS(buf,size,NEWA_MAGIC);
#endif
#ifdef _DEADBEEF_ON_ALLOCATE_
deadbeefset(buf, size);
#endif

	return(buf);
}

void operator delete[](void *buf) throw()
{	MEM_SUBROUTINE(777006)

CHECK_DELETE_PROTECTION(buf);
#ifdef BUFFER_LIMIT_CHECK
int bufsize = BUFFER_SIZE(buf);
CHECK_MARKERS(buf, NEWA_MAGIC);
#ifdef _DEADBEEF_ON_DISPOSE_
feebdaedset(((char*)buf)+LIMIT_CHECK_PRE_SIZE, bufsize);
#endif
#endif
	MAC_FREE(buf);
}

#endif // _MAC_DEBUG && !MEM_CAP_DECLARES_MEMORY_API

#pragma mark -

// A few tools that allows us to check malloc/free abuse.
// Release builds will use the proper malloc/free

void *CarbonMalloc(size_t size)
{	MEM_SUBROUTINE(777007)

	register void * buf;

#ifdef BUFFER_LIMIT_CHECK
LIMIT_CHECK_ALLOC_SIZE(size);
#endif

	buf = MAC_ALLOCATE(size);

#ifdef BUFFER_LIMIT_CHECK
SET_MARKERS(buf,size,OP_ALLOC_MAGIC);
#endif
#ifdef _DEADBEEF_ON_ALLOCATE_
deadbeefset(buf, size);
#endif

	return(buf);
}

void *CarbonCalloc(size_t nmemb, size_t memb_size)
{	MEM_SUBROUTINE(777008)

	size_t size = nmemb * memb_size;
	register void * buf;

#ifdef BUFFER_LIMIT_CHECK
LIMIT_CHECK_ALLOC_SIZE(size);
#endif

	buf = MAC_ALLOCATE(size);

#ifdef BUFFER_LIMIT_CHECK
SET_MARKERS(buf,size,OP_ALLOC_MAGIC);
#endif

	if (buf)
	{
		memset(buf, 0, size);
	}
	return buf;
}

void *CarbonRealloc(void* buf, size_t newSize)
{	MEM_SUBROUTINE(777009)

CHECK_DELETE_PROTECTION(buf);
	if (buf == nil)
	{
		return CarbonMalloc(newSize);
	}
	else
	{
#ifdef BUFFER_LIMIT_CHECK
CHECK_MARKERS(buf, OP_ALLOC_MAGIC);
LIMIT_CHECK_ALLOC_SIZE(newSize);
#endif

		register void * newBuf = MAC_REALLOCATE(buf, newSize);

#ifdef BUFFER_LIMIT_CHECK
SET_MARKERS((newBuf?newBuf:buf),newSize,OP_ALLOC_MAGIC);
#endif
		return newBuf;
	}
}

void CarbonFree(void *buf)
{	MEM_SUBROUTINE(777010)

CHECK_DELETE_PROTECTION(buf);
#ifdef BUFFER_LIMIT_CHECK
int bufsize = BUFFER_SIZE(buf);
CHECK_MARKERS(buf, OP_ALLOC_MAGIC);
#ifdef _DEADBEEF_ON_DISPOSE_
feebdaedset(((char*)buf)+LIMIT_CHECK_PRE_SIZE, bufsize);
#endif
#endif
	MAC_FREE(buf);
}

// added CarbonStrdup to avoid warnings with the perfectly legit op_strdup/op_free combo. Only used in debug.
char *CarbonStrdup(const char * src)
{
	size_t len = strlen(src) + 1;
	char * dest = (char *)CarbonMalloc(len);
	memcpy(dest, src, len);
	return dest;
}

#ifdef __MWERKS__
#pragma profile reset
#endif
