/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef PROTOCOL_COMMON_H
#define PROTOCOL_COMMON_H

#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"


enum PROXY_type {NO_PROXY, PROXY_HTTP, PROXY_SOCKS};

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION

struct ProxyListItem : public Link
{
	ServerName_Pointer	proxy_candidate;
	unsigned int		port;
	PROXY_type			type;
};
#endif


struct Base_request_st
{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	Head				connect_list;
	ProxyListItem		*current_connect_host;
#endif
	ServerName_Pointer	connect_host;
	unsigned short		connect_port;
	PROXY_type			proxy;

	// Not sure why these are called "origin". It's the actual host we want to connect to
	// i.e. it may be the target host but there's a proxy we need to connect to to get to it.
	ServerName_Pointer	origin_host;
	unsigned short		origin_port;

	OpString8 request;

	Base_request_st()
	{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		current_connect_host = NULL;
#endif
		connect_host	= NULL;
		origin_host		= NULL;
		connect_port	= 0;
		origin_port		= 0;
		proxy			= NO_PROXY;
	};

	virtual ~Base_request_st()
	{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		connect_list.Clear();
		current_connect_host = NULL;
#endif
	};
};

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
void GetAutoProxyL(const uni_char *apcString, Base_request_st *req);
#endif //SUPPORT_AUTO_PROXY_CONFIGURATION


struct authdata_st
{
	ServerName_Pointer	connect_host;
	ServerName_Pointer	origin_host;

	unsigned short		connect_port;
	unsigned short		origin_port;

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	BOOL				session_based_support;
#endif

	HeaderList*			auth_headers;

	authdata_st();
	~authdata_st();
};

#ifdef HTTP_DIGEST_AUTH

#include "modules/libcrypto/include/CryptoHash.h"

struct HTTP_Request_digest_data{
	CryptoHashAlgorithm	hash_base;
	char*				used_A1;
	char*				used_cnonce;
	char*				used_nonce;
	unsigned long		used_noncecount;

	CryptoHash*			entity_hash;

	HTTP_Request_digest_data()	{ op_memset(this, 0, sizeof(HTTP_Request_digest_data)); }

	void ClearAuthentication();
};
#endif

#endif //PROTOCOL_COMMON_H
