/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_ESTIMERMANAGER_H
#define ES_UTILS_ESTIMERMANAGER_H

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/messobj.h"

class ES_TimerEvent;

/**
 * A manager for managing timer events.
 *
 * Events are added with a delay and are fired in expire order. It also supports
 * repeating events, although the event itself is responsible for updating the
 * new timeout time of the event and adding the event to the timer manager
 * again.
 */
class ES_TimerManager
	: public MessageObject
{
public:
	ES_TimerManager();
	~ES_TimerManager();

	void SetMessageHandler(MessageHandler *msg_handler_) { msg_handler = msg_handler_; }
	/**< Sets the message handler. A message handler must be set before an event
	     is added to the timer manager.
	     @param msg_handler_ The handler to set. */

	OP_STATUS AddEvent(ES_TimerEvent *event);
	/**< Add a new event to the timer manager. The event will expire when its
	     time is up. If the timer manager is deactivated it will be activated by
	     this call. Timer manager takes ownership of this event and will always
	     add it to its waiting list. The event may also be deleted in case
	     FireExpiredEvents() is called and fails.
	     @param event The event to add.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	OP_STATUS RepeatEvent(ES_TimerEvent *event);
	/**< Add an event to the timer manager again. The event must have been
	     previously added (and have expired) to this timer manager. The event
	     will expire when its time is up.
	     @param event The event to add.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	OP_STATUS RemoveEvent(unsigned id);
	/**< Remove and delete a waiting event from the timer manager.
	     @param id The id of the event to remove.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	void RemoveAllEvents();
	/**< Remove and delete all events from the timer manager. */

	void RemoveExpiredEvent(ES_TimerEvent *event);
	/**< Remove and delete an expired event from the timer manager's expired
	     list.
	     @param event The event to delete. */

	void Deactivate();
	/**< Deactivate the timer manager. No eventes will expire anymore but they
	     will still be around if we decide to activate the timer manager
	     again. */

	OP_STATUS Activate();
	/**< Activate the timer manager if it has been deactivated. Existing events
	     will expire based on the timeout time they had when the timer manager
	     was deactivated. Hence if an event expired while the timer manager was
	     not active it will (asynchronously) expire immediately when the timer
	     manager is activated again.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	BOOL HasWaiting();
	/**< Returns if the timer manager has waiting events.
	     @return TRUE if there are waiting events. */

private:
	List<ES_TimerEvent> waiting;
	/**< Contains events waiting for expiration. The timer manager can have
	     waiting events in this list at the time the timer manager is destroyed
	     and events in this list will be deleted at that time. */
	List<ES_TimerEvent> expired;
	/**< Contains expired events. All expired events should have been deleted at
	     timer manager destruction by calls to RemoveExpiredEvent() or
	     RemoveAllEvents(). */

	MessageHandler *msg_handler;

	unsigned id; unsigned timer_event_id;
#ifdef SCOPE_PROFILER
	unsigned scope_event_id;
#endif // SCOPE_PROFILER
	BOOL has_set_callbacks;
	BOOL has_posted_timeout;
	BOOL is_active;

	OP_STATUS AddEventInternal(ES_TimerEvent *event);

	OP_STATUS FireExpiredEvents();

	OP_STATUS PostMessage();

	void RemoveMessage();

	OP_STATUS SetCallbacks();

	void UnsetCallbacks();

	/* From MessageObject. */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
};

#endif // ES_UTILS_ESTIMERMANAGER_H
