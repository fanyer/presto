/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_EVENTTARGET_H
#define DOM_EVENTTARGET_H

#include "modules/dom/domeventtypes.h"
#include "modules/dom/domevents.h"

#include "modules/util/simset.h"

class DOM_Event;
class DOM_EventListener;
class DOM_EventHandler;
class ES_Object;

class DOM_EventTarget
#ifdef DOM2_EVENTS_API
	: public DOM_EventsAPI::EventTarget
#endif // DOM2_EVENTS_API
{
protected:
	DOM_Object *owner;
	Head listeners;

public:
	DOM_EventTarget(DOM_Object *owner);
	virtual ~DOM_EventTarget();

	/**
	 * Iterates over all native listeners in the event target, other listeners
	 * are skipped.
	 */
	class NativeIterator
	{
	public:
		/**
		 * Initialize iterator from the listeners in the event target.
		 * @param target Event target that contains event listeners.
		 */
		NativeIterator(const DOM_EventTarget &target);

		/**
		 * @return @c TRUE if the iterator has iterated over all native listeners, @c FALSE otherwise.
		 */
		BOOL AtEnd() const;
		/**
		 * Returns the current native event listener.
		 * @note Only call this when AtEnd() is @c FALSE.
		 * @return The current event listener.
		 */
		const DOM_EventListener *Current() const;
		/**
		 * Advances to the next native listener or the end.
		 * @note Only call this when AtEnd() is @c FALSE.
		 */
		void Next();

	private:
		const DOM_EventListener *listener;
	};
	friend class NativeIterator;

	/**
	 * Returns an iterator that covers all native listeners.
	 */
	NativeIterator NativeListeners() const { return NativeIterator(*this); }

	BOOL HasListeners(DOM_Event *event, ES_EventPhase phase);

#ifdef DOM3_EVENTS
	BOOL HasListeners(DOM_EventType known_type, const uni_char *namespaceURI, const uni_char *type, ES_EventPhase phase);
#else // DOM3_EVENTS
	BOOL HasListeners(DOM_EventType type, const uni_char *type_string, ES_EventPhase phase);
#endif // DOM3_EVENTS

	OP_STATUS HandleEvent(DOM_Event *event);

	virtual void AddListener(DOM_EventListener *listener);
	virtual void RemoveListener(DOM_EventListener *listener);
	OP_BOOLEAN FindOldStyleHandler(DOM_EventType type, ES_Object **handler);

	OP_STATUS AddHandler(DOM_EventHandler *handler);
	void RemoveHandler(DOM_EventHandler *handler);

	DOM_Object* GetOwner() { return owner; }

#ifdef DOM2_EVENTS_API
	virtual DOM_Object *GetNode();

	virtual OP_STATUS AddEventHandler(DOM_EventsAPI::EventHandler *handler);
	virtual void RemoveEventHandler(DOM_EventsAPI::EventHandler *handler);
#endif // DOM2_EVENTS_API

	void GCTrace();
};

class DOM_EventTargetOwner
{
protected:
	DOM_EventTarget *event_target;

public:
	DOM_EventTargetOwner()
		: event_target(NULL)
	{
	}

	virtual ~DOM_EventTargetOwner()
	{
		OP_DELETE(event_target);
	}

	virtual DOM_Object *GetOwnerObject() = 0;

	virtual DOM_EventTarget *FetchEventTarget();
	virtual OP_STATUS CreateEventTarget();
};

#endif // DOM_EVENTTARGET_H
