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

#include "core/pch.h"

#ifdef _ENABLE_AUTHENTICATE

#include "modules/auth/auth_sn.h"
#include "modules/auth/auth_elm.h"
#include "modules/auth/pre_auth.h"

#include "modules/url/url_man.h"
#include "modules/cookies/url_cm.h"


ServerNameAuthenticationManager::ServerNameAuthenticationManager()
{
#if defined(_USE_PREAUTHENTICATION_) 
	auth_paths_http = NULL;
	auth_paths_https = NULL;
#endif
}

ServerNameAuthenticationManager::~ServerNameAuthenticationManager()
{
#ifdef _USE_PREAUTHENTICATION_
	if(auth_paths_http)
		auth_paths_http->Clear();
	if(auth_paths_https)
		auth_paths_https->Clear();
	OP_DELETE(auth_paths_http);
	OP_DELETE(auth_paths_https);
#endif
}

void ServerNameAuthenticationManager::RemoveSensitiveData()
{
	username.Empty();
	proxyusername.Empty();
}

BOOL ServerNameAuthenticationManager::SafeToDelete()
{
	if (
		authenticate_proxy.Empty() &&
#ifndef NO_FTP_SUPPORT
		authenticate_ftp.Empty() &&
#endif // !NO_FTP_SUPPORT
		authenticate_http.Empty() &&
		authenticate_https.Empty())
	{
		return TRUE;
	}
	return FALSE;
}

#ifndef AUTH_RESTRICTED_USERNAME_STORAGE
OP_STATUS ServerNameAuthenticationManager::SetLastUserName(const OpStringC &nam)
{
	return username.Set(nam);
}

OP_STATUS ServerNameAuthenticationManager::SetLastProxyUserName(const OpStringC &nam)
{
	return proxyusername.Set(nam);
}
#endif

void ServerNameAuthenticationManager::Add_Auth(AuthElm *authelm, const OpStringC8 &path)
{
	if(!authelm)
		return;

	Head *auth_list;
	AuthScheme schm =authelm->GetScheme();
	URLType typ = authelm->GetUrlType();

	if(authelm->GetContextId() && urlManager->GetContextIsTemporary(authelm->GetContextId()))
		authelm->SetContextId(0);

#ifndef NO_FTP_SUPPORT
	if(schm & AUTH_SCHEME_FTP)
		auth_list = &authenticate_ftp;
	else
#endif // NO_FTP_SUPPORT
	if(schm & AUTH_SCHEME_HTTP_PROXY)
		auth_list = &authenticate_proxy;
	else if(typ == URL_HTTPS)
		auth_list = &authenticate_https;
	else
		auth_list = &authenticate_http;

	AuthElm *elm = (AuthElm *) auth_list->First();
	const char *realm = authelm->GetRealm();
	unsigned port = authelm->GetPort();
	unsigned long is_alias = authelm->IsAlias();
	while(elm)
	{
		if(elm == authelm)
			break;
		if((elm->GetScheme() == schm 
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
				|| (((schm & AUTH_SCHEME_HTTP_MASK) == AUTH_SCHEME_HTTP_NTLM_NEGOTIATE) &&
					(((elm->GetScheme() & AUTH_SCHEME_HTTP_MASK) == AUTH_SCHEME_HTTP_NEGOTIATE) ||
					((elm->GetScheme() & AUTH_SCHEME_HTTP_MASK) == AUTH_SCHEME_HTTP_NTLM)) &&
					(elm->GetScheme() & AUTH_SCHEME_HTTP_PROXY) == (schm &AUTH_SCHEME_HTTP_PROXY)
				)
#endif
			
			) && elm->GetPort() == port
			&& authelm->GetContextId() == elm->GetContextId()
			)
		{
			const char *r2 = elm->GetRealm();
			if(((schm & (AUTH_SCHEME_FTP)) != 0 && ((!r2 && !realm) ||
					(r2 && realm && op_strcmp(realm, r2) == 0))) ||
				(elm->GetUrlType() == typ && r2 && realm && op_strcmp(realm, r2) == 0))
			{
				if(is_alias && elm->GetId() == is_alias)
					break; // Already present in list, no need to add another alias//FIXME:OOM7 - When is authelm deleted if we break here?
				AuthElm *elm1 = elm;
				elm = elm->Suc();
				OP_DELETE(elm1);
				continue;
			}
		}
		elm = elm->Suc();
	}
	if(!elm)
		authelm->Into(auth_list);
#ifdef _USE_PREAUTHENTICATION_
	if(realm && ((typ == URL_HTTP || typ == URL_HTTPS)  &&
		(schm & (AUTH_SCHEME_FTP | AUTH_SCHEME_HTTP_PROXY)) == 0 ))
	{
		OP_STATUS op_err = OpStatus::OK;

		if(!auth_paths_http)
		{
			auth_paths_http = CookiePath::Create(NULL, op_err);
			if(OpStatus::IsError(op_err))
				return;
		}
		if(!auth_paths_https)
		{
			auth_paths_https = CookiePath::Create(NULL, op_err);
			if(OpStatus::IsError(op_err))
				return;
		}
		CookiePath *autpath = (typ == URL_HTTPS ? auth_paths_https : auth_paths_http);
		const char *path2 = path.CStr();
		char *path_end=NULL;
		char path_tmp=0;

		if(path2 && *path2 == '/')
			path2++;

		if(path2)
			path_end = (char*) op_strpbrk(path2,"?&");
		if(path_end)
		{
			path_tmp = *path_end;
			*path_end = '\0';
		}

		if((schm & AUTH_SCHEME_HTTP_MASK) != AUTH_SCHEME_HTTP_DIGEST)
		{
			char *tmp = (path2 ? (char*) op_strrchr(path2,'/') : NULL);
			if(tmp)
			{
				if(path_end)
					*path_end = path_tmp;

				path_end = tmp;
				path_tmp = *path_end;
				*path_end = '\0';
			}
			else
			{
				if(path_end)
					*path_end = path_tmp;
				path_end = NULL;
				path2 = NULL;
			}
		}

		autpath = autpath->GetAuthenticationPath(path2 ? path2 :"", TRUE);//FIXME:OOM7 - Can't return error code.
		if(path_end)
			*path_end = path_tmp;
		if(autpath)
		{
			autpath->GetAuthenticationCookie(port,realm, schm,typ, TRUE
				, authelm->GetContextId()
				);//FIXME:OOM7 - Can't return error code.
		}

	}
#endif // _USE_PREAUTHENTICATION_
}

AuthElm *ServerNameAuthenticationManager::Get_Auth(const char *realm, unsigned short port,
							  const char *path,
							  URLType typ, AuthScheme schm
							  , URL_CONTEXT_ID a_context_id
							  )
{
	OP_MEMORY_VAR URL_CONTEXT_ID context_id = a_context_id;
	Head *auth_list;

	if(context_id && urlManager->GetContextIsTemporary(context_id))
		context_id = 0;

#ifdef _USE_PREAUTHENTICATION_
	if(realm == NULL && ((typ == URL_HTTP || typ == URL_HTTPS) &&
		(schm & (AUTH_SCHEME_FTP | AUTH_SCHEME_HTTP_PROXY)) == 0 ) )
	{
		realm = ResolveRealm(port, path, typ, schm, context_id);
		if (!realm)
			return NULL;
	}
#endif // _USE_PREAUTHENTICATION_

#ifndef NO_FTP_SUPPORT
	if(schm & AUTH_SCHEME_FTP)
		auth_list = &authenticate_ftp;
	else
#endif // NO_FTP_SUPPORT
	if(schm & AUTH_SCHEME_HTTP_PROXY)
		auth_list = &authenticate_proxy;
	else if(typ == URL_HTTPS)
		auth_list = &authenticate_https;
	else
		auth_list = &authenticate_http;

	AuthElm *elm = (AuthElm *) auth_list->First();
	while(elm)
	{
		AuthScheme elm_schm = elm->GetScheme();
		if((elm_schm == schm  ||
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
			// NTLM and Negotiate combination devolve to one of them after init
			((schm & AUTH_SCHEME_HTTP_MASK) == AUTH_SCHEME_HTTP_NTLM_NEGOTIATE && 
			  ((elm_schm & AUTH_SCHEME_HTTP_MASK) == AUTH_SCHEME_HTTP_NTLM ||
			   (elm_schm & AUTH_SCHEME_HTTP_MASK) == AUTH_SCHEME_HTTP_NEGOTIATE) &&
			   (elm_schm & AUTH_SCHEME_HTTP_PROXY) == (schm & AUTH_SCHEME_HTTP_PROXY)) ||
#endif
			(schm == AUTH_SCHEME_HTTP_UNKNOWN && (elm_schm & ~AUTH_SCHEME_HTTP_MASK) == 0) ||
			(schm == AUTH_SCHEME_HTTP_PROXY && (elm_schm & AUTH_SCHEME_HTTP_PROXY) != 0))
			&& elm->GetPort() == port
			&& elm->GetContextId() == context_id
			)
		{
			const char *r2 = elm->GetRealm();
			if((schm & (AUTH_SCHEME_FTP)) != 0 && ((!r2 && !realm) ||
					(r2 && realm && op_strcmp(realm, r2) == 0)))
			{
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
				if(elm->MustClone())
					return elm->MakeClone();
#endif
				return elm;
			}
			else if(((schm & AUTH_SCHEME_HTTP_PROXY) == 0 && elm->GetUrlType() == typ &&
					r2 && realm && op_strcmp(realm, r2) == 0))
			{
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
				if(elm->MustClone())
					return elm->MakeClone();
#endif
				return elm;
			}
			else if((schm & AUTH_SCHEME_HTTP_PROXY) != 0 &&
				(!realm || (r2 && op_strcmp(realm, r2) == 0)))
			{
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
				if(elm->MustClone())
					return elm->MakeClone();
#endif
				return elm;
			}
		}
		elm = elm->Suc();
	}

	return NULL;
}

#ifdef _USE_PREAUTHENTICATION_
const char *ServerNameAuthenticationManager::ResolveRealm(unsigned short port, const char *path, URLType typ,  AuthScheme schm, URL_CONTEXT_ID context_id) const
{
		OP_STATUS op_err = OpStatus::OK;
		
		// In multi-threading environment the code between BEGIN and END must be corectly locked.
		// BEGIN LOCK HERE
		ServerNameAuthenticationManager* non_const_this = const_cast <ServerNameAuthenticationManager*> (this);
		if(!auth_paths_http)
		{
			non_const_this->auth_paths_http = CookiePath::Create(NULL, op_err);
			if(OpStatus::IsError(op_err))
				return NULL;
		}
		if(!auth_paths_https)
		{
			non_const_this->auth_paths_https = CookiePath::Create(NULL, op_err);
			if(OpStatus::IsError(op_err))
				return NULL;
		}
		// END LOCK HERE
		
		CookiePath *autpath = (typ == URL_HTTPS ? auth_paths_https : auth_paths_http);
		AuthenticationCookie *auth_item=NULL;
		const char *path2 = path;
		char *path_end=NULL;
		char path_tmp=0;

		if(path2 && *path2 == '/')
			path2++;

		path_end = path2 ? (char*) op_strpbrk(path2,"?&;") : NULL;
		if(path_end)
		{
			path_tmp = *path_end;
			*path_end = '\0';
		}

		char *tmp = path2 ? (char*) op_strrchr(path2,'/') : NULL;
		if(tmp)
		{
			if(path_end)
				*path_end = path_tmp;

			path_end = tmp;
			path_tmp = *path_end;
			*path_end = '\0';
		}
		else
		{
			if(path_end)
				*path_end = path_tmp;
			path_end = NULL;
			path2 = NULL;
		}

		autpath = autpath->GetAuthenticationPath(path2 ? path2 :"", FALSE);
		if(path_end)
			*path_end = path_tmp;
		while(autpath)
		{
			auth_item = autpath->GetAuthenticationCookie(port,schm,typ, context_id);
			if(auth_item)
				break;
			autpath = autpath->Parent();
		}

		if(auth_item)
			return auth_item->Realm();
		else
			return NULL;
}
#endif // _USE_PREAUTHENTICATION_

#ifdef HTTP_DIGEST_AUTH

OP_STATUS ServerNameAuthenticationManager::Update_Authenticate(unsigned long aid, ParameterList *header)
{
	AuthElm *elm = (AuthElm *) authenticate_http.First();
	while(elm)
	{
		if(elm->GetId() == aid)
			return elm->Update_Authenticate(header);
		elm = elm->Suc();
	}

	elm = (AuthElm *) authenticate_https.First();
	while(elm)
	{
		if(elm->GetId() == aid)
			return elm->Update_Authenticate(header);
		elm = elm->Suc();
	}

	elm = (AuthElm *) authenticate_proxy.First();
	while(elm)
	{
		if(elm->GetId() == aid)
			return elm->Update_Authenticate(header);
		elm = elm->Suc();
	}
	return OpStatus::OK;
}
#endif // HTTP_DIGEST_AUTH

void ServerNameAuthenticationManager::Clear_Authentication_List()
{
	authenticate_proxy.Clear();
#ifndef NO_FTP_SUPPORT
	authenticate_ftp.Clear();
#endif // NO_FTP_SUPPORT
	authenticate_http.Clear();
	authenticate_https.Clear();
}



#ifdef _OPERA_DEBUG_DOC_
void ServerNameAuthenticationManager::GetMemUsed(DebugUrlMemory &debug)
{
	debug.memory_servernames += username.Capacity();
	debug.memory_servernames += proxyusername.Capacity();
}
#endif

#endif
