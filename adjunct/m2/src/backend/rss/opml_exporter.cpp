/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "opml_exporter.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "modules/locale/oplanguagemanager.h"

#ifdef M2_SUPPORT

// ***************************************************************************
//
//	OPMLExporter
//
// ***************************************************************************

OPMLExporter::OPMLExporter()
:	m_indexer(0)
,	m_export_count(0)
{
}

OP_STATUS OPMLExporter::XMLEscape(const OpStringC& input, OpString& output)
{
	RETURN_IF_ERROR(output.Set(input));

	RETURN_IF_ERROR(StringUtils::Replace(output, UNI_L("&"), UNI_L("&amp;")));
	RETURN_IF_ERROR(StringUtils::Replace(output, UNI_L("<"), UNI_L("&lt;")));
	RETURN_IF_ERROR(StringUtils::Replace(output, UNI_L(">"), UNI_L("&gt;")));
	RETURN_IF_ERROR(StringUtils::Replace(output, UNI_L("'"), UNI_L("&apos;")));
	RETURN_IF_ERROR(StringUtils::Replace(output, UNI_L("\""), UNI_L("&quot;")));

	return OpStatus::OK;
}

 OP_STATUS OPMLExporter::WriteUTF8String(OpFile& file, const OpStringC& data)
{
	OpString8 data8;
	RETURN_IF_ERROR(data8.SetUTF8FromUTF16(data));

	RETURN_IF_ERROR(file.Write(data8.CStr(), data8.Length()));
	return file.Write("\n", 1);
}


OP_STATUS OPMLExporter::Export(OpFile& file)
{
	m_indexer = g_m2_engine->GetIndexer();
	if (!m_indexer)
		return OpStatus::ERR;

	m_export_count = 0;
	
	// Iterate the indexes and write out OPML for them.
	Index* index;
	Account* account = g_m2_engine->GetAccountManager()->GetRSSAccount();
	if (!account)
		return OpStatus::OK; // No RSS account

	// Write initial OPML header.
	{
		OpString header;
		RETURN_IF_ERROR(header.Append("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"));
		RETURN_IF_ERROR(header.Append("<opml version=\"1.0\">\n"));

			OpString head_title;
			OpString8 user_agent;
			RETURN_IF_ERROR(g_m2_engine->GetGlueFactory()->GetBrowserUtils()->GetUserAgent(user_agent));
			RETURN_IF_ERROR(head_title.Set(user_agent.CStr()));
			RETURN_IF_ERROR(head_title.Insert(0,("Newsfeeds exported from ")));

		if (head_title.HasContent())
		{
			RETURN_IF_ERROR(header.Append("\t<head>\n"));
			RETURN_IF_ERROR(header.Append("\t\t<title>"));
			RETURN_IF_ERROR(header.Append(head_title));
			RETURN_IF_ERROR(header.Append("</title>\n"));
			RETURN_IF_ERROR(header.Append("\t</head>\n"));
		}

		RETURN_IF_ERROR(header.Append("\t<body>"));

		RETURN_IF_ERROR(WriteUTF8String(file, header));
	}

	// Write Content
	{
		index = m_indexer->GetIndexById(IndexTypes::FIRST_ACCOUNT + account->GetAccountId());
		RETURN_IF_ERROR(WriteOutlineForIndexAndChildIndexes(file, 0, index));
	}
	
	// Write OPML footer.
	{
		OpString footer;
		RETURN_IF_ERROR(footer.Append("\t</body>\n"));
		RETURN_IF_ERROR(footer.Append("</opml>"));

		RETURN_IF_ERROR(WriteUTF8String(file, footer));
	}
	
	// Show success dialog
	{

		OpString formatted_message, message;

		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEWSFEED_LIST_SUCCESSFULLY_EXPORTED, message));
		RETURN_IF_ERROR(formatted_message.AppendFormat(message.CStr(), m_export_count));

		SimpleDialog::ShowDialog(WINDOW_NAME_FEEDLIST_EXPORTED, NULL, formatted_message.CStr(), UNI_L("Opera"), Dialog::TYPE_OK, Dialog::IMAGE_INFO);
	}

	return OpStatus::OK;
}

OP_STATUS OPMLExporter::WriteOutlineForIndexAndChildIndexes(OpFile& file, int level, Index* index)
{
	if (!index)
		return OpStatus::ERR;
	
	OpString title;
	OpStatus::Ignore(index->GetName(title));
	
	if (level == 0 || index->GetType() == IndexTypes::UNIONGROUP_INDEX)
	{
		OpString empty;
		// this is a folder
		// write <outline title="Fun" text="Fun">
		if (level != 0)
			OpStatus::Ignore(WriteOutline(file, level, title, title, 0, 0)); 		

		OpINT32Vector children;
		m_indexer->GetChildren(index->GetId(), children, TRUE);
		for (UINT i = 0; i < children.GetCount(); i++)
		{
			Index* child = m_indexer->GetIndexById(children.Get(i));
			OpStatus::Ignore(WriteOutlineForIndexAndChildIndexes(file, level+1, child));
		}

		// write </outline>
		if (level != 0) 
			OpStatus::Ignore(WriteOutline(file, level, 0, 0, 0, 0)); 
	}
	else
	{
		// this is a feed:
		// <outline text="Adobe Labs" type="rss" xmlUrl="http://weblogs.macromedia.com/labs/index.xml"  title="Adobe Labs" />
		IndexSearch *search = index->GetSearch();
		if (search && search->GetSearchText().HasContent())
		{
			OpStatus::Ignore(WriteOutline(file, level, title, title, search->GetSearchText(), 0));
			m_export_count++;
		}
	}
	return OpStatus::OK;
}

OP_STATUS OPMLExporter::WriteOutline(OpFile& file, int level, const uni_char* text, const uni_char* title, const uni_char* xml_url, const uni_char* description)
{
	TempBuffer outline;
	for (int i = 0; i <= level; i++)
		RETURN_IF_ERROR(outline.Append("\t"));

	if (!text)
	{
		RETURN_IF_ERROR(outline.Append("</outline>"));
	}
	else
	{
		OpString escaped_text;
		RETURN_IF_ERROR(XMLEscape(text, escaped_text));
		RETURN_IF_ERROR(outline.AppendFormat(UNI_L("<outline text=\"%s\""), escaped_text.CStr()));
	
		if (title)
		{
			OpString escaped_title;
			RETURN_IF_ERROR(XMLEscape(title, escaped_title));

			RETURN_IF_ERROR(outline.AppendFormat(UNI_L(" title=\"%s\""), escaped_title.CStr()));
		}
		if (xml_url)
		{
			OpString escaped_xml_url;
			RETURN_IF_ERROR(XMLEscape(xml_url, escaped_xml_url));

			RETURN_IF_ERROR(outline.AppendFormat(UNI_L(" type=\"rss\" xmlUrl=\"%s\""), escaped_xml_url.CStr()));
			

			if (description)
			{
				OpString escaped_description;
				RETURN_IF_ERROR(XMLEscape(description, escaped_description));

				RETURN_IF_ERROR(outline.AppendFormat(UNI_L(" description=\"%s\""), escaped_description.CStr()));
			}
			RETURN_IF_ERROR(outline.Append(" />"));
		}
		else
		{
			// no xml_url -> folder, which is closed separately
			RETURN_IF_ERROR(outline.Append(" >"));
		}
	}

	RETURN_IF_ERROR(WriteUTF8String(file, outline.GetStorage()));
	return OpStatus::OK;
}

#endif //M2_SUPPORT
