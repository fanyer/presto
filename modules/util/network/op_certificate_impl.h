/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef OP_CERTIFICATE_IMPL_H
#define OP_CERTIFICATE_IMPL_H

#ifdef ABSTRACT_CERTIFICATES

#include "modules/pi/network/OpCertificate.h"


/** Implementation of OpCertificate interface. */
class OpCertificateImpl : public OpCertificate
{
public:
	static OpCertificateImpl* CreateImpl(
		const uni_char *short_name,
		const uni_char *full_name,
		const uni_char *issuer,
		const uni_char *valid_from,
		const uni_char *valid_to,
		const char *certificate_hash, int certificate_hash_length
		);

	virtual ~OpCertificateImpl();

	virtual const uni_char* GetShortName() const { return m_short_name.CStr(); }
	virtual const uni_char* GetFullName() const  { return m_full_name.CStr();  }
	virtual const uni_char* GetIssuer() const    { return m_issuer.CStr();     }
	virtual const uni_char* GetValidFrom() const { return m_valid_from.CStr(); }
	virtual const uni_char* GetValidTo() const   { return m_valid_to.CStr();   }
	virtual const uni_char* GetInfo() const      { return m_info.CStr();       }
	virtual const char* GetCertificateHash(unsigned int &length) const { length = m_length; return m_certificate_hash; }

private:
	OpString m_short_name;
	OpString m_full_name;
	OpString m_issuer;
	OpString m_valid_from;
	OpString m_valid_to;
	OpString m_info;

	char *m_certificate_hash;
	int m_length;
};

#endif // ABSTRACT_CERTIFICATES
#endif // OP_CERTIFICATE_IMPL_H
