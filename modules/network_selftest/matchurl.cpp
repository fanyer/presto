/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/



#include "core/pch.h"

#if defined(SELFTEST)

#include "modules/network_selftest/matchurl.h"
#include "modules/locale/oplanguagemanager.h"

void MatchWithURL_Tester::MatchURLLoader::OnAllDocumentsFinished()
{
	parent->MatchURLs_Loaded();
}

OP_STATUS MatchWithURL_Tester::Construct(URL &a_url, URL &ref, URL &match_base, URL &mtch_url, BOOL unique)
{
	if(a_url.IsEmpty() || match_url.IsEmpty() || a_url == match_url)
		return OpStatus::ERR;

	URL_DocSelfTest_Item::Construct(a_url, ref, unique);

	match_url = mtch_url;
	if(match_url.IsEmpty())
		return OpStatus::ERR;

	match_url_use = match_url;

	return url.IsValid() ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS MatchWithURL_Tester::Construct(const OpStringC8 &source_url, URL &ref, URL &match_base, const OpStringC8 &mtch_url, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;
	URL empty;

	a_url = g_url_api->GetURL(empty,source_url,unique);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	URL_DocSelfTest_Item::Construct(a_url, ref, unique);

	match_url = g_url_api->GetURL(match_base, mtch_url, unique);
	if(match_url.IsEmpty() || a_url == match_url)
		return OpStatus::ERR;

	match_url_use = match_url;

	return url.IsValid() ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS MatchWithURL_Tester::Construct(const OpStringC &source_url, URL &ref, URL &match_base, const OpStringC &mtch_url, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;
	URL empty;

	a_url = g_url_api->GetURL(empty,source_url,unique);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	URL_DocSelfTest_Item::Construct(a_url, ref, unique);

	match_url = g_url_api->GetURL(match_base, mtch_url, unique);
	if(match_url.IsEmpty() || a_url == match_url)
		return OpStatus::ERR;

	match_url_use = match_url;

	return url.IsValid() ? OpStatus::OK : OpStatus::ERR;
}

BOOL MatchWithURL_Tester::Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code)
{
	switch(event)
	{
	case URLDocST_Event_Data_Received:
		{
			URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus, TRUE);
			
			if(status == URL_LOADED)
			{
				URL_LoadPolicy policy(FALSE, URL_Load_Normal);
				URL empty;
				OP_STATUS op_err= loader.LoadDocument(match_url, empty, policy);
				if(OpStatus::IsError(op_err))
				{
					ReportFailure("Loading match item failed.");
					return FALSE;
				}

				return TRUE;
			}
			else if(status != URL_LOADING)
			{
				ReportFailure("Some kind of loading failure %d.", status);
				return FALSE;
			}
		}
		break;
	default:
		return URL_DocSelfTest_Item::Verify_function(event, status_code);
	}
	return TRUE;
}

void MatchWithURL_Tester::MatchURLs_Loaded()
{
	Verify_function((CompareURLAndURL(this, url, match_url) ? URLDocST_Event_LoadingFinished : URLDocST_Event_LoadingFailed));
}
#endif // SELFTEST
