/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_HARDCORE_LOGGING_OPSOCKETMESSAGELOGGER_H
#define MODULES_HARDCORE_LOGGING_OPSOCKETMESSAGELOGGER_H

#ifdef HC_MESSAGE_LOGGING
#include "modules/hardcore/logging/OpMessageLogger.h"
#include "modules/util/OpSharedPtr.h"
#include "modules/pi/network/OpSocket.h"
#include "modules/opdata/OpStringStream.h"

class OpLogFormatter;

/** An OpMessageLogger that sends logs to a remote host.
 *
 * An implementation of OpMessageLogger that connects to a remote host and
 * sends the logs there.
 *
 * If there's an error during sending, the Logger will disable itself and refuse
 * to send any further messages.
 *
 * @note For an example implementation of a remote logger, see
 * modules/hardcore/tools/simple-remote-logger.py.
 */
class OpSocketMessageLogger : public OpMessageLogger, public OpSocketListener
{
public:
	/** Construct an OpSocketMessageLogger.
	 *
	 * @param logFormatter The log formatter to use for preparing messages.
	 */
	OpSocketMessageLogger(OpSharedPtr<OpLogFormatter> logFormatter);
	~OpSocketMessageLogger();

	/** Set the socket that this logger will use.
	 *
	 * Closes the curent socket, if present. Does not attempt to Connect with the new one.
	 * @param socket New socket.
	 */
	void SetSocket(OpSharedPtr<OpSocket> socket);

	/** Connect to the remote logger.
	 *
	 * Attempts to connect with the remote logger. It is not guaranteed that logs
	 * will reach the remote host immediately after this method returns as sockets
	 * are asynchronous. The logs issued before the logger actually connects are
	 * lost.
	 *
	 * The socket must be set prior to calling this method.
	 *
	 * @param remoteLoggerAddress Address of the remote host.
	 * @return Status of the operation. If there's an immediate error,
	 * like an OOM, an error status will be returned. However, a successful
	 * status doesn't indicate that the connection is established. ERR_NO_SUCH_RESOURCE
	 * is returned when the socket is not set. @see SetSocket().
	 */
	OP_STATUS Connect(OpSocketAddress& remoteLoggerAddress);

	virtual void BeforeDispatch(const OpTypedMessage* msg);
	virtual void AfterDispatch(const OpTypedMessage* msg);
	virtual void Log(const OpTypedMessage* msg, const uni_char* action);
	virtual bool IsAbleToLog() const;

	virtual void OnSocketConnected(OpSocket* socket);
	virtual void OnSocketDataReady(OpSocket* socket);
	virtual void OnSocketDataSent(OpSocket* socket, UINT bytes_sent);
	virtual void OnSocketClosed(OpSocket* socket);
	virtual void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketSendError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);
	virtual void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent);

	// Unused, the logger is never a listening socket
	virtual void OnSocketConnectionRequest(OpSocket*) {}
	virtual void OnSocketListenError(OpSocket*, OpSocket::Error) {}

	/** Creates an OpSocketMessageLogger and prepare it for use.
	 *
	 * Creates an OpSocketMessageLogger and lets it connect to the specified
	 * remote host. If connection cannot be established or any other error occurs,
	 * returns an empty pointer.
	 *
	 * @param address IP address of the remote logging host.
	 * @param port Port of the remote logging host.
	 * @return A ready to use OpSocketMessageLogger or an empty pointer if something
	 * went wrong.
	 */
	static OpSharedPtr<OpSocketMessageLogger> Create(const uni_char* address, UINT port);

private:
	enum Status {
		Disconnected,
		Connecting,
		Connected,
		Error} m_status;

	bool ErrorOccured() const;

	void CheckOpStatus(OP_STATUS status);
	void Send(OpUniStringStream& ss);

	OpSharedPtr<OpLogFormatter> m_logFormatter;
	OpSharedPtr<OpSocket> m_socket;
};

#endif // HC_MESSAGE_LOGGING
#endif // MODULES_HARDCORE_LOGGING_OPSOCKETMESSAGELOGGER_H
