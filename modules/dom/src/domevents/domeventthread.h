/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_EVENTTHREAD_H
#define DOM_EVENTTHREAD_H

#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/esthread.h"

class DOM_Event;
class DOM_Object;
class DOM_EnvironmentImpl;
class HTML_Element;

class DOM_EventThread
	: public ES_Thread
{
public:
	DOM_EventThread();
	~DOM_EventThread();

	virtual OP_STATUS EvaluateThread();
	/**< Evaluates this thread by propagating the event, initiating
	     subthreads to execute event handlers.

	     @return OpStatus::OK on success, OpStatus::ERR or
	             OpStatus::ERR_NO_MEMORY on failure. */

	ES_ThreadType Type();
	/**< The thread's type.  This class' type is ES_THREAD_EVENT.

	     @return The thread's type. */

	ES_ThreadInfo GetInfo();
	/**< Return information about the thread, including the thread type and
	     the event type (as returned by GetEventType()).

	     @return A thread info object. */

	ES_ThreadInfo GetOriginInfo();
	/**< Returns the same as ES_Thread::GetOriginInfo(), except if this is
	     an unload event, in which case it returns the result of GetInfo()
	     called on this thread, even though this thread actually interrupts
	     a terminating thread.

	     @return A thread info object. */

	const uni_char *GetInfoString();
	/**< Return a string describing the thread (for error reporting, mainly).
	     @return A string: "Event thread: <event type>". */

	OP_STATUS InitEventThread(DOM_Event *event, DOM_EnvironmentImpl *environment);
	/**< Initialize this event thread with its event.  This function
	     performs any necessary garbage collection protections of objects
	     involved in the processing of this event.  It also records the
	     propagation path of the event; any modifications to the document
	     between the call to this function and the actual evaluation will
	     not reflect what nodes the event propagates past.

	     @param event_ The event processed by this thread.  MUST NOT be
	    			   NULL.
	     @param environment A DOM environment.  MUST be the one that
	                       owns the scheduler that this thread is added
	    				   to. */

	DOM_EventType GetEventType();
	/**< Returns the type of the event this thread processes.  MUST NOT
	     be called before InitEventThread() is called.

	     @return An event type. */

	DOM_Object *GetEventTarget();
	/**< Returns the target node of the event this thread processes.  MUST
	     NOT be called before InitEventThread() is called.

	     @return A node. */

	DOM_Event *GetEvent();
	/**< Returns the event object representing the event this thread
	     processes.

	     @return An event object. */

	OP_STATUS PutWindowEvent(ES_Value *value);
	/**< Update this event thread's binding for the global window.event
	     variable to 'value'. Called if event handler code updates
	     "event" on the global object (e.g., as "event" or "window.event".)
	     Its scope only extends over the lifetime of this event thread. */

	OP_STATUS GetWindowEvent(ES_Value *value);
	/**< Return this event thread's binding for window.event. Either
	     the explicitly updated value (see PutWindowEvent()) or the
	     event object of this thread. To discourage code from using
	     window.event (an MSIEism), we mark the event object as hidden. */

protected:
	OP_STATUS Signal(ES_ThreadSignal signal);

	OP_STATUS ConstructEventPath();
	/**< Record the propagation path of the event this thread processes.
	     Called by InitEventThread().  It starts at the target node and
	     traces up the dom tree all the way to the document node, adding
	     the nodes to the event propagation path.

	     The nodes that are in the event propagation path are also
	     protected from garbage collection.

	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

	BOOL GetNextCurrentTarget(DOM_Object *&node);
	/**< Moves to the next node in the event's path, adjusting the event's
	     current phase as necessary.

	     @return TRUE if there was a next node, FALSE if there wasn't and
	             propagation is finished. */

	DOM_Object *GetNode(int index);
	/**< Returns the DOM node at position 'index' in the event's path;
	     where zero is the root and N-1 is the target, if N is the length
	     of the path (i.e, DOM_EventThread::nodes_in_path).

	     @param The index.  MUST be in the range [0, nodes_in_path).
	     @return A DOM node, or NULL if the element at the position 'index'
	             has no corresponding DOM node object. */

	int current_event_node;
	/**< The current position in the event's path.  Used and updated by
	     AdvanceCurrentEventNode(). */

	DOM_Event *event;
	/**< The event processed by this thread. */

	DOM_EnvironmentImpl *environment;
	/**< The DOM environment in which this event is processed. */

	DOM_Object **path_head;
	/**< The head of the propagation path, any non-HTML_Element objects
	     closer to the root of the path that the first HTML_Element in it;
	     normally only the DOM document. */

	HTML_Element **path_body;
	/**< The main body of the propagation path, including all HTML_Elements
	     in it.  Note: the least significant bit in every pointer is used
	     for other, dark purposes and must be masked away.  This is done
	     by GetDOM_Node(), which should always be used to access elements in
	     the path. */

#ifdef DOM_NOT_USED
	DOM_Node **path_tail;
	/**< The tail of the propagation path, including any non_HTML_Element
	     objects closer to the target than the last HTML_Element in the
	     path.  Used when the target of an event is an attribute node. */
#endif // DOM_NOT_USED

	int nodes_in_path;
	/**< Total number of nodes in the event's path. */

	int nodes_in_path_head;
	/**< Number of nodes in path_head. */

	int nodes_in_path_body;
	/**< Number of nodes in path_body. */

#ifdef DOM_NOT_USED
	int nodes_in_path_tail;
	/**< Number of nodes in path_tail. */
#endif // DOM_NOT_USED

#ifdef USER_JAVASCRIPT
	BOOL has_fired_afterevent;
	/**< Set to TRUE when an AfterEvent event if fired. */
#endif // USER_JAVASCRIPT

	BOOL is_performing_default;
	/**< Set to TRUE while the default action is performed. */

	BOOL has_performed_default;
	/**< Set to TRUE when the default action is performed. */

	BOOL has_updated_window_event;
	/**< Set to TRUE when window.event is updated by an event handler. */

	ES_Value window_event;
	/**< The current value of window.event that an event handler has
	     updated it to. */
};

#endif // DOM_EVENTTHREAD_H
