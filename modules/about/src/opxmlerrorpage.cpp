/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#include "modules/about/opxmlerrorpage.h"
#include "modules/xmlutils/xmltypes.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmlerrorreport.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/tempbuf.h"

OP_STATUS OpXmlErrorPage::GenerateData()
{
#ifdef _LOCALHOST_SUPPORT_
	RETURN_IF_ERROR(OpenDocument(Str::S_LOADINGFAILED_PAGE_TITLE, PrefsCollectionFiles::StyleErrorFile, FALSE));
#else
	RETURN_IF_ERROR(OpenDocument(Str::S_LOADINGFAILED_PAGE_TITLE, FALSE));
#endif
	OpenBody(Str::SI_ERR_XML_PARSING);

	/* FIXME: fix error reporting. */
	Str::LocaleString message_id = Str::SI_ERR_XML_SYNTAXERROR;

	OpString header, message, linelabel, columnlabel, reparsehtml;

	TRAPD(rc,
		g_languageManager->GetStringL(Str::SI_ERR_XML_PARSING, header);
		g_languageManager->GetStringL(message_id, message);
		g_languageManager->GetStringL(Str::S_ERR_XML_LINE, linelabel);
		g_languageManager->GetStringL(Str::S_ERR_XML_CHARACTER, columnlabel);
		g_languageManager->GetStringL(Str::S_REPARSE_AS_HTML, reparsehtml));
	RETURN_IF_ERROR(rc);

	TempBuffer buffer;

#ifdef XML_ERRORS
	OP_MEMORY_VAR unsigned line = 0;
	OP_MEMORY_VAR unsigned column = 0;

	XMLRange position = m_xml_parser->GetErrorPosition();
	line = position.start.IsValid() ? position.start.line + 1 : 0;
	column = position.start.IsValid() ? position.start.column : 0;

	m_url.WriteDocumentDataUniSprintf(
		UNI_L("<p>%s: <dfn>%s</dfn> (%s %d, %s %d)</p>\n\n"),
	    header.CStr(), message.CStr(), linelabel.CStr(), line, columnlabel.CStr(), column);
#else // XML_ERRORS
	m_url.WriteDocumentDataUniSprintf(
		UNI_L("<p>%s: <dfn>%s</dfn></p>\n\n"),
	    header.CStr(), message.CStr());
#endif // XML_ERRORS

	m_url.WriteDocumentDataUniSprintf(
		UNI_L("<p><a href=\"opera:forcehtml\">%s</a></p>\n"),
		reparsehtml.CStr());

#ifdef XML_ERRORS
	/* Extended details */

	/* XML error description */
	const char *error, *uri, *fragmentid;
	m_xml_parser->GetErrorDescription(error, uri, fragmentid);

	OpString description, href;
	unsigned int error_len = error ? op_strlen(error) : 0;
	AppendHTMLified(&description, error, error_len);
	unsigned int uri_len = uri ? op_strlen(uri) : 0;
	AppendHTMLified(&href, uri, uri_len);
	if (fragmentid)
	{
		RETURN_IF_ERROR(href.Append("#"));
		AppendHTMLified(&href, fragmentid, op_strlen(fragmentid));
	}

	OpString descriptioncaption, hrefcaption;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_XML_ERROR, descriptioncaption));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_XML_SPECIFICATION, hrefcaption));

	m_url.WriteDocumentDataUniSprintf(
		UNI_L("<dl>\n <dt>%s</dt><dd>%s</dd>\n <dt>%s</dt><dd><a href=\"%s\">%s</a></dd>\n</dl>\n"),
		descriptioncaption.CStr(), description.CStr(),
		hrefcaption.CStr(), href.CStr(), href.CStr());

	const XMLErrorReport* report = m_xml_parser->GetErrorReport();
	if (report)
	{
		unsigned item_index = 0, stop_index, items_count = report->GetItemsCount();
		bool in_error = false, in_information = false;

		while (item_index < items_count)
		{
			unsigned first_line = report->GetItem(item_index)->GetLine(), last_line = first_line;

			for (stop_index = item_index + 1; stop_index < items_count; ++stop_index)
			{
				unsigned item_line = report->GetItem(stop_index)->GetLine();
				if (item_line == last_line || item_line == last_line + 1)
				{
					last_line = item_line;
				}
				else
				{
					break;
				}
			}

			unsigned tmp_index, total_len=0;
			const XMLErrorReport::Item *item;
			const uni_char *str;
			for (tmp_index = item_index; tmp_index < stop_index; tmp_index++)
			{
				item = report->GetItem(tmp_index);
				if (item)
				{
					str = item->GetString();
					if (str)
						total_len += uni_strlen(str);
				}
				total_len += 32; // Some extra space for HTML-tags.
			}

			TempBuffer line;
			line.SetCachedLengthPolicy(TempBuffer::TRUSTED);
			RETURN_IF_ERROR(line.Expand(total_len));
			RETURN_IF_ERROR(line.Append("<pre>"));

			unsigned current_line = ~0u;

			while (item_index < stop_index)
			{
				const XMLErrorReport::Item *item = report->GetItem(item_index);

				if (item->GetLine() != current_line)
				{
					current_line = item->GetLine();

					char linenr[16]; // ARRAY OK adame 2008-09-11
					op_snprintf(linenr, 16, "%3u: ", current_line + 1);

					RETURN_IF_ERROR(line.Append(linenr));

					if (in_error)
					{
						RETURN_IF_ERROR(line.Append("<em class=\"error\">"));
					}
					else if (in_information)
					{
						RETURN_IF_ERROR(line.Append("<em>"));
					}
				}

				XMLErrorReport::Item::Type type = item->GetType();

				if (type == XMLErrorReport::Item::TYPE_LINE_FRAGMENT || type == XMLErrorReport::Item::TYPE_LINE_END)
				{
					RETURN_IF_ERROR(AppendHTMLified(&line, item->GetString(), uni_strlen(item->GetString())));

					if (type == XMLErrorReport::Item::TYPE_LINE_END)
					{
						if (in_error || in_information)
						{
							RETURN_IF_ERROR(line.Append("</em>"));
						}

						RETURN_IF_ERROR(line.Append("\n"));
					}
				}
				else
				{
					const char *string = "</em>";

					if (type == XMLErrorReport::Item::TYPE_ERROR_START)
					{
						OP_ASSERT(!in_error && !in_information);

						string = "<em class=\"error\">";
						in_error = true;
					}
					else if (type == XMLErrorReport::Item::TYPE_ERROR_END)
					{
						in_error = false;
					}
					else if (type == XMLErrorReport::Item::TYPE_INFORMATION_START)
					{
						OP_ASSERT(!in_error && !in_information);

						string = "<em>";
						in_information = true;
					}
					else if (type == XMLErrorReport::Item::TYPE_INFORMATION_END)
					{
						in_information = false;
					}

					RETURN_IF_ERROR(line.Append(string));
				}

				++item_index;
			}

			RETURN_IF_ERROR(line.Append("</pre>\n"));
			RETURN_IF_ERROR(m_url.WriteDocumentData(URL::KNormal, line.GetStorage()));
		}
	}
#endif // XML_ERRORS

	/* Finish document */
	return CloseDocument();
}
