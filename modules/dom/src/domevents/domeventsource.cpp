/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef EVENT_SOURCE_SUPPORT

#include "modules/dom/domtypes.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/element.h"
#include "modules/dom/src/domcore/domdoc.h"
#include "modules/dom/src/domevents/domeventsource.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"
#include "modules/dom/domutils.h"

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/estimermanager.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/util/str.h"
#include "modules/util/tempbuf.h"
#include "modules/url/url2.h"
#include "modules/url/tools/url_util.h"
#include "modules/dochand/win.h"

#ifdef OPERA_CONSOLE
#include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE

/* static */ OP_STATUS
#ifdef CORS_SUPPORT
DOM_EventSource::Make(DOM_EventSource *&event_source, URL &url, DOM_EnvironmentImpl *environment, OpCrossOrigin_Request *cors_request)
#else
DOM_EventSource::Make(DOM_EventSource *&event_source, URL &url, DOM_EnvironmentImpl *environment)
#endif // CORS_SUPPORT
{
	DOM_Runtime *runtime = environment->GetDOMRuntime();
#ifdef CORS_SUPPORT
	event_source = OP_NEW(DOM_EventSource, (environment, url, cors_request));
	if (event_source && cors_request)
		event_source->m_with_credentials = cors_request->WithCredentials();
#else
	event_source = OP_NEW(DOM_EventSource, (environment, url));
#endif // CORS_SUPPORT
	return DOMSetObjectRuntime(event_source, runtime, runtime->GetPrototype(DOM_Runtime::EVENTSOURCE_PROTOTYPE), "EventSource");
}

DOM_EventSource::~DOM_EventSource()
{
	Stop(FALSE, TRUE);
	event_target = NULL;
#ifdef CORS_SUPPORT
	OP_DELETE(m_cross_origin_request);
	if (m_security_callback)
		m_security_callback->Release();
#endif // CORS_SUPPORT
	m_url_inuse.UnsetURL();
}

void
DOM_EventSource::GCTrace()
{
	DOM_EventTarget::GCTrace();
}

/* virtual */ ES_GetState
DOM_EventSource::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	/* The event properties are not found as atoms */
	enumerator->AddPropertyL("onmessage");
	enumerator->AddPropertyL("onopen");
	enumerator->AddPropertyL("onerror");

	return GET_SUCCESS;
}

/* virtual */ void
DOM_EventSource::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (m_ready_state == CLOSED)
		/* The connection was failed or closed in the meantime,
		   ignore the data. */
		return;

	switch (msg)
	{
	case MSG_URL_MOVED:
		{
			URL moved = m_url.GetAttribute(URL::KMovedToURL, TRUE);
			if (!moved.IsEmpty())
			{
				if (moved.GetServerName() != m_url.GetServerName() || OpStatus::IsError(ResetCallbacks()))
					FailConnection();
#ifdef CORS_SUPPORT
				/* Note: While with the above constraint on redirects, the origin won't change,
				         but the redirect will still have to pass the CORS "redirect steps". */
				if (OpStatus::IsError(HandleRedirect(moved)))
					FailConnection();
#endif // CORS_SUPPORT
			}
			break;
		}
	case MSG_URL_LOADING_FAILED:
#ifdef CORS_SUPPORT
		if (!m_cross_origin_request)
			if (OpCrossOrigin_Manager *manager = g_secman_instance->GetCrossOriginManager(m_url))
				/* If the failure was due to a cross-origin redirect being denied, remove
				   the request that the network layer deposited. */
				if (OpCrossOrigin_Request *request = manager->FindNetworkRedirect(m_url))
					OP_DELETE(request);
		/* Fallthrough */
#endif // CORS_SUPPORT
	case MSG_URL_DATA_LOADED:
		if (OpStatus::IsMemoryError(HandleDataLoaded()))
			if (FramesDocument *frames_doc = m_environment->GetFramesDocument())
				frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			else
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		break;
#ifdef CORS_SUPPORT
	case MSG_HEADER_LOADED:
		if (m_cross_origin_request)
		{
			BOOL allowed = FALSE;
			OpSecurityContext source_context(GetRuntime()->GetOriginURL(), m_cross_origin_request);
			OpSecurityContext target_context(m_url);
			/* This is guaranteed to be a sync security check. */
			OP_STATUS status = OpSecurityManager::CheckSecurity(OpSecurityManager::CORS_RESOURCE_ACCESS, source_context, target_context, allowed);
			if (OpStatus::IsMemoryError(status))
				g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			if (OpStatus::IsError(status))
				allowed = FALSE;

			if (!allowed)
				Stop(TRUE, TRUE);

			/* Passed the CORS access check; continue. */
			OP_DELETE(m_cross_origin_request);
			m_cross_origin_request = NULL;
		}
#endif // CORS_SUPPORT
	}
}

#ifdef CORS_SUPPORT
OP_STATUS
DOM_EventSource::HandleRedirect(const URL &moved)
{
	/* Assume that there will only be one redirect "in flight" at any one time. */
	OP_ASSERT(!m_security_callback || m_security_callback->IsFinished());
	if (!m_security_callback)
		RETURN_OOM_IF_NULL(m_security_callback = OP_NEW(OpSecurityCallbackHelper, (this)));
	else
		m_security_callback->Reset();

	if (m_cross_origin_request)
		m_cross_origin_request->SetInRedirect();

	OpSecurityContext source_context(GetRuntime(), m_cross_origin_request);
	OpSecurityContext redirect_context(moved, m_url);
	OP_STATUS status = OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_LOADSAVE, source_context, redirect_context, m_security_callback);
	/* This is a synchronous security check of the redirect. */
	OP_ASSERT(m_security_callback->IsFinished());
	if (OpStatus::IsError(status) || !m_security_callback->IsAllowed() && m_security_callback->IsFinished())
		return OpStatus::ERR;
	else
		return OpStatus::OK;
}

void
DOM_EventSource::SecurityCheckCompleted(BOOL allowed, OP_STATUS error)
{
	if (!allowed)
		FailConnection();
}
#endif // CORS_SUPPORT

/* virtual */
ES_GetState
DOM_EventSource::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_readyState:
		DOMSetNumber(value, m_ready_state);
		return GET_SUCCESS;

	case OP_ATOM_url:
		if (value)
		{
			if (m_url_string.IsEmpty())
				GET_FAILED_IF_ERROR(m_url.GetAttribute(URL::KUniName_With_Fragment, m_url_string));
			DOMSetString(value, m_url_string.CStr());
		}
		return GET_SUCCESS;

#ifdef CORS_SUPPORT
	case OP_ATOM_withCredentials:
		DOMSetBoolean(value, m_with_credentials);
		return GET_SUCCESS;
#endif // CORS_SUPPORT
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_EventSource::GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_GetState res = GetEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != GET_FAILED)
			return res;
	}
	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */
ES_PutState
DOM_EventSource::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_readyState:
	case OP_ATOM_url:
#ifdef CORS_SUPPORT
	case OP_ATOM_withCredentials:
#endif // CORS_SUPPORT
		return PUT_READ_ONLY; // read only
	}

	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/* virtual */
ES_PutState
DOM_EventSource::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState res = PutEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != PUT_FAILED)
			return res;
	}
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

OP_STATUS
DOM_EventSource::ResetCallbacks()
{
	GetMessageHandler()->UnsetCallBacks(this);
	return SetCallbacks();
}

OP_STATUS
DOM_EventSource::SetCallbacks()
{
	MessageHandler *mh = GetMessageHandler();

	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_MOVED, m_url.Id(TRUE)));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_DATA_LOADED, m_url.Id(TRUE)));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_URL_LOADING_FAILED, m_url.Id(TRUE)));
#ifdef CORS_SUPPORT
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_HEADER_LOADED, m_url.Id(TRUE)));
#endif // CORS_SUPPORT

	return OpStatus::OK;
}

static BOOL
IsEnvironmentEnabled(DOM_EnvironmentImpl *environment)
{
#ifdef DOM_WEBWORKERS_SUPPORT
	return (environment->GetFramesDocument() || environment->GetWindow()->IsA(DOM_TYPE_WEBWORKERS_WORKER));
#else
	return environment->GetFramesDocument() != NULL;
#endif // DOM_WEBWORKERS_SUPPORT
}

OP_STATUS
DOM_EventSource::Start()
{
	if (!IsEnvironmentEnabled(m_environment))
		return OpStatus::OK;

	OP_ASSERT(m_ready_state != CLOSED);

	m_ready_state = CONNECTING;

	SetCallbacks();
	g_url_api->MakeUnique(m_url);

	URL_Custom_Header accept_header_item;

	// We only accept text/event-stream responses here, so overwrite the Accept header appropriately.
	RETURN_IF_ERROR(accept_header_item.name.Set("Accept"));
	RETURN_IF_ERROR(accept_header_item.value.Set("text/event-stream"));
	RETURN_IF_ERROR(m_url.SetAttribute(URL::KAddHTTPHeader, &accept_header_item));

	if (!m_last_event_id.IsEmpty())
	{
		URL_Custom_Header header_item;

		RETURN_IF_ERROR(header_item.name.Set("Last-Event-ID"));
		RETURN_IF_ERROR(header_item.value.SetUTF8FromUTF16(m_last_event_id));
		RETURN_IF_ERROR(m_url.SetAttribute(URL::KAddHTTPHeader, &header_item));
	}

	BOOL thirdparty_determined = FALSE;
#ifdef CORS_SUPPORT
	if (m_cross_origin_request)
	{
		/* Reload() will enable cookie processing if thirdparty isn't determined. */
		RETURN_IF_ERROR(m_url.SetAttribute(URL::KDisableProcessCookies, !m_with_credentials));
		thirdparty_determined = TRUE;
	}
#endif // CORS_SUPPORT

	m_url.Reload(GetMessageHandler(), GetRuntime()->GetOriginURL(), FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, thirdparty_determined);

	return OpStatus::OK;
}

OP_STATUS
DOM_EventSource::HandleDataLoaded()
{
	if (!IsEnvironmentEnabled(m_environment))
		return Stop(FALSE, FALSE);

	OP_ASSERT(m_ready_state != CLOSED);

	if (!m_url_dd)
	{
		RETURN_IF_ERROR(m_url.SetAttribute(URL::KCacheTypeStreamCache, TRUE));
		URL& tmp_url = m_url;
		m_moved_to_url = m_url.GetAttribute(URL::KMovedToURL, TRUE);
		if (!m_moved_to_url.IsEmpty())
		{
			m_moved_to_url_inuse.SetURL(m_moved_to_url);
			tmp_url = m_moved_to_url;
		}

		UINT32 response_code = tmp_url.GetAttribute(URL::KHTTP_Response_Code);

		switch (response_code)
		{
		case HTTP_OK:
		case HTTP_MOVED:
		case HTTP_FOUND:
		case HTTP_SEE_OTHER:
		case HTTP_USE_PROXY:
		case HTTP_TEMPORARY_REDIRECT:
		case HTTP_UNAUTHORIZED:
		case HTTP_PROXY_UNAUTHORIZED:
			break;

		default:
			return FailConnection();
		}

		if (tmp_url.ContentType() != URL_EVENTSOURCE_CONTENT)
			return FailConnection();

		if (m_ready_state != OPEN)
		{
			m_ready_state = OPEN;

			DOM_Runtime *runtime = m_environment->GetDOMRuntime();
			DOM_Event *event;

			RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));

			event->InitEvent(ONOPEN, this);

			RETURN_IF_ERROR(m_environment->SendEvent(event));
		}

		unsigned short charset_id = 0;
		OpStatus::Ignore(g_charsetManager->GetCharsetID("utf-8", &charset_id));

		RETURN_OOM_IF_NULL(m_url_dd = m_url.GetDescriptor(GetMessageHandler(), TRUE, FALSE, TRUE, NULL, URL_TEXT_CONTENT, charset_id));
	}

	BOOL more = TRUE;

	while (more)
	{
		OP_STATUS status;
		TRAP_AND_RETURN_VALUE_IF_ERROR(status, m_url_dd->RetrieveDataL(more), status);

		const uni_char *data = reinterpret_cast<const uni_char *>(m_url_dd->GetBuffer());
		int data_length = UNICODE_DOWNSIZE(m_url_dd->GetBufSize());

		if (data_length > 0)
		{
			RETURN_IF_ERROR(m_buffer.Append(data, data_length));
			m_url_dd->ConsumeData(UNICODE_SIZE(data_length));
			RETURN_IF_ERROR(ParseData(more));
		}
		else
			break;

	}

	if (!more)
	{
		if (m_buffer.Length())
			RETURN_IF_ERROR(ParseData(FALSE));

		OP_ASSERT(m_buffer.Length() == 0);

		if (m_url.Status(TRUE) == URL_LOADED || m_url.Status(TRUE) == URL_LOADING_ABORTED)
			return ReestablishConnection();
		else if (m_url.Status(TRUE) != URL_LOADING)
			Stop(FALSE, FALSE);
	}

	return OpStatus::OK;
}

#define IS_CR_OR_LF(c) ((c) == 0x0a || (c) == 0x0d)
#define IS_CRLF(str_p) (*(str_p) == 0x0d && *((str_p)+1) == 0x0a)
// Given a pointer to a string, increment the pointer to skip either a \n, a \r or a \r\n.
#define SKIP_NEWLINE(str_p) do { \
								if (*(str_p) == 0x0a || *(str_p) == 0x0d && *(++(str_p)) == 0x0a) \
									++str_p; \
							} while(0);

/**
 * Check that data contains at least one complete event, meaning it returns TRUE if there's data
 * followed by two consecutive newlines, or EOF.
 * If there's more data that will arrive at a later time, it will only return TRUE if it finds two
 * consecutive newlines, since the end of the data string is not really EOF (so to speak). In the
 * more data case it will also return FALSE if the second newline is a CR and is the last character
 * on the data buffer, to avoid weirdness when a CRLF is split between url data packages.
 *
 * @param data The string containing the event(s).
 * @param more Whether there's more data to be consumed/loaded.
 */
static BOOL
IsCompleteEvent(const uni_char *data, BOOL more)
{
	BOOL newline = FALSE;

	if (!*data)
		return FALSE;

	while (*data)
	{
		if (*data == 0x0a || IS_CRLF(data) || *data == 0x0d)
		{
			/* Don't declare it a complete event just yet if the last char of the data is a CR and
			   there's more data coming later on. This is to prevent weirdness if a CRLF is split
			   between url data packages, which could lead to consider a single CRLF as two
			   newlines. This is not really a problem right now since the extra newline would be
			   ignored, but this makes it more explicit and avoid hard-to-track bugs in the future. */
			if (newline && (*data == 0x0a || !more || *(data+1)))
				return TRUE;
			newline = TRUE;
			SKIP_NEWLINE(data);
		}
		else
		{
			newline = FALSE;
			++data;
		}
	}

	return !more; // true if EOF
}

static BOOL
GetNextField(const uni_char *&data, const uni_char *&name, int &name_length, const uni_char *&value, int &value_length)
{
	name = value = 0;
	name_length = value_length = 0;

	if (!*data)
		return FALSE;

	while (TRUE)
	{
		if (IS_CR_OR_LF(*data))
		{
			SKIP_NEWLINE(data);
			return FALSE;
		}
		else if (*data == ':')
		{
			// Whole line is a comment (ignored)
			while (*data && !IS_CR_OR_LF(*data))
				++data;
			SKIP_NEWLINE(data);
		}
		else
		{
			name = data++;
			name_length = 1;

			while (*data && !IS_CR_OR_LF(*data) && *data != ':')
				++data, ++name_length;

			if (*data == ':')
			{
				++data;

				if (*data == ' ')
					++data;

				value = data;

				while (*data && !IS_CR_OR_LF(*data))
					++data, ++value_length;
			}
			else
				value = UNI_L("");

			SKIP_NEWLINE(data)

			return TRUE;
		}
	}
}

OP_STATUS
DOM_EventSource::ParseData(BOOL more)
{
	TempBuffer data, tmp;

	const uni_char *p = m_buffer.GetStorage();

	while (IsCompleteEvent(p, more))
	{
		const uni_char *name, *value, *type = NULL;
		int name_length, value_length, type_length = 0;
		BOOL seen_data_field = FALSE;

		data.Clear();

		while (GetNextField(p, name, name_length, value, value_length))
		{
			if (name_length == 2 && uni_strncmp(name, UNI_L("id"), 2) == 0)
			{
				if (value_length > 0)
					m_last_event_id.Set(value, value_length);
				else
					m_last_event_id.Set("");
			}
			else if (name_length == 4 && uni_strncmp(name, UNI_L("data"), 4) == 0)
			{
				if (seen_data_field)
					RETURN_IF_ERROR(data.Append("\n"));

				RETURN_IF_ERROR(data.Append(value, value_length));
				seen_data_field = TRUE;
			}
			else if (name_length == 5)
			{
				if (uni_strncmp(name, UNI_L("event"), 5) == 0)
				{
					if (value_length > 0)
					{
						type = value;
						type_length = value_length;
					}
					else
						type = NULL;
				}
				else if (uni_strncmp(name, UNI_L("retry"), 5) == 0 && value_length > 0)
				{
					/* Cheating a bit here, null-terminate value just for the call to uni_strtol,
					   value points to a place in the buffer, which itself _is_ null-terminated
					   (and not const), so there's no risk of touching other memory. */
					uni_char prev = value[value_length];
					const_cast<uni_char*>(value)[value_length] = 0;

					BOOL overflow;
					uni_char *endptr;
					int t = uni_strtol(value, &endptr, 10, &overflow);

					if (!*endptr && !overflow && t > 0)
						m_reconnection_time = t; // ignore if there's strange chars or if too big.

					const_cast<uni_char*>(value)[value_length] = prev;
				}
			}
			// else: ignore field
		}

		if (data.Length())
		{
			DOM_Runtime *runtime = m_environment->GetDOMRuntime();
			DOM_MessageEvent *event;

			RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_MessageEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::CROSSDOCUMENT_MESSAGEEVENT_PROTOTYPE), "MessageEvent"));

			if (type)
			{
				event->InitEvent(DOM_EVENT_CUSTOM, this);
				tmp.Clear();
				RETURN_IF_ERROR(tmp.Append(type, type_length));
				event->SetType(tmp.GetStorage());
			}
			else
				event->InitEvent(ONMESSAGE, this);

			if (!m_last_event_id.IsEmpty())
				RETURN_IF_ERROR(event->SetLastEventId(m_last_event_id.CStr()));

			ES_Value es_data;
			DOMSetString(&es_data, &data);
			event->SetData(es_data);

			tmp.Clear();
			RETURN_IF_ERROR(runtime->GetSerializedOrigin(tmp));
			RETURN_IF_ERROR(event->SetOrigin(tmp.GetStorage()));

			RETURN_IF_ERROR(m_environment->SendEvent(event));
		}
	}

	m_buffer.Delete(0, p - m_buffer.GetStorage());

	return OpStatus::OK;
}

OP_STATUS
DOM_EventSource::ScheduleReopenConnection()
{
	OP_ASSERT(m_ready_state != CLOSED);

	Stop(FALSE, FALSE);

	if (!m_reopen_timer_event)
	{
		RETURN_OOM_IF_NULL(m_reopen_timer_event = OP_NEW(ES_ReopenConnectionTimerEvent, (this, m_reconnection_time)));
		OP_STATUS ret = m_environment->GetScheduler()->GetTimerManager()->AddEvent(m_reopen_timer_event);
		if (OpStatus::IsMemoryError(ret))
		{
			m_reopen_timer_event = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_EventSource::DoReopenConnection()
{
	m_reopen_timer_event = NULL;
	return m_ready_state != CLOSED ? Start() : OpStatus::OK;
}

void
DOM_EventSource::CancelReopen(BOOL cancel_event)
{
	if (m_reopen_timer_event)
	{
		if (cancel_event)
			OpStatus::Ignore(m_reopen_timer_event->GetTimerManager()->RemoveEvent(m_reopen_timer_event->GetId()));
		m_reopen_timer_event = NULL;
	}
}

OP_STATUS
DOM_EventSource::Stop(BOOL send_error_event, BOOL close)
{
	if (m_ready_state != CLOSED)
	{
		CancelReopen(TRUE);

		MessageHandler *mh = GetMessageHandler();

		mh->UnsetCallBacks(this);

		if (!m_url.IsEmpty())
			m_url.StopLoading(mh);

		OP_DELETE(m_url_dd);
		m_url_dd = NULL;

		if (close)
		{
			m_ready_state = CLOSED;
			/* Event sources don't do anything after they're closed, so get it out of the list of
			   active sources in the environment, so that it's not kept needlessly alive for the
			   lifetime of the environment and can be GCed appropriately. */
			Out();
		}

		if (send_error_event)
		{
			DOM_Runtime *runtime = m_environment->GetDOMRuntime();
			DOM_Event *event;

			RETURN_IF_ERROR(DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));

			event->InitEvent(ONERROR, this);

			RETURN_IF_ERROR(m_environment->SendEvent(event));
		}
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_EventSource::FailConnection()
{
	OP_ASSERT(m_ready_state != CLOSED);

	return Stop(TRUE, TRUE);
}

OP_STATUS
DOM_EventSource::ReestablishConnection()
{
	OP_ASSERT(m_ready_state != CLOSED);

	m_ready_state = CONNECTING;

	RETURN_IF_ERROR(Stop(TRUE, FALSE));

	return ScheduleReopenConnection();
}

/* static */
void
DOM_EventSource::ConstructEventSourceObjectL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "CONNECTING", CONNECTING, runtime);
	PutNumericConstantL(object, "OPEN", OPEN, runtime);
	PutNumericConstantL(object, "CLOSED", CLOSED, runtime);
}

int
DOM_EventSource::close(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(event_source, DOM_TYPE_EVENTSOURCE, DOM_EventSource);

	event_source->Stop(FALSE, TRUE);

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_EventSource)
	DOM_FUNCTIONS_FUNCTION(DOM_EventSource, DOM_EventSource::close, "close", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_EventSource, DOM_Node::dispatchEvent, "dispatchEvent", NULL)
DOM_FUNCTIONS_END(DOM_EventSource)

DOM_FUNCTIONS_WITH_DATA_START(DOM_EventSource)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_EventSource, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_EventSource, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
#ifdef DOM3_EVENTS
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_EventSource, DOM_Node::accessEventListener, 2, "addEventListenerNS", "ss-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_EventSource, DOM_Node::accessEventListener, 3, "removeEventListenerNS", "ss-b-")
#endif // DOM3_EVENTS
DOM_FUNCTIONS_WITH_DATA_END(DOM_EventSource)

class EventSourceConstruct_State;

class EventSourceConstruct_Callback
	: public OpSecurityCheckCallback
{
public:
	EventSourceConstruct_Callback(ES_Thread *thread)
		: m_thread(thread)
		, m_state(NULL)
		, m_is_allowed(FALSE)
		, m_is_finished(FALSE)
		, m_error(OpStatus::OK)
#ifdef CORS_SUPPORT
		, m_cross_origin_request(NULL)
#endif // CORS_SUPPORT
	{
	}

	virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type = PERSISTENCE_NONE)
	{
		m_is_allowed = allowed;
		Finish();
	}

	virtual void OnSecurityCheckError(OP_STATUS error)
	{
		m_error = error;
		Finish();
	}

	void Cancel()
	{
		m_is_finished = TRUE;
		m_thread = NULL;
	}

	OP_STATUS GetError() const { return m_error; }
	BOOL IsFinished() const { return m_is_finished; }
	BOOL IsAllowed() const { return m_is_allowed; }

#ifdef CORS_SUPPORT
	void SetCrossOriginRequest(OpCrossOrigin_Request *request)
	{
		m_cross_origin_request = request;
	}

	OpCrossOrigin_Request *GetCrossOriginRequest() { return m_cross_origin_request; }
#endif // CORS_SUPPORT

	void SetIsAsync(EventSourceConstruct_State *state)
	{
		m_state = state;
	}

protected:
	BOOL IsAsync() const { return m_state != NULL; }

	void Finish()
	{
		if (!m_is_finished)
		{
			m_is_finished = TRUE;
			if (IsAsync())
				m_thread->Unblock();
		}
		else // Has been cancelled.
			OP_DELETE(this);
	}

private:
	ES_Thread *m_thread;
	EventSourceConstruct_State *m_state;
	BOOL m_is_allowed;
	BOOL m_is_finished;
	OP_STATUS m_error;
#ifdef CORS_SUPPORT
	OpCrossOrigin_Request *m_cross_origin_request;
#endif // CORS_SUPPORT
};

class EventSourceConstruct_State : public DOM_Object, public ES_ThreadListener
{
public:
	static OP_STATUS Make(EventSourceConstruct_State *&new_object, URL url, EventSourceConstruct_Callback *callback, DOM_Runtime *runtime, ES_Thread *thread)
	{
		new_object = OP_NEW(EventSourceConstruct_State, (url, callback, thread));
		OP_STATUS status = DOMSetObjectRuntime(new_object, runtime);
		if (OpStatus::IsError(status))
			callback->Cancel();
		return status;
	}

	URL GetUrl() const { return m_url; }
	EventSourceConstruct_Callback *GetCallback() const { return m_callback; }

	// From ES_ThreadListener
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		OP_ASSERT(m_thread == thread);
		switch (signal)
		{
		case ES_SIGNAL_SCHEDULER_TERMINATED:
			break; // Doesn't matter to us
		case ES_SIGNAL_CANCELLED:
		case ES_SIGNAL_FINISHED:
		case ES_SIGNAL_FAILED:
			m_thread = NULL;
			m_callback->Cancel();
			Remove();	// Prevent deletion of this object
			break;
		default:
			OP_ASSERT(FALSE);
		}
		return OpStatus::OK;
	}

	void PrepareSuspend()
	{
		m_callback->SetIsAsync(this);
		m_thread->AddListener(this);
		m_thread->Block();
	}

private:
	EventSourceConstruct_State(URL url, EventSourceConstruct_Callback *callback, ES_Thread *thread) : m_url(url), m_callback(callback), m_thread(thread) { }

	URL m_url;
	EventSourceConstruct_Callback *m_callback;
	ES_Thread *m_thread;
};

#ifdef CORS_SUPPORT
static OP_STATUS
MakeCrossOriginRequest(OpCrossOrigin_Request *&request, const URL &origin_url, const URL &target_url, BOOL with_credentials)
{
	const uni_char *method = UNI_L("GET");
	BOOL is_anonymous = FALSE;
	return OpCrossOrigin_Request::Make(request, origin_url, target_url, method, with_credentials, is_anonymous);
}
#endif // CORS_SUPPORT

/* virtual */
int
DOM_EventSource_Constructor::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	if (!IsEnvironmentEnabled(GetEnvironment()))
		return ES_FAILED;

	URL origin_url = GetRuntime()->GetOriginURL();
	BOOL bad_url = FALSE;
	URL url;
#ifdef CORS_SUPPORT
	BOOL with_credentials = FALSE;
#endif // CORS_SUPPORT

	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("s|o");
		url = g_url_api->GetURL(origin_url, argv[0].value.string, FALSE);

		// Throw syntax error for badly formed urls, fake a network error for non-http ones.
		if (DOM_Utils::IsOperaIllegalURL(url))
			return CallDOMException(SYNTAX_ERR, return_value);

		switch (url.Type())
		{
		case URL_HTTP:
		case URL_HTTPS:
			break;
		default:
			bad_url = TRUE;
			break;
		}
#ifdef CORS_SUPPORT
		if (argc >= 2)
			with_credentials = DOMGetDictionaryBoolean(argv[1].value.object, UNI_L("withCredentials"), FALSE);
#endif // CORS_SUPPORT
	}

	FramesDocument *frames_doc = GetEnvironment()->GetFramesDocument();
	DOM_EventSource *event_source;
	URL empty_url;
	BOOL allowed = FALSE;
#ifdef CORS_SUPPORT
	OpCrossOrigin_Request *cors_request = NULL;
#endif // CORS_SUPPORT

	if (!bad_url)
	{
		EventSourceConstruct_Callback *callback = NULL;
		if (argc > 0)
		{
			ES_Thread *thread = GetCurrentThread(origining_runtime);
			callback = OP_NEW(EventSourceConstruct_Callback, (thread));
			if (!callback)
				return ES_NO_MEMORY;

			OpSecurityContext target_context(url);
			OpSecurityContext source_context(origin_url);
			OpSecurityManager::Operation security_operation = OpSecurityManager::DOM_STANDARD;
			if (frames_doc)
#ifdef GADGET_SUPPORT
				if (frames_doc->GetWindow()->GetGadget())
				{
					security_operation = OpSecurityManager::GADGET_STANDARD;
					source_context = OpSecurityContext(frames_doc->GetWindow()->GetGadget());
				}
				else
#endif // GADGET_SUPPORT
					source_context = OpSecurityContext(frames_doc->GetSecurityContext());
#ifdef CORS_SUPPORT
			if (security_operation == OpSecurityManager::DOM_STANDARD)
			{
				BOOL allowable = FALSE;
				CALL_FAILED_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::CORS_PRE_ACCESS_CHECK, OpSecurityContext(origin_url), target_context, allowable));
				if (allowable)
				{
					RETURN_IF_ERROR(MakeCrossOriginRequest(cors_request, origin_url, url, with_credentials));
					callback->SetCrossOriginRequest(cors_request);
				}
#if defined(GADGET_SUPPORT) && !defined(SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT)
				else if (!frames_doc || frames_doc->GetWindow()->GetType() != WIN_TYPE_GADGET)
#else
				else
#endif // GADGET_SUPPORT && !SECMAN_CROSS_ORIGIN_XHR_GADGET_SUPPORT
					/* No cross-origin redirects to be attempted. */
					RETURN_IF_ERROR(url.SetAttribute(URL::KFollowCORSRedirectRules, URL::CORSRedirectDenySimpleCrossOrigin));

				if (frames_doc)
					source_context = OpSecurityContext(frames_doc->GetSecurityContext(), cors_request);
				else
					source_context = OpSecurityContext(origin_url, cors_request);
			}
#endif // CORS_SUPPORT

			OP_STATUS status = g_secman_instance->CheckSecurity(security_operation, source_context, target_context, callback);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(callback);
				CALL_FAILED_IF_ERROR(status);
			}

			if (!callback->IsFinished())
			{
				EventSourceConstruct_State *state;
				CALL_FAILED_IF_ERROR(EventSourceConstruct_State::Make(state, url, callback, GetRuntime(), thread));
				state->PrepareSuspend();
				DOMSetObject(return_value, state);
				return ES_SUSPEND | ES_RESTART;
			}
		}
		else
		{
			OP_ASSERT(return_value->type == VALUE_OBJECT && return_value->value.object);
			EventSourceConstruct_State *state = DOM_HOSTOBJECT(return_value->value.object, EventSourceConstruct_State);

			callback = state->GetCallback();
			url = state->GetUrl();
#ifdef CORS_SUPPORT
			cors_request = callback->GetCrossOriginRequest();
#endif // CORS_SUPPORT
			state->Remove(); // Stop listening on the thread.
		}
		OP_ASSERT(callback);
		OP_ASSERT(callback->IsFinished());

		allowed = callback->IsAllowed();
		OP_DELETE(callback);
	}

#ifdef CORS_SUPPORT
	CALL_FAILED_IF_ERROR(DOM_EventSource::Make(event_source, allowed ? url : empty_url, GetEnvironment(), cors_request));
#else
	CALL_FAILED_IF_ERROR(DOM_EventSource::Make(event_source, allowed ? url : empty_url, GetEnvironment()));
#endif // CORS_SUPPORT

	GetEnvironment()->AddEventSource(event_source);

	if (!allowed || bad_url)
	{
#ifdef OPERA_CONSOLE
		if (g_console->IsLogging())
		{
			const uni_char *error_message;
			if (!allowed)
				error_message = UNI_L("Unable to connect to event source: security violation.");
			else if (bad_url)
				error_message = UNI_L("Unable to connect to event source: invalid URL.");
			else
				error_message = UNI_L("Unable to connect to event source.");

			OpConsoleEngine::Message console_message(OpConsoleEngine::EcmaScript, OpConsoleEngine::Error);
			if (frames_doc)
				console_message.window = frames_doc->GetWindow()->Id();

			if (OpStatus::IsSuccess(console_message.message.Set(error_message))
				&& OpStatus::IsSuccess(console_message.context.Set(UNI_L("EventSource")))
				&& OpStatus::IsSuccess(GetRuntime()->GetDisplayURL(console_message.url)))
			{
				TRAPD(result, g_console->PostMessageL(&console_message));
				OpStatus::Ignore(result);
			}
		}
#endif // OPERA_CONSOLE

		// Don't raise a security error, just act as if there was an error fetching the url.
		CALL_FAILED_IF_ERROR(event_source->FailConnection());
	}
	else
		CALL_FAILED_IF_ERROR(event_source->Start());

	DOMSetObject(return_value, event_source);
	return ES_VALUE;
}

/* static */ void
DOM_EventSource_Constructor::AddConstructorL(DOM_Object *object)
{
	DOM_EventSource_Constructor* eventsource_constructor = OP_NEW_L(DOM_EventSource_Constructor, ());
	object->PutConstructorL(eventsource_constructor, "s{withCredentials:b}-");
	DOM_EventSource::ConstructEventSourceObjectL(*eventsource_constructor, object->GetRuntime());
}

/* virtual */
DOM_EventSource::ES_ReopenConnectionTimerEvent::~ES_ReopenConnectionTimerEvent()
{
	if (source)
		source->CancelReopen(FALSE);
}

/* virtual */ OP_BOOLEAN
DOM_EventSource::ES_ReopenConnectionTimerEvent::Expire()
{
	if (OpStatus::IsMemoryError(source->DoReopenConnection()))
		return OpBoolean::ERR_NO_MEMORY;
	return OpBoolean::IS_FALSE;
}

#endif // EVENT_SOURCE_SUPPORT
