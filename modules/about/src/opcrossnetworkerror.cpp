/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ERROR_PAGE_SUPPORT

#include "modules/about/opcrossnetworkerror.h"
#include "modules/locale/oplanguagemanager.h"

OpCrossNetworkError::OpCrossNetworkError(const URL& url, Str::LocaleString error, const URL& blocked_url)
	: OpGeneratedDocument(url, OpGeneratedDocument::HTML5),
	  m_ref_url(blocked_url),
	  m_error(error)
{
	OP_ASSERT(!blocked_url.IsEmpty());
}

OP_STATUS OpCrossNetworkError::GenerateData()
{
	OpString ignore, always_ignore, line;

#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_MSG_CROSS_NETWORK_TITLE, PrefsCollectionFiles::StyleErrorFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_MSG_CROSS_NETWORK_TITLE));
#endif
	switch (m_error)
	{
	case Str::S_MSG_CROSS_NETWORK_INTERNET_INTRANET:
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MSG_CROSS_NETWORK_INTERNET_INTRANET_CONTINUE, always_ignore));
		break;
	case Str::S_MSG_CROSS_NETWORK_INTERNET_LOCALHOST:
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MSG_CROSS_NETWORK_INTERNET_LOCALHOST_CONTINUE, always_ignore));
		break;
	case Str::S_MSG_CROSS_NETWORK_INTRANET_LOCALHOST:
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MSG_CROSS_NETWORK_INTRANET_LOCALHOST_CONTINUE, always_ignore));
		break;
	default:
		OP_ASSERT(FALSE);
	}

	RETURN_IF_ERROR(OpenBody(Str::S_MSG_WARNING, UNI_L("cross")));

	OpString ref_url_name;
	RETURN_IF_ERROR(m_ref_url.GetAttribute(URL::KUniName, ref_url_name));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<h2>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, ref_url_name));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</h2>\n<p>")));

	RETURN_IF_ERROR(OpGeneratedDocument::AppendLocaleString(&line, m_error));
	RETURN_IF_ERROR(line.Append("</p>\n<p>"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MSG_CONTINUE, ignore));

	RETURN_IF_ERROR(line.Set("<a href=\"opera:proceed\" title=\""));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, ref_url_name));
	RETURN_IF_ERROR(line.Set("\">"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, ignore.CStr()));
	RETURN_IF_ERROR(line.Set("</a></p>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));

#ifdef PREFS_HOSTOVERRIDE
	//this piece needs to be ifdefed because it adds the link that sets the preference, per domain
	RETURN_IF_ERROR(line.Set("<p><small><a href=\"opera:proceed/override\" title=\""));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, ref_url_name));
	RETURN_IF_ERROR(line.Set("\">"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, always_ignore.CStr()));
	RETURN_IF_ERROR(line.Set("</a></small></p>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
#endif // PREFS_HOSTOVERRIDE

	RETURN_IF_ERROR(GeneratedByOpera());

	return CloseDocument();
}

#endif // ERROR_PAGE_SUPPORT
