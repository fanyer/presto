/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

/* This source file is used to generate equest-response sequences for OpSocksSocket.ot;
 * It is not supposed to be compiled and linked into any opera executable.
 */
#include "core/pch.h"

#ifdef SOCKS_SUPPORT

#include "modules/socks/OpTracingSocket.h"
#include <stdio.h>

#define LOG_SOCKS_ACTIVITY
#ifdef  LOG_SOCKS_ACTIVITY

OP_STATUS
OpTracingSocket::Send(const void* data, UINT length)
{
	Trace("Send", data, length);
	return socket->Send(data, length);
}

OP_STATUS
OpTracingSocket::Recv(void* buffer, UINT length, UINT* bytes_received)
{
	OP_STATUS stat = socket->Recv(buffer, length, bytes_received);
	if (OpStatus::IsSuccess(stat))
		Trace("Recv", buffer, length);
	else
		Trace("Recv", "*** FAILURE ***");
	return stat;
}


static FILE* log_file = fopen("socket.trace.txt", "w");
static OpSocket*  last_traced_socket = NULL;
void
OpTracingSocket::Trace(const char* method, const void* data, UINT length)
{
	if (do_trace)
	{
		if (this != last_traced_socket)
		{
			last_traced_socket = this;
			fprintf(log_file, "SOCKET #%x\n", (unsigned)socket);
		}
		fprintf(log_file, "[%s] \"", method);
		const unsigned char* bytes = reinterpret_cast<const unsigned char*>(data);
		for (UINT i=0; i < length; ++i)
			if (0x20 <= bytes[i] && bytes[i] < 0x7f)
				fputc(bytes[i], log_file);
			else
				fprintf(log_file, "\\x%02x", bytes[i]);
		fprintf(log_file, "\"\n");
	}
}

#endif//LOG_SOCKS_ACTIVITY
#endif
