/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DOM_EVENTS_H
#define DOM_EVENTS_H

#ifdef DOM2_EVENTS_API

#include "modules/dom/domeventtypes.h"

class DOM_Object;
class DOM_Environment;
class ES_Thread;


/** Interface for external (outside of the DOM module) handling of DOM events.

    To add an external listener for events, one needs

      1) a reference to a DOM object that implements the EventTarget DOM
         interface (such as all Node subclasses,) for instance the Document
         object returned by DOM_Environment::GetDocument or the Node objects
         created and/or returned by DOM_Environment::ConstructNode.

      2) an object that implements the DOM_EventsAPI::EventHandler interface.

      3) create and/or retrieve a DOM_EventsAPI::EventTarget object for the
         DOM object in (1).

      4) add the event handler in (2) to the event target in (3) using the
         function DOM_EventsAPI::EventTarget::AddEventHandler.

    The object in (2) needs to return TRUE from its HandlesEvent function for
    the various event types it wants to handle.  Its HandleEvent function will
    then be called to handle each event that is handled by the EventTarget
    according to the event propagation rules in DOM Events.

    An event type can be represented in one of two ways: a "known" event type,
    which is represented by an enumerator in the DOM_EventType enumeration or as
    a type string.  Known event types can be retrieved using the function
    DOM_EventsAPI::GetEventType.  The two special DOM_EventType enumerators
    DOM_EVENT_NONE and DOM_EVENT_CUSTOM are used to represent an unrecognized
    type and a type that is represented as a type string, respectively.

    Event handler objects added to event targets are owned by the event target
    object and deleted when it is deleted, unless they are removed again
    before that happens.

    The event objects sent to event handlers via the virtual function
    DOM_EventsAPI::EventHandler::HandleEvent are garbage collected objects
    owned by the ECMAScript garbage collector.  They are guaranteed to live at
    least as long as the thread handling the event (the thread returned by the
    function DOM_EventsAPI::Event::GetThread.)  They usually live slightly
    longer than that, and can live even longer if a script stores a reference
    to the event.  If an external event handler needs to store a reference to
    the event it can protect it, but should be very careful to unprotect any
    events it protects as substantial memory leaks can result otherwise.

    This API is currently used by the XML Events implementation. */
class DOM_EventsAPI
{
public:
	/** Interface representing a DOM Events Event (or subclass).  This
	    interface is implemented by objects in the DOM module and should
	    not be inherited by external classes.

	    Objects of this type are sent to the function HandleEvent in the
	    EventHandler interface. */
	class Event
	{
	public:
		virtual DOM_EventType GetType() = 0;
		/**< Returns the event's type as an enumerator in the DOM_EventType
		     enumeration or DOM_EVENT_CUSTOM if it is included therein.

		     @return A DOM_EventType code. */

		virtual const uni_char *GetTypeString() = 0;
		/**< Returns the event's type as a string.  This value is only valid
		     if GetType returned DOM_EVENT_CUSTOM.

		     @return An event type. */

		virtual DOM_Object *GetTarget() = 0;
		/**< Returns the target of the event.

		     @return Never NULL.  Usually a DOM Node, but since other objects
		             implement the EventTarget DOM interface, it can be
		             something else. */

		virtual DOM_Object *GetCurrentTarget() = 0;
		/**< Returns the current target of the event.  This is always an
		     EventTarget to which the event handler that is currently handling
		     the event has been added.

		     @return Never NULL. */

		virtual ES_EventPhase GetEventPhase() = 0;
		/**< An event phase.  Always one of ES_PHASE_CAPTURING,
		     ES_PHASE_AT_TARGET or ES_PHASE_BUBBLING, never ES_PHASE_ANY.

		     @return An ES_EventPhase code. */

		virtual BOOL GetBubbles() = 0;
		/**< Returns TRUE if the event bubbles.

		     @return TRUE or FALSE. */

		virtual BOOL GetCancellable() = 0;
		/**< Returns TRUE if the event is cancellable.

		     @return TRUE or FALSE. */

		virtual double GetTimeStamp() = 0;
		/**< Returns the event's time stamp.  This is the time the event was
		     dispatched, in seconds from 1 January 1970.

		     @return Number of seconds since 1 January 1970 the event was
		             dispatched. */

		virtual ES_Thread *GetThread() = 0;
		/**< Returns the ECMAScript thread that is handling the event.  This
		     thread should be interrupted by any thread that is part of the
		     handling the event.

		     @return An ECMAScript thread. */

		virtual void StopPropagation() = 0;
		/**< Stops the event's propagation.  See the DOM Events specification
		     for the exact meaning. */

		virtual void StopImmediatePropagation() = 0;
		/**< Stops the event's propagation immediately.  See the DOM 3 Events
		     specification for the exact meaning.  If support for DOM 3 Events
		     is disabled, calling this function is equivalent to calling
		     StopPropagation. */

		virtual void PreventDefault() = 0;
		/**< Prevent the event's default action (a.k.a. cancelling the event.)
		     See the DOM Events specification for the exact meaning. */

		virtual OP_STATUS Protect() = 0;
		/**< Protect the event from deletion.  For each successful call to this
		     function Unprotect must be called exactly once at some point or
		     substantial memory leaks will occur.  The time span of such a pair
		     of calls must never depend on the life time of a garbage collected
		     object since that can cause circular references the garbage
		     collector cannot resolve.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

		virtual void Unprotect() = 0;
		/**< Unprotect the event.  Must be called once for each call to
		     Protect. */

	protected:
		virtual ~Event() {}
		/**< Virtual destructor so that compilers won't give bogus
		     warnings claiming this class needs a virtual destructor,
	         which it doesn't. */
	};


	/** Interface representing an event handler.  External classes wishing to
	    listen to DOM Events should implement this interface, return TRUE from
	    the function HandlesEvent for the event types it wishes to handle and
	    handle events in the HandleEvent function.

	    Furthermore, the functions RegisterHandlers and UnreqisterHandlers need
	    to register and unregister the known event types (event types included
	    in the DOM_EventType enumeration) for which HandlesEvent can return
	    TRUE, using the functions DOM_Environment::AddEventHandler and
	    DOM_Environment::RemoveEventHandler.  Failure to do so can prevent the
	    events from ever being sent and thus from being seen by the event
	    handler. */
	class EventHandler
	{
	public:
		virtual ~EventHandler() {}
		/** Destructor. */

		virtual BOOL HandlesEvent(DOM_EventType known_type, const uni_char *type, ES_EventPhase phase) = 0;
		/**< Called to query whether the event handler would handle an event
		     of the specified type during the specified event phase.  Only
		     events of types for which this function returns TRUE will ever be
		     sent to the HandleEvent function.  The special phase ES_PHASE_ANY
		     should cause this function to return TRUE for any event types it
		     would handle at all, not only for event types it would handle in
		     all phases.

		     Note: the type argument is only valid if known_type is
		     DOM_TYPE_CUSTOM.  If the event handler wishes to handle events of
		     types included in the DOM_EventType enumeration it must use the
		     appropriate enumerator and not rely on type argument.  The right
		     DOM_EventType enumerator to be used can be identified using the
		     function DOM_EventsAPI::GetEventType.

		     @param known_type An event type included in the DOM_EventType
		                       enumeration or DOM_TYPE_CUSTOM.
		     @param type The event type as a string, if it is not one
		                 included in the DOM_EventType enumeration.
		     @param phase The event phase.

		     @return The function should return TRUE or FALSE. */

		virtual OP_STATUS HandleEvent(Event *event) = 0;
		/**< Called to let the event handler handle an event.  The event is
		     guaranteed to be of a type and in a phase for which HandlesEvent
		     has been called and returned TRUE.

		     If the event handler starts execution of an ECMAScript thread as
		     part of its handling of the event, that thread should interrupt
		     the thread returned by 'event->GetThread()'.

		     The event handler can only call the functions StopPropagation and
		     PreventDefault on the event immediately or during the execution
		     of an ECMAScript thread that interrupts the event's thread.

		     @param event The event to handle.

		     @return The function should return OpStatus::OK or
		             OpStatus::ERR_NO_MEMORY. */

		virtual void RegisterHandlers(DOM_Environment *environment) = 0;
		/**< Called to request that the event handler registers all event types
		     it handles with the environment through the function
		     DOM_Environment::AddEventHandler. */

		virtual void UnregisterHandlers(DOM_Environment *environment) = 0;
		/**< Called to request that the event handler unregisters all event
		     types it handles with the environment through the function
		     DOM_Environment::RemoveEventHandler. */
	};


	/** Interface representing an DOM Events EventTarget.  This interface is
	    implemented by objects in the DOM module and should not be inherited
	    by external classes.

	    Objects of this type are created or fetched using the function
	    DOM_EventsAPI::GetEventTarget. */
	class EventTarget
	{
	public:
		virtual DOM_Object *GetNode() = 0;
		/**< Retrieve the DOM Node that this EventTarget belongs to, if any.

		     @return A DOM Node object or NULL if the EventTarget does not
		             belong to a Node. */

		virtual OP_STATUS AddEventHandler(EventHandler *handler) = 0;
		/**< Add an event handler to the EventTarget.  If this function
		     returns OpStatus::OK, the handler is owned by the EventTarget
		     and will be deleted by it unless it is later removed using
		     RemoveEventHandler.

		     @param handler The event handler to add.

		     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

		virtual void RemoveEventHandler(EventHandler *handler) = 0;
		/**< Remove an event handler from the EventTarget.  If the event
		     handler has not been added by a call to AddEventHandler, this
		     function does nothing.

		     @param handler The event handler to remove. */

	protected:
		virtual ~EventTarget() {}
		/**< Virtual destructor so that compilers won't give bogus
		     warnings claiming this class needs a virtual destructor,
		     which it doesn't. */
	};

	static DOM_EventType GetEventType(const uni_char *type);
	/**< Return the DOM_EventType code for the event type 'type'.  If no event
	     type code is found, DOM_EVENT_NONE is returned.

	     @param type An event type.  Should not include an "on" prefix.

	     @return A DOM_EventType code. */

	static const char *GetEventTypeString(DOM_EventType type);
	/**< Return the string event type that corresponds to a DOM_EventType
	     enumerator.

	     @param type A DOM_EventType enumerator.

	     @return A string or NULL if known_type is invalid (DOM_EVENT_NONE
	             or DOM_EVENT_CUSTOM). */

	static BOOL IsWindowEvent(DOM_EventType type);
	/**< Returns TRUE if the event type is one that is sent to the Window object
	     in a DOM environment, and for which event handlers registered via
	     old-style event listener attributes on for instance the BODY element in
	     an HTML document are transferred from the element on which they were
	     specified to the Window object.

	     @param type A DOM_EventType enumerator.

	     @return TRUE or FALSE. */

	static BOOL IsWindowEventAsBodyAttr(DOM_EventType type);
	/**< Returns TRUE if the event type is one that is sent to the Window object
	     in a DOM environment, and for which event handlers registered via
	     old-style event listener attributes on for instance the BODY element in
	     an HTML document are transferred from the element on which they were
	     specified to the Window object. This includes events which are
	     dispatched on the document but bubble to the window, but only those
	     that can be declared as attributes of <body> (e.g. onscroll, onresize).

	     @param type A DOM_EventType enumerator.

	     @return TRUE or FALSE. */

	static OP_STATUS GetEventTarget(EventTarget *&event_target, DOM_Object *node);
	/**< Get or create an event target object for the given node.

	     @param event_target Set to the event target object on success.
	     @param node Node object.

	     @return OpStatus::OK, OpStatus::ERR if node is not a valid node or
	             OpStatus::ERR_NO_MEMORY on OOM. */


private:
	DOM_EventsAPI();
	DOM_EventsAPI(DOM_EventsAPI &);
};

#endif // DOM2_EVENTS_API
#endif // DOM_EVENT_H
