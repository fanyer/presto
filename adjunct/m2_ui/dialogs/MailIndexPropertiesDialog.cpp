/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT
//# include "modules/prefs/storage/pfmap.h"

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "MailIndexPropertiesDialog.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/index.h"
#include "modules/locale/oplanguagemanager.h"

MailIndexPropertiesDialog::MailIndexPropertiesDialog(BOOL is_first_newsfeed_edit)
:	m_index_id(-1),
	m_groups_model(0),
	m_item_id(-1),
	m_is_first_newsfeed_edit(is_first_newsfeed_edit)
{
}

void MailIndexPropertiesDialog::Init(DesktopWindow* parent_window, int index_id, OpTreeModel* groups_model, int item_id)
{
	m_index_id = index_id;
	m_groups_model = groups_model;
	m_item_id = item_id;
	Dialog::Init(parent_window);
}

void MailIndexPropertiesDialog::OnInit()
{
	Index *index = g_m2_engine->GetIndexById(m_index_id);
	if (index == 0)
		return;

	OpEdit* location_edit = GetWidgetByName<OpEdit>("Location_edit", WIDGET_TYPE_EDIT);
	if (location_edit)
		location_edit->SetForceTextLTR(TRUE);

	OpString str;
	index->GetName(str);
	SetWidgetText("Indexname_edit", str.CStr() ? str.CStr() : UNI_L(""));

	SetWidgetEnabled("Indexname_edit", (m_index_id >= IndexTypes::FIRST_SEARCH && m_index_id < IndexTypes::LAST_SEARCH) ||
		(m_index_id >= IndexTypes::FIRST_THREAD && m_index_id < IndexTypes::LAST_THREAD) ||
		(m_index_id >= IndexTypes::FIRST_NEWSGROUP && m_index_id < IndexTypes::LAST_NEWSGROUP) ||
		(m_index_id >= IndexTypes::FIRST_IMAP && m_index_id < IndexTypes::LAST_IMAP) ||
		(m_index_id >= IndexTypes::FIRST_POP && m_index_id < IndexTypes::LAST_POP) ||
		(m_index_id >= IndexTypes::FIRST_NEWSFEED && m_index_id < IndexTypes::LAST_NEWSFEED));

	if (index->GetId() >= IndexTypes::FIRST_NEWSFEED && index->GetId() < IndexTypes::LAST_NEWSFEED)
	{
		OpCheckBox* checkbox = (OpCheckBox *)(GetWidgetByName("Get_name_from_feed_checkbox"));
		if (checkbox != 0)
		{
			if (m_is_first_newsfeed_edit)
			{
				checkbox->SetValue(1);
				OnChange(checkbox, FALSE);
			}
			else
			{
				checkbox->SetValue(0);
				// checkbox->SetEnabled(FALSE);
			}
		}

		IndexSearch* search = index->GetM2Search();
		if (search)
			SetWidgetText("Location_edit", search->GetSearchText().CStr());

		OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Update_dropdown");

		if (dropdown)
		{
			OpString buffer;
			if (buffer.Reserve(128))
			{
				g_languageManager->GetString(Str::DI_IDM_CMOD_DOCS_NEVER, buffer);
				dropdown->AddItem(buffer.CStr(), -1, NULL, (INTPTR)UINT_MAX);

				OpString format_str;

				g_languageManager->GetString(Str::S_EVERY_X_MINUTES, format_str);

				if( format_str.CStr() )
				{
					uni_sprintf(buffer.CStr(), format_str.CStr(), 5);
					dropdown->AddItem(buffer.CStr(), -1, NULL, 300);
					uni_sprintf(buffer.CStr(), format_str.CStr(), 15);
					dropdown->AddItem(buffer.CStr(), -1, NULL, 900);
					uni_sprintf(buffer.CStr(), format_str.CStr(), 30);
					dropdown->AddItem(buffer.CStr(), -1, NULL, 1800);
				}

				g_languageManager->GetString(Str::S_EVERY_HOUR, format_str);
				if( format_str.CStr() )
				{
					uni_sprintf(buffer.CStr(), format_str.CStr());
					dropdown->AddItem(buffer.CStr(), -1, NULL, 3600);
				}

				g_languageManager->GetString(Str::S_EVERY_X_HOURS, format_str);
				if( format_str.CStr() )
				{
					uni_sprintf(buffer.CStr(), format_str.CStr(), 2);
					dropdown->AddItem(buffer.CStr(), -1, NULL, 7200);
					uni_sprintf(buffer.CStr(), format_str.CStr(), 3);
					dropdown->AddItem(buffer.CStr(), -1, NULL, 10800);
					uni_sprintf(buffer.CStr(), format_str.CStr(), 5);
					dropdown->AddItem(buffer.CStr(), -1, NULL, 18000);
					uni_sprintf(buffer.CStr(), format_str.CStr(), 10);
					dropdown->AddItem(buffer.CStr(), -1, NULL, 36000);
					uni_sprintf(buffer.CStr(), format_str.CStr(), 24);
					dropdown->AddItem(buffer.CStr(), -1, NULL, 86400);
				}

				g_languageManager->GetString(Str::S_EVERY_WEEK, format_str);
				if( format_str.CStr() )
				{
					uni_sprintf(buffer.CStr(), format_str.CStr());
					dropdown->AddItem(buffer.CStr(), -1, NULL, 604800);
				}
			}

			const time_t update_freq = index->GetUpdateFrequency();
			if (update_freq == -1)
			{
				if (m_is_first_newsfeed_edit)
					dropdown->SelectItem(6, TRUE);
				else
					dropdown->SelectItem(0, TRUE);
			}
			else if (update_freq > -1 && update_freq <= 300) // 5 min
				dropdown->SelectItem(1, TRUE);
			else if (update_freq > 300 && update_freq <= 900) // 15 min
				dropdown->SelectItem(2, TRUE);
			else if (update_freq > 900 && update_freq <= 1800) // 30 min
				dropdown->SelectItem(3, TRUE);
			else if (update_freq > 1800 && update_freq <= 3600) // 1h
				dropdown->SelectItem(4, TRUE);
			else if (update_freq > 3600 && update_freq <= 7200) // 2h
				dropdown->SelectItem(5, TRUE);
			else if (update_freq > 7200 && update_freq <= 10800) // 3h
				dropdown->SelectItem(6, TRUE);
			else if (update_freq > 10800 && update_freq <= 18000) // 5h
				dropdown->SelectItem(7, TRUE);
			else if (update_freq > 18000 && update_freq <= 36000) // 10h
				dropdown->SelectItem(8, TRUE);
			else if (update_freq > 36000 && update_freq <= 86400) // 24h
				dropdown->SelectItem(9, TRUE);
			else if (update_freq > 86400 && update_freq <= 604800) // week
				dropdown->SelectItem(10, TRUE);
			else
				dropdown->SelectItem(6, TRUE);
		}
	}
	// else
	// {
	// 	GetWidgetByName("Get_name_from_feed_checkbox")->SetVisibility(FALSE);
	// 	GetWidgetByName("label_for_Location_edit")->SetVisibility(FALSE);
	// 	GetWidgetByName("Location_edit")->SetVisibility(FALSE);
	// 	GetWidgetByName("label_for_Update_dropdown")->SetVisibility(FALSE);
	// 	GetWidgetByName("Update_dropdown")->SetVisibility(FALSE);
	// }

}


void MailIndexPropertiesDialog::OnInitVisibility()
{
	Index *index = g_m2_engine->GetIndexById(m_index_id);
	if (index == 0)
		return;

	if (index->GetId() >= IndexTypes::FIRST_NEWSFEED && index->GetId() < IndexTypes::LAST_NEWSFEED)
	{
		// if (!m_is_first_newsfeed_edit)
		//	GetWidgetByName("Get_name_from_feed_checkbox")->SetVisibility(FALSE);
	}
	else
	{
		OpWidget* widget;

		if ((widget = GetWidgetByName("Get_name_from_feed_checkbox")))
			widget->SetVisibility(FALSE);
		if ((widget = GetWidgetByName("label_for_Location_edit")))
			widget->SetVisibility(FALSE);
		if ((widget = GetWidgetByName("Location_edit")))
			widget->SetVisibility(FALSE);
		if ((widget = GetWidgetByName("label_for_Update_dropdown")))
			widget->SetVisibility(FALSE);
		if ((widget = GetWidgetByName("Update_dropdown")))
			widget->SetVisibility(FALSE);
	}

	Dialog::OnInitVisibility();
}


UINT32 MailIndexPropertiesDialog::OnOk()
{

	Index *index = g_m2_engine->GetIndexById(m_index_id);

	OpString name;
	OpString filter;

	GetWidgetText("Indexname_edit", name);

	if (index == NULL)
		return 0;

	const BOOL is_newsfeed_index = (index->GetId() >= IndexTypes::FIRST_NEWSFEED && index->GetId() < IndexTypes::LAST_NEWSFEED);

	if (!name.IsEmpty())
	{
		OpString old_name;
		if(index->GetName(old_name) == OpStatus::OK)
		{
			if(old_name.Compare(name) != 0)
			{
				UINT32 account_id = index->GetAccountId();
				if (account_id && !is_newsfeed_index)
				{
					Account* account;
					if(g_m2_engine->GetAccountManager()->GetAccountById(account_id, account) == OpStatus::OK && account)
						OpStatus::Ignore(account->RenameFolder(old_name, name));
				}
				else
				{
					OpStatus::Ignore(index->SetName(name.CStr()));
				}
			}
		}
	}

	if (is_newsfeed_index)
	{
		OpString location;
		GetWidgetText("Location_edit", location);

		if (index->GetM2Search())
			index->GetM2Search()->SetSearchText(location);

		OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Update_dropdown");

		if (dropdown)
		{
			time_t update_frequency = (time_t)dropdown->GetItemUserData(dropdown->GetSelectedItem());
			index->SetUpdateFrequency(update_frequency);
			index->SetLastUpdateTime(0); // make it possible to check immediately
		}

		if (GetWidgetValue("Get_name_from_feed_checkbox") != 0)
		{
			index->SetUpdateNameManually(TRUE);

			// Set the default name for the newsfeed to be the entered url, so
			// we potentially don't get several 'rss newsfeed' entries.
			index->SetName(location.CStr());
		}
		else
			index->SetUpdateNameManually(FALSE);

		if (m_groups_model)
			g_m2_engine->UpdateFolder(m_groups_model, m_item_id, location, name);
	}

	const UINT32 index_id = index->GetId();
	g_m2_engine->UpdateIndex(index_id);
	g_m2_engine->RefreshFolder(index_id);

	// Immediately fetch the newsfeed if it was just added.
	if (m_is_first_newsfeed_edit)
	{
		g_m2_engine->SelectFolder(index_id);
	}

	return 0;
}


void MailIndexPropertiesDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{

	if (widget->IsNamed("Get_name_from_feed_checkbox"))
	{
		// Enable and disable the name input, based on what's selected.
		OpWidget* name_widget = GetWidgetByName("Indexname_edit");
		if (name_widget == 0)
			return;

		if (GetWidgetValue("Get_name_from_feed_checkbox") == 1)
			name_widget->SetEnabled(FALSE);
		else
			name_widget->SetEnabled(TRUE);
	}

	Dialog::OnChange(widget, changed_by_mouse);
}

void MailIndexPropertiesDialog::OnCancel()
{
	if (m_is_first_newsfeed_edit)
	{
		if (m_groups_model)
			g_m2_engine->DeleteFolder(m_groups_model, m_item_id);
		g_m2_engine->RemoveIndex(m_index_id);
	}
}

#endif //M2_SUPPORT
