/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software AS.  All rights reserved.
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
#include "modules/url/url_rep.h"
#include "modules/cookies/url_cm.h"

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)
#include "adjunct/quick/dialogs/ServerManagerDialog.h"
#endif

#if defined(QUICK_COOKIE_EDITOR_SUPPORT)

#include "adjunct/quick/dialogs/ServerManagerDialog.h"

void Cookie_Manager::BuildCookieEditorListL(URL_CONTEXT_ID context_id)
{
	if(context_id)
	{
		Cookie_Manager::CookieContextItem *ctx = FindContext(context_id);

		if(ctx && ctx->context)
			ctx->context->BuildCookieEditorListL(0);

		return;
	}
#ifdef DISK_COOKIES_SUPPORT
	if( !cookie_file_read )
	{
		ReadCookiesL();
	}
#endif // DISK_COOKIES_SUPPORT
	if( cookie_root )
	{
		cookie_root->BuildCookieEditorListL();
	}
}

#endif //QUICK_COOKIE_EDITOR_SUPPORT

#ifdef NEED_URL_COOKIE_ARRAY
OP_STATUS Cookie_Manager::BuildCookieList(
                               Cookie ** cookieArray,
                               int * size_returned,
                               URL_Rep *url
							   )
{
#ifdef DISK_COOKIES_SUPPORT
	if( !cookie_file_read )
	{
        OP_STATUS op_err;
	    TRAP(op_err,ReadCookiesL());  
        RETURN_IF_ERROR(op_err);
	}
#endif // DISK_COOKIES_SUPPORT

	OpStringC8 hostname = url->GetAttribute(URL::KHostName);
	OpStringC8 pathbase = url->GetAttribute(URL::KPath);

	if(hostname.IsEmpty())
        return OpStatus::ERR;

	RETURN_IF_ERROR(CheckCookieTempBuffers(MAX(hostname.Length(), pathbase.Length())+50)); // A little extra

    char *domain = cookie_processing_buf1;
	op_strcpy(domain, hostname.CStr());

	char *path = cookie_processing_buf2;
	op_strcpy(path,(pathbase.HasContent() ? pathbase.CStr() : ""));
	if(*path == '/')
		path++;

	/* // Ignore ";" parameters
	int templen = op_strcspn(path, ";");
	path[templen] = '\0';
	*/

	if( cookie_root )
	{
        * size_returned = 0;
		cookie_root->BuildCookieList(cookieArray, size_returned, domain, path, FALSE, FALSE);
	}

    return OpStatus::OK;
}

OP_STATUS Cookie_Manager::BuildCookieList(
                               Cookie ** cookieArray,
                               int * size_returned,
                               uni_char * domainstr,
                               uni_char * pathstr,
                               BOOL is_export /*= FALSE*/,
                               BOOL match_subpaths /*= FALSE*/)
{
    char     *domain;
    char     *path;
    OpString8 pathTmp;

#ifdef DISK_COOKIES_SUPPORT
	if( !cookie_file_read )
	{
        OP_STATUS op_err;
	    TRAP(op_err,ReadCookiesL()); 
        RETURN_IF_ERROR(op_err);
	}
#endif // DISK_COOKIES_SUPPORT

    if(domainstr)
    {
    	ServerName * serverName = g_url_api->GetServerName(domainstr,TRUE);
		if(!serverName)
			return OpStatus::ERR_NO_MEMORY;
		OP_ASSERT(serverName->Name() != NULL);
		OpStringC8 hostname(serverName->Name());
		RETURN_IF_ERROR(CheckCookieTempBuffers(hostname.Length()+50)); // A little extra

        domain = cookie_processing_buf1;
	    op_strcpy(domain, hostname.CStr());
    }
    else
        domain = NULL;

    if(pathstr)
    {
        RETURN_IF_ERROR(pathTmp.SetUTF8FromUTF16(pathstr));

    	path = pathTmp.CStr();
	    if(*path == '/')
		    path++;

		/* // Ignore ";" parameters
    	int templen = op_strcspn(path, ";");
	    path[templen] = '\0';
		*/
    }
    else
        path = NULL;

	if( cookie_root )
	{
        * size_returned = 0;
		cookie_root->BuildCookieList(cookieArray, size_returned, domain, path, is_export, match_subpaths);
	}

    return OpStatus::OK;
}


OP_STATUS Cookie_Manager::RemoveCookieList(uni_char * domainstr, uni_char * pathstr, uni_char * namestr)
{
    char     *domain;
    char     *path;
    char     *name;
    OpString8 pathTmp;
    OpString8 nameTmp;

#ifdef DISK_COOKIES_SUPPORT
	if( !cookie_file_read )
	{
        OP_STATUS op_err;
	    TRAP(op_err,ReadCookiesL());  
        RETURN_IF_ERROR(op_err);
	}
#endif // DISK_COOKIES_SUPPORT

    if(domainstr)
    {
    	ServerName * serverName = g_url_api->GetServerName(domainstr, TRUE);
		if(!serverName)
			return OpStatus::ERR_NO_MEMORY;

		OP_ASSERT(serverName->Name() != NULL);
		OpStringC8 hostname(serverName->Name());
		RETURN_IF_ERROR(CheckCookieTempBuffers(hostname.Length()+50)); // A little extra

        domain = cookie_processing_buf1;
	    op_strcpy(domain, serverName->Name());
    }
    else
        domain = NULL;

    if(pathstr)
    {
        RETURN_IF_ERROR(pathTmp.SetUTF8FromUTF16(pathstr));

    	path = pathTmp.CStr();
	    if(*path == '/')
		    path++;

		/* // Ignore ";" parameters
    	int templen = op_strcspn(path, ";");
	    path[templen] = '\0';
		*/
    }
    else
        path = NULL;

    if(namestr)
    {
        RETURN_IF_ERROR(nameTmp.Set(namestr));

    	name = nameTmp.CStr();
    }
    else
        name = NULL;


	if( cookie_root )
		cookie_root->RemoveCookieList(domain, path, name);

    return OpStatus::OK;
}

#endif // NEED_URL_COOKIE_ARRAY

