/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _BITTORRENT_SUPPORT_
#include "modules/util/gen_math.h"
#include "modules/util/str.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/account.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/util/OpLineParser.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/util/str/strutil.h"

#include "bt-util.h"
#include "bt-benode.h"
#include "bt-download.h"


//////////////////////////////////////////////////////////////////////
// construction/destruction

BENode::BENode() :
	m_type(beNull),
	m_value(NULL),
	m_size(0)
{
	BT_RESOURCE_ADD("BENode", this);
}

BENode::~BENode()
{
	Clear();
	BT_RESOURCE_REMOVE(this);
}

//////////////////////////////////////////////////////////////////////
// clear
void BENode::Clear()
{
	if(m_value != NULL)
	{
		if(m_type == beString)
		{
			OP_DELETEA((char *)m_value);
		}
		else if(m_type == beList)
		{
			BENode** node = (BENode**)m_value;
			for(; m_size-- ; node++)
			{
				OP_DELETE(*node);
			}
			OP_DELETEA((BENode**)m_value);
		}
		else if(m_type == beDict)
		{
			BENode** node = (BENode**)m_value;
			for(; m_size-- ; node++)
			{
				// Dictionaries are allocated in pairs (values, keys) and must be deleted as such.
				OP_DELETE(*node); // the increment can't be inside the MACRO 
				node++;           
				OP_DELETEA((byte *)*node);
			}
			OP_DELETEA((BENode**)m_value);
		}
	}

	m_type	= beNull;
	m_value	= NULL;
	m_size	= 0;
}

//////////////////////////////////////////////////////////////////////
// BENode add a child node

BENode* BENode::Add(byte *pKey, int nKey)
{
	switch (m_type)
	{
		case beNull:
			m_type = (pKey != NULL && nKey > 0) ? beDict : beList;
			m_value	= NULL;
			m_size = 0;
			break;
		case beList:
			OP_ASSERT(pKey == NULL && nKey == 0);
			if(pKey != NULL || nKey != 0)
			{
				return NULL;
			}
			break;
		case beDict:
			OP_ASSERT(pKey != NULL && nKey > 0);
			if(pKey == NULL || nKey == 0)
			{
				return NULL;
			}
			break;

		default:
			OP_ASSERT(FALSE);	// if you get this, then the type is wrong
			return NULL;
			break;
	}
	BENode* newnode = OP_NEW(BENode, ());
	if(NULL == newnode)
	{
		return NULL;
	}
	if(m_type == beList)
	{
		BENode** newlist = OP_NEWA(BENode *, (UINT32)m_size + 1);	// m_size holds the number of items in this list
		if(NULL != newlist)
		{
			if(m_value != NULL)
			{
				memcpy(newlist, m_value, sizeof(BENode*) * (UINT32)m_size);
				OP_DELETEA((BENode**)m_value);
			}
			newlist[m_size++] = newnode;
			m_value = newlist;
		}
	}
	else
	{
		// we have a dictionary
		BENode** newlist = OP_NEWA(BENode *, (UINT32)m_size * 2 + 2);
		if (newlist)
		{
			if(m_value != NULL)
			{
				memcpy(newlist, m_value, sizeof(BENode*) * 2 * (UINT32)m_size);
				OP_DELETEA((BENode**)m_value);
			}
			byte* pxKey = OP_NEWA(byte, nKey + 1);
			if (pxKey)
			{
				memcpy(pxKey, pKey, nKey);
				pxKey[nKey] = 0;

				newlist[m_size * 2]		= newnode;
				newlist[m_size * 2 + 1]	= (BENode *)pxKey;

				m_value = newlist;
				m_size++;
			}
		}
	}

	return newnode;
}

//////////////////////////////////////////////////////////////////////
// BENode find a child node

BENode* BENode::GetNode(const char * key)
{
	if(m_type != beDict || !key) return NULL;

	BENode** retnode = (BENode**)m_value;

	UINT32 node;
	for(node = (UINT32)m_size; node ; node--, retnode += 2)
	{
		if(strcmp(key, (char *)retnode[1]) == 0) return *retnode;
	}
	return NULL;
}

BENode* BENode::GetNode(const byte *pKey, int nKey)
{
	if(m_type != beDict || !pKey) return NULL;

	BENode** retnode = (BENode**)m_value;

	UINT32 node;
	for(node = (UINT32)m_size; node ; node--, retnode += 2)
	{
		if(memcmp(pKey, (byte *)retnode[1], nKey) == 0) return *retnode;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////
// BENode SHA1 computation

OP_STATUS BENode::GetSHA1(SHAStruct* pSHA1)
{
	OP_ASSERT(this != NULL);

	OpByteBuffer buffer;

	OP_STATUS ret;

	if((ret = Encode(&buffer)) != OpStatus::OK)
		return ret;

	BTSHA pSHA;

//	if((ret = ) != OpStatus::OK)
//		return ret;
	pSHA.Add(buffer.Buffer(), buffer.DataSize());

//	if((ret = pSHA.Finish()) != OpStatus::OK)
//		return ret;
	pSHA.Finish();

	pSHA.GetHash(pSHA1);

	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// BENode encoding

OP_STATUS BENode::Encode(OpByteBuffer* buffer, BOOL debug)
{
	OpString str;
	OP_STATUS ret;
	char szBuffer[64];

	OP_ASSERT(this != NULL);
	OP_ASSERT(buffer != NULL);

	if(NULL == buffer)
	{
		return OpStatus::ERR_NULL_POINTER;
	}
	if(m_type == beString)
	{
		sprintf(szBuffer, "%u:", (UINT32)m_size);
		if ((ret = buffer->Append((unsigned char *)szBuffer, strlen(szBuffer))) != OpStatus::OK ||
			(ret = buffer->Append((unsigned char *)m_value, (UINT32)m_size)) != OpStatus::OK)
		{
			return ret;
		}

		if(debug)
		{
			DEBUGTRACE(UNI_L("DBG: %s\n"), (unsigned char *)szBuffer);
			DEBUGTRACE(UNI_L("DBG: %s\n"), (unsigned char *)m_value);
		}
	}
	else if(m_type == beInt)
	{
		OpString8 temp_start;
		ANCHOR(OpString8, temp_start);
		RETURN_IF_ERROR(g_op_system_info->OpFileLengthToString((OpFileLength)m_size, &temp_start));

		sprintf(szBuffer, "i%se", temp_start.CStr());

		if ((ret = buffer->Append((unsigned char *)szBuffer, strlen(szBuffer))) != OpStatus::OK)
			return ret;

		if(debug)
		{
			DEBUGTRACE(UNI_L("DBG: %s\n"), (unsigned char *)szBuffer);
		}
	}
	else if(m_type == beList)
	{
		BENode** retnode = (BENode**)m_value;
		if (*retnode == NULL)
			return OpStatus::ERR_NULL_POINTER;

		if ((ret = buffer->Append((unsigned char *)"l", 1)) != OpStatus::OK)
			return ret;

		UINT32 item;
		for(item = 0 ; item < (UINT32)m_size; item++, retnode++)
		{
			if ((ret = (*retnode)->Encode(buffer, debug)) != OpStatus::OK)
				return ret;
		}
		if ((ret = buffer->Append((unsigned char *)"e", 1)) != OpStatus::OK)
			return ret;
	}
	else if(m_type == beDict)
	{
		BENode** retnode = (BENode**)m_value;
		if (*retnode == NULL)
			return OpStatus::ERR_NULL_POINTER;

		if ((ret = buffer->Append((unsigned char *)"d", 1)) != OpStatus::OK)
			return ret;

		UINT32 item;
		for(item = 0 ; item < m_size; item++, retnode += 2)
		{
			char * key = (char *)retnode[1];
			if (key == NULL)
				return OpStatus::ERR_NULL_POINTER;

			sprintf(szBuffer, "%i:", (int)strlen(key));
			if ((ret = buffer->Append((unsigned char *)szBuffer, strlen(szBuffer))) != OpStatus::OK ||
				(ret = buffer->Append((unsigned char *)key, strlen(key))) != OpStatus::OK)
			{
				return ret;
			}

			if(debug)
			{
				DEBUGTRACE(UNI_L("DBG: %s\n"), (unsigned char *)key);
				DEBUGTRACE(UNI_L("DBG: %s\n"), (unsigned char *)szBuffer);
			}

			if ((ret = (*retnode)->Encode(buffer, debug)) != OpStatus::OK)
				return ret;
		}
		if ((ret = buffer->Append((unsigned char *)"e", 1)) != OpStatus::OK)
			return ret;
	}
	else
	{
		OP_ASSERT(FALSE);
	}
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// BENode decoding

BENode* BENode::Decode(OpByteBuffer* buffer)
{
	OP_ASSERT(buffer != NULL);

	BENode* node	= OP_NEW(BENode, ());
	byte *pInput	= buffer->Buffer();
	UINT32 nInput	= buffer->DataSize();

	BOOL retval = node ? node->Decode(pInput, nInput) : FALSE;

	if(FALSE == retval)
	{
		OP_DELETE(node);
		node = NULL;
	}
	return node;
}

/*
Integers are encoded as follows: i<integer>e
The initial i and trailing e are beginning and ending delimiters.
Example i3e represents the integer "3".

Strings are encoded as follows: <string length>:<string data>
Note that there is no constant beginning delimiter, and no specific ending delimiter.
Example: 4:spam represents the string "spam".

Lists are encoded as follows: l<bencoded type>e
The initial l and trailing e are beginning and ending delimiters.
Lists may contain any bencoded type, including integers, strings, dictionaries, and other lists.
Example: l4:spam4:eggse represents the list of two strings: ["spam", "eggs"]

Dictionaries are encoded as follows: d<bencoded string><bencoded element>e
The initial d and trailing e are the beginning and ending delimiters.
Note that the keys must be bencoded strings. The values may be any bencoded type,
including integers, strings, lists, and other dictionaries. Keys must be strings
and appear in sorted order (sorted as raw strings, not alphanumerics).
Example: d3:cow3:moo4:spam4:eggse represents the dictionary { "cow" => "moo", "spam" => "eggs" }
Example: d4:spaml1:a1:bee represents the dictionary { "spam" => ["a", "b"] }
*/

#define INC(x) { pInput += (x); nInput -= (x); }

BOOL BENode::Decode(byte *&pInput, UINT32& nInput)
{
	OP_ASSERT(m_type == beNull);
	OP_ASSERT(pInput != NULL);

	if(nInput < 1 || pInput==NULL)
	{
		return FALSE;
	}
	if(*pInput == 'i')	// integer
	{
		INC(1);
		UINT32 seek;
		for(seek = 1 ; seek < 40 ; seek++)
		{
			if(seek >= nInput)
			{
				return FALSE;
			}
			if(pInput[seek] == 'e') break;
		}

		if(seek >= 40)
		{
			return FALSE;
		}

		pInput[seek] = 0;

		OpString8 temp_start;
		OpFileLength length;
		ANCHOR(OpString8, temp_start);
		OP_STATUS s = StrToOpFileLength((char *)pInput, &length);
		m_size = length;

		if(OpStatus::IsError(s))
		{
			return FALSE;
		}
		pInput[seek] = 'e';

		INC(seek + 1);
		m_type = beInt;
	}
	else if(*pInput == 'l')	// lists
	{
		m_type = beList;
		INC(1);

		while(TRUE)
		{
			if(nInput < 1)
			{
				return FALSE;
			}
			if(*pInput == 'e') break;
			BENode* node = Add();
			if (!node)
				return FALSE;

			if(!node->Decode(pInput, nInput)) //FIXME: OOM
			{
				return FALSE;
			}
		}

		INC(1);
	}
	else if(*pInput == 'd') // dictionary
	{
		m_type = beDict;
		INC(1);

		while(TRUE)
		{
			if(nInput < 1)
			{
				return FALSE;
			}
			if(*pInput == 'e') break;

			int len = DecodeLen(pInput, nInput);
/*
			if(len == 0)
			{
				OP_ASSERT(len != 0);
				break;
			}
*/
			byte * key = pInput;
			INC(len);

			BENode* node = Add(key, len);
			if (!node)
				return FALSE;

			if(!node->Decode(pInput, nInput))
			{
				return FALSE;
			}
		}

		INC(1);
	}
	else if(*pInput >= '0' && *pInput <= '9')	// string
	{
		m_type		= beString;
		m_size = DecodeLen(pInput, nInput);
		UINT32 value_size = (UINT32)m_size;
		m_value	= OP_NEWA(char, value_size + 1 );
		if (!m_value)
			return FALSE;

		memcpy(m_value, pInput, value_size);
		((byte *)m_value)[ value_size] = 0;

		INC(value_size);
	}
	else
	{
		return FALSE;
	}
	return TRUE;
}

UINT32 BENode::DecodeLen(byte*& pInput, UINT32& nInput)
{
	if(!pInput)
		return FALSE;

	UINT32 seek;
	for(seek = 1 ; seek < 32 ; seek++)
	{
		if(seek >= nInput)
		{
			return FALSE;
		}
		if(pInput[ seek ] == ':') break;
	}

	if(seek >= 32)
	{
		return FALSE;
	}
	unsigned long len = 0; // %lu requires correct type !

	pInput[ seek ] = 0;
	if(sscanf((char *)pInput, "%lu", &len) != 1)
	{
		return FALSE;
	}
	pInput[ seek ] = ':';
	INC(seek + 1);

	if(nInput < (UINT32)len)
	{
		return FALSE;
	}
	return len;
}

#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
