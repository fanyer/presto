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
#ifdef _SSL_USE_SMARTCARD_

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/http1.h"

#include "modules/hardcore/mem/mem_man.h"

BOOL ServerName::InvalidateSmartCardSession(const SSL_varvector24_list &certificate)
{
	SSL_Port_Sessions *current;
	BOOL ret = FALSE;
	
	current = (SSL_Port_Sessions *) Session_data.First();
	while(current)
	{
		if(current->InvalidateSmartCardSession(certificate))
			ret = TRUE;
		current = (SSL_Port_Sessions *) current->Suc();
	}

	return ret;
}

BOOL SSL_Port_Sessions::InvalidateSmartCardSession(const SSL_varvector24_list &certificate)
{
	BOOL ret = FALSE;
	SSL_SessionStateRecordList *session,*oldsession;
	
	session = sessioncache.First();
	while(session != NULL)
	{
		if(session->Client_Certificate.Count() > 0 &&
			session->Client_Certificate[0] == certificate[0])
		{
			ret = TRUE;
			session->is_resumable = FALSE;
			oldsession = session;
			session = session->Suc();
			if(oldsession->connections == 0)
			{
				//oldsession->connections--;
				oldsession->Out();
				OP_DELETE(oldsession);
			}
			continue;
		}
		session = session->Suc();
	}

	if(last_certificate && last_certificate->certificate == certificate[0])
	{
		ret = TRUE;
		OP_DELETE(last_certificate);
		last_certificate = NULL;
		Certificate_last_used = 0;
	}

	return ret;
}

void URL_Manager::InvalidateSmartCardSessions(SSL_varvector24_list &certificate)
{
	ServerName *sn = GetFirstServerName();

	while(sn)
	{
		if(sn->InvalidateSmartCardSession(certificate))
		{
			CleanSmartCardAuthenticatedCookies(sn);
			CleanSmartCardAuthenticatedDocuments(sn);
		}

		sn = GetNextServerName();
	}
}
#endif
#endif
