/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM2_EVENTS_API

#include "modules/dom/domevents.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventtarget.h"

/* static */ DOM_EventType
DOM_EventsAPI::GetEventType(const uni_char *type)
{
	return DOM_Environment::GetEventType(type);
}

/* static */ const char *
DOM_EventsAPI::GetEventTypeString(DOM_EventType type)
{
	return DOM_Environment::GetEventTypeString(type);
}

/* static */ BOOL
DOM_EventsAPI::IsWindowEvent(DOM_EventType type)
{
	return DOM_Event::IsWindowEvent(type);
}

/* static */ BOOL
DOM_EventsAPI::IsWindowEventAsBodyAttr(DOM_EventType type)
{
	return DOM_Event::IsWindowEventAsBodyAttr(type);
}

/* static */ OP_STATUS
DOM_EventsAPI::GetEventTarget(EventTarget *&event_target, DOM_Object *node)
{
	RETURN_IF_ERROR(node->CreateEventTarget());
	event_target = node->GetEventTarget();
	return OpStatus::OK;
}

#endif // DOM2_EVENTS_API
