/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifdef LIBOPEAY_X509_EX_DATA

#include "modules/libopeay/openssl/x509.h"

#include "modules/libopeay/libopeay_module.h"
#include "modules/libopeay/libopeay_implmodule.h"

void free_X509_EV_OID_data(void *parent, void *ptr, CRYPTO_EX_DATA *ad,
					int idx, long argl, void *argp)
{
	sk_ASN1_OBJECT_pop_free((STACK_OF(ASN1_OBJECT) *) ptr, ASN1_OBJECT_free);
}

int CheckX509_EV_dataID()
{
	if(g_ev_oid_x509_list_index != -1)
		return g_ev_oid_x509_list_index;

	g_ev_oid_x509_list_index = X509_get_ex_new_index(0, NULL, NULL, NULL, free_X509_EV_OID_data);
	return g_ev_oid_x509_list_index;
}

#endif
