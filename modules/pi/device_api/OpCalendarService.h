		/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef PI_DEVICE_API_OPCALENDAR_H
#define PI_DEVICE_API_OPCALENDAR_H

#ifdef PI_CALENDAR
#include "modules/stdlib/util/opdate.h"
#include "modules/device_api/OpListenable.h"

class OpCalendarService;

/** Data structure describing an event in calendar
 * with some utility mthods to operate on it.
 *
 * It is meant for communication with OpCalendar, it is NOT
 * implemented by platform
 */
class OpCalendarEvent
{
public:
	/// Enumerated models of reccurence
	enum RecurrenceType
	{
		SINGLE_SHOT,		//< no recurring. specified as - d/m/y (ie. on 2nd of may 2012)
		YEARLY_ON_DATE,		//< on specified date every year. specified as d/m (ie. every 2nd of may month)
		YEARLY_ON_DAY,		//< on specified day every year. specified as d (ie. every 42nd day of the month)
		YEARLY_ON_WEEKDAY,	//< on specified n-th weekday every year. specified as nth/weekday(ie. every 2nd wednsday of the year)
		MONTHLY_ON_DAY,		//< on specified days every month. specified as day (ie. every 2nd day of the month)
		MONTHLY_ON_WEEKDAY, //< on specified weekday every month. specified as nth/weekday(ie. every 2nd wednsday of the month)
		WEEKLY,				//< on specified days every week. specified as weekday mask (ie. every wednsday and friday)
		INTERVAL,			//< every specified amount of seconds (ie. every 3600 secs)
	};

	typedef char Month;
	/// Enumerated months
	/// Months are 0 based - JAN = 0, FEB = 1 ... (as in OpDate)
	enum Months
	{
		JANUARY = 0, FEBRUARY, MARCH, APRIL, MAY, JUNE, JULY, AUGUST, SEPTEMBER, OCTOBER, NOVEMBER, DECEMBER
	};

	typedef char Weekday;
	/// Enumerated weekday masks
	/// weekdays are represented as bits ordered from Sunday to Saturday (as in OpDate)
	/// ie Sun = 0x01, Mon = 0x02, Tue = 0x04
	enum Weekdays
	{
		SUNDAY = 0x01,
		MONDAY = 0x02,
		TUESDAY = 0x04,
		WEDNSDAY = 0x08,
		THURSDAY = 0x10,
		FRIDAY = 0x20,
		SATURDAY = 0x40,
		WEEKDAYS = MONDAY | TUESDAY | WEDNSDAY | THURSDAY | FRIDAY,
		WEEKEND = SUNDAY | SATURDAY,
		ALL_WEEK = WEEKDAYS | WEEKEND,
	};

	///  All years are 1 based ie. 1999 A.D. = 1999 (as in OpDate)

	/// Description of recurrence pattern of an event.
	struct RecurrenceData
	{
		RecurrenceType type; //< type of recurrence
		union
		{
			/// Data for SINGLE_SHOT recurrence type
			struct
			{
				UINT16 year;
				Month month;
				unsigned char day;
			} single_shot;

			/// Data for YEARLY_ON_DATE recurrence type
			struct
			{
				Month month;
				signed char day_of_the_month;
			} yearly_on_date;

			/// Data for YEARLY_ON_DAY recurrence type
			struct
			{
				INT32 day_of_the_year; // if negative then treat it as countng from the last day of year( ie. -1 means last day of year, -2 means day 30th of december etc)
			} yearly_on_day;

			/// Data for YEARLY_ON_WEEKDAY recurrence type
			struct
			{
				signed char week_number; // if negative then count form the end
				Weekday weekday;		 // only one day(only one bt can be set in the mask)
			} yearly_on_weekday;

			/// Data for MONTHLY_ON_DAY recurrence type
			struct
			{
				INT32 day_number; //  if negative then count form the end
			} monthly_on_day;

			/// Data for MONTHLY_ON_WEEKDAY recurrence type
			struct
			{
				signed char week_number; // if negative then count form the end
				Weekday weekday;	     // only one day(not a mask)
			} monthly_on_weekday;

			/// Data for WEEKLY recurrence type
			struct
			{
				Weekday weekday_mask; //
			} weekly;

			/// Data for INTERVAL recurrence type
			struct
			{
				UINT32 seconds;
			} interval;
		};
		/// Time of day at which the event starts
		double time_of_day;
	};

	/** Unique platform id of the event. This will generally be set only for
	 * the events retrieved from the platform. OpCalendarEvent-s which havent been
	 * added to the platform calendar will have an empty id.
	 */
	OpString id;
	OpString name;       //< name of the event
	OpString description;//< description of the event
	double start_date;   //< UTC millisecond timestamp of a date at which the event starts occurring
	double duration;     //< duration of the event in milliseconds

	BOOL alarmed;        //< set to TRUE if the alarm is sounded when the event occurs

	/** time(in milliseconds) in relation to event start time when
	 *  the alarm should be sounded. Positive reminder_time_offset means
	 *  before the event, and negative after the event
	 */
	double reminder_time_offset;

	RecurrenceData recurrence_data; //< recurring pattern description

	/** Constructs new OpCalendarEvent, optionally taking another event and cloning it
	 *
	 * @param[out] new_calendar_event newly allocated calendar event will be placed here
	 * @param to_clone optional. If not NULL then newly created event will be clone of this event.
	 * @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY if failed
	 */
	static OP_STATUS Make(OpCalendarEvent** new_calendar_event, const OpCalendarEvent* to_clone = 0);
};

/** Callback class interface for OpCalendar::Iterate and OpCalendar::GetEvent.
 */
class OpCalendarIterationCallback
{
public:
	virtual ~OpCalendarIterationCallback(){}
	/** This method is called when iterate operation has a result ready.
	 *
	 * @param result - result of a search is stored here. This value is
	 * allocated by platform(using OP_NEW) and ownership of it is handed over to the
	 * callback, so it needs to be freed (with OP_DELETE).
	 *
	 * @return TRUE if the callback wants more results or FALSE if it wants to
	 * stop the search. In case it returns FALSE platform code MUST NOT call
	 * this (or OnFindFinished/Failed) callback again!
	 */
	virtual BOOL OnItem(OpCalendarEvent* result) = 0;

	/** Called when find operation finishes successfully.
	 */
	virtual void OnFinished() = 0;

	/** Called when the find operation fails. This finishes iteration
	 * so no OnFinished will be called after it.
	 *
	 * @param error error code.
	 *        - ERR_NO_MEMORY - in case of oom
	 *        - ERR_NO_ACCESS - if we dont have acccess to calendar book
	 *        - ERR - any other error
	 */
	virtual void OnError(OP_STATUS error) = 0;
};

/** Callback class interface for modyfying calendar operations(add/delete/update).
 */
class OpCalendarEventModificationCallback
{
public:
	virtual ~OpCalendarEventModificationCallback(){}
	/** Called when modification operation finished successfully
	 *
	 * @param id id of the modified event
	 */
	virtual void OnSuccess(const uni_char* id) = 0;
	/** Called when modification operation fails. If this callback is called
	 * then this means that no changes should have ocurred in platform calendar
	 *
	 * @param id id of the modified event. For add operation this is always NULL.
	 * @param error error code
	 *        - ERR_NO_MEMORY - in case of oom
	 *        - ERR_NO_ACCESS - if we dont have acccess to calendar book
	 *        - ERR_NO_SUCH_RESOURCE - if the event with a given id does not evist in the calendar.
	 *        - ERR - any other error
	 */
	virtual void OnFailed(const uni_char* id, OP_STATUS error) = 0;
};

/** Callback class interface for OpCalendar::GetCalendarCount.
 */
class OpGetCalendarEventCountCallback
{
public:
	virtual ~OpGetCalendarEventCountCallback(){}
	/** Called when count operation finished successfully
	 *
	 * @param count number of events in calendar
	 */
	virtual void OnSuccess(UINT32 count) = 0;
	/** Called when count operation fails.
	 *
	 * @param error error code
	 *        - ERR_NO_MEMORY - in case of oom
	 *        - ERR_NO_ACCESS - if we dont have acccess to calendar book
	 *        - ERR - any other error
	 */
	virtual void OnFailed(OP_STATUS error) = 0;
};

/** Interface for OpCalendar listener object
 */
class OpCalendarServiceListener
{
public:
	virtual ~OpCalendarServiceListener(){}
	/** Called when an event in calendar occurs
	 *
	 * @param calendar calendar which notifies about an event
	 * @param calendar_event Data for event that occured. This pointer is valid only
	 *        during call to this event and SHOULD NOT be saved by client. If there is
	 *        a need to save this data, a copy should be made using OpCalendarEvent::Make
	 */
	virtual void OnEvent(OpCalendarService* calendar, OpCalendarEvent* calendar_event) = 0;

	/** Called when an calendar is destroyed.
	 *
	 * @param calendar calendar which notifies it's destruction.
	 */
	virtual void OnCalendarDestroyed(OpCalendarService* calendar) = 0;
};

class OpCalendarService
{
public:
	virtual ~OpCalendarService(){}

	/** Constructs OpCalendarService.
	 *
	 * @param[out] new_calendar Set to the newly created calendar service object.
	 *             The caller is responsible for deleting it with OP_DELETE
	 * @param[in] listener Optional listener object which will receive notifications
	 *              about ocurring calendar events.
	 *
	 * @return
	 *	- OpStatus::OK
	 *	- OpStatus::ERR_NO_MEMORY
	 *	- OpStatus::ERR_NOT_SUPPORTED
	 */
	static OP_STATUS Create(OpCalendarService** new_calendar, OpCalendarServiceListener* listener);

	/** Calls callback for each event in the calendar which happens in a given period of time.
	 * (Iteration can be stopped early by returning FALSE from OnItem)
	 *
	 * Only events that start at or after the start_time and end at or after the end_time should
	 * be provided.
	 *
	 * @param[in] callback Callback object which is called when operation finishes
	 *            or fails. Can be called synchronously (during execution of this function)
	 *            or asynchronously. MUST NOT be NULL.
	 * @param[in] start_time UTC milisecond timestamp of the start of time period to search for events.
	 *                     If it is equal -infinity then the time period is unbounded on the left side.
	 * @param[in] end_time UTC milisecond timestamp of the end of time period to search for events
	 *                     If it is equal +infinity then the time period is unbounded on the right side.
	 */
	virtual void Iterate(OpCalendarIterationCallback* callback, double start_time, double end_time) = 0;

	/** Retrieves OpCalendarEvent with a given id.
	 *
	 * Find result will be passed through callback. If the event is found then
	 * OnItem shall be called one time and if it returns TRUE then it will be
	 * followed by OnFinished. If the event is not found then only OnFinished
	 * will be called.
	 * If any errors occur during search then OnError shall be called with
	 * relevant error code.
	 *
	 * @param[in] callback Callback object which is called when operation finishes
	 *            or fails. Can be called synchronously (during execution of this function)
	 *            or asynchronously. MUST NOT be NULL.
	 * @param calendar_event_id ID of the event to be retrieved.
	 */
	virtual void GetEvent(OpCalendarIterationCallback* callback, const uni_char* calendar_event_id) = 0;

	/** Gets number of events in this calendar
	 *
	 * @param[in] callback Callback object which is called when operation finishes
	 *            or fails. Can be called synchronously (during execution of this function)
	 *            or asynchronously. MUST NOT be NULL.
	 */
	virtual void GetEventCount(OpGetCalendarEventCountCallback* callback) = 0;

	/** Adds an OpCalendarEvent to the calendar.
	 *
	 * The event being added must be new (i.e. not yet registered with the
	 * calendar).
	 *
	 * @param calendar_event Event to add, cannot be NULL.
	 * @param[in] callback Callback object which is called when operation finishes
	 *            or fails. Can be called synchronously (during execution of this function)
	 *            or asynchronously.
	 */
	virtual void AddEvent(OpCalendarEventModificationCallback* callback, OpCalendarEvent* calendar_event) = 0;

	/** Removes OpCalendarEvent from the calendar
	 *
	 * @param[in] calendar_event_id ID of the calendar event to remove. MUST NOT be NULL
	 * @param[in] callback Callback object which is called when operation finishes
	 *            or fails. Can be called synchronously (during execution of this function)
	 *            or asynchronously.
	 */
	virtual void RemoveEvent(OpCalendarEventModificationCallback* callback, const uni_char* calendar_event_id) = 0;
	/** Commits changes to an OpCalendarEvent to the calendar.
	 *
	 * @param[in] calendar_event Item to commit. Cannot be NULL. The item must be item already
	 *            added to the calendar (i.e. has non-empty id)
	 * @param[in] callback Callback object which is called when operation finishes
	 *            or fails. Can be called synchronously (during execution of this function)
	 *            or asynchronously.
	 */
	virtual void CommitEvent(OpCalendarEventModificationCallback* callback, OpCalendarEvent* calendar_event) = 0;
};

#endif // PI_CALENDAR

#endif // PI_DEVICE_API_OPCALENDAR_H
