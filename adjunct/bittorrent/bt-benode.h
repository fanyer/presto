// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef BT_BENODE_H
#define BT_BENODE_H

// ----------------------------------------------------

#include "adjunct/m2/src/util/buffer.h"

# include "modules/util/OpHashTable.h"
# include "modules/util/adt/opvector.h"

#include "bt-util.h"

//********************************************************************
//
//	BENode
//
//  Used to parse the bencoded ("be"-encoded) data of a .torrent file
//
//********************************************************************

class BENode
{
// Construction
public:
	BENode();
	~BENode();

	// different type of info that can be contained in a node
	enum BENodeType { beNull, beString, beInt, beList, beDict };

// Attributes
private:
	BENodeType	m_type;
	void*		m_value;
	UINT64		m_size;		// file size stored here

// Operations
public:
	void		Clear();
	BENode*		Add(byte *pKey, int nKey);
	BENode*		GetNode(const char* key);
	BENode*		GetNode(const byte *pKey, int nKey);
	OP_STATUS	GetSHA1(SHAStruct* pSHA1);
	OP_STATUS	Encode(OpByteBuffer* buffer, BOOL debug = FALSE);
	static BENode*	Decode(OpByteBuffer* buffer);
private:
	BOOL		Decode(byte *& pInput, UINT32& nInput);
	static UINT32 DecodeLen(byte*& pInput, UINT32& nInput);


// Inline
public:
	inline void SetType(BENodeType type)
	{
		OP_ASSERT(m_type == beNull);
		m_type = type;
	}
	inline bool IsType(BENodeType type)
	{
		if ( this == NULL ) return false;
		return m_type == type;
	}

	inline INT64 GetInt()
	{
		if ( m_type != beInt ) return 0;
		return m_size;
	}

	inline void SetInt(INT64 value)
	{
		Clear();
		m_type		= beInt;
		m_size		= value;
	}

	OP_STATUS GetString(OpString& str)
	{
		if ( m_type != beString )
		{
			str.Empty();
			return OpStatus::OK;
		}
		return str.Set((char *)m_value, m_size);
	}

	OP_STATUS GetStringFromUTF8(OpString& str)
	{
		if ( m_type != beString )
		{
			str.Empty();
			return OpStatus::OK;
		}
		const char * string_value = reinterpret_cast<char *>(m_value);
		return str.SetFromUTF8(string_value, m_size);
	}

	char * GetString()
	{
		if ( m_type != beString )
		{
			return NULL;
		}
		return (char *)m_value;
	}

    UINT32 GetStringLength()
    {
        if ( m_type != beString )
        {
            return 0;
        }
        return (UINT32)m_size;
    }

	inline OP_STATUS SetString(char * psz)
	{
		return SetString( psz, strlen(psz), TRUE );
	}

	inline OP_STATUS SetString(void * str, UINT32 length, BOOL bNull = FALSE)
	{
		Clear();
		m_type		= beString;
		m_size		= (UINT64)length;
		m_value		= OP_NEWA(byte, length + ( bNull ? 1 : 0 ));
		if (!m_value)
			return OpStatus::ERR_NO_MEMORY;

		memcpy( m_value, str, length + ( bNull ? 1 : 0 ) );

		return OpStatus::OK;
	}

	inline BENode* Add(char * key = NULL)
	{
		return Add( (byte *)key, key != NULL ? strlen( key ) : 0 );
	}

	inline int GetCount()
	{
		if ( m_type != beList && m_type != beDict ) return 0;
		return (INT32)m_size;
	}

	inline BENode* GetNode(INT32 item)
	{
		if ( m_type != beList && m_type != beDict ) return NULL;
		if ( item < 0 || item >= (INT32)m_size) return NULL;
		if ( m_type == beDict ) item *= 2;
		return *( (BENode**)m_value + item );
	}
};

#endif // BT_BENODE_H
