/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILCALENDARITEM_H
#define DOM_DOMJILCALENDARITEM_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/pi/device_api/OpCalendarService.h"
#include "modules/device_api/jil/JILCalendar.h"

class DOM_JILPIM;
class DOM_JILCalendarItem_Constructor;

class DOM_JILCalendarItem : public DOM_JILObject
{
	friend class DOM_JILCalendarItem_Constructor;
public:
	virtual ~DOM_JILCalendarItem();
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_CALENDARITEM || DOM_JILObject::IsA(type); }

	virtual void GCTrace();
	/**
	 * Constructs a new DOM_JILCalendarItem object
	 *
	 * @param new_obj set to a newly constructed object
	 * @param calendar_event reference event to construct new calendar item.
	 *				This function does NOT take ownership of this object.
	 *				If NULL then default values will be used
	 * @param origining_runtime runtime to which the newly created object will be added. MUST NOT be NULL
	 */
	static OP_STATUS Make(DOM_JILCalendarItem*& new_obj, OpCalendarEvent* calendar_event, DOM_Runtime* origining_runtime);

	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

	DOM_DECLARE_FUNCTION(update);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

	JILCalendarItem* GetCalendarItem() { return &m_calendar_event; }
	OP_STATUS PushDatesToCalendarItem();
private:
	DOM_JILCalendarItem();
	JILCalendarItem m_calendar_event;
	ES_Object* m_start_date;
	ES_Object* m_end_date;
	ES_Object* m_alarm_date;
};

class DOM_JILCalendarItem_Constructor : public DOM_BuiltInConstructor
{
	friend class DOM_JILPIM;
public:
	DOM_JILCalendarItem_Constructor()
	  : DOM_BuiltInConstructor(DOM_Runtime::JIL_CALENDARITEM_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
private:
	static int CreateCalendarItem(ES_Value* return_value, ES_Runtime* origining_runtime);
};

#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILCALENDARITEM_H
