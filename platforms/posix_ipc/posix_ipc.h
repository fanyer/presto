/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UNIX_IPC_H
#define UNIX_IPC_H

#include "modules/otl/list.h"

class OpTypedMessage;

/** @brief A handle to the reading and writing pipes of a sub-process
 */
class IpcHandle
{
public:
	IpcHandle(int readfd, int writefd) : m_read_pipe(readfd), m_write_pipe(writefd) {}
	~IpcHandle();

	/**
	 * @return true if the send queue is not empty (i.e. we want to send data).
	 */
	inline bool WantSend() { return m_send_queue.Length() > 0; }

	/**
	 * Send a message to remote peer.
	 * @param msg Message to send. This function takes ownership of @ref msg.
	 */
	OP_STATUS SendMessage(OpTypedMessage* msg);

	/**
	 * Send queued data (queued with SendMessage).
	 * - only from mainloop when select() indicates that data can be sent.
	 * @return OpStatus::OK on success, or an error if an unrecoverable error has occured (socket error, memory error).
	 */
	OP_STATUS Send();

	/**
	 * Receive data.
	 * - only from mainloop when select() indicates that data can be read.
	 * @return OpStatus::OK on success, or an error if an unrecoverable error has occured (socket error or memory allocaton error).
	 */
	OP_STATUS Receive();

	/**
	 * Check if a complete message can be decoded.
	 * 
	 * Use ReceiveMessage to receive the message.
	 * @return true if a complete message can be received with ReceiveMessage.
	 */
	bool MessageAvailable();

	/**
	 * Receive a message.
	 *
	 * @note You should use MessageAvailable() to determine if a message can be received. If no messages
	 * are available this method will return OpStatus::ERR.
	 *
	 * @return OpStatus::OK on success or an apropriate error if unable to receive message.
	 */
	OP_STATUS ReceiveMessage(OpTypedMessage*& msg);

	int GetReadPipe() const { return m_read_pipe; }
	int GetWritePipe() const { return m_write_pipe; }

protected:
	int m_read_pipe;
	int m_write_pipe;
	OpData m_recv_buffer;
	OtlCountedList<OpData> m_send_queue;
};

#endif // UNIX_IPC_H
