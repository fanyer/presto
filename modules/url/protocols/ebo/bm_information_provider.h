/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Sebastian Baberowski
**
*/
#ifndef BMINFORMATIONPROVIDER_H
#define BMINFORMATIONPROVIDER_H

#ifdef _EMBEDDED_BYTEMOBILE

/**
 * BID used in heder sended to BM server
 */
#define BM_BID "1.3.0"
#define BM_guidSize (32 + 4)
#define BM_reqSize 32
#define BM_secretSize 16
#define BM_cndogvn 0xCB

class BMInformationProvider
{
	char bcreq[BM_reqSize + 1];		/* ARRAY OK 2008-04-27 jonnyr */
	char ocresp[BM_reqSize + 1];	/* ARRAY OK 2008-04-27 jonnyr */
	BOOL eboConnection;

	static void convertToHexString( const void* input, int inSize, void* output, int maxLen);
	static void generateMD5HashL( const char* input, void* output, int maxLen);
	
	/**
	 * Computing byte mobile hash for given parameters
	 * bid, uid and req can be NULL, then it will be not included
	 */
	static void computeBMHashL(const char* bid, const char* uid, const char* req, void* output, int maxLen);

	//forbiden operations
	BMInformationProvider( const BMInformationProvider&);
	BMInformationProvider& operator=(const BMInformationProvider&);

public:

	BMInformationProvider();

	~BMInformationProvider();


	/**
	 * Returns guid for BM (128-bit, hex encoded)
	 * It can leave if OOM
	 */
	void getOperaGUIDL(OpString8& ebo_guid);

	int getSettings() const;
	/**
	 * Generates BCRequest (128 bit, hex encoded)
	 * Returned pointer is static - pointed array will be changed after next call of this function
	 * It can leave if OOM and _NO_GLOBALS_ is defined
	 */
	const char* generateBCReqL();

	/**
	 * Check if given BC response is valid
	 * It will returns false if is called without calling generateBCReq before.
	 *
	 * @param eboConnection false for first request, true if connection is authorized
	 */
	bool checkBCRespL( const char* bcresp);

	/**
	 * Generates OC responce for given OC request (128 bit, hex encoded)
	 * Returned pointer is static - pointed array will be changed after next call of this function
	 */
	const char* generateOCRespL( const char* eboServer, const char* ocreq);
};

#endif /* _EMBEDDED_BYTEMOBILE */

#endif /* BMINFORMATIONPROVIDER_H */
