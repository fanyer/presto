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

#ifndef SSLVERIFYINFO_H
#define SSLVERIFYINFO_H

#ifdef _NATIVE_SSL_SUPPORT_

struct SSL_CertificateVerification_Info
{
public:
	int	status;
	
	OpStringS issuer_name;
	OpStringS subject_name;
	OpStringS expiration_date;
	OpStringS valid_from_date;
	SSL_DistinguishedName subject_name_dn;
	SSL_varvector16	key_hash;
	SSL_ASN1Cert certificate;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	BOOL	  extended_validation;
#endif

	SSL_CertificateVerification_Info(){
		status = SSL_CERT_UNUSED;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
		extended_validation= FALSE;
#endif
	}
};
#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLVERIFYINFO_H */
