/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
#include "modules/util/gen_math.h"
#include "adjunct/m2/src/engine/engine.h"
//#include "irc-message.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
# include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/m2/src/util/misc.h"

#if defined(WIN32) && !defined(WIN_CE)
#include <sys/timeb.h>
#endif

#include "bt-util.h"
#include "bt-globals.h"
#include "bt-packet.h"

//////////////////////////////////////////////////////////////////////
// P2PPacket construction

P2PPacket::P2PPacket(PROTOCOLID protocol)
:	m_protocol(protocol),
	m_next(NULL),
	m_reference(0),
	m_buffer(NULL),
	m_buffersize(0),
	m_length(0),
	m_position(0),
	m_bigendian(TRUE)
{
	BT_RESOURCE_ADD("P2PPacket", this);
}

P2PPacket::~P2PPacket()
{
	OP_ASSERT(m_reference == 0);
	
	OP_DELETEA(m_buffer);

	BT_RESOURCE_REMOVE(this);
}

void P2PPacket::ReadString(OpString& output, UINT32 maximum)
{
	maximum = min(maximum, m_length - m_position);
	if(!maximum) return;

	char *input	= (char *)m_buffer + m_position;
	char *scan	= input;
	UINT32 length;

	for(length = 0; length < maximum ; length++)
	{
		m_position++;
		if(!*scan++) break;
	}
	output.Set(input, length);
	return;
}

void P2PPacket::WriteString(char * str, BOOL bNull)
{
	Write(str, strlen(str) + (bNull ? 1 : 0));
}

void P2PPacket::Reset()
{
	OP_ASSERT(m_reference == 0);

	m_next			= NULL;
	m_length		= 0;
	m_position		= 0;
	m_bigendian		= TRUE;
}


//////////////////////////////////////////////////////////////////////
// BTPacket construction

BTPacket::BTPacket()
: P2PPacket(PROTOCOL_BT),
	m_type(0)
{
}

BTPacket::~BTPacket()
{
}

BTPacket* BTPacket::New(byte type)
{
	BTPacket* packet = (BTPacket*)g_PacketPool->New();
	if (!packet)
		return NULL;

	packet->m_type = type;
	return packet;
}

void BTPacket::Delete()
{
	g_PacketPool->Delete(this);
}

//////////////////////////////////////////////////////////////////////
// BTPacket serialize

void BTPacket::ToBuffer(OpByteBuffer* buffer, BOOL append)
{
	OP_ASSERT(buffer != NULL);

	if(buffer == NULL)
	{
		return;
	}
	if(!append)
	{
		buffer->Empty();
	}

	if (m_type == BT_PACKET_KEEPALIVE)
	{
		UINT32 zero = 0;
		buffer->Append((byte *)&zero, 4);
	}
	else
	{
		UINT32 length;

#ifdef OPERA_BIG_ENDIAN
		length = m_length + 1;
#else
		length = SWAP_LONG(m_length + 1);
#endif
		buffer->Append((byte *)&length, 4);
		buffer->Append((byte *)&m_type, 1);
		buffer->Append((byte *)m_buffer, m_length);
	}
}

//////////////////////////////////////////////////////////////////////
// BTPacket deserialize

BTPacket* BTPacket::ReadBuffer(OpByteBuffer* buffer)
{
	OP_ASSERT(buffer != NULL);
	if (buffer->DataSize() < 4) return NULL;

	UINT32 length = *(UINT32 *)buffer->Buffer();
#ifndef OPERA_BIG_ENDIAN
	length = SWAP_LONG(length);
#endif
	if(buffer->DataSize() < 4 + length) return NULL;

	if(length == 0)
	{
		buffer->Remove(4);
		return BTPacket::New(BT_PACKET_KEEPALIVE);
	}
    OP_ASSERT(length > 0); // fatuous as it's unsigned and != 0.

	BTPacket* packet = BTPacket::New(buffer->Buffer()[4]);
	if (!packet)
		return NULL;

#ifdef _DEBUG
//	if(packet->m_type == BT_PACKET_EXTENSIONS)
//	{
//		OP_ASSERT(FALSE);
//	}
#endif
	if((packet->Write(buffer->Buffer() + 5, length - 1)) == OpStatus::ERR_PARSING_FAILED)
	{
		buffer->Empty();
		return NULL;
	}
	buffer->Remove(4 + length);
	return packet;
}

#ifdef _DEBUG
BOOL BTPacket::VerifyBuffer(OpByteBuffer* buffer)
{
	UINT32 offset = 0;
	OP_ASSERT(buffer != NULL);
	if (buffer->DataSize() < 4)
	{
		DEBUGTRACE_UP(UNI_L("** VERIFIY ERROR: buffer too small: %d\n"), buffer->DataSize());
		OP_ASSERT(FALSE);
		return FALSE;
	}

	while(offset < buffer->DataSize())
	{
		UINT32 length = *(UINT32 *)&buffer->Buffer()[offset];
#ifndef OPERA_BIG_ENDIAN
		length = SWAP_LONG(length);
#endif
		if((buffer->DataSize() - offset) < (4 + length))
		{
			DEBUGTRACE_UP(UNI_L("** VERIFIY ERROR: buffer too small for len: %d\n"), length);
			OP_ASSERT(FALSE);
			return FALSE;
		}
		if(length > 33000)
		{
			DEBUGTRACE_UP(UNI_L("** VERIFIY ERROR: length error: %d\n"), length);
			OP_ASSERT(FALSE);
			return FALSE;
		}
		offset += (length + BT_PIECE_HEADER_LEN);
	}
	return TRUE;
}

#endif
///////////////////////////////////////////////////////////////////////
/// Validate if a buffer contains a complete packet of type BT_PACKET_PIECE. If
/// the packet type is some other type, this method will return TRUE.
BOOL BTPacket::IsValidPiece(OpByteBuffer* buffer)
{
	OP_ASSERT(buffer != NULL);
	UINT32 buflen = buffer->DataSize();

	if(buflen < 5)
	{
		if (buflen < 4)
			return FALSE;

		UINT32 length = *(UINT32 *)buffer->Buffer();

		// no swapping necessary, 0 stays 0 anyway
		if(length == 0)
		{
			// it's a keep-alive packet
			return TRUE;
		}
		return FALSE;
	}

	BT_PIECE_HEADER* header = (BT_PIECE_HEADER *)(buffer->Buffer());

	switch(header->type)
	{
		case BT_PACKET_KEEPALIVE:
		case BT_PACKET_CHOKE:
		case BT_PACKET_UNCHOKE:
		case BT_PACKET_INTERESTED:
		case BT_PACKET_NOT_INTERESTED:
		case BT_PACKET_HAVE:
		case BT_PACKET_BITFIELD:
		case BT_PACKET_REQUEST:
		case BT_PACKET_CANCEL:
		case BT_PACKET_HANDSHAKE:
		case BT_PACKET_EXTENSIONS:
			return TRUE;
			break;

		case BT_PACKET_PIECE:	// we'll check this package type below
			break;

		default:
			return FALSE;	// we don't recognize this type, so we'll cache the data for now
			break;
	}
	UINT32 len;

#ifdef OPERA_BIG_ENDIAN
	len = header->length;
#else
	len = SWAP_LONG(header->length);
#endif

	if(len <= (buflen + BT_PIECE_HEADER_LEN))
	{
//		DEBUGTRACE(UNI_L("Piece %d bytes, "), len);
//		DEBUGTRACE(UNI_L("buffer %d bytes from "), buflen);

		return TRUE;
	}
	return FALSE;
}


//////////////////////////////////////////////////////////////////////
// construction

P2PPacketPool::P2PPacketPool()
:	m_free(NULL),
	m_freesize(0)
{
	BT_RESOURCE_ADD("P2PPacketPool", this);
}

P2PPacketPool::~P2PPacketPool()
{
	DEBUGTRACE_RES(UNI_L("*** PacketPool: %d pools undeleted\n"), m_pools.GetCount());
	Clear();

	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// clear

void P2PPacketPool::Clear()
{
	for(UINT32 index = 0; index < m_pools.GetCount(); index++)
	{
		P2PPacket* pool = (P2PPacket*)m_pools.Get(index);
		FreePoolImpl(pool);
	}
	m_pools.Clear();
	m_free = NULL;
	m_freesize = 0;
}

//////////////////////////////////////////////////////////////////////
// P2PPacketPool new pool setup

void P2PPacketPool::NewPool()
{
	P2PPacket* pool = NULL;
	int pitch = 0, size = 256;

	NewPoolImpl(size, pool, pitch);
	if (!pool)
		return;

	m_pools.Add(pool);

	byte* bytes = (byte *)pool;

	while(size-- > 0)
	{
		pool = (P2PPacket *)bytes;
		bytes += pitch;

		pool->m_next = m_free;
		m_free = pool;
		m_freesize++;
	}
}

#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
