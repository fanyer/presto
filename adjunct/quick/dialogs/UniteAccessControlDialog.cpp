/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Karianne Ekern, Adam Minchinton
 */
#include "core/pch.h"

#ifdef GADGET_SUPPORT

#ifdef WEBSERVER_SUPPORT

# include "UniteAccessControlDialog.h"

#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/widgets/OpComposeEdit.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/hotlist/HotlistManager.h"

/***********************************************************************************
 **
 **	Init
 **
 ***********************************************************************************/

void InviteFriendsDialog::Init(DesktopWindow* parent_window, OpString& address, OpString& service)
{
	m_address.Set(address.CStr());
	m_service.Set(service.CStr());
	Dialog::Init(parent_window);
}


/***********************************************************************************
 **
 **	InviteFriendsDialog
 **
 ***********************************************************************************/

InviteFriendsDialog::InviteFriendsDialog(BOOL invite):
	m_invite(invite)
{
}

InviteFriendsDialog::~InviteFriendsDialog()
{
	//destruct
}


/***********************************************************************************
 **
 **	OnInit
 **
 ***********************************************************************************/
void InviteFriendsDialog::OnInit()
{
	// AJMTODO: Strings
	OpString title;
	if (!m_invite)
	{
		title.AppendFormat(UNI_L("Add people to share %s with"), m_service.CStr()); 
	}
	else
	{
		title.AppendFormat(UNI_L("Invite people to %s"), m_service.CStr()); 
	}

	SetTitle(title);

	OpEdit* subject_edit = (OpEdit*)GetWidgetByName("subject_edit");
	OpMultilineEdit* message_edit = (OpMultilineEdit*)GetWidgetByName("message_edit");

	if (m_service.HasContent())
	{
		OpString subject;
		subject.AppendFormat(UNI_L("Check out my %s"), m_service.CStr());
		subject_edit->SetText(subject.CStr());
	
		OpString msg;
		msg.AppendFormat(UNI_L("Hi!\nTake a look at my %s.\n"), m_service.CStr());
		if (m_address.HasContent())
		{
			msg.AppendFormat(UNI_L("It's available at %s\n"), m_address.CStr());
			OpStringC username = g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser);
			msg.AppendFormat(UNI_L("\n\n%s"), username.CStr());
		}
		message_edit->SetText(msg.CStr());
	}

	OpButton* to_button = (OpButton*)GetWidgetByName("to_button");

	to_button->SetButtonType(OpButton::TYPE_TOOLBAR);
	to_button->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
	// AJMTODO: Justify text to left

	OpComposeEdit* compose_edit = (OpComposeEdit*)GetWidgetByName("to_edit");
	compose_edit->autocomp.SetType(AUTOCOMPLETION_CONTACTS);

	compose_edit->SetDropdown(UNI_L("Compose To Contact Menu"));
}


/***********************************************************************************
 **
 **	OnInit
 **
 ***********************************************************************************/

BOOL InviteFriendsDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	    case OpInputAction::ACTION_OK:
		{
			// AJMTODO: validity check mail addresses?
			OpEdit* edit = (OpEdit*)GetWidgetByName("to_edit");
			m_users.Wipe();
			OpStatus::Ignore(edit->GetText(m_users));
			
			// TODO: Get the mail subject and body
			
			return Dialog::OnInputAction(action);
		}
		case OpInputAction::ACTION_CANCEL:
		{
			return Dialog::OnInputAction(action);
		}
	    case OpInputAction::ACTION_COMPOSE_TO_CONTACT:
		{
			AppendContact(action->GetActionData(),
						  action->GetActionDataString());
			return TRUE;
		}

	}
	return FALSE;
}


/***********************************************************************************
 **
 **	GetUsers
 **
 ***********************************************************************************/

void InviteFriendsDialog::GetUsers(OpString& address)
{
	address.Set(m_users.CStr());
}


/***********************************************************************************
 **
 **	AppendContact
 **
 ***********************************************************************************/

void InviteFriendsDialog::AppendContact(INT32 contact_id, const OpStringC& contact_string)
{
	OpComposeEdit* edit = (OpComposeEdit*)GetWidgetByName("to_edit");

	if (edit)
	{
		OpString str;
		edit->GetText(str);

		g_hotlist_manager->GetContactAddresses(contact_id, str, contact_string);
		
		edit->SetText(str.CStr());
		edit->SetCaretPos(str.Length());
	}
}



/////////////////////////////////////////////////////////////////////


/***********************************************************************************
 **
 **	Init
 **
 ***********************************************************************************/

void UniteAccessControlDialog::Init(DesktopWindow* parent_window, HotlistModelItem* item)
{
	m_item = item;
	m_gadget_id = item->GetID();
	if (m_item)
		m_model = m_item->GetModel();

	Dialog::Init(parent_window);
}

/***********************************************************************************
 **
 **	AddItem
 **
 ***********************************************************************************/

void UniteAccessControlDialog::AddItem(OpString* address)
{
	if (address && address->HasContent())
		m_user_model->AddItem(address->CStr());
}


/***********************************************************************************
 **
 **	UniteAccessControlDialog
 **
 ***********************************************************************************/

UniteAccessControlDialog::UniteAccessControlDialog()
	: m_item(0), 
	  m_model(0),
	  m_gadget_id(-1),
	  m_user_model(0),
	  m_selected_friends_radio(0),
	  m_invite_dialog(0)
{
}

UniteAccessControlDialog::~UniteAccessControlDialog()
{
	//destruct
}


/***********************************************************************************
 **
 **	OnInit
 **
 ***********************************************************************************/
void UniteAccessControlDialog::OnInit()
{
	// Temporary
	m_user_model = OP_NEW(SimpleTreeModel, ());

	m_user_model->AddItem(UNI_L("user@mail.com"));

	OpLabel* label_desc = (OpLabel*)GetWidgetByName("address__label");

	if (label_desc)
	{
		label_desc->SetWrap(TRUE);
	}
	
	GadgetModelItem* gmi = GetGadgetModelItem();
	if (gmi)
	{
		m_address.Set(g_webserver_manager->GetRootServiceAddress());

		OpGadget *gadget = gmi->GetOpGadget();
		if(gadget && m_address.HasContent())
		{
			const uni_char *service_name = NULL;
			service_name = gadget->GetUIData(UNI_L("serviceName"));			
			m_service_name.Set(service_name);
			m_address.Append(service_name);

		}

		SetWidgetText("address__label", m_address.CStr());
	}

	OpTreeView* edit = (OpTreeView *)GetWidgetByName("these_treeview");
	
	if(edit)
	{
		edit->SetTreeModel(m_user_model);
		edit->SetSelectedItem(0);
		edit->SetReselectWhenSelectedIsRemoved(TRUE);
		edit->SetDragEnabled(FALSE);
	}

#if 0
	m_selected_friends_radio   = (OpRadioButton *)GetWidgetByName("Selected_friends");
	if(edit && m_selected_friends_radio)
	{
		BOOL is_set = m_selected_friends_radio->GetValue();
	}
#endif
}


/***********************************************************************************
 **
 **	GetGadgetModelItem
 **
 ***********************************************************************************/

GadgetModelItem* UniteAccessControlDialog::GetGadgetModelItem()
{
	if( m_model )
	{
		HotlistModelItem* hmi = m_model->GetItemByID( m_gadget_id );
		if (hmi && hmi->IsUniteService())
			return static_cast<GadgetModelItem*>(hmi);
	}
	return NULL;
}

/***********************************************************************************
 **
 **	OnOk
 **
 ***********************************************************************************/

void UniteAccessControlDialog::OnOk(Dialog* dialog, UINT32 result)
{
	if (((InviteFriendsDialog*)dialog) == m_invite_dialog)
	{
		OpString users;
		m_invite_dialog->GetUsers(users);
		OpVector<OpString> addresses;
		if (OpStatus::IsSuccess(StringUtils::SplitString( addresses, users, ',', FALSE)))
		{
			for (UINT32 i = 0; i < addresses.GetCount(); i++)
			{
				OpString* str = addresses.Get(i);
				if (str)
				{
					str->Strip();
					AddItem(str);
				}
			}
		}
	}
	m_invite_dialog = 0;
}


/***********************************************************************************
 **
 **	OnInputAction
 **
 ***********************************************************************************/

BOOL UniteAccessControlDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	    case OpInputAction::ACTION_OK:
		{
			SaveSettings();
			return Dialog::OnInputAction(action);
		}
	    case OpInputAction::ACTION_CANCEL:
		{
			return Dialog::OnInputAction(action);
		}
	    case OpInputAction::ACTION_INVITE_FRIENDS:
		{
			InviteFriendsDialog* dialog = OP_NEW(InviteFriendsDialog, (TRUE));
			if (dialog)
			{
				dialog->SetDialogListener(this);
				m_invite_dialog = dialog;
				dialog->Init(this, m_address, m_service_name);
			}
			
			return TRUE;
		}
	    case OpInputAction::ACTION_REMOVE_UNITE_USER:
		{
			OpTreeView *edit = (OpTreeView *)GetWidgetByName("these_treeview");

			if(edit)
			{
				INT32 pos = edit->GetSelectedItemModelPos();

				if(pos > -1)
				{
					m_user_model->Delete(pos);
				}
			}
			return TRUE;
		}
	    case OpInputAction::ACTION_COPY_UNITE_ADDRESS:
		{
			if(m_address.HasContent())
			{
				g_desktop_clipboard_manager->PlaceText(m_address.CStr(), g_application->GetClipboardToken());
			}
			return TRUE;
		}
	    case OpInputAction::ACTION_ADD_UNITE_USER:
		{
			// TODO: Add dialog
			InviteFriendsDialog* dialog = OP_NEW(InviteFriendsDialog, (FALSE));
			if (dialog)
			{
				dialog->SetDialogListener(this);
				m_invite_dialog = dialog;
				dialog->Init(this, m_address, m_service_name);
			}
			return TRUE;
		}
	}
	return FALSE;
}

/***********************************************************************************
 **
 **	SaveSettings
 **
 **
 ***********************************************************************************/

BOOL UniteAccessControlDialog::SaveSettings()
{
	OpRadioButton* everyone = (OpRadioButton*)GetWidgetByName("radio_everyone");
	OpRadioButton* selected = (OpRadioButton*)GetWidgetByName("radio_friends");
	OpRadioButton* none     = (OpRadioButton*)GetWidgetByName("radio_noone");

	if (everyone->GetValue())
	{

	}
	else if (selected->GetValue())
	{

		
		OpCheckBox* box = (OpCheckBox*)GetWidgetByName("myopera_checkbox");
		if (box->GetValue())
		{

		}
		else
		{

			// Get the treeview changes
		}



	}
	else if (none->GetValue())
	{

	}

	return TRUE;
}

#endif // WEBSERVER_SUPPORT

#endif // GADGET_SUPPORT
