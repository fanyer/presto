/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
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

#if defined(_NATIVE_SSL_SUPPORT_) && defined LIBSSL_ENABLE_CRL_SUPPORT


#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/url/url2.h"
#include "modules/cache/url_stream.h"
#include "modules/libssl/ssl_api.h"

BOOL load_PEM_certificates2(SSL_varvector32 &data_source, SSL_varvector32 &pem_content);

#ifdef SSL_ALLOW_REDIRECT_OF_CRLS
#define CRL_REDIRECT_ALLOWED	URL::KFollowRedirect
#else
#define CRL_REDIRECT_ALLOWED	URL::KNoRedirect
#endif


OP_STATUS CRL_item::DecodeData()
{
	if(loaded)
		return OpStatus::OK;

	OpFileLength cnt_len=0;
	crl_url.GetAttribute(URL::KContentLoaded, &cnt_len, CRL_REDIRECT_ALLOWED);

	if(crl_url.IsEmpty() || 
		(URLStatus) crl_url.GetAttribute(URL::KLoadStatus, CRL_REDIRECT_ALLOWED) != URL_LOADED ||
		crl_url.GetAttribute(URL::KHTTP_Response_Code, CRL_REDIRECT_ALLOWED) != 200 ||
		cnt_len == 0 /*||
		crl_url.GetAttribute(URL::KHTTP_ContentType).CompareI("application/pkix-crl") != 0 */
		)
		return OpStatus::ERR;

	URL_DataStream crl_reader(crl_url, TRUE);
	SSL_varvector32 crl_temp;

	TRAP_AND_RETURN(op_err, crl_temp.AddContentL(&crl_reader));
	// Detect PEM source
	{
		uint32 pos = 0;
		uint32 dlen = crl_temp.GetLength();
		byte *data = crl_temp.GetDirect();

		while(pos +10 <= dlen)
		{
			if(!op_isspace(data[pos]))
			{
				if(op_strnicmp((char *) data+pos, "-----BEGIN",10) == 0)
				{
					// this is a PEM file
					crl_temp.Append("\0", 1);

					if(!load_PEM_certificates2(crl_temp, crl_data))
						return OpStatus::ERR;
				}
				else
					crl_data = crl_temp;

				break;
			}
			pos ++;
		}
	}

	loaded = TRUE;

	return OpStatus::OK;
}

BOOL CRL_item::AddURL(URL &a_crl_url, SSL_varvector32 &a_issuer, CRL_List *precollected_crls, CRL_List *collect_crls, BOOL promote)
{
	BOOL ret = FALSE;
	URLType url_type = (URLType) a_crl_url.GetAttribute(URL::KType);

	if((url_type == URL_HTTP || url_type == URL_HTTPS) && 
		a_crl_url.GetAttribute(URL::KHostName).FindFirstOf('.') != KNotFound)
	{
		if(precollected_crls || collect_crls)
		{
			CRL_item *item = (precollected_crls ? precollected_crls->FindCRLItem(a_crl_url) : NULL);

			if(item == NULL && collect_crls)
				item = collect_crls->FindCRLItem(a_crl_url);

			if(item)
			{
				if(!promote || 
					item->UpgradeURL(a_crl_url))
					return TRUE;
				item = NULL; // could not promote the URL;
			}
		}

		ret = TRUE;
		if(crl_url.IsEmpty())
		{
			issuer = a_issuer;
			crl_url = a_crl_url;
		}
		else
		{
			OP_ASSERT(a_issuer.GetLength() == 0 || issuer == a_issuer);
			URLLink *new_url = OP_NEW(URLLink, (a_crl_url));

			if(new_url)
				new_url->Into(&alternative_urls);
		}

	}

	return ret;
}

BOOL CRL_item::UpgradeURL(URL &url)
{
	if(crl_url == url)
		return TRUE;

	URLLink *url_item = (URLLink *) alternative_urls.First();

	while(url_item)
	{
		if(url_item->url == url)
		{
			url_item->Out();
			url_item->url = crl_url;
			crl_url = url;
			url_item->IntoStart(&alternative_urls);
			return TRUE;
		}
		
		url_item = (URLLink *) url_item->Suc();
	}

	return FALSE;
}

BOOL CRL_item::MatchURL(URL &our_url, URL &url, OpString8 &url_str)
{
	if(our_url == url)
		return TRUE;

	if(our_url.GetAttribute(URL::KUnique))
	{
		if (url_str.IsEmpty())
			url.GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI,url_str);

		if (url_str.HasContent())
		{
			OpString8 tempstring;
			
			crl_url.GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI,tempstring);
			if(url_str.Compare(tempstring) == 0)
				return TRUE;
		}
	}

	return FALSE;
}


BOOL CRL_item::HaveURL(URL &url)
{
	OpString8 url_str;

	if(MatchURL(crl_url, url, url_str))
		return TRUE;

	URLLink *url_item = (URLLink *) alternative_urls.First();

	while(url_item)
	{
		if(MatchURL(url_item->url , url, url_str))
			return TRUE;

		url_item = (URLLink *) url_item->Suc();
	}

	return FALSE;
}

BOOL CRL_item::PromoteURL()
{
	URLLink *url_item=NULL;

	while((url_item = (URLLink *) alternative_urls.First()) != NULL)
	{
		URL url = url_item->url;
		url_item->Out();
		OP_DELETE(url_item);
		if(url.IsValid())
		{
			crl_url = url;

			return TRUE;
		}
	}

	return FALSE;
}


CRL_item *CRL_List::FindCRLItem(URL &url)
{
	CRL_item *item = First();

	while(item)
	{
		if(item->HaveURL(url))
			return item;
		
		item = item->Suc();
	}
	
	return NULL;
}

CRL_item *CRL_List::FindCRLItem(SSL_varvector32 &a_issuer)
{
	CRL_item *item = First();

	while(item)
		{
		if(item->HaveIssuer(a_issuer))
				return item;

		item = item->Suc();
	}
	
	return NULL;
}

#endif // defined(_NATIVE_SSL_SUPPORT_) && defined LIBSSL_ENABLE_CRL_SUPPORT
