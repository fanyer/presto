/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Arne Martin Guettler
*/

#include "core/pch.h"

#ifdef TRUST_RATING

#include "modules/about/operafraudwarning.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

OperaFraudWarning::OperaFraudWarning(const URL& url, const URL& suppressed_url, TrustInfoParser::Advisory *advisory) :
	OpGeneratedDocument(url, OpGeneratedDocument::HTML5),
	m_suppressed_url(suppressed_url),
	m_id(UINT_MAX),
	m_type(FraudTypeUnknown)
{
	OP_ASSERT(!m_suppressed_url.IsEmpty());
	if (advisory)
	{
		m_id = advisory->id;

		if (advisory->type == SERVER_SIDE_FRAUD_PHISHING)
			m_type = FraudTypePhishing;
		else if (advisory->type == SERVER_SIDE_FRAUD_MALWARE)
			m_type = FraudTypeMalware;
		else
			m_type = FraudTypeUnknown;
	}
}

OP_STATUS OperaFraudWarning::GenerateData()
{
	OpString line;
	
	// Get all the locale strings we need
	OpString warning, ignore, cancel, why_blocked, advice, heading;
	if (m_type == FraudTypePhishing)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_SITE_TITLE_FRAUD, heading));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_SITE_WARNING, warning));
	}
	else if (m_type == FraudTypeMalware)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_SITE_TITLE_MALWARE, heading));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_MALWARE_SITE_WARNING, warning));
	}
	else
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_SITE_HEADING, heading));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_SITE_WARNING, warning));
	}

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_SITE_IGNORE, ignore));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_SITE_CANCEL, cancel));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_SITE_ADVISORY_REASONS, why_blocked));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_BLOCKED_SITE_WARNING_ADVICE, advice));

	/* Generate document */
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_BLOCKED_SITE_HEADING, PrefsCollectionFiles::StyleWarningFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_BLOCKED_SITE_HEADING));
#endif

	RETURN_IF_ERROR(OpenBody()); /* Close HEAD and open BODY */
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<div id='main'>")));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<h1>")));
	RETURN_IF_ERROR(line.Set(heading));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, line.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</h1><hr>")));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, warning.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>\n")));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<p>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, advice.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</p>\n")));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<div id='links'>")));
	// <button onclick="history.back();">Go Back Safely</button>
	RETURN_IF_ERROR(line.Set("<a class=\"button\" href=\"opera:back\">"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, cancel.CStr()));
	RETURN_IF_ERROR(line.Set("</a>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));

#ifdef HELP_SUPPORT
	// <a href='opera:/help/filter.html'>Why was this page blocked?</a>
	RETURN_IF_ERROR(line.Set("<a class=\"link\" href=\"opera:/help/filter.html\">"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, why_blocked.CStr()));
	RETURN_IF_ERROR(line.Set("</a>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
#endif // HELP_SUPPORT

	OpString url_str;
	RETURN_IF_ERROR(m_suppressed_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url_str));
	// <a href="opera:proceed">Ignore this warning</a>
	RETURN_IF_ERROR(line.Set("<a class=\"link\" href=\"opera:proceed\" title=\""));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, url_str.CStr()));
	RETURN_IF_ERROR(line.Set("\">"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, ignore.CStr()));
	RETURN_IF_ERROR(line.Set("</a>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));

	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</div>")));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</div>")));

	// <iframe src="http://sitecheck2.opera.com/banner/<url>"></iframe>
	OpString banner_url;
	RETURN_IF_ERROR(banner_url.Set("http://sitecheck2.opera.com/banner/%u/"));
	RETURN_IF_ERROR(line.Set("<iframe src=\""));
	RETURN_IF_ERROR(line.AppendFormat(banner_url, m_id));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));
	RETURN_IF_ERROR(line.Set("\"></iframe>"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line));

	return CloseDocument(); /* Close the HTML code and signal that we are done */
}

#endif // TRUST_RATING
