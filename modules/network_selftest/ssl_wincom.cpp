/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#include "core/pch.h"

#if defined(SELFTEST) && defined(_NATIVE_SSL_SUPPORT_)

#include "modules/selftest/src/testutils.h"
#include "modules/network_selftest/sslwincom.h"

BasicSSLWindowListener::~BasicSSLWindowListener(){fallback=NULL;}

void BasicSSLWindowListener::OnCertificateBrowsingNeeded(OpWindowCommander* wic, SSLCertificateContext* context, SSLCertificateReason reason, SSLCertificateOption options)
{
	fallback->OnCertificateBrowsingNeeded(wic, context, reason, options);
}

void BasicSSLWindowListener::OnSecurityPasswordNeeded(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback)
{
	fallback->OnSecurityPasswordNeeded(wic, callback);
}

void BasicSSLWindowListener::ReportFailure(URL &url, const char *format, ...)
{
	OpString8 tempstring;
	va_list args;
	va_start(args, format);

	if(format == NULL)
		format = "";

	OP_STATUS op_err = url.GetAttribute(URL::KName_Escaped, tempstring);
	if(OpStatus::IsSuccess(op_err))
		op_err = tempstring.Append(" :");

	if(OpStatus::IsSuccess(op_err))
		tempstring.AppendVFormat(format, args);

	if(test_manager)
		test_manager->ReportTheFailure(OpStatus::IsSuccess(op_err) ? tempstring.CStr() : format);
	else
		ST_failed(OpStatus::IsSuccess(op_err) ? tempstring.CStr() : format);

	va_end(args);
}

#endif // SELFTEST && _NATIVE_SSL_SUPPORT_
