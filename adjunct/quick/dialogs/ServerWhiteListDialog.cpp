/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Adam Minchinton (adamm)
 *
 */

#include "core/pch.h"

#include "ServerWhiteListDialog.h"

#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpEdit.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/hardcore/keys/opkeys.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/inputmanager/inputaction.h"
#include "adjunct/quick/models/ServerWhiteList.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/opstrlst.h"

ServerWhiteListDialog::ServerWhiteListDialog()
	: m_whitelist_model(0),
	m_override_hosts_model(0),
	m_current_model(0)
{
}

ServerWhiteListDialog::~ServerWhiteListDialog()
{
	OP_DELETE(m_whitelist_model);
	OP_DELETE(m_override_hosts_model);
}

void ServerWhiteListDialog::Init(DesktopWindow *parent)
{
	m_whitelist_model = g_server_whitelist->GetModel();
	OP_ASSERT(m_whitelist_model);

	Dialog::Init(parent);
}

void ServerWhiteListDialog::OnInit()
{
	OpTreeView* edit = (OpTreeView *)GetWidgetByName("Whitelist_address_view");
	if(edit)
	{
		edit->SetReselectWhenSelectedIsRemoved(TRUE);
		edit->SetDragEnabled(FALSE);
	}

	Dialog::OnInit();
}


void ServerWhiteListDialog::OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	OpTreeView *edit = (OpTreeView *)GetWidgetByName("Whitelist_address_view");

	OP_ASSERT(edit == widget);

	if(edit == widget)
	{
		OpTreeModelItem* item = edit->GetItemByPosition(pos);
		if(item)
		{
			INT32 model_pos = edit->GetModelPos(pos);
			if(model_pos > -1)
			{
				OpString oldaddress;
				oldaddress.Set(m_current_model->GetItemString(model_pos));

				if (OnItemEdit(oldaddress,text)) // This will insert the exact item
				{
					m_current_model->SetItemData(model_pos, 0, text.CStr());
				}
				else 
				{
					// Remove entry, it could not be inserted in whitelist
					m_current_model->Remove(model_pos);
				}
			}
		}
	}
}

void ServerWhiteListDialog::OnClick(OpWidget *widget, UINT32 id)
{
	OpTreeView *edit = (OpTreeView *)GetWidgetByName("Whitelist_address_view");
	if (!edit)
		return;

	INT32 pos = edit->GetSelectedItemModelPos();
	const uni_char *address = m_current_model->GetItemString(pos);
	if (pos > -1 && IsItemEditable(address))
	{
		if (widget->IsNamed("Whitelist_delete")	&& OnItemDelete(address))
			m_current_model->Delete(pos);

		if (widget->IsNamed("Whitelist_edit"))
			edit->EditItem(pos, 0, TRUE);
	}

    if (widget->IsNamed("Whitelist_new"))
	{
		pos = m_current_model->AddItem(GetDefaultNewItem());
		edit->EditItem(pos, 0, TRUE);
	}
}

void ServerWhiteListDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	OpTreeView *edit = (OpTreeView *)GetWidgetByName("Whitelist_address_view");

	if (widget == edit)
	{
		OpTreeModelItem* item = edit->GetItemByPosition(pos);
		const uni_char *address	= m_current_model->GetItemString(pos);

		if (item)
		{
			if (nclicks == 2)
			{
				INT32 model_pos = edit->GetModelPos(pos);
				if (model_pos > -1 && IsItemEditable(address))
				{
					edit->EditItem(model_pos, 0, TRUE);
				}
			}
		}

		OpWidget *edit_button = GetWidgetByName("Whitelist_edit");
		OpWidget *delete_button = GetWidgetByName("Whitelist_delete");

		if (edit_button)
		{
			edit_button->SetEnabled(item != NULL && IsItemEditable(address));
		}
		if (delete_button)
		{
			delete_button->SetEnabled(item != NULL && IsItemEditable(address));
		}
	}
}

void ServerWhiteListDialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	is_enabled = TRUE;
	switch (index)
	{
		case 0:
		{
			action = GetOkAction();
			g_languageManager->GetString(Str::DI_IDBTN_CLOSE, text);
			is_default = TRUE;
			name.Set(WIDGET_NAME_BUTTON_CLOSE);
			break;
		}
	}
}

void ServerWhiteListDialog::OnCancel()
{
}

UINT32 ServerWhiteListDialog::OnOk()
{
	g_server_whitelist->Write();
	return 0;
}

void ServerWhiteListDialog::OnInitPage(INT32 page_number, BOOL first_time)
{
	OP_ASSERT(m_whitelist_model);
	OpTreeView *edit = (OpTreeView *)GetWidgetByName("Whitelist_address_view");

	if(edit)
	{
		if (first_time)
		{
			edit->SetSelectedItem(0);
		}

		// reload content of Whitelist_address_view
		if(page_number == WhiteList)
		{
			edit->SetTreeModel(m_whitelist_model);
			m_current_model = m_whitelist_model;
			SetWidgetText("label_header", Str::D_SERVER_WHITELSIT_HEADER);
		}
		else if(page_number == SecureInternalHosts)
		{
			GetOverrideHostsModel(); // initialize
			edit->SetTreeModel(m_override_hosts_model);
			m_current_model = m_override_hosts_model;
			SetWidgetText("label_header", Str::D_SECURE_INTERNAL_HOSTS_HEADER);
		}	
	}

	ShowWidget("Whitelist_add", TRUE); // always show the groups since the 2 pages share the same logic and UI
}

BOOL ServerWhiteListDialog::OnItemEdit(const OpString& old_item,const OpString& new_item)
{
	if(new_item.IsEmpty())
		return FALSE;

	if(GetCurrentPage() == WhiteList)
	{
		// Remove the old item, and add the new one
		g_server_whitelist->RemoveServer(old_item);
		return g_server_whitelist->AddServer(new_item);
	}
	else if(GetCurrentPage() == SecureInternalHosts)
	{
		if(!old_item.IsEmpty())
			RETURN_VALUE_IF_ERROR(g_pcnet->OverridePrefL(old_item.CStr(), PrefsCollectionNetwork::AllowCrossNetworkNavigation, FALSE, TRUE),FALSE);
		if(!new_item.IsEmpty())
			RETURN_VALUE_IF_ERROR(g_pcnet->OverridePrefL(new_item.CStr(), PrefsCollectionNetwork::AllowCrossNetworkNavigation, TRUE,  TRUE),FALSE);
	}
	
	return TRUE;
}

BOOL ServerWhiteListDialog::OnItemDelete(const uni_char* item)
{
	if(GetCurrentPage() == WhiteList)
	{
		return g_server_whitelist->RemoveServer(item);
	}
	else if(GetCurrentPage() == SecureInternalHosts)
	{
		RETURN_VALUE_IF_ERROR(g_pcnet->OverridePrefL(item, PrefsCollectionNetwork::AllowCrossNetworkNavigation, FALSE, TRUE),FALSE);
	}
	return TRUE;
}

BOOL ServerWhiteListDialog::IsItemEditable(const OpStringC& address)
{
	if(GetCurrentPage() == WhiteList)
	{
		return g_server_whitelist->ElementIsEditable(address);
	}
	else if(GetCurrentPage() == SecureInternalHosts)
	{

	}
	return TRUE;
}

const uni_char*	ServerWhiteListDialog::GetDefaultNewItem()
{
	if(GetCurrentPage() == WhiteList)
	{
		return UNI_L("http://");
	}
	return UNI_L("");
}

SimpleTreeModel* ServerWhiteListDialog::GetOverrideHostsModel()
{
	if(m_override_hosts_model)
		return m_override_hosts_model;

	// load m_override_hosts_model
	m_override_hosts_model = OP_NEW(SimpleTreeModel,());
	if(m_override_hosts_model)
	{
		OpString_list* hosts = NULL;
		TRAPD(err, hosts = g_prefsManager->GetOverriddenHostsL());
		if(hosts)
		{
			for (UINT32 i = 0; i < hosts->Count(); i++)
			{
				if(g_prefsManager->IsHostOverridden(hosts->Item(i).CStr()) 
					&& g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowCrossNetworkNavigation,hosts->Item(i).CStr()) )
				{
					m_override_hosts_model->AddItem(hosts->Item(i).CStr());
				}
			}			
		}
	
		OP_DELETE(hosts);
	}

	return m_override_hosts_model;
}
