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

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#ifdef SSL_DH_SUPPORT

#include "modules/libssl/sslbase.h"
#include "modules/libssl/handshake/dhparam.h"

SSL_ServerDHParams::SSL_ServerDHParams()
{
	AddItem(dh_p);
	AddItem(dh_g);
	AddItem(dh_Ys);
}

BOOL SSL_ServerDHParams::Valid(SSL_Alert *msg) const
{
	if(!LoadAndWritableList::Valid(msg))
		return FALSE;

	if(dh_p.GetLength() <1 || dh_g.GetLength() <1 || dh_Ys.GetLength() <1)
	{
		if(msg != NULL)
			msg->Set(SSL_Fatal, SSL_Illegal_Parameter);
		return FALSE;
	}
	
	return TRUE;
}
#endif // SSL_DH_SUPPORT
#endif
