 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DOM_STORAGEEVENT_H
#define DOM_STORAGEEVENT_H

#ifdef CLIENTSIDE_STORAGE_SUPPORT

#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/domtypes.h" // for ES_Value

class DOM_Storage;
class OpStorageValueChangedEvent;

class DOM_StorageEvent
	: public DOM_Event
{
private:

	OpString m_origining_url;
	ES_ValueString m_string_holder;
	OpStorageValueChangedEvent *m_value_changed_event;
	DOM_Storage *m_custom_storage_obj;

public:
	DOM_StorageEvent()
	   : m_value_changed_event(NULL)
	   , m_custom_storage_obj(NULL)
	{
	}

	~DOM_StorageEvent();

	static OP_STATUS Make(DOM_StorageEvent *&event, OpStorageValueChangedEvent *event_obj, DOM_Runtime *runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_STORAGEEVENT || DOM_Event::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(initStorageEvent);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#endif // CLIENTSIDE_STORAGE_SUPPORT

#endif // DOM_STORAGEEVENT_H
