/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

// URL Cookie Management
#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/cookies/url_cm.h"
#include "modules/formats/url_dfr.h"

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
#include "adjunct/quick/dialogs/ServerManagerDialog.h"
#endif

#if defined(EMBROWSER_SUPPORT)
#include "adjunct/embrowser/EmBrowser_main.h"
#endif

#include "modules/olddebug/tstdump.h"

Cookie*
Cookie::CreateL(Cookie_Item_Handler *params)
{
	OpStringS8 rpath;
	ANCHOR(OpStringS8, rpath);

	if(params == NULL)
		LEAVE(OpStatus::ERR_NULL_POINTER);
	
	if(params->flags.recv_path)
	{
		rpath.TakeOver(params->recvd_path);
	}

	return OP_NEW_L(Cookie, (params, rpath));
}

//Private constructor
Cookie::Cookie(Cookie_Item_Handler *params, OpStringS8 &rpath)
{
	InternalInit(params, rpath);
}

void Cookie::InternalInit(Cookie_Item_Handler *params, OpStringS8 &rpath)
{
	name.TakeOver(params->name);

	value.TakeOver(params->value);

	expires = params->expire;
	flags.secure = params->flags.secure;
	flags.httponly = params->flags.httponly;
	flags.full_path_only = params->flags.full_path_only;

	flags.have_password = params->flags.have_password; 
	flags.have_authentication = params->flags.have_authentication;

	flags.accept_updates = params->flags.accept_updates;
	flags.accepted_as_third_party = params->flags.accepted_as_third_party;
	flags.assigned = params->flags.assigned;

	last_used = params->last_used;
	last_sync = params->last_sync;

	port.TakeOver(params->port);
	portlist = params->portlist;
	params->portlist = NULL;
	portlen = params->port_count;

	version = params->version;
	
	flags.port_received = (version  && (!port.IsEmpty() || portlen) ? TRUE : FALSE);

	flags.only_server = params->flags.only_server;
	flags.discard_at_exit = params->flags.discard_at_exit;
	flags.protected_cookie = FALSE;

#ifdef _SSL_USE_SMARTCARD_
	flags.smart_card_authenticated = params->flags.smart_card_authenticated;
	if(flags.smart_card_authenticated )
		flags.discard_at_exit = TRUE;
#endif

	comment.TakeOver(params->comment);
	commenturl.TakeOver(params->comment_URL);
	if(params->flags.recv_dom)
		received_domain.TakeOver(params->recvd_domain);

	received_path.TakeOver(rpath);

	g_cookieManager->IncCookiesCount();
#ifdef DEBUG_COOKIE_MEMORY_SIZE
	g_cookieManager->AddCookieMemorySize(name.Length() +
		name.Length() +
		value.Length() +
		received_path.Length() +
		received_domain.Length() +
		port.Length() +
		comment.Length() +
		commenturl.Length() +
		(portlen * sizeof(unsigned short)) +
		sizeof(Cookie));
#endif
#ifdef NEED_URL_COOKIE_ARRAY
	domain = NULL;
	path = NULL;
#endif // NEED_URL_COOKIE_ARRAY

	cookie_type = params->cookie_type;
}

Cookie::~Cookie()
{
#ifdef WIC_COOKIES_LISTENER
	if(InList() && g_cookie_API)
		g_cookie_API->GetCookieIteratorListener()->OnDeleteCookie(*this);
#endif // WIC_COOKIES_LISTENER
	Out();
	InternalDestruct();
}

void Cookie::InternalDestruct()
{
#ifdef DEBUG_COOKIE_MEMORY_SIZE
	g_cookieManager->SubCookieMemorySize(name.Length() +
		name.Length() +
		value.Length() +
		received_path.Length() +
		received_domain.Length() +
		port.Length() +
		comment.Length() +
		commenturl.Length() +
		(portlen * sizeof(unsigned short)) +
		sizeof(Cookie));
#endif
#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
	if (g_server_manager)
		g_server_manager->OnCookieRemoved(this);
#endif

	OP_DELETEA(portlist);
	portlist = NULL;

	g_cookieManager->DecCookiesCount();

#ifdef COOKIES_USE_UNREGISTER_FUNC
	void	UnregisterCookie(Cookie *);
    UnregisterCookie(this);
#endif // COOKIES_USE_UNREGISTER_FUNC
}

BOOL Cookie::CheckPort(unsigned short portnum)
{
	if (portlist && portlen > 0)
		if (op_bsearch(&portnum, portlist, portlen, sizeof(unsigned short), uint_cmp) == NULL)
			return FALSE;

	return TRUE;
}

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
#include "adjunct/quick/dialogs/ServerManagerDialog.h"

BOOL Cookie::SetName( const OpStringC8 &n )
{
	if(n.IsEmpty() )
	{
		return FALSE;
	}

	OpStringS8 n1;

	RETURN_VALUE_IF_ERROR(n1.Set(n), FALSE);

	name.TakeOver(n1);
	return TRUE;
}

BOOL Cookie::SetValue( const OpStringC8 &v )
{
	if(v.IsEmpty() )
	{
		return FALSE;
	}

	OpStringS8 v1;

	RETURN_VALUE_IF_ERROR(v1.Set(v), FALSE);

	value.TakeOver(v1);
	return TRUE;
}

void Cookie::BuildCookieEditorListL(CookieDomain* domain, CookiePath* path)
{
	Cookie *ck = this;
	while(ck)
	{
		if (g_server_manager)
			g_server_manager->OnCookieAdded(ck, domain, path);

		ck = ck->Suc();
	}
}
#endif //QUICK_COOKIE_EDITOR_SUPPORT

#ifdef NEED_URL_COOKIE_ARRAY
OP_STATUS Cookie::BuildCookieList(Cookie ** cookieArray, int * pos, CookieDomain *domainHolder, CookiePath * pathHolder, BOOL is_export)
{
	Cookie *ck = this;
	while(ck)
	{
		if (is_export)
		{
			if (last_used != last_sync)
			{
				if (last_sync == 0)
					flags.sync_added = TRUE;
				else
					flags.sync_added = FALSE;

				last_sync = last_used;

				if(cookieArray) cookieArray[*pos] = ck;
				(*pos)++;
				ck->domain = domainHolder;
				ck->path   = pathHolder;
			}
		}
		else
		{
			if(cookieArray) cookieArray[*pos] = ck;
			(*pos)++;
			ck->domain = domainHolder;
			ck->path   = pathHolder;
		}

		ck = ck->Suc();
	}

    return OpStatus::OK;
}
#endif // NEED_URL_COOKIE_ARRAY

#ifdef _OPERA_DEBUG_DOC_
void Cookie::GetMemUsed(DebugUrlMemory &debug)
{
	debug.number_cookies++;
	debug.memory_cookies += sizeof(*this);
	debug.memory_cookies += name.Length()+1;
	debug.memory_cookies += value.Length()+1;
	debug.memory_cookies += comment.Length()+1;
	debug.memory_cookies += commenturl.Length()+1;
	debug.memory_cookies += received_domain.Length()+1;
	debug.memory_cookies += received_path.Length()+1;
	debug.memory_cookies += port.Length()+1;
	if(portlist && portlen)
		debug.memory_cookies += portlen * sizeof(unsigned short);
}
#endif

#ifdef DISK_COOKIES_SUPPORT
void Cookie::FillDataFileRecordL(DataFile_Record &rec)
{
	rec.AddRecordL(TAG_COOKIE_NAME, Name()) ;
	rec.AddRecordL(TAG_COOKIE_VALUE,Value());
	OP_ASSERT(sizeof(OpFileLength) >= sizeof(time_t));
	rec.AddRecord64L(TAG_COOKIE_EXPIRES,(OpFileLength) Expires());
	rec.AddRecord64L(TAG_COOKIE_LAST_USED, (OpFileLength) GetLastUsed());
	rec.AddRecord64L(TAG_COOKIE_LAST_SYNC, (OpFileLength) GetLastSync());
	if(Secure())
		rec.AddRecordL(TAG_COOKIE_SECURE);
	if(HTTPOnly())
		rec.AddRecordL(TAG_COOKIE_HTTP_ONLY);
	if(FullPathOnly())
		rec.AddRecordL(TAG_COOKIE_NOT_FOR_PREFIX);
	if(ProtectedCookie())
		rec.AddRecordL(TAG_COOKIE_PROTECTED);
	if(SendOnlyToServer())
		rec.AddRecordL(TAG_COOKIE_ONLY_SERVER);
	if(Assigned())
		rec.AddRecordL(TAG_COOKIE_ASSIGNED);

	if(Version())
	{
		rec.AddRecordL(TAG_COOKIE_VERSION,Version());
		rec.AddRecordL(TAG_COOKIE_COMMENT, Comment());
		rec.AddRecordL(TAG_COOKIE_COMMENT_URL, CommentURL());
		rec.AddRecordL(TAG_COOKIE_RECVD_DOMAIN, Received_Domain());
		rec.AddRecordL(TAG_COOKIE_RECVD_PATH, Received_Path());
		rec.AddRecordL(TAG_COOKIE_PORT, PortReceived());
	}
	if(GetHavePassword())
		rec.AddRecordL(TAG_COOKIE_HAVE_PASSWORD);
	if(GetHaveAuthentication())
		rec.AddRecordL(TAG_COOKIE_HAVE_AUTHENTICATION);
	if(GetAcceptedAsThirdParty())
		rec.AddRecordL(TAG_COOKIE_ACCEPTED_AS_THIRDPARTY);
}
#endif // DISK_COOKIES_SUPPORT


