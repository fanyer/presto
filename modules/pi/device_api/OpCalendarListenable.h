/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PI_DEVICE_API_OPCALENDARLISTENABLE_H
#define PI_DEVICE_API_OPCALENDARLISTENABLE_H

#ifdef PI_CALENDAR
#include "modules/device_api/OpListenable.h"
#include "modules/pi/device_api/OpCalendarService.h"


/** This class acts as splitter between OpCalendarService(which handles only one listener and CORE which needs many listeners)
 *
 * This class is implemented by core and NOT platforms
 */
class OpCalendarListenable : public OpCalendarServiceListener, public OpListenable<OpCalendarServiceListener>
{
public:
	OpCalendarListenable()
#ifdef _DEBUG
	: m_calendar_destroyed(FALSE)
#endif // _DEBUG
	{
	}

	~OpCalendarListenable()
	{
#ifdef _DEBUG
	  OP_ASSERT(m_calendar_destroyed || !"Listener destroyed before calendar was destroyed -> dangling pointer in calendar");
#endif // _DEBUG
	}
	virtual void OnEvent(OpCalendarService* calendar, OpCalendarEvent* calendar_event) { NotifyEvent(calendar, calendar_event); }
	virtual void OnCalendarDestroyed(OpCalendarService* calendar) {
		NotifyCalendarDestroyed(calendar);
#ifdef _DEBUG
		m_calendar_destroyed = TRUE;
#endif // _DEBUG
	}
private:
	/**
	 * Adds void NotifyEvent(OpCalendarService* cal, OpCalendarEvent* cal_evt); method which
	 * should be called by the platform when the calendar event is triggered
	 * che parameters are
	 *  - cal - pointer to OpCalenar object in which the event was triggered
	 *  - cal_evt - pointer to OpCalenarEvent object representing the event which was triggered
	 */
	DECLARE_LISTENABLE_EVENT2(OpCalendarServiceListener, Event, OpCalendarService*, OpCalendarEvent*);
	/**
	 * Adds void NotifyCalendarDestroyed(OpCalendarService* cal); method which
	 * should be called by the platform when the calendar is destroyed
	 * che parameters are
	 *  - cal - pointer to OpCalenar object in which the event was triggered
	 */
	DECLARE_LISTENABLE_EVENT1(OpCalendarServiceListener, CalendarDestroyed, OpCalendarService*);

#ifdef _DEBUG
	BOOL m_calendar_destroyed;
#endif // _DEBUG
};

#endif // PI_CALENDAR

#endif // PI_DEVICE_API_OPCALENDARLISTENABLE_H
