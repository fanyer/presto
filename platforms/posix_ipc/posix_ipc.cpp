/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/posix_ipc/posix_ipc.h"

IpcHandle::~IpcHandle()
{
	close(m_read_pipe);
	close(m_write_pipe);
}

OP_STATUS IpcHandle::Receive()
{
	static const ssize_t bufsize = 16384;
	OpAutoArray<char> buf (OP_NEWA(char, bufsize));
	if (!buf.get())
		return OpStatus::ERR_NO_MEMORY;

	bool more = true;

	while (more)
	{
		ssize_t bytes_read = read(m_read_pipe, buf.get(), bufsize);
		if (bytes_read == 0)
			// Pipe closed - treat as unexpected error
			return OpStatus::ERR;

		if (bytes_read == -1)
			return (errno == EINTR || errno == EAGAIN) ? OpStatus::OK : OpStatus::ERR;

		char* append = m_recv_buffer.GetAppendPtr(bytes_read);
		if (!append)
			return OpStatus::ERR_NO_MEMORY;

		op_memcpy(append, buf.get(), bytes_read);

		if (bytes_read < bufsize)
			more = false;
	}

	return OpStatus::OK;
};

bool IpcHandle::MessageAvailable()
{
	UINT32 sz;
	if (m_recv_buffer.Length() < sizeof(sz))
		return false;
	
	const char* buffer = m_recv_buffer.Data();
	if (!buffer)
		return false;

	op_memcpy(&sz, buffer, sizeof(sz));
	return (m_recv_buffer.Length() >= (sz + sizeof(sz)));
}

OP_STATUS IpcHandle::ReceiveMessage(OpTypedMessage*& msg)
{
	UINT32 sz;
	if (m_recv_buffer.Length() < sizeof(sz))
		return OpStatus::ERR;

	const char* buffer = m_recv_buffer.Data();
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	op_memcpy(&sz, buffer, sizeof(sz));

	OpData payload(m_recv_buffer, sizeof(sz), sz);
	if (!payload.Length())
		return OpStatus::ERR_NO_MEMORY;

	OpSerializedMessage smsg(payload);
	msg = OpSerializedMessage::Deserialize(&smsg);
	if (!msg)
		return OpStatus::ERR_NO_MEMORY;

	m_recv_buffer.Consume(sizeof(sz));
	m_recv_buffer.Consume(sz);

	if (op_getenv("OPERA_PLUGINWRAPPER_IPC"))
		fprintf(stderr, "[%d] %d.%d.%d --> %d.%d.%d (%s) RECV\n", getpid(), msg->GetSrc().cm, msg->GetSrc().co, msg->GetSrc().ch, msg->GetDst().cm, msg->GetDst().co, msg->GetDst().ch, msg->GetTypeString());

	return OpStatus::OK;
}

OP_STATUS IpcHandle::SendMessage(OpTypedMessage* message)
{
	if (op_getenv("OPERA_PLUGINWRAPPER_IPC"))
		fprintf(stderr, "[%d] %d.%d.%d --> %d.%d.%d (%s) SEND\n", getpid(), message->GetSrc().cm, message->GetSrc().co, message->GetSrc().ch, message->GetDst().cm, message->GetDst().co, message->GetDst().ch, message->GetTypeString());

	OpAutoPtr<OpSerializedMessage> msg (OpTypedMessage::Serialize(message));
	OP_DELETE(message);

	if (!msg.get())
		return OpStatus::ERR_NO_MEMORY;

	// Prepend IPC header and send it.
	UINT32 len = msg->data.Length();
	OpData ipcMsg;
	RETURN_IF_ERROR(ipcMsg.SetCopyData(reinterpret_cast<const char*>(&len), sizeof(len)));
	RETURN_IF_ERROR(ipcMsg.Append(msg->data));

	return m_send_queue.Append(ipcMsg);
}

OP_STATUS IpcHandle::Send()
{
	while (m_send_queue.Count() > 0)
	{
		OpData& msg = m_send_queue.First();
		const char* buffer = msg.Data();
		size_t length = msg.Length();
		if (!buffer)
			return OpStatus::ERR_NO_MEMORY;

		ssize_t bytes_written = write(m_write_pipe, buffer, length);
		if (bytes_written == -1)
			return (errno == EINTR || errno == EAGAIN) ? OpStatus::OK : OpStatus::ERR;

		msg.Consume(bytes_written);

		if (msg.IsEmpty())
			m_send_queue.PopFirst();
		else // We did not send everything, so don't try again until select() says it is OK.
			break;

	}
	return OpStatus::OK;
}


