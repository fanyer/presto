/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef COCOAMACOPPLATFORMEVENT_H
#define COCOAMACOPPLATFORMEVENT_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "platforms/mac/pi/MacOpPlatformEvent.h"
#include "modules/ns4plugins/src/plug-inc/npapi.h"

class CocoaMacOpPlatformEvent : public MacOpPlatformEvent
{
public:
	OpAutoVector<NPCocoaEvent> m_events;
	
public:
	CocoaMacOpPlatformEvent() {}
	virtual ~CocoaMacOpPlatformEvent() {}
	
	// Implement MacOpPlatformEvent
	virtual int GetEventDataCount();
	virtual void* GetEventData(uint index) const;

	// Implement OpPlatformEvent
	virtual void* GetEventData() const;
};

#endif // NS4P_COMPONENT_PLUGINS

#endif //!COCOAMACOPPLATFORMEVENT_H
