/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _AUTH_ENUM_H_
#define _AUTH_ENUM_H_

// 05/04/98 YNP // Modified values and added new constants
// scheme is now ftp and news are bit zero and one respectively 
// HTTP proxy auth is indicated by bit 2
// HTTP authentication is indicated by bit 3 through 7 in numerical order 0 through 31 (shifted 3 bits)
#ifndef AuthScheme
// AuthScheme may have defined in url_enum.h
# define AuthScheme			BYTE
#endif // AuthScheme
#define AUTH_SCHEME_FTP			((BYTE)(1<<0))
#define OBSOLETE_AUTH_SCHEME_NEWS		((BYTE)(1<<1))
#define AUTH_SCHEME_HTTP_PROXY	((BYTE)(1<<2)) 
#define AUTH_SCHEME_HTTP_UNKNOWN	((BYTE)(0x00 << 3))
#define AUTH_SCHEME_HTTP_BASIC	((BYTE)(0x01 << 3))
#define AUTH_SCHEME_HTTP_DIGEST	((BYTE)(0x02 << 3))
#define AUTH_SCHEME_HTTP_NTLM	((BYTE)(0x03 << 3))
#define AUTH_SCHEME_HTTP_NEGOTIATE	((BYTE)(0x04 << 3))
#define AUTH_SCHEME_HTTP_NTLM_NEGOTIATE	((BYTE)(0x05 << 3))

// HTTP authentication mask
#define AUTH_SCHEME_HTTP_MASK	((BYTE) 0xF8)

// Proxy auth macros
#define AUTH_SCHEME_HTTP_PROXY_BASIC	((AUTH_SCHEME_HTTP_PROXY) | AUTH_SCHEME_HTTP_BASIC) 
#define AUTH_SCHEME_HTTP_PROXY_DIGEST	((AUTH_SCHEME_HTTP_PROXY) | AUTH_SCHEME_HTTP_DIGEST) 
#define AUTH_SCHEME_HTTP_PROXY_NTLM		((AUTH_SCHEME_HTTP_PROXY) | AUTH_SCHEME_HTTP_NTLM) 
#define AUTH_SCHEME_HTTP_PROXY_NEGOTIATE		((AUTH_SCHEME_HTTP_PROXY) | AUTH_SCHEME_HTTP_NEGOTIATE) 
#define AUTH_SCHEME_HTTP_PROXY_NTLM_NEGOTIATE	((AUTH_SCHEME_HTTP_PROXY) | AUTH_SCHEME_HTTP_NTLM_NEGOTIATE) 

#endif // _AUTH_ENUM_H_
