/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
/** @file opcrypt_xp2.cpp
** General crypto functions while working on files.
**
** Intended to be cross-platform.  That's what XP stands for.  The file header's
** integer fields are stored in little-endian form with no padding.  Code
** reading or writing to the header must not assume that structs have no padding
** (despite the # pragma pack() below) and do need to deal with endianness.
**
** Depends on ANSI C functions like fopen(), fseek() and such.  When FILECLASS
** has the (platform-neutral) features needed by this code, it should be used
** here.
*/

#include "core/pch.h"

#ifdef _LOCAL_FILE_CRYPTO_

#ifdef OPCRYPT_ALTERNATIVE_IMPLEMENTATION
# include OPCRYPT_ALTERNATIVE_IMPLEMENTATION
#else

#define OPCF_ID1		"OPCF"

#include "modules/util/opcrypt_xp.h"
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/util/gen_str.h"

/* The only real export from opcrypt is OPC_Crypt, which doesn't need these
 * defines or the struct following them to be externally visible.  So I've moved
 * them into this source file.  If that causes problems (or you miss some of the
 * functions this file used to export), check out o72_work's non-module
 * util/opcrypt_xp.cpp (and its .h); grab the relevant chunks of them (which are
 * merely #if'd out) and add them back to this (and its .h).  Eddy/2003/July/30.
 */

#define OPCF_ID2		0x0d11a502L
#define OPCF_VERSION	2

#ifdef _MAX_EXT
# undef _MAX_EXT
#endif

#define _MAX_EXT 256
#ifndef __arm ///< Packing causes memory corruption on arm systems.
#ifndef __arm__
#pragma pack(1) // no longer really necessary, but can't hurt ;^>
#endif
#endif
typedef struct
{
	// NB: id2, version and crc16 are stored in files in little-endian order
	UINT32 id1;	//	== OPCF_ID1; actually a thinly-veiled *string*
	UINT32 id2;	//	== OPCF_ID2
	UINT32 version;	//	== OPCF_VERSION
	char extension[_MAX_EXT]; // File extension for the encoded file (ignored) // ARRAY OK 2009-04-24 johanh */
	BOOL compressed; // == FALSE
	UINT16 crc16; // 16 bits check sum for the decoded file
} OPCRYPT_FILE_HEADER;
#ifndef __arm
#ifndef __arm__
# pragma pack()
#endif
#endif

static UINT32 UpdateCRC16(void *buffer, int size, UINT32 seed)
{
	static const UINT16 crctab[256] =
	{
		0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
		0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
		0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
		0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
		0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
		0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
		0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
		0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
		0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
		0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
		0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
		0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
		0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
		0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
		0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
		0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
		0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
		0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
		0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
		0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
		0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
		0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
		0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
		0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
		0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
		0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
		0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
		0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
		0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
		0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
		0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
		0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
	};


	UINT8 *pb = (UINT8 *)buffer;

	for (int i=0; i<size; i++)
	{
		seed = (crctab[((seed>> 8) & 255)] ^ (seed<< 8) ^ *pb);
		++ pb;
	}

	return seed;
}

static BOOL GetFileCheckSum(const uni_char *filename, UINT16 *crc)
{
	*crc = 0;
	OpFile fp;
	if(OpStatus::IsError(fp.Construct(filename)) || OpStatus::IsError(fp.Open(OPFILE_READ)))
		return FALSE;

	UINT8 buffer[16384];
	do
	{
		OpFileLength nBytes = 0;
		if(OpStatus::IsError(fp.Read(buffer, sizeof(buffer), &nBytes)))
			break;
		if (nBytes > 0)
			*crc = UpdateCRC16(buffer, nBytes, *crc);

	} while (!fp.Eof());

	fp.Close();
	return TRUE;
}

/* Check validity of header, return checksum if requested */
static BOOL OPC_GoodHeader(const OPCRYPT_FILE_HEADER &header, UINT16 *checkSum)
{
	// id1 is really a string in disguise !
	if (op_strncmp( (char*)&header.id1, OPCF_ID1, 4)) return FALSE;
	UINT32 version;

#ifdef OPERA_BIG_ENDIAN
	/* Big Bad Legacy Bodge -- Eddy/2003/July/29.
	 *
	 * Some existing files have been written with id2, version and crc16 in
	 * big-endian format; rather than reject them as invalid, we notice them (by
	 * their id2) and don't swap their endianness.  Trying to read them on a
	 * little-endian box will still fail, but then it should, and always did.
	 */
	if (header.id2 == OPCF_ID2)
	{
		if (checkSum) *checkSum = header.crc16;
		version = header.version;
	}
	else
	{
		// check byte-swapped id2
		const unsigned char *tmp = (const unsigned char *)&(header.id2);
		if ((tmp[0] | tmp[1] << 8 | tmp[2] << 16 | tmp[3] << 24) != OPCF_ID2)
			return FALSE;

		// byte-swap version
		tmp = (const unsigned char *)&(header.version);
		version = tmp[0] | tmp[1] << 8 | tmp[2] << 16 | tmp[3] << 24;
		if (checkSum)
		{
			tmp = (const unsigned char *)&(header.crc16);
			*checkSum = tmp[0] | tmp[1] << 8;
		}
	}
#else

	if (header.id2 != OPCF_ID2) return FALSE;
	if (checkSum) *checkSum = header.crc16;
	version = header.version;
#endif // OPERA_BIG_ENDIAN

	return version >= 1 && version <= OPCF_VERSION;
}

static BOOL OPC_Crypt_gutsDecodeL(SSL_BulkCipherType cipher,
							SSL_secure_varvector32 &data,
							UINT16 &checkSum,
							const uni_char *filename_in, const uni_char *filename_out,
							BYTE *userKey, int keylen)
{
	//	stuff
	OpStackAutoPtr<SSL_GeneralCipher> crypt(g_ssl_api->CreateSymmetricCipherL(cipher));
	
	if(!crypt.get())
		return FALSE;

	{
		// Does not leave
		SSL_varvector32 usr_key;
		SSL_varvector32 salt;
		usr_key.SetExternal(userKey);
		usr_key.Resize(keylen);
	
		crypt->BytesToKey(SSL_MD5, usr_key, salt, 1);
		if(crypt->Error())
		{
			return FALSE;
		}
	}

	OpFile *file = OP_NEW(OpFile, ()); // LEAVE safe
	if(!file)
		return FALSE;

	if(OpStatus::IsError(file->Construct(filename_in))|| OpStatus::IsError(file->Open(OPFILE_READ)))
	{
		OP_DELETE(file);
		return FALSE;
	}

	DataStream_GenericFile infile(file);
	ANCHOR(DataStream_GenericFile, infile);

	infile.InitL();

#define OPC_READ(v) if (infile.ReadDataL((unsigned char*)&(v), sizeof(v)) != sizeof(v)) return FALSE
	OPCRYPT_FILE_HEADER opcFileHeader;
	OPC_READ(opcFileHeader.id1);
	OPC_READ(opcFileHeader.id2);
	OPC_READ(opcFileHeader.version);
	OPC_READ(opcFileHeader.extension); // ignored !
	OPC_READ(opcFileHeader.compressed); // ignored !
	OPC_READ(opcFileHeader.crc16);
#undef OPC_READ
	
	if (opcFileHeader.compressed) return FALSE; // unsupported
	if (!OPC_GoodHeader(opcFileHeader, &checkSum)) return FALSE; // bad id/version

	SSL_secure_varvector32 encrypted_data;
	ANCHOR(SSL_secure_varvector32, encrypted_data);
	
	encrypted_data.AddContentL(&infile);
	crypt->DecryptVector(encrypted_data, data);
	if(data.Error() || crypt->Error())
		return FALSE;

	checkSum = UpdateCRC16(data.GetDirect(), data.GetLength(), checkSum);

	if(filename_out && *filename_out)
	{
		OpFile *file = OP_NEW(OpFile, ()); // LEAVE safe
		if(!file)
			return FALSE;
	
		if(OpStatus::IsError(file->Construct(filename_out)) ||
			OpStatus::IsError(file->Open(OPFILE_WRITE)))
		{
			OP_DELETE(file);
			return FALSE;
		}
	
		DataStream_GenericFile outfile(file, TRUE);
		ANCHOR(DataStream_GenericFile, outfile);
	
		outfile.InitL();

		outfile.AddContentL(&data);

		outfile.Close();
	}

	return TRUE;
}

// Single extern exported by this file.
BOOL OPC_Crypt(const uni_char *filename_in, const uni_char *filename_out,
			   BYTE *userKey, int keylen, OPCF_ACTION action, BOOL compress)
{
	if (compress)
	{
		OP_ASSERT(!"compression has never been implemented in OPC_Crypt");
		// and I see no calls which try to pass compress, either.
		return FALSE;
	}

	if(action == OPCF_ENCODE)
	{
		OP_ASSERT(!"encryption  is not implemented in this version OPC_Crypt, use the utility instead");
		// and I see no calls which try to encrypt, either.
		return FALSE;
	}

	if (!(filename_in && *filename_in && filename_out && *filename_out))
	{
		OP_ASSERT(!"Bad or missing filename in OPC_Crypt");
		return FALSE;
	}

	UINT16 checkSumToMatch = 0;
	BOOL bad_crc = FALSE;

	SSL_secure_varvector32 data;

	BOOL ok = FALSE;
	
	TRAPD(op_err, ok = OPC_Crypt_gutsDecodeL(SSL_BLOWFISH_CBC, data,
					   checkSumToMatch,
					   filename_in, filename_out,
					   userKey, keylen));

	if (ok && action == OPCF_DECODE)
	{

		//	Check file crc
		UINT16 decodedCheckSum = 0;
		ok = GetFileCheckSum(filename_out, &decodedCheckSum);
		if (ok)
		{
			ok = (checkSumToMatch == decodedCheckSum);
			if (!ok)
				bad_crc = TRUE;
		}
	}

	if (!ok)
	{
		//	delete output file, something went wrong
		if (filename_out && *filename_out)
		{
			OpFile f;
			if (OpStatus::IsSuccess(f.Construct(filename_out)))
			{
				f.Delete();
			}
		}
	}

	return ok;
}

// OPC_DeCrypt
// This version that uses a OpMemoryFile that is used instead of a temporary file.
// for more info on BIO_s_mem : http://www.openssl.org/docs/crypto/BIO_s_mem.html
//
BOOL OPC_DeCrypt(const uni_char *filename_in, OpMemFile* &resOpFile, BYTE *userKey, int keylen)
{
	if (!(filename_in && *filename_in))
	{
		OP_ASSERT(!"Bad or missing filename");
		return FALSE;
	}

	UINT16 checkSumToMatch = 0;

	SSL_secure_varvector32 data;

	BOOL ok = FALSE;
	
	TRAPD(op_err, ok = OPC_Crypt_gutsDecodeL(SSL_BLOWFISH_CBC, data,
					   checkSumToMatch,
					   filename_in, NULL,
					   userKey, keylen));

	resOpFile = OpMemFile::Create((unsigned char*)data.GetDirect(), data.GetLength());

	return ok;
}

#endif // OPCRYPT_ALTERNATIVE_IMPLEMENTATION
#endif //  _LOCAL_FILE_CRYPTO_
