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

#ifndef _DIGEST_AUTH_ELEMENT_H_
#define _DIGEST_AUTH_ELEMENT_H_

#include "modules/auth/auth_elm.h"

#ifdef HTTP_DIGEST_AUTH 

#include "modules/libcrypto/include/CryptoHash.h"
#define EXTERNAL_PERFORM_INIT(hash, op, param)

void ConvertToHex(OpAutoPtr<CryptoHash> &hash, char *buffer);
void ConvertToHex(CryptoHash *hash, char *buffer);
 
#if 0 //def _NATIVE_SSL_SUPPORT_
class SSL_Hash;
void ConvertToHex(SSL_Hash *hash, char *buffer);

# include "modules/libssl/sslenum.h"
#endif

# ifdef EXTERNAL_DIGEST_API
#  include "modules/util/smartptr.h"
#  include "modules/libssl/external/ext_digest/external_digest_man.h"
# else
#  define EXTERNAL_PERFORM_INIT(hash, op, param) 
# endif

class ServerName;

enum digest_alg {
	DIGEST_UNKNOWN, 
	DIGEST_MD5, 
	DIGEST_MD5_SESS
#ifdef HTTP_DIGEST_AUTH_SHA1
	, DIGEST_SHA
	, DIGEST_SHA_SESS
#endif
};

enum digest_qop {
	DIGEST_QOP_NONE, 
	DIGEST_QOP_AUTH,
	DIGEST_QOP_AUTH_INT
};


class AuthElm_Alias : public AuthElm 
{
private:
	AuthElm *original;
public:
	AuthElm_Alias(AuthElm *org, unsigned short a_port, URLType a_urltype);
	virtual ~AuthElm_Alias();

	virtual unsigned long	IsAlias();
	virtual AuthElm	*AliasOf();

    //virtual URLType				GetUrlType() const; 
    //virtual unsigned short		GetPort() const; 
    virtual const char*			GetRealm() const;
    virtual const char*			GetUser() const;
    virtual AuthScheme			GetScheme() const;
	virtual const char*			GetPassword() const;
	virtual URL_CONTEXT_ID		GetContextId() const;

    virtual OP_STATUS	GetAuthString(OpStringS8 &ret_str, URL_Rep *, HTTP_Request *http=NULL);
	virtual OP_STATUS	GetProxyConnectAuthString(OpStringS8 &ret_str, URL_Rep *, Base_request_st *request, HTTP_Request_digest_data &proxy_digest);

    virtual OP_STATUS	SetPassword(OpStringC8 a_passwd);
	virtual OP_STATUS	Update_Authenticate(ParameterList *header);
};

class AuthAlias_Ref : public Link
{
public:
	AuthElm *alias;

	AuthAlias_Ref(AuthElm *als=NULL);
	virtual ~AuthAlias_Ref();
};

struct digest_data 
{
	OpString8		nonce;
	unsigned long	noncecount;
	OpString8		cnonce;
	OpString8		opaque;
	digest_alg		alg;
	CryptoHashAlgorithm digest;
	
	OpString8		H_A1;
	unsigned		H_A1_size;
	
	struct 
	{
		BYTE qop_found:1;
		BYTE qop_auth:1;
		BYTE qop_auth_int:1;
	} qop;
};

class URL_Rep;
class Sequence_Splitter;
typedef Sequence_Splitter ParameterList;

OP_STATUS Check_Digest_Authinfo(HTTP_Authinfo_Status &ret_status, ParameterList *header, BOOL proxy, BOOL finished,
								HTTP_Request_digest_data &auth_digest,
								HTTP_Request_digest_data &proxy_digest,
								HTTP_Method method,
								HTTP_request_st* request,
								unsigned long auth_id,
								unsigned long proxy_auth_id);

struct Base_request_st;

class Digest_AuthElm : public AuthElm_Base
{
private:
	OpString8 	password;
	OpString8 	auth_string;
	int state;
	digest_data digest_fields;
#if defined EXTERNAL_DIGEST_API
	OpSmartPointerWithDelete<External_Digest_Handler> digest_handler;
#endif

	OP_STATUS   Calculate_Digest(OpStringS8 &ret_str,
								 URL_Rep *url,
#if !defined(_EXTERNAL_SSL_SUPPORT_) || defined(_USE_HTTPS_PROXY)
								 BOOL secure,
#endif
								 HTTP_Method http_method,
								 Base_request_st* request,
								 HTTP_Request_digest_data &auth_digest,
								 HTTP_Request_digest_data &proxy_digest,
								 Upload_Base* upload_data);
public:

    Digest_AuthElm(AuthScheme a_scheme, URLType a_urltype, 	unsigned short a_port, BOOL authenticate_once = FALSE);
	OP_STATUS Construct(ParameterList *header, OpStringC8 a_realm, OpStringC8 a_user, OpStringC8 a_passwd, ServerName *server);
    virtual ~Digest_AuthElm();
    
	OP_STATUS	Init_Digest(ParameterList *header, ServerName *server);
	BOOL		Check_Digest_Authinfo(ParameterList *header);
    virtual OP_STATUS	GetAuthString(OpStringS8 &ret_str, URL_Rep *, HTTP_Request *http=NULL);

	// More specific case than GetAuthString that requires much less info getting passed in.
    virtual OP_STATUS	GetProxyConnectAuthString(OpStringS8 &ret_str, URL_Rep *, Base_request_st *request, HTTP_Request_digest_data &proxy_digest);

    virtual OP_STATUS	SetPassword(OpStringC8 a_passwd);

	virtual OP_STATUS	Update_Authenticate(ParameterList *header);

	virtual const char*	GetPassword() const;

};
#endif


#endif /* _DIGEST_AUTH_ELEMENT_H_ */

