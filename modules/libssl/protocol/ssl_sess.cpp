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

#include "modules/libssl/sslbase.h"
#include "modules/libssl/protocol/sslstat.h"

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/http1.h"

#include "modules/hardcore/mem/mem_man.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/libssl/method_disable.h"


SSL_SessionStateRecord::SSL_SessionStateRecord()
{
	InternalInit();
}

void SSL_SessionStateRecord::InternalInit()
{
	//servername = NULL;
	//port = 0;
	//tls_disabled = FALSE;
	last_used = 0;
	is_resumable = TRUE;
	session_negotiated = FALSE;
	connections = 0;
	cipherdescription =NULL;
	security_rating = SECURITY_STATE_UNKNOWN;
	low_security_reason = SECURITY_REASON_NOT_NEEDED;
	used_compression = SSL_Null_Compression;
	used_correct_tls_no_cert = FALSE;
	use_correct_tls_no_cert = TRUE;
	UserConfirmed = NO_INTERACTION;
	certificate_status = SSL_CERT_NO_WARN;
	renegotiation_extension_supported = FALSE;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	extended_validation = FALSE;
#endif
#ifndef TLS_NO_CERTSTATUS_EXTENSION
	ocsp_extensions_sent=FALSE;
#endif

#ifdef _SECURE_INFO_SUPPORT
	session_information = NULL;
#endif
}

#ifdef _SECURE_INFO_SUPPORT
void SSL_SessionStateRecord::DestroySessionInformation()
{
	session_information_lock.UnsetURL();
	OP_DELETE(session_information);
	session_information = NULL;
}
#endif

SSL_SessionStateRecord::~SSL_SessionStateRecord()
{
#ifdef _SECURE_INFO_SUPPORT
	DestroySessionInformation();
#endif
}

SSL_SessionStateRecordList::SSL_SessionStateRecordList()
{
}

SSL_SessionStateRecordList::~SSL_SessionStateRecordList()
{
	if(InList())
		Out();
}
#endif
