/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "platforms/mac/pi/MacOpPlatformEvent.h"

#ifdef NS4P_COMPONENT_PLUGINS

//////////////////////////////////////////////////////////////////////////////////////////

MacOpPlatformEvent::MacOpPlatformEvent()
{
	
	
}

//////////////////////////////////////////////////////////////////////////////////////////

MacOpPlatformEvent::~MacOpPlatformEvent()
{
//	m_mac_shm.Detach();
}

//////////////////////////////////////////////////////////////////////////////////////////
/*
OP_STATUS MacOpPlatformEvent::AttachSharedMemory(OpPluginImageID key)
{
	return m_mac_shm.Attach(key);
}

//////////////////////////////////////////////////////////////////////////////////////////

void *MacOpPlatformEvent::GetSharedMemoryBuffer()
{
	return m_mac_shm.GetBuffer();
}
*/

#endif // NS4P_COMPONENT_PLUGINS

