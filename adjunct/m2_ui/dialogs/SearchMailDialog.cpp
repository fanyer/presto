/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "SearchMailDialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2_ui/models/accessmodel.h"
//#include "modules/prefs/prefsmanager/prefsmanager.h"
//#include "modules/prefs/storage/pfmap.h"

#include "modules/locale/oplanguagemanager.h"

void SearchMailDialog::Init(MailDesktopWindow* parent_window, int index_id, int search_index_id)
{
	m_parent_window = parent_window;
	m_index_id      = index_id;
	m_search_index_id = search_index_id;

	Dialog::Init(parent_window ? parent_window : g_application->GetActiveDesktopWindow());
}

SearchMailDialog::~SearchMailDialog() 
{ 
	OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Folders_treeview");
	OP_DELETE(treeview->GetTreeModel());
}

void SearchMailDialog::OnInit()
{
	if (m_search_index_id)
	{
		IndexSearch* search = g_m2_engine->GetIndexById(m_search_index_id)->GetSearch();
		if (search)
		{
			OpEdit* search_text = (OpEdit*) GetWidgetByName("Search_text_edit");
			search_text->SetText(search->GetSearchText().CStr());
		}
	}

	SetWidgetValue("Include_subfolders_checkbox", TRUE);

	OpString loc_str;
	INT32 got_index;
	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Field_dropdown");

	g_languageManager->GetString(Str::S_FILTER_ENTIRE_MESSAGE, loc_str);
	dropdown->AddItem(loc_str.CStr(), -1, &got_index, SearchTypes::ENTIRE_MESSAGE);
	g_languageManager->GetString(Str::S_FILTER_BODY, loc_str);
	dropdown->AddItem(loc_str.CStr(), -1, &got_index, SearchTypes::BODY);
	g_languageManager->GetString(Str::S_FILTER_SUBJECT, loc_str);
	dropdown->AddItem(loc_str.CStr(), -1, &got_index, SearchTypes::CACHED_SUBJECT);
	g_languageManager->GetString(Str::S_FILTER_FROM_HEADER, loc_str);
	dropdown->AddItem(loc_str.CStr(), -1, &got_index, SearchTypes::HEADER_FROM);
	g_languageManager->GetString(Str::S_FILTER_TO_HEADER, loc_str);
	dropdown->AddItem(loc_str.CStr(), -1, &got_index, SearchTypes::HEADER_TO);
	g_languageManager->GetString(Str::S_FILTER_CC_HEADER, loc_str);
	dropdown->AddItem(loc_str.CStr(), -1, &got_index, SearchTypes::HEADER_CC);
	g_languageManager->GetString(Str::S_FILTER_REPLYTO_HEADER, loc_str);
	dropdown->AddItem(loc_str.CStr(), -1, &got_index, SearchTypes::HEADER_REPLYTO);
	g_languageManager->GetString(Str::S_FILTER_NEWSGROUPS_HEADER, loc_str);
	dropdown->AddItem(loc_str.CStr(), -1, &got_index, SearchTypes::HEADER_NEWSGROUPS);
	g_languageManager->GetString(Str::S_FILTER_ANY_HEADER, loc_str);
	dropdown->AddItem(loc_str.CStr(), -1, &got_index, SearchTypes::HEADERS);

	OpTreeView* treeview = (OpTreeView*) GetWidgetByName("Folders_treeview");
	AccessModel* access_model = OP_NEW(AccessModel,(IndexTypes::CATEGORY_MY_MAIL, g_m2_engine->GetIndexer()));
	access_model->Init();
	treeview->SetTreeModel(access_model, 0);
	treeview->SetShowColumnHeaders(FALSE);
	treeview->SetColumnVisibility(1, FALSE);
	treeview->SetColumnVisibility(2, FALSE);
	treeview->SetColumnVisibility(3, FALSE);
	treeview->SetColumnVisibility(4, FALSE);
	treeview->SetColumnVisibility(5, FALSE);
	treeview->SetShowThreadImage(TRUE);

	if (!m_index_id && m_parent_window && m_parent_window->GetIndexID() != IndexTypes::UNREAD_UI)
		m_index_id = m_parent_window->GetIndexID();

	if (m_index_id)
		treeview->SetSelectedItemByID(m_index_id);
}


UINT32 SearchMailDialog::OnOk()
{
	OpString text;
	GetWidgetText("Search_text_edit", text);

	if (text.IsEmpty() == FALSE)
	{
		SearchTypes::Field field = SearchTypes::ENTIRE_MESSAGE;
		SearchTypes::Option option = SearchTypes::EXACT_PHRASE;

		OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Field_dropdown");
		field = (SearchTypes::Field)(INTPTR)dropdown->GetItemUserData(dropdown->GetSelectedItem());

		index_gid_t id;
		index_gid_t search_only_in = m_index_id;

		time_t start_time = 0;
		if (!m_search_index_id)
			g_m2_engine->GetIndexer()->StartSearch(text, option, field, start_time, UINT32(-1), id, search_only_in);
		else
		{
			// re-use the existing index, but first empty it
			Index* index = g_m2_engine->GetIndexById(m_search_index_id);
			index->Empty();
			g_m2_engine->GetIndexer()->StartSearch(text, option, field, start_time, UINT32(-1), id, search_only_in, index);
		}
		if (m_parent_window)
		{
			m_parent_window->SetMailViewToIndex(id, NULL, NULL, TRUE);
		}
		else
		{
			g_application->GoToMailView(id);
		}
	}

	return 0;
}


void SearchMailDialog::OnCancel()
{

	//abort

}


void SearchMailDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("Folders_treeview"))
	{
		// set correct filter view
		OpTreeView* folders = (OpTreeView*) GetWidgetByName("Folders_treeview");
		OpTreeModelItem* item = folders->GetSelectedItem();

		if (item)
		{
			m_index_id = item->GetID();

			SetWidgetEnabled("Include_subfolders_checkbox", ((m_index_id < IndexTypes::FIRST_CATEGORY
															|| m_index_id > IndexTypes::LAST_CATEGORY)
															&& m_index_id > IndexTypes::LAST_IMPORTANT
															&& (m_index_id < IndexTypes::FIRST_LABEL
															|| m_index_id > IndexTypes::LAST_LABEL)
															&& ( m_index_id < IndexTypes::FIRST_SEARCH
															|| m_index_id > IndexTypes::LAST_SEARCH)
															&& ( m_index_id < IndexTypes::FIRST_NEWSFEED
															|| m_index_id > IndexTypes::LAST_NEWSFEED)
															&& ( m_index_id < IndexTypes::FIRST_NEWSGROUP
															|| m_index_id > IndexTypes::LAST_NEWSGROUP)
															&& ( m_index_id < IndexTypes::FIRST_CONTACT
															|| m_index_id > IndexTypes::LAST_CONTACT)
															&& ( m_index_id < IndexTypes::DOC_ATTACHMENT
															|| m_index_id > IndexTypes::ZIP_ATTACHMENT)));
		}
	}

	Dialog::OnChange(widget, changed_by_mouse);
}

#endif //M2_SUPPORT
