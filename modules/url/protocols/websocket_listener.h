/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Niklas Beischer <no@opera.com>, Erik Moller <emoller@opera.com>
**
*/

#ifndef WEBSOCKET_LISTENER_H
#define WEBSOCKET_LISTENER_H

class OpWebSocket;

class OpWebSocketListener
{
public:
	enum WS_Event
	{
		WSWrongSubprotocol,
		WSMissingSubprotocol,
		WSNonZeroSizedBinaryFrame,
		WSDenormalizedBinaryFrame,
		WSInvalidFrameType,
		WSInvalidUTF8,
		WSFrameTooLong,
		WSFailedToConnect,
		WSWrongResponseCode,
		WSHandshakeParsing,
		WSHeaderParsing,
		WSFrameParsing,
		WS407WithNoProxy,
		WS200WithNoProxy,
		WSProxyAuthFailed,
		WS403WithNoProxy,
		WS403WithProxy,
		WSHeaderMissing,
		WSHeaderMismatch,
		WSHeaderDuplicate
	};

	virtual ~OpWebSocketListener() {};

	/** @short Called when WebSocket connection is established.
	 *
	 * If this function returns any error code, the socket will close immediately.
	 *
	 * @return OK, ERR for generic, or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS OnSocketOpen(OpWebSocket *socket) = 0;

	/** @short Called when the WebSocket closing handshake has been initiated by the server.
	 *
	 * If this function returns any error code, the socket will immediately close.
	 *
	 * @return OK, ERR for generic, or ERR_NO_MEMORY.
	 */
	virtual OP_STATUS OnSocketClosing(OpWebSocket *socket) = 0;

	/** @short Called when WebSocket connection is closed.
	 *
	 * This callback notifies of completed shutdown as a result
	 * of a call to OpWebSocket::Close() as well as socket closed by
	 * remote end.
	 *
	 * @param wasClean TRUE if this was an orderly close.
	 * @param code If non-zero, the connection close code.
	 * @param reason If non-NULL, the connection close reason.
	 *               The zero-terminated string is UTF-8 encoded and owned
	 *               by the caller.
	 */
	virtual void OnSocketClosed(OpWebSocket *socket, BOOL wasClean, unsigned short code, const uni_char *reason) = 0;

	/** @short Called when WebSocket receives message from remote end.
	 *
	 * When receiving this callback use appropriate function on socket to read the message.
	 *
	 *  If is_file is TRUE socket->GetMessageFile() must be used to retrieve the
	 *  file path.
	 *
	 *  Otherwise, call ReadMessage() with UINT8 *buffer or char *buffer depending on is_binary
	 *  parameter.
	 *
	 * If this function returns any error code, the socket will immediately close.
	 *
	 * @param socket     The socket on which the message received.
	 * @param is_binary  If true this was a binary message
	 * @param is_file    If true this message was big and stored on disk
	 *
	 * return OK, ERR_NO_MEMORY, or generic ERR.
	 */
	virtual OP_STATUS OnSocketMessage(OpWebSocket *socket, BOOL is_binary, BOOL is_file) = 0;

	/** @short Called when WebSocket encounters an error.
	 */
	virtual void OnSocketError(OpWebSocket *socket, OP_STATUS error) = 0;

	/** @short Called when an event that perhaps should be logged occurs.
	 */
	virtual void OnSocketInfo(OpWebSocket *socket, WS_Event evt, const uni_char *str = NULL, const uni_char *str2 = NULL, const uni_char *str3 = NULL) = 0;
};

#endif // WEBSOCKET_LISTENER_H
