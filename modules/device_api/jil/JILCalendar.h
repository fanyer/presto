/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DEVICE_API_JIL_CALENDAR_ITEM_H
#define DEVICE_API_JIL_CALENDAR_ITEM_H

#ifdef DAPI_JIL_CALENDAR_SUPPORT

#include "modules/pi/device_api/OpCalendarService.h"
#include "modules/device_api/jil/utils.h"

#define EVENT_RECCURENCE_DAILY_STR (UNI_L("DAILY"))
#define EVENT_RECCURENCE_EVERY_WEEKDAY_STR (UNI_L("EVERY_WEEKDAY"))
#define EVENT_RECCURENCE_MONTHLY_ON_DAY_STR (UNI_L("MONTHLY_ON_DAY"))
#define EVENT_RECCURENCE_MONTHLY_ON_DAY_COUNT_STR (UNI_L("MONTHLY_ON_DAY_COUNT"))
#define EVENT_RECCURENCE_NOT_REPEAT_STR (UNI_L("NOT_REPEAT"))
#define EVENT_RECCURENCE_WEEKLY_ON_DAY_STR (UNI_L("WEEKLY_ON_DAY"))
#define EVENT_RECCURENCE_YEARLY_STR (UNI_L("YEARLY"))
#define EVENT_RECCURENCE_TYPES_STR (UNI_L("EventRecurrenceTypes"))

class JILCalendarItem
{
public:
	JILCalendarItem();
	static OP_STATUS Make(JILCalendarItem*& new_obj, JILCalendarItem* ref_obj);

	struct NullOrUndefStruct {
		unsigned int alarm_date:2;
		unsigned int start_date:2;
		unsigned int end_date:2;
		unsigned int alarmed:2;
		unsigned int name:2;
		unsigned int notes:2;
		unsigned int recurrence:2;
	} undefnull;

	double alarm_date;
	double start_date;
	double end_date;
	BOOL alarmed;
	OpString name;
	OpString notes;

	enum RecurrenceType
	{
		RECURRENCE_INVALID,
		RECURRENCE_DAILY,
		RECURRENCE_MONTHLY_ON_DAY,
		RECURRENCE_MONTHLY_ON_DAY_COUNT,
		RECURRENCE_NOT_REPEAT,
		RECURRENCE_EVERY_WEEKDAY,
		RECURRENCE_WEEKLY_ON_DAY,
		RECURRENCE_YEARLY
	};
	RecurrenceType recurrence;
	OpString id;
	static RecurrenceType GetRecurrenceType(const uni_char* str);
	static RecurrenceType GetRecurrenceType(const OpCalendarEvent::RecurrenceData& recurrence_data);
	static const uni_char* GetRecurrenceTypeStr(RecurrenceType recurrence);
	void ExportToRecurrenceData(OpCalendarEvent::RecurrenceData& recurrence_data);
		
	/**
	 * Exports data to newly created OpCalendarEvent for communication with platform.
	 *
	 * @param cal_evt pointer to a newly constructed OpCalendarEvent will be placed here.
	 */
	OP_STATUS ExportToOpCalendarEvent(OpCalendarEvent*& cal_evt);
	/**
	 * Imports the data from OpCalendarEvent object.
	 * NOTE: in case of failure there is no rollback and
	 * the data in this JILCalendarItem may be inconsistent.
	 * It is not an issue now as it is only used in Make and the
	 * whole object is scrapped if it fails
	 */
	OP_STATUS ImportFromOpCalendarEvent(const OpCalendarEvent* cal_evt);

	/** Checks that the item matches 'this' item(ie object on which the
	 * method is called is reference item)
	 *
	 * Matching is performed in accordance to the JIL specification.
	 * The reference item is the one specifying search criteria, i.e.
	 * it is the one that may contain wildcards in strings.
	 *
	 * The matching rules are as follows:
	 * - "*" matches any substring,
	 * - "\*" represents a literal "*",
	 * - no attribute value matches any value,
	 * - empty string in attribute matches only empty string.
	 *
	 * @param reference Specifes the search criteria (it doesn't get modified).
	 */
	OP_BOOLEAN IsMatch(OpCalendarEvent* item);
};

#endif // DAPI_JIL_CALENDAR_SUPPORT

#endif // DEVICE_API_JIL_CALENDAR_ITEM_H
