/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
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
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/certs/certhandler.h"
#include "modules/libssl/smartcard/smc_man.h"
#include "modules/libssl/smartcard/smckey.h"

#include "modules/libssl/debug/tstdump2.h"

SSL_CertificateHandler_List::SSL_CertificateHandler_List()
{
	certitem = NULL;
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	external_key_item = NULL;
#endif
}

SSL_CertificateHandler_List::~SSL_CertificateHandler_List()
{
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	OP_DELETE(external_key_item);
	external_key_item = NULL;
#endif
}

SSL_CertificateHandler_List* SSL_CertificateHandler_ListHead::Item(int n)
{
    SSL_CertificateHandler_List *present;
    
    for(present = First();n>0 && present != NULL; n--)
		present = present->Suc();
    
    return present;
}

#endif // relevant support
