/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#if defined HISTORY_SUPPORT && !defined NO_URL_OPERA
#include "modules/url/url2.h"
#include "modules/about/operahistory.h"
#include "modules/locale/locale-enum.h"
#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/pi/OpLocale.h"

OP_STATUS OperaHistory::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::SI_HISTORY_LIST_TEXT, PrefsCollectionFiles::StyleHistoryFile));
#else
	RETURN_IF_ERROR(OpenDocument(Str::SI_HISTORY_LIST_TEXT));
#endif
	RETURN_IF_ERROR(OpenBody(Str::SI_HISTORY_LIST_TEXT));

	// Cache the possibly expensive global pointer lookups
	OpHistoryModel *history = g_globalHistory;

	OpLocale *oplocale = g_oplocale;
	uni_char *tmpbuf = static_cast<uni_char *>(g_memory_manager->GetTempBuf2k());
	unsigned int tmpbuflen = UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen());

	// Declare string variables outside the loop to allow reuse of buffers
	OpString url;
	OpString title;

	unsigned int items = static_cast<unsigned int>(history->GetCount());
	int day_of_month = -1; // Used to write date separators
	for (unsigned int i = 0; i < items; ++ i)
	{
		HistoryPage *item = history->GetItemAtPosition(i);

		if (item)
		{
			time_t timeaccessed = item->GetAccessed();
			struct tm *tm = op_localtime(&timeaccessed);

			RETURN_IF_ERROR(item->GetAddress(url));
			RETURN_IF_ERROR(item->GetTitle(title));
			
			if(tm && url.CStr())
			{
				if (tm->tm_mday != day_of_month)
				{
					// Close old table, if open
					if (day_of_month != -1)
					{
						RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</table>\n")));
					}

					// Add day heading
#ifdef STRFTIME_DATE_INCLUDES_DAYNAME
					oplocale->op_strftime(tmpbuf, tmpbuflen, UNI_L("%x"), tm);
#else
					oplocale->op_strftime(tmpbuf, tmpbuflen, UNI_L("%A %x"), tm);
#endif
					// Some languages use a lowercase initial letter in
					// running text.
					if (Unicode::IsLower(*tmpbuf))
					{
						*tmpbuf = Unicode::ToUpper(*tmpbuf);
					}

					// Write heading and open new table
					RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("<h2>")));
					RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, tmpbuf));
					RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</h2>\n<table>\n")));

					// New day of month
					day_of_month = tm->tm_mday;
				}

				// Time string for item
				oplocale->op_strftime(tmpbuf, tmpbuflen, UNI_L("%X"), tm);

				// Write item in table
				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L(" <tr>\n  <td>")));
				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, tmpbuf));
				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</td>\n  <td><a href=\"")));
				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, url));
				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("\">")));
				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KHTMLify, !title.IsEmpty() ? title.CStr() : url.CStr()));
				RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</a></td>\n </tr>\n")));
			}
		}
	}

	// End table, if open
	if (day_of_month != -1)
	{
		RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, UNI_L("</table>\n")));
	}
	return CloseDocument();
}

#endif
