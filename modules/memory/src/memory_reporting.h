/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef MEMORY_REPORTING_H
#define MEMORY_REPORTING_H

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_events.h"

class OpMemReporting : public OpMemEventListener
{
public:
	OpMemReporting(void);
	virtual void MemoryEvent(OpMemEvent* event);

	void PrintStatistics(void);
	void SetMemoryMaxErrorCount(int value) { memory_max_error_count = value; }

	//
	// This will avoid getting leak-errors for this object.  We don't want
	// to destroy this object too early or else we will miss memory events.
	// FIXME: Actually destroy this object somewhere.
	//
	void* operator new(size_t size) OP_NOTHROW { return op_lowlevel_malloc(size); }
	void operator delete(void* ptr) { op_lowlevel_free(ptr); }

private:
	int event_count;
	int suppressed_event_count;
	int memory_max_error_count;

	void EventKeywords(char* buffer, OpMemEvent* event);
	void LogAllocate(OpMemEvent* event);
	void LogFree(OpMemEvent* event);
};

#endif // ENABLE_MEMORY_DEBUGGING

#endif // MEMORY_REPORTING_H
