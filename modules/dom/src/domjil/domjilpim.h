/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_DOMJILPIM_H
#define DOM_DOMJILPIM_H

#ifdef DOM_JIL_API_SUPPORT

#include "modules/dom/src/domjil/domjilobject.h"
#include "modules/dom/src/domjil/domjilcalendaritem.h"
#include "modules/dom/src/domasynccallback.h"
#include "modules/pi/device_api/OpAddressBook.h"
#include "modules/pi/device_api/OpCalendarService.h"
#include "modules/dom/src/domjil/utils/jilutils.h"

#include "modules/device_api/jil/JILCalendar.h"

class DOM_JILAddressBookItem;
class DOM_JILPIM : public DOM_JILObject, OpCalendarServiceListener
{
	friend class DOM_JILAddressBookItem;
	friend class DOM_JILCalendarItem;
public:
	virtual ~DOM_JILPIM();
	virtual void GCTrace();
	static OP_STATUS Make(DOM_JILPIM*& bpim, DOM_Runtime* runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_JIL_PIM || DOM_Object::IsA(type); }

	DOM_DECLARE_FUNCTION(getAddressBookItem);
	DOM_DECLARE_FUNCTION(addAddressBookItem);
	DOM_DECLARE_FUNCTION(createAddressBookItem);
	DOM_DECLARE_FUNCTION(deleteAddressBookItem);
	DOM_DECLARE_FUNCTION(findAddressBookItems);
	DOM_DECLARE_FUNCTION(getAddressBookItemsCount);
	DOM_DECLARE_FUNCTION(exportAsVCard);

	DOM_DECLARE_FUNCTION(getCalendarItem);
	DOM_DECLARE_FUNCTION(addCalendarItem);
	DOM_DECLARE_FUNCTION(deleteCalendarItem);
	DOM_DECLARE_FUNCTION(getCalendarItems);
	DOM_DECLARE_FUNCTION(findCalendarItems);

#ifdef SELFTEST
	DOM_DECLARE_FUNCTION(getCalendarItemsCount); // this function was "ommited" in JIL spec, but
												 // they will probably just forgot to put it in the spec(as usual)
												 // for now lets keep it out of production code, but keep in selftest
												 // be able to test it
#endif

	enum { FUNCTIONS_ARRAY_SIZE = 13
#ifdef SELFTEST
		+ 1 //
#endif
	};
	// OpCalendarServiceListener implementation
	virtual void OnEvent(OpCalendarService* calendar, OpCalendarEvent* evt);
	virtual void OnCalendarDestroyed(OpCalendarService* calendar) {};
	
protected:
	virtual ES_GetState InternalGetName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);
	virtual ES_PutState InternalPutName(OpAtom property_atom, ES_Value* value, DOM_Runtime* origining_runtime, ES_Value* restart_value);

private:
	DOM_JILPIM();

	static int HandleAddressBookError(OP_ADDRESSBOOKSTATUS error, ES_Value* return_value, DOM_Runtime* runtime);
	static int HandleCalendarError(OP_ADDRESSBOOKSTATUS error, ES_Value* return_value, DOM_Runtime* runtime);
	static int HandleExportCalendarEventError(OP_ADDRESSBOOKSTATUS error, ES_Value* return_value, DOM_Runtime* runtime);

	OP_STATUS CreateEventReccurenceTypes();

	ES_Object* m_on_address_book_items_found;
	ES_Object* m_on_calendar_items_found;
	ES_Object* m_on_calendar_item_alert;
	ES_Object* m_event_reccurence_types;
	ES_Object* m_on_v_card_exporting_finish;
	OP_STATUS GetDateFromDateObject(ES_Value& date_object, double& date);
	typedef JILUtils::ModificationSuspendingCallbackImpl<OpAddressBookItemModificationCallback> AddressBookModificationCallbackImpl;
	typedef JILUtils::ModificationSuspendingCallbackImpl<OpCalendarEventModificationCallback> CalendarModificationCallbackImpl;
};
#endif // DOM_JIL_API_SUPPORT

#endif // DOM_DOMJILPIM_H
