/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#if defined(_MAC_DEBUG)

#if defined(_AM_DEBUG)
#include "platforms/mac/debug/anders/settings.anders.h"
#elif defined(_MINCH_DEBUG)
#include "platforms/mac/debug/minch/settings.minch.h"
#endif

#include "platforms/mac/debug/debug_mem.h"
#include "modules/util/adt/opvector.h"

// --------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------

#ifdef BUFFER_PROTECTION

OpVector<void> protected_buffers;

void PlotProtected()
{
	int i;
	int count = protected_buffers.GetCount();
	for (i=0;i<count;i++)
	{
		Str8OnScreen(0,-(i+1)*8,kOnScreenRed,kOnScreenWhite,"0x%08X",protected_buffers.Get(i));
	}
	Str8OnScreen(0,-((i++)+1)*8,kOnScreenRed,kOnScreenWhite,"          ");
	Str8OnScreen(0,-((i++)+1)*8,kOnScreenRed,kOnScreenWhite,"          ");
	Str8OnScreen(0,-((i++)+1)*8,kOnScreenRed,kOnScreenWhite,"          ");
}

bool IsBufferProtected(void * buf)
{
	return (protected_buffers.Find(buf) >= 0);
}

bool ProtectBuffer(void * buf)
{
	//Stop in the debugger if deleted
	OP_ASSERT(!IsBufferProtected(buf));
	protected_buffers.Add(buf);
	PlotProtected();
}

void UnprotectBuffer(void * buf)
{
	//Call before it's meant to be deleted.
	INT32 index = protected_buffers.Find(buf);
	OP_ASSERT(index >= 0);
	if (index >= 0)
	{
		protected_buffers.Remove(index);
	}
	PlotProtected();
}

#endif

#pragma mark -

#ifdef BUFFER_LIMIT_CHECK
unsigned long gMemMarkerCount = 0;
unsigned long gMemMaxSize = 0;
#endif

#if defined(MAC_MEMORY_DEBUG)

OpVector<void> *buffers = NULL;//(nil, true);
OpVector<void> *tmp = NULL;
/*---*/

long gWidgetBufCount = 0;
long gWidgetMemUsed = 0;

void PlotAllocCount() {
#if defined(MAC_WIDGET)
#define kAllocCountPos 40
#else
#define kAllocCountPos 32
#endif
	DecOnScreen(-48,kAllocCountPos,kOnScreenBlack,kOnScreenWhite,gMemMarkerCount);
}

void WidgetPlotBufCount() {
	DecOnScreen(-48,0,kOnScreenBlack,kOnScreenWhite,gWidgetBufCount);
}

void WidgetPlotMemUsed() {
	Str8OnScreen(-64,8,kOnScreenBlack,kOnScreenWhite,"%8d",gWidgetMemUsed);
}

long gWidgetPrev = 0;
long gWidgetPrevPrev = 0;
long gWidgetPrevPrevPrev = 0;
void *WidgetMemNew(size_t size) {

	atexit(PlotBuffers);

	long sz = size - LIMIT_CHECK_OVERHEAD;
	long num = gMemMarkerCount;
// This code is to prevent infinite recusion.
// "buffers" itself is initialized with another allocator.
	if (!buffers)
	{
		buffers = (OpVector<void> *) 0xDEADBEEF;
		buffers = new OpVector<void>();
	}
	else if (((unsigned long)buffers) == 0xDEADBEEF)
	{
		return CarbonNew(size);
	}
// end magic

	gWidgetPrevPrevPrev = gWidgetPrevPrev;
	gWidgetPrevPrev = gWidgetPrev;
	gWidgetPrev = sz;
	void *result = CarbonNew(size);
	if (result) {
		tmp = buffers;
		buffers = (OpVector<void> *) 0xDEADBEEF;
		tmp->Add(result);
		buffers = tmp;
		gWidgetMemUsed += (size - LIMIT_CHECK_OVERHEAD);
		++gWidgetBufCount;
	}
	WidgetPlotBufCount();
	WidgetPlotMemUsed();
	PlotAllocCount();
	return result;
}

void WidgetMemDelete(void *buf) {
	if (buf) {
		if (((unsigned long)buffers) != 0xDEADBEEF) {
			tmp = buffers;
			buffers = (OpVector<void> *) 0xDEADBEEF;
			tmp->RemoveByItem(buf);
			buffers = tmp;
			--gWidgetBufCount;
			gWidgetMemUsed -= (BUFFER_SIZE(buf) - LIMIT_CHECK_OVERHEAD);
		}
	}
	CarbonDelete(buf);
	WidgetPlotBufCount();
	WidgetPlotMemUsed();
}

void *WidgetMemSetSize(void* buf, size_t newSize) {
	if (buf && ((unsigned long)buffers) != 0xDEADBEEF) {
		gWidgetMemUsed -= (BUFFER_SIZE(buf) - LIMIT_CHECK_OVERHEAD);
	}
	void *result = CarbonSetSize(buf, newSize);
	if (result) {
		if (((unsigned long)buffers) != 0xDEADBEEF) {
			tmp = buffers;
			buffers = (OpVector<void> *) 0xDEADBEEF;
			if (buf) {
				tmp->RemoveByItem(buf);
			} else {
				++gWidgetBufCount;
			}
			gWidgetMemUsed += (newSize - LIMIT_CHECK_OVERHEAD);
			tmp->Add(result);
			buffers = tmp;
		}
	} else if (((unsigned long)buffers) != 0xDEADBEEF) {
		gWidgetMemUsed += (BUFFER_SIZE(buf) - LIMIT_CHECK_OVERHEAD);
	}
	WidgetPlotBufCount();
	WidgetPlotMemUsed();
	return result;
}

/*---*/

long gAppBufCount = 0;
long gAppMemUsed = 0;

long gAppPrev = 0;
long gAppPrevPrev = 0;
long gAppPrevPrevPrev = 0;

void AppPlotBufCount() {
	DecOnScreen(-48,16,kOnScreenBlue,kOnScreenWhite,gAppBufCount);
}

void AppPlotMemUsed() {
	Str8OnScreen(-64,24,kOnScreenBlue,kOnScreenWhite,"%8d",gAppMemUsed);
}

void *AppMemNew(size_t size) {
	long sz = size - LIMIT_CHECK_OVERHEAD;
	long num = gMemMarkerCount;
// This code is to prevent infinite recusion.
// "buffers" itself is initialized with another allocator.
	if (!buffers)
	{
		buffers = (OpVector<void> *) 0xDEADBEEF;
		buffers = new OpVector<void>();
	}
	else if (((unsigned long)buffers) == 0xDEADBEEF)
	{
		return CarbonNew(size);
	}
// end magic

	gAppPrevPrevPrev = gAppPrevPrev;
	gAppPrevPrev = gAppPrev;
	gAppPrev = sz;
	void *result = CarbonNew(size);
	if (result) {
		tmp = buffers;
		buffers = (OpVector<void> *) 0xDEADBEEF;
		tmp->Add(result);
		buffers = tmp;
		gAppMemUsed += (size - LIMIT_CHECK_OVERHEAD);
		++gAppBufCount;
	}
	AppPlotBufCount();
	AppPlotMemUsed();
	PlotAllocCount();
	return result;
}

void AppMemDelete(void *buf) {
	if (buf) {
		if (((unsigned long)buffers) != 0xDEADBEEF) {
			tmp = buffers;
			buffers = (OpVector<void> *) 0xDEADBEEF;
			tmp->RemoveByItem(buf);
			buffers = tmp;
			--gAppBufCount;
			gAppMemUsed -= (BUFFER_SIZE(buf) - LIMIT_CHECK_OVERHEAD);
		}
	}
	CarbonDelete(buf);
	AppPlotBufCount();
	AppPlotMemUsed();
}

void *AppMemSetSize(void* buf, size_t newSize) {
	if (buf && ((unsigned long)buffers) != 0xDEADBEEF) {
		gAppMemUsed -= (BUFFER_SIZE(buf) - LIMIT_CHECK_OVERHEAD);
	}
	void *result = CarbonSetSize(buf, newSize);
	if (result) {
		if (((unsigned long)buffers) != 0xDEADBEEF) {
			tmp = buffers;
			buffers = (OpVector<void> *) 0xDEADBEEF;
			if (buf) {
				tmp->RemoveByItem(buf);
			} else {
				++gAppBufCount;
			}
			gAppMemUsed += (newSize - LIMIT_CHECK_OVERHEAD);
			tmp->Add(result);
			buffers = tmp;
		}
	} else if (((unsigned long)buffers) != 0xDEADBEEF) {
		gAppMemUsed += (BUFFER_SIZE(buf) - LIMIT_CHECK_OVERHEAD);
	}
	AppPlotBufCount();
	AppPlotMemUsed();
	return result;
}

/*---*/

#if defined(MAC_WIDGET)
#define kBufferListPos 100
#else
#define kBufferListPos 0
#endif

void PlotBuffers() {
	if (buffers) {
		void *buf;
		int i;
		int count = buffers->GetCount();
		for (i=0; i<count; i++) {
			buf = buffers->Get(i);
			long * b = (long*)(((char*)buf) - LIMIT_CHECK_PRE_SIZE);
			long len = (*b) - LIMIT_CHECK_OVERHEAD;	//this is the overhead from the first allocator (malloc/op_malloc/new)
			long num = *(b+1);
			DecOnScreen(kBufferListPos + 0,i*8,kOnScreenRed,kOnScreenWhite,num);
			DecOnScreen(kBufferListPos + 48,i*8,kOnScreenRed,kOnScreenWhite,len);
		}
	}
}

void CheckAllBuffers() {
	if (buffers) {
		void *buf;
		int i;
		int count = buffers->GetCount();
		for (i=0; i<count; i++) {
			CHECK_AND_LEAVE_MARKERS(buffers->Get(i));
		}
	}
}

#endif // MAC_MEMORY_DEBUG

#pragma mark -

void *zeroset(void *address, long byteCount)
{
	long longCount = byteCount / 4;
	long *longs = (long*) address;
	if (!address)
	{
		return NULL;
	}
	while (longCount--)
	{
		*longs++ = 0;
	}
	if (byteCount & 2)
	{
		short* shorts = (short*)longs;
		*shorts++ = 0;
		longs = (long*)shorts;
	}
	if (byteCount & 1)
	{
		*((char*)longs) = 0;
	}
	return address;
}

// --------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------

void *deadbeefset(void *address, long byteCount)
{
	long longCount = byteCount / 4;
	long *longs = (long*) address;
	if (!address)
	{
		return NULL;
	}
	while (longCount--)
	{
		*longs++ = 0xDEADBEEF;
	}
	if (byteCount & 2)
	{
		short* shorts = (short*)longs;
		*shorts++ = 0xDEAD;
		if (byteCount & 1)
		{
			*((char*)shorts) = 0xBE;
		}
	}
	else if (byteCount & 1)
	{
		*((char*)longs) = 0xDE;
	}
	return address;
}

// --------------------------------------------------------------------------------------------------------------
//
// --------------------------------------------------------------------------------------------------------------

void *feebdaedset(void *address, long byteCount)
{
	long longCount = byteCount / 4;
	long *longs = (long*) address;
	if (!address)
	{
		return NULL;
	}
	while (longCount--)
	{
		*longs++ = 0xFEEBDAED;
	}
	if (byteCount & 2)
	{
		short* shorts = (short*)longs;
		*shorts++ = 0xFEEB;
		if (byteCount & 1)
		{
			*((char*)shorts) = 0xDA;
		}
	}
	else if (byteCount & 1)
	{
		*((char*)longs) = 0xFE;
	}
	return address;
}


#endif
