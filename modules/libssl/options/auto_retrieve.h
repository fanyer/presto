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

#ifndef SSLAUTORETR_H
#define SSLAUTORETR_H

#ifdef _NATIVE_SSL_SUPPORT_

class SSL_AutoRetrieve_Item : public Link
{
public:
	SSL_varvector32  issuer_id;
	time_t last_checked;

public:
	SSL_AutoRetrieve_Item():last_checked(0){};
	virtual ~SSL_AutoRetrieve_Item(){};

	SSL_AutoRetrieve_Item *Suc() const {return (SSL_AutoRetrieve_Item *) Link::Suc();}
	SSL_AutoRetrieve_Item *Pred() const {return (SSL_AutoRetrieve_Item *) Link::Pred();}
};

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLAUTORETR_H */
