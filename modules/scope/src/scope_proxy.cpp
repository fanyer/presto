/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Johannes Hoff / Jan Borsodi
**
*/

#include "core/pch.h"

#ifdef SCOPE_ACCEPT_CONNECTIONS

#include "modules/scope/src/scope_proxy.h"
#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/protobuf/src/protobuf_input.h"
#include "modules/protobuf/src/protobuf_output.h"
#include "modules/protobuf/src/protobuf_ecmascript.h"
#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_manager.h"
#include "modules/url/url_socket_wrapper.h"

/* OpScopeNetworkServer */

OpScopeNetworkServer::OpScopeNetworkServer(/*OpScopeManagerInterface *scope_manager*/)
	: listening_socket(NULL)
	, is_listening(FALSE)
	, is_address_in_use(FALSE)
	, listen_port(0)
{
}

OpScopeNetworkServer::~OpScopeNetworkServer()
{
	State state = GetListenerState();

	ResetSocket();

	if (state == STATE_LISTENING)
		NotifyListenerClosed(this);
}

OP_STATUS
OpScopeNetworkServer::Construct()
{
	return OpStatus::OK;
}

OpScopeNetworkServer::State
OpScopeNetworkServer::GetListenerState() const
{
	if (listening_socket && is_listening)
		return STATE_LISTENING;
	else
		return STATE_IDLE;
}

void
OpScopeNetworkServer::ResetSocket()
{
	is_listening = FALSE;
	listen_port = 0;
	listen_address.Empty();

	if (OpSocket *closing_socket = listening_socket)
	{
		listening_socket = NULL;
		closing_socket->Close();
		OP_DELETE(closing_socket);
	}

	OP_DELETE(sockaddr);
	sockaddr = NULL;
}

void
OpScopeNetworkServer::Disconnect()
{
	BOOL was_listening = GetListenerState() == STATE_LISTENING;

	ResetSocket();

	if (was_listening)
		NotifyListenerClosed(this);
}

OP_STATUS
OpScopeNetworkServer::Listen(OpStringC &address, int port)
{
	RETURN_IF_ERROR(listen_address.Set(address));
	listen_port = port;
	return SetupListener();
}

OP_STATUS
OpScopeNetworkServer::Listen(int port)
{
	if (port == 0)
		RETURN_IF_ERROR(listen_address.Set(UNI_L("127.0.0.1")));
	else
		RETURN_IF_ERROR(listen_address.Set(UNI_L("")));
	listen_port = port;
	return SetupListener();
}

OP_STATUS
OpScopeNetworkServer::SetupListener()
{
	if (is_listening)
		ResetSocket();

	RETURN_IF_ERROR(SocketWrapper::CreateTCPSocket(&listening_socket, this, SocketWrapper::NO_WRAPPERS));
	RETURN_IF_ERROR(OpSocketAddress::Create(&sockaddr));

	OP_STATUS status;

	is_listening = TRUE;
	if (listen_port == 0)
	{
		sockaddr->FromString(listen_address.CStr());
		int port = 49152; // Dynamic/private ports: 49152-65535 <http://www.iana.org/assignments/port-numbers>
		do {
			port++;
			sockaddr->SetPort(port);
			listen_port = port;
			// See OnSocketListenError
			is_address_in_use = FALSE;
			status = listening_socket->Listen(sockaddr, 1);
		} while (is_address_in_use && port < 65535);
	}
	else
	{
		if (!listen_address.IsEmpty())
			RETURN_IF_ERROR(sockaddr->FromString(listen_address.CStr()));
		sockaddr->SetPort(listen_port);
		status = listening_socket->Listen(sockaddr, 1);
	}

	if (!is_listening)
		ResetSocket();

	if (OpStatus::IsError(status) || !is_listening)
		NotifyListeningFailure(this);
	else
		NotifyListeningSuccess(this);
	return status;
}

/* virtual */
void
OpScopeNetworkServer::OnSocketListenError(OpSocket*, OpSocket::Error error)
{
	if (error == OpSocket::ADDRESS_IN_USE)
		is_address_in_use = TRUE;
	is_listening = FALSE;
}

/* virtual */
void
OpScopeNetworkServer::OnSocketConnectionRequest(OpSocket* listsocket)
{
	OP_ASSERT(listsocket == listening_socket);
	if (listsocket != listening_socket)
		return;

	NotifyConnectionRequest(this);
}

/* virtual */
void
OpScopeNetworkServer::OnSocketClosed(OpSocket* closing_socket)
{
	if (closing_socket != listening_socket)
		return;

	ResetSocket();

	NotifyListenerClosed(this);
}

OP_STATUS
OpScopeNetworkServer::Accept(OpScopeNetwork *network)
{
	OP_ASSERT(network);

	OpSocket *socket = NULL;
	RETURN_IF_ERROR(SocketWrapper::CreateTCPSocket(&socket, network, SocketWrapper::NO_WRAPPERS));
	if (OpStatus::IsError(listening_socket->Accept(socket)))
	{
		OP_DELETE(socket);
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(network->Connect(socket));
	return OpStatus::OK;
}

/* OpScopeNetworkHost */

OpScopeNetworkHost::OpScopeNetworkHost(int desired_stp_version, BOOL auto_disconnect, OpScopeHostManager *host_manager)
	: OpScopeNetwork()
	, OpScopeHost(host_manager)
	, auto_disconnect(auto_disconnect)
	, state(State_None)
	, setup_state(SetupState_Waiting)
	, desired_stp_version(desired_stp_version)
#ifdef SCOPE_MESSAGE_TRANSCODING
	, transcoder(*this)
#endif // SCOPE_MESSAGE_TRANSCODING
{
}

OpScopeNetworkHost::~OpScopeNetworkHost()
{
	// OpScopeNetwork will cleanup socket, socketaddr and connected flag
	// It will also notify external listeners if needed
	ResetState();

	if (GetClient())
		DetachClient();

	if (OpScopeHostManager *manager = GetHostManager())
		manager->ReportDestruction(this);
}

OP_STATUS
OpScopeNetworkHost::Construct()
{
	return OpScopeNetwork::Construct(&incoming);
}

/*virtual*/
OP_STATUS
OpScopeNetworkHost::OnClientAttached(OpScopeClient *client)
{
	OP_ASSERT(client);
	if (service_message.get() != NULL)
	{
		if (!client)
			return OpStatus::ERR_NULL_POINTER;
		if (client->GetHost() == this)
		{
			OpAutoPtr<OpScopeTPMessage> message_copy(OP_NEW(OpScopeTPMessage, ()));
			RETURN_OOM_IF_NULL(message_copy.get());
			RETURN_IF_ERROR(message_copy->Copy(*service_message.get()));
			RETURN_IF_ERROR(client->Receive(message_copy));
		}
	}
	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeNetworkHost::OnClientDetached(OpScopeClient *client)
{
	OP_ASSERT(client);

	if (auto_disconnect)
	{
		Disconnect();
	}

	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeNetworkHost::InitializeConnection()
{
	RETURN_IF_ERROR(SetWriterEnabled(FALSE));
	RETURN_IF_ERROR(SetReaderEnabled(TRUE));
	state = State_Handshake;
	return OpStatus::OK;
}

/* virtual */
void
OpScopeNetworkHost::OnSocketClosed(OpSocket* closing_socket)
{
	OpScopeNetwork::OnSocketClosed(closing_socket);
}

/*virtual*/
void
OpScopeNetworkHost::Disconnect()
{
	OpScopeNetwork::Disconnect();

	if (GetClient())
		DetachClient();
}

/*virtual*/
void
OpScopeNetworkHost::Reset()
{
	ResetState();
	OpScopeNetwork::Reset();
}

void
OpScopeNetworkHost::ResetState()
{
	state = State_None;
	service_message.reset();
}


/*virtual*/
OP_STATUS
OpScopeNetworkHost::OnHandshakeCompleted(unsigned int version)
{
	OpScopeNetwork::SetProtocolVersion(version, TRUE);
	state = version == 0 ? State_Stp0 : State_Stp1;

	return ProcessSetup(NULL);
}

OP_STATUS
OpScopeNetworkHost::ProcessSetup(const OpScopeTPMessage *message)
{
	switch (setup_state)
	{
		case SetupState_Waiting:
			setup_state = SetupState_Transcoder;
		case SetupState_Transcoder:
		{
			setup_state = SetupState_SendServiceList;
#ifdef SCOPE_MESSAGE_TRANSCODING
			// If the transcoder fails in any way it will be disabled and the next state is processed
			if (OpStatus::IsError(transcoder.ProcessSetup(message)))
			{
				transcoder.Disable();
			}
			else if (!transcoder.IsSetupFinished())
			{
				setup_state = SetupState_Transcoder;
				break;
			}
#endif // SCOPE_MESSAGE_TRANSCODING
		}
		case SetupState_SendServiceList:
		{
			setup_state = SetupState_Done;
			OP_ASSERT(service_message.get() != NULL);
			if (service_message.get() == NULL)
				return OpStatus::ERR_NULL_POINTER;
			OpScopeClient *client = GetClient();
			if (!client)
				return OpStatus::ERR_NULL_POINTER;
			if (client->GetHost() != this)
				return OpStatus::OK;
			OpAutoPtr<OpScopeTPMessage> message_copy(OP_NEW(OpScopeTPMessage, ()));
			RETURN_OOM_IF_NULL(message_copy.get());
			RETURN_IF_ERROR(message_copy->Copy(*service_message.get()));
			RETURN_IF_ERROR(client->Receive(message_copy));
			break;
		}
		case SetupState_Done:
			break;
		default:
			OP_ASSERT(!"Unknown setup state");
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeNetworkHost::InitiateHandshake(unsigned int ver)
{
	if (ver >= 1)
	{
		// Initiate STP/1 handshake
		OpAutoPtr<OpScopeTPMessage> msg(OP_NEW(OpScopeTPMessage, (OpScopeTPHeader::STP_Call, UNI_L("*enable"), 0, OpScopeTPHeader::OK, 0, OpScopeTPHeader::Version_0)));
		RETURN_OOM_IF_NULL(msg.get());
		const uni_char *stp_selection = UNI_L("stp-1");
		RETURN_IF_ERROR(msg->CreateEmptyData(OpScopeTPHeader::JSON));
		RETURN_IF_ERROR(msg->Data()->AppendBytes((const char *)stp_selection, 5*sizeof(uni_char)));
		RETURN_IF_ERROR(EnqueueMessage(msg));
	}
	else
	{
		SetProtocolVersion(ver, TRUE);
	}
	return OpStatus::OK;
}

void 
OpScopeNetworkHost::OnSocketDataReady(OpSocket* socket)
{
	char buffer[1024]; // ARRAY OK 2009-05-05 jhoff
	unsigned int len;
	RETURN_VOID_IF_ERROR(socket->Recv(buffer, ARRAY_SIZE(buffer), &len));
	RETURN_VOID_IF_ERROR(incoming.AppendBytes(buffer, len));

	if (state == State_HandshakeResponse || state == State_HandshakeVersion)
	{
		if (state == State_HandshakeResponse)
		{
			if (incoming.Length() < 4) // Not enough data yet
				return;
			incoming.Extract(0, 4, buffer);
			if (buffer[0] != 'S' || buffer[1] != 'T' || buffer[2] != 'P' || buffer[3] != '/')
				return; // FIXME: Error handling?
			state = State_HandshakeVersion;
			incoming.Consume(4);
		}

		if (state == State_HandshakeVersion)
		{
			unsigned int ver;
			if (!ParseVersion('\n', ver))
				return;

			if (ver != 1)
				return; // FIXME: Error handling?
			state = State_Stp1;

			RETURN_VOID_IF_ERROR(OnHandshakeCompleted(ver));
			OP_STATUS status = OnDataReady();
			OpStatus::Ignore(status);
			return;
		}
	}
	else // State_Handshake || State_Stp0 || State_Stp1
	{
		OpScopeNetwork::OnDataReady();
	}
}

BOOL
OpScopeNetworkHost::ParseVersion(char terminator, unsigned int &version)
{
	unsigned remaining = incoming.Length();
	if (remaining < 2)
	{
		return FALSE;
	}
	char buffer[11]; // 10 digits + terminator  // ARRAY OK 2009-05-05 jhoff
	unsigned int buffer_len = MIN(ARRAY_SIZE(buffer), remaining);
	incoming.Extract(0, buffer_len, buffer);
	unsigned int ver = 0;
	unsigned int i;
	for (i = 0; i < buffer_len; ++i)
	{
		char c = buffer[i];
		if (c == terminator && i == 0)
			return FALSE; // SetError(InvalidData);
		if (c == terminator)
			break;
		if (c < '0' || c > '9')
			return FALSE; // SetError(InvalidData);
		if (i == (11-1)) // No marker found after 10 digits, invalid format
			return FALSE; // SetError(InvalidData);
		if (i == buffer_len - 1) // Not enough data, return and wait for more
		{
			return FALSE;
		}
		ver *= 10;
		ver += (c-'0');
	}
	incoming.Consume(i + 1);
	version = ver;
	return TRUE;
}

/*virtual*/
OP_STATUS
OpScopeNetworkHost::OnMessageParsed(const OpScopeTPMessage &message)
{
	if (state == State_None)
		return OpStatus::OK;
	if (state == State_HandshakeResponse || state == State_HandshakeVersion)
	{
		// OnSocketDataReady handles this state
		return OpStatus::OK;
	}
	else if (state == State_Handshake)
	{
		if (message.Version() != 0 || message.ServiceName().Compare(UNI_L("*services")) != 0)
		{
			// FIXME: Invalid message received, expected service list, should disconnect
			return OpStatus::OK;
		}
		OP_ASSERT(message.Data() != NULL);
		OpString payload;
		RETURN_IF_ERROR(OpProtobufUtils::ExtractUTF16BE(payload, *message.Data()));
		const uni_char *str = payload.CStr();
		BOOL has_stp1 = FALSE;
		while (*str != '\0')
		{
			const uni_char *match = uni_strchr(str, ',');
			if (match == NULL)
			{
				has_stp1 = uni_strcmp(str, UNI_L("stp-1")) == 0;
				break;
			}
			int len = match - str;
			if (uni_strncmp(str, UNI_L("stp-1"), len) == 0)
			{
				has_stp1 = TRUE;
				break;
			}
			str = match + 1;
		}
		int stp_version = 0;
		if (desired_stp_version == 0)
		{
			stp_version = 0;
		}
		else if (desired_stp_version == 1 && has_stp1)
		{
			stp_version = 1;
		}
		else if (desired_stp_version < 0)
		{
			stp_version = has_stp1 ? 1 : 0;
		}
		else
		{
			state = State_None;
			return OpStatus::OK; // FIXME: Host does not have desired version, should disconnect. Alternatively a callback could be used for more advanced negatioation.
		}
		SetWriterEnabled(TRUE);
		state = stp_version == 0 ? State_Stp0 : State_HandshakeResponse;

		service_message.reset(OP_NEW(OpScopeTPMessage, ()));
		RETURN_OOM_IF_NULL(service_message.get());
		RETURN_IF_ERROR(service_message->Copy(message));

		OP_STATUS status = InitiateHandshake(stp_version);

		if(OpStatus::IsError(status) || state != State_Stp0)
			return status;
	}

	if (state == State_Stp0)
	{
		OpAutoPtr<OpScopeTPMessage> message_copy(OP_NEW(OpScopeTPMessage, ()));
		RETURN_OOM_IF_NULL(message_copy.get());
		RETURN_IF_ERROR(message_copy->Copy(message));
		if (GetClient() == NULL)
			return OpStatus::ERR_NULL_POINTER;
		return GetClient()->Receive(message_copy);
	}
	else if (state == State_Stp1)
	{
		if (setup_state != SetupState_Done)
			return ProcessSetup(&message);

#ifdef SCOPE_MESSAGE_TRANSCODING
		// If the transcoder is active check if this message should be filtered
		if (transcoder.IsEnabled())
		{
			BOOL skip = FALSE;
			if (OpStatus::IsError(transcoder.PreProcessMessageToClient(message, GetClient(), skip)))
				transcoder.Disable(); // Any error in the transcoder disables it for the remaining runs
			if (skip)
				return OpStatus::OK;
		}
#endif // SCOPE_MESSAGE_TRANSCODING

		OpAutoPtr<OpScopeTPMessage> message_copy(OP_NEW(OpScopeTPMessage, ()));
		RETURN_OOM_IF_NULL(message_copy.get());
		RETURN_IF_ERROR(message_copy->Copy(message));
		OpScopeClient *client = GetClient();
		if (client == NULL)
			return OpStatus::OK; // FIXME: Should handle error, ignore or disconnect

#ifdef SCOPE_MESSAGE_TRANSCODING
		// If transcoding is on we need to convert it to a format the client can understand, if possible
		if (transcoder.IsActive())
		{
			OP_STATUS status = transcoder.TranscodeMessageToClient(*message_copy.get(), client, message);
			if (OpStatus::IsError(status)) // Any error in the transcoder disables it for the remaining runs
				transcoder.Disable();
		}
#endif // SCOPE_MESSAGE_TRANSCODING

		return client->Receive(message_copy);
	}

	OP_ASSERT(!"Unknown internal state");
	return OpStatus::ERR;
}

/*virtual*/
void
OpScopeNetworkHost::OnError(OpScopeTPReader::Error error)
{
	// FIXME: Handle errors, should disconnect?
}

/*virtual*/
const uni_char *
OpScopeNetworkHost::GetType() const
{
	return UNI_L("host-connection");
}

/*virtual*/
int
OpScopeNetworkHost::GetVersion() const
{
	return OpScopeNetwork::ProtocolVersion();
}

/*virtual*/
OP_STATUS
OpScopeNetworkHost::Receive( OpScopeClient *client, OpAutoPtr<OpScopeTPMessage> &message )
{
	if (state == State_None || state == State_Handshake || state == State_HandshakeResponse || state == State_HandshakeVersion)
		return OpStatus::OK;

	if (state == State_Stp0)
	{
		OP_ASSERT(message->Type() == OpScopeTPHeader::JSON || message->Type() == OpScopeTPHeader::XML);
		if (message->Type() != OpScopeTPHeader::JSON && message->Type() != OpScopeTPHeader::XML)
			return OpStatus::ERR; // Cannot send message of this type
		return EnqueueMessage(message);
	}
	else
	{
#ifdef SCOPE_MESSAGE_TRANSCODING
		if (transcoder.IsEnabled())
		{
			BOOL skip = FALSE;
			if (OpStatus::IsError(transcoder.PreProcessMessageFromClient(*message, client, skip)))
				transcoder.Disable(); // Any error in the transcoder disables it for the remaining runs
			if (skip)
				return OpStatus::OK;
		}
#endif // SCOPE_MESSAGE_TRANSCODING

		if (message->Type() == OpScopeTPHeader::ECMAScript)
		{
			// Convert from ECMAScript to a suitable network format
			OP_ASSERT(message->GetESObject() != NULL);
			if (message->GetESObject() == NULL)
				return OpStatus::ERR_NULL_POINTER;
			ES_Object *obj = message->GetESObject();

			OpAutoPtr<OpScopeTPMessage> new_message;
			RETURN_IF_ERROR(OpScopeTPMessage::Clone(new_message, *message.get(), FALSE));

#ifdef SCOPE_MESSAGE_TRANSCODING
			// If transcoding is on we need to convert it to a format the host can understand, if possible
			if (transcoder.IsActive())
			{
				OP_STATUS status = transcoder.TranscodeMessageFromClient(*new_message, client, *message);
				if (OpStatus::IsError(status)) // Any error in the transcoder disables it for the remaining runs
					transcoder.Disable();
				else // FIXME: Should it return ERR if it fails to transcode?
					return EnqueueMessage(new_message);
			}
#endif // SCOPE_MESSAGE_TRANSCODING

			// No transcoding, use OpESToJSON to convert without protocol buffer definitions
			RETURN_IF_ERROR(new_message->CreateEmptyData(OpScopeTPHeader::JSON));

			OpESToJSON to_json;
			RETURN_IF_ERROR(to_json.Construct(obj, message->GetESRuntime()));
			RETURN_IF_ERROR(to_json.Write(*new_message->Data()));
			return EnqueueMessage(new_message);
		}
		return EnqueueMessage(message);
	}
}

OP_STATUS
OpScopeNetworkHost::ConfigureClient(OpScopeClient *client, unsigned int tag, BOOL &immediate)
{
	OP_ASSERT(client);
	immediate = FALSE; // The caller must wait for a response

	OpAutoPtr<OpScopeTPMessage> msg;
	// We configure the host to send events back as JSON
	RETURN_IF_ERROR(OpScopeProtocolService::CreateConfigureMessage(msg, client, tag, OpScopeTPHeader::JSON));
	return Receive(client, msg);
}
#endif // SCOPE_ACCEPT_CONNECTIONS
