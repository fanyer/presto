/**
 * Copyright (C) 2011-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef UNIX_OPPLATFORMEVENT_H_
#define UNIX_OPPLATFORMEVENT_H_

#include "modules/pi/OpPluginWindow.h"

class UnixOpPlatformEvent : public OpPlatformEvent
{
public:
	UnixOpPlatformEvent(void* event);
	virtual ~UnixOpPlatformEvent() { }

	/// Get the platform specific event.
	virtual void* GetEventData() const;

private:
	void* m_event;
};

inline UnixOpPlatformEvent::UnixOpPlatformEvent(void* event) : m_event(event) { }

inline void* UnixOpPlatformEvent::GetEventData() const
{
	return m_event;
}

#endif  // UNIX_OPPLATFORMEVENT_H_
