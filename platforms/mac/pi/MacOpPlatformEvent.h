/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MACOPPLATFORMEVENT_H
#define MACOPPLATFORMEVENT_H

#ifdef NS4P_COMPONENT_PLUGINS

#include "modules/pi/OpPluginImage.h"
//#include "platforms/mac/util/MacSharedMemory.h"
#include "modules/pi/OpPluginWindow.h"

class MacOpPlatformEvent : public OpPlatformEvent
{
public:
	MacOpPlatformEvent();
	virtual ~MacOpPlatformEvent();
	
//	OP_STATUS AttachSharedMemory(OpPluginImageID key);
//	void *GetSharedMemoryBuffer();
	
	virtual int GetEventDataCount() = 0;
	virtual void* GetEventData(uint index) const = 0;
	
	// Implement OpPlatformEvent
	virtual void* GetEventData() const = 0;
	
private:
//	MacSharedMemory m_mac_shm;
};

#endif // NS4P_COMPONENT_PLUGINS

#endif //!MACOPPLATFORMEVENT_H
