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

#ifndef SSLURL_RETRIEVE_H
#define SSLURL_RETRIEVE_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/url/url2.h"

class SSL_URLRetrieve_Item : public Link
{
public:
	URL url;
	time_t expire;

public:
	SSL_URLRetrieve_Item(URL &a_url, time_t expire_time):url(a_url), expire(expire_time){};
	virtual ~SSL_URLRetrieve_Item(){};

	SSL_URLRetrieve_Item *Suc() const {return (SSL_URLRetrieve_Item *) Link::Suc();}
	SSL_URLRetrieve_Item *Pred() const {return (SSL_URLRetrieve_Item *) Link::Pred();}
};

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLURL_RETRIEVE_H */
