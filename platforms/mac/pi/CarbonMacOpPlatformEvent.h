/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CARBONMACOPPLATFORMEVENT_H
#define CARBONMACOPPLATFORMEVENT_H

#ifdef NS4P_COMPONENT_PLUGINS

#ifndef SIXTY_FOUR_BIT

#include "platforms/mac/pi/MacOpPlatformEvent.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"
#include "modules/util/adt/opvector.h"

class CarbonMacOpPlatformEvent : public MacOpPlatformEvent
{
public:
	OpAutoVector<EventRecord>  m_events;
	
public:
	CarbonMacOpPlatformEvent() {}
	virtual ~CarbonMacOpPlatformEvent() {}
	
	// Implement MacOpPlatformEvent
	virtual int GetEventDataCount();
	virtual void* GetEventData(uint index) const;
	
	// Implement OpPlatformEvent
	virtual void* GetEventData() const;
};

#endif // !SIXTY_FOUR_BIT

#endif // NS4P_COMPONENT_PLUGINS

#endif //!CARBONMACOPPLATFORMEVENT_H
