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

#include "modules/network_selftest/justload.h"
#include "modules/locale/oplanguagemanager.h"


OP_STATUS JustLoad_Tester::Construct(URL &a_url, URL &ref, BOOL unique)
{
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	URL_DocSelfTest_Item::Construct(a_url, ref, unique);
	return OpStatus::OK;
}
OP_STATUS JustLoad_Tester::Construct(const OpStringC8 &source_url, URL &ref, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;
	URL empty;

	a_url = g_url_api->GetURL(empty, source_url, unique);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	URL_DocSelfTest_Item::Construct(a_url, ref, unique);
	return url.IsValid() ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS JustLoad_Tester::Construct(const OpStringC &source_url, URL &ref, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;
	URL empty;

	a_url = g_url_api->GetURL(empty, source_url, unique);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	URL_DocSelfTest_Item::Construct(a_url, ref, unique);
	return url.IsValid() ? OpStatus::OK : OpStatus::ERR;
}

BOOL JustLoad_Tester::Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code)
{
	switch(event)
	{
	case URLDocST_Event_Header_Loaded:
		if(header_loaded)
		{
			ReportFailure("Multiple header loaded.");
			return FALSE;				
		}
		header_loaded = TRUE;
		break;
	case URLDocST_Event_Redirect:
		if(header_loaded)
		{
			ReportFailure("Redirect after header loaded.");
			return FALSE;				
		}
		break;
	case URLDocST_Event_Data_Received:
		{
			if(!header_loaded)
			{
				ReportFailure("Data before header loaded.");
				return FALSE;				
			}
			URLStatus status = (URLStatus) url.GetAttribute(URL::KLoadStatus, TRUE);
			
			if(status == URL_LOADED)
			{
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
		{
			OpString temp;
			OpString8 lang_code, errorstring;
			g_languageManager->GetString(status_code, temp);
			lang_code.SetUTF8FromUTF16(temp);
			temp.Empty();
			url.GetAttribute(URL::KInternalErrorString, temp);
			errorstring.SetUTF8FromUTF16(temp);

			ReportFailure("Some kind of loading failure %d:%d:\"%s\":\"%s\" .", (int) event, (int) status_code,
				(lang_code.HasContent() ? lang_code.CStr() : "<No String>"),
				(errorstring.HasContent() ? errorstring.CStr() : "")
				);
		}
		return FALSE;
	}
	return TRUE;
}
#endif // SELFTEST
