/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "opml_importer.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/util/str/strutil.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/locale/oplanguagemanager.h"

// ***************************************************************************
//
//	OPMLM2ImportHandler
//
// ***************************************************************************

OPMLM2ImportHandler::OPMLM2ImportHandler()
:	m_indexer(0),
	m_newsfeed_account(0),
	m_import_count(0),
	m_parent_folder(NULL)
{
}


OP_STATUS OPMLM2ImportHandler::Init()
{
	m_indexer = g_m2_engine ? g_m2_engine->GetIndexer(): NULL;
	// Get a valid newsfeed account.

	m_newsfeed_account = g_m2_engine->GetAccountManager()->GetRSSAccount(TRUE);

	if (!m_indexer || !m_newsfeed_account)
		return OpStatus::ERR;
	
	m_parent_folder = m_indexer->GetIndexById(IndexTypes::FIRST_ACCOUNT + m_newsfeed_account->GetAccountId());

	return OpStatus::OK;
}


OP_STATUS OPMLM2ImportHandler::OnOutlineBegin(const OpStringC& text,
	const OpStringC& xml_url, const OpStringC& title,
	const OpStringC& description)
{
	if (xml_url.HasContent())
	{
		const OpStringC& folder_name = title.HasContent() ? title : text;

		// First we see if this index already exists.
		Index* index = m_indexer->GetSubscribedFolderIndex(m_newsfeed_account, xml_url, 0, folder_name, FALSE, FALSE);
		if (index == 0)
		{
			// Didn't exist; create it.
			if ((index = m_indexer->GetSubscribedFolderIndex(m_newsfeed_account, xml_url, 0, folder_name, TRUE, FALSE)) == 0)
				return OpStatus::ERR;

			g_m2_engine->RefreshFolder(index->GetId());

			if (m_parent_folder)
				index->SetParentId(m_parent_folder->GetId());

			++m_import_count;
		}
	}
	else
	{
		// this is a folder
		m_parent_folder = m_indexer->GetFeedFolderIndexByName(m_parent_folder->GetId(), title.HasContent() ? title : text);
	}

	return OpStatus::OK;
}

OP_STATUS OPMLM2ImportHandler::OnOutlineEnd(BOOL folder)
{
	if (folder && m_parent_folder)
	{
		m_parent_folder = m_indexer->GetIndexById(m_parent_folder->GetParentId());
	}
	return OpStatus::OK;
}

OP_STATUS OPMLM2ImportHandler::OnOPMLEnd()
{
	// We might have triggered the creation of a newsfeed account, so we fire
	// these just in case.
	if (g_application != 0)
	{
		g_application->SettingsChanged(SETTINGS_ACCOUNT_SELECTOR);
		g_application->SettingsChanged(SETTINGS_MENU_SETUP);
		g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);

		g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PANEL, 0, UNI_L("Mail"), g_application->GetActiveDesktopWindow());

		OpString formatted_message;

		OpString message;

		RETURN_IF_ERROR(g_languageManager->GetString(Str::D_NEWSFEED_LIST_SUCCESSFULLY_IMPORTED, message));
		RETURN_IF_ERROR(formatted_message.AppendFormat(message.CStr(), m_import_count));

		SimpleDialog::ShowDialog(WINDOW_NAME_FEEDLIST_IMPORTED, NULL, formatted_message.CStr(), UNI_L("Opera"), Dialog::TYPE_OK, Dialog::IMAGE_INFO);
	}

	return OpStatus::OK;
}

#endif //M2_SUPPORT
