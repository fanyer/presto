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

#ifndef SSLACCEPTITEM_H
#define SSLACCEPTITEM_H

#ifdef _NATIVE_SSL_SUPPORT_
#include "modules/util/opstrlst.h"

class SSL_AcceptCert_Item: public Link
{
public:
	SSL_ASN1Cert certificate;

	OpString AcceptedAs;

	OpString_list Certificate_names;

	int certificate_status;

	SSL_ConfirmedMode_enum  confirm_mode;

public:

	SSL_AcceptCert_Item *Suc(){return (SSL_AcceptCert_Item *) Link::Suc();}
	SSL_AcceptCert_Item *Pred(){return (SSL_AcceptCert_Item *) Link::Pred();}
};

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLACCEPTITEM_H */
