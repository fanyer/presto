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

#ifdef _SECURE_INFO_SUPPORT
#include "modules/util/htmlify.h"
#endif

SSL_SessionStateRecordList *SSL_ConnectionState::FindSessionRecord()
{
	RemoveSession();
	return (session = server_info->FindSessionRecord());
} 


void SSL_ConnectionState::AddSessionRecord(SSL_SessionStateRecordList *target)
{ 
	server_info->AddSessionRecord(target);
}

void SSL_ConnectionState::RemoveSession()
{
	if(session != NULL)
	{
		session->connections --;
		if (!session->is_resumable && !session->connections)
			server_info->RemoveSessionRecord(session);
		session = NULL;
	}     
}

void SSL_ConnectionState::OpenNewSession()
{
	RemoveSession();

	session = OP_NEW(SSL_SessionStateRecordList, ());
	if(session == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
		return;
	}
	
	session->is_resumable = TRUE;
	session->session_negotiated = FALSE;
	session->connections = 1;
	session->use_correct_tls_no_cert = !server_info->TLSUseSSLv3NoCert();
	
	session->used_cipher.Set(0,0);
	session->used_compression = SSL_Null_Compression;
}

#endif
