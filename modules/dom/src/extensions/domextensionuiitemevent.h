/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.	All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/
#ifndef DOM_EXTENSIONS_UIITEMEVENT_H
#define DOM_EXTENSIONS_UIITEMEVENT_H

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/domevents/domevent.h"

/** This class implements the events which are received by UIItems,
 *  for example "click" on toolbar button.
 *  \see DOM_ExtensionUIItem
 */
class DOM_ExtensionUIItemEvent
	: public DOM_Event
{
public:
	static OP_STATUS Make(DOM_ExtensionUIItemEvent *&new_obj, DOM_Object *target_object, DOM_Runtime *origining_runtime);

	DOM_ExtensionUIItemEvent(){}
	virtual ~DOM_ExtensionUIItemEvent() {}

	virtual BOOL IsA(int type) { return type == DOM_TYPE_EXTENSION_UIITEM_EVENT || DOM_Event::IsA(type); }

	DOM_DECLARE_FUNCTION(initUIItemEvent);
	enum {
		FUNCTIONS_initUIItemEvent = 1,
		FUNCTIONS_ARRAY_SIZE
	};

};

#endif // EXTENSION_SUPPORT
#endif // DOM_EXTENSIONS_UIITEMEVENT_H
