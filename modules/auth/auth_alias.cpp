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

#include "core/pch.h"

#if defined _ENABLE_AUTHENTICATE && defined HTTP_DIGEST_AUTH

#include "modules/auth/auth_digest.h"


#if defined DEBUG && defined YNP_WORK 
#include "modules/olddebug/tstdump.h"

#define DEBUG_AUTH
#endif

AuthElm_Alias::AuthElm_Alias(AuthElm *org, unsigned short a_port, URLType a_urltype)
 : AuthElm(a_port, a_urltype)
{
	original = org;
	OP_ASSERT(original != this);
	if(org && OpStatus::IsError(org->AddAlias(this)))
		original = NULL;
}

AuthElm_Alias::~AuthElm_Alias()
{
	if(original)
		original->RemoveAlias(this);
}

unsigned long AuthElm_Alias::IsAlias()
{
	unsigned long ret = 0;
	if(original)
	{
		ret = original->IsAlias();
		if(ret == 0)
			ret = original->GetId();
	}

	return ret;
}

AuthElm	*AuthElm_Alias::AliasOf()
{
	return original;
}

OP_STATUS AuthElm_Alias::Update_Authenticate(ParameterList *header)
{
	if(original)
		return original->Update_Authenticate(header);
	return OpStatus::OK;
}

/*
URLType	AuthElm_Alias::GetUrlType() const
{
	return (original ? original->GetUrlType() : URL_UNKNOWN);
}

unsigned short	AuthElm_Alias::GetPort() const
{
	return (original ? original->GetPort() : 0);
}
*/

const char*		AuthElm_Alias::GetRealm() const
{
	return (original ? original->GetRealm() : NULL);
}

const char*		AuthElm_Alias::GetUser() const
{
	return (original ? original->GetUser() : NULL);
}

const char*		AuthElm_Alias::GetPassword() const
{
	return (original ? original->GetPassword() : NULL);
}

AuthScheme		AuthElm_Alias::GetScheme() const
{
	return (original ? original->GetScheme() : AUTH_SCHEME_HTTP_UNKNOWN);
}

URL_CONTEXT_ID	AuthElm_Alias::GetContextId() const
{
	return (original ? original->GetContextId() : 0);
}


OP_STATUS		AuthElm_Alias::GetAuthString(OpStringS8 &ret_str, URL_Rep *url, HTTP_Request *http)
{
	if( original )
		return original->GetAuthString(ret_str, url, http);
	else
	{
		ret_str.Empty();
		return OpStatus::OK;
	}
}

OP_STATUS		AuthElm_Alias::GetProxyConnectAuthString(OpStringS8 &ret_str, URL_Rep *url, Base_request_st *request, HTTP_Request_digest_data &proxy_digest)
{
	if( original )
		return original->GetProxyConnectAuthString(ret_str, url, request, proxy_digest);
	else
	{
		ret_str.Empty();
		return OpStatus::OK;
	}
}

/*
OP_STATUS		AuthElm_Alias::GetAuthString(OpStringS8 &ret_str, URL_Rep *url,
#if !defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY)
											 BOOL secure,
#endif
											 HTTP_Method http_method,
											 HTTP_request_st* request,
											 HTTP_Request_digest_data &auth_digest,
											 HTTP_Request_digest_data &proxy_digest,
											 Upload_Base* upload_data,
											 OpStringC8 &data)

{
	if( original )
		return original->GetAuthString(ret_str, url,
#if !defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY)
									   secure,
#endif
									   http_method, request,
									   auth_digest,
									   proxy_digest,
									   upload_data, data);
	else
	{
		ret_str.Empty();
		return OpStatus::OK;
	}
}
*/

OP_STATUS	AuthElm_Alias::SetPassword(OpStringC8 a_passwd)
{
	if(original)
		return original->SetPassword(a_passwd);

	return OpStatus::OK;
}

AuthAlias_Ref::AuthAlias_Ref(AuthElm *als)
{
	alias = als;
}

AuthAlias_Ref::~AuthAlias_Ref()
{
	if(alias)
		OP_DELETE(alias);
}

#endif // _ENABLE_AUTHENTICATE
