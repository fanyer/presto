/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
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

#include "bt-util.h"
#include "bt-globals.h"

#if defined(_DEBUG) && defined (MSWIN)
uni_char g_tmp[2000];
#endif

void OutputDebugLogging(const uni_char *x, char *y)
{
	OpString str;

	str.Set(y);

	OutputDebugLogging(x, str.CStr());
}

void OutputDebugLogging(const uni_char *x, ...)
{
#if defined(_DEBUG) && defined (MSWIN)
	{
		uni_char *tmp = OP_NEWA(uni_char, 500);
		int num = 0;
		va_list ap;

		va_start( ap, x );

		if(tmp)
		{
			num = uni_vsnprintf( tmp, 499, x, ap );
			if(num != -1)
			{
				tmp[499] = '\0';
				OutputDebugString(tmp);
			}
			OP_DELETEA(tmp);
		}
		va_end(ap);
	}
#endif
#if defined(BT_FILELOGGING_ENABLED)
	{
		if(g_BTFileLogging->IsEnabled())
		{
			OpString8 tmp;
			tmp.AppendFormat(x, y);

			OpString str;

			str.Set(tmp);

			g_BTFileLogging->WriteLogEntry(str);
		}
	}
#endif
}

//********************************************************************
//
//	SHA wrapper code
//
//********************************************************************
/*
SSL_Hash_Pointer hash(digest_fields.digest);

hash->InitHash();
hash->CalculateHash(GetUser());
hash->CalculateHash(":");
hash->CalculateHash(GetRealm());
hash->CalculateHash(":");
if(password.CStr())
hash->CalculateHash(password.CStr());
*/
BTSHA::BTSHA()
{
	m_hash.Set(SSL_SHA);
	Reset();

	BT_RESOURCE_ADD("BTSHA", this);
}

BTSHA::~BTSHA()
{
	BT_RESOURCE_REMOVE(this);
}

void BTSHA::Reset()
{
	m_hash->InitHash();
}

void BTSHA::GetHash(SHAStruct* pHash)
{
	memcpy(pHash->b, m_md, SHA_DIGEST_LENGTH);

#ifdef _DEBUG
	// test stuff
/*
	SHAStruct hash;

	Reset();

	Add("abc", 3);
	Finish();

	memcpy(hash.b, m_md, SHA_DIGEST_LENGTH);

	OpString str;

	HashToHexString(&hash, str);
*/
#endif

/*
Test Vectors (from FIPS PUB 180-1)
"abc"
A9993E36 4706816A BA3E2571 7850C26C 9CD0D89D
"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
84983E44 1C3BD26E BAAE4AA1 F95129E5 E54670F1
*/
}

OP_STATUS BTSHA::Add(void *pData, UINT32 nLength)
{
	m_hash->CalculateHash((const byte *)pData, nLength);
	return OpStatus::OK;
}


OP_STATUS BTSHA::Finish()
{
	m_hash->ExtractHash(m_md);
	return OpStatus::OK;
}

//////////////////////////////////////////////////////////////////////
// get hash string (Base64)

void BTSHA::GetHashString(OpString& outstr)
{
	SHAStruct pHash;
	GetHash( &pHash );
	HashToString( &pHash, outstr);
}

//////////////////////////////////////////////////////////////////////
// convert hash to string (Base64)

void BTSHA::HashToString(const SHAStruct *pHashIn, OpString& outstr)
{
	static const char *pszBase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

	char hashbuffer[64];	// 32 + 1 is used
	char *hash = (char *)hashbuffer;

	byte * pHash = (byte *)pHashIn;
	int nShift = 7;

	for ( int nChar = 32 ; nChar ; nChar-- )
	{
		BYTE nBits = 0;

		for ( int nBit = 0 ; nBit < 5 ; nBit++ )
		{
			if ( nBit ) nBits <<= 1;
			nBits |= ( *pHash >> nShift ) & 1;

			if ( ! nShift-- )
			{
				nShift = 7;
				pHash++;
			}
		}

		*hash++ = pszBase64[ nBits ];
	}
	*hash = '\0';

	outstr.Append((char *)hashbuffer);
}

//////////////////////////////////////////////////////////////////////
// SHA convert hash to string (hex)

void BTSHA::HashToHexString(const SHAStruct* pHashIn, OpString& outstr)
{
	static const char *pszHex = "0123456789ABCDEF";
	byte *pHash = (byte *)pHashIn;
	OpString8 strHash;
	char *pszHash = strHash.Reserve( 40 );
	if (!pszHash)
		return;

	for ( int nByte = 0 ; nByte < 20 ; nByte++, pHash++ )
	{
		*pszHash++ = pszHex[ *pHash >> 4 ];
		*pszHash++ = pszHex[ *pHash & 15 ];
	}
	outstr.Append(strHash);
}

//////////////////////////////////////////////////////////////////////
// SHA parse hash from string (Base64)

BOOL BTSHA::HashFromString(char *pszHash, SHAStruct* pHashIn)
{
	if ( ! pszHash || strlen( pszHash ) < 32 ) return FALSE;  //Invalid hash

	if ( op_strnicmp(pszHash, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", 32 ) == 0 ) return FALSE; //Bad hash

	SHAStruct Hash;
	byte *pHash = (byte *)&Hash;
	UINT32 nBits	= 0;
	INT32 nCount	= 0;

	for ( INT32 nChars = 32 ; nChars-- ; pszHash++ )
	{
		if ( *pszHash >= 'A' && *pszHash <= 'Z' )
			nBits |= ( *pszHash - 'A' );
		else if ( *pszHash >= 'a' && *pszHash <= 'z' )
			nBits |= ( *pszHash - 'a' );
		else if ( *pszHash >= '2' && *pszHash <= '7' )
			nBits |= ( *pszHash - '2' + 26 );
		else
			return FALSE;

		nCount += 5;

		if ( nCount >= 8 )
		{
			*pHash++ = (byte)( nBits >> ( nCount - 8 ) );
			nCount -= 8;
		}

		nBits <<= 5;
	}

	*pHashIn = Hash;

	return TRUE;
}

BOOL BTSHA::IsNull(SHAStruct* pHash)
{
	SHAStruct Blank;

	memset( &Blank, 0, sizeof(SHAStruct) );

	if ( *pHash == Blank ) return TRUE;

	return FALSE;
}


#endif // _BITTORRENT_SUPPORT_

#endif //M2_SUPPORT
