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

#ifndef WEBSOCKET_H
#define WEBSOCKET_H

class WebSocketProtocol;
class OpWebSocketListener;
class MessageHandler;

class OpWebSocket
{
public:

	enum State { WS_INITIALIZING, WS_CONNECTING, WS_CONNECTED, WS_CLOSING, WS_CLOSED };

	/** @short Static creation method.
	 *
	 * @param socket    (IN/OUT) A handle to the allocated object. Must be ignored if OpStatus::OK is not returned
	 * @param listener  A handle to the listener object
	 * @param mh        A handle to the MessageHandler to use for internal communication
	 *
	 * @return OpStatus values ERR_NO_MEMORY in case of allocation failure and OK in case of success
	 */
	static OP_STATUS Create(OpWebSocket** socket, OpWebSocketListener* listener, MessageHandler* mh);


	/** @ Static destruction method.
	 *
	 * OpWebSocket objects should never be deleted directly but rather using this method. Internal members need
	 * to be destroyed asynchronously and will be garbage collected later.
	 */
	static void Destroy(OpWebSocket** socket);


	/** @short Returns the current state of the object.
	 *
	 * An OpWebSocket object will be in WS_INITIALIZING until its Open() method is called. After a successful
	 * handshake it will go from WS_CONNECTING to WS_CONNECTED. Otherwise it will go directly to WS_CLOSED.
	 *
	 * @return The state of the OpWebSocket object.
	 */
	virtual State GetState() { return m_state; }


	/** @short Opens a connection to the remote host.
	 *
	 * Once the connection is open a WebSocket handshake procedure will be initiated. OnSocketOpen() or OnSocketError()
	 * will be called on the OpWebSocketListener upon success or failure respectively.
	 *
	 * @param target        URL string describing the remote endpoint of the WebSocket. Must be
	 *                      describing an absolute URL. E.g. "ws://example.com/WebSocket_example"
	 * @param origin        URL object describing the origin of the script that created the WebSocket
	 * @param sub_protocols Vector of sub protocols to communicate to the WebSocket remote end.
	 *                      Each sub protocol must be a valid token, as defined by RFC 2616 and
	 *                      http://tools.ietf.org/html/rfc6455#section-11.5.
	 *
	 * @return      OpStatus values. ERR_NO_MEMORY if allocation failure, ERR_NO_ACCESS in case of security error
	 *              (e.g. port restrictions), ERR_PARSING_FAILED if URL components cannot be obtained or are
	 *              incorrect (e.g. wrong scheme), ERR_NOT_SUPPORTED if called in a state that is not WS_INITIALIZING
	 *              and ERR_NO_SUCH_RESOURCE if URL was valid but initial socket open failed,
	 *              and ERR in case of unspecified error.
	 */
	virtual OP_STATUS Open(const uni_char *url, const URL& origin, const OpVector<OpString> &sub_protocols) = 0;


	/** @short Initiates a graceful closing of the WebSocket.
	 *
	 * @param status If non-zero, the status code to include in the WebSocket Close
	 *               message.
	 * @param reason If non-NULL, the UTF-8 encoded reason string to append after
	 *               the status code.
	 *
	 * OpWebSocketListener::OnSocketClosed() will be called once the operation completes.
	 */
	virtual void Close(unsigned short status, const uni_char *reason) = 0;


	/** @short Sends a WebSocket message containing 16-bit data.
	 *
	 * This method currently assumes that the data in the send buffer is UTF-16 encoded
	 *
	 * @param data                  Pointer to a buffer containing the 16-bit data to send.
	 * @param length                The length of the data buffer.
	 * @param buffered_amount [OUT] buffered_amount The amount of bytes the socket had to
	 *                              buffer for this call to SendMessage. To get the total
	 *                              amount of buffered bytes for all SendMessage operations
	 *                              call GetBufferedAmount.
	 *
	 * @return      OpStatus values ERR_NO_MEMORY if memory failure, ERR_NOT_SUPPORTED if called
	 *              while not in connected state and ERR in case of unspecified error.
	 */
	virtual OP_STATUS	SendMessage(const uni_char* data, OpFileLength length, OpFileLength &buffered_amount) = 0;

	/** @short Sends a WebSocket message containing byte data.
	 *
	 * This method sends the byte data as raw data.
	 *
	 * @param data                  Pointer to a buffer containing the byte data to send.
	 * @param length                The length of the data buffer.
	 * @param buffered_amount [OUT] buffered_amount The amount of bytes the socket had to
	 *                              buffer for this call to SendMessage. To get the total
	 *                              amount of buffered bytes for all SendMessage operations
	 *                              call GetBufferedAmount.
	 *
	 * @return      OpStatus values ERR_NO_MEMORY if memory failure, ERR_NOT_SUPPORTED if called
	 *              while not in connected state and ERR in case of unspecified error.
	 */
	virtual OP_STATUS	SendMessage(const UINT8 *data, OpFileLength length, OpFileLength &buffered_amount) = 0;

	/** @short Sends a WebSocket message from a file.
	 *
	 * This method sends the byte data as raw data.
	 *
	 * @param filepath              Full path and filename.
	 * @param is_binary             FALSE if it contains utf8 text, TRUE otherwise.
	 * @param buffered_amount [OUT] buffered_amount The amount of bytes the socket had to
	 *                              buffer for this call to SendFile. To get the total
	 *                              amount of buffered bytes for all SendMessage operations
	 *                              call GetBufferedAmount.
	 *
	 * @return      OpStatus values ERR_NO_MEMORY if memory failure, ERR_NOT_SUPPORTED if called
	 *              while not in connected state and ERR in case of unspecified error.
	 */
	virtual OP_STATUS   SendFile(const uni_char *filepath, BOOL is_binary, OpFileLength &buffered_amount) = 0;

	/** @short Reads data from the OpWebSocket one message at a time
	*
	* The buffer passed to this function should be large enough to contain the entire message to be read.
	* If the size is less than the message length then the OpWebSocket expects the next call to be a new
	* call to the same function.
	*
	* @param buffer     [OUT] The 16-bit buffer to write received data into. Must be ignored by the caller
	*                         unless OpStatus::OK is returned.
	* @param length           Maximum number of bytes to read.
	* @param read_len   [OUT] Set to the number of characters actually read. Must be ignored by the caller
	*                         unless OpStatus::OK is returned.
	*
	* @return       OpStatus values ERR_NO_MEMORY if memory failure, ERR_NOT_SUPPORTED if called
	*               while not in connected state and ERR in case of unspecified error.
	*/
	virtual OP_STATUS   ReadMessage(uni_char* buffer, OpFileLength buffer_length, OpFileLength& read_len) = 0;

	/** @short Reads binary data from the OpWebSocket.
	*
	* The buffer passed to this function should be large enough to contain the entire message to be read.
	* If the size is less than the message length then the OpWebSocket expects the next call to be a new
	* call to the same function.
	*
	* @param buffer           The byte buffer to write received data into. Must be ignored by the caller
	*                         unless OpStatus::OK is returned.
	* @param length           Maximum number of bytes to read.
	* @param read_len   [OUT] Set to the number of characters actually read. Must be ignored by the caller
	*                         unless OpStatus::OK is returned.
	*
	* @return       OpStatus values ERR_NO_MEMORY if memory failure, ERR_NOT_SUPPORTED if called
	*               while not in connected state and ERR in case of unspecified error.
	*/
	virtual OP_STATUS   ReadMessage(UINT8* buffer, OpFileLength buffer_length, OpFileLength& read_len) = 0;

	/** @short Gets the message file from the OpWebSocket.
	*
	* Must only be called if the last OnSocketMessage was received with is_file == TRUE.
	*
	* If OnSocketMessage was called with is_binary == FALSE, the message is in utf8 format.
	*
	* Note that the caller of this function takes owner ship of the file. If the file
	* is not deleted when closed, it will leak and fill up the disk.
	*
	* @param fulle_path [OUT] The full path of the file.
	*
	* @return       OpStatus values ERR_NO_MEMORY if memory failure, ERR_NOT_SUPPORTED if called
	*               while not in connected state and ERR in case of unspecified error.
	*/
	virtual OP_STATUS   GetMessageFilePath(OpString &fulle_path) = 0;


	/** @short Check if there are more messages ready.
	 *
	 * @return  TRUE if there are more messages ready to collect with ReadMessage.
	 */
	virtual BOOL        HasMessages() = 0;

	/** @short Return the number of bytes in the least recently received message.
	 *
	 * @return  The number of bytes needed to read the entire raw message.
	 */
	virtual OpFileLength        GetReceivedMessageBytes() = 0;

	/** @short Return the resolved URL.
	*
	* @return  OpStatus values ERR_NO_MEMORY if memory failure.
	*/
	virtual OP_STATUS   GetURL(OpString &url) = 0;

	/** @short Return the currently number of buffered bytes.
	*
	* @return  number of buffered bytes.
	*/
	virtual OpFileLength GetBufferedAmount() = 0;

	/** @short Return the selected sub protocol.
	*
	* @return The sub protocol negotiated by client and server. Can be NULL.
	*/
	virtual const uni_char *GetNegotiatedSubProtocol() = 0;

	/** @short Close the socket without attempting a graceful close.
	*/
	virtual void        CloseHard() = 0;

	/** @short Set any cookies received in the headers.
	*/
	virtual void        SetCookies() = 0;

	/** @short Set the auto proxy string.
	*/
	virtual OP_STATUS   SetAutoProxy(const uni_char *str) { return m_auto_proxy.Set(str); }
protected:

	OpString m_auto_proxy;
	State m_state;
	OpWebSocketListener* m_listener;

	OpWebSocket(OpWebSocketListener* listener)
		: m_state(WS_INITIALIZING), m_listener(listener)
	{ }

	virtual ~OpWebSocket() { }

};

#endif // WEBSOCKET_H
