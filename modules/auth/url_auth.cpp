/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url2.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/auth/auth_elm.h"
#include "modules/auth/auth_basic.h"
#include "modules/auth/auth_digest.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_pd.h"
#ifndef NO_FTP_SUPPORT
#include "modules/url/protocols/ftp.h"
#endif // NO_FTP_SUPPORT

// URL Authentication Management

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define AUTH_ERRSTR(p,x) Str::##p##_##x
#else
#define AUTH_ERRSTR(p,x) x
#endif

#ifdef _ENABLE_AUTHENTICATE

#ifndef CLEAR_PASSWD_FROM_MEMORY
#error "For security reasons FEATURE_CLEAR_PASSWORDS *MUST* be YES when HTTP Authentication is used"
#endif

void URL_DataStorage::Authenticate(AuthElm *auth_elm)
{
	if(!auth_elm || !mh_list || mh_list->IsEmpty())
		return;

	if((auth_elm->GetScheme() & AUTH_SCHEME_HTTP_PROXY) == 0 && (url->GetAttribute(URL::KUserName).CStr() || url->GetAttribute(URL::KPassWord).CStr()))
	{
		if(url->GetAttribute(URL::KUserName).Compare(auth_elm->GetUser()) != 0)
		{
			if((auth_elm->GetScheme() & AUTH_SCHEME_FTP) != 0)
			{
				FailAuthenticate(AUTH_GENERAL_FAILURE);
				return;
			}
			// redirect to non-username URL
			OpString8 temp_url;
			url->GetAttribute(URL::KName_With_Fragment_Escaped, temp_url);
			URL redirect_url = urlManager->GetURL(temp_url);
			TRAPD(op_err, SetAttributeL(URL::KMovedToURL, redirect_url));
			if(OpStatus::IsSuccess(op_err))
				ExecuteRedirect_Stage2();
			HandleFinished();
			return;
		}
	}

	if(loading)
		loading->Authenticate(auth_elm);
}

BOOL URL_DataStorage::CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy)
{
	if(!auth_elm || !mh_list || mh_list->IsEmpty())
		return TRUE;
	return !loading || loading->CheckSameAuthorization(auth_elm, proxy);
}

void  URL_DataStorage::FailAuthenticate(int mode)
{
	if(mode == AUTH_USER_CANCELLED)
	{
		OpFileLength registered_len=0;
		GetAttribute(URL::KContentLoaded, &registered_len);
		if(!registered_len)
			HandleFailed(ERR_SSL_ERROR_HANDLED);
		else
			HandleFinished();
	}
	else
	{
		int err = AUTH_ERRSTR(SI,ERR_UNSUPPORTED_SERVER_AUTH);

		switch(mode)
		{
		case AUTH_NO_DOMAIN_MATCH: 
			err = AUTH_ERRSTR(SI,ERR_AUTH_DOMAIN_MATCH);
			break;
		case AUTH_NO_PROXY_METH: 
			err = AUTH_ERRSTR(SI,ERR_UNSUPPORTED_PROXY_AUTH);
			break;
		case AUTH_MEMORY_FAILURE:
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
        case AUTH_GENERAL_FAILURE:
			err = AUTH_ERRSTR(SI,ERR_COMM_INTERNAL_ERROR);
			break;
		}
		HandleFailed(err);
	}
}

authdata_st *URL_DataStorage::GetAuthData()
{
	URL_HTTP_ProtocolData *http = GetHTTPProtocolData();
	if (http)
		return http->authinfo;
	return NULL;
}

#ifndef NO_FTP_SUPPORT
void URL_FTP_LoadHandler::Authenticate(AuthElm *auth_elm)
{
	if(!auth_elm || !ftp)
		return;

	authenticating = FALSE;
	OpStringS8 auth_str;

	URL_DataStorage *url_ds = url->GetDataStorage();
	if(url_ds == NULL)
		return; // should never happen

	OP_STATUS op_err = auth_elm->GetAuthString(auth_str, url);
	if( OpStatus::IsError(op_err) )
	{
		if( op_err == OpStatus::ERR_NO_MEMORY )
			url_ds->FailAuthenticate(AUTH_MEMORY_FAILURE);
		return; // cannot add authentication data (but let the process continue)
	}

	ftp->SetUserNameAndPassword(auth_elm->GetUser(), auth_str);
	ftp->SetAuthorizationId(auth_elm->GetId());

	authentication_done = TRUE;

	if(url_ds)
		url_ds->SetAttribute(URL::KHeaderLoaded,FALSE);
	ftp->Load();
}

BOOL URL_FTP_LoadHandler::CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy)
{
	return (!auth_elm || !ftp || proxy ||
		ftp->GetAuthorizationId() == auth_elm->GetId() ? TRUE : FALSE);
}
#endif // NO_FTP_SUPPORT

void URL_Rep::Authenticate(AuthElm *auth_elm)
{
	if (auth_interface)
		auth_interface->Authenticate(auth_elm);
	else if(storage)
		storage->Authenticate(auth_elm);
}

void URL_Rep::FailAuthenticate(int mode)
{
	if (auth_interface)
		auth_interface->FailAuthenticate(mode);
	else if(storage)
		storage->FailAuthenticate(mode);
}

BOOL URL_Rep::CheckSameAuthorization(AuthElm *auth_elm, BOOL proxy)
{
	if (auth_interface)
		return auth_interface->CheckSameAuthorization(auth_elm, proxy);
	else
		return !storage || storage->CheckSameAuthorization(auth_elm, proxy);
}

authdata_st *URL_Rep::GetAuthData()
{
	if (auth_interface)
		return auth_interface->GetAuthData();
	else if (storage)
		return storage->GetAuthData();
	return NULL;
}

MsgHndlList *URL_Rep::GetMessageHandlerList()
{
	if (auth_interface)
		return auth_interface->GetMessageHandlerList();
	else if (storage)
		return storage->GetMessageHandlerList();
	return NULL;
}


#endif
