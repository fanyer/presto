/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

/** \file
 *
 * \brief Implement OpMemEvent and OpMemEventListener objects
 */

#include "core/pch.h"

#ifdef ENABLE_MEMORY_DEBUGGING

#include "modules/memory/src/memory_events.h"

OpMemEventListener::OpMemEventListener(UINT32 eventclasses) :
	eventclass_mask(eventclasses)
{
	Into(&g_mem_event_listeners);
}

OpMemEventListener::~OpMemEventListener(void)
{
	Out();
}

void OpMemEventListener::DeliverMemoryEvent(OpMemEvent* event)
{
	UINT32 cls = event->eventclass;
	OpMemEventListener* evl =
		(OpMemEventListener*)g_mem_event_listeners.First();

	while ( evl != 0 )
	{
		if ( evl->eventclass_mask & cls )
			evl->MemoryEvent(event);
		evl = (OpMemEventListener*)evl->Suc();
	}
}

#endif // ENABLE_MEMORY_DEBUGGING
