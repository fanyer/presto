/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef NS4P_COMPONENT_PLUGINS

#ifndef SIXTY_FOUR_BIT

#include "platforms/mac/pi/CarbonMacOpPlatformEvent.h"

//////////////////////////////////////////////////////////////////////////////////////////

int CarbonMacOpPlatformEvent::GetEventDataCount()
{
	return m_events.GetCount();
}

//////////////////////////////////////////////////////////////////////////////////////////

void *CarbonMacOpPlatformEvent::GetEventData(uint index) const
{
	OP_ASSERT(index < m_events.GetCount()); 
	OP_ASSERT(m_events.Get(index) != NULL); 
	
	return m_events.Get(index);
}

//////////////////////////////////////////////////////////////////////////////////////////

void *CarbonMacOpPlatformEvent::GetEventData() const
{
	return GetEventData(0);
}

#endif // !SIXTY_FOUR_BIT

#endif // NS4P_COMPONENT_PLUGINS
