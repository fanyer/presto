/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Sebastian Baberowski
**
*/
#include "core/pch.h"

#ifdef _EMBEDDED_BYTEMOBILE

#include "modules/url/protocols/ebo/bm_information_provider.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

#include "modules/url/url_man.h"

#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"

#ifndef SSL_MD5_LENGTH
#define SSL_MD5_LENGTH 16
#endif


static const char hexchars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};


	/**
	 * Shared secrets are xored to avoid clear existance inside opera binary
	 * Also they names are "obfuscated" and we do not want to optimize this part
	 */
static const unsigned char dfkhfdsi[ BM_secretSize] = 
{ 0x8a, 0xf3, 0xf8, 0x8f, 0xf9, 0xf9, 0x88, 0x8a, 0x8f, 0xfa, 0xfa, 0x8f, 0x89, 0xf9, 0xfa, 0xfa };
static const unsigned char lkdngied[ BM_secretSize] = 
{ 0xf8, 0xff, 0x8a, 0x8d, 0xfb, 0xfb, 0xfb, 0xfb, 0xfc, 0xf2, 0xf2, 0xfc, 0xff, 0xf9, 0xfc, 0xf3 };


BMInformationProvider::BMInformationProvider()
{
	bcreq[0] = '\0';
	ocresp[0] = '\0';
	eboConnection = urlManager->GetEmbeddedBmOpt();
}

BMInformationProvider::~BMInformationProvider()
{
}

void BMInformationProvider::convertToHexString( const void* input, int inSize, void* output, int maxLen)
{
	// Also exist in Digest authentication
	// TODO: merge into one API
	const unsigned char* in = reinterpret_cast< unsigned const char*>( input);
	unsigned char* out = reinterpret_cast< unsigned char*>( output);

	for (; maxLen > 0 && inSize > 0; maxLen -= 2, --inSize)
	{
		*out++ = static_cast<unsigned char>( hexchars[ *in >> 4]);

		if (maxLen > 1)
			*out++ = hexchars[ *in++ & 0xf];
	}

	if (maxLen > 0)
		*out = '\0';
}

void BMInformationProvider::generateMD5HashL( const char* input, void* output, int maxLen)
{
	unsigned char* out = reinterpret_cast<unsigned char*>( output);

	op_memset( out, 0, maxLen);

	OpStackAutoPtr<CryptoHash> hasher(CryptoHash::CreateMD5());
	if (!hasher.get())
		LEAVE(OpStatus::ERR_NO_MEMORY);

	hasher->InitHash();

    hasher->CalculateHash(input);

	byte digest[SSL_MD5_LENGTH];  /* ARRAY OK 2008-03-01 yngve */ 
	hasher->ExtractHash(digest);

	convertToHexString( digest, sizeof(digest), output, maxLen);
}

void BMInformationProvider::computeBMHashL(const char* bid, const char* uid, const char* req, void* output, int maxLen)
{
	char tempSec[BM_secretSize+1];	/* ARRAY OK 2008-04-27 jonnyr */
	int i;
	// first secret
	for (i = 0; i < BM_secretSize; ++i)
		tempSec[i] = dfkhfdsi[i] ^ BM_cndogvn;
	tempSec[BM_secretSize] = '\0';

	OpString8 hash;
	hash.ReserveL( 2*BM_secretSize + sizeof(BM_BID) + BM_guidSize + BM_reqSize + 2 + 1);

	hash.SetL(tempSec);
	if (bid != NULL)
	{
		hash.AppendL("+");
		hash.AppendL(bid);
	}

	if (uid != NULL)
	{
		hash.AppendL("+");
		hash.AppendL(uid);
	}

	if (req != NULL)
	{
		hash.AppendL("+");
		hash.AppendL(req);
	}

	// second secret
	for (i = 0; i < BM_secretSize; ++i)
		tempSec[i] = lkdngied[i] ^ BM_cndogvn;
	tempSec[BM_secretSize] = '\0';

	hash.AppendL("+");
	hash.Append(tempSec);
	hash.MakeUpper();

	generateMD5HashL( hash.CStr(), output, maxLen );
}

int BMInformationProvider::getSettings() const
{
	int result = (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EmbeddedOptimizationLevel) & 0x7)
				 | (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EmbeddedPrediction) << 3 )
				 | (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EmbeddedImageResize) << 4 )
				 | (((SHOWIMAGESTATE) g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowImageState) == FIGS_ON ? 0 : 1) << 5)
				 | ((g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled)? 0 : 1) << 6)
				 //ShowAnimation?
				 ;
	return result;
}

void BMInformationProvider::getOperaGUIDL(OpString8& ebo_guid)
{
	ebo_guid.Empty();
	const OpStringC bytemobile_guid = g_pcnet->GetStringPref(PrefsCollectionNetwork::BytemobileGUID);

	if (bytemobile_guid.Length() == BM_guidSize)
	{
		ebo_guid.SetUTF8FromUTF16L( bytemobile_guid);
		return;
	}

	//generate guid
	unsigned char *random = (unsigned char *) g_memory_manager->GetTempBuf2k();
	char *ebo_guid_temp = ((char *) random) +128; // give some room 
	const int randomSize = 16;	
	OP_ASSERT(((unsigned char *)ebo_guid_temp) - random > randomSize && g_memory_manager->GetTempBuf2kLen() > 128+ BM_guidSize+1);

	SSL_RND(random+4,randomSize-4);
	
	time_t guid_time = g_timecache->CurrentTime();

	//lowest common denominator for time:
	unsigned int part_guid_time = (unsigned int) guid_time;
	random[0] = (part_guid_time>>24) & 0xFF;
	random[1] = (part_guid_time>>16) & 0xFF;
	random[2] = (part_guid_time>>8) & 0xFF;
	random[3] = part_guid_time & 0xFF;
	
	int ii = 0;
	int i = 0;
	for(; i < 16 && ii < BM_guidSize; ++i)
	{
		if (i==4 || i==6 || i==8 || i==10)
			ebo_guid_temp[ii++] = '-';

		unsigned char val = random[i];
		ebo_guid_temp[ii++] = hexchars[(val>>4) & 0x0f];
		ebo_guid_temp[ii++] = hexchars[val & 0x0f];
	}

	ebo_guid_temp[ii] = '\0';
	
	OpString newPref;
	ANCHOR( OpString, newPref);

	newPref.SetL(ebo_guid_temp);
	g_pcnet->WriteStringL(PrefsCollectionNetwork::BytemobileGUID, newPref.CStr());

	ebo_guid.SetL(ebo_guid_temp);
}

const char* BMInformationProvider::generateBCReqL()
{
	static const int randomSize = 16;
	unsigned char random[randomSize];	/* ARRAY OK 2008-04-27 jonnyr */
	SSL_RND(random,randomSize);

	convertToHexString( random, randomSize, bcreq, BM_reqSize + 1);

	//just for sure
	bcreq[ BM_reqSize] = '\0';

	return bcreq;
}

bool BMInformationProvider::checkBCRespL( const char* bcresp)
{
	char myBcresp[BM_reqSize + 1];	/* ARRAY OK 2008-04-27 jonnyr */

	if (eboConnection == false)
		computeBMHashL( BM_BID, NULL, bcreq, myBcresp, sizeof( myBcresp) );
	else
	{
		OpString8 ebo_guid;
		ANCHOR(OpString8, ebo_guid);
		getOperaGUIDL(ebo_guid);

		computeBMHashL( BM_BID, ebo_guid.CStr(), bcreq, myBcresp, sizeof( myBcresp) );
	}

	const char* tbcresp = myBcresp;

	//if one of resp is eq '\0' and second one is not - it will return inside "for" loop
	//if both are eq '\0' it will return true after "for" loop
	for (; *bcresp != '\0' || *tbcresp != '\0'; ++bcresp, ++tbcresp)
	{
		if ( *bcresp != *tbcresp)
			return false;
	}

	return true;
}

const char* BMInformationProvider::generateOCRespL( const char* eboServer, const char* ocreq)
{
	computeBMHashL( eboServer, NULL, ocreq, ocresp, sizeof(ocresp) );
	return ocresp;
}

#endif /* _EMBEDDED_BYTEMOBILE */

