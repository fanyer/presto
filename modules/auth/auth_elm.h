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

#ifndef _AUTH_ELEMENT_H_
#define _AUTH_ELEMENT_H_

#include "modules/url/url_id.h"
#include "modules/url/protocols/http_met.h"
#include "modules/auth/auth_enum.h"

class URL_Rep;
class HTTP_Request;
struct HTTP_request_st;
struct HTTP_Request_digest_data;
class Upload_Base;
class Sequence_Splitter;
typedef Sequence_Splitter ParameterList;
struct Base_request_st;

class AuthElm : public Link 
{
private:
	unsigned long id;
	unsigned short	port; 
	URLType			urltype;
	BOOL			m_authenticate_once;

public:
	AuthElm(unsigned short a_port, URLType a_urltype, BOOL authenticate_once = FALSE);
	virtual ~AuthElm();

    AuthElm*	Suc() const { return (AuthElm *)Link::Suc(); };
    AuthElm*	Pred() const { return (AuthElm *)Link::Pred(); };

	unsigned long GetId() const{return id;};
	virtual BOOL GetAuthenticateOnce(){ return m_authenticate_once; }
	void SetId(unsigned long new_id){id = new_id;};

	virtual unsigned long	IsAlias();
	virtual AuthElm			*AliasOf();
    virtual URLType			GetUrlType() const{return urltype;}; 
    virtual unsigned short	GetPort() const { return port; }; 
    virtual const char*		GetRealm() const=0;
    virtual const char*		GetUser() const=0;
    virtual AuthScheme		GetScheme() const=0;
	virtual const char*		GetPassword() const=0;
	virtual	URL_CONTEXT_ID	GetContextId() const=0;
	virtual void			SetContextId(URL_CONTEXT_ID a_id){}

    virtual OP_STATUS		GetAuthString(OpStringS8 &ret_str, URL_Rep *, HTTP_Request *http=NULL)=0;
	/*
    virtual OP_STATUS	    GetAuthString(OpStringS8 &ret_str, URL_Rep *,
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
										  OpStringC8 &data);
*/
	// More specific case than GetAuthString that requires much less info getting passed in.
    virtual OP_STATUS		GetProxyConnectAuthString(OpStringS8 &ret_str, URL_Rep *, Base_request_st *request, HTTP_Request_digest_data &proxy_digest)=0;


    virtual OP_STATUS		SetPassword(OpStringC8 a_passwd)=0;
#ifdef HTTP_DIGEST_AUTH 
	virtual void			RemoveAlias(AuthElm *);
	virtual OP_STATUS		AddAlias(AuthElm *);
	virtual OP_STATUS		Update_Authenticate(ParameterList *header);
	virtual BOOL			Aliased() const{return FALSE;}
#endif

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	virtual BOOL MustClone() const{return FALSE;}
	virtual AuthElm *MakeClone(){return NULL;}
	virtual void ClearAuth(){};
#endif

};

class AuthElm_Base : public AuthElm {
private:
#ifdef HTTP_DIGEST_AUTH 
	AutoDeleteHead	aliased_elements;
#endif
	AuthScheme		scheme;
    OpString8		realm;
	OpString8		user;
	URL_CONTEXT_ID	context_id;
	
public:
    AuthElm_Base(unsigned short a_port, AuthScheme a_scheme, URLType a_urltype, BOOL authenticate_once = FALSE);
	OP_STATUS	Construct(OpStringC8 a_realm, OpStringC8 a_user);
    virtual ~AuthElm_Base();

    virtual const char*		GetRealm() const { return realm.CStr(); };
    virtual const char*		GetUser() const { return user.CStr(); };
    virtual AuthScheme		GetScheme() const { return scheme; };
    virtual void			SetScheme(AuthScheme a_scheme) {scheme = a_scheme; };
	virtual URL_CONTEXT_ID	GetContextId() const{return context_id;}
	virtual void			SetContextId(URL_CONTEXT_ID a_id){context_id = a_id;}
#ifdef HTTP_DIGEST_AUTH 
	virtual OP_STATUS		AddAlias(AuthElm *);
	virtual void			RemoveAlias(AuthElm *alias);
	virtual BOOL			Aliased() const{return !aliased_elements.Empty();}
#endif
};

#endif /* _AUTH_ELEMENT_H_ */

