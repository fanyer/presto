/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Mostyn Bramley-Moore
 */
#include "core/pch.h"

#ifndef NO_URL_OPERA
#include "modules/about/operacredits.h"
#include "modules/sqlite/sqlite3.h" // To get the SQLite version.
#include "modules/locale/locale-enum.h"

#ifdef OPERA_CREDITS_PAGE
OP_STATUS OperaCredits::GenerateData()
{
	// Initialize, open HEAD and write TITLE and stylesheet link
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::SI_IDABOUT_INCLUDESFROM, PrefsCollectionFiles::StyleAboutFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::SI_IDABOUT_INCLUDESFROM));
#endif

	// Close HEAD, open BODY and write the top H1
	RETURN_IF_ERROR(OpenBody(Str::SI_IDABOUT_INCLUDESFROM));

	// Write info block. Each of these should close exactly the HTML tags it opens:

	OpString credits;
	RETURN_IF_ERROR(GetString(credits, m_rtl));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, credits.CStr()));

	// Dedication and copyright
	OpString line;
	line.Reserve(256);
	RETURN_IF_ERROR(line.Set("\n\n <address"));
	RETURN_IF_ERROR(line.Append(m_rtl ? " dir=\"ltr\">" : ">"));
	RETURN_IF_ERROR(line.Append("Copyright \xA9 1995-"));
	RETURN_IF_ERROR(line.Append(ABOUT_CURRENT_YEAR));
	RETURN_IF_ERROR(line.Append(" Opera Software ASA. All rights reserved.</address>\n"));
	RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.CStr()));

	// Close BODY and HTML elements and finish off.
	return CloseDocument();
}
#endif // OPERA_CREDITS_PAGE

OP_STATUS OperaCredits::GetString(OpString &line, BOOL rtl)
{
	/* The attributions and licenses probably all need to be reviewed
	 * periodically: are they up-to-date and relevant ?  They're now managed via
	 * the third-party features system (q.v., in hardcore/features/).  See the
	 * developer Wiki's Third_party_code page for authoritative details:
	 * http://wiki.oslo.osa/developerwiki/index.php/Third_party_code
	 * https://ssl.opera.com:8008/developerwiki/index.php/Third_party_code
	 */

	line.Reserve(6144);
	RETURN_IF_ERROR(line.Set(rtl ? " <ul dir=\"ltr\">" : " <ul>"));

	OP_STATUS rc = line.Append(
		"\n"
#include "modules/hardcore/features/features-thirdparty_attributions.inl"

#ifdef ABOUT_ELEKTRANS
		"  <li>The Elektrans</li>\n" // what about panic ?
#endif // ABOUT_ELEKTRANS
		);
	RETURN_IF_ERROR(rc);

	RETURN_IF_ERROR(line.Append(" </ul>\n <p>"));
	RETURN_IF_ERROR(AppendLocaleString(&line, Str::SI_IDABOUT_WELOVEYOU));
	return line.Append("</p>");
}

#endif // NO_URL_OPERA
