/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#ifndef _SPDY_HEADERS_H_
#define _SPDY_HEADERS_H_

/**
 * Just simple and fast container for char* that may take the responsibility for freeing the string and remember it's length.
 * It's not guaranteed that the holded string in NULL terminated, so client code should obey the length field.
 */
struct SpdyHeaderString
{
	unsigned int external:1;
	unsigned int length:31;
	const char *str;

	SpdyHeaderString(): external(TRUE) {}
	void Init(const char *s, BOOL ext, UINT32 len = 0)
	{
		str = s;
		external = ext;
		length = len ? len : op_strlen(str);
	}
	~SpdyHeaderString()
	{
		if (!external)
			OP_DELETEA(str);
	}
};

/**
 * A simple struct for passing headers between SpdyStreamController and SpdyStreamHandler
 */
struct SpdyHeaders
{
	UINT32 headersCount;
	struct HeaderItem
	{
		SpdyHeaderString name;
		SpdyHeaderString value;
	} *headersArray;

	SpdyHeaders(): headersCount(0), headersArray(NULL) {}
	void InitL(int count) { headersArray = OP_NEWA_L(HeaderItem, headersCount = count); }
	~SpdyHeaders() { OP_DELETEA(headersArray); }
};


#endif // _SPDY_HEADERS_H_