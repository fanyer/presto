/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#include "core/pch.h"

#include "ProxyExceptionDialog.h"

#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/opstrlst.h"
#include "modules/url/url_man.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

ProxyExceptionDialog::ProxyExceptionDialog()
	: m_whitelist_model(0),
	m_blacklist_model(0),
	m_current_model(0),
	m_treeview(0)
{
}

ProxyExceptionDialog::~ProxyExceptionDialog()
{
	OP_DELETE(m_whitelist_model);
	OP_DELETE(m_blacklist_model);
}

void ProxyExceptionDialog::OnInit()
{
	SetWidgetValue("black_list_radio", g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableProxy));
	SetWidgetValue("white_list_radio", !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableProxy));

	m_treeview = static_cast<OpTreeView*>(GetWidgetByName("Address_view"));
	if (m_treeview)
	{
		m_treeview->SetReselectWhenSelectedIsRemoved(TRUE);
		m_treeview->SetDragEnabled(FALSE);
		m_treeview->SetSelectedItem(0);
		m_current_model = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableProxy) ? GetBlacklistModel() : GetWhitelistModel();
		m_treeview->SetTreeModel(m_current_model);
	}

	Dialog::OnInit();
}

BOOL ProxyExceptionDialog::OnInputAction(OpInputAction* action)
{
	if (m_treeview && m_current_model)
	{
		INT32 pos = m_treeview->GetSelectedItemModelPos();
		const uni_char *address = m_current_model->GetItemString(pos);

		switch (action->GetAction())
		{
			case OpInputAction::ACTION_GET_ACTION_STATE:
			{
				OpInputAction* child_action = action->GetChildAction();

				switch (child_action->GetAction())
				{
					case OpInputAction::ACTION_NEW_ITEM:
						return TRUE;
					case OpInputAction::ACTION_DELETE_ITEM:
					case OpInputAction::ACTION_EDIT_ITEM:
						child_action->SetEnabled(pos != -1);
						return TRUE;
				}
				break;
			}
			case OpInputAction::ACTION_NEW_ITEM:
				pos = m_current_model->AddItem(UNI_L(""));
				m_treeview->EditItem(pos, 0, TRUE);
				return TRUE;
			case OpInputAction::ACTION_DELETE_ITEM:
				if (pos > -1)
				{
					if (OnItemDelete(address))
						m_current_model->Delete(pos);
				}
				return TRUE;
			case OpInputAction::ACTION_EDIT_ITEM:
				if (pos > -1)
				{
					m_treeview->EditItem(pos, 0, TRUE);
				}
				return TRUE;
		}
	}
	return Dialog::OnInputAction(action);
}

void ProxyExceptionDialog::OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	OP_ASSERT(m_treeview == widget);

	if(m_treeview == widget && m_current_model)
	{
		OpTreeModelItem* item = m_treeview->GetItemByPosition(pos);
		if(item)
		{
			INT32 model_pos = m_treeview->GetModelPos(pos);
			if(model_pos > -1)
			{
				OpString oldaddress;
				RETURN_VOID_IF_ERROR(oldaddress.Set(m_current_model->GetItemString(model_pos)));

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

void ProxyExceptionDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->IsNamed("black_list_radio") || widget->IsNamed("white_list_radio"))
	{
		m_current_model = GetWidgetValue("black_list_radio") == 1 ? GetBlacklistModel() : GetWhitelistModel();
		if (m_treeview)
			m_treeview->SetTreeModel(m_current_model);

		g_input_manager->UpdateAllInputStates();
	}
}

void ProxyExceptionDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == m_treeview)
	{
		OpTreeModelItem* item = m_treeview->GetItemByPosition(pos);

		if (item)
		{
			if (nclicks == 2)
			{
				INT32 model_pos = m_treeview->GetModelPos(pos);
				if (model_pos > -1)
				{
					m_treeview->EditItem(model_pos, 0, TRUE);
				}
			}
		}
	}
}

UINT32 ProxyExceptionDialog::OnOk()
{
	TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::EnableProxy, GetWidgetValue("black_list_radio")));
	return 0;
}

BOOL ProxyExceptionDialog::OnItemEdit(const OpString& old_item, OpString& new_item)
{
	BOOL use_proxy = GetWidgetValue("white_list_radio") == 1;

	if(!old_item.IsEmpty())
	{
		if (!OnItemDelete(old_item))
			return FALSE;
	}
	if(!new_item.IsEmpty())
	{
		// Validate the input from http://aaa.com:80, aaa.com:80 or aaa.com/something/ to aaa.com
		OpString full_url;
		RETURN_VALUE_IF_ERROR(full_url.Set(new_item), FALSE);
		if (full_url.Find("://") == KNotFound)
			RETURN_VALUE_IF_ERROR(full_url.Insert(0, "http://"), FALSE);
		URL url = urlManager->GetURL(full_url);
		ServerName* sn = url.GetServerName();
		if (sn)
			RETURN_VALUE_IF_ERROR(new_item.Set(sn->GetUniName()), FALSE);

		TRAPD(err, g_pcnet->OverridePrefL(new_item.CStr(), PrefsCollectionNetwork::EnableProxy, use_proxy, TRUE));
		return OpStatus::IsSuccess(err) && g_pcnet->IsPreferenceOverridden(PrefsCollectionNetwork::EnableProxy, new_item);
	}
	
	return FALSE;
}

BOOL ProxyExceptionDialog::OnItemDelete(const uni_char* item)
{
	TRAPD(err, g_pcnet->RemoveOverrideL(item, PrefsCollectionNetwork::EnableProxy, TRUE));
	return OpStatus::IsSuccess(err);
}

SimpleTreeModel* ProxyExceptionDialog::GetBlacklistModel()
{
	if(m_blacklist_model)
		return m_blacklist_model;

	// load m_blacklist_model
	m_blacklist_model = OP_NEW(SimpleTreeModel,());
	if(m_blacklist_model)
	{
		OpString_list* hosts = NULL;
		TRAPD(err, hosts = g_prefsManager->GetOverriddenHostsL());
		if(hosts)
		{
			for (UINT32 i = 0; i < hosts->Count(); i++)
			{
				if(g_prefsManager->IsHostOverridden(hosts->Item(i).CStr())
					&& g_pcnet->IsPreferenceOverridden(PrefsCollectionNetwork::EnableProxy, hosts->Item(i).CStr())
					&& !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableProxy, hosts->Item(i).CStr()) )
				{
					m_blacklist_model->AddItem(hosts->Item(i).CStr());
				}
			}			
		}
	
		OP_DELETE(hosts);
	}

	return m_blacklist_model;
}

SimpleTreeModel* ProxyExceptionDialog::GetWhitelistModel()
{
	if(m_whitelist_model)
		return m_whitelist_model;

	// load m_whitelist_model
	m_whitelist_model = OP_NEW(SimpleTreeModel,());
	if(m_whitelist_model)
	{
		OpString_list* hosts = NULL;
		TRAPD(err, hosts = g_prefsManager->GetOverriddenHostsL());
		if(hosts)
		{
			for (UINT32 i = 0; i < hosts->Count(); i++)
			{
				if(g_prefsManager->IsHostOverridden(hosts->Item(i).CStr()) 
					&& g_pcnet->IsPreferenceOverridden(PrefsCollectionNetwork::EnableProxy, hosts->Item(i).CStr())
					&& g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableProxy, hosts->Item(i).CStr()) )
				{
					m_whitelist_model->AddItem(hosts->Item(i).CStr());
				}
			}			
		}
	
		OP_DELETE(hosts);
	}

	return m_whitelist_model;
}
