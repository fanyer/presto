/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DAPI_JIL_CALENDAR_SUPPORT

#include "modules/device_api/jil/JILCalendar.h"

// this header will be moved to device_api
// TODO :change the include when it happens
#include "modules/device_api/WildcardCmp.h"
#include "modules/device_api/jil/utils.h"

JILCalendarItem::JILCalendarItem()
	: alarm_date(0.0)
	, start_date(OpDate::GetCurrentUTCTime())
	, end_date(0.0)
	, alarmed(FALSE)
{
	undefnull.alarm_date = IS_UNDEF;
	undefnull.alarmed = IS_UNDEF;
	undefnull.start_date = IS_VALUE;
	undefnull.end_date = IS_UNDEF;
	undefnull.name = IS_UNDEF;
	undefnull.notes = IS_UNDEF;
	undefnull.recurrence = IS_UNDEF;
}

/* static */ OP_STATUS
JILCalendarItem::Make(JILCalendarItem*& new_obj, JILCalendarItem* ref_obj)
{
	OpAutoPtr<JILCalendarItem> new_obj_deleter(OP_NEW(JILCalendarItem, ()));
	RETURN_OOM_IF_NULL(new_obj_deleter.get());
	new_obj_deleter->undefnull = ref_obj->undefnull;
	new_obj_deleter->alarm_date = ref_obj->alarm_date;
	new_obj_deleter->alarmed = ref_obj->alarmed;
	new_obj_deleter->start_date = ref_obj->start_date;
	new_obj_deleter->end_date = ref_obj->end_date;
	RETURN_IF_ERROR(new_obj_deleter->name.Set(ref_obj->name));
	RETURN_IF_ERROR(new_obj_deleter->notes.Set(ref_obj->notes));
	new_obj_deleter->recurrence = ref_obj->recurrence;
	RETURN_IF_ERROR(new_obj_deleter->id.Set(ref_obj->id));
	new_obj = new_obj_deleter.release();
	return OpStatus::OK;
}

/* static */ JILCalendarItem::RecurrenceType
JILCalendarItem::GetRecurrenceType(const uni_char* str)
{
	OP_ASSERT(str);
	if (uni_str_eq(EVENT_RECCURENCE_NOT_REPEAT_STR, str))
		return RECURRENCE_NOT_REPEAT;
	else if (uni_str_eq(EVENT_RECCURENCE_DAILY_STR, str))
		return RECURRENCE_DAILY;
	else if (uni_str_eq(EVENT_RECCURENCE_EVERY_WEEKDAY_STR, str))
		return RECURRENCE_EVERY_WEEKDAY;
	else if (uni_str_eq(EVENT_RECCURENCE_MONTHLY_ON_DAY_STR, str))
		return RECURRENCE_MONTHLY_ON_DAY;
	else if (uni_str_eq(EVENT_RECCURENCE_MONTHLY_ON_DAY_COUNT_STR, str))
		return RECURRENCE_MONTHLY_ON_DAY_COUNT;
	else if (uni_str_eq(EVENT_RECCURENCE_WEEKLY_ON_DAY_STR, str))
		return RECURRENCE_WEEKLY_ON_DAY;
	else if (uni_str_eq(EVENT_RECCURENCE_YEARLY_STR, str))
		return RECURRENCE_YEARLY;
	else
		return RECURRENCE_INVALID;
}

/* static */ JILCalendarItem::RecurrenceType
JILCalendarItem::GetRecurrenceType(const OpCalendarEvent::RecurrenceData& recurrence_data)
{
	switch (recurrence_data.type)
	{
	case OpCalendarEvent::SINGLE_SHOT:
		return JILCalendarItem::RECURRENCE_NOT_REPEAT;
	case OpCalendarEvent::YEARLY_ON_DATE:
		return JILCalendarItem::RECURRENCE_YEARLY;
	case OpCalendarEvent::YEARLY_ON_DAY:
		return JILCalendarItem::RECURRENCE_INVALID; // not mappable to jil recurrence
	case OpCalendarEvent::YEARLY_ON_WEEKDAY:
		return JILCalendarItem::RECURRENCE_INVALID; // not mappable to jil recurrence
	case OpCalendarEvent::MONTHLY_ON_DAY:
		return JILCalendarItem::RECURRENCE_MONTHLY_ON_DAY_COUNT;
	case OpCalendarEvent::MONTHLY_ON_WEEKDAY:
		return JILCalendarItem::RECURRENCE_MONTHLY_ON_DAY;
	case OpCalendarEvent::WEEKLY:
		switch (recurrence_data.weekly.weekday_mask)
		{
		case OpCalendarEvent::ALL_WEEK:
			return JILCalendarItem::RECURRENCE_DAILY;
		case OpCalendarEvent::WEEKDAYS:
			return JILCalendarItem::RECURRENCE_EVERY_WEEKDAY;
		case OpCalendarEvent::SUNDAY:
		case OpCalendarEvent::MONDAY:
		case OpCalendarEvent::TUESDAY:
		case OpCalendarEvent::WEDNSDAY:
		case OpCalendarEvent::THURSDAY:
		case OpCalendarEvent::FRIDAY:
		case OpCalendarEvent::SATURDAY:
			return JILCalendarItem::RECURRENCE_WEEKLY_ON_DAY;
		default:
			return JILCalendarItem::RECURRENCE_INVALID; // not mappable to jil recurrence
		}
	default:
		OP_ASSERT(!"Unknown recurrence type"); // intentional fall through
	case OpCalendarEvent::INTERVAL:
		return JILCalendarItem::RECURRENCE_INVALID;	// not mappable to jil recurrence
	}
}

/* static */ const uni_char*
JILCalendarItem::GetRecurrenceTypeStr(RecurrenceType recurrence)
{
	switch (recurrence)
	{
	case RECURRENCE_DAILY:
		return EVENT_RECCURENCE_DAILY_STR;
	case RECURRENCE_MONTHLY_ON_DAY:
		return EVENT_RECCURENCE_MONTHLY_ON_DAY_STR;
	case RECURRENCE_MONTHLY_ON_DAY_COUNT:
		return EVENT_RECCURENCE_MONTHLY_ON_DAY_COUNT_STR;
	case RECURRENCE_NOT_REPEAT:
		return EVENT_RECCURENCE_NOT_REPEAT_STR;
	case RECURRENCE_EVERY_WEEKDAY:
		return EVENT_RECCURENCE_EVERY_WEEKDAY_STR;
	case RECURRENCE_WEEKLY_ON_DAY:
		return EVENT_RECCURENCE_WEEKLY_ON_DAY_STR;
	case RECURRENCE_YEARLY:
		return EVENT_RECCURENCE_YEARLY_STR;
	}
	OP_ASSERT(FALSE);
	return NULL;
}

void
JILCalendarItem::ExportToRecurrenceData(OpCalendarEvent::RecurrenceData& recurrence_data)
{
	RecurrenceType effective_recurrence = undefnull.recurrence == IS_VALUE ? recurrence : RECURRENCE_NOT_REPEAT;
	double start_date_adjusted = start_date + OpDate::LocalTZA() + OpDate::DaylightSavingTA(start_date);

	switch (effective_recurrence)
	{
	case RECURRENCE_NOT_REPEAT:
		recurrence_data.type = OpCalendarEvent::SINGLE_SHOT;
		recurrence_data.single_shot.day = OpDate::DateFromTime(start_date_adjusted);
		// Months are 0 based - JAN = 0, FEB = 1 ... as in OpDate
		recurrence_data.single_shot.month = OpDate::MonthFromTime(start_date_adjusted);
		// Years are 1 based ie. 1999 A.D. = 1999  as in OpDate
		recurrence_data.single_shot.year = OpDate::YearFromTime(start_date_adjusted);
		break;
	case RECURRENCE_DAILY:
		recurrence_data.type = OpCalendarEvent::WEEKLY;
		recurrence_data.weekly.weekday_mask = OpCalendarEvent::ALL_WEEK;
		break;
	case RECURRENCE_WEEKLY_ON_DAY:
		recurrence_data.type = OpCalendarEvent::WEEKLY;
		// weekdays are represented as bits ordered from Sunday to Saturday 
		// ie Sun = 0x01, Mon = 0x02, Tue = 0x04
		recurrence_data.weekly.weekday_mask = 0x01 << OpDate::WeekDay(start_date_adjusted);
		break;
	case RECURRENCE_MONTHLY_ON_DAY:
		recurrence_data.type = OpCalendarEvent::MONTHLY_ON_WEEKDAY;
		recurrence_data.monthly_on_weekday.weekday = 0x01 << OpDate::WeekDay(start_date_adjusted);
		recurrence_data.monthly_on_weekday.week_number = static_cast<signed char>(op_floor((OpDate::DateFromTime(start_date_adjusted) - 1)  / 7));
		break;
	case RECURRENCE_MONTHLY_ON_DAY_COUNT:
		recurrence_data.type = OpCalendarEvent::MONTHLY_ON_DAY;
		recurrence_data.monthly_on_day.day_number = OpDate::DateFromTime(start_date_adjusted);
		break;
	case RECURRENCE_EVERY_WEEKDAY:
		recurrence_data.type = OpCalendarEvent::WEEKLY;
		recurrence_data.weekly.weekday_mask = OpCalendarEvent::WEEKDAYS;
		break;
	case RECURRENCE_YEARLY:
		recurrence_data.type = OpCalendarEvent::YEARLY_ON_DATE;
		recurrence_data.yearly_on_date.day_of_the_month = OpDate::DateFromTime(start_date_adjusted);
		recurrence_data.yearly_on_date.month = OpDate::MonthFromTime(start_date_adjusted);
		break;
	default:
		OP_ASSERT(FALSE);
	}
	recurrence_data.time_of_day = OpDate::TimeWithinDay(start_date_adjusted);
}

OP_STATUS
JILCalendarItem::ExportToOpCalendarEvent(OpCalendarEvent*& cal_evt)
{
	OpAutoPtr<OpCalendarEvent> cal_evt_ap(OP_NEW(OpCalendarEvent, ()));

	if (undefnull.alarmed == IS_VALUE  && alarmed && undefnull.alarm_date != IS_VALUE)
		return OpStatus::ERR; // if alarm_date not set and alarmed is true we should throw an exception
	if (undefnull.start_date != IS_VALUE)
		return OpStatus::ERR; // start time has to be defined

	RETURN_OOM_IF_NULL(cal_evt_ap.get());

	RETURN_IF_ERROR(cal_evt_ap->id.Set(id.CStr()));

	cal_evt_ap->start_date = start_date;
	cal_evt_ap->alarmed = undefnull.alarmed == IS_VALUE && alarmed;

	if (undefnull.notes == IS_VALUE)
		RETURN_IF_ERROR(cal_evt_ap->description.Set(notes));
	if (undefnull.name == IS_VALUE)
		RETURN_IF_ERROR(cal_evt_ap->name.Set(name));

	if (undefnull.start_date == IS_VALUE
	 && undefnull.end_date == IS_VALUE
	 && start_date <= end_date)
		cal_evt_ap->duration = end_date - start_date;
	else
		cal_evt_ap->duration = 0;

	if (undefnull.start_date == IS_VALUE
	 && undefnull.alarm_date == IS_VALUE)
		cal_evt_ap->reminder_time_offset = start_date - alarm_date;
	else
		cal_evt_ap->reminder_time_offset = 0;

	ExportToRecurrenceData(cal_evt_ap->recurrence_data);

	cal_evt = cal_evt_ap.release();
	return OpStatus::OK;
}

OP_STATUS
JILCalendarItem::ImportFromOpCalendarEvent(const OpCalendarEvent* cal_evt)
{
	OP_ASSERT(cal_evt);

	RETURN_IF_ERROR(id.Set(cal_evt->id));
	undefnull.alarmed = IS_VALUE;
	alarmed = cal_evt->alarmed;
	RETURN_IF_ERROR(notes.Set(cal_evt->description));
	undefnull.notes = cal_evt->description.CStr() ? IS_VALUE : IS_NULL;
	RETURN_IF_ERROR(name.Set(cal_evt->name));
	undefnull.name = cal_evt->name.CStr() ? IS_VALUE : IS_NULL;

	undefnull.start_date = IS_VALUE;
	start_date = cal_evt->start_date;
	undefnull.end_date = IS_VALUE;
	end_date = (cal_evt->start_date + cal_evt->duration);
	undefnull.alarm_date = IS_VALUE;
	alarm_date = (cal_evt->start_date - cal_evt->reminder_time_offset);

	undefnull.recurrence = IS_VALUE;
	recurrence = JILCalendarItem::GetRecurrenceType(cal_evt->recurrence_data);
	if (recurrence == JILCalendarItem::RECURRENCE_INVALID)
		return OpStatus::ERR; // not mappable to jil recurrence
	return OpStatus::OK;
}

OP_BOOLEAN
JILCalendarItem::IsMatch(OpCalendarEvent* item)
{
	OP_ASSERT(item);

	if (undefnull.alarmed == IS_VALUE && item->alarmed != alarmed)
		return OpBoolean::IS_FALSE;

	if (undefnull.alarm_date == IS_VALUE && item->start_date - item->reminder_time_offset != alarm_date) // alarmDate
		return OpBoolean::IS_FALSE;

	if (undefnull.recurrence == IS_VALUE && recurrence != JILCalendarItem::GetRecurrenceType(item->recurrence_data))
		return OpBoolean::IS_FALSE;

	OP_BOOLEAN match_result = undefnull.name == IS_VALUE ? WildcardCmp(name.CStr(), item->name.CStr() ? item->name.CStr() : UNI_L(""), FALSE) : OpBoolean::IS_TRUE;
	if (match_result != OpBoolean::IS_TRUE)
		return match_result;
	match_result = undefnull.notes == IS_VALUE ? WildcardCmp(notes.CStr(), item->description.CStr() ? item->description.CStr() : UNI_L(""), FALSE) : OpBoolean::IS_TRUE;
	if (match_result != OpBoolean::IS_TRUE)
		return match_result;
	return OpBoolean::IS_TRUE;
}


#endif // DAPI_JIL_CALENDAR_SUPPORT
