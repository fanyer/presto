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

// Url_cm.cpp

// URL Cookie Management

#include "core/pch.h"

#include "modules/url/url_man.h"
#include "modules/cookies/url_cm.h"
#include "modules/cookies/cookie_common.h"

#ifdef _SSL_USE_SMARTCARD_
void Cookie_Manager::CleanSmartCardAuthenticatedCookies(ServerName *domain_sn)
{
	if(!domain_sn || !domain_sn->Name() || !(*domain_sn->Name()))
		return;

	char *domain_name = (char *) domain_sn->Name();

	if(!domain_name || !(*domain_name))
		return;

	if(op_strchr(domain_name,'.') == NULL)
	{
		if(op_strlen(domain_name) + 10 > g_memory_manager->GetTempBuf2kLen())
			return;

		domain_name = (char *) g_memory_manager->GetTempBuf2k();

		domain_name[0] = '\0';
		StrMultiCat(domain_name, domain_sn->Name(),".local");
	}
	
	CookieDomain* lowest_domain = 0;
	BOOL is_full_path = FALSE;
	
	CookiePath* cp = NULL;
	TRAPD(op_err, cp = cookie_root->GetCookiePathL(domain_name, NULL, 
		NULL, 
		FALSE, lowest_domain, is_full_path));
	OpStatus::Ignore(op_err); // No allocation in this call

	while(lowest_domain)
	{
		if(cp)
		{
			// Ignore return value
			cp->CleanSmartCardAuthenticatedCookies();
		}

		lowest_domain = lowest_domain->Parent();
		if(lowest_domain)
			cp = lowest_domain->GetPathRoot();
	}
}
#endif

void Cookie_Manager::ClearCookiesCommitL(BOOL delete_filters)
{
	CheckCookiesReadL();

	CookieContextItem *manager = (CookieContextItem *) ContextManagers.First();

	while(manager)
	{
		if(manager->context)
		{
			manager->context->cookie_root->DeleteAllCookies( delete_filters );
		}
		manager = (CookieContextItem *) manager->Suc();
	}

	cookie_root->DeleteAllCookies( delete_filters );
	cookies_count = 0;
#ifdef DISK_COOKIES_SUPPORT
	cookie_file_read = TRUE;
	WriteCookiesL();
#endif
}

void Cookie_Manager::CleanNonPersistenCookies()
{
	cookie_root->RemoveNonPersistentCookies();
}

void Cookie_Manager::FreeUnusedResources()
{
	CookieContextItem *manager = (CookieContextItem *) ContextManagers.First();

	while(manager)
	{
		if(manager->context)
		{
			manager->context->FreeUnusedResources();
		}
		manager = (CookieContextItem *) manager->Suc();
	}

	cookie_root->FreeUnusedResources();
}


