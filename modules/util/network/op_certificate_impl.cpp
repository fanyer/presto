/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ABSTRACT_CERTIFICATES

#include "modules/util/network/op_certificate_impl.h"


/* static */ OpCertificateImpl* OpCertificateImpl::CreateImpl(
	const uni_char *short_name,
	const uni_char *full_name,
	const uni_char *issuer,
	const uni_char *valid_from,
	const uni_char *valid_to,
	const char *certificate_hash, int certificate_hash_length
	)
{
	OpCertificateImpl *certificate = OP_NEW(OpCertificateImpl, ());

	if (
		certificate == NULL ||
		(certificate->m_certificate_hash = OP_NEWA(char, certificate_hash_length)) == NULL ||
		OpStatus::IsError(certificate->m_short_name.Set(short_name)) ||
		OpStatus::IsError(certificate->m_full_name.Set(full_name)) ||
		OpStatus::IsError(certificate->m_issuer.Set(issuer)) ||
		OpStatus::IsError(certificate->m_valid_from.Set(valid_from)) ||
		OpStatus::IsError(certificate->m_valid_to.Set(valid_to))
	)
	{
		OP_DELETE(certificate);
		return NULL;
	}

	certificate->m_length = certificate_hash_length;

	op_memcpy(certificate->m_certificate_hash, certificate_hash, certificate_hash_length);
	return certificate;
}

OpCertificateImpl::~OpCertificateImpl()
{
	OP_DELETEA(m_certificate_hash);
}

#endif
