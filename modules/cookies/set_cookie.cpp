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

// Url_cm.cpp

// URL Cookie Management

#include "core/pch.h"

#include "modules/pi/OpSystemInfo.h"

#include "modules/url/url_man.h"
#include "modules/cookies/url_cm.h"


void Cookie_Manager::SetCookie(Cookie_Item_Handler *cookie_item)
{
	if(cookie_item == NULL)
		return;

	if(cookie_item->context_id != context_id && !GetContextIsTemporary(cookie_item->context_id))
	{
		CookieContextItem *ctx = FindContext(cookie_item->context_id);

		//cookie_item->context_id = 0; // Making sure it is set for the context
		if(ctx && ctx->context)
			ctx->context->SetCookie(cookie_item);

		return;
	}

	updated_cookies = TRUE;

	int len = cookie_item->domain.Length();
	int len2 = cookie_item->path.Length();

	RETURN_VOID_IF_ERROR(CheckCookieTempBuffers(len > len2 ? len : len2));

	char *domain = cookie_processing_buf1;
	char *path = cookie_processing_buf2;

	op_strcpy(domain, cookie_item->domain.CStr());
	if (len2) 
		op_strcpy(path, cookie_item->path.CStr());
	else
		*path = 0;

	CookieDomain* lowest_domain = 0;
	BOOL is_full_path = FALSE;
	
	CookiePath* OP_MEMORY_VAR cp = NULL;
	TRAPD(op_err, cp = cookie_root->GetCookiePathL(domain, path, 
		NULL, 
		TRUE, lowest_domain, is_full_path));
	RAISE_AND_RETURN_VOID_IF_ERROR(op_err);
	if (cp)
	{
		time_t this_time;
		this_time =  (time_t) (g_op_time_info->GetTimeUTC()/1000.0);
		cookie_item->flags.full_path_only = is_full_path;
		Cookie  * OP_MEMORY_VAR tempcookie =  NULL;
		TRAP(op_err, tempcookie = cp->GetCookieL(cookie_item, !cookie_item->flags.delete_cookie));
		RAISE_AND_RETURN_VOID_IF_ERROR(op_err);
		if(tempcookie && (cookie_item->flags.delete_cookie || (tempcookie->Expires() && tempcookie->Expires() < this_time)))
		{
			OP_DELETE(tempcookie);
			tempcookie = NULL;
		}

#ifdef WIC_COOKIES_LISTENER
		if (tempcookie && g_cookie_API)
			g_cookie_API->GetCookieIteratorListener()->OnSetCookie(*tempcookie);
#endif // WIC_COOKIES_LISTENER

		// check limits
		if (lowest_domain)
		{
			// check number of cookies for this domain and total
			int ccount = lowest_domain->GetCookiesCount();
			if (cookies_count > max_cookies || ccount > GetMaxCookiesInDomain())
			{
				// find least recently used cookie and delete expired
				if(tempcookie)
					tempcookie->SetLastUsed(this_time+1);
				Cookie* ck = NULL;

				if(ccount > GetMaxCookiesInDomain())
					ck = lowest_domain->GetLeastRecentlyUsed(this_time, this_time, TRUE);

				if(!ck)
					ck = cookie_root->GetLeastRecentlyUsed(this_time, this_time);

				// delete if still necessary
				if (ck && (cookies_count > max_cookies || lowest_domain->GetCookiesCount() > GetMaxCookiesInDomain()))
				{
					OP_DELETE(ck);
				}

				if(tempcookie)
					tempcookie->SetLastUsed(0);
			}
		}
	}
#ifdef COOKIES_IMMEDIATE_AUTO_SAVE
	TRAP(op_err, this->WriteCookiesL());
    OpStatus::Ignore(op_err);//FIXME:OOM-QNX
#endif

#ifdef _DEBUG_COOKIES_
			FILE* fp = fopen("c:\\klient\\dcookie.txt", "a");
			if (fp)
			{
				cookie_root->DebugWriteCookies(fp);
				fclose(fp);
			}
#endif
}

