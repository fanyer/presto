/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/desktop_util/search/search_net.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "adjunct/quick/Application.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/widgets/OpEdit.h"
#include "WebSearchDialog.h"

WebSearchDialog* WebSearchDialog::m_dialog = 0;

void WebSearchDialog::OnInit()
{
	m_edit = 0;

	OpSearchDropDown* drop_down = static_cast<OpSearchDropDown*>(GetWidgetByName("search_edit"));
	if( drop_down )
	{
		m_edit = drop_down->edit;
		drop_down->SetSearchDropDownListener( this );

		SearchTemplate* search = 0;
#ifdef PREFS_CAP_DEFAULTSEARCHTYPE_STRINGS
		search = g_searchEngineManager->GetDefaultSearch();
#endif
		OpString search_guid;

		if( g_application->GetActiveDocumentDesktopWindow() )
		{
			OpBrowserView* browser_view = g_application->GetActiveDocumentDesktopWindow()->GetBrowserView();
			if( browser_view )
			{
				browser_view->GetWindowCommander()->SetScriptingDisabled(TRUE);

				if (browser_view->GetSearchGUID().HasContent())
					search_guid.Set(browser_view->GetSearchGUID());
			}
		}

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		if (search && search_guid.IsEmpty())
			search_guid.Set(search->GetUniqueGUID());
#endif // DESKTOP_UTIL_SEARCH_ENGINES

		OnSearchChanged( search_guid );
	}
}

WebSearchDialog::~WebSearchDialog()
{
	m_dialog = 0;
}

void WebSearchDialog::OnSearchChanged( const OpStringC& search_guid )
{
	OpString title;
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	SearchTemplate* search = g_searchEngineManager->GetByUniqueGUID(search_guid);
	g_searchEngineManager->MakeSearchWithString(search, title);
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	SetTitle( title.CStr() );
}

void WebSearchDialog::OnSearchDone()
{
	m_search_done = TRUE;
	CloseDialog(FALSE);
}

BOOL WebSearchDialog::OnInputAction(OpInputAction* action)
{
	if( action->GetAction() == OpInputAction::ACTION_OK )
	{
		if(!m_search_done)
		{
			OpSearchDropDown* drop_down = static_cast<OpSearchDropDown*>(GetWidgetByName("search_edit"));
			if( drop_down )
			{
				SearchEngineManager::SearchSetting settings;
				drop_down->Search(settings, FALSE);
			}
		}
		return TRUE;
	}
	return Dialog::OnInputAction(action);
}

//static
void WebSearchDialog::Create(DesktopWindow* parent_window )
{
	if( !m_dialog )
	{
		m_dialog = OP_NEW(WebSearchDialog, ());
		if (m_dialog)
		{
			if (OpStatus::IsError(m_dialog->Init(parent_window)))
				m_dialog = NULL;
		}
	}
}
