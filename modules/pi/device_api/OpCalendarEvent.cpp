		/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "modules/pi/device_api/OpCalendarService.h"

#ifdef PI_CALENDAR

/* static */ OP_STATUS
OpCalendarEvent::Make(OpCalendarEvent** new_calendar_event, const OpCalendarEvent* to_clone)
{
	OP_ASSERT(new_calendar_event);
	*new_calendar_event = OP_NEW(OpCalendarEvent, ());
	RETURN_OOM_IF_NULL(*new_calendar_event);
	if (to_clone)
	{
		OpAutoPtr<OpCalendarEvent> new_calendar_event_deleter(*new_calendar_event);
		(*new_calendar_event)->alarmed = to_clone->alarmed;
		RETURN_IF_ERROR((*new_calendar_event)->description.Set(to_clone->description));
		RETURN_IF_ERROR((*new_calendar_event)->name.Set(to_clone->name));
		RETURN_IF_ERROR((*new_calendar_event)->id.Set(to_clone->id));
		(*new_calendar_event)->duration = to_clone->duration;
		(*new_calendar_event)->start_date = to_clone->start_date;
		(*new_calendar_event)->recurrence_data = to_clone->recurrence_data;
		(*new_calendar_event)->reminder_time_offset = to_clone->reminder_time_offset;
		new_calendar_event_deleter.release();
	}
	return OpStatus::OK;
}

#endif // PI_CALENDAR
