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

#ifndef _ROOT_CRL_H_
#define _ROOT_CRL_H_

#include "extra/vrsn_ofx.h"
#include "extra/vrsn_svrint.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))
#endif

#define ROOT_CERT_CRL_OVERRIDE_BASE(cert, crl, before, after, first, last) {cert, NULL, ARRAY_SIZE(cert), crl, before, after, first, last}
#define ROOT_NAME_CRL_OVERRIDE_BASE(name, crl, before, after, first, last) {NULL, name, ARRAY_SIZE(name), crl, before, after, first, last}

#define ROOT_CERT_CRL_OVERRIDE(cert, crl) ROOT_CERT_CRL_OVERRIDE_BASE(cert, crl,0,0,1,0)
#define ROOT_NAME_CRL_OVERRIDE(name, crl) ROOT_NAME_CRL_OVERRIDE_BASE(name, crl,0,0,1,0)

#define ROOT_CERT_CRL_OVERRIDE_BEFORE(cert, crl, before) ROOT_CERT_CRL_OVERRIDE_BASE(cert, crl, before,0,1,0)
#define ROOT_CERT_CRL_OVERRIDE_AFTER(cert, crl, after) ROOT_CERT_CRL_OVERRIDE_BASE(cert, crl, 0, after,1,0)

#define ROOT_CERT_CRL_OVERRIDE_VERSION(cert, crl, first, last) ROOT_CERT_CRL_OVERRIDE_BASE(cert, crl, 0,0, first, last)

const DEFCERT_crl_overrides crl_overrides[] = {
#ifdef USE_OLD_VERISIGN_CLASS3
	ROOT_CERT_CRL_OVERRIDE_VERSION(Verisign_Class3_G1, "http://crl.verisign.com/pca3.crl",2,2),
#endif
#ifdef USE_NEW_VERISIGN_CLASS3
	ROOT_CERT_CRL_OVERRIDE_VERSION(VERISIGN_PCA3_SHA, "http://crl.verisign.com/pca3.crl",3,0),
#endif
	ROOT_CERT_CRL_OVERRIDE_AFTER(VERISIGN_CLASS3_G1_INTERMEDIATE, "http://svrintl-crl.verisign.com/SVRIntl.crl",1),
	ROOT_CERT_CRL_OVERRIDE_AFTER(VERISIGN_OPEN_FIN_EX_CA_G2, "http://crl.verisign.com/Class3NewOFX.crl",1),
	ROOT_CERT_CRL_OVERRIDE(SONERA_CLASS2_CA, "http://crl-2.trust.teliasonera.com/soneraclass2ca.crl")	,

	// All new entries above this line
	{NULL,NULL,0, NULL, 0, 0, 0, 0}
};

DEFCERT_ocsp_overrides ocsp_overrides[] = {
	"http://ocsp.turktrust.com.tr",
	"http://ocsp.wisekey.com/",
	"http://validation.diginotar.nl",

	// All new entries above this line
	NULL
};

#endif // _ROOT_CRL_H_
