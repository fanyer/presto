/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_EVENTDATA_H
#define DOM_EVENTDATA_H

#include "modules/dom/domeventtypes.h"

#define DOM_EVENT_NAME(type) \
	(g_DOM_eventData[(type)].name)
#define DOM_EVENT_NAMEX(type, data) \
	(data[(type)].name)

#define DOM_EVENT_BUBBLES(type) \
	((g_DOM_eventData[(type)].flags & DOM_EventData::FLAG_BUBBLES) == DOM_EventData::FLAG_BUBBLES)

#define DOM_EVENT_IS_CANCELABLE(type) \
	((g_DOM_eventData[(type)].flags & DOM_EventData::FLAG_CANCELABLE) == DOM_EventData::FLAG_CANCELABLE)

#define DOM_EVENT_IS_PROPERTY(type) \
	((g_DOM_eventData[(type)].flags & DOM_EventData::FLAG_PROPERTY) == DOM_EventData::FLAG_PROPERTY)
#define DOM_EVENT_IS_PROPERTYX(type, data) \
	((data[(type)].flags & DOM_EventData::FLAG_PROPERTY) == DOM_EventData::FLAG_PROPERTY)

#ifdef DOM3_EVENTS
#  define DOM_EVENT_HAS_NAMESPACE_DATA(type) \
	((g_DOM_eventData[(type)].flags & DOM_EventData::FLAG_NAMESPACE) == DOM_EventData::FLAG_NAMESPACE)
#  define DOM_EVENT_HAS_NAMESPACE_DATAX(type, data) \
	((data[(type)].flags & DOM_EventData::FLAG_NAMESPACE) == DOM_EventData::FLAG_NAMESPACE)
#endif // DOM3_EVENTS

#define DOM_EVENT_AS_HTMLWINDOW_PROPERTY(type) \
	((g_DOM_eventData[(type)].flags & DOM_EventData::FLAG_HTML_WINDOW_PROPERTY) == DOM_EventData::FLAG_HTML_WINDOW_PROPERTY)

#define DOM_EVENT_AS_HTMLDOC_PROPERTY(type) \
	((g_DOM_eventData[(type)].flags & DOM_EventData::FLAG_HTML_DOCUMENT_PROPERTY) == DOM_EventData::FLAG_HTML_DOCUMENT_PROPERTY)

#define DOM_EVENT_AS_SVGDOC_PROPERTY(type) \
	((g_DOM_eventData[(type)].flags & DOM_EventData::FLAG_SVG_DOCUMENT_PROPERTY) == DOM_EventData::FLAG_SVG_DOCUMENT_PROPERTY)

#define DOM_EVENT_AS_HTMLELM_PROPERTY(type) \
	((g_DOM_eventData[(type)].flags & DOM_EventData::FLAG_HTML_ELEMENT_PROPERTY) == DOM_EventData::FLAG_HTML_ELEMENT_PROPERTY)

#define DOM_EVENT_AS_HTMLFORMELM_PROPERTY(type) \
	((g_DOM_eventData[(type)].flags & DOM_EventData::FLAG_HTML_FORM_ELEMENT_PROPERTY) == DOM_EventData::FLAG_HTML_FORM_ELEMENT_PROPERTY)

struct DOM_EventData
{
	enum
	{
		FLAG_BUBBLES    = 0x01,
		/**< The event bubbles. */

		FLAG_CANCELABLE = 0x02,
		/**< The event is cancelable. */

		FLAG_PROPERTY   = 0x04,
		/**< Old style event handlers for this event type should be
		     accessible as properties of the EventTarget object. */

		FLAG_NAMESPACE  = 0x08,
		/**< There is extra namespace information about this event type.
		     This means that it is not in the normal DOM 3 Events namespace
			 and that it might be called something else in its namespace
			 than it is in a namespace ignorant context. */

		FLAG_HTML_DOCUMENT_PROPERTY       = 0x10,
		/**< There should be a corresponding visible (empty) on<event> property on
		     the HTMLDocument. */
		FLAG_HTML_ELEMENT_PROPERTY        = 0x20,
		/**< There should be a corresponding visible (empty) on<event> property on
		     any HTMLElement. */
		FLAG_HTML_FORM_ELEMENT_PROPERTY   = 0x40,
		/**< There should be a corresponding visible (empty) on<event> property on
		     any form related html element. */
		FLAG_HTML_WINDOW_PROPERTY         = 0x80,
		/**< There should be a corresponding visible (empty) on<event> property on
		     the global window object. */

		FLAG_SVG_DOCUMENT_PROPERTY        = 0x100,
		/**< There should be a corresponding visible (empty) on<event> property on
		     the SVGDocument. */

		FLAG_NONE = 0
	};

	const char *name;
	/**< The event name (never including an "on" prefix). */

	unsigned flags;
	/**< Event type flags. */
};

/** List of DOM_EventData records.  The order matches that of the
    DOM_EventType enum defined in domeventtypes.h.  The last element's
    'name' field has the value NULL. */
//extern const DOM_EventData g_DOM_eventData[];


#ifdef DOM3_EVENTS

struct DOM_EventNamespaceData
{
	DOM_EventType known_type;
	/**< The event type this applies to. */

	const char *namespaceURI;
	/**< The event type's namespace URI. */

	const char *localname;
	/**< The event type's localname. */
};

/** List of DOM_EventNamespaceData records.  The order matches that
    of the DOM_EventType enum defined in domeventtypes.h, but not all
    event types are represented.  The last element's 'known_type'
    field has the value DOM_EVENT_NONE. */
//extern const DOM_EventNamespaceData g_DOM_eventNamespaceData[];

#endif // DOM3_EVENTS
#endif // DOM_EVENTDATA_H
