/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "platforms/mac/pi/CocoaMacOpPlatformEvent.h"

#ifdef NS4P_COMPONENT_PLUGINS

//////////////////////////////////////////////////////////////////////////////////////////

int CocoaMacOpPlatformEvent::GetEventDataCount()
{
	return m_events.GetCount();
}

//////////////////////////////////////////////////////////////////////////////////////////

void *CocoaMacOpPlatformEvent::GetEventData(uint index) const
{
	OP_ASSERT(index < m_events.GetCount()); 
	OP_ASSERT(m_events.Get(index) != NULL); 
	
	return m_events.Get(index);
}

//////////////////////////////////////////////////////////////////////////////////////////

void *CocoaMacOpPlatformEvent::GetEventData() const
{
	return GetEventData(0);
}

#endif // NS4P_COMPONENT_PLUGINS
