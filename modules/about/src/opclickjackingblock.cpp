/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/about/opclickjackingblock.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/formats/uri_escape.h"

OpClickJackingBlock::OpClickJackingBlock(const URL& url, const URL& blocked_url)
	: OpGeneratedDocument(url, OpGeneratedDocument::HTML5)
	, m_blocked_url(blocked_url)
{}

OP_STATUS OpClickJackingBlock::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_LOADINGFAILED_PAGE_TITLE, PrefsCollectionFiles::StyleWarningFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_LOADINGFAILED_PAGE_TITLE));
#endif

	RETURN_IF_ERROR(OpenBody(Str::S_LOADINGFAILED_PAGE_TITLE, NULL));

	OpString click_jacked_url;
	RETURN_IF_ERROR(m_blocked_url.GetAttribute(URL::KUniName_With_Fragment, click_jacked_url));

	if (!click_jacked_url.IsEmpty())
	{
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<h2>")));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, click_jacked_url.CStr()));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</h2>\n")));
	}

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	OpString cant_load_page_in_frame;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CANT_LOAD_PAGE_IN_FRAME, cant_load_page_in_frame));
	if (!cant_load_page_in_frame.IsEmpty())
	{
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, cant_load_page_in_frame.CStr()));
	}

	if (!click_jacked_url.IsEmpty())
	{
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("\n<p><ul><li><a class=\"link\" href=\"opera:proceed\" title='")));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, click_jacked_url.CStr()));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("'>")));
		OpString load_in_new_window;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_LOAD_IN_NEW_WINDOW, load_in_new_window));
		if (!load_in_new_window.IsEmpty())
		{
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, load_in_new_window.CStr()));
		}
		else
		{
			OP_ASSERT(!"Old language file?");
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, click_jacked_url.CStr()));
		}
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</a></li></ul>")));
	}

	return CloseDocument();
}
