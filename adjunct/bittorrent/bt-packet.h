// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_PACKET_H
#define BT_PACKET_H

// ----------------------------------------------------

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

class OpByteBuffer;
class P2PPacket;

#include "bt-globals.h"

typedef INT32	PROTOCOLID;

enum Protocols
{
	PROTOCOL_NULL = 0,
	PROTOCOL_BT = 1
}; 

//
// Tools
//

#define SWAP_SHORT(x) ((((x) & 0xFF00) >> 8) + (((x) & 0x00FF) << 8))
#define SWAP_LONG(x) ((((x) & 0xFF000000) >> 24) + (((x) & 0x00FF0000) >> 8) + (((x) & 0x0000FF00) << 8) + (((x) & 0x000000FF) << 24))
#define SWAP_64(x) ((SWAP_LONG((x) & 0xFFFFFFFF) << 32) | SWAP_LONG((x) >> 32))

#define PACKET_GROW			128
#define PACKET_BUF_SCHAR	127
#define PACKET_BUF_WCHAR	127

//
// P2PPacket
//

class P2PPacket
{
// Construction
protected:
	P2PPacket(PROTOCOLID nProtocol);
	virtual ~P2PPacket();

// Attributes
public:
	PROTOCOLID	m_protocol;
	P2PPacket*	m_next;
	UINT32		m_reference;
public:
	BYTE*		m_buffer;
	UINT32		m_buffersize;
	UINT32		m_length;
	UINT32		m_position;
	BOOL		m_bigendian;	// is the data we receive big endian?
	
	enum { seekStart, seekEnd };
	
// Static Buffers
protected:
	static char		m_szSCHAR[PACKET_BUF_SCHAR+1];
	static uni_char	m_szWCHAR[PACKET_BUF_WCHAR+1];
	
// Operations
public:
	virtual void	Reset();
	virtual void	ToBuffer(OpByteBuffer * buffer, BOOL append = FALSE)= 0;
public:
	void			Shorten(UINT32 length);
	virtual void	ReadString(OpString& output, UINT32 maximum = 0xFFFFFFFF);
	virtual void	WriteString(char *str, BOOL bNull = TRUE);
	byte*			WriteGetPointer(UINT32 length, UINT32 offset = 0xFFFFFFFF);
public:
//	virtual char*	GetType();
	void			ToHex(OpString& outbuf);
	void			ToASCII(OpString& outbuf);
public:

	// Inline Packet Operations
public:
	
	inline int GetRemaining()
	{
		return m_length - m_position;
	}
	
	inline void	Seek(INT32 position, int relative = seekStart)
	{
		switch(relative)
		{
			case seekStart:
				m_position = position;
				break;

			case seekEnd:
				if(m_position - position > 0)
				{
					m_position -= position;
				}
				break;
		}
	}

	inline void Read(void *pData, int length)
	{
		if (m_position + length > m_length)
		{
			// should prolly throw an exception here?
			return;
		}
		memcpy(pData, m_buffer + m_position, length);
		m_position += length;
	}
	
	inline byte ReadByte()
	{
		if (m_position >= m_length)
		{
			return 0;
		}
		return m_buffer[ m_position++ ];
	}
	
	inline byte PeekByte()
	{
		if (m_position >= m_length)
		{
			return 0;
		}
		return m_buffer[ m_position ];
	}
	
	inline WORD ReadShortLE()
	{
		if (m_position + 2 > m_length)
		{
			return 0;
		}
		WORD value = *(WORD *)(m_buffer + m_position);
		m_position += 2;
		return value;
	}
	
	inline WORD ReadShortBE()
	{
		if (m_position + 2 > m_length)
		{
			return 0;
		}
		WORD value = *(WORD*)(m_buffer + m_position);
		m_position += 2;
#ifdef OPERA_BIG_ENDIAN
		return m_bigendian ? value : SWAP_SHORT(value);
#else
		return m_bigendian ? SWAP_SHORT(value) : value;
#endif
	}
	
	inline UINT32 ReadLongLE()
	{
		if (m_position + 4 > m_length)
		{
			return 0;
		}
		UINT32 value = *(UINT32*)(m_buffer + m_position);
		m_position += 4;
		return value;
	}
	
	inline UINT32 ReadLongBE()
	{
		if (m_position + 4 > m_length)
		{
			return 0;
		}
		UINT32 value = *(UINT32*)(m_buffer + m_position);
		m_position += 4;
#ifdef OPERA_BIG_ENDIAN
		return m_bigendian ? value : SWAP_LONG(value);
#else
		return m_bigendian ? SWAP_LONG(value) : value;
#endif
	}
	
	inline UINT64 ReadInt64()
	{
		if (m_position + 8 > m_length)
		{
			return 0;
		}
		UINT64 value = *(UINT64*)(m_buffer + m_position);
		m_position += 8;
#ifdef OPERA_BIG_ENDIAN
		return m_bigendian ? value : SWAP_64(value);
#else
		return m_bigendian ? SWAP_64(value) : value;
#endif
	}
	
	inline OP_STATUS Ensure(int length)
	{
		if (m_length + length > m_buffersize)
		{
			UINT32 new_size = m_buffersize + max(length, PACKET_GROW);
			byte *pNew = OP_NEWA(byte, new_size);
			if (!pNew)
				return OpStatus::ERR_NO_MEMORY;

			m_buffersize = new_size;
			memcpy(pNew, m_buffer, m_length);
			OP_DELETEA(m_buffer);
			m_buffer = pNew;
		}
		return OpStatus::OK;
	}
	
	inline OP_STATUS Write(void *pData, int length)
	{
		if(length < 0)
		{
//			OP_ASSERT(FALSE);
			return OpStatus::ERR_PARSING_FAILED;
		}
		RETURN_IF_ERROR(Ensure(length));
		memcpy(m_buffer + m_length, pData, length);
		m_length += length;

		return OpStatus::OK;
	}
	
	inline void WriteByte(BYTE value)
	{
		RETURN_VOID_IF_ERROR(Ensure(sizeof(value)));
		m_buffer[ m_length++ ] = value;
	}
	
	inline void WriteShortLE(WORD value)
	{
		RETURN_VOID_IF_ERROR(Ensure(sizeof(value)));
		*(WORD*)(m_buffer + m_length) = value;
		m_length += sizeof(value);
	}
	
	inline void WriteShortBE(WORD value)
	{
		RETURN_VOID_IF_ERROR(Ensure(sizeof(value)));

#ifdef OPERA_BIG_ENDIAN
		*(WORD*)(m_buffer + m_length) = m_bigendian ? value : SWAP_SHORT(value);
#else
		*(WORD*)(m_buffer + m_length) = m_bigendian ? SWAP_SHORT(value) : value;
#endif
		m_length += sizeof(value);
	}
	
	inline void WriteLongLE(UINT32 value)
	{
		RETURN_VOID_IF_ERROR(Ensure(sizeof(value)));
		*(UINT32*)(m_buffer + m_length) = value;
		m_length += sizeof(value);
	}
	
	inline void WriteLongBE(UINT32 value)
	{
		RETURN_VOID_IF_ERROR(Ensure(sizeof(value)));

#ifdef OPERA_BIG_ENDIAN
		*(UINT32*)(m_buffer + m_length) = m_bigendian ? value : SWAP_LONG(value);
#else
		*(UINT32*)(m_buffer + m_length) = m_bigendian ? SWAP_LONG(value) : value;
#endif
		m_length += sizeof(value);
	}
	
	inline void WriteInt64(UINT64 value)
	{
		RETURN_VOID_IF_ERROR(Ensure(sizeof(value)));

#ifdef OPERA_BIG_ENDIAN
		*(UINT64*)(m_buffer + m_length) = m_bigendian ? value : SWAP_64(value);
#else
		*(UINT64*)(m_buffer + m_length) = m_bigendian ? SWAP_64(value) : value;
#endif
		m_length += sizeof(value);
	}
	
// Inline Allocation
public:
	
	inline void AddRef()
	{
		m_reference++;
	}
	
	inline void Release()
	{
		if(this)
		{
			OP_ASSERT(m_reference != 0);
			if (!--m_reference)
			{
				Delete();
			}
		}
	}
	
	inline void ReleaseChain()
	{
		if (this == NULL) return;
		
		for (P2PPacket* packet = this ; packet ;)
		{
			P2PPacket* next = packet->m_next;
			packet->Release();
			packet = next;
		}
	}
	
	virtual void Delete() = 0;
	
	friend class PacketPool;
};


class P2PPacketPool
{
// Construction
public:
	P2PPacketPool();
	virtual ~P2PPacketPool();

// Attributes
protected:
	P2PPacket*	m_free;
	UINT32		m_freesize;
protected:
	OpVector<P2PPacket>	m_pools;

// Operations
protected:
	void	Clear();
	void	NewPool();
protected:
	virtual void NewPoolImpl(int size, P2PPacket*& pool, INT32& pitch) = 0;
	virtual void FreePoolImpl(P2PPacket* pool) = 0;
	
// Inlines
public:
	inline P2PPacket* New()
	{
		if (m_freesize == 0)
		{
			NewPool();
		}
		if (m_freesize == 0)	// OOM
			return NULL;

		P2PPacket* packet = m_free;
		m_free = m_free->m_next;
		m_freesize--;

		packet->Reset();
		packet->AddRef();

		return packet;
	}
	
	inline void Delete(P2PPacket* packet)
	{
		OP_ASSERT(packet != NULL);
		OP_ASSERT(packet->m_reference == 0);
		
		packet->m_next = m_free;
		m_free = packet;
		m_freesize ++;
	}

};

// Bittorrent specific packet class

class BTPacket : public P2PPacket
{
// Construction
protected:
	BTPacket();
	virtual ~BTPacket();

// Attributes
public:
	byte	m_type;
	
// Operations
public:
	virtual	void		ToBuffer(OpByteBuffer* buffer, BOOL append = FALSE);
	static	BTPacket*	ReadBuffer(OpByteBuffer* buffer);
	static	BOOL		IsValidPiece(OpByteBuffer* buffer);

#ifdef _DEBUG
	static	BOOL		VerifyBuffer(OpByteBuffer* buffer);
#endif

public:
//	virtual char *		GetType() const;

// Allocation
public:
	static BTPacket* New(byte type);
	virtual void Delete();

	friend class BTPacketPool;
};

class BTPacketPool : public P2PPacketPool
{
public:
	virtual ~BTPacketPool() 
	{ 
		Clear(); 
	}

protected:

	void NewPoolImpl(int size, P2PPacket*& pool, int& pitch)
	{
		pitch	= sizeof(BTPacket);
		pool	= OP_NEWA(BTPacket, size);
	}
	void FreePoolImpl(P2PPacket* packet)
	{
		OP_DELETEA((BTPacket*)packet);
	}
};
	
//
// Packet Types
//

#define BT_PACKET_CHOKE				0
#define BT_PACKET_UNCHOKE			1
#define BT_PACKET_INTERESTED		2
#define BT_PACKET_NOT_INTERESTED	3
#define BT_PACKET_HAVE				4
#define BT_PACKET_BITFIELD			5
#define BT_PACKET_REQUEST			6
#define BT_PACKET_PIECE				7
#define BT_PACKET_CANCEL			8

#define BT_PACKET_EXTENSIONS		20 // see http://www.rasterbar.com/products/libtorrent/extension_protocol.html

#define BT_PACKET_HANDSHAKE			128
#define BT_PACKET_SOURCE_REQUEST	129
#define BT_PACKET_SOURCE_RESPONSE	130

#define BT_PACKET_KEEPALIVE			255

#define BT_PROTOCOL_HEADER			"\023BitTorrent protocol"
#define BT_PROTOCOL_HEADER_LEN		19
#define BT_PIECE_HEADER_LEN			13

//#pragma pack(1)

typedef struct
{
	UINT32	length;		// length of the data + 9
	byte	type;		// type of packet
	UINT32	piece;		// block index
	UINT32	offset;		// index within the block

} BT_PIECE_HEADER;

typedef struct BTHandshakeHeader
{
	byte pstrlen;		// len of pstr, should be 19 bytes with Bittorrent
	char pstr[19];		// pstr, should be "BitTorrent protocol" for Bittorrent
	byte reserved[8];	// 8 reserved bytes
	byte info_hash[20];	// 20-byte SHA1 hash of the info key in the metainfo file. This is the same
						// info_hash that is transmitted in tracker requests
	byte peer_id[20];	// 20-byte string used as a unique ID for the client. This is the same peer_id
						// that is transmitted in tracker requests.

} BT_HANDSHAKE_HEADER;

//#pragma pack()


#endif // BT_PACKET_H

