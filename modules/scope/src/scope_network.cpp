/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Johannes Hoff / Jan Borsodi
 */
#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/hardcore/mh/mh.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/scope_network.h"
#include "modules/scope/src/scope_protocol_service.h"
#include "modules/scope/src/scope_transport.h"
#include "modules/scope/scope_connection_manager.h"
#include "modules/url/url_socket_wrapper.h"

OpScopeNetwork::OpScopeNetwork()
	: OpScopeTPWriter(FALSE)
	, OpScopeTPReader(TRUE)
	, socket(NULL)
	, sockaddr(NULL)
	, is_connected(FALSE)
	, in_destruct(FALSE)
	, socket_port(0)
	, message_sent(FALSE)
{
	OpStatus::Ignore(OpScopeTPReader::SetEnabled(FALSE));
	OpStatus::Ignore(g_main_message_handler->SetCallBack(this, MSG_SCOPE_PROCESS_MESSAGE, reinterpret_cast<MH_PARAM_1>(this)));
}

OpScopeNetwork::~OpScopeNetwork()
{
	ConnectionState constate = GetConnectionState();

	in_destruct = TRUE;
	g_main_message_handler->UnsetCallBack(this, MSG_SCOPE_PROCESS_MESSAGE);

	ResetSTP();
	if (constate != STATE_CLOSED)
		ResetSocket();

	if (constate == STATE_CONNECTED)
		NotifyConnectionClosed(this);
}

OP_STATUS
OpScopeNetwork::Construct(ByteBuffer *buffer)
{
	return OpScopeTPReader::Construct(buffer);
}

OP_STATUS
OpScopeNetwork::Connect(OpStringC &addr, int port)
{
	if (GetConnectionState() != STATE_CLOSED)
		Disconnect();

	ResetSTP();
	RETURN_IF_ERROR(socket_address.Set(addr));
	socket_port = port;
	RETURN_IF_ERROR(SocketWrapper::CreateTCPSocket(&socket, this, SocketWrapper::NO_WRAPPERS));
	RETURN_IF_ERROR(OpSocketAddress::Create(&sockaddr));
	RETURN_IF_ERROR(sockaddr->FromString(addr.CStr()));
	sockaddr->SetPort(port);

	return socket->Connect(sockaddr);
}

OP_STATUS
OpScopeNetwork::Connect(OpSocket *new_socket)
{
	OP_ASSERT(new_socket);
	if (GetConnectionState() != STATE_CLOSED)
		Disconnect();

	ResetSTP();
	socket = new_socket;
	RETURN_IF_ERROR(OpSocketAddress::Create(&sockaddr));
	RETURN_IF_ERROR(socket->GetSocketAddress(sockaddr));
	RETURN_IF_ERROR(sockaddr->ToString(&socket_address));
	socket_port = 0;

	OnSocketConnected(socket);
	return OpStatus::OK;
}

OP_STATUS
OpScopeNetwork::ResetWriter()
{
	OpScopeTPWriter::SetProtocolVersion(0);
	RETURN_IF_ERROR(OpScopeTPWriter::Reset());
	return OpScopeTPWriter::SetEnabled(FALSE);
}

OP_STATUS
OpScopeNetwork::ResetReader()
{
	OpScopeTPReader::SetProtocolVersion(0);
	RETURN_IF_ERROR(OpScopeTPReader::Reset());
	return OpScopeTPReader::SetEnabled(FALSE);
}

/*virtual*/
void
OpScopeNetwork::Disconnect()
{
	BOOL was_connected = (GetConnectionState() == STATE_CONNECTED);

	Reset();

	if (was_connected)
		NotifyConnectionClosed(this);
}

/*virtual*/
void
OpScopeNetwork::Reset()
{
	ResetSTP();
	if (GetConnectionState() != STATE_CLOSED)
		ResetSocket();
}

void
OpScopeNetwork::ResetSTP()
{
	//
	// Reset the TP writer
	//
	OP_STATUS status;
	status = ResetWriter();
	OP_ASSERT(OpStatus::IsSuccess(status));
	OpStatus::Ignore(status);

	//
	// Reset the TP reader
	//
	status = ResetReader();
	OP_ASSERT(OpStatus::IsSuccess(status));
	OpStatus::Ignore(status);
}

void
OpScopeNetwork::ResetSocket()
{
	is_connected = FALSE;
	socket_port = 0;
	socket_address.Empty();
	message_sent = FALSE;

	//
	// Clean up the socket itself
	//
	if (socket)
	{
		// Remove queued items in sent_queue which matches the socket
		SentData *sent_data = sent_queue.First();
		while (sent_data)
		{
			SentData *cur = sent_data;
			sent_data = sent_data->Suc();;
			if (cur->socket == socket)
			{
				cur->Out();
				OP_DELETE(cur);
			}
		}

		OpSocket *closing_socket = socket;
		socket = NULL; // avoids recursion problems
		closing_socket->Close();
		OP_DELETE(closing_socket);
	}

	OP_DELETE(sockaddr);
	sockaddr = NULL;
}

OpScopeNetwork::ConnectionState
OpScopeNetwork::GetConnectionState() const
{
	if (socket == NULL)
		return STATE_CLOSED;
	else if (is_connected)
		return STATE_CONNECTED;
	else
		return STATE_CONNECTING;
}

OP_STATUS
OpScopeNetwork::EnqueueMessage(OpAutoPtr<OpScopeTPMessage> &msg)
{
	return OpScopeTPWriter::EnqueueMessage(msg);
}

unsigned int
OpScopeNetwork::ProtocolVersion() const
{
	return OpScopeTPWriter::ProtocolVersion();
}

void
OpScopeNetwork::SetProtocolVersion(unsigned int ver, BOOL force)
{
	OpScopeTPWriter::SetProtocolVersion(ver);
	OpScopeTPReader::SetProtocolVersion(ver, force);
}

OP_STATUS
OpScopeNetwork::SetReaderEnabled(BOOL enabled)
{
	return OpScopeTPReader::SetEnabled(enabled);
}

OP_STATUS
OpScopeNetwork::SetWriterEnabled(BOOL enabled)
{
	return OpScopeTPWriter::SetEnabled(enabled);
}

/* virtual */ void 
OpScopeNetwork::OnSocketConnected(OpSocket*)
{
	is_connected = TRUE;

	if (OpStatus::IsSuccess(InitializeConnection()))
	{
		// Network client is ready, it can now be connected to a host
		NotifyConnectionSuccess(this);
	}
	else
	{
		Reset();
		NotifyConnectionFailure(this, OpSocket::NETWORK_NO_ERROR);
	}
}


/*virtual*/
void
OpScopeNetwork::OnSocketClosed(OpSocket* socket)
{
	if (socket != this->socket)
		return;

	Reset();

	NotifyConnectionLost(this);
}

void 
OpScopeNetwork::OnSocketDataReady(OpSocket* socket)
{
	char buffer[1024]; // ARRAY OK 2009-05-05 jhoff
	unsigned int len;
	RETURN_VOID_IF_ERROR(socket->Recv(buffer, ARRAY_SIZE(buffer), &len));
	RETURN_VOID_IF_ERROR(OpScopeTPReader::OnDataReady(buffer, len));
}

OP_STATUS
OpScopeNetwork::OnDataReady()
{
	return OpScopeTPReader::OnDataReady();
}

// From OpScopeTPWriter

/*virtual*/
OP_STATUS
OpScopeNetwork::SendData(const char *data, size_t len)
{
	return socket->Send(data, len);
}

/*virtual*/
OP_STATUS
OpScopeNetwork::OnMessageSent(const OpScopeTPMessage &message)
{
	return OpStatus::OK;
}

OP_STATUS
OpScopeNetwork::EnqueueError(const OpScopeTPMessage &message, const OpScopeTPError &error)
{
	OpAutoPtr<OpScopeTPMessage> err_msg(OP_NEW(OpScopeTPMessage, ()));
	RETURN_OOM_IF_NULL(err_msg.get());
	RETURN_IF_ERROR(err_msg->Copy(message, FALSE));
	RETURN_IF_ERROR(OpScopeClient::SerializeErrorDefault(*err_msg.get(), error, message.Type()));
	return EnqueueMessage(err_msg);
}

void
OpScopeNetwork::OnSocketDataSent(OpSocket *sent_socket, unsigned int bytes_sent)
{
	OpAutoPtr<SentData> sent_data(OP_NEW(SentData, (sent_socket, bytes_sent)));
	OP_ASSERT(sent_data.get());
	if (!sent_data.get())
		return;
	sent_data.release()->Into(&sent_queue);
	if (!message_sent)
	{
		message_sent = TRUE;
		g_main_message_handler->PostMessage(MSG_SCOPE_PROCESS_MESSAGE, reinterpret_cast<MH_PARAM_1>(this), 0);
	}
}

void
OpScopeNetwork::OnSocketConnectError(OpSocket* error_socket, OpSocket::Error error)
{
	if (in_destruct || error_socket != socket)
		return;

	Reset();

	NotifyConnectionFailure(this, error);
}

#ifdef SOCKET_LISTEN_SUPPORT
void OpScopeNetwork::OnSocketConnectionRequest(OpSocket*)
{
	// Ignore it, we don't care
}

/* virtual */ void
OpScopeNetwork::OnSocketListenError(OpSocket*, OpSocket::Error)
{
	// Ignore it, we don't care
}
#endif

void OpScopeNetwork::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    if (msg == MSG_SCOPE_PROCESS_MESSAGE && (GetConnectionState() == STATE_CONNECTED || GetConnectionState() == STATE_CONNECTING))
	{
		message_sent = FALSE;
		OpAutoPtr<SentData> sent_data(sent_queue.First());
		if (sent_data.get())
		{
			sent_data->Out();
			OP_ASSERT(sent_data->socket == GetSocket());
			OpStatus::Ignore(OnDataSent(sent_data->bytes_sent));
		}
	}
}

void OpScopeNetwork::OnSocketReceiveError(OpSocket*, OpSocket::Error)
{
	// Ignore it, we don't care
}

void OpScopeNetwork::OnSocketSendError(OpSocket* error_socket, OpSocket::Error)
{
	if (in_destruct || error_socket != socket)
		return;

	OP_STATUS status = OpScopeTPWriter::Reset();
	OpStatus::Ignore(status);
}

void OpScopeNetwork::OnSocketCloseError(OpSocket*, OpSocket::Error)
{
	// Ignore it, we don't care
}

void OpScopeNetwork::OnSocketDataSendProgressUpdate(OpSocket*, UINT bytes_sent)
{
	// Ignore it, we don't care
	(void)bytes_sent;
}

/* OpScopeNetworkClient */

/*
* This class handles connection with a network client.
* It starts with a handshake where it sends of the service list to the
* client over STP/0, the client responds by enabling the stp-1 service.
* Finally STP/1 is enabled and the handshake response ("STP/1\n") is sent
* to the client.
*
* After this normal message processing is activated.
*
* An overview of the handshake:
*  => is data sent to client
*  <= is data received from client
*  xx is the size of the STP/0 payload as ASCII numbers
*  STP/0: Means message is encoded for STP/0
*  STP/1: Means message is encoded for STP/1
*
* state                reader writer data
* ==================== ====== ====== ===================================================
* SendServiceList      off    on
*                                    => STP/0: xx *services ecmascript-debugger,stp-0,stp-1,...
* Handshake            on     off
*                                    <= STP/0: xx *enable stp-1
* HandshakeResponse    off    off
*                                    => socket: "STP/1\n"
* Normal               on     on
*                                    <= STP/1: ...
*                                    => STP/1: ...
*
*/


OpScopeNetworkClient::OpScopeNetworkClient(OpScopeTPMessage::MessageType type, BOOL auto_disconnect, OpScopeHostManager *host_manager)
	: OpScopeClient(type, host_manager)
	, OpScopeNetwork()
	, state(State_Normal)
	, handshake_remaining(0)
	, auto_disconnect(auto_disconnect)
{
}

/*virtual*/
OpScopeNetworkClient::~OpScopeNetworkClient()
{
	// OpScopeNetwork will cleanup socket, socketaddr and connected flag
	// It will also notify external listeners if needed
	ResetState();

	if (GetHost())
		DetachHost();

	if (OpScopeHostManager *manager = GetHostManager())
		manager->ReportDestruction(this);
}

OP_STATUS
OpScopeNetworkClient::Construct()
{
	return OpScopeNetwork::Construct(&incoming);
}

/*virtual*/
void
OpScopeNetworkClient::Disconnect()
{
	ResetState();
	OpScopeNetwork::Disconnect();

	if (GetHost())
		DetachHost();
}

/*virtual*/
void
OpScopeNetworkClient::Reset()
{
	ResetState();
	OpScopeNetwork::Reset();
}

void
OpScopeNetworkClient::ResetState()
{
	state = State_Normal;
}

/*virtual*/
void
OpScopeNetworkClient::OnSocketClosed(OpSocket* socket)
{
	OpScopeNetwork::OnSocketClosed(socket);

	if (GetHost())
		DetachHost();
}

// From OpScopeClient

/*virtual*/
OP_STATUS
OpScopeNetworkClient::Receive(OpAutoPtr<OpScopeTPMessage> &message)
{
	return EnqueueMessage(message);
}

/*virtual*/
OP_STATUS
OpScopeNetworkClient::Serialize(OpScopeTPMessage &to, const OpProtobufInstanceProxy &from, OpScopeTPHeader::MessageType type)
{
	return OpScopeClient::SerializeDefault(to, from, type);
}

/*virtual*/
OP_STATUS
OpScopeNetworkClient::Parse(const OpScopeTPMessage &from, OpProtobufInstanceProxy &to, OpScopeTPError &error)
{
	return OpScopeClient::ParseDefault(from, to, error);
}

/*virtual*/
OP_STATUS
OpScopeNetworkClient::OnHostAttached(OpScopeHost *host)
{
	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeNetworkClient::OnHostDetached(OpScopeHost *host)
{
	if (auto_disconnect)
		Disconnect();

	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeNetworkClient::OnHostMissing()
{
	return OpStatus::OK;
}

// From OpScopeTPReader

/*virtual*/
OP_STATUS
OpScopeNetworkClient::OnMessageParsed(const OpScopeTPMessage &message)
{
	if (!GetHost())
	{
		// No host attached, report error to client
		OP_ASSERT(!"No host attached to client connection, should not happen");
		OpScopeTPError error;
		error.SetStatus(OpScopeTPHeader::InternalError);
		error.SetStaticDescription(UNI_L("No host attached to client connection, should not happen"));
		return EnqueueError(message, error);
	}

	if (state == State_Handshake)
	{
		if (ProtocolVersion() == 0)
		{
			if ((message.ServiceName().Compare(UNI_L("scope")) == 0 && message.CommandID() == OpScopeProtocolService::Command_Enable) ||
				message.ServiceName().Compare(UNI_L("*enable")) == 0)
			{
				// TODO: Optimize extraction, since we know the length of the text we can use a stack buffer
				OP_ASSERT(message.Data() != NULL);
				OpString payload;
				RETURN_IF_ERROR(OpProtobufUtils::ExtractUTF16BE(payload, *message.Data()));
				if (payload.Compare(UNI_L("stp-1")) == 0)
				{
					SetProtocolVersion(1, TRUE);
					return SwitchState(State_HandshakeResponse);
				}
			}
			SetProtocolVersion(0, TRUE);
			state = State_Normal;
		}
		else
		{
			OP_ASSERT(!"STP/1+ handshake not implemented");
			return OpStatus::ERR;
		}
	}
	else if (state == State_HandshakeResponse || state == State_SendServiceList)
	{
		// Ignore incoming messages while in handshake response or while sending service list
		OP_ASSERT(!"Should not happen");
		return OpStatus::OK;
	}
	if (message.Version() == 0)
	{
		const OpString &service_name = message.ServiceName();
		unsigned int len = service_name.Length();
		if (len == 0)
			return OpStatus::OK;

		// For some reason there is no const version of [] operator.
		const uni_char *chars = service_name.CStr();
		if (chars[0] == '*')
		{
			// Ignore all special messages (start with *), currently they are:
			// - *enable
			// - *disable
			// - *quit
			//
			// Pre core-2.4 internal proxies will look for *enable/*disable calls
			// to allow messages to be sent to a service, we just ignore this
			// request and instead wait for the real message to enable a service
			// later on.
			return OpStatus::OK;
		}
	}
	else
	{
		if (message.TransportType() != OpScopeTPMessage::STP_Call)
		{
			OpScopeTPError error;
			error.SetStatus(OpScopeTPHeader::BadRequest);
			error.SetStaticDescription(UNI_L("Host can only receive STP messages with Type = Call"));
			return EnqueueError(message, error);
		}
	}

	OpAutoPtr<OpScopeTPMessage> message_copy(OP_NEW(OpScopeTPMessage, ()));
	RETURN_OOM_IF_NULL(message_copy.get());
	RETURN_IF_ERROR(message_copy->Copy(message));

	OP_STATUS status = GetHost()->Receive(this, message_copy);
	return status;
}

/*virtual*/
void
OpScopeNetworkClient::OnSocketDataReady(OpSocket* socket)
{
	// Implemented to ease debugging of network client, can place breakpoints here
	OpScopeNetwork::OnSocketDataReady(socket);
}

/*virtual*/
void
OpScopeNetworkClient::OnSocketDataSent(OpSocket *s, unsigned int bytes_sent)
{
	if (state == State_HandshakeResponse)
	{
		OP_ASSERT(!OpScopeTPReader::IsEnabled());
		OP_ASSERT(!OpScopeTPWriter::IsEnabled());
		OP_ASSERT((int)bytes_sent <= handshake_remaining);

		handshake_remaining -= bytes_sent;
		if (handshake_remaining == 0)
		{
			// Handshake is finished, continue with normal STP/1 message processing
			OpStatus::Ignore(SwitchState(State_Normal));
		}
	}
	else // State_Normal | State_SendServiceList | State_Handshake | State_HandshakeRequest
	{
		OpScopeNetwork::OnSocketDataSent(s, bytes_sent);
	}
}

/*virtual*/
void
OpScopeNetworkClient::OnError(OpScopeTPReader::Error)
{
	// Do nothing for now
}

OP_STATUS
OpScopeNetworkClient::SendSTPHeader()
{
	OP_ASSERT(state == State_HandshakeResponse);
	handshake_remaining = 6;
	RETURN_IF_ERROR(GetSocket()->Send("STP/1\n", 6));
	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeNetworkClient::OnMessageSent(const OpScopeTPMessage &message)
{
	// Change state when service-list message is sent
	if (state == State_SendServiceList &&
		(message.ServiceName().Compare(UNI_L("*services")) == 0 ||
		(message.ServiceName().Compare(UNI_L("scope")) == 0 && message.CommandID() == OpScopeProtocolService::Command_OnServices)))
	{
		// Prepare for handshake, the client should send "*enable stp-1" to initiate
		RETURN_IF_ERROR(SwitchState(State_Handshake));
	}
	return OpStatus::OK;
}

/* virtual */
OP_STATUS
OpScopeNetworkClient::InitializeConnection()
{
#ifdef OPSOCKET_OPTIONS
	// Since scope often sends packets smaller than the TCP send buffer size
	// we can improve performance (latency) by setting this socket option.
	RETURN_IF_ERROR(GetSocket()->SetSocketOption(OpSocket::SO_TCP_NODELAY, TRUE));
#endif // OPSOCKET_OPTIONS

	return SwitchState(State_SendServiceList);
}

OP_STATUS
OpScopeNetworkClient::SwitchState(State new_state)
{
	OP_STATUS status;
	switch (new_state)
	{
	case State_SendServiceList:
		state = new_state;

		// Disable the reader until the service list is fully sent
		// Process continues in OnMessageSent().
		OpScopeTPReader::SetEnabled(FALSE);
		OpScopeTPWriter::SetEnabled(TRUE);

		// NOTE: The actual service-list message will be sent by the host as soon as the client is attached
		return OpStatus::OK;

	case State_Handshake:
		// Prepare for handshake, the client should send "*enable stp-1" to initiate
		// Process continues in OnMessageParsed().
		state = new_state;

		// Try to parse next message in case we already have incoming data
		OpScopeTPReader::SetEnabled(TRUE);
		OpScopeTPWriter::SetEnabled(FALSE);
		status = OpScopeTPReader::ParseNextMessage();
		if (status == OpScopeTPReader::STPStatus::ERR_NOT_ENOUGH_DATA)
			status = OpStatus::OK;
		OP_ASSERT(OpStatus::IsSuccess(status));
		return status;

	case State_HandshakeResponse:
		state = new_state;

		OpScopeTPReader::SetEnabled(FALSE); // Turn off reader until the STP Version is sent
		OpScopeTPWriter::SetEnabled(FALSE); // Turn off writer until the STP Version is sent

		// Send the handshake response, process continues in OnSocketDataSent()
		return SendSTPHeader();

	case State_Normal:
		state = new_state;

		// STP Version is sent so turn on writer and reader
		OpScopeTPReader::SetEnabled(TRUE);
		OpScopeTPWriter::SetEnabled(TRUE);

		// Try to parse next message in case we already have incoming data
		status = OpScopeTPReader::ParseNextMessage();
		if (status == OpScopeTPReader::STPStatus::ERR_NOT_ENOUGH_DATA)
			status = OpStatus::OK;
		OP_ASSERT(OpStatus::IsSuccess(status));
		return status;

	default:
		OP_ASSERT(!"Invalid state, should not happen");
		return OpStatus::ERR;
	}
}

#endif // SCOPE_SUPPORT
