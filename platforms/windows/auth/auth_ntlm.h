/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _AUTH_NTLM_H_
#define _AUTH_NTLM_H_


#ifdef _SUPPORT_PROXY_NTLM_AUTH_

#include <security.h>
#include <sspi.h>

class NTLM_AuthElm : public AuthElm_Base
{
private:
	// Global handles for SSPI package
	static PSecurityFunctionTable SecurityInterface ;
	//static SecPkgInfo *SecurityPackages;
	//static DWORD NumOfPkgs;
	//static DWORD PkgToUseIndex;

	OpString  domain;
	OpString  user_name;
	OpString  password;

	OpString8 last_challenge;
	OpString8 ntlm_challenge;


	BOOL gotten_credentials;
	BOOL try_negotiate;
	BOOL try_ntml;
	CredHandle  Credentials;
	CtxtHandle  SecurityContext;
	_SEC_WINNT_AUTH_IDENTITY id;

	OpString SPN;

public:
    NTLM_AuthElm(AuthScheme scheme,	URLType a_urltype, 	unsigned short a_port);
    ~NTLM_AuthElm();

	OP_STATUS Construct(OpStringC8 a_user, OpStringC8 a_passwd);
	OP_STATUS Construct(NTLM_AuthElm *master);
    
    OP_STATUS	GetAuthString(OpStringS8 &ret_str, URL_Rep *, HTTP_Request *http= NULL);
	OP_STATUS	Update_Authenticate(ParameterList *header);

    virtual OP_STATUS SetPassword(OpStringC8 a_passwd);
    virtual const char* GetPassword() const { return NULL; }	// Security problem to return anything else according to Yngve

	static void Terminate();
    virtual OP_STATUS	    GetAuthString(OpStringS8 &ret_str, URL_Rep *,
#if defined(SSL_ENABLE_DELAYED_HANDSHAKE) && (!defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY))
										  BOOL secure,
#endif
										  HTTP_Method http_method,
										  HTTP_request_st* request,
#ifdef HTTP_DIGEST_AUTH
										  HTTP_Request_digest_data *auth_digest,
										  HTTP_Request_digest_data *proxy_digest,
#endif
										  Upload_Base* upload_data,
										  OpStringS8 &data);

	virtual BOOL MustClone() const;
	virtual AuthElm *MakeClone();
	virtual void ClearAuth();

	OP_STATUS SetHost(ServerName *sn, unsigned short port);

	virtual OP_STATUS GetProxyConnectAuthString(OpStringS8 &ret_str, URL_Rep *, Base_request_st *request, HTTP_Request_digest_data &proxy_digest)
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
};

#endif // _SUPPORT_PROXY_NTLM_AUTH_

#endif // _AUTH_NTLM_H_
