/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_EVENTLISTENER_H
#define DOM_EVENTLISTENER_H

#include "modules/dom/domeventtypes.h"
#include "modules/dom/domevents.h"

#include "modules/util/simset.h"

class ES_Object;
class DOM_Object;
class DOM_Event;
class DOM_Node;
class DOM_EnvironmentImpl;
class DOM_Runtime;

class DOM_EventListener
	: public Link
{
private:
	enum Type { UNKNOWN, NATIVE, EXTERNAL };

	Type listener_type;

	union
	{
		struct
		{
			DOM_EventType known_type;
			uni_char *type;
			unsigned int capture:1;
			// These two always have the same value for HTML events but differ for SVG.
			unsigned int return_value_controls_propagation:1;
			unsigned int from_attribute:1;
			ES_Object *this_object;
			ES_Object *handler;
			uni_char *handler_text;
#ifdef DOM3_EVENTS
			ES_Object *group;
#endif // DOM3_EVENTS
		} native;
#ifdef DOM2_EVENTS_API
		struct
		{
			DOM_EventsAPI::EventHandler *handler;
		} external;
#endif // DOM2_EVENTS_API
	} data;

#ifdef ECMASCRIPT_DEBUGGER
	/// The ID of the script that registered the listener, or ~0u if this information is unavailable.
	unsigned caller_script_guid;
	/// The line number where the listener was registered, or ~0u if this information is unavailable.
	unsigned caller_line_no;
#endif // ECMASCRIPT_DEBUGGER

	int ref_count;

public:
	DOM_EventListener();
	~DOM_EventListener();

	OP_STATUS SetNativeText(DOM_EnvironmentImpl *environment, DOM_EventType known_type, const uni_char *handler_text, unsigned handler_text_length, ES_Object *this_object);
	OP_STATUS SetNative(DOM_EnvironmentImpl *environment, DOM_EventType known_type, const uni_char *type, BOOL capture, BOOL old_style, ES_Object *this_object, ES_Object *handler);

#ifdef WEBSOCKETS_SUPPORT
	void ResetNativeHandler(ES_Object *handler);
#endif //WEBSOCKETS_SUPPORT

#ifdef DOM2_EVENTS_API
	void SetExternal(DOM_EventsAPI::EventHandler *handler);
#endif // DOM2_EVENTS_API

#ifdef ECMASCRIPT_DEBUGGER
	/**
	 * Sets information about the caller which registered this event listener.
	 * @param script_guid The ID of the script which did the call.
	 * @param line_no The line number where the call happened.
	 */
	void SetCallerInformation(unsigned script_guid, unsigned line_no);
	/**
	 * @return TRUE if it contains caller information, FALSE otherwise.
	 */
	BOOL HasCallerInformation() const { return GetCallerScriptGuid() != ~0u && GetCallerLineNo() != ~0u; }
	/**
	 * @return The ID of script that registered this listener, or ~0u if no script was set.
	 */
	unsigned GetCallerScriptGuid() const { return caller_script_guid; }
	/**
	 * @return The line number where this listener was registered, or ~0u if no line was set.
	 */
	unsigned GetCallerLineNo() const { return caller_line_no; }
#endif // ECMASCRIPT_DEBUGGER

	OP_STATUS CompileHandler(DOM_Object *target, ES_Thread *interrupt_thread = NULL);

	void Reset();

	BOOL HandlesEvent(DOM_Event *event, ES_EventPhase phase);
	BOOL IsRegistered() { return InList(); }

	BOOL HandlesEvent(DOM_EventType known_type, const uni_char *type, ES_EventPhase phase);

	BOOL CanOverride(DOM_EventListener *other);

#ifdef SVG_SUPPORT
	void SetPropagationControlledByReturnValue(BOOL new_val) { data.native.return_value_controls_propagation = new_val; }
#endif // SVG_SUPPORT
	/**
	 * @return TRUE if propagation of event is controlled by return value, FALSE otherwise.
	 */
	BOOL IsPropagationControlledByReturnValue() const { return listener_type == NATIVE && data.native.return_value_controls_propagation; }
	/**
	 * @return TRUE if this listener was registered from an on* attribute, FALSE otherwise.
	 */
	BOOL IsFromAttribute() const { return listener_type == NATIVE && data.native.from_attribute; }
	/**
	 * @return The DOM event type that this listener is registered for, for
	 *         custom events it returns DOM_EVENT_CUSTOM in which case
	 *         GetCustomEventType() can be called to get the name.
	 */
	DOM_EventType GetKnownType() const { OP_ASSERT(listener_type == NATIVE); return data.native.known_type; }
	/**
	 * @return TRUE if the listener is native ECMAScript listener, FALSE if the listener is a pure C++ listener.
	 */
	BOOL IsNativeHandler() const { return listener_type == NATIVE; }
	/**
	 * Returns the function object for the native listener.
	 * If the listener was created from an on* attribute then the object might not exist yet as it has not been compiled, call GetNativeCode() to get the code.
	 * This must only be called if IsNativeHandler() is TRUE.
	 * @return An ES_Object which represents the function, or NULL if there is no function object yet.
	 */
	ES_Object *GetNativeHandler() const { OP_ASSERT(listener_type == NATIVE); return data.native.handler; }
	/**
	 * Returns the code for the native listener. The code is only set for on* attributes and will only exist until it gets compiled, in which case GetNativeHandler() should be used.
	 * @note This must only be called if IsNativeHandler() is TRUE.
	 * @return The string containing the code, or NULL if the code is no longer available.
	 */
	const uni_char *GetNativeCode() const { OP_ASSERT(listener_type == NATIVE); return data.native.handler_text; }
	/**
	 * @note This must only be called if IsNativeHandler() is TRUE.
	 * @return The name of the custom event, or NULL if the listener is not for a custom event.
	 */
	const uni_char *GetCustomEventType() const { OP_ASSERT(listener_type == NATIVE); return GetKnownType() == DOM_EVENT_CUSTOM ? data.native.type : NULL; }
	/**
	 * @return TRUE if the listener was registered with the has_capture parameter set to true, FALSE otherwise.
	 */
	BOOL IsCapture() const { OP_ASSERT(listener_type == NATIVE); return data.native.capture; }

#ifdef DEBUG_ENABLE_OPASSERT
	static BOOL IsNativeHandler(DOM_EventListener *listener) { return listener->listener_type == NATIVE; }
	static DOM_EventType GetNativeKnownType(DOM_EventListener *listener) { return listener->data.native.known_type; }
#endif // DEBUG_ENABLE_OPASSERT

	OP_STATUS HandleEvent(DOM_Event *event, ES_Thread *interrupt_thread = NULL);

	void RegisterHandlers(DOM_EnvironmentImpl *environment);
	void UnregisterHandlers(DOM_EnvironmentImpl *environment);

	void GCTrace(DOM_Runtime *runtime);

	static void IncRefCount(DOM_EventListener *listener);
	static void DecRefCount(DOM_EventListener *listener);
};

#endif // DOM_EVENTLISTENER_H
