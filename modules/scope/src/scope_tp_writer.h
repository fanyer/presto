/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_SCOPE_TP_WRITER_H
#define OP_SCOPE_TP_WRITER_H

#include "modules/util/adt/bytebuffer.h"

class OpScopeTPMessage;

class OpScopeTPWriter
{
public:
	OpScopeTPWriter(BOOL enabled = TRUE, unsigned int version = 0);
	virtual ~OpScopeTPWriter();

	OP_STATUS Reset();
	/**<
	* Resets the incoming and outgoing message queue and the currently
	* processed message (if any).
	*/

	OP_STATUS EnqueueMessage(OpAutoPtr<OpScopeTPMessage> &msg);
	/**<
	* Enqueues a new message to be sent using the scope protocol.
	*
	* If the writer is enabled it will start processing all message
	* in the queue.
	*
	* @note The writer object takes ownership of the transport message, @a msg will be released.
	*/

	unsigned int ProtocolVersion() const { return version; }
	void SetProtocolVersion(unsigned int ver);

	OP_STATUS SetEnabled(BOOL enable);
	/**<
	 * Sets whether the writer is enabled or not.
	 * If it is enabled then any messages in the queue will be processed
	 * and data sent to SendData().
	 * Otherwise messages are left in the queue and new message
	 * will be appended to it but not processed.
	 *
	 * @param enable TRUE to enable, otherwise it is disabled
	 */

	BOOL IsEnabled() const { return enabled; }
	/**
	 * Returns whether the writer is enabled or not.
	 * @see SetEnabled()
	 */

protected: // For subclasses to implement
	virtual OP_STATUS SendData(const char *data, size_t len) = 0;
	/**<
	* Called when there is data ready to be sent.
	* Sub-classes must implement this method to sent the
	* data using which ever method is desired.
	*
	* The sub-class does not have to guarantee that all of the data
	* is sent, in which case the encoder will try to resend the
	* part of the data which was not sent.
	* When a piece of the data has been sent the sub-class must
	* call the OnDataSent() method to report how many bytes
	* were actually sent.
	*
	* @param data The bytes to be sent. There is no need to copy this data
	*             as it will not change or be destroyed until all data is sent.
	* @param len  The number of bytes in @a data.
	*/

	virtual OP_STATUS OnMessageDequeued(const OpScopeTPMessage &message) { return OpStatus::OK; }
	/**<
	* Called when a message is taken out of the queue and is going to be processed
	* and sent.
	*/

	virtual OP_STATUS OnMessageEncoded(const OpScopeTPMessage &message) { return OpStatus::OK; }
	/**<
	* Called when the message is being encoded and before the message is to be
	* sent.
	*/

	virtual OP_STATUS OnMessageSent(const OpScopeTPMessage &message) = 0;
	/**<
	* Called when the message has been succesfully sent.
	* This can be used to terminate a connection, close a file
	* or actions to perform inbetween each message.
	*/

protected: // For subclasses to call
	OP_STATUS OnDataSent(size_t len);
	/**<
	* Tells the encoder that a certain amount of data has been
	* sent. The encoder can then send the remaining data
	* or prepare the next data set.
	*
	* @param len The number of bytes which were successfully sent.
	* @pre   bytes_sent: len > 0
	*/

private:
	OP_STATUS ClearOutgoingMessage();
	/**<
	* Removes the outgoing message and all state variables
	* related to encoding the outgoing message.
	*
	* This is usually done when the outgoing message is complete
	* and the next one should be processed.
	*/

	OP_STATUS ProcessOutgoingQueue();
	/**<
	* Process the current message in the outgoing queue. If there is
	* no current message it will get the first item in the FIFO queue
	* and make it the current message.
	*/

	OP_STATUS ProcessMessage();
	/**<
	* Process the currently selected message.
	* The message will be encoded according to the current version of the
	* STP protocol, that means either as STP/0 or STP/1.
	*
	* @note This method will be called multiple times for a single message
	*       until all data is processed.
	*/

	enum State
	{
		  State_Idle
		, State_Stp0
		, State_Header
		, State_HeaderData
		, State_Chunk
	};

	// Sending
	unsigned int      version;            ///< The current STP version in use
	OpScopeTPMessage *outgoing_message;   // Current message to be sent
	Head              message_queue;      // List of Messages to be sent
	ByteBuffer        header;
	size_t            bytes_remaining;   ///< Number of bytes remaining to be sent by network
	char              prefix[4+10];      //< Buffer for the prefix data, needs to hold "STP" + version (1 byte) + 64 bit varint (max 10 bytes) // ARRAY OK 2009-05-05 jhoff
	uni_char         *block;				///< Used when sending STP/0 data, contains the entire payload
	int               chunk_len;		  ///< Size of current chunk that is to be sent
	unsigned int      bytebuffer_chunk_idx; ///< The current index of the bytebuffer being sent out
	size_t            payload_length;     //< Number of bytes remaining in out data
	BOOL              enabled;            // TRUE if the writer is enabled and the queue is to be processed
	BOOL              deferred;           ///< TRUE if the writing process has been deferred to callbacks from the network
	State             state;              ///< The current state of the writer.
};


#endif // OP_SCOPE_TRANSPORT_SERIALIZER_H
