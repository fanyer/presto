/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef SSLCRLITEM_H
#define SSLCRLITEM_H

#if defined _NATIVE_SSL_SUPPORT_ && defined LIBSSL_ENABLE_CRL_SUPPORT
#include "modules/url/url2.h"

class CRL_List;

class CRL_item: public Link
{
public:
	BOOL loaded;
	BOOL retrieved;
	URL crl_url;
	SSL_varvector32 crl_data;
	SSL_varvector32 issuer;
	AutoDeleteHead alternative_urls; // URLLink

public:
	CRL_item():loaded(FALSE), retrieved(FALSE){};

	OP_STATUS	DecodeData();
	
	BOOL AddURL(URL &url, SSL_varvector32 &a_issuer, CRL_List *precollected_crls, CRL_List *collect_crls, BOOL promote=FALSE);
	BOOL HaveURL(URL &url);
	BOOL HaveIssuer(SSL_varvector32 &a_issuer){return issuer == a_issuer;};
	BOOL PromoteURL();
	BOOL UpgradeURL(URL &url);

	CRL_item *Suc()const{return (CRL_item *) Link::Suc();}
	CRL_item *Pred()const{return (CRL_item *) Link::Pred();}

private:
	BOOL MatchURL(URL &our_url, URL &url, OpString8 &url_str);
};

class CRL_List : public AutoDeleteHead
{
public:

	CRL_item *FindCRLItem(URL &url);
	CRL_item *FindCRLItem(SSL_varvector32 &a_issuer);

	CRL_item *First()const{return (CRL_item *) AutoDeleteHead::First();}
	CRL_item *Last()const{return (CRL_item *) AutoDeleteHead::Last();}
};
#endif

struct SSL_CRL_Override_Item : public Link
{
	SSL_varvector16 cert_id;
	OpString8	crl_url;
};

#endif /* SSLCRLITEM_H */
