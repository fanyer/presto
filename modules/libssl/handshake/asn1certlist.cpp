/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"
#include "modules/libssl/handshake/asn1certlist.h"

BOOL SSL_ASN1Cert_list::Valid(SSL_Alert *msg) const
{
	uint32 i;

	if(!SSL_varvector24_list::Valid(msg))
		return FALSE;

	for(i=Count();i>0;)
	{
		i--;
		if(operator [](i).GetLength() <1)
		{
			if(msg != NULL)
				msg->Set(SSL_Fatal, SSL_Illegal_Parameter);
			return FALSE;
		}
	}

	return TRUE;
}
#endif
