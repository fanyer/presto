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

#include "modules/selftest/src/testutils.h"
#include "modules/network_selftest/scanpass.h"
#include "modules/cache/url_stream.h"
#include "modules/locale/oplanguagemanager.h"


OP_STATUS ScanPass_SimpleTester::Construct(const OpStringC8 &pass_string)
{
	return test_string.Set(pass_string);
}

OP_STATUS ScanPass_SimpleTester::Construct(URL &a_url, URL &ref, const OpStringC8 &pass_string, BOOL unique)
{
	URL_DocSelfTest_Item::Construct(a_url, ref, unique);
	return test_string.Set(pass_string);
}

OP_STATUS ScanPass_SimpleTester::Construct(URL &a_url, URL &ref, const OpStringC &pass_string, BOOL unique)
{
	URL_DocSelfTest_Item::Construct(a_url, ref, unique);
	return test_string.SetUTF8FromUTF16(pass_string);
}

OP_STATUS ScanPass_SimpleTester::Construct(const OpStringC8 &source_url, URL &ref, const OpStringC8 &pass_string, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;
	URL empty;

	a_url = g_url_api->GetURL(empty, source_url, unique);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	return Construct(a_url, ref, pass_string, unique);
}

OP_STATUS ScanPass_SimpleTester::Construct(const OpStringC &source_url, URL &ref, const OpStringC &pass_string, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;
	URL empty;

	a_url = g_url_api->GetURL(empty, source_url, unique);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	return Construct(a_url, ref, pass_string, unique);
}

BOOL ScanPass_SimpleTester::Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code)
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
				if(!CheckPass(test_string))
					return FALSE;
				if(extra_test_string.HasContent() && !CheckPass(extra_test_string))
					return FALSE;
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
			g_languageManager->GetString(Str::LocaleString(status_code), temp);
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

BOOL ScanPass_SimpleTester::CheckPass(const OpStringC8 &string)
{
	OpAutoPtr<URL_DataDescriptor> desc(NULL);

	desc.reset(url.GetDescriptor(NULL, TRUE, TRUE, TRUE));
	if(desc.get() == NULL)
	{
		ReportFailure("Compare failed.");
		return FALSE;				
	}
	
	BOOL more=TRUE;
	size_t pass_len = string.Length();
	
	while(more)
	{
		size_t buf_len;

		buf_len = desc->RetrieveData(more);

		if(buf_len == 0)
		{
			if(more)
			{
				ReportFailure("Read descriptor failed.");
				return FALSE;				
			}
			continue;
		}

		if(desc->GetBuffer() == NULL)
		{
			ReportFailure("Compare failed.");
			return FALSE;				
		}

		if(pass_len <= buf_len)
		{
			const char *buf = string.CStr();
			const char *comp_buffer = desc->GetBuffer();
			size_t scan_len = buf_len -pass_len;
			size_t i;

			for(i=0; i<= scan_len; i++)
			{
				if(op_memcmp(comp_buffer+i, buf , pass_len) == 0)
					return TRUE;
			}
			desc->ConsumeData((unsigned)i);
		}

	}

	desc.reset();

	ReportFailure("Pass string not found.");
	return FALSE;
}

OP_STATUS UniScanPass_SimpleTester::Construct(const OpStringC &pass_string)
{
	return test_string.Set(pass_string);
}

OP_STATUS UniScanPass_SimpleTester::Construct(URL &a_url, URL &ref, const OpStringC &pass_string, BOOL unique)
{
	URL_DocSelfTest_Item::Construct(a_url, ref, unique);
	return test_string.Set(pass_string);
}

OP_STATUS UniScanPass_SimpleTester::Construct(const OpStringC8 &source_url, URL &ref, const OpStringC &pass_string, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;

	a_url = g_url_api->GetURL(source_url);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	return Construct(a_url, ref, pass_string, unique);
}

OP_STATUS UniScanPass_SimpleTester::Construct(const OpStringC &source_url, URL &ref, const OpStringC &pass_string, BOOL unique)
{
	if(source_url.IsEmpty())
		return OpStatus::ERR;

	URL a_url;

	a_url = g_url_api->GetURL(source_url);
	if(a_url.IsEmpty())
		return OpStatus::ERR;

	return Construct(a_url, ref, pass_string, unique);
}

BOOL UniScanPass_SimpleTester::Verify_function(URL_DocSelfTest_Event event, Str::LocaleString status_code)
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
				if(!CheckPass())
					return FALSE;
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
			g_languageManager->GetString(Str::LocaleString(status_code), temp);
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

BOOL UniScanPass_SimpleTester::CheckPass()
{
	OpAutoPtr<URL_DataDescriptor> desc(NULL);

	desc.reset(url.GetDescriptor(NULL, TRUE, FALSE, TRUE));
	if(desc.get() == NULL)
	{
		ReportFailure("Compare failed.");
		return FALSE;				
	}
	
	BOOL more=TRUE;
	size_t pass_len = UNICODE_SIZE(test_string.Length());
	
	while(more)
	{
		size_t buf_len;

		buf_len = desc->RetrieveData(more);

		if(buf_len == 0)
		{
			if(more)
			{
				ReportFailure("Read descriptor failed.");
				return FALSE;				
			}
			continue;
		}

		if(desc->GetBuffer() == NULL)
		{
			ReportFailure("Compare failed.");
			return FALSE;				
		}

		if(pass_len <= buf_len)
		{
			const byte *buf = (const byte *) test_string.CStr();
			const byte *comp_buffer = (const byte *) desc->GetBuffer();
			size_t scan_len = buf_len -pass_len;
			size_t i;

			for(i=0; i<= scan_len; i++)
			{
				if(op_memcmp(comp_buffer+i, buf , pass_len) == 0)
					return TRUE;
			}
			desc->ConsumeData((unsigned)i);
		}

	}

	desc.reset();

	ReportFailure("Pass string not found.");
	return FALSE;
}


#endif // SELFTEST
