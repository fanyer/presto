/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** @author Johannes Hoff jhoff@opera.com
*/

#include "core/pch.h"

#ifdef JS_SCOPE_CLIENT

#include "modules/dom/src/opera/domscopeclient.h"

#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/scope/scope_message.h"
#include "modules/scope/scope_connection_manager.h"

JS_ScopeClient::JS_ScopeClient(DOM_Runtime *origining_runtime)
	: OpScopeClient()
	  , owner(origining_runtime)
	  , cb_host_connected(NULL)
	  , cb_receive(NULL)
	  , cb_host_quit(NULL)
	  , state(State_Idle)
	  , tag_counter(1)
	  , connect_tag(0)
	  , host_quit_sent(FALSE)
{
	OP_ASSERT(origining_runtime);
	SetMessageType(OpScopeTPHeader::ECMAScript);
}

OP_STATUS
JS_ScopeClient::Construct(ES_Object *host_connected, ES_Object *host_receive, ES_Object *host_quit)
{
	OP_ASSERT(host_connected && host_receive && host_quit);

	if (!owner->Protect(host_connected))
		return OpStatus::ERR_NO_MEMORY;
	cb_host_connected = host_connected;

	if (!owner->Protect(host_receive))
		return OpStatus::ERR_NO_MEMORY;
	cb_receive = host_receive;

	if (!owner->Protect(host_quit))
		return OpStatus::ERR_NO_MEMORY;
	cb_host_quit = host_quit;

	return OpStatus::OK;
}

JS_ScopeClient::~JS_ScopeClient()
{
	serviceCallbacks.Clear();

	if (cb_host_quit)
		owner->Unprotect(cb_host_quit);
	if (cb_receive)
		owner->Unprotect(cb_receive);
	if (cb_host_connected)
		owner->Unprotect(cb_host_connected);

	owner = NULL;

	// Ensure that the quit-callback is never called in case the environment
	// is being torn down.
	host_quit_sent = TRUE;

	// Tell scope that it should no longer use this client.
	if (GetHostManager())
		OpStatus::Ignore(OpScopeConnectionManager::Disconnect(this));

	OP_ASSERT(!GetHost() && !GetHostManager());
}

void
JS_ScopeClient::Disconnect(BOOL allow_js)
{
	state = State_Idle;
	if (allow_js)
		OpStatus::Ignore(DisconnectJS()); // Tell the JS client that it has disconnected.
	else
		host_quit_sent = TRUE;

	OpStatus::Ignore(OpScopeConnectionManager::Disconnect(this));

	OP_ASSERT(!GetHost() && !GetHostManager());
	OP_ASSERT(host_quit_sent);
}

OP_STATUS
JS_ScopeClient::DisconnectJS()
{
	if (!host_quit_sent)
	{
		host_quit_sent = TRUE;
		if (cb_host_quit && owner)
			RETURN_IF_ERROR(owner->GetEnvironment()->GetAsyncInterface()->CallFunction(cb_host_quit, 0, 0, NULL));
	}
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JS_ScopeClient::OnHostAttached(OpScopeHost *host)
{
	OP_ASSERT(state == State_Idle);
	// Wait for the service list to be sent from the scope host
	state = State_ServiceList;
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JS_ScopeClient::OnHostDetached(OpScopeHost *host)
{
	state = State_Idle;
	if (!host_quit_sent)
		return DisconnectJS();
	else
		return OpStatus::OK;
}

/* virtual */ OP_STATUS
JS_ScopeClient::OnHostMissing()
{
	// The client will not be deleted here, instead it is up to the owner.
	// The owner is either DOM_EnvironmentImpl or JS_Opera::scopeAddClient.
	// If DOM_EnvironmentImpl is the owner then the client is deleted when
	// a new client as added or the dom environment is torn down
	// @see DOM_EnvironmentImpl::BeforeUnload(), this is the same
	// behaviour when an existing client gets disconnected.
	//
	// Note: It would be better to use a shared_ptr type class in the future
	//       to manage client lifetime.

	state = State_Idle;

	OP_STATUS status = DisconnectJS();

	return status;
}

/* virtual */ OP_STATUS
JS_ScopeClient::EnableService(const uni_char *name, ES_Object *cb_service_enabled, unsigned int tag)
{
	OP_ASSERT(state == State_Connected);
	if (state != State_Connected)
		return OpStatus::ERR;
	if (GetHostVersion() >= 1)
		return OpStatus::ERR;

	OpAutoPtr<OpScopeTPMessage> msg;
	RETURN_IF_ERROR(OpScopeProtocolService::CreateEnableMessage(msg, name, tag));
	return SendToHost(msg);
}

OP_STATUS
JS_ScopeClient::SetString(ES_Value *value, ByteBuffer *data, TempBuffer &tmp)
{
	OP_ASSERT(data != NULL);
	OP_ASSERT(data->Length() % 2 == 0);
	char *str = data->Copy();
	if (str == NULL)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = tmp.Append(reinterpret_cast<uni_char *>(str), data->Length() / 2);
	OP_DELETEA(str);
	if (OpStatus::IsError(status))
		return status;

	DOM_Object::DOMSetString(value, &tmp);
	return OpStatus::OK;
}

OP_STATUS
JS_ScopeClient::OnServices(OpAutoPtr<OpScopeTPMessage> &message)
{
	if (message->Version() != 0 || message->Data() == NULL)
		return OpStatus::ERR;

	// Copy the service list from the message, it is stored in a bytebuffer as UTF-16BE.
	ByteBuffer *data = message->Data();
	OP_ASSERT(data != NULL);
	if (data == NULL)
		return OpStatus::ERR_NULL_POINTER;
	OP_ASSERT(data->Length() % 2 == 0);
	char *str = data->Copy();
	if (str == NULL)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS status = service_list.Append(reinterpret_cast<uni_char *>(str), data->Length() / 2);
	OP_DELETEA(str);
	if (OpStatus::IsError(status))
		return status;

	if (GetHostVersion() >= 1)
	{
		state = State_ConnectResponse;
		connect_tag = tag_counter++; // These tags are unique before the control is passed to the DOM client (in OnHostConfigured).

		OpAutoPtr<OpScopeTPMessage> msg;
		BOOL immediate;
		// Connect to the host and configure the default format.
		RETURN_IF_ERROR(Configure(connect_tag, immediate));
		if (immediate)
		{
			state = State_Connected;
			return OnHostConfigured();
		}
		else
			return OpStatus::OK; // Response for 'Connect' will come at a later time.
	}
	else
	{
		state = State_Connected;
		return OnHostConfigured();
	}
}

OP_STATUS
JS_ScopeClient::OnHostConfigured()
{
	ES_Value arguments[1];
	DOM_Object::DOMSetString(arguments + 0, service_list.CStr());

	return owner->GetEnvironment()->GetAsyncInterface()->CallFunction(cb_host_connected, 0, 1, arguments);
}

OP_STATUS
JS_ScopeClient::OnQuit()
{
	return owner->GetEnvironment()->GetAsyncInterface()->CallFunction(cb_host_quit, 0, 0, NULL);
}

/* virtual */ OP_STATUS
JS_ScopeClient::Receive(OpAutoPtr<OpScopeTPMessage> &message)
{
	OP_ASSERT(state != State_Idle);

	if (state == State_ServiceList)
	{
		if ( (message->ServiceName().Compare("scope") == 0 && message->CommandID() == OpScopeProtocolService::Command_OnServices) ||
			 (message->ServiceName().Compare("*services") == 0 && message->Version() == 0) )
		{
			return OnServices(message);
		}

		// Invalid message, quit.
		RETURN_IF_ERROR(OnQuit());
		return OpStatus::OK;
	}
	else if (state == State_ConnectResponse)
	{
		if (message->Version() == 0 || message->ServiceName().Compare("scope") != 0 || message->CommandID() != OpScopeProtocolService::Command_Connect)
		{
			RETURN_IF_ERROR(OnQuit());
			return OpStatus::OK;
		}
		if (message->TransportType() == OpScopeTPHeader::STP_Error || message->Tag() != connect_tag)
		{
			RETURN_IF_ERROR(OnQuit());
			return OpStatus::OK;
		}
		state = State_Connected;
		return OnHostConfigured();
	}
	// else state == State_Connected.

	if (message->Version() >= 1 && message->Type() != OpScopeTPHeader::ECMAScript)
	{
		if (message->Type() == OpScopeTPHeader::JSON)
		{
			// Decode from JSON to ES.
			ES_Object *root = NULL;
			OpJSONToES to_es(root, owner);
			ByteBuffer *buffer = message->Data();
			to_es.Construct(*buffer);
			RETURN_IF_ERROR(to_es.Read());

			RETURN_IF_ERROR(message->SetESObject(root, owner));
			// Message is decoded to ES, continue normal processing.
		}
		else // XML/Protocol buffer.
		{
			// Unsupported type, ignore message.
			OP_ASSERT(!"Client should not receive non-JSON messages when STP/1 is used");
			return OpStatus::OK;
		}
	}

	ES_Value arguments[5];
	int argc = 2;

	TempBuffer tmp;
	DOM_Object::DOMSetString(&arguments[0], message->ServiceName().CStr());
	if (message->Type() == OpScopeTPHeader::ECMAScript)
	{
		if (message->GetESObject() == NULL)
			return OpStatus::ERR;
		DOM_Object::DOMSetObject(&arguments[1], message->GetESObject());
		DOM_Object::DOMSetNumber(&arguments[2], message->CommandID());
		DOM_Object::DOMSetNumber(&arguments[3], message->Status());
		DOM_Object::DOMSetNumber(&arguments[4], message->Tag());
		argc = 5;
	}
	else
	{
		RETURN_IF_ERROR(SetString(&arguments[1], message->Data(), tmp));
	}
	return owner->GetEnvironment()->GetAsyncInterface()->CallFunction(cb_receive, 0, argc, arguments);
}

/* virtual */ OP_STATUS
JS_ScopeClient::Serialize(OpScopeTPMessage &msg, const OpProtobufInstanceProxy &proxy, OpScopeTPHeader::MessageType type)
{
	if (type == OpScopeTPMessage::ECMAScript)
		return SerializeECMAScript(msg, proxy, owner);
	else
		return SerializeDefault(msg, proxy, type);
}

/* static */ OP_STATUS
JS_ScopeClient::SerializeECMAScript(OpScopeTPMessage &msg, const OpProtobufInstanceProxy &proxy, ES_Runtime *runtime)
{
	ES_Object *obj = NULL;
	OpESOutputStream stream(obj, runtime);
	RETURN_IF_ERROR(stream.Write(proxy));
	RETURN_IF_ERROR(msg.SetESObject(obj, runtime));
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
JS_ScopeClient::Parse(const OpScopeTPMessage &msg, OpProtobufInstanceProxy &proxy, OpScopeTPError &error)
{
	OpScopeTPMessage::MessageType type = msg.Type();
	if (type == OpScopeTPMessage::ECMAScript)
	{
		OpESInputStream stream;
		RETURN_IF_ERROR(stream.Construct(msg.GetESObject(), msg.GetESRuntime()));
		OP_STATUS status = stream.Read(proxy);
		// There is no detailed error description available in the parser so we return a generic message.
		if (status == OpStatus::ERR_PARSING_FAILED)
		{
			error.SetStatus(OpScopeTPHeader::BadRequest);
			error.SetStaticDescription(UNI_L("Parser error while parsing ECMAScript object"));
		}
		else if (OpStatus::IsError(status))
		{
			error.SetStatus(OpScopeTPHeader::InternalError);
			error.SetStaticDescription(UNI_L("Internal error while parsing ECMAScript object"));
		}
		return status;
	}
	else
		return ParseDefault(msg, proxy, error);
}


#endif // JS_SCOPE_CLIENT
