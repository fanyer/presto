/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */

// Url_cm.cpp

// URL Cookie Management

#include "core/pch.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/cookies/url_cm.h"
#include "modules/cookies/cookie_common.h"


/** Helper function to avoid setjmp/longjmp warnings about trampled local variables.
    Will return either cp or a better match under cp if the path is found. See
    CookiePath::GetCookiePathL for full documentation. */
static CookiePath *GetCookiePathNotCreate(CookiePath *cp, const char* path, BOOL& is_full_path)
{
	TRAPD(status, cp = cp->GetCookiePathL(path, FALSE, is_full_path));
	OP_ASSERT(OpStatus::IsSuccess(status) || !"GetCookiePathL() can't LEAVE when create=FALSE");
	return cp;
}

const char*	Cookie_Manager::GetCookiesL(URL_Rep *url, 
									int& version, int &max_version,
									BOOL already_have_password,
									BOOL already_have_authentication,
									BOOL &have_password,
									BOOL &have_authentication,
									URL_CONTEXT_ID context_id,
									BOOL for_http
									)
{
	if(context_id && !GetContextIsTemporary(context_id))
	{
		CookieContextItem *ctx = FindContext(context_id);

		if(ctx && ctx->context
#ifdef COOKIES_CONTROL_PER_CONTEXT
			&& ctx->context->cookies_enabled_for_context
#endif
			)
		{
			return ctx->context->GetCookiesL(url, version, max_version,
									already_have_password,
									already_have_authentication,
									have_password,
									have_authentication,
									0,
									for_http);
		}

		return NULL;
	}

    BOOL is_secure; // 10/09/97 YNP
#ifdef _ASK_COOKIE
	ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	COOKIE_MODES cmode_site = (server ? server->GetAcceptCookies(TRUE) :COOKIE_DEFAULT);
#endif
	COOKIE_MODES cmode_global = (COOKIE_MODES) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);
	OP_MEMORY_VAR COOKIE_MODES cmode;
	
#ifdef _ASK_COOKIE
	cmode = cmode_site;

	if(cmode == COOKIE_DEFAULT)
#endif
		cmode = cmode_global;

	if (cmode != COOKIE_NONE && url &&
		((URLType) url->GetAttribute(URL::KType) == URL_HTTP || (URLType) url->GetAttribute(URL::KType) == URL_HTTPS
#ifdef __FILE_COOKIES_SUPPORTED
		|| (URLType) url->GetAttribute(URL::KType) == URL_FILE
#endif
#ifdef WEBSOCKETS_SUPPORT
		|| (URLType) url->GetAttribute(URL::KType) == URL_WEBSOCKET || (URLType) url->GetAttribute(URL::KType) == URL_WEBSOCKET_SECURE
#endif // WEBSOCKETS_SUPPORT
		))
	{
#ifdef DISK_COOKIES_SUPPORT
		if (!cookie_file_read)
		{
			ReadCookiesL();
		}
#endif // DISK_COOKIES_SUPPORT

		OpStringC8 hostname = url->GetAttribute(URL::KHostName);

		if(hostname.IsEmpty())
			return NULL;

		RETURN_VALUE_IF_ERROR(CheckCookieTempBuffers(hostname.Length()+50), NULL); // A little extra

		char *domain = cookie_processing_buf1;

		op_strcpy(domain, hostname.CStr());

		BOOL isFileURL = (URLType) url->GetAttribute(URL::KType) == URL_FILE;
		LEAVE_IF_ERROR(CheckLocalNetworkAndAppendDomain(domain, isFileURL));

		ServerName *actual_server = g_url_api->GetServerName(domain, TRUE);

#ifdef _ASK_COOKIE
		COOKIE_MODES scmode = COOKIE_DEFAULT;
		OP_MEMORY_VAR BOOL third_party_only=FALSE;

		if(url->GetAttribute(URL::KIsThirdParty) && 
				cmode == COOKIE_WARN_THIRD_PARTY)
				third_party_only = TRUE;

		if(actual_server)
		{
			scmode = actual_server->GetAcceptCookies(TRUE);
			if(cmode == COOKIE_WARN_THIRD_PARTY)
			{
				if(scmode != COOKIE_DEFAULT && scmode != COOKIE_ALL)
					return NULL;
			}
			else if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DisplayReceivedCookies) && 
				scmode != COOKIE_ALL && scmode != COOKIE_DEFAULT)
				return NULL;

			COOKIE_MODES tcmode = actual_server->GetAcceptThirdPartyCookies(TRUE);
			if(url->GetAttribute(URL::KIsThirdParty) && 
				tcmode != COOKIE_DEFAULT && 
				tcmode != COOKIE_ACCEPT_THIRD_PARTY)
				return NULL;
		}
#else // !_ASK_COOKIE
		OP_MEMORY_VAR BOOL third_party_only=FALSE;

		if(url->GetAttribute(URL::KIsThirdParty) && 
				cmode == COOKIE_WARN_THIRD_PARTY)
				third_party_only = TRUE;

		if(actual_server)
		{
			COOKIE_MODES tcmode = actual_server->GetAcceptThirdPartyCookies(TRUE);
			if(tcmode != COOKIE_DEFAULT && 
				tcmode != COOKIE_ACCEPT_THIRD_PARTY)
				return NULL;
		}
#endif // else !_ASK_COOKIE

		is_secure = ((URLType) url->GetAttribute(URL::KType) == URL_HTTPS)
#ifdef WEBSOCKETS_SUPPORT
				 || ((URLType) url->GetAttribute(URL::KType) == URL_WEBSOCKET_SECURE)
#endif //WEBSOCKETS_SUPPORT
			;

		OpString8 temp_path;
		ANCHOR(OpString8, temp_path);

		url->GetAttributeL(URL::KPath, temp_path);

		const char * OP_MEMORY_VAR path = (temp_path.HasContent() ? temp_path.CStr() : "");
		OP_MEMORY_VAR unsigned short port;

		if(*path == '/')
			path++;

		port = (unsigned short) url->GetAttribute(URL::KResolvedPort);
		
		OP_MEMORY_VAR int cookie_len;
		CookieDomain* cd;
		BOOL is_full_path;
		CookiePath* OP_MEMORY_VAR cp = NULL;
		
		OP_MEMORY_VAR BOOL allow_dollar_cookies = TRUE;
		BOOL is_server;
	restart_fetching:
		is_server = TRUE;
		max_version = 0;
		version = INT_MAX;
		have_password = FALSE;
		have_authentication = FALSE;
		cookie_len = 0;
		is_full_path = FALSE;

		cp = cookie_root->GetCookiePathL(domain, path,
			&is_server,
			FALSE, cd, is_full_path);

		char* cookie_buffer = g_cookie_header_buffer;
		if (cp)
		{
			time_t this_time;
			this_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);
			
			cookie_buffer[0] = '\0';

			while (cp)
			{
				BOOL seen_cookie2 = FALSE;
				int this_cookie_len = cp->GetCookieRequest(this_time, is_secure,
					is_server, port, version, max_version, 
					third_party_only,
					already_have_password,
					already_have_authentication,
					have_password,
					have_authentication,
					is_full_path,
					cookie_buffer + cookie_len, COOKIE_BUFFER_SIZE - cookie_len,
					for_http, allow_dollar_cookies, seen_cookie2);

				// if a Cookie2-cookie is detected: restart,
				// discarding any cookies starting with '$' - see
				// CORE-15647
				if (allow_dollar_cookies && seen_cookie2)
				{
					allow_dollar_cookies = FALSE;
					goto restart_fetching;
				}

				if (this_cookie_len)
				{
					cookie_len += this_cookie_len;
					if (cookie_len + 2 < COOKIE_BUFFER_SIZE)
					{
						op_strcpy(cookie_buffer + cookie_len, "; ");
						cookie_len += 2;
					}
				}

				CookiePath *cp1 = cp->GetNextPrefix();
				if(cp1)
				{
					cp = cp1;
					is_full_path = FALSE;
					continue;
				}
				is_full_path = TRUE;
				cp = cp->Parent();
				if (!cp && cd)
				{
					is_server = FALSE;

					cd = cd->Parent();
					if (cd)
					{
						cp = cd->GetPathRoot();
						if (cp)
						{
							is_full_path = FALSE;
							cp = GetCookiePathNotCreate(cp, path, is_full_path);
						}
					}
				}
			}
		}

		if (cookie_len > 2)
		{
			// remove "; " at end
			cookie_buffer[cookie_len-2] = '\0';
#ifdef _DEBUG_COOKIES_
			FILE* fp = fopen("c:\\klient\\dcookie.txt", "a");
			if (fp)
			{
				fprintf(fp, "GetCookie: url_name=%s, cookie=%s\n", url->Name(), cookie_buffer);
				fclose(fp);
			}
#endif
			return cookie_buffer;
		}
#ifdef _DEBUG_COOKIES_
		else
		{
			FILE* fp = fopen("c:\\klient\\dcookie.txt", "a");
			if (fp)
			{
				fprintf(fp, "GetCookie: url_name=%s, cookie=NULL\n", url->Name());
				fclose(fp);
			}
		}
#endif
	}

	return 0;

}

