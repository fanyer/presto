/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/


#include "core/pch.h"

#ifdef _MIME_SEC_ENABLED_

#include "modules/libssl/sslhead.h"
#include "modules/libssl/sslopt.h"
#include "modules/mime/mime_lib.h"

Hash_Item::Hash_Item(SSL_HashAlgorithmType _alg)
{
	count = 0;
	alg = _alg;
	hasher = NULL;
	
	OP_STATUS op_err = OpStatus::OK;
	hasher = g_ssl_api->CreateMessageDigest(_alg, op_err);
	OpStatus::Ignore(op_err);
}

Hash_Item::Hash_Item(SSL_HashAlgorithmType _alg, Hash_Item *org)
{
	count = 0;
	alg = (org ? org->alg : _alg);
	OP_ASSERT(_alg == alg);

	hasher = NULL;

	if(org && org->hasher)
	{
		hasher = org->hasher->Fork();
		count = org->count;
	}

	if(hasher == NULL)
	{
		OP_STATUS op_err = OpStatus::OK;
		hasher = g_ssl_api->CreateMessageDigest(_alg, op_err);
		OpStatus::Ignore(op_err);
	}
}

Hash_Item::~Hash_Item()
{
	OP_DELETE(hasher);
	hasher = NULL;
}

void Hash_Item::InitHash()
{
	count = 0;
	if(hasher)
		hasher->InitHash();
}

void Hash_Item::HashData(const unsigned char *buffer, unsigned long len)
{
	if(hasher && buffer && len)
	{
		DS_DEBUG_Start();

		DS_Debug_Write("Hash_Item::HashData", "Hashing data", buffer, len);

		hasher->CalculateHash(buffer, len);
		count += len;
	}
}

void Hash_Item::ExtractL(SSL_varvector32 &digest)
{
	if(hasher)
	{
		DS_DEBUG_Start();

		hasher->ExtractHash(digest);

		DS_Debug_Write("Hash_Item::ExtractL", "Extracted Hash", digest.GetDirect(), digest.GetLength());
	}
	else
		digest.Resize(0);

	if(digest.Error())
		DS_LEAVE(digest.GetOPStatus());
}

void Hash_Item::WriteDataL(const unsigned char *buffer, unsigned long len)
{
	DS_DEBUG_Start();

	DS_Debug_Write("Hash_Item::WriteDataL", "Writing data", buffer, len);
	HashData(buffer, len);
}

Hash_Head::Hash_Head()
{
	binary = FALSE;
}

Hash_Head::~Hash_Head()
{
	Clear();
}

void Hash_Head::CopyL(Hash_Head *src)
{
	if(!src)
		return;

	Clear();

	Hash_Item *item = src->First();

	while(item)
	{
		Hash_Item *digest = OP_NEW_L(Hash_Item, (item->alg, item));
		if(digest && digest->hasher)
			digest->Into(this);
		else
		{
			OP_DELETE(digest);
			DS_LEAVE(OpRecStatus::ERR_NO_MEMORY);
		}
		item = item->Suc();
	}
}

void Hash_Head::AddMethodL(SSL_HashAlgorithmType _alg)
{
	if(GetDigest(_alg) != NULL)
		return;

	Hash_Item *digest = OP_NEW_L(Hash_Item, (_alg));
	if(digest && digest->hasher)
		digest->Into(this);
	else
	{
		OP_DELETE(digest);
		DS_LEAVE(OpRecStatus::ERR_NO_MEMORY);
	}
}

Hash_Item *Hash_Head::GetDigest(SSL_HashAlgorithmType _alg)
{
	Hash_Item *digest = First();
	while(digest && digest->alg != _alg)
		digest = digest->Suc();

	return digest;
}


void Hash_Head::HashDataL(const unsigned char *buffer, unsigned long len)
{
	if(buffer == NULL || len == 0)
		return;

	if(binary)
	{
		HashDataStep(buffer,  len);
		return;
	}

	// Data is assumed to be dash escaped.

	uint32 pos = 0;

	while(pos < len)
	{
		uint32 j = ScanForEOL(buffer+pos, len -pos);

		unsigned long non_blank_len;
		const unsigned char *current_pos;
		unsigned char temp;

		non_blank_len = (j ? j-1 /* to remove LF */ : len-pos);
		current_pos = buffer +pos + non_blank_len;

		if(j && non_blank_len && *(current_pos -1) == '\r')
		{
			// do not include CRLF in search
			current_pos--;
			non_blank_len--;
		}

		while(non_blank_len >0)
		{
			current_pos--;
			temp = *(current_pos);
			if(temp != ' ' && temp != '\t')
				break;
			non_blank_len--;
		}

		if(non_blank_len != 0)
		{
			if(text_buffer.GetLength())
			{
				HashDataStep(text_buffer.GetDirect(), text_buffer.GetLength());
				text_buffer.Resize(0);
			}

			HashDataStep(buffer+pos, non_blank_len);
		}

		if(j == 0)
		{
			pos += non_blank_len;
			if(pos < len)
			{
				text_buffer.Append(buffer+pos , len-pos);
				if(text_buffer.Error())
					DS_LEAVE(text_buffer.GetOPStatus() == OpStatus::ERR_NO_MEMORY);
			}
			pos = len;
		}
		else
		{
			HashDataStep((unsigned char *) "\r\n", 2);
			pos += j;
		}
	}
}

void Hash_Head::HashDataStep(const unsigned char *buffer, unsigned long len)
{
	Hash_Item *item = First();

	while(item)
	{
		item->HashData(buffer, len);
		item = item->Suc();
	}
}

void Hash_Head::WriteDataL(const unsigned char *buffer, unsigned long len)
{
	HashDataL(buffer, len);
}

uint32 ScanForEOL(const unsigned char *buf, uint32 len)
{
	uint32 pos = 0;
	
	if(buf == NULL || len == 0)
		return 0;

	while(pos < len)
	{
		char temp = *(buf++);
		pos++;

		if(temp == '\r')
		{
			if(pos < len && *buf == '\n')
				pos ++;

			return pos;
		}
		else if(temp == '\n')
			return pos;
	}

	return 0;
}

#endif //# _MIME_SEC_ENABLED_
