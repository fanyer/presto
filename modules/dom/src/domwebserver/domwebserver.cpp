/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1996-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Haavard Molland haavardm@opera.com
*/
#include "core/pch.h"

#include "modules/dom/src/domwebserver/domwebserver.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/dom/src/opera/dombytearray.h"
#include "modules/webserver/webserver_request.h"
#include "modules/webserver/webserver_filename.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_custom_resource.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/webserver/webserver_body_objects.h"
#include "modules/formats/hdsplit.h"
#include "modules/util/opfile/opfile.h"
#include "modules/formats/argsplit.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/opera/domwidget.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/viewers/viewers.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/util/datefun.h"
#include "modules/dom/src/opera/domio.h"
#ifdef DOM_GADGET_FILE_API_SUPPORT
# include "modules/dom/src/opera/domgadgetfile.h"
#endif // DOM_GADGET_FILE_API_SUPPORT
#include "modules/logdoc/urlimgcontprov.h"

#include "modules/util/gen_str.h"

DOM_WebServerRequestEvent::DOM_WebServerRequestEvent(DOM_WebServerConnection *connection)
	: m_connection(connection)
{
}

/* virtual */
DOM_WebServerRequestEvent::~DOM_WebServerRequestEvent()
{
}

/* virtual */ void
DOM_WebServerRequestEvent::GCTrace()
{
	DOM_Event::GCTrace();
	GCMark(m_connection);
}

/* static */ OP_STATUS
DOM_WebServerRequestEvent::CreateAndSendEvent(const uni_char *type, DOM_WebServer *webserver, WebResource_Custom *web_resource_script)
{
	OP_ASSERT(type && *type);
	OP_ASSERT(webserver);

	DOM_Runtime *runtime = webserver->GetRuntime();

	DOM_WebServerConnection *connection;

	RETURN_IF_ERROR(DOM_WebServerConnection::Make(connection, webserver, runtime, web_resource_script));

	webserver->IncreaseNumberOfOpenedConnections();

	if (webserver->GetEventTarget() != NULL &&
		webserver->GetEventTarget()->HasListeners(DOM_EVENT_CUSTOM, type, ES_PHASE_AT_TARGET))
	{
		DOM_WebServerRequestEvent *event = OP_NEW(DOM_WebServerRequestEvent, (connection));
		RETURN_IF_ERROR(DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "WebServerRequestEvent"));

		event->InitEvent(DOM_EVENT_CUSTOM, webserver);

		RETURN_IF_ERROR(event->SetType(type));
		RETURN_IF_ERROR(runtime->GetEnvironment()->SendEvent(event));
	}

	if (webserver->GetEventTarget() != NULL &&
		webserver->GetEventTarget()->HasListeners(DOM_EVENT_CUSTOM, UNI_L("_request"), ES_PHASE_AT_TARGET))
	{
		DOM_WebServerRequestEvent *new_request_event = OP_NEW(DOM_WebServerRequestEvent, (connection));
		RETURN_IF_ERROR(DOMSetObjectRuntime(new_request_event, runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "WebServerRequestEvent"));

		new_request_event->InitEvent(DOM_EVENT_CUSTOM, webserver);

		RETURN_IF_ERROR(new_request_event->SetType(UNI_L("_request")));
		RETURN_IF_ERROR(runtime->GetEnvironment()->SendEvent(new_request_event));
	}

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_WebServerRequestEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_connection:
		DOMSetObject(value, m_connection);
		return GET_SUCCESS;

	case OP_ATOM_id:
		DOMSetNumber(value, m_connection->GetConnectionId());
		return GET_SUCCESS;

	default:
		return DOM_Event::GetName(property_name, value, origining_runtime);
	}
}

DOM_WebServerConnectionClosedEvent::DOM_WebServerConnectionClosedEvent(unsigned int connection_id)
	: m_connection_id(connection_id)
{
}

/* virtual */
DOM_WebServerConnectionClosedEvent::~DOM_WebServerConnectionClosedEvent()
{}

/* static */ OP_STATUS
DOM_WebServerConnectionClosedEvent::CreateAndSendEvent(unsigned int connection_id, DOM_WebServer *webserver)
{
	DOM_Runtime *runtime = webserver->GetRuntime();
	if (webserver->GetEventTarget() != NULL &&
		webserver->GetEventTarget()->HasListeners(DOM_EVENT_CUSTOM, UNI_L("_close"), ES_PHASE_AT_TARGET))
	{

		DOM_WebServerConnectionClosedEvent *connection_closed_event = OP_NEW(DOM_WebServerConnectionClosedEvent, (connection_id));

		RETURN_IF_ERROR(DOMSetObjectRuntime(connection_closed_event, runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "WebServerConnectionCloseEvent"));

		connection_closed_event->InitEvent(DOM_EVENT_CUSTOM, webserver);

		RETURN_IF_ERROR(connection_closed_event->SetType(UNI_L("closed")));
		RETURN_IF_ERROR(runtime->GetEnvironment()->SendEvent(connection_closed_event));
	}
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_WebServerConnectionClosedEvent::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_connection:
		DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_id:
		DOMSetNumber(value, m_connection_id);
		return GET_SUCCESS;

	default:
		return DOM_Event::GetName(property_name, value, origining_runtime);
	}
}

DOM_WebServerConnection::DOM_WebServerConnection(WebResource_Custom *web_resource_script, DOM_WebServer *webserver)
	: m_connection_is_alive(TRUE),
	  m_webserver(webserver),
	  m_request(NULL),
	  m_response(NULL),
	  m_connection_id(0),
	  m_web_resource_script(web_resource_script)
{
}

/* virtual */
DOM_WebServerConnection::~DOM_WebServerConnection()
{
	GetWebResourceCustom()->RemoveHttpConnectionListener(this);
}

/* static */ OP_STATUS
DOM_WebServerConnection::Make(DOM_WebServerConnection *&connection, DOM_WebServer *webserver,  DOM_Runtime *runtime, WebResource_Custom *web_resource_script)
{
	DOM_WebServerRequest *request;
	DOM_WebServerResponse *response;

	RETURN_IF_ERROR(DOMSetObjectRuntime(connection = OP_NEW(DOM_WebServerConnection, (web_resource_script, webserver)), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERCONNECTION_PROTOTYPE), "WebServerConnection"));

	RETURN_IF_ERROR(web_resource_script->GetConnection()->GetProtocol(connection->m_protocol));

	RETURN_IF_ERROR(DOM_WebServerResponse::Make(response, runtime, web_resource_script, connection));
	RETURN_IF_ERROR(DOM_WebServerRequest::Make(request, runtime, web_resource_script, connection));

	connection->m_response = response;
	connection->m_request = request;

	connection->m_connection_id = webserver->GetNumberOfOpenedConnections();

	web_resource_script->AddHttpConnectionListener(connection);
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_WebServerConnection::WebServerConnectionKilled()
{
	if (m_connection_is_alive)
	{
		m_connection_is_alive = FALSE;
		RETURN_IF_ERROR(DOM_WebServerConnectionClosedEvent::CreateAndSendEvent(m_connection_id, m_webserver));
	}
	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_WebServerConnection::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_request:
		DOMSetObject(value, m_request);
		return GET_SUCCESS;

	case OP_ATOM_response:
		DOMSetObject(value, m_connection_is_alive ? m_response : NULL);
		return GET_SUCCESS;

	case OP_ATOM_id:
		DOMSetNumber(value, m_connection_id);
		return GET_SUCCESS;

	case OP_ATOM_closed:
		DOMSetBoolean(value, !m_connection_is_alive);
		return GET_SUCCESS;

	case OP_ATOM_isProxied:
		DOMSetBoolean(value, GetWebResourceCustom()->IsProxied());
		return GET_SUCCESS;

#if defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	case OP_ATOM_isOwner:
		DOMSetBoolean(value, GetWebResourceCustom()->ClientIsOwner());
		return GET_SUCCESS;

	case OP_ATOM_isLocal:
		DOMSetBoolean(value, GetWebResourceCustom()->ClientIsLocal());
		return GET_SUCCESS;

#endif // WEBSERVER_DIRECT_SOCKET_SUPPORT

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_WebServerConnection::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	// All the attributes are read only.
	switch (property_name)
	{
	case OP_ATOM_request:
	case OP_ATOM_response:
	case OP_ATOM_id:
	case OP_ATOM_closed:
	case OP_ATOM_isProxied:
#if defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	case OP_ATOM_isOwner:
	case OP_ATOM_isLocal:
#endif // WEBSERVER_DIRECT_SOCKET_SUPPORT

		return PUT_READ_ONLY;
	}

	return PUT_FAILED;
}


/* virtual */ void
DOM_WebServerConnection::GCTrace()
{
	GCMark(m_request);
	GCMark(m_response);

	GCMark(m_webserver);
}

DOM_WebServerRequest::DOM_WebServerRequest(WebResource_Custom *web_resource_script, DOM_WebServerConnection *connection)
	: m_web_resource_script(web_resource_script),
	  m_header_collection(NULL),
	  m_uri_item_collection(NULL),
	  m_body_item_collection(NULL),
	  m_uploaded_file_array(NULL),
	  m_connection(connection),
	  m_session(NULL)
{
	OP_ASSERT(web_resource_script);
}

/* static */ OP_STATUS
DOM_WebServerRequest::Make(DOM_WebServerRequest *&request_object, DOM_Runtime *runtime, WebResource_Custom *web_resource_script, DOM_WebServerConnection *connection)
{
	OP_ASSERT(runtime);
	OP_ASSERT(web_resource_script);

	RETURN_IF_ERROR(DOMSetObjectRuntime(request_object = OP_NEW(DOM_WebServerRequest, (web_resource_script, connection)), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERREQUEST_PROTOTYPE), "WebServerRequest"));

	WebserverRequest *request = web_resource_script->GetRequestObject();
	RETURN_IF_ERROR(DOM_WebServerSession::Make(request_object->m_session, runtime, request->GetSessionId(), request->GetAuthenticatedUsername(), request->UserAuthenticated() ? request->GetAuthSession()->GetSessionAuthType() : WEB_AUTH_TYPE_NONE));

	return OpStatus::OK;
}

/* virtual */ void
DOM_WebServerRequest::GCTrace()
{
	GCMark(m_header_collection);
	GCMark(m_uploaded_file_array);
	GCMark(m_connection);
	GCMark(m_session);

	GCMark(m_uri_item_collection);
	GCMark(m_body_item_collection);
}

/* virtual */ ES_GetState
DOM_WebServerRequest::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	WebResource_Custom *web_resource_script = GetWebResourceCustom();
	TempBuffer *buffer = GetEmptyTempBuf();

	switch (property_name)
	{
	case  OP_ATOM_session:
		DOMSetObject(value, m_session);
		return GET_SUCCESS;

	case OP_ATOM_connection:
		DOMSetObject(value, m_connection);
		return GET_SUCCESS;

	case OP_ATOM_method:
		DOMSetString(value, web_resource_script->GetHttpMethodString());
		return GET_SUCCESS;

	case OP_ATOM_uri:
		if (value)
		{
			OpString ecaped_uri;
			GET_FAILED_IF_ERROR(web_resource_script->GetEscapedRequestURI(ecaped_uri));

			if (ecaped_uri.Compare(UNI_L("/"), 1) != 0)
				GET_FAILED_IF_ERROR(buffer->Append(UNI_L("/")));

			GET_FAILED_IF_ERROR(buffer->Append(ecaped_uri.CStr()));
			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;

	case OP_ATOM_body:
		if (value)
			if (web_resource_script->GetHttpMethod() == WEB_METH_POST)
			{
				GET_FAILED_IF_ERROR(web_resource_script->GetRequestBody(buffer));
				DOMSetString(value, buffer);
			}
			else
				DOMSetNull(value);
		return GET_SUCCESS;
	case OP_ATOM_files:
		if (value)
			if (web_resource_script->GetHttpMethod() == WEB_METH_POST)
			{
				if (!m_uploaded_file_array)
					GET_FAILED_IF_ERROR(DOM_WebServerObjectArray::MakeUploadedFilesArray(m_uploaded_file_array, GetRuntime(), web_resource_script));

				DOMSetObject(value, m_uploaded_file_array);
			}
			else
				DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_ip:
		if (value)
		{
			OpString ipAddress;
			GET_FAILED_IF_ERROR(web_resource_script->GetClientIp(ipAddress));
			GET_FAILED_IF_ERROR(buffer->Append(ipAddress.CStr()));
			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;

	case OP_ATOM_host:
		if (value)
		{
			OpString host;
			GET_FAILED_IF_ERROR(web_resource_script->GetHostHeader(host));
			GET_FAILED_IF_ERROR(buffer->Append(host.CStr()));
			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;

	case OP_ATOM_protocol:
		if (value)
		{
			GET_FAILED_IF_ERROR(buffer->Append(m_connection->GetProtocol().CStr()));
			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;

	case OP_ATOM_headers:
		if (value)
		{
			if (!m_header_collection)
				GET_FAILED_IF_ERROR(DOM_WebServerCollection::MakeHeaderCollection(m_header_collection, GetRuntime(), web_resource_script->GetRequestObject()->GetClientHeaderList()));
			DOMSetObject(value, m_header_collection);
		}
		return GET_SUCCESS;

	case OP_ATOM_queryItems:
		if (value)
		{
			if (!m_uri_item_collection)
				GET_FAILED_IF_ERROR(DOM_WebServerCollection::MakeItemCollectionUri(m_uri_item_collection, GetRuntime(), web_resource_script));
			DOMSetObject(value, m_uri_item_collection);
		}
		return GET_SUCCESS;

	case OP_ATOM_bodyItems:
		if (value)
		{
			if (!m_body_item_collection)
				GET_FAILED_IF_ERROR(DOM_WebServerCollection::MakeItemCollectionBody(m_body_item_collection, GetRuntime(), web_resource_script));
			DOMSetObject(value, m_body_item_collection);
		}
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_WebServerRequest::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	WebResource_Custom *web_resource_script = GetWebResourceCustom();
	OpString8 utf8_uri;
	switch (property_name)
	{
	case OP_ATOM_uri:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		PUT_FAILED_IF_ERROR(utf8_uri.SetUTF8FromUTF16(value->value.string));
		PUT_FAILED_IF_ERROR(web_resource_script->GetRequestObject()->Reconstruct(utf8_uri.CStr(), utf8_uri.Length()));
		return PUT_SUCCESS;

	// READONLY attributes.
	case OP_ATOM_session:
	case OP_ATOM_connection:
	case OP_ATOM_method:
	case OP_ATOM_body:
	case OP_ATOM_files:
	case OP_ATOM_ip:
	case OP_ATOM_host:
	case OP_ATOM_protocol:
	case OP_ATOM_headers:
	case OP_ATOM_queryItems:
	case OP_ATOM_bodyItems:

		return PUT_READ_ONLY;
	}

	return PUT_FAILED;
}

/* static */ int
DOM_WebServerRequest::getRequestHeader(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(request, DOM_TYPE_WEBSERVER_REQUEST, DOM_WebServerRequest);
	DOM_CHECK_ARGUMENTS("s");

	DOM_WebserverStringArray* header_string_list;
	CALL_FAILED_IF_ERROR(DOM_WebserverStringArray::Make(header_string_list, request->GetRuntime()));

	CALL_FAILED_IF_ERROR(request->GetWebResourceCustom()->GetRequestHeader(argv[0].value.string, header_string_list->GetStrings()));

	if (header_string_list->GetCount() == 0)
		DOMSetNull(return_value);
	else
		DOMSetObject(return_value, header_string_list);

	return ES_VALUE;
}

/* static */ int
DOM_WebServerRequest::getItem(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(request, DOM_TYPE_WEBSERVER_REQUEST, DOM_WebServerRequest);

	BOOL unescape = TRUE;

	if (argc == 1)
		DOM_CHECK_ARGUMENTS("s");
	else if (argc == 2)
		DOM_CHECK_ARGUMENTS("ss");
	else
	{
		DOM_CHECK_ARGUMENTS("ssb");
		unescape = argv[2].value.boolean;
	}

	DOM_WebserverStringArray* item_string_list;
	CALL_FAILED_IF_ERROR(DOM_WebserverStringArray::Make(item_string_list, request->GetRuntime()));


	if (argc == 1 || uni_str_eq(argv[1].value.string, "ALL"))
	{
		CALL_FAILED_IF_ERROR(request->GetWebResourceCustom()->GetItemInBody(argv[0].value.string, item_string_list->GetStrings(), unescape));
		CALL_FAILED_IF_ERROR(request->GetWebResourceCustom()->GetItemInUri(argv[0].value.string, item_string_list->GetStrings(), unescape));
	}
	else
	{
		if (uni_str_eq(argv[1].value.string, "POST"))
			CALL_FAILED_IF_ERROR(request->GetWebResourceCustom()->GetItemInBody(argv[0].value.string, item_string_list->GetStrings(), unescape));
		else if (uni_str_eq(argv[1].value.string, "GET"))
			CALL_FAILED_IF_ERROR(request->GetWebResourceCustom()->GetItemInUri(argv[0].value.string, item_string_list->GetStrings(), unescape));
		else
			return ES_FAILED;
	}


	if (item_string_list->GetCount() == 0)
		DOMSetNull(return_value);
	else
		DOMSetObject(return_value, item_string_list);

	return ES_VALUE;
}

DOM_FUNCTIONS_START(DOM_WebServerRequest)
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerRequest, DOM_WebServerRequest::getRequestHeader, "getRequestHeader", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerRequest, DOM_WebServerRequest::getItem, "getItem", "ssb-")
DOM_FUNCTIONS_END(DOM_WebServerRequest)


/* static */ OP_STATUS
DOM_WebserverStringArray::Make(DOM_WebserverStringArray *&stringlist, DOM_Runtime *runtime)
{
	return DOMSetObjectRuntime(stringlist = OP_NEW(DOM_WebserverStringArray, ()), runtime, runtime->GetPrototype(DOM_Runtime::DOMSTRINGLIST_PROTOTYPE), "DOMStringList");
}

DOM_WebserverStringArray::DOM_WebserverStringArray()
	: DOM_DOMStringList(NULL, this)
{
}

/* virtual */ unsigned
DOM_WebserverStringArray::StringList_length()
{
	return m_strings.GetCount();
}

/* virtual */ OP_STATUS
DOM_WebserverStringArray::StringList_item(int index, const uni_char *&name)
{
	name = m_strings.Get(index)->CStr();
	return OpStatus::OK;
}

OP_STATUS
DOM_WebserverStringArray::Add(const uni_char* item)
{
	OpString *stored_data_string = OP_NEW(OpString, ());

	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (!stored_data_string || OpStatus::IsError(status = stored_data_string->Set(item)) || (OpStatus::IsError(status = Add(stored_data_string))))
		OP_DELETE(stored_data_string);

	return status;
}

/* virtual */ BOOL
DOM_WebserverStringArray::StringList_contains(const uni_char *string)
{
	for (unsigned int index = 0; index < m_strings.GetCount(); ++index)
	{
		if (uni_str_eq(m_strings.Get(index)->CStr(), string))
			return TRUE;
	}
	return FALSE;
}

/* static */ OP_STATUS
DOM_WebServerResponse::Make(DOM_WebServerResponse *&response, DOM_Runtime *runtime, WebResource_Custom *web_resource_script, DOM_WebServerConnection *connection)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(response = OP_NEW(DOM_WebServerResponse, (web_resource_script, connection)), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERRESPONSE_PROTOTYPE), "WebServerResponse"));
	web_resource_script->AddHttpConnectionListener(response);
	return OpStatus::OK;
}

/* virtual */
DOM_WebServerResponse::~DOM_WebServerResponse()
{
	GetWebResourceCustom()->RemoveHttpConnectionListener(this);
}

/* virtual */ OP_STATUS
DOM_WebServerResponse::WebServerConnectionKilled()
{
	m_connection_is_alive = FALSE;
	return OpStatus::OK;
}

/*virtual*/ ES_GetState
DOM_WebServerResponse::GetName(OpAtom property_name, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{

	case OP_ATOM_connection:
		DOMSetObject(return_value, m_connection);
		return GET_SUCCESS;

	case OP_ATOM_chunked:
		DOMSetBoolean(return_value, GetWebResourceCustom()->GetChunkedEncoding());
		return GET_SUCCESS;

	case OP_ATOM_closed:
		DOMSetBoolean(return_value, !m_connection_is_alive);
		return GET_SUCCESS;

	case OP_ATOM_implicitFlush:
		DOMSetBoolean(return_value, m_implicit_flush);
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_WebServerResponse::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_chunked:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;

		if (OpStatus::IsError(GetWebResourceCustom()->setChunkedEncoding(value->value.boolean)))
			return DOM_PUTNAME_DOMEXCEPTION(INVALID_STATE_ERR);
		break;

	case OP_ATOM_implicitFlush:
		if (value->type != VALUE_BOOLEAN)
			return PUT_NEEDS_BOOLEAN;
		m_implicit_flush = value->value.boolean;
		break;

	default:
		return PUT_FAILED;
	}

	return PUT_SUCCESS;
}

/* virtual */ void
DOM_WebServerResponse::GCTrace()
{
	GCMark(m_connection);
}

/* static */ int
DOM_WebServerResponse::setResponseHeader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);

	if (!response->m_connection_is_alive)
		return ES_FAILED;

	BOOL append;

	if (argc == 2)
	{
		DOM_CHECK_ARGUMENTS("ss");
		append = FALSE;
	}
	else
	{
		DOM_CHECK_ARGUMENTS("ssb");
		append = argv[2].value.boolean;
	}

	OP_BOOLEAN status;
	CALL_FAILED_IF_ERROR(status = response->GetWebResourceCustom()->AddResponseHeader(argv[0].value.string, argv[1].value.string, append));

	if (status == OpBoolean::IS_FALSE)
		return ES_EXCEPT_SECURITY;

	return ES_FAILED;
}

/* static */ int
DOM_WebServerResponse::setProtocolString(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);
	DOM_CHECK_ARGUMENTS("s");

	if (!response->m_connection_is_alive)
		return ES_FAILED;

	response->GetWebResourceCustom()->SetProtocolString(argv[0].value.string);
	return ES_FAILED;
}

/* static */ int
DOM_WebServerResponse::setStatusCode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);

	if (argc > 1)
		DOM_CHECK_ARGUMENTS("ns");
	else
		DOM_CHECK_ARGUMENTS("n");

	if (!response->m_connection_is_alive)
		return ES_FAILED;

	if (argc == 1)
		CALL_FAILED_IF_ERROR(response->GetWebResourceCustom()->SetResponseCode(static_cast<int>(argv[0].value.number)));
	else
		CALL_FAILED_IF_ERROR(response->GetWebResourceCustom()->SetResponseCode(static_cast<int>(argv[0].value.number), argv[1].value.string));


	return ES_FAILED;
}

/* static */ int
DOM_WebServerResponse::write(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);
	DOM_CHECK_ARGUMENTS("s");

	if (!response->m_connection_is_alive)
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(response->GetWebResourceCustom()->AppendBodyData(argv[0].value.string));

	if (response->m_implicit_flush)
		return DOM_WebServerResponse::flush(this_object, NULL, 0, return_value, origining_runtime);

	return ES_FAILED;
}

/* static */ int
DOM_WebServerResponse::writeLine(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);
	DOM_CHECK_ARGUMENTS("s");

	if (!response->m_connection_is_alive)
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(response->GetWebResourceCustom()->AppendBodyData(argv[0].value.string));
	CALL_FAILED_IF_ERROR(response->GetWebResourceCustom()->AppendBodyData(UNI_L("\n")));

	if (response->m_implicit_flush)
		return DOM_WebServerResponse::flush(this_object, NULL, 0, return_value, origining_runtime);

	return ES_FAILED;
}

/* static */ int
DOM_WebServerResponse::writeBytes(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);
	DOM_CHECK_ARGUMENTS("o");

	if (!response->m_connection_is_alive)
		return ES_FAILED;

	OP_STATUS status = OpStatus::OK;
	DOM_Object *obj = DOM_GetHostObject(argv[0].value.object);

	if (!obj)
		return ES_FAILED;

	if (obj->IsA(DOM_TYPE_WEBSERVER_BINARY_OBJECT))
	{
		DOM_ByteArray *binary_object = static_cast<DOM_ByteArray*>(obj);
		WebserverBodyObject_Binary *body_binary_object;

		CALL_FAILED_IF_ERROR(WebserverBodyObject_Binary::Make(body_binary_object, binary_object->GetDataPointer(), binary_object->GetDataSize()));

		if (OpStatus::IsError(status = response->m_connection->GetDomWebserver()->AppendBodyObject(obj, body_binary_object, response)))
		{
			OP_DELETE(body_binary_object);
			CALL_FAILED_IF_ERROR(status);
		}
	}
	else
		return DOM_CALL_WEBSERVEREXCEPTION(TYPE_NOT_SUPPORTED_ERR);

	if (response->m_implicit_flush)
		return DOM_WebServerResponse::flush(this_object, NULL, 0, return_value, origining_runtime);

	return ES_FAILED;
}

/* static */ int
DOM_WebServerResponse::writeFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);
	DOM_CHECK_ARGUMENTS("o");

	if (!response->m_connection_is_alive)
		return ES_FAILED;

	OP_STATUS status = OpStatus::OK;
	DOM_Object *obj = DOM_GetHostObject(argv[0].value.object);

	if (!obj)
		return ES_FAILED;

#ifdef DOM_GADGET_FILE_API_SUPPORT
	if (obj->IsA(DOM_TYPE_GADGETFILE))
	{
		DOM_GadgetFile *file = static_cast<DOM_GadgetFile*>(obj);

		OpString path;
		CALL_FAILED_IF_ERROR(file->GetPhysicalPath(path));

		WebserverBodyObject_File *file_object;
		CALL_FAILED_IF_ERROR(WebserverBodyObject_File::Make(file_object, path));

		if (OpStatus::IsError(status = response->m_connection->GetDomWebserver()->AppendBodyObject(obj, file_object, response)))
		{
			OP_DELETE(file_object);
			CALL_FAILED_IF_ERROR(status);
		}
	}
	else
#endif // DOM_GADGET_FILE_API_SUPPORT
		return DOM_CALL_WEBSERVEREXCEPTION(TYPE_NOT_SUPPORTED_ERR);

	if (response->m_implicit_flush)
		return DOM_WebServerResponse::flush(this_object, NULL, 0, return_value, origining_runtime);

	return ES_FAILED;
}

class WebServerImageLoader : public ExternalInlineListener, public DOM_Object, public ES_ThreadListener
{
public:
	WebServerImageLoader(const URL& iu, DOM_Object* io, ES_Thread* t) : img_url(iu), imgobj(io), thread(t)
	{}

	// From ExternalInlineListener.
	void LoadingProgress(const URL &url){}
	void LoadingStopped(const URL &url)
	{
		GetFramesDocument()->StopLoadingInline(img_url, this);
		thread->Unblock();
	}
	void LoadingRedirected(const URL &from, const URL &to){}

	// From ES_ThreadListener.
	OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		switch (signal)
		{
		case ES_SIGNAL_CANCELLED:
		case ES_SIGNAL_FINISHED:
		case ES_SIGNAL_FAILED:
			GetFramesDocument()->StopLoadingInline(img_url, this);
			break;
		default:
			break;
		}
		return OpStatus::OK;
	}

	// From DOM_Object.
	void GCTrace()
	{
		GCMark(imgobj);
	}

	URL& GetImageURL() { return img_url; }
	DOM_Object* GetElement() { return imgobj; }
	const uni_char* GetHtmlString() { return image_type.CStr(); }
	OP_STATUS SetHtmlString(const uni_char* string) { return image_type.Set(string); }

private:
	URL img_url;
	DOM_Object* imgobj;
	ES_Thread* thread;
	OpString image_type;
};

/* static */ int
DOM_WebServerResponse::writeImage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);

	if (!response->m_connection_is_alive)
		return ES_FAILED;

	DOM_Object *obj = NULL;
	const uni_char* image_type = NULL;
	if (argc < 0)
	{
		WebServerImageLoader* il = DOM_VALUE2OBJECT(*return_value, WebServerImageLoader);
		obj = il->GetElement();
		image_type = il->GetHtmlString();
	}
	else
	{
		DOM_CHECK_ARGUMENTS("o|s");
		obj = DOM_GetHostObject(argv[0].value.object);
		if (argc > 1)
			image_type = argv[1].value.string;
	}

	if (!obj)
		return ES_FAILED;

	if (obj->IsA(DOM_TYPE_HTML_ELEMENT))
	{
		DOM_HTMLElement *dom_html_element = static_cast<DOM_HTMLElement*>(obj);

		HTML_Element *html_element = dom_html_element->GetThisElement();
		if (obj->IsA(DOM_TYPE_HTML_CANVASELEMENT))
		{
			static_cast<DOM_HTMLCanvasElement*>(obj)->Invalidate();

			WebserverBodyObject_Base *body_object;

			CALL_FAILED_IF_ERROR(WebserverBodyObject_Base::MakeBodyObjectFromHtml(body_object, html_element));

			OP_STATUS status = OpStatus::ERR_NO_MEMORY;
			if (body_object == NULL || OpStatus::IsError(status = response->m_connection->GetDomWebserver()->AppendBodyObject(obj, body_object, response)))
			{
				OP_DELETE(body_object);
				CALL_FAILED_IF_ERROR(status);
			}

			if (response->m_implicit_flush)
				return DOM_WebServerResponse::flush(this_object, NULL, 0, return_value, origining_runtime);
			else
				return ES_FAILED;
		}
		else
		{
			LogicalDocument* element_logdoc = obj->GetLogicalDocument();
			if (!element_logdoc)
				return ES_FAILED;
			URL img_url = html_element->GetImageURL(TRUE, element_logdoc);

			if (img_url.IsEmpty())
				return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

			FramesDocument* element_doc = obj->GetFramesDocument();

			// Make sure the image is loaded
			URLStatus stat = img_url.Status(TRUE);
			if (stat == URL_LOADING || stat == URL_UNLOADED)
			{
				ES_Thread* thread = GetCurrentThread(origining_runtime);
				WebServerImageLoader* il = OP_NEW(WebServerImageLoader, (img_url, obj, thread));
				CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(il, origining_runtime));
				CALL_FAILED_IF_ERROR(il->SetHtmlString(image_type));

				OP_LOAD_INLINE_STATUS status = element_doc->LoadInline(il->GetImageURL(), il);
				if (OpStatus::IsMemoryError(status))
					return ES_NO_MEMORY;
				else if (status == LoadInlineStatus::LOADING_CANCELLED ||
				         status == LoadInlineStatus::LOADING_REFUSED)
					return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
				else if (status == LoadInlineStatus::LOADING_STARTED)
				{
					thread->Block();
					thread->AddListener(il);
					return ES_RESTART|ES_SUSPEND;
				}
				OP_ASSERT(status == LoadInlineStatus::USE_LOADED);
				DOM_Object::DOMSetObject(return_value, il);
			}

			// Make it BGIMAGE_INLINE to trigger the SVGImageRef code in frm_doc.cpp.
			OP_LOAD_INLINE_STATUS load_status = element_doc->LoadInline(&img_url, html_element, BGIMAGE_INLINE);
			if (load_status == LoadInlineStatus::LOADING_CANCELLED ||
			    load_status == LoadInlineStatus::LOADING_REFUSED)
				return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
			else if (OpStatus::IsMemoryError(load_status))
				return ES_NO_MEMORY;

			Image img = UrlImageContentProvider::GetImageFromUrl(img_url);
			UrlImageContentProvider *prov = UrlImageContentProvider::FindImageContentProvider(img_url);

#ifdef SVG_SUPPORT
			if (prov && prov->GetSVGImageRef())
			{
				// FIXME: support svg in <img>.
				return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
			}
#endif // SVG_SUPPORT
			img.IncVisible(null_image_listener);
			if (prov)
				img.OnLoadAll(prov);

			WebserverBodyObject_Base *body_object;

			OP_STATUS status = WebserverBodyObject_Base::MakeBodyObjectFromHtml(body_object, html_element, image_type);

			if (OpStatus::IsError(status))
			{
				img.DecVisible(null_image_listener);
				CALL_FAILED_IF_ERROR(status);
			}
			if (OpStatus::IsError(status = response->m_connection->GetDomWebserver()->AppendBodyObject(obj, body_object, response)))
			{
				img.DecVisible(null_image_listener);
				OP_DELETE(body_object);
				CALL_FAILED_IF_ERROR(status);
			}

			int res = ES_FAILED;
			if (response->m_implicit_flush)
				res = DOM_WebServerResponse::flush(this_object, NULL, 0, return_value, origining_runtime);

			img.DecVisible(null_image_listener);
			return res;
		}
	}

	return DOM_CALL_WEBSERVEREXCEPTION(TYPE_NOT_SUPPORTED_ERR);
}

/* static */ int
DOM_WebServerResponse::flush(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);

	if (argc >= 1)
		DOM_CHECK_ARGUMENTS("o");


	if (!response->m_connection_is_alive)
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(response->m_connection->GetDomWebserver()->Flush(response->GetWebResourceCustom(), FALSE, response->m_connection->GetConnectionId(), (argc >= 1) ? argv[0].value.object : NULL));

	return ES_FAILED;
}

/* static */ int
DOM_WebServerResponse::close(DOM_Object* this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);
	if (argc >= 1)
		DOM_CHECK_ARGUMENTS("o");

	if (!response->m_connection_is_alive)
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(response->m_connection->WebServerConnectionKilled());

	response->m_connection_is_alive = FALSE;

	CALL_FAILED_IF_ERROR(response->m_connection->GetDomWebserver()->Flush(response->GetWebResourceCustom(), TRUE, response->m_connection->GetConnectionId(), (argc >= 1) ? argv[0].value.object : NULL));

	return ES_FAILED;
}

/* static */ int
DOM_WebServerResponse::closeAndRedispatch(DOM_Object* this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(response, DOM_TYPE_WEBSERVER_RESPONSE, DOM_WebServerResponse);

	OP_BOOLEAN status = OpStatus::OK;

	if (!response->m_connection_is_alive || (status = response->GetWebResourceCustom()->CloseAndForward()) == OpStatus::ERR)
		return ES_FAILED;

	if (status == OpBoolean::IS_FALSE)
		return ES_EXCEPT_SECURITY;

	response->m_connection_is_alive = FALSE;

	CALL_FAILED_IF_ERROR(status); /* In case of memory error */

	return ES_FAILED;
}

DOM_WebServerResponse::DOM_WebServerResponse(WebResource_Custom *web_resource_script, DOM_WebServerConnection *connection)
	: m_connection_is_alive(TRUE),
	  m_implicit_flush(FALSE),
	  m_connection(connection),
	  m_web_resource_script(web_resource_script)
{
}

DOM_FUNCTIONS_START(DOM_WebServerResponse)
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::setResponseHeader, "setResponseHeader", "ssb-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::setStatusCode, "setStatusCode", "ns-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::setProtocolString, "setProtocolString", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::write, "write", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::writeLine, "writeLine", "s-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::writeBytes, "writeBytes", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::writeFile, "writeFile", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::writeImage, "writeImage", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::close, "close", "o-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::closeAndRedispatch, "closeAndRedispatch", "-")
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerResponse, DOM_WebServerResponse::flush, "flush", "o-")
DOM_FUNCTIONS_END(DOM_WebServerResponse)



/* static */ OP_STATUS
DOM_WebServerObjectArray::Make(DOM_WebServerObjectArray *&object_array, DOM_Runtime *runtime, const char* class_name)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(object_array = OP_NEW(DOM_WebServerObjectArray, ()), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERARRAY_PROTOTYPE), class_name));
	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_WebServerObjectArray::MakeUploadedFilesArray(DOM_GadgetFile *&file_array, DOM_Runtime *runtime, WebResource_Custom *web_resource_script)
{
	RETURN_IF_ERROR(DOM_GadgetFile::MakeVirtual(file_array, runtime));

	OpVector<WebserverUploadedFileWrapper> *multiParts = web_resource_script->GetMultipartVector();

	for (unsigned index = 0; index < multiParts->GetCount(); ++index)
	{
		WebserverUploadedFile *file = multiParts->Get(index)->Ptr();

		if (file->GetMultiPartFileType() != WebserverUploadedFile::WEB_FILETYPE_FILE)
			continue;

		OpString filename;
		RETURN_IF_ERROR(file->GetFileName(filename));

		OpString path;
		RETURN_IF_ERROR(path.Set(file->GetTemporaryFilePath()));

		DOM_GadgetFile *uploaded_file;
		RETURN_IF_ERROR(DOM_GadgetFile::MakePhysical(uploaded_file, runtime, file_array, filename.CStr(), path.CStr()));

		DOM_WebServerUploadedFile* meta_data;
		RETURN_IF_ERROR(DOM_WebServerUploadedFile::Make(meta_data, runtime, file));

		uploaded_file->SetMetaData(meta_data);

		RETURN_IF_ERROR(file_array->AddNode(uploaded_file));
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_WebServerObjectArray::MakeServiceArray(DOM_WebServerObjectArray *&service_array, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(service_array = OP_NEW(DOM_WebServerObjectArray, ()), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERARRAY_PROTOTYPE), "WebServerServiceArray"));

	WebSubServer *subServer;

	for (unsigned index = 0; index < g_webserver->GetSubServerCount(); ++index)
	{
		subServer = g_webserver->GetSubServerAtIndex(index);

		if (subServer->IsVisible())
		{
			const uni_char* subServerVirtualPath = subServer->GetSubServerVirtualPath().CStr();
			if (!subServerVirtualPath || !*subServerVirtualPath)
			{
				continue;
			}

			TempBuffer serviceUri;
			RETURN_IF_ERROR(serviceUri.Append("http://"));
			RETURN_IF_ERROR(serviceUri.Append(g_webserver->GetWebserverUri()));
			RETURN_IF_ERROR(serviceUri.Append('/'));
			RETURN_IF_ERROR(serviceUri.Append(subServer->GetSubServerVirtualPath().CStr()));

			DOM_WebServerServiceDescriptor *descriptor;
			RETURN_IF_ERROR(DOM_WebServerServiceDescriptor::Make(descriptor, runtime, subServer->GetSubServerName().CStr(), subServer->GetSubServerVirtualPath().CStr(), serviceUri.GetStorage(), subServer->GetSubServerDescription().CStr(), subServer->GetSubServerAuthor().CStr(), subServer->GetSubServerOriginUrl().CStr(), subServer->HasActiveAccessControl()));

			RETURN_IF_ERROR(service_array->Add(descriptor));

			for (int i = 0, len = subServer->GetGadgetIconCount(); i < len; i++)
			{
				DOM_WidgetIcon *icon;
				INT32 w;
				INT32 h;
				OpString src;

				RETURN_IF_ERROR(subServer->GetGadgetIcon(i, src, w, h));
				RETURN_IF_ERROR(DOM_WidgetIcon::Make(icon, runtime, w, h, src.CStr()));

				OP_STATUS ops = descriptor->AddIcon(icon);

				if (OpStatus::IsError(ops))
				{
					OP_DELETE(icon);
					return ops;
				}
			}
		}
	}

	return OpStatus::OK;
}

DOM_WebServerUploadedFile::DOM_WebServerUploadedFile(WebserverUploadedFile *uploaded_file)
	: m_uploaded_file(uploaded_file),
	  m_header_collection(NULL)
{
}

/* static */ OP_STATUS
DOM_WebServerUploadedFile::Make(DOM_WebServerUploadedFile *&uploaded_file, DOM_Runtime *runtime, WebserverUploadedFile *file)
{
	return DOMSetObjectRuntime(uploaded_file = OP_NEW(DOM_WebServerUploadedFile, (file)), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERUPLOADEDFILE_PROTOTYPE), "UploadedFileMetaData");
}

/* virtual */ ES_GetState
DOM_WebServerUploadedFile::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_headers:
		if (value)
		{
			if (!m_header_collection)
				GET_FAILED_IF_ERROR(DOM_WebServerCollection::MakeHeaderCollection(m_header_collection, GetRuntime(), m_uploaded_file.Ptr()->GetHeaderList()));
			DOMSetObject(value, m_header_collection);
		}
		return GET_SUCCESS;

	case OP_ATOM_filename:
		if (value)
		{
			OpString filename;
			GET_FAILED_IF_ERROR(m_uploaded_file.Ptr()->GetFileName(filename));
			TempBuffer *buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(buffer->Append(filename.CStr()));
			DOMSetString(value, buffer);
		}
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* static */ int
DOM_WebServerUploadedFile::getFileHeader(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(uploaded_file, DOM_TYPE_WEBSERVER_UPLOADED_FILE, DOM_WebServerUploadedFile);
	DOM_CHECK_ARGUMENTS("s");

	DOM_WebserverStringArray* header_string_list;
	CALL_FAILED_IF_ERROR(DOM_WebserverStringArray::Make(header_string_list, uploaded_file->GetRuntime()));

	CALL_FAILED_IF_ERROR(uploaded_file->m_uploaded_file.Ptr()->GetRequestHeader(argv[0].value.string, header_string_list->GetStrings()));

	DOMSetObject(return_value, header_string_list);

	return ES_VALUE;
}

/* virtual */ void
DOM_WebServerUploadedFile::GCTrace()
{
	GCMark(m_header_collection);
}

DOM_FUNCTIONS_START(DOM_WebServerUploadedFile)
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerUploadedFile, DOM_WebServerUploadedFile::getFileHeader, "getFileHeader", "s-")
DOM_FUNCTIONS_END(DOM_WebServerUploadedFile)

/* static */ OP_STATUS
DOM_WebServerCollection::MakeHeaderCollection(DOM_WebServerCollection *&header_collection, DOM_Runtime *runtime, HeaderList *header_list)
{
	HeaderEntry *header_entry = static_cast<HeaderEntry *>(header_list->First());
	OP_STATUS ops = OpStatus::OK;
	OpString header_name;
	OpString header_value;

	ops = DOMSetObjectRuntime(header_collection = OP_NEW(DOM_WebServerCollection, ()), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERCOLLECTION_PROTOTYPE), "WebServerHeaderCollection");

	while (header_entry && OpStatus::IsSuccess(ops))
	{
		if (OpStatus::IsSuccess(ops = header_name.SetFromUTF8(header_entry->Name())) &&
			OpStatus::IsSuccess(ops = header_value.SetFromUTF8(header_entry->Value())) &&
			OpStatus::IsSuccess(ops = header_collection->AddToCollection(header_name.CStr(), header_value.CStr())))
			header_entry = static_cast<HeaderEntry *>(header_entry->Suc());
	}

	if (OpStatus::IsError(ops))
		header_collection = NULL;

	return ops;
}

/* static */ OP_STATUS
DOM_WebServerCollection::MakeItemCollectionUri(DOM_WebServerCollection *&item_collection, DOM_Runtime *runtime, WebResource_Custom *web_resource_script)
{
	OP_STATUS ops = DOMSetObjectRuntime(item_collection = OP_NEW(DOM_WebServerCollection, ()), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERCOLLECTION_PROTOTYPE), "WebServerItemCollection");

	if (OpStatus::IsError(ops) ||
		OpStatus::IsError(ops = web_resource_script->GetAllItemsInUri(item_collection, FALSE, TRUE)))
		item_collection = NULL;

	return ops;
}

/* static */ OP_STATUS
DOM_WebServerCollection::MakeItemCollectionBody(DOM_WebServerCollection *&item_collection, DOM_Runtime *runtime, WebResource_Custom *web_resource_script)
{
	OP_STATUS ops = DOMSetObjectRuntime(item_collection = OP_NEW(DOM_WebServerCollection, ()), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERCOLLECTION_PROTOTYPE), "WebServerItemCollection");

	if (OpStatus::IsError(ops) ||
		OpStatus::IsError(ops = web_resource_script->GetAllItemsInBody(item_collection)))
		item_collection = NULL;

	return ops;
}

/* Also called by WebResource_Custom::GetAllItemsInBody and WebResource_Custom::GetAllItemsInUri via class AddToCollectionCallback. */
OP_STATUS
DOM_WebServerCollection::AddToCollection(const uni_char* key, const uni_char* dataString)
{
	DOM_WebserverStringArray *string_array;

	ES_Value value;
	OP_BOOLEAN exists;
	RETURN_IF_ERROR(exists = Get(key, &value));

	if (exists == OpBoolean::IS_TRUE)
		string_array = static_cast<DOM_WebserverStringArray *>(DOM_GetHostObject(value.value.object));
	else
	{
		RETURN_IF_ERROR(DOM_WebserverStringArray::Make(string_array, GetRuntime()));
		RETURN_IF_ERROR(Put(key, *string_array, TRUE));
	}

	return string_array->Add(dataString);
}

OP_BOOLEAN DOM_WebServerCollection::GetFromCollection(const uni_char* key, DOM_WebServerObjectArray *&object_array)
{
	object_array = NULL;

	ES_Value value;
	OP_BOOLEAN exists;

	RETURN_IF_ERROR(exists = Get(key, &value));
	if (exists == OpBoolean::IS_TRUE)
		object_array = static_cast<DOM_WebServerObjectArray *>(DOM_GetHostObject(value.value.object));

	return exists;
}

DOM_WebServerServiceDescriptor::DOM_WebServerServiceDescriptor(BOOL has_active_auth)
	: m_has_active_auth(has_active_auth),
	  m_icons(NULL)
{
}

OP_STATUS DOM_WebServerServiceDescriptor::AddIcon(DOM_WidgetIcon *icon)
{
	return m_icons->Add(icon);
}

void DOM_WebServerServiceDescriptor::GCTrace()
{
	GCMark(m_icons);
}

/* static */ OP_STATUS
DOM_WebServerServiceDescriptor::Make(DOM_WebServerServiceDescriptor *&descriptor, DOM_Runtime *runtime, const uni_char* service_name, const uni_char* service_path, const uni_char* service_uri, const uni_char* service_description, const uni_char* service_author, const uni_char* service_origin_url, BOOL has_active_auth)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(descriptor = OP_NEW(DOM_WebServerServiceDescriptor, (has_active_auth)), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERSERVICEDESCRIPTOR_PROTOTYPE), "WebServerServiceDescriptor"));
	RETURN_IF_ERROR(descriptor->m_service_name.Set(service_name));

	OpString escaped_service_path;
	RETURN_IF_ERROR(WebserverRequest::UrlEncodeString(service_path, escaped_service_path));

	if (escaped_service_path.Compare(UNI_L("/"), 1) != 0)
		RETURN_IF_ERROR(descriptor->m_service_path.Append("/"));

	RETURN_IF_ERROR(descriptor->m_service_path.Append(escaped_service_path.CStr()));

	if (escaped_service_path.FindLastOf('/') != escaped_service_path.Length() - 1) // This works because Length() - 1 is kNotFound (-1) if Length() is 0
		RETURN_IF_ERROR(descriptor->m_service_path.Append("/"));

	RETURN_IF_ERROR(WebserverRequest::UrlEncodeString(service_origin_url, descriptor->m_service_origin_url));
	RETURN_IF_ERROR(descriptor->m_service_description.Set(service_description));
	RETURN_IF_ERROR(descriptor->m_service_author.Set(service_author));
	RETURN_IF_ERROR(WebserverRequest::UrlEncodeString(service_uri, descriptor->m_service_uri));

	DOM_WebServerObjectArray *icons;
	RETURN_IF_ERROR(DOMSetObjectRuntime(icons = OP_NEW(DOM_WebServerObjectArray, ()), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERARRAY_PROTOTYPE), "OpWidgetIcons"));
	descriptor->m_icons = icons;

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_WebServerServiceDescriptor::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_name:
		DOMSetString(value, m_service_name.CStr());
		return GET_SUCCESS;

	case OP_ATOM_description:
		DOMSetString(value, m_service_description.CStr());
		return GET_SUCCESS;

	case OP_ATOM_author:
		DOMSetString(value, m_service_author.CStr());
		return GET_SUCCESS;

	case OP_ATOM_uri:
		DOMSetString(value, m_service_uri.CStr());
		return GET_SUCCESS;

	case OP_ATOM_originURL:
		DOMSetString(value, m_service_origin_url.CStr());
		return GET_SUCCESS;

	case OP_ATOM_authentication:
		DOMSetBoolean(value, m_has_active_auth);
		return GET_SUCCESS;

	case OP_ATOM_servicePath:
		DOMSetString(value, m_service_path.CStr());
		return GET_SUCCESS;

	case OP_ATOM_icons:
		DOMSetObject(value, m_icons);
		return GET_SUCCESS;

	}

	return GET_FAILED;
}

ES_PutState DOM_WebServerServiceDescriptor::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_name:
	case OP_ATOM_description:
	case OP_ATOM_author:
	case OP_ATOM_uri:
	case OP_ATOM_originURL:
	case OP_ATOM_authentication:
	case OP_ATOM_servicePath:
	case OP_ATOM_icons:

		return PUT_READ_ONLY;
	}

	return PUT_FAILED;
}


DOM_WebServer::DOM_WebServer(WebSubServer *subserver)
	: m_total_connection_count(0),
	  m_subserver(subserver),
	  m_flush_counter(0)
{
	m_subserver->AddListener(this);
}

/* virtual */
DOM_WebServer::~DOM_WebServer()
{
	OP_ASSERT(m_flushing_dom_objects.GetCount() == 0 && "missing flushcallbacks");
	OP_ASSERT(m_flush_finished_callbacks.GetCount() == 0 && "missing flushcallbacks");
	if (m_subserver)
		m_subserver->RemoveListener(this);
}

/* virtual */ void
DOM_WebServer::InitializeL(ES_Runtime *runtime)
{
#ifdef DOM_GADGET_FILE_API_SUPPORT
	AddFunctionL(DOM_WebServer::shareFile, "shareFile", "os-");
	AddFunctionL(DOM_WebServer::unshareFile, "unshareFile", "os-");
	AddFunctionL(DOM_WebServer::sharePath, "sharePath", "so-");
	AddFunctionL(DOM_WebServer::unsharePath, "unsharePath", "so-");
#endif // DOM_GADGET_FILE_API_SUPPORT

	AddFunctionL(DOM_WebServer::getContentType, "getContentType", "s-");
	AddFunctionL(DOM_WebServer::setDefaultHeader, "setDefaultHeader", "ssb-");

	AddFunctionL(DOM_WebServer::addEventListener, "addEventListener", "s-b-");
	AddFunctionL(DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-");
	AddFunctionL(DOM_Node::dispatchEvent, "dispatchEvent", NULL);
}

/*virtual*/ void
DOM_WebServer::GCTrace()
{
	GCMark(event_target);

	unsigned int index;

	for (index = 0; index < m_flushing_dom_objects.GetCount(); ++index)
		GCMark(m_flushing_dom_objects.Get(index)->m_dom_flush_object);

	for (index = 0; index < m_flush_finished_callbacks.GetCount(); ++index)
		GCMark(m_flush_finished_callbacks.Get(index)->m_flush_finished_callback);
}

/* static */ void DOM_WebServer::PutConstructorsL(DOM_Object* target)
{
	target->PutConstructorL("WebServerConnection", DOM_Runtime::WEBSERVERCONNECTION_PROTOTYPE);
	target->PutConstructorL("WebServerRequest", DOM_Runtime::WEBSERVERREQUEST_PROTOTYPE);
	target->PutConstructorL("WebServerResponse", DOM_Runtime::WEBSERVERRESPONSE_PROTOTYPE);
	target->PutConstructorL("WebServerCollection", DOM_Runtime::WEBSERVERCOLLECTION_PROTOTYPE);
	target->PutConstructorL("WebServerServiceDescriptor", DOM_Runtime::WEBSERVERSERVICEDESCRIPTOR_PROTOTYPE);
	target->PutConstructorL("WebServerSession", DOM_Runtime::WEBSERVERSESSION_PROTOTYPE);

	target->PutConstructorL("UploadedFileMetaData", DOM_Runtime::WEBSERVERUPLOADEDFILE_PROTOTYPE);
	target->PutConstructorL("WebServerServiceArray", DOM_Runtime::WEBSERVERARRAY_PROTOTYPE);
}

OP_STATUS
DOM_WebServer::AppendBodyObject(DOM_Object *dom_object, WebserverBodyObject_Base *webserver_body_object, DOM_WebServerResponse *responseObject)
{
	BodyObjectContainer *body_object = OP_NEW(BodyObjectContainer, (dom_object, webserver_body_object));

	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (body_object == NULL || OpStatus::IsError(status = m_flushing_dom_objects.Add(body_object)))
	{
		OP_DELETE(body_object);
		return status;
	}

	if (OpStatus::IsError(status = responseObject->GetWebResourceCustom()->AppendBodyData(webserver_body_object, this)))
	{
		OpStatus::Ignore(m_flushing_dom_objects.Delete(body_object));
		return status;
	};

	return OpStatus::OK;
}

OP_STATUS
DOM_WebServer::Flush(WebResource_Custom *m_web_resource_script, BOOL closeConnection, unsigned int connection_id, ES_Object* flush_finished_callback)
{
	FlushCallbackContainer *flush_callback_cont = OP_NEW(FlushCallbackContainer, (flush_finished_callback, m_flush_counter, connection_id));

	if (!flush_callback_cont)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status;
	if (OpStatus::IsError(status = m_flush_finished_callbacks.Add(flush_callback_cont)))
	{
		OP_DELETE(flush_callback_cont);
		return status;
	}

	if (closeConnection)
		status = m_web_resource_script->Close(this, m_flush_counter);
	else
		status = m_web_resource_script->Flush(this, m_flush_counter);


	if (OpStatus::IsError(status))
	{
		OpStatus::Ignore(m_flush_finished_callbacks.RemoveByItem(flush_callback_cont));
		RETURN_IF_MEMORY_ERROR(status);

		if (flush_finished_callback != NULL)
		{
			ES_Value arguments[1];
			DOMSetNumber(&arguments[0], connection_id);
			RETURN_IF_ERROR(GetEnvironment()->GetAsyncInterface()->CallFunction(flush_finished_callback, *this, 1, arguments, NULL, DOM_Object::GetCurrentThread(GetRuntime())));
		}
		return status;
	}

	m_flush_counter++;

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_WebServer::BodyObjectFlushed(WebserverBodyObject_Base *bodyDataObject)
{
	for (unsigned int index = 0; index < m_flushing_dom_objects.GetCount(); ++index)
	{
		if (m_flushing_dom_objects.Get(index)->m_flush_object == bodyDataObject)
		{
			BodyObjectContainer *body_object = m_flushing_dom_objects.Remove(index);
			OP_DELETE(body_object);
			return OpStatus::OK;
		}
	}
	OP_ASSERT(!"bodyDataObject not found");
	return OpStatus::ERR;
}

/* virtual */ OP_STATUS
DOM_WebServer::FlushIsFinished(unsigned int flush_id)
{
	ES_Object *flush_finished_callback = NULL;
	unsigned int connection_id = 0;
	BOOL found = FALSE;

	for (unsigned int index = 0; index < m_flush_finished_callbacks.GetCount(); index ++)
	{
		FlushCallbackContainer *flushCallbackCont = m_flush_finished_callbacks.Get(index);

		if (flushCallbackCont->m_flush_id == flush_id)
		{
			found = TRUE;
			connection_id = flushCallbackCont->m_connection_id;
			flush_finished_callback = flushCallbackCont->m_flush_finished_callback;

			OpStatus::Ignore(m_flush_finished_callbacks.RemoveByItem(flushCallbackCont));

			OP_DELETE(flushCallbackCont);
			break;
		}
	}

	if (!found)
	{
		OP_ASSERT(!"unknown callback!");
		return OpStatus::ERR;
	}

	if (flush_finished_callback != NULL)
	{
		ES_Value arguments[1];
		DOMSetNumber(&arguments[0], connection_id);
		RETURN_IF_ERROR(GetEnvironment()->GetAsyncInterface()->CallFunction(flush_finished_callback, *this, 1, arguments, NULL, DOM_Object::GetCurrentThread(GetRuntime())));
	}

	return OpStatus::OK;
}

/* virtual */ ES_GetState
DOM_WebServer::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	case OP_ATOM_hostName:
		DOMSetString(value, g_webserver->GetWebserverUri());
		break;

	case OP_ATOM_publicIP:
		if (g_webserver->GetPublicIP())
			DOMSetString(value, g_webserver->GetPublicIP());
		else
			DOMSetNull(value);
		break;

	case OP_ATOM_publicPort:
		if (g_webserver->GetPublicIP()) // No Public IP, no port
			DOMSetNumber(value, g_webserver->GetPublicPort());
		else
			DOMSetNull(value);
		break;

	case OP_ATOM_proxyName:
		if (value)
			DOMSetString(value, g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverProxyHost).CStr());
		break;
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

	case OP_ATOM_currentServiceName:
		if (m_subserver == NULL)
			return GET_FAILED;

		DOMSetString(value, m_subserver->GetSubServerName().CStr());
		break;

	case OP_ATOM_currentServicePath:
		if (value)
		{
			if (m_subserver == NULL)
				return GET_FAILED;

			TempBuffer *buffer = GetEmptyTempBuf();
			OpString escaped_uri;
			GET_FAILED_IF_ERROR(WebserverRequest::UrlEncodeString(m_subserver->GetSubServerVirtualPath(), escaped_uri));

			if (escaped_uri.Compare(UNI_L("/"), 1) != 0)
				GET_FAILED_IF_ERROR(buffer->Append('/'));

			GET_FAILED_IF_ERROR(buffer->Append(escaped_uri.CStr()));

			if (escaped_uri.FindLastOf('/') != escaped_uri.Length() - 1) // This works because Length() - 1 is kNotFound (-1) if Length() is 0
				GET_FAILED_IF_ERROR(buffer->Append('/'));

			DOMSetString(value, buffer);
		}
		break;

	case OP_ATOM_port:
		DOMSetNumber(value, g_webserver->GetLocalListeningPort());
		break;

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	case OP_ATOM_userName:
		if (value)
			DOMSetString(value, g_webserver->GetUserName());
		break;

	case OP_ATOM_deviceName:
		if (value)
			DOMSetString(value, g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice).CStr());
		break;
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

	case OP_ATOM_originURL:
		if (value)
		{
			if (m_subserver == NULL)
				return GET_FAILED;

			TempBuffer *buffer = GetEmptyTempBuf();
			OpString escaped_uri;
			GET_FAILED_IF_ERROR(WebserverRequest::UrlEncodeString(m_subserver->GetSubServerOriginUrl(), escaped_uri));
			GET_FAILED_IF_ERROR(buffer->Append(escaped_uri.CStr()));
			DOMSetString(value, buffer);
		}
		break;

	case OP_ATOM_services:
		if (value)
		{
			DOM_WebServerObjectArray *service_array;
			GET_FAILED_IF_ERROR(DOM_WebServerObjectArray::MakeServiceArray(service_array, GetRuntime()));
			DOMSetObject(value, service_array);
		}
		break;

	case OP_ATOM_passwordProtected:
		DOMSetNumber(value, m_subserver->GetPasswordProtected());
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
	return GET_SUCCESS;
}

ES_PutState DOM_WebServer::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	// All the attributes are read only.
	switch (property_name)
	{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
		case OP_ATOM_hostName:
		case OP_ATOM_publicIP:
		case OP_ATOM_publicPort:
		case OP_ATOM_proxyName:
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

		case OP_ATOM_currentServiceName:
		case OP_ATOM_currentServicePath:
		case OP_ATOM_port:

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
		case OP_ATOM_userName:
		case OP_ATOM_deviceName:
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

		case OP_ATOM_originURL:
		case OP_ATOM_services:

			return PUT_READ_ONLY;

		case OP_ATOM_passwordProtected:
			if (value->type != VALUE_NUMBER)
				return PUT_NEEDS_NUMBER;
			else
				m_subserver->SetPasswordProtected((int) value->value.number);
			return PUT_SUCCESS;
	}

	return PUT_FAILED;
}

/* static */ int
DOM_WebServer::getContentType(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("s");

	TempBuffer *buffer = GetEmptyTempBuf();

	Viewer* viewer = g_viewers->FindViewerByExtension(WebServer::StrFileExt(argv[0].value.string));

	// Undefined types are identified as application/octet-stream
	if (viewer)
		CALL_FAILED_IF_ERROR(buffer->Append(viewer->GetContentTypeString()));

	DOMSetString(return_value, buffer);

	return ES_VALUE;
}

/* static */ int
DOM_WebServer::addEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(webserver, DOM_TYPE_WEBSERVER, DOM_WebServer);
	DOM_CHECK_ARGUMENTS("s");

	int ret = DOM_Node::accessEventListener(this_object, argv, argc, return_value, origining_runtime, 0 /* 0 == addEventListener */);

	if (ret == ES_FAILED && webserver->m_subserver != NULL) // If the function returned without throwing an exception or security error
		g_webserver->OnNewDOMEventListener(webserver->m_subserver->GetSubServerName().CStr(), argv->value.string, webserver->m_subserver->GetSubServerVirtualPath().CStr());

	return ret;
}


/* static */ int
DOM_WebServer::shareFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(webserver, DOM_TYPE_WEBSERVER, DOM_WebServer);
	DOM_CHECK_ARGUMENTS("os");
	DOM_ARGUMENT_OBJECT(file, 0, DOM_TYPE_GADGETFILE, DOM_GadgetFile);

	OP_BOOLEAN exists;
	CALL_FAILED_IF_ERROR(exists = file->Share(webserver, argv[1].value.string));

	if (exists == OpBoolean::IS_TRUE)
		return DOM_CALL_WEBSERVEREXCEPTION(ALREADY_SHARED_ERR);

	return ES_FAILED;
}

/* static */ int
DOM_WebServer::unshareFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(webserver, DOM_TYPE_WEBSERVER, DOM_WebServer);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(file, 0, DOM_TYPE_GADGETFILE, DOM_GadgetFile);

	CALL_FAILED_IF_ERROR(file->Unshare(webserver));

	return ES_FAILED;
}

/* static */ int
DOM_WebServer::sharePath(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(webserver, DOM_TYPE_WEBSERVER, DOM_WebServer);
	DOM_CHECK_ARGUMENTS("so");
	DOM_ARGUMENT_OBJECT(file, 1, DOM_TYPE_GADGETFILE, DOM_GadgetFile);

	OpString virtual_path;
	const uni_char *uri = argv[0].value.string;

	if (!webserver->GetSubServer())
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(virtual_path.Set(uri));

	if (webserver->GetSubServer()->GetSubserverResource(virtual_path))
		return DOM_CALL_WEBSERVEREXCEPTION(ALREADY_SHARED_ERR);

	OpString path;
	RETURN_IF_ERROR(file->GetPhysicalPath(path));
	if (path.IsEmpty())
		return ES_FAILED;

	WebserverResourceDescriptor_Base *res_desc = WebserverResourceDescriptor_Static::Make(path, uri);

	OP_BOOLEAN status = OpStatus::ERR_NO_MEMORY;
	if (res_desc == NULL || OpStatus::IsError(status = webserver->GetSubServer()->AddSubserverResource(res_desc)) || status == OpBoolean::IS_TRUE)
	{
		OP_DELETE(res_desc);
		CALL_FAILED_IF_ERROR(status);
	}

	return ES_FAILED;
}

/* static */ int
DOM_WebServer::unsharePath(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(webserver, DOM_TYPE_WEBSERVER, DOM_WebServer);
	DOM_CHECK_ARGUMENTS("s");

	OpString virtual_path;

	if (!webserver->GetSubServer())
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(virtual_path.Set(argv[0].value.string));

	WebserverResourceDescriptor_Base *res_desc = webserver->GetSubServer()->RemoveSubserverResource(virtual_path);

	OP_DELETE(res_desc);

	return ES_FAILED;
}

/* static */ int
DOM_WebServer::setDefaultHeader(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WEBSERVER);

	BOOL append;

	if (argc == 2)
	{
		DOM_CHECK_ARGUMENTS("ss");
		append = FALSE;
	}
	else
	{
		DOM_CHECK_ARGUMENTS("ssb");
		append = argv[2].value.boolean;
	}

	OP_BOOLEAN status;
	CALL_FAILED_IF_ERROR(status = g_webserver->SetDefaultHeader(argv[0].value.string, argv[1].value.string, append));

	if (status == OpBoolean::IS_FALSE)
		return ES_EXCEPT_SECURITY;

	return ES_FAILED;
}


/* static */ OP_STATUS
DOM_WebServerSession::Make(DOM_WebServerSession *&auth, DOM_Runtime *runtime, const uni_char* session_id, const uni_char* username, WebSubServerAuthType type)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(auth = OP_NEW(DOM_WebServerSession, (type)), runtime, runtime->GetPrototype(DOM_Runtime::WEBSERVERSESSION_PROTOTYPE), "WebServerSession"));

	RETURN_IF_ERROR(auth->m_session_id.Set(session_id));
	return auth->m_username.Set(username);
}


/* virtual */ ES_GetState
DOM_WebServerSession::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_id:
		DOMSetString(value, m_session_id.CStr());
		return GET_SUCCESS;

	case OP_ATOM_userName:
		DOMSetString(value, m_username.CStr());
		return GET_SUCCESS;

	case OP_ATOM_type:
		DOMSetString(value, WebserverAuthenticationId::AuthTypeToString(m_type));
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* static */ int
DOM_WebServerSession::logout(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(session, DOM_TYPE_WEBSERVER_SESSION, DOM_WebServerSession);
	if (argc >= 1)
		DOM_CHECK_ARGUMENTS("b");

	WebserverAuthUser *auth_user;
	if ((auth_user = g_webserverUserManager->GetUserFromSessionId(session->m_session_id)))
	{
		if (argc >= 1 && argv[0].value.boolean)
			auth_user->RemoveAllSessions();
		else
			auth_user->RemoveSession(session->m_session_id);
	}

	return ES_FAILED;
}

DOM_WebServerSession::DOM_WebServerSession(WebSubServerAuthType type)
	: m_type(type)
{
}

DOM_FUNCTIONS_START(DOM_WebServerSession)
	DOM_FUNCTIONS_FUNCTION(DOM_WebServerSession, DOM_WebServerSession::logout, "logout", "b-")
DOM_FUNCTIONS_END(DOM_WebServerSession)

#endif // WEBSERVER_SUPPORT

#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
/* virtual */ ES_GetState
DOM_WebServerObjectArray::GetIndex(int property_index, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	if (property_index >= 0 && property_index < static_cast<int>(GetCount()))
	{
		DOMSetObject(return_value, Get(property_index));
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_GetState
DOM_WebServerObjectArray::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name == OP_ATOM_length)
	{
		DOMSetNumber(value, GetCount());
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ void
DOM_WebServerObjectArray::GCTrace()
{
	for (unsigned index = 0; index < GetCount(); ++index)
		GCMark(Get(index));
}
#endif //UPNP_SUPPORT && UPNP_SERVICE_DISCOVERY

