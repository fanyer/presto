/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef ABOUT_PRIVATE_BROWSING

#include "modules/about/opprivacymode.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

OP_STATUS OpPrivacyModePage::GenerateData()
{
	// Make sure the "privacy mode enabled" and "privacy mode disabled"
	// pages are visually distinct by using the error page template
	// for the disabled variant.
	if (m_enabled)
	{
		RETURN_IF_ERROR(OpenDocument(Str::S_TITLE_PRIVATE_BROWSING, PrefsCollectionFiles::StylePrivateModeFile));
		RETURN_IF_ERROR(OpenBody(Str::S_HEADER_PRIVATE_BROWSING, NULL));
	}
	else
	{
		RETURN_IF_ERROR(OpenDocument(Str::S_TITLE_PRIVATE_BROWSING_DISABLED, PrefsCollectionFiles::StyleErrorFile));
		RETURN_IF_ERROR(OpenBody(Str::S_HEADER_PRIVATE_BROWSING_DISABLED, NULL));
	}

	OpString text, more_link, go_to_speeddial, not_again;
	g_languageManager->GetString(Str::S_MSG_PRIVATE_BROWSING,text);
	g_languageManager->GetString(Str::S_MORE_PRIVATE_BROWSING,more_link);
#if defined OPERASPEEDDIAL_URL || defined ABOUT_SPEEDDIAL_LINK
	g_languageManager->GetString(Str::S_GO_TO_SPEEDDIAL,go_to_speeddial);
#endif
	g_languageManager->GetString(Str::S_DO_NOT_SHOW_AGAIN,not_again);

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	if (m_enabled)
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, text));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L(" <a target=\"_blank\" href=\"opera:/help/tabs.html#private\">")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, more_link));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</a></p>\n")));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	if (m_enabled)
	{
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<input id=\"dontshow\" type=\"checkbox\" /> <label for=\"dontshow\">")));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, not_again));
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</label>")));
	}
#if defined OPERASPEEDDIAL_URL || defined ABOUT_SPEEDDIAL_LINK
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<a href=\"opera:speeddial\"><button>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, go_to_speeddial));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</button></a></p>\n")));
#else
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>\n")));
#endif

	if (m_enabled)
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<script>\ndocument.addEventListener(\"click\", function(e){\nif(e.target.getAttribute(\"id\") == \"dontshow\"){\n opera.setPreference('User Prefs', 'Show Private Mode Introduction Page', e.target.checked ? 0:1 );}\n}, false);\n</script>\n")));

	return CloseDocument();
}

#endif // ABOUT_PRIVATE_BROWSING

