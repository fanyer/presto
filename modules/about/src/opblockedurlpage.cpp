/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ERROR_PAGE_SUPPORT
#ifdef URL_FILTER

#include "modules/about/opblockedurlpage.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

OP_STATUS
OpBlockedUrlPage::GenerateData()
{
#ifdef PREFS_HAVE_ERROR_CSS
	RETURN_IF_ERROR(OpenDocument(Str::SI_ERR_COMM_BLOCKED_URL, PrefsCollectionFiles::StyleErrorFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::SI_ERR_COMM_BLOCKED_URL));
#endif

	RETURN_IF_ERROR(OpenBody(Str::SI_ERR_COMM_BLOCKED_URL, UNI_L("blockedurl")));

	OpString header_template;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_URL_ERROR_PAGE_DESCRIPTION, header_template));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	OpString url_string;
	if (m_ref_url)
	{
		OpString errormessage;
		RETURN_IF_ERROR(m_ref_url->GetAttribute(URL::KUniName, url_string, TRUE));
		int url_string_length = url_string.Length();
		errormessage.Reserve(url_string_length + 32);
		RETURN_IF_ERROR(errormessage.Set(UNI_L("<cite><a href=\"")));
		RETURN_IF_ERROR(AppendHTMLified(&errormessage, url_string, url_string_length));
		RETURN_IF_ERROR(errormessage.Append(UNI_L("\">")));
		RETURN_IF_ERROR(AppendHTMLified(&errormessage, url_string, url_string_length));
		RETURN_IF_ERROR(errormessage.Append(UNI_L("</a></cite>")));
		m_url.WriteDocumentDataUniSprintf(header_template.CStr(), errormessage.CStr());
	}
	else
	{
		m_url.WriteDocumentDataUniSprintf(header_template.CStr(), UNI_L(""));
	}

#ifdef AB_ERROR_DESKTOP_HELP
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_URL_ERROR_PAGE_DESKTOP_INSTRUCTIONS, header_template));
	m_url.WriteDocumentDataUniSprintf(UNI_L("</p>\n<p>%s</p>"), header_template.CStr());
#else
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>")));
#endif // AB_ERROR_DESKTOP_HELP

	return CloseDocument();
}

#endif // URL_FILTER
#endif // ERROR_PAGE_SUPPORT
