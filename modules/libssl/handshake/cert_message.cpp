/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/pi/OpSystemInfo.h"

#include "modules/libssl/sslbase.h"
#include "modules/libssl/keyex/sslkeyex.h"
#include "modules/libssl/sslrand.h"

#include "modules/util/cleanse.h"

SSL_KEA_ACTION SSL_Certificate_st::ProcessMessage(SSL_ConnectionState *pending)
{
	OP_ASSERT(pending && pending->session && pending->key_exchange);

	pending->session->Site_Certificate = *this;
	if(pending->session->Site_Certificate.Error())
		return SSL_KEA_Handle_Errors;
#ifndef TLS_NO_CERTSTATUS_EXTENSION
	if(pending->session->sent_ocsp_extensions.GetLength() != 0 || pending->session->ocsp_extensions_sent)
		return SSL_KEA_No_Action;
#endif
	return pending->key_exchange->ReceivedCertificate();
}

void SSL_Certificate_st::SetUpMessageL(SSL_ConnectionState *pending)
{
	OP_ASSERT(pending && pending->session);
	operator =(pending->session->Client_Certificate);
}

#endif // relevant support
