/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_EVENTSOURCE_H
#define DOM_EVENTSOURCE_H

#ifdef EVENT_SOURCE_SUPPORT

#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/ecmascript_utils/estimerevent.h"
#ifdef CORS_SUPPORT
#include "modules/security_manager/include/security_manager.h"
#endif // CORS_SUPPORT
#include "modules/util/simset.h"

class DOM_Element;
class DOM_EventSource;
class URL;

class DOM_EventSource
	: public DOM_Object
	, public DOM_EventTarget
	, public DOM_EventTargetOwner
	, public ListElement<DOM_EventSource>
	, public MessageObject
#ifdef CORS_SUPPORT
	, public OpSecurityCallbackOwner
#endif // CORS_SUPPORT
{
public:
	friend class ES_ReopenConnectionTimerEvent;

	enum ReadyState
	{
		CONNECTING = 0,
		OPEN = 1,
		CLOSED = 2
	};

#ifdef CORS_SUPPORT
	static OP_STATUS Make(DOM_EventSource *&event_source, URL &url, DOM_EnvironmentImpl *environment, OpCrossOrigin_Request *cors_request);
#else
	static OP_STATUS Make(DOM_EventSource *&event_source, URL &url, DOM_EnvironmentImpl *environment);
#endif // CORS_SUPPORT

	virtual ~DOM_EventSource();

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	virtual DOM_Object *GetOwnerObject() { return this; };

	virtual BOOL IsA(int type) { return type == DOM_TYPE_EVENTSOURCE || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

	OP_STATUS Start();
	OP_STATUS FailConnection();
	OP_STATUS ReestablishConnection();
	OP_STATUS Stop(BOOL send_error_event, BOOL close);

#ifdef CORS_SUPPORT
	virtual void SecurityCheckCompleted(BOOL allowed, OP_STATUS error);
#endif // CORS_SUPPORT

	/** Initialize the global variable "EventSource". */
	static void ConstructEventSourceObjectL(ES_Object *object, DOM_Runtime *runtime);

	DOM_DECLARE_FUNCTION(close);
	// Also: dispatchEvent
	enum { FUNCTIONS_ARRAY_SIZE = 3 };

	// {add,remove}EventListener{,NS}
	enum {
		FUNCTIONS_WITH_DATA_BASIC = 2,
#ifdef DOM3_EVENTS
		FUNCTIONS_WITH_DATA_addEventListenerNS,
		FUNCTIONS_WITH_DATA_removeEventListenerNS,
#endif // DOM3_EVENTS
		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

private:
	class ES_ReopenConnectionTimerEvent : public ES_TimerEvent
	{
	public:
		DOM_EventSource *source;

		ES_ReopenConnectionTimerEvent(DOM_EventSource *source, int delay)
			: ES_TimerEvent(delay, FALSE),
			  source(source) {}

		virtual ~ES_ReopenConnectionTimerEvent();
		virtual OP_BOOLEAN Expire();
	};
	friend class ES_ReopenConnectionTimerEvent;

	DOM_EnvironmentImpl *m_environment;
	URL m_url;
	URL_InUse m_url_inuse;
	URL m_moved_to_url; // See CORE-19401. Get rid of this once it's fixed.
	URL_InUse m_moved_to_url_inuse;
	URL_DataDescriptor *m_url_dd;
	OpString m_url_string;
	TempBuffer m_buffer;
	ES_ReopenConnectionTimerEvent *m_reopen_timer_event;
	OpString m_last_event_id;
	ReadyState m_ready_state;
	int m_reconnection_time;

#ifdef CORS_SUPPORT
	BOOL m_with_credentials;
	OpCrossOrigin_Request *m_cross_origin_request;
	OpSecurityCallbackHelper *m_security_callback;

	OP_STATUS HandleRedirect(const URL &moved_to_url);
	/**< Verify that a redirect during a cross-origin request
	     adheres to CORS. Returns OpStatus::ERR if not
	     allowed, OpStatus::OK if redirect is acceptable.
	     OpStatus::ERR_NO_MEMORY on OOM. */

	DOM_EventSource(DOM_EnvironmentImpl *environment, URL &url, OpCrossOrigin_Request *cors_request)
#else
	DOM_EventSource(DOM_EnvironmentImpl *environment, URL &url)
#endif // CORS_SUPPORT
		: DOM_EventTarget(NULL)
		, m_environment(environment)
		, m_url(url)
		, m_url_inuse(url)
		, m_url_dd(NULL)
		, m_reopen_timer_event(NULL)
		, m_ready_state(CONNECTING)
		, m_reconnection_time(3000)
#ifdef CORS_SUPPORT
		, m_with_credentials(FALSE)
		, m_cross_origin_request(cors_request)
		, m_security_callback(NULL)
#endif // CORS_SUPPORT
	{
		DOM_EventTarget::owner = this;
		DOM_EventTargetOwner::event_target = this;
	}

	MessageHandler *GetMessageHandler() { return g_main_message_handler; }
	OP_STATUS SetCallbacks();
	OP_STATUS ResetCallbacks();

	OP_STATUS HandleDataLoaded();
	OP_STATUS ParseData(BOOL more);
	OP_STATUS ScheduleReopenConnection();
	OP_STATUS DoReopenConnection();
	void CancelReopen(BOOL cancel_event);
};

class DOM_EventSource_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_EventSource_Constructor()
		: DOM_BuiltInConstructor(DOM_Runtime::EVENTSOURCE_PROTOTYPE)
	{
	}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);

	static void AddConstructorL(DOM_Object *object);
};

#endif // EVENT_SOURCE_SUPPORT
#endif // DOM_EVENTSOURCE_H
