/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef ERROR_PAGE_SUPPORT
#include "modules/about/opillegalhostpage.h"
#include "modules/about/operrorpage.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/locale/oplanguagemanager.h"

#ifdef AB_ERROR_SEARCH_FORM
#include "modules/dochand/winman.h"
#endif // AB_ERROR_SEARCH_FORM

OP_STATUS OpIllegalHostPage::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_LOADINGFAILED_PAGE_TITLE, PrefsCollectionFiles::StyleErrorFile, FALSE));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_LOADINGFAILED_PAGE_TITLE, FALSE));
#endif

	/* Fetch strings */

	OpString line_1, line_2;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ILLEGAL_URL_LINE_1, line_1));
	RETURN_IF_ERROR(g_languageManager->GetString(m_bad_kind != KIllegalPort ? Str::S_ILLEGAL_URL_LINE_2 : Str::S_ILLEGAL_URL_LINE_2a, line_2));

	/* Start document body */
	RETURN_IF_ERROR(OpenBody(Str::S_ILLEGAL_URL_TITLE));

	/* Error description goes here */
	m_url.WriteDocumentDataUniSprintf(UNI_L("<div>\n<p>%s <cite>"), line_1.CStr());
	m_url.WriteDocumentData(URL::KHTMLify, m_bad_url);
	m_url.WriteDocumentDataUniSprintf(UNI_L("</cite><span dir='ltr'> %s</span></p>\n"), line_2.CStr());

	RETURN_IF_ERROR(OpenUnorderedList());
	RETURN_IF_ERROR(AddListItem(Str::S_ILLEGAL_URL_REASON));
	RETURN_IF_ERROR(CloseList());

#if defined(AB_ERROR_SEARCH_FORM) && !defined(DOC_SEARCH_SUGGESTIONS_SUPPORT)
	RETURN_IF_ERROR(OpErrorPage::GenerateSearchForm(m_bad_url, m_url, g_windowManager->GetLastTypedUserText(), TRUE));
#endif // AB_ERROR_SEARCH_FORM && !DOC_SEARCH_SUGGESTIONS_SUPPORT

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</div>\n")));

	/* Finish document */
	return CloseDocument();
}

OP_STATUS OpIllegalHostPageURL_Generator::QuickGenerate(URL& url, OpWindowCommander*)
{
	OpString problem_url;

	RETURN_IF_ERROR(url.GetAttribute(URL::KName, problem_url));

	OpIllegalHostPage error_page(url, problem_url,
			(OpIllegalHostPage::IllegalHostKind)url.GetAttribute(URL::KInvalidURLKind));

	RETURN_IF_ERROR(error_page.GenerateData());
	return OpStatus::OK;
}

#endif
