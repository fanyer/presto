/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_COOKIE_MANAGER

#include "modules/scope/src/scope_transport.h"
#include "modules/scope/src/scope_cookie_manager.h"
#include "modules/url/url_man.h"
#include "modules/cookies/url_cm.h"

/* OpScopeCookieManager */

OpScopeCookieManager::OpScopeCookieManager()
	: OpScopeCookieManager_SI()
{
}

/* virtual */
OpScopeCookieManager::~OpScopeCookieManager()
{
}

// Cookies

OP_STATUS
OpScopeCookieManager::DoGetCookie(const GetCookieArg &in, CookieList &out)
{
	OpString in_domain;
	RETURN_IF_ERROR(in_domain.Set(in.GetDomain()));

	RETURN_IF_ERROR(Cookie_Manager::CheckLocalNetworkAndAppendDomain(in_domain));

	// we need non-const strings for the URL API
	OpAutoArray<uni_char> domain_copy( OP_NEWA(uni_char, in_domain.Length() + 1) );
	RETURN_OOM_IF_NULL(domain_copy.get());
	uni_strcpy(domain_copy.get(), in_domain.CStr());
	uni_char *domain = domain_copy.get();
	uni_char *path = NULL;

	OpAutoArray<uni_char> path_copy( in.HasPath() ? OP_NEWA(uni_char, in.GetPath().Length() + 1) : NULL );
	if (in.HasPath())
	{
		RETURN_OOM_IF_NULL(path_copy.get());
		uni_strcpy(path_copy.get(), in.GetPath().CStr());
		path = path_copy.get();
	}

	OpAutoArray< ::Cookie * > cookies( OP_NEWA(::Cookie *, urlManager->GetMaxCookiesInDomain()) );
	RETURN_OOM_IF_NULL(cookies.get());
	int cookie_size = 0;
	OP_STATUS status = g_url_api->BuildCookieList(cookies.get(), &cookie_size, domain, path, TRUE);
	if (OpStatus::IsError(status))
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Could not get cookies for specified domain and path"));

	for (int i = 0; i < cookie_size; ++i)
	{
		Cookie *cookie_out = out.AppendNewCookieList();
		RETURN_OOM_IF_NULL(cookie_out);

		::Cookie *cookie = cookies[i];
		if (!cookie || !(cookie->Received_Domain().CStr() || (cookie->GetDomain() && cookie->GetDomain()->GetFullDomain())))
		{
			OP_ASSERT(cookie && cookie->GetDomain() && cookie->GetDomain()->GetFullDomain());
			return OpStatus::ERR_NULL_POINTER;
		}

		RETURN_IF_ERROR(SetCookieValue(*cookie_out, *cookie));
	}
	return OpStatus::OK;
}

OP_STATUS
OpScopeCookieManager::DoRemoveCookie(const RemoveCookieArg &in)
{
	OpString in_domain;
	RETURN_IF_ERROR(in_domain.Set(in.GetDomain()));
	RETURN_IF_ERROR(Cookie_Manager::CheckLocalNetworkAndAppendDomain(in_domain));

	// we need non-const strings for the URL API
	uni_char *domain = NULL;
	OpAutoArray<uni_char> domain_copy( OP_NEWA(uni_char, in_domain.Length() + 1) );
	{
		RETURN_OOM_IF_NULL(domain_copy.get());
		uni_strcpy(domain_copy.get(), in_domain.CStr());
		domain = domain_copy.get();
	}
	uni_char *path = NULL;
	OpAutoArray<uni_char> path_copy( in.HasPath() ? OP_NEWA(uni_char, in.GetPath().Length() + 1) : NULL );
	if (in.HasPath())
	{
		RETURN_OOM_IF_NULL(path_copy.get());
		uni_strcpy(path_copy.get(), in.GetPath().CStr());
		path = path_copy.get();
	}
	uni_char *name = NULL;
	OpAutoArray<uni_char> name_copy( in.HasName() ? OP_NEWA(uni_char, in.GetName().Length() + 1) : NULL );
	if (in.HasName())
	{
		RETURN_OOM_IF_NULL(name_copy.get());
		uni_strcpy(name_copy.get(), in.GetName().CStr());
		name = name_copy.get();
	}

	OP_STATUS status = g_url_api->RemoveCookieList(domain, path, name);
	if (OpStatus::IsError(status))
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Failure while removing cookies for specified domain, path and name"));
	return OpStatus::OK;
}

OP_STATUS
OpScopeCookieManager::DoRemoveAllCookies()
{
	OP_STATUS status = g_url_api->RemoveCookieList(NULL, NULL, NULL);
	if (OpStatus::IsError(status))
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Failure while removing all cookies"));
	return OpStatus::OK;
}

OP_STATUS
OpScopeCookieManager::DoGetCookieSettings(CookieSettings &out)
{
	out.SetMaxCookies(urlManager->GetMaxCookies());
	out.SetMaxCookiesPerDomain(urlManager->GetMaxCookiesInDomain());
	out.SetMaxCookieLength(urlManager->GetMaxCookieLen());
	return OpStatus::OK;
}

OP_STATUS OpScopeCookieManager::SetCookieValue(Cookie& cookie_out, ::Cookie& cookie)
{
	RETURN_IF_ERROR( cookie_out.SetDomain(cookie.GetDomain()->GetFullDomain()->GetUniName()) );

	RETURN_IF_ERROR( cookie_out.SetName(cookie.Name()) );
	RETURN_IF_ERROR( cookie_out.SetValue(cookie.Value()) );

	CookiePath* p = cookie.GetPath();
	OpString8 fullpath;
	RETURN_IF_ERROR(p->GetFullPath(fullpath));
	RETURN_IF_ERROR( cookie_out.SetPath(fullpath.CStr()) );
	
	cookie_out.SetIsHTTPOnly(cookie.HTTPOnly());
	cookie_out.SetIsSecure(cookie.Secure());
	UINT64 expires = cookie.Expires();
	// TODO: The expires value should be sent as a uint64 field.
	cookie_out.SetExpires(static_cast<UINT32>(expires));
	return OpStatus::OK;
}

OP_STATUS
OpScopeCookieManager::DoAddCookie(const AddCookieArg &in)
{
#ifdef URL_ENABLE_EXT_COOKIE_ITEM
	OpAutoPtr<Cookie_Item_Handler> cookie(OP_NEW(Cookie_Item_Handler, ()));
	RETURN_OOM_IF_NULL(cookie.get());

	if (in.GetDomain().IsEmpty() || in.GetName().IsEmpty())		//don't allow empty name or domain
		return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Empty required values are not allowed"));

	OpString in_domain;
	RETURN_IF_ERROR(in_domain.Set(in.GetDomain()));
	RETURN_IF_ERROR(Cookie_Manager::CheckLocalNetworkAndAppendDomain(in_domain));

	RETURN_IF_ERROR(cookie->domain.Set(in_domain.CStr()));

	RETURN_IF_ERROR(cookie->name.Set(in.GetName()));

	unsigned int pos = (in.GetPath().Compare("/", 1) == 0) ? 1 : 0;// skip the root slash if any

	RETURN_IF_ERROR(cookie->path.Set(in.GetPath().SubString(pos)));

	cookie->flags.recv_path = TRUE;
	RETURN_IF_ERROR(cookie->recvd_path.Set(in.GetPath()));	// recvd_path gets the full path

	if (in.HasValue())
		RETURN_IF_ERROR(cookie->value.Set(in.GetValue()));

	if (in.HasExpires())
		cookie->expire = in.GetExpires();

	cookie->flags.secure = in.GetIsSecure();
	cookie->flags.httponly = in.GetIsHTTPOnly();

	g_url_api->SetCookie(cookie.get());

	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif //URL_ENABLE_EXT_COOKIE_ITEM
}

#endif // SCOPE_COOKIE_MANAGER
