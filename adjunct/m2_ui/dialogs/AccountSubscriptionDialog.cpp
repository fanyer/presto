/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "modules/widgets/OpDropDown.h"
#include "AccountSubscriptionDialog.h"
#include "AccountPropertiesDialog.h"
#include "adjunct/m2_ui/dialogs/MailIndexPropertiesDialog.h"
#include "adjunct/m2_ui/models/groupsmodel.h"
#include "adjunct/m2_ui/dialogs/NewAccountWizard.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/chatinfo.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/locale/oplanguagemanager.h"

// include the templates

#include "adjunct/quick/controller/SimpleDialogController.h"


class DeleteAccountFolderController : public SimpleDialogController
{
public:

	DeleteAccountFolderController(GroupsModel* tree_model, int item_id, BOOL on_server):
		SimpleDialogController(TYPE_OK_CANCEL, IMAGE_WARNING,WINDOW_NAME_DELETE_ACCOUNT_FOLDER),
		m_tree_model(tree_model),
		m_item_id(item_id),
		m_on_server(on_server)
	{}

private:
	virtual void InitL()
	{
		if (!m_tree_model)
			LEAVE(OpStatus::ERR_NULL_POINTER);

		SimpleDialogController::InitL();

		ANCHORD(OpString, title);
		ANCHORD(OpString, message);

		if (m_on_server)
		{
			g_languageManager->GetStringL(Str::D_MAILER_DELETE_FOLDER_NOTIFICATION_HEADER, title);
			g_languageManager->GetStringL(Str::D_MAILER_DELETE_FOLDER_NOTIFICATION, message);
		}
		else
		{
			GroupsModelItem* item = m_tree_model->GetItemByID(m_item_id);
			if (!item)
				LEAVE(OpStatus::ERR_NULL_POINTER);

			LEAVE_IF_ERROR(StringUtils::GetFormattedLanguageString(title, Str::S_REMOVE_FOLDER, item->GetNameCStr()));
			LEAVE_IF_ERROR(StringUtils::GetFormattedLanguageString(message, Str::S_DELETE_FOLDER_WARNING, item->GetNameCStr()));
		}
		LEAVE_IF_ERROR(SetTitle(title));
		LEAVE_IF_ERROR(SetMessage(message));
	}

	virtual void OnOk()
	{
		//Find path before item is deleted
		Indexer* indexer = MessageEngine::GetInstance()->GetIndexer();

		UINT16 account_id = m_tree_model->GetAccountId();
		Account* account = MessageEngine::GetInstance()->GetAccountById(account_id);

		OpString path;
		GroupsModelItem* item = m_tree_model->GetItemByID(m_item_id);
		if (item)
		{
			OpStatus::Ignore(item->GetPath(path));
		}

		//Delete from model (subscription-dialog) and on server
		g_m2_engine->DeleteFolder(m_tree_model, m_item_id);
		item = NULL; //Item has just been deleted

		//Delete from panel and delete messages, but not for IMAP, which handles this itself
		if (indexer && account && account->GetIncomingProtocol() != AccountTypes::IMAP && path.HasContent())
		{
			Index* index = indexer->GetSubscribedFolderIndex(account, path, 0, path, FALSE, TRUE);

			if (index)
			{
				OpINT32Vector message_ids;

				index->GetAllMessages(message_ids);

				if (MessageEngine::GetInstance()->RemoveMessages(message_ids, TRUE) != OpStatus::OK)
				{
					OP_ASSERT(0);
				}

				indexer->RemoveIndex(index);
			}
		}

		SimpleDialogController::OnOk();
	}

	GroupsModel* m_tree_model;
	int m_item_id;
	BOOL m_on_server;
};

/***********************************************************************************
**
**	AccountSubscriptionDialog
**
***********************************************************************************/

AccountSubscriptionDialog::AccountSubscriptionDialog() :
	m_accounts_treeview(NULL),
	m_groups_treeview(NULL),
	m_creating_account(FALSE),
	m_commit(FALSE),
	m_read_only(FALSE),
	// m_account_type
	m_account_id(0),
	m_delete_account_folder_controller(NULL),
	m_mail_index_properties_dialog(NULL)
{
}

/***********************************************************************************
**
**	~AccountSubscriptionDialog
**
***********************************************************************************/

AccountSubscriptionDialog::~AccountSubscriptionDialog()
{
	g_m2_engine->RemoveAccountListener(this);

	if(m_groups_treeview)
	{
		m_groups_treeview->SetTreeModel(NULL);
	}

	INT32 count = m_group_models_list.GetCount();

	for (INT32 i = 0; i < count; i++)
	{
		OpTreeModel* tree_model = (OpTreeModel *)(m_group_models_list.Get(i));
		OP_ASSERT(tree_model != 0);

		g_m2_engine->ReleaseGroupsModel(tree_model, m_commit);
	}

	if (m_delete_account_folder_controller)
		m_delete_account_folder_controller->SetListener(NULL);

	if (m_mail_index_properties_dialog)
		m_mail_index_properties_dialog->RemoveListener(this);
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

void AccountSubscriptionDialog::Init(AccountTypes::AccountType account_type, DesktopWindow* window)
{
	m_account_type = account_type == AccountTypes::UNDEFINED ? AccountTypes::NEWS : account_type;
	m_read_only = (m_account_type == AccountTypes::IRC);

    Dialog::Init(window);
}

void AccountSubscriptionDialog::Init(UINT16 account_id, DesktopWindow* window)
{
    m_account_id = account_id;

    if (account_id != 0)
    {
	    Account* account = NULL;

		g_m2_engine->GetAccountManager()->GetAccountById(account_id, account);

        if (account)
        {
		    Init(account->GetIncomingProtocol(), window);
			return;
        }
    }

    Init(AccountTypes::UNDEFINED, window);
}

/***********************************************************************************
**
**	GetHelpAnchor
**
***********************************************************************************/

const char* AccountSubscriptionDialog::GetHelpAnchor()
{
	switch (m_account_type)
	{
		case AccountTypes::NEWS:
			return "mail.html#news";

		case AccountTypes::RSS:
			return "feeds.html";

		case AccountTypes::IRC:
			return "chat.html";

		case AccountTypes::IMAP:
			return "mail.html";
	}

	return NULL;
}

/***********************************************************************************
**
**	OnInitVisibility
**
***********************************************************************************/

void AccountSubscriptionDialog::OnInitVisibility()
{
	if (m_account_type != AccountTypes::IRC)
	{
		ShowWidget("Join_button", FALSE);
		ShowWidget("Refresh_button", FALSE);
	}
	else
	{
		ShowWidget("Del_folder_button", FALSE);
	}

	if (m_account_type == AccountTypes::NEWS)
	{
		ShowWidget("New_folder_button", FALSE);
		ShowWidget("Del_folder_button", FALSE);
	}
	if (m_account_type == AccountTypes::RSS)
	{
		ShowWidget("Accounts_treeview", FALSE);
		ShowWidget("Account_button", FALSE);
	}

	if (m_account_type != AccountTypes::RSS &&
		m_account_type != AccountTypes::IMAP)
	{
		ShowWidget("Edit_folder_button", FALSE);
	}
}

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void AccountSubscriptionDialog::OnInit()
{
	m_accounts_treeview = (OpTreeView*) GetWidgetByName("Accounts_treeview");
	m_groups_treeview = (OpTreeView*) GetWidgetByName("Groups_treeview");

	if(m_groups_treeview || m_groups_treeview)

	m_creating_account = FALSE;
	m_commit = FALSE;

	//AccountManager* account_manager = g_m2_engine->GetAccountManager();
	OpString title;

	if (m_account_type == AccountTypes::NEWS)
	{
		g_languageManager->GetString(Str::D_SUBSCRIBE_NEWSGROUPS, title);
	}
	else if (m_account_type == AccountTypes::IMAP)
	{
		g_languageManager->GetString(Str::D_SUBSCRIBE_IMAP, title);
	}
	else if (m_account_type == AccountTypes::RSS)
	{
		g_languageManager->GetString(Str::D_SUBSCRIBE_RSS, title);
	}
	else if (m_account_type == AccountTypes::IRC)
	{
		g_languageManager->GetString(Str::D_SUBSCRIBE_CHAT, title);
	}

	SetTitle(title.CStr());

	OpQuickFind* quickfind = (OpQuickFind*) GetWidgetByName("Quickfind_edit");

	if(quickfind)
	{
		quickfind->SetTarget(m_groups_treeview);
		quickfind->SetFocus(FOCUS_REASON_OTHER);
	}

	if (m_accounts_treeview)
	{
		OpTreeModel* tree_model;

		g_m2_engine->GetAccountsModel(&tree_model);
		m_accounts_treeview->SetMatchType((MatchType) m_account_type);
		m_accounts_treeview->SetDeselectable(FALSE);
		m_accounts_treeview->SetTreeModel(tree_model);
		m_accounts_treeview->SetSelectedItemByID(m_account_id);

		if (!m_accounts_treeview->GetSelectedItem())
			m_accounts_treeview->SetSelectedItem(m_accounts_treeview->GetItemByLine(0));

		m_accounts_treeview->SetColumnVisibility(1, FALSE);
		m_accounts_treeview->SetColumnVisibility(2, FALSE);
	}

	g_m2_engine->AddAccountListener(this);
}

/***********************************************************************************
**
**	OnItemChanged
**
***********************************************************************************/

void AccountSubscriptionDialog::OnItemChanged(OpWidget *widget, INT32 pos)
{
	if (widget == m_groups_treeview)
	{
		OpTreeModelItem* item = m_groups_treeview->GetItemByPosition(pos);

		if (item)
		{
			OpString name;
			g_m2_engine->SubscribeFolder(m_groups_treeview->GetTreeModel(), item->GetID(), m_groups_treeview->IsItemChecked(pos));
		}
	}
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void AccountSubscriptionDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_accounts_treeview)
	{
		OpTreeModelItem* item = m_accounts_treeview->GetSelectedItem();

		if (!item)
		{
			m_groups_treeview->SetTreeModel(NULL);
			return;
		}

		m_account_id = item->GetID();

		const BOOL irc = m_account_type == AccountTypes::IRC;

		GroupsModel* groups_model = 0;
		m_group_models_hashtable.GetData(m_account_id, &groups_model);

		if (!groups_model)
		{
			OpTreeModel *tree_model = 0;
			g_m2_engine->GetGroupsModel(&tree_model, m_account_id, m_read_only);

			if (!tree_model)
			{
				m_groups_treeview->SetTreeModel(NULL);
				return;
			}
			groups_model = (GroupsModel*) tree_model;

			m_group_models_hashtable.Add(m_account_id, groups_model);
			m_group_models_list.Add(groups_model);
		}

		m_groups_treeview->SetCheckableColumn(m_read_only ? -1 : 0);
		m_groups_treeview->SetTreeModel((OpTreeModel *)(groups_model), irc ? 1 : 0, irc ? FALSE : TRUE);
		m_groups_treeview->SetColumnVisibility(2, irc);
		m_groups_treeview->SetColumnWeight(1, 50);
		m_groups_treeview->SetColumnWeight(2, 200);
		m_groups_treeview->SetAction(irc ? OP_NEW(OpInputAction, (OpInputAction::ACTION_JOIN_CHAT_ROOM)) : NULL);

		if (GetShowProgressIndicator())
		{
			UpdateProgressInformation(groups_model);

			// Force state update of the refresh button.
			g_input_manager->UpdateAllInputStates();
		}
	}
	else
	{
		Dialog::OnChange(widget, changed_by_mouse);
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL AccountSubscriptionDialog::OnInputAction(OpInputAction* action)
{
	if (!m_groups_treeview)
		return FALSE;

	OpTreeModelItem* item = m_groups_treeview->GetSelectedItem();

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			OP_ASSERT(child_action != 0);

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_REFRESH_CHATROOM_LIST:
				{
					GroupsModel* groups_model = 0;
					m_group_models_hashtable.GetData(m_account_id, &groups_model);
					OP_ASSERT(groups_model != 0);

					child_action->SetEnabled(groups_model->FolderLoadingCompleted());
					return TRUE;
				}
				case OpInputAction::ACTION_NEW_FOLDER:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_DELETE:
				case OpInputAction::ACTION_EDIT_PROPERTIES:
				{
					GroupsModelItem* group_item = static_cast<GroupsModelItem*>(item);
					BOOL enabled = item && group_item->IsEditable();
					child_action->SetEnabled(enabled);
					return TRUE;
				}
			}

			return FALSE;
		}

		case OpInputAction::ACTION_NEW_ACCOUNT:
		{
			g_application->CreateAccount(m_account_type, NULL, this);
			m_creating_account = TRUE;
			return TRUE;
		}
		case OpInputAction::ACTION_NEW_FOLDER:
		{
			if (m_account_type == AccountTypes::IRC)
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_JOIN_CHAT_ROOM, m_account_id, NULL, this);
			}
			else if (m_groups_treeview->GetTreeModel())
			{
				UINT32 id = 0;

				g_m2_engine->CreateFolder(m_groups_treeview->GetTreeModel(), &id);

				INT32 pos = m_groups_treeview->GetItemByID(id);

				if (pos != -1)
				{
					if (m_account_type == AccountTypes::IMAP)
					{
						m_groups_treeview->EditItem(pos);
					}
					else
					{
						m_groups_treeview->SetSelectedItem(pos);
						g_input_manager->InvokeAction(OpInputAction::ACTION_EDIT_PROPERTIES, 1);
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_DELETE_ACCOUNT:
		{
			if (action->GetAction() == OpInputAction::ACTION_DELETE_ACCOUNT || action->GetFirstInputContext() == m_accounts_treeview)
			{
				return g_application->DeleteAccount(m_account_id, this);
			}
			else if (item)
			{
				UINT32 item_id = item->GetID();
				OpString name;
				GroupsModelItem* group_item = static_cast<GroupsModelItem*>(item);

				if (!m_delete_account_folder_controller && group_item->IsEditable())
				{

					m_delete_account_folder_controller = OP_NEW(DeleteAccountFolderController,
						(static_cast<GroupsModel*>(m_groups_treeview->GetTreeModel()), item_id, m_account_type == AccountTypes::IMAP));

					if (m_delete_account_folder_controller)
					{
						OpStatus::Ignore(ShowDialog(m_delete_account_folder_controller, g_global_ui_context, this));
						m_delete_account_folder_controller->SetListener(this);
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_EDIT_PROPERTIES:
		case OpInputAction::ACTION_EDIT_ACCOUNT:
		{
			if (action->GetAction() == OpInputAction::ACTION_EDIT_ACCOUNT || action->GetFirstInputContext() == m_accounts_treeview)
			{
				OpTreeModelItem* account_item = m_accounts_treeview->GetSelectedItem();

				if (!account_item)
					return FALSE;

				return g_application->EditAccount(account_item->GetID(), this, TRUE);
			}
			else if (item)
			{
				UINT32 index_id = g_m2_engine->GetIndexIdFromFolder(item);
				OpString name;

				if (index_id >= IndexTypes::FIRST_NEWSFEED && index_id < IndexTypes::LAST_NEWSFEED)
				{
					const BOOL is_first_newsfeed_edit = (action->GetActionData() == 1) ? TRUE : FALSE;

					if (!m_mail_index_properties_dialog)
					{
						MailIndexPropertiesDialog* mail_index_properties_dialog = OP_NEW(MailIndexPropertiesDialog, (is_first_newsfeed_edit));
						if (mail_index_properties_dialog)
						{
							mail_index_properties_dialog->Init(this, index_id, m_groups_treeview->GetTreeModel(), item->GetID());
							mail_index_properties_dialog->AddListener(this);
							m_mail_index_properties_dialog = mail_index_properties_dialog;
						}
					}
				}
				else if (index_id >= IndexTypes::FIRST_IMAP && index_id < IndexTypes::LAST_IMAP)
				{
					INT32 pos = m_groups_treeview->GetItemByID(item->GetID());

					if (pos != -1)
					{
						m_groups_treeview->EditItem(pos);
					}
				}
			}
			return TRUE;
		}
#ifdef IRC_SUPPORT
		case OpInputAction::ACTION_JOIN_CHAT_ROOM:
		{
			if (item && !action->GetActionData())
			{
				OpTreeModelItem::ItemData item_data(COLUMN_QUERY);
				OpString name;

				item_data.column_query_data.column = 0;
				item_data.column_query_data.column_text = &name;

				item->GetItemData(&item_data);

				ChatInfo chat_info(name,OpStringC());

				g_application->GoToChat(m_account_id, chat_info, TRUE);
				return TRUE;
			}
			return FALSE;
		}
#endif // IRC_SUPPORT
		case OpInputAction::ACTION_REFRESH_CHATROOM_LIST:
		{
			GroupsModel* groups_model = 0;
			m_group_models_hashtable.GetData(m_account_id, &groups_model);
			OP_ASSERT(groups_model != 0);

			// Refresh all information in the model.
			groups_model->Refresh();

			UpdateProgressInformation(groups_model);
			g_input_manager->UpdateAllInputStates();

			return TRUE;
		}
	}

	return Dialog::OnInputAction(action);
}


/***********************************************************************************
**
**	OnItemEdited
**
***********************************************************************************/

void AccountSubscriptionDialog::OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text)
{
	if (column != 0)
		return;

	// subscribe to item

	int item_id = m_groups_treeview->GetItemByPosition(pos)->GetID();

	g_m2_engine->SubscribeFolder(m_groups_treeview->GetTreeModel(), item_id, TRUE);

	// rename item

	g_m2_engine->UpdateFolder(m_groups_treeview->GetTreeModel(), item_id, text, text);
}


/***********************************************************************************
**
**	GetShowProgressIndicator
**
***********************************************************************************/

BOOL AccountSubscriptionDialog::GetShowProgressIndicator() const
{
	BOOL show = FALSE;

	switch (m_account_type)
	{
		case AccountTypes::IRC :
			show = TRUE;
			break;
		default :
			show = FALSE;
			break;
	}

	return show;
}


/***********************************************************************************
**
**	OnFolderAdded
**
***********************************************************************************/

void AccountSubscriptionDialog::OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable)
{
	if (account_id == m_account_id)
		UpdateProgressInformation();
}

/***********************************************************************************
**
**	OnFolderLoadingCompleted
**
***********************************************************************************/

void AccountSubscriptionDialog::OnFolderLoadingCompleted(UINT16 account_id)
{
	if (account_id == m_account_id)
	{
		// Set the completed flag, since we don't know in which order the
		// event handlers are called.
		GroupsModel* groups_model = 0;
		m_group_models_hashtable.GetData(m_account_id, &groups_model);

		OP_ASSERT(groups_model != 0);
		groups_model->SetFolderLoadingCompleted(TRUE);

		// Update the progress and button state.
		UpdateProgressInformation();
		g_input_manager->UpdateAllInputStates();
	}
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void AccountSubscriptionDialog::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == m_accounts_treeview && !down && button == MOUSE_BUTTON_2)
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Account Item Menu", PopupPlacement::AnchorAtCursor());
	}
}

/***********************************************************************************
**
**	OnAccountAdded
**
***********************************************************************************/

void AccountSubscriptionDialog::OnAccountAdded(UINT16 account_id)
{
	if (m_creating_account || !m_accounts_treeview->GetSelectedItem())
	{
		m_accounts_treeview->SetSelectedItemByID(account_id);
	}

	m_creating_account = FALSE;
}

/***********************************************************************************
**
**	OnAccountRemoved
**
***********************************************************************************/

void AccountSubscriptionDialog::OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type)
{
	OpTreeModelItem* item = m_accounts_treeview->GetSelectedItem();

	if (!item)
	{
		m_groups_treeview->SetTreeModel(NULL);
		return;
	}
}

/***********************************************************************************
**
**	UpdateProgressInformation
**
***********************************************************************************/

void AccountSubscriptionDialog::UpdateProgressInformation(GroupsModel *groups_model)
{
	if (!GetShowProgressIndicator())
		return;

	const INT32 item_count = m_groups_treeview->GetItemCount();
	OpString label;

	switch (m_account_type)
	{
		case AccountTypes::IRC :
		{
			OpString unformatted_label;
			g_languageManager->GetString(Str::D_ACCOUNT_SUBSCRIPTION_ROOM_COUNT, unformatted_label);

			if (unformatted_label.HasContent())
				{
					label.AppendFormat(unformatted_label.CStr(), item_count);
				}
			break;
		}
		default :
			OP_ASSERT(0);
			break;
	}

	if (groups_model == 0)
	{
		m_group_models_hashtable.GetData(m_account_id, &groups_model);
		OP_ASSERT(groups_model != 0);
	}

	if (groups_model->FolderLoadingCompleted())
		UpdateProgress(item_count, item_count, label.CStr(), TRUE);
	else
	{
		if (item_count == 0)
		{
			OpString fetching_room_list;
			g_languageManager->GetString(Str::D_FETCHING_ROOM_LIST, fetching_room_list);

			UpdateProgress(0, 0, fetching_room_list.CStr(), TRUE);
		}
		else
			UpdateProgress(item_count * 2000, 0, label.CStr());
	}
}

/***********************************************************************************
**
**	OnDialogClosing
**
***********************************************************************************/

void AccountSubscriptionDialog::OnDialogClosing(DialogContext* dialog)
{
	if (dialog == m_delete_account_folder_controller)
		m_delete_account_folder_controller = NULL;
}

/***********************************************************************************
**
**	OnDesktopWindowClosing
**
***********************************************************************************/

void AccountSubscriptionDialog::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (desktop_window == m_mail_index_properties_dialog)
		m_mail_index_properties_dialog = NULL;
}

#endif //M2_SUPPORT
