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

#ifdef _ENABLE_AUTHENTICATE

#include "modules/auth/auth_elm.h"

// AuthElm base class
AuthElm::AuthElm(unsigned short a_port, URLType a_type, BOOL authenticate_once)
	: 	id(++g_auth_id_counter),
		port(a_port),	
		urltype(a_type),
		m_authenticate_once(authenticate_once)
{
}

AuthElm::~AuthElm()
{
	if(InList())
		Out();
}

unsigned long AuthElm::IsAlias()
{
	return 0;
}

AuthElm	*AuthElm::AliasOf()
{
	return this;
}



/*
OP_STATUS AuthElm::GetAuthString(OpStringS8 &ret_str, URL_Rep * url,
#if !defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY)
									   BOOL secure,
#endif
									   HTTP_Method http_method,
									   HTTP_request_st* request,
#ifdef HTTP_DIGEST_AUTH
									   HTTP_Request_digest_data &auth_digest,
									   HTTP_Request_digest_data &proxy_digest,
#endif
									   Upload_Base* upload_data,
									   OpStringC8 &data)
{
	return GetAuthString(ret_str,url);
}
*/


#ifdef HTTP_DIGEST_AUTH
void AuthElm::RemoveAlias(AuthElm *)
{
}

OP_STATUS AuthElm::AddAlias(AuthElm *)
{
	return OpStatus::ERR;
}
#endif

#endif
