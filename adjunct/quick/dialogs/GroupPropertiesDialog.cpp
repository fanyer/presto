/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"
#include "GroupPropertiesDialog.h"

#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/panels/ContactsPanel.h"
#include "adjunct/quick_toolkit/widgets/OpImageWidget.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpWidgetFactory.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	GroupPropertiesDialog
**
***********************************************************************************/

GroupPropertiesDialog::GroupPropertiesDialog() :
	m_group_model(2)
{

}

void GroupPropertiesDialog::Init(DesktopWindow* parent_window, OpTreeModelItem* item, OpTreeModelItem* parent)
{
	m_group    = item;
	m_group_id = item->GetID();
	m_parent_id  = parent ? parent->GetID() : 0;

	Dialog::Init(parent_window);
}

void GroupPropertiesDialog::OnInit()
{
	HotlistManager::ItemData item_data;
	g_hotlist_manager->GetItemValue( m_group, item_data );

	SetWidgetText("Name_edit", item_data.name.CStr());

	// Dialog caption
	OpString title;
	g_languageManager->GetString(Str::D_PROPERTIES_DIALOG_CAPTION, title);

	SetTitle(title.CStr());

	OpTreeView* treeview = (OpTreeView*)GetWidgetByName("Group_treeview");

	if (treeview)
	{
		OpString str;

		g_languageManager->GetString(Str::D_GROUP_NAME, str);
		m_group_model.SetColumnData(0, str.CStr());
		g_languageManager->GetString(Str::D_GROUP_ADDRESS, str);
		m_group_model.SetColumnData(1, str.CStr());

		GroupModelItem* group = (GroupModelItem*)m_group;

		UINT32 count = group->GetItemCount();

		UINT32 j = 0;
		for (UINT32 i = 0; i < count; i++)
		{
			UINT32 id = group->GetItemByPosition(i);

			HotlistModelItem* item = g_hotlist_manager->GetItemByID(id);
			if (item)
			{
				HotlistManager::ItemData item_data;

				g_hotlist_manager->GetItemValue( item, item_data );
				m_group_model.AddItem(item_data.name.CStr());
				m_group_model.SetItemData(j++, 1, item_data.mail.CStr(), item_data.image.CStr());
			}
		}

		treeview->SetTreeModel(&m_group_model);
		treeview->SetColumnWeight(0,60);
		treeview->SetSelectedItem(0);
	}
}

UINT32 GroupPropertiesDialog::OnOk()
{
	if( m_group )
	{
		HotlistManager::ItemData item_data;

		GetWidgetText("Name_edit", item_data.name);

		if(g_hotlist_manager->RegisterTemporaryItem( m_group,m_parent_id))
		{
			// New item
			g_hotlist_manager->SetItemValue( m_group, item_data, TRUE );
		}
		else
		{
			// The item was only modified
			g_hotlist_manager->SetItemValue( m_group, item_data, TRUE );
		}
	}

	return 0;
}

void GroupPropertiesDialog::OnCancel()
{
	g_hotlist_manager->RemoveTemporaryItem(m_group);
}
