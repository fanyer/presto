/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef HC_MESSAGE_LOGGING

#include "modules/hardcore/logging/OpSocketMessageLogger.h"

#include "modules/hardcore/logging/OpLogFormatter.h"
#include "modules/stdlib/include/opera_stdlib.h"

// For ::Create
#include "modules/hardcore/logging/OpReadableLogFormatter.h"
#include "modules/url/url_connection_socket_wrapper.h"

OpSocketMessageLogger::OpSocketMessageLogger(OpSharedPtr<OpLogFormatter> logFormatter) :
	m_logFormatter(logFormatter)
{
	if(logFormatter.get() == NULL)
		m_status = Error;
}

OpSocketMessageLogger::~OpSocketMessageLogger()
{
	if(IsAbleToLog())
	{
		OpUniStringStream ss;
		m_logFormatter->FormatEndLogging(ss);
		Send(ss);
	}
}

void OpSocketMessageLogger::SetSocket(OpSharedPtr<OpSocket> socket)
{
	if(m_socket.get())
		m_socket->Close();
	m_socket = socket;
	if(m_socket.get())
		m_socket->SetListener(this);
}

OP_STATUS OpSocketMessageLogger::Connect(OpSocketAddress &remoteLoggerAddress)
{
	if(m_socket.get() == NULL) return OpStatus::ERR_NO_SUCH_RESOURCE;
	OP_STATUS status = m_socket->Connect(&remoteLoggerAddress);
	if(OpStatus::IsSuccess(status))
		m_status = Connecting;
	else
		m_status = Error;
	return status;
}

void OpSocketMessageLogger::Send(OpUniStringStream &ss)
{
	if(!IsAbleToLog()) return;
	CheckOpStatus(m_socket->Send(ss.Str(), ss.Size()));
}

void OpSocketMessageLogger::CheckOpStatus(OP_STATUS status)
{
	if(!OpStatus::IsSuccess(status))
		m_status = Error;
}

bool OpSocketMessageLogger::IsAbleToLog() const
{
	return (m_status == Connected) && (m_socket.get());
}

bool OpSocketMessageLogger::ErrorOccured() const
{
	return m_status == Error;
}

void OpSocketMessageLogger::BeforeDispatch(const OpTypedMessage* msg)
{
	if(!IsAbleToLog() || !msg) return;
	OpUniStringStream ss;
	CheckOpStatus(m_logFormatter->FormatStartedDispatching(ss, *msg));
	Send(ss);
}

void OpSocketMessageLogger::AfterDispatch(const OpTypedMessage* msg)
{
	if(!IsAbleToLog() || !msg) return;
	OpUniStringStream ss;
	CheckOpStatus(m_logFormatter->FormatFinishedDispatching(ss, *msg));
	Send(ss);
}
void OpSocketMessageLogger::Log(const OpTypedMessage* msg, const uni_char* action)
{
	if(!IsAbleToLog() || !msg) return;
	OpUniStringStream ss;
	CheckOpStatus(m_logFormatter->FormatLog(ss, *msg, action));
	Send(ss);
}

void OpSocketMessageLogger::OnSocketConnected(OpSocket*)
{
	// This means we can log, yay.
	m_status = Connected;

	OpUniStringStream ss;
	CheckOpStatus(m_logFormatter->FormatBeginLogging(ss));
	Send(ss);
}

void OpSocketMessageLogger::OnSocketDataReady(OpSocket* socket)
{
	/* We're not expecting any incoming data...?
	But we'll take it off the socket so it doesn't throttle. Grab
	some bytes from the socket. If it's not enough, this method will get
	called again. */
	const size_t bufferSize = 1024;
	char buffer[bufferSize]; /* ARRAY OK 2012-04-17 mpawlowski */
	UINT bytesReceived = 0;
	OpStatus::Ignore(socket->Recv(buffer, bufferSize, &bytesReceived));
}

void OpSocketMessageLogger::OnSocketDataSent(OpSocket*, UINT)
{
	// Not doing any explicit buffering, so we'll ignore this for now.
}

void OpSocketMessageLogger::OnSocketClosed(OpSocket*)
{
	m_status = Disconnected;
}

void OpSocketMessageLogger::OnSocketConnectError(OpSocket*, OpSocket::Error)
{
	/* No handling of unstable connections yet. If there's any error, set
	status to Error and disable logging entirely. */
	m_status = Error;
	/* Maybe reconnections will be attempted here in a future implementation */
}

void OpSocketMessageLogger::OnSocketReceiveError(OpSocket*, OpSocket::Error)
{
	// We weren't expecting to receive anything, so whatever.
}

void OpSocketMessageLogger::OnSocketSendError(OpSocket*, OpSocket::Error)
{
	m_status = Error;
}

void OpSocketMessageLogger::OnSocketCloseError(OpSocket*, OpSocket::Error)
{
	/* This will most likely happen during destruction, if the socket fails
	to close gracefully. Not much we can do about it at this point. */
	m_status = Error;
}

void OpSocketMessageLogger::OnSocketDataSendProgressUpdate(OpSocket*, UINT)
{
	// We don't really care.
}

OpSharedPtr<OpSocketMessageLogger> OpSocketMessageLogger::Create(const uni_char* address,
														   UINT port)
{
	OpSharedPtr<OpLogFormatter> logFormatter =
				make_shared<OpReadableLogFormatter>(g_component_manager);
	OpSharedPtr<OpSocketMessageLogger> logger =
			make_shared<OpSocketMessageLogger>(logFormatter);

	// Return empty pointer if there was an OOM
	if(!logFormatter.get() || !logger.get())
		return OpSharedPtr<OpSocketMessageLogger>();

	// Create socket
	OpSocket* socketRawPtr = NULL;
	OP_STATUS status = SocketWrapper::CreateTCPSocket(&socketRawPtr, logger.get(), 0);
	OpSharedPtr<OpSocket> socket(socketRawPtr);

	// Return empty pointer if there was an error
	if(OpStatus::IsError(status) && !socket.get())
		return OpSharedPtr<OpSocketMessageLogger>();
	logger->SetSocket(socket);

	// Create address
	OpSocketAddress* socketAddressRawPtr = NULL;
	status = OpSocketAddress::Create(&socketAddressRawPtr);
	OpSharedPtr<OpSocketAddress> socketAddress(socketAddressRawPtr);

	// Return empty pointer if there was an error
	if(OpStatus::IsError(status) ||
	   !socketAddress.get() ||
	   OpStatus::IsError(socketAddress->FromString(address)))
	{
		return OpSharedPtr<OpSocketMessageLogger>();
	}

	socketAddress->SetPort(port);

	// Connect, return empty pointer if there was an error
	if(OpStatus::IsError(logger->Connect(*socketAddress)))
		return OpSharedPtr<OpSocketMessageLogger>();

	return logger;
}

#endif // HC_MESSAGE_LOGGING
