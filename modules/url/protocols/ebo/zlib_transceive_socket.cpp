/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef _EMBEDDED_BYTEMOBILE

#include "modules/url/protocols/ebo/zlib_transceive_socket.h"
#include "modules/hardcore/mh/mh.h"

OP_STATUS
ZlibTransceiveSocket::Create(
		OpSocket** new_socket,
		OpSocket* wrapped_socket,
		OpSocketListener* listener,
		unsigned buffer_size,
		BOOL transfer_ownership)
{
	if (!new_socket || !wrapped_socket || !listener)
		return OpStatus::ERR;

	z_stream recv_stream;
	op_memset(&recv_stream, 0, sizeof(recv_stream));
	int err = inflateInit2(&recv_stream, 12);
	if (err != Z_OK)
		return err == Z_MEM_ERROR ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;
	
	z_stream send_stream;
	op_memset(&send_stream, 0, sizeof(send_stream));
	err	= deflateInit2(&send_stream, 1, Z_DEFLATED, 12, 4, Z_DEFAULT_STRATEGY);
	if (err != Z_OK)
		return err == Z_MEM_ERROR ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;

	char* buffer = OP_NEWA(char, buffer_size);
	if (!buffer)
		return OpStatus::ERR_NO_MEMORY;

	OpSocket* zts = OP_NEW(ZlibTransceiveSocket, (wrapped_socket, listener, buffer, buffer_size, recv_stream, send_stream, transfer_ownership));
	if (!zts) {
		OP_DELETEA(buffer);
		return OpStatus::ERR_NO_MEMORY;
	}

	*new_socket = zts;
	return OpStatus::OK;
}

ZlibTransceiveSocket::ZlibTransceiveSocket(
	  OpSocket* wrapped_socket,
	  OpSocketListener* listener,
	  char* buffer,
	  unsigned buffer_size,
	  z_stream& recv_stream,
	  z_stream& send_stream,
	  BOOL transfer_ownership
	)
	: m_socket(wrapped_socket),
	  m_listener(listener),
	  m_owns_socket(transfer_ownership),
	  m_recv_buffer(buffer),
	  m_recv_buffer_size(buffer_size),
	  m_recv_buffer_rest(buffer),
	  m_recv_buffer_rest_len(buffer_size),
	  m_recv_buffer_read_number(0),
	  m_recv_stream(recv_stream),
	  m_send_stream(send_stream),
	  m_socket_blocking(FALSE),
	  m_pending_closed(FALSE),
	  m_pending_receive_error(NETWORK_NO_ERROR),
	  m_pending_on_socket_data_ready(FALSE),
	  m_has_pending_data(FALSE),
	  m_pending_data(0)
{
	m_socket->SetListener(this);
}

ZlibTransceiveSocket::~ZlibTransceiveSocket()
{
    g_main_message_handler->UnsetCallBacks(this);

    if (m_owns_socket)
		OP_DELETE(m_socket);
	m_socket = NULL;

	if (m_recv_stream.state != NULL)
		inflateEnd(&m_recv_stream);
	if (m_send_stream.state != NULL)
		deflateEnd(&m_send_stream);

	OP_DELETEA(m_recv_buffer);
	m_recv_buffer = NULL;
}

#define ZTS_SECURE
#define ZTS_SECURE2

OP_STATUS
ZlibTransceiveSocket::Send(const void* data, UINT length    ZTS_SECURE)
{
	unsigned long compressed_filelength = (unsigned long)((length*1.001)+12);
	Bytef* compressed_filecontent = OP_NEWA(Bytef, compressed_filelength);

	if (!compressed_filecontent)
	{
    	m_listener->OnSocketSendError(this, OpSocket::CONNECTION_FAILED);
		return OpStatus::ERR_NO_MEMORY;
	}

	m_send_stream.next_in = (Bytef*)data;
	m_send_stream.avail_in = length;
	m_send_stream.next_out = compressed_filecontent;
	m_send_stream.avail_out = compressed_filelength;

	int err	= deflate(&m_send_stream, Z_SYNC_FLUSH);
	if (err != Z_OK) OP_ASSERT(0); // OP_ASSERT(err == Z_OK) warns: "unused variable: err"

	OP_STATUS result = m_socket->Send(compressed_filecontent, compressed_filelength - m_send_stream.avail_out    ZTS_SECURE2);
	OP_DELETEA(compressed_filecontent);
	return result;
}

OP_STATUS
ZlibTransceiveSocket::DoInflate(int& inflate_status)
{
	inflate_status = Z_OK;
	BOOL inflate_more = TRUE;
	while (m_recv_stream.avail_out != 0 && inflate_more)
	{
		inflate_status = inflate(&m_recv_stream, Z_SYNC_FLUSH);
		if (inflate_status != Z_OK && inflate_status != Z_STREAM_END && inflate_status != Z_BUF_ERROR)
		{
			m_listener->OnSocketReceiveError(this, OpSocket::CONNECTION_FAILED);
			return inflate_status == Z_MEM_ERROR ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;
		}
		if (inflate_status == Z_STREAM_END)
		{
			int err = inflateEnd(&m_recv_stream);
			if (err == Z_OK)
				err = inflateInit2(&m_recv_stream, 12);
			if (err	!= Z_OK)
			{
				m_listener->OnSocketReceiveError(this, OpSocket::CONNECTION_FAILED);
				return err == Z_MEM_ERROR ? OpStatus::ERR_NO_MEMORY : OpStatus::ERR;
			}
		}
		else
			inflate_more = FALSE;
	}

	return OpStatus::OK;
}

OP_STATUS
ZlibTransceiveSocket::Recv(void* buffer, UINT length, UINT* bytes_received    ZTS_SECURE)
{
	m_recv_buffer_read_number++;
	*bytes_received = 0;
	if (length == 0)
		return OpStatus::OK;

	unsigned bytesRecv = 0;
	m_socket_blocking = FALSE;
	OP_STATUS result = m_socket->Recv(m_recv_buffer_rest, m_recv_buffer_rest_len, &bytesRecv    ZTS_SECURE2);
	if (OpStatus::IsError(result) && !m_socket_blocking)
		return result;

	// PH: -1 used by unixsocket.cpp to indicate EWOULDBLOCK
	if ((int)bytesRecv == ((int)-1))
		bytesRecv = 0;

    //  m_recv_buffer            m_recv_buffer_rest
    //        |                          |
    //        V                          V
    //        +--------------------------+----------------------------------+--------------+
    //        |//////////////////////////|###############Recv###############|              |
    //        +--------------------------+----------------------------------+--------------+
    //        |                          |                                  |              |
    //        |<--------------------------- recv_buffer_size ----------------------------->|
    //        |<--remaining_uninflated-->|<------------ m_recv_buffer_rest_len ----------->|
    //        |                          |<---------- bytesRecv ----------->|              |
    //        |<----------------- m_recv_stream.avail_in ------------------>|              |

	unsigned remaining_uninflated = m_recv_buffer_size - m_recv_buffer_rest_len;
	// Avoid reading uninitialized memory. Nothing dangerous, but
    // some	fields,	such as	total_out, are updated by inflate.
    m_recv_stream.next_in = (Bytef*)m_recv_buffer;
    m_recv_stream.avail_in = remaining_uninflated + bytesRecv;
    m_recv_stream.next_out = (Bytef*)buffer;
    m_recv_stream.avail_out = length;
	m_recv_stream.total_out = 0;

	m_recv_buffer_rest_len -= bytesRecv;
	
	if (m_has_pending_data) {
		*m_recv_stream.next_out++ = m_pending_data;
		m_recv_stream.avail_out--;
		m_recv_stream.total_out++;
		m_has_pending_data = FALSE;
		m_pending_on_socket_data_ready = FALSE;
	}

	int inflate_status;
	RETURN_IF_ERROR(DoInflate(inflate_status));

	if (inflate_status == Z_OK && m_recv_stream.avail_out == 0)
	{
		m_recv_stream.next_out = &m_pending_data;
		m_recv_stream.avail_out = 1;

		RETURN_IF_ERROR(DoInflate(inflate_status));

		if (m_recv_stream.avail_out == 0)
		{
			m_recv_stream.total_out--;
			m_has_pending_data = TRUE;
			m_pending_on_socket_data_ready = TRUE;
		}
	}

	m_recv_buffer_rest_len += (char*)m_recv_stream.next_in - m_recv_buffer;
	remaining_uninflated = m_recv_buffer_size - m_recv_buffer_rest_len;
	op_memmove(m_recv_buffer, m_recv_stream.next_in, remaining_uninflated);
	m_recv_buffer_rest = m_recv_buffer + remaining_uninflated;
	*bytes_received = m_recv_stream.total_out;

	if (m_pending_on_socket_data_ready || m_pending_closed || m_pending_receive_error != NETWORK_NO_ERROR)
	{
		// We have callbacks that must be sent to m_listener, but we cannot do it right now
		g_main_message_handler->SetCallBack(this, MSG_ZLIB_TRANSCEIVE_SOCKET_CALLBACK, (MH_PARAM_1)this);
		g_main_message_handler->PostMessage(MSG_ZLIB_TRANSCEIVE_SOCKET_CALLBACK, (MH_PARAM_1)this, 0);
	}
	
	return OpStatus::OK;
}

void
ZlibTransceiveSocket::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    g_main_message_handler->UnsetCallBacks(this);

	if (m_pending_on_socket_data_ready)
	{
		m_pending_on_socket_data_ready = FALSE;
		m_listener->OnSocketDataReady(this);
	}
	if (m_pending_receive_error != NETWORK_NO_ERROR)
	{
		m_pending_receive_error = NETWORK_NO_ERROR;
		m_listener->OnSocketReceiveError(this, m_pending_receive_error);
	}
	if (m_pending_closed)
	{
		m_pending_closed = FALSE;
		m_listener->OnSocketClosed(this);
	}
}

void
ZlibTransceiveSocket::OnSocketClosed(OpSocket* socket)
{
	if (m_has_pending_data == 0)
		m_listener->OnSocketClosed(this);
	else
		m_pending_closed = TRUE;
}

void
ZlibTransceiveSocket::OnSocketReceiveError(OpSocket* socket, OpSocket::Error error)
{
    if (error == OpSocket::SOCKET_BLOCKING) {
    	m_socket_blocking = TRUE;
		return;
    }

	if (m_has_pending_data == 0)
		m_listener->OnSocketReceiveError(this, error);
	else
		m_pending_receive_error = error;
}

#endif // _EMBEDDED_BYTEMOBILE
