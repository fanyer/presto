/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilcalendaritem.h"
#include "modules/dom/src/domjil/domjilpim.h"
#include "modules/dom/src/domjil/utils/jilutils.h"
#include "modules/stdlib/util/opdate.h"

DOM_JILCalendarItem::DOM_JILCalendarItem()
	: m_calendar_event()
	, m_start_date(NULL)
	, m_end_date(NULL)
	, m_alarm_date(NULL)
{
}

DOM_JILCalendarItem::~DOM_JILCalendarItem()
{
}

/* virtual */ void
DOM_JILCalendarItem::GCTrace()
{
	GCMark(m_start_date);
	GCMark(m_end_date);
	GCMark(m_alarm_date);
}

/* static */ OP_STATUS
DOM_JILCalendarItem::Make(DOM_JILCalendarItem*& new_obj, OpCalendarEvent* calendar_event, DOM_Runtime* origining_runtime)
{
	new_obj = OP_NEW(DOM_JILCalendarItem, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::JIL_CALENDARITEM_PROTOTYPE), "CalendarItem"));
	if (calendar_event)
		RETURN_IF_ERROR(new_obj->GetCalendarItem()->ImportFromOpCalendarEvent(calendar_event));

	if (new_obj->GetCalendarItem()->undefnull.start_date == IS_VALUE)
	{
		ES_Value val;
		RETURN_IF_ERROR(DOMSetDate(&val, origining_runtime, new_obj->GetCalendarItem()->start_date));
		new_obj->m_start_date = val.value.object;
	}

	if (new_obj->GetCalendarItem()->undefnull.end_date == IS_VALUE)
	{
		ES_Value val;
		RETURN_IF_ERROR(DOMSetDate(&val, origining_runtime, new_obj->GetCalendarItem()->end_date));
		new_obj->m_end_date = val.value.object;
	}

	if (new_obj->GetCalendarItem()->undefnull.alarm_date == IS_VALUE)
	{
		ES_Value val;
		RETURN_IF_ERROR(DOMSetDate(&val, origining_runtime, new_obj->GetCalendarItem()->alarm_date));
		new_obj->m_alarm_date = val.value.object;
	}

	return OpStatus::OK;
}

/* virtual */ ES_PutState
DOM_JILCalendarItem::InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_calendarItemId:
			return PUT_SUCCESS;
		JIL_PUT_BOOLEAN(OP_ATOM_alarmed,  m_calendar_event.alarmed,  m_calendar_event.undefnull.alarmed);
		JIL_PUT_DATE(OP_ATOM_eventStartTime, m_start_date);
		JIL_PUT_DATE(OP_ATOM_eventEndTime, m_end_date);
		JIL_PUT_DATE(OP_ATOM_alarmDate, m_alarm_date);
		JIL_PUT_STRING(OP_ATOM_eventName,   m_calendar_event.name,  m_calendar_event.undefnull.name);
		JIL_PUT_STRING(OP_ATOM_eventNotes,   m_calendar_event.notes,  m_calendar_event.undefnull.notes);
		case OP_ATOM_eventRecurrence:
		{
			switch (value->type)
			{
			case VALUE_NULL:
				m_calendar_event.undefnull.recurrence = IS_NULL;
				m_calendar_event.recurrence = JILCalendarItem::RECURRENCE_INVALID;
				break;
			case VALUE_UNDEFINED:
				m_calendar_event.undefnull.recurrence = IS_UNDEF;
				m_calendar_event.recurrence = JILCalendarItem::RECURRENCE_INVALID;
				break;
			case VALUE_STRING:
				{
				JILCalendarItem::RecurrenceType recurrence = JILCalendarItem::GetRecurrenceType(value->value.string);
				if (recurrence != JILCalendarItem::RECURRENCE_INVALID)
				{
					m_calendar_event.undefnull.recurrence = IS_VALUE;
					m_calendar_event.recurrence = recurrence;
				}
				break;
				}
			default:
				return PUT_NEEDS_STRING;
			}
			return PUT_SUCCESS;
		}
	}
	return PUT_FAILED;
}

/* virtual */ ES_GetState
DOM_JILCalendarItem::InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value)
{
	switch (property_atom)
	{
		case OP_ATOM_calendarItemId:
			if (m_calendar_event.id.CStr())
				DOMSetString(value, m_calendar_event.id.CStr());
			// else leave it undefined
			return GET_SUCCESS;
		JIL_GET_BOOLEAN(OP_ATOM_alarmed,  m_calendar_event.alarmed,  m_calendar_event.undefnull.alarmed);
		case OP_ATOM_eventStartTime:
			DOMSetObject(value, m_start_date);
			return GET_SUCCESS;
		case OP_ATOM_eventEndTime:
			DOMSetObject(value, m_end_date);
			return GET_SUCCESS;
		case OP_ATOM_alarmDate:
			DOMSetObject(value, m_alarm_date);
			return GET_SUCCESS;
		JIL_GET_STRING(OP_ATOM_eventName,  m_calendar_event.name,  m_calendar_event.undefnull.name);
		JIL_GET_STRING(OP_ATOM_eventNotes,  m_calendar_event.notes,  m_calendar_event.undefnull.notes);
		case OP_ATOM_eventRecurrence:
			switch (m_calendar_event.undefnull.recurrence)
			{
				case IS_VALUE: DOMSetString(value, JILCalendarItem::GetRecurrenceTypeStr(m_calendar_event.recurrence)); break;
				case IS_NULL: DOMSetNull(value); break;
				case IS_UNDEF: DOMSetUndefined(value); break;
				default: OP_ASSERT(FALSE);
			}
			return GET_SUCCESS;
	}
	return GET_FAILED;
}

OP_STATUS
DOM_JILCalendarItem::PushDatesToCalendarItem()
{
	ES_Value val;
	if (m_start_date)
	{
		RETURN_IF_ERROR(GetRuntime()->GetNativeValueOf(m_start_date, &val));
		if (val.type != VALUE_NUMBER)
			return OpStatus::ERR; // this should be date which return number as native value
		GetCalendarItem()->undefnull.start_date = IS_VALUE;
		GetCalendarItem()->start_date = val.value.number;
	}
	else
	{
		GetCalendarItem()->undefnull.start_date = IS_NULL;
		GetCalendarItem()->start_date = 0;
	}

	if (m_end_date)
	{
		RETURN_IF_ERROR(GetRuntime()->GetNativeValueOf(m_end_date, &val));
		if (val.type != VALUE_NUMBER)
			return OpStatus::ERR; // this should be date which return number as native value
		GetCalendarItem()->undefnull.end_date = IS_VALUE;
		GetCalendarItem()->end_date = val.value.number;
	}
	else
	{
		GetCalendarItem()->undefnull.end_date = IS_NULL;
		GetCalendarItem()->end_date = 0;
	}

	if (m_alarm_date)
	{
		RETURN_IF_ERROR(GetRuntime()->GetNativeValueOf(m_alarm_date, &val));
		if (val.type != VALUE_NUMBER)
			return OpStatus::ERR; // this should be date which return number as native value
		GetCalendarItem()->undefnull.alarm_date = IS_VALUE;
		GetCalendarItem()->alarm_date = val.value.number;
	}
	else
	{
		GetCalendarItem()->undefnull.alarm_date = IS_NULL;
		GetCalendarItem()->alarm_date = 0;
	}
	return OpStatus::OK;
}

/* static */ int
DOM_JILCalendarItem::update(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(calendar_item, DOM_TYPE_JIL_CALENDARITEM, DOM_JILCalendarItem);
	DOM_CHECK_OR_RESTORE_ARGUMENTS_JIL("");

	OpCalendarEvent* calendar_event = 0;
	if (DOM_CallState::GetPhaseFromESValue(return_value) < DOM_CallState::PHASE_EXECUTION_0)
	{
		if (calendar_item->GetCalendarItem()->id.IsEmpty())
			return CallException(DOM_Object::JIL_UNKNOWN_ERR, return_value, origining_runtime, UNI_L("CalendarItem was not added yet"));
		CALL_FAILED_IF_ERROR_WITH_HANDLER(calendar_item->PushDatesToCalendarItem(), DOM_JILPIM::HandleExportCalendarEventError);
		CALL_FAILED_IF_ERROR_WITH_HANDLER(calendar_item->GetCalendarItem()->ExportToOpCalendarEvent(calendar_event), DOM_JILPIM::HandleExportCalendarEventError);
		OP_ASSERT(calendar_event);
	}
	OpAutoPtr<OpCalendarEvent> calendar_event_deleter(calendar_event);

	DOM_SuspendingCall call(this_object, argv, argc, return_value, return_value, origining_runtime, DOM_CallState::PHASE_EXECUTION_0);
	NEW_SUSPENDING_CALLBACK(DOM_JILPIM::CalendarModificationCallbackImpl, callback, return_value, call, ());
	OpMemberFunctionObject2<OpCalendarService, OpCalendarEventModificationCallback*, OpCalendarEvent*>
		function(g_op_calendar, &OpCalendarService::CommitEvent, callback, calendar_event);
	DOM_SUSPENDING_CALL(call, function, DOM_JILPIM::CalendarModificationCallbackImpl, callback);

	OP_ASSERT(callback);
	CALL_FAILED_IF_ERROR_WITH_HANDLER(callback->GetErrorCode(), DOM_JILPIM::HandleCalendarError);
	return ES_FAILED; // success
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_JILCalendarItem)
	DOM_FUNCTIONS_WITH_SECURITY_RULE(DOM_JILCalendarItem, DOM_JILCalendarItem::update, "update", "-", "CalendarItem.update")
DOM_FUNCTIONS_END(DOM_JILCalendarItem)

/* virtual */
int DOM_JILCalendarItem_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	return CreateCalendarItem(return_value, origining_runtime);
}

/* static */
int DOM_JILCalendarItem_Constructor::CreateCalendarItem(ES_Value* return_value, ES_Runtime* origining_runtime)
{
	DOM_JILCalendarItem* dom_calendar_item = NULL;
	CALL_FAILED_IF_ERROR(DOM_JILCalendarItem::Make(dom_calendar_item, NULL, static_cast<DOM_Runtime*>(origining_runtime)));
	DOMSetObject(return_value, dom_calendar_item);
	return ES_VALUE;
}

#endif // DOM_JIL_API_SUPPORT
