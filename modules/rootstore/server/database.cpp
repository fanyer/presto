/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include <stdio.h>

#include "modules/rootstore/server/types.h"
#include "modules/rootstore/server/arrays.h"
#include "modules/rootstore/rootstore_table.h"

/** Number of certificate roots */
unsigned int GetRootCount()
{
	return LOCAL_CONST_ARRAY_SIZE(defcerts);
}

/** Get a given root */
const DEFCERT_st *GetRoot(unsigned int i)
{
	return (i< LOCAL_CONST_ARRAY_SIZE(defcerts) ? &defcerts[i] : NULL);
}

/** Number of certificates to delete */ 
unsigned int GetDeleteCertCount()
{
	return LOCAL_CONST_ARRAY_SIZE(defcerts_delete_list);
}

/** Get a cert delete item */
const DEFCERT_DELETE_cert_st *GetDeleteCertSpec(unsigned int i)
{
	return (i< LOCAL_CONST_ARRAY_SIZE(defcerts_delete_list) ? &defcerts_delete_list[i] : NULL);
}

/** Get number of untrusted certificates */
unsigned int GetUntrustedCertCount()
{
	return LOCAL_CONST_ARRAY_SIZE(defcerts_untrusted_list);
}

/** Get untrusted item */
const DEFCERT_UNTRUSTED_cert_st *GetUntrustedCertSpec(unsigned int i)
{
	return (i< LOCAL_CONST_ARRAY_SIZE(defcerts_untrusted_list) ? &defcerts_untrusted_list[i] : NULL);
}

/** Get number of untrusted certificates */
unsigned int GetUntrustedRepositoryCertCount()
{
	return LOCAL_CONST_ARRAY_SIZE(defcerts_untrusted_repository_list);
}

/** Get untrusted item */
const DEFCERT_UNTRUSTED_cert_st *GetUntrustedRepositoryCertSpec(unsigned int i)
{
	return (i< LOCAL_CONST_ARRAY_SIZE(defcerts_untrusted_repository_list) ? &defcerts_untrusted_repository_list[i] : NULL);
}

/** Get the NULL-item terminated list of CRL overrides */
const DEFCERT_crl_overrides *GetCRLOverride()
{
	return crl_overrides;
}

/** Get the NULL-item terminated list of OCSP overrides */
DEFCERT_ocsp_overrides *GetOCSPOverride()
{
	return ocsp_overrides;
}
