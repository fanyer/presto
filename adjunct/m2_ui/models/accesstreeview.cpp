/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h"
#ifdef M2_SUPPORT

#include "accesstreeview.h"
#include "adjunct/desktop_util/mail/mailformatting.h"
#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/desktop_util/mail/mailto.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2_ui/controllers/LabelPropertiesController.h"
#include "adjunct/m2_ui/dialogs/AccountPropertiesDialog.h"
#include "adjunct/m2_ui/dialogs/AccountSubscriptionDialog.h"
#include "adjunct/m2_ui/dialogs/DeleteFolderDialog.h"
#include "adjunct/m2_ui/dialogs/MailIndexPropertiesDialog.h"
#include "adjunct/m2_ui/dialogs/SearchMailDialog.h"
#include "adjunct/m2_ui/panels/AccordionMailPanel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/dialogs/ContactPropertiesDialog.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/TreeViewModelItem.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/WidgetContainer.h"

DEFINE_CONSTRUCT(AccessTreeView);

class DownloadRemainingBodiesController : public SimpleDialogController
{
public:
	DownloadRemainingBodiesController(UINT32 id, MailPanel& mailpanel):
	  SimpleDialogController(TYPE_YES_NO_CANCEL,IMAGE_QUESTION,WINDOW_NAME_DOWNLOAD_REMAINING_BODIES,Str::S_MAIL_PANEL_BODY_MISSING, Str::S_MESSAGE_BODY_NOT_DOWNLOADED),
	m_id(id),
	m_mailpanel(&mailpanel)
	{}

private:
	virtual void OnOk()
	{
		g_m2_engine->EnsureAllBodiesDownloaded(m_id);
		SimpleDialogController::OnOk();
	}

	virtual void OnNo()
	{
		m_mailpanel->ExportMailIndex(m_id);
		SimpleDialogController::OnNo();
	}
	
	UINT32 m_id;
	MailPanel* m_mailpanel;
};

/*****************************************************************************
**
**	Init
**
*****************************************************************************/

OP_STATUS AccessTreeView::Init(MailPanel* panel, UINT32 category_id)
{
	m_id = category_id;
	m_mailpanel = panel;
	SetMinHeight(200);
	SetShowColumnHeaders(FALSE);
	SetShowThreadImage(TRUE);
	SetDragEnabled(TRUE);
	SetExpandOnSingleClick(FALSE);
	SetRestrictImageSize(TRUE);
	SetExtraLineHeight(2);

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_BOOKMARK)
	{
		SetDragEnabled(TRUE);
	}

	AccessModel* access_model = OP_NEW(AccessModel,(category_id, g_m2_engine->GetIndexer()));
	RETURN_IF_ERROR(access_model->Init());
	
	SetTreeModel(access_model, 0);
	SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);

	INT32 width;
	RETURN_IF_ERROR(panel->GetUnreadBadgeWidth(width));

	SetColumnFixedWidth(1, width);
	SetColumnWeight(2, 30);
	SetColumnWeight(3, 30);
	SetColumnWeight(4, 100);
	GetBorderSkin()->SetImage("Accordion Skin", "Panel Treeview Skin");

	OpString8 name;
	RETURN_IF_ERROR(name.AppendFormat("Access View %d", category_id));
	SetName(name.CStr());

	return OpStatus::OK;
}

/*****************************************************************************
**
**	OnFullModeChanged
**
*****************************************************************************/

void AccessTreeView::OnFullModeChanged(BOOL full_mode)
{
	SetLinkColumn(!full_mode && g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSingleClick)? 0 : -1); 
	SetColumnVisibility(0, TRUE); 
	SetColumnVisibility(1, TRUE); 
	SetColumnVisibility(2, full_mode); 
	SetColumnVisibility(3, full_mode); 
	SetColumnVisibility(4, full_mode); 
	SetColumnVisibility(5, FALSE); // only for menus
	SetThreadColumn(0); 
}

/*****************************************************************************
**
**	OnMouseUp
**
*****************************************************************************/

void AccessTreeView::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	OpTreeView::OnMouseUp(point, button, nclicks);
	OpTreeModelItem* item = GetSelectedItem();

	if (button == MOUSE_BUTTON_2)
	{
		ShowContextMenu(point, FALSE, FALSE);
		return;
	}

	if (item && item->GetID() && button == MOUSE_BUTTON_1 && !IsThreadImage(point, GetItemByPoint(point)))
		g_application->GoToMailView(item->GetID());
}


/*****************************************************************************
**
**	ShowContextMenu
**
*****************************************************************************/

BOOL AccessTreeView::ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context)
{
	OpPoint p( point.x+GetScreenRect().x, point.y+GetScreenRect().y);
	OpString8 menu;
	Account* feed_account = g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE);
	
	if (feed_account && ((UINT32)feed_account->GetAccountId() + IndexTypes::FIRST_ACCOUNT) == GetAccessModel()->GetCategoryID())
	{
		menu.Set("Feed Index Item Popup Menu");
	}
	else if (g_m2_engine->GetAccountManager()->GetMailNewsAccountCount() == 0 && feed_account)
	{
		menu.Set("Label Index Item Feed Only Popup Menu");
	}
	else
	{
		menu.Set("Index Item Popup Menu");
	}
	g_application->GetMenuHandler()->ShowPopupMenu(menu.CStr(), PopupPlacement::AnchorAt(p, center), 0, use_keyboard_context);
	return TRUE;
}

/*****************************************************************************
**
**	OnInputAction
**
*****************************************************************************/

BOOL AccessTreeView::OnInputAction(OpInputAction* action)
{
	if (OpTreeView::OnInputAction(action))
		return TRUE;
	AccessModelItem* item = static_cast<AccessModelItem*>(GetSelectedItem());
	
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_EDIT_PROPERTIES:
					if (item)
					{
						int id = item->GetID();
						if ((id >= IndexTypes::FIRST_ATTACHMENT && id < IndexTypes::LAST_ATTACHMENT) ||
							(id >= IndexTypes::FIRST_THREAD && id < IndexTypes::LAST_THREAD) ||
							(id >= 1 && id < IndexTypes::LAST_IMPORTANT && id != IndexTypes::SPAM) ||
							(id >= IndexTypes::FIRST_UNIONGROUP && id < IndexTypes::LAST_UNIONGROUP))
						{
							child_action->SetEnabled(FALSE);
						}
						else
						{
							child_action->SetEnabled(TRUE);
						}
					}
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				case OpInputAction::ACTION_EXPORT_MAIL_INDEX:
					child_action->SetEnabled( item && item->GetType() == OpTreeModelItem::INDEX_TYPE);
					return TRUE;
				case OpInputAction::ACTION_DELETE:
					child_action->SetEnabled( item && item->GetType() == OpTreeModelItem::INDEX_TYPE &&
							((item->GetID() >= IndexTypes::FIRST_CONTACT && item->GetID() < IndexTypes::FIRST_LABEL) || item->GetID() > IndexTypes::LAST_LABEL) && !IsImapInbox(item->GetID()));
					return TRUE;
				case OpInputAction::ACTION_NEW_FOLDER:
				{
					// in the context menu for mailing lists we want "New folder" instead of "New filter"
					if (GetAccessModel()->GetCategoryID() == IndexTypes::CATEGORY_MAILING_LISTS && (!child_action->GetActionDataString() || uni_strcmp(child_action->GetActionDataString(), UNI_L("union")) != 0))
						child_action->SetEnabled(FALSE);
					else
						child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_MARK_ALL_AS_READ:
					child_action->SetEnabled(item && item->GetType() == OpTreeModelItem::INDEX_TYPE);
					return TRUE;

				case OpInputAction::ACTION_RELOAD:
					child_action->SetEnabled(item && item->GetType() == OpTreeModelItem::INDEX_TYPE);
					return TRUE;
				case OpInputAction::ACTION_RENAME:
					child_action->SetEnabled(item && (item->GetID() >= IndexTypes::FIRST_UNIONGROUP && item->GetID() <= IndexTypes::LAST_UNIONGROUP));
					return TRUE;
			}
			return FALSE;
		}
		
		case OpInputAction::ACTION_NEW_FOLDER:
		{
			index_gid_t parent = GetAccessModel()->GetCategoryID();
			
			if (!action->HasActionDataString() || uni_strcmp(action->GetActionDataString(), UNI_L("union")) != 0)
				parent = IndexTypes::CATEGORY_LABELS;
				

			if (parent == IndexTypes::CATEGORY_LABELS)
			{
				if (item && item->GetID() >= IndexTypes::FIRST_FOLDER && item->GetID() < IndexTypes::LAST_FOLDER )
				{
					parent = item->GetID();
				}
			}
			else
			{
				if (item && item->GetID() >= IndexTypes::FIRST_UNIONGROUP && item->GetID() < IndexTypes::LAST_UNIONGROUP )
				{
					parent = item->GetID();
				}
				else if (item)
				{
					// if the parent of this index is not the category then it's a folder, we should that index as parent for the new folder
					Index* selected_index = g_m2_engine->GetIndexById(item->GetID());
					if (selected_index && selected_index->GetParentId() && selected_index->GetParentId() != GetAccessModel()->GetCategoryID())
						parent = selected_index->GetParentId();
				}
			}

			OpStatus::Ignore(CreateNewFolder(parent));

			return TRUE;
		}

		case OpInputAction::ACTION_NEW_NEWSFEED:
		{
			Index* index = g_m2_engine->CreateIndex(GetAccessModel()->GetCategoryID(), TRUE);
			if (index)
			{
				INT32 pos = GetItemByID(index->GetId());
				SetSelectedItem(pos);

				// This sould only happen for feeds, it will hopefully soon become the feed properties dialog
				MailIndexPropertiesDialog *dialog = OP_NEW(MailIndexPropertiesDialog, (TRUE));
				if (dialog)
					dialog->Init(GetParentDesktopWindow(), index->GetId());
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
			ShowContextMenu(GetBounds().Center(),TRUE,TRUE);
			return TRUE;

	}

	// then actions that need selected items

	if (item == NULL)
	{
		switch (action->GetAction())
		{
			case OpInputAction::ACTION_CUT:
			case OpInputAction::ACTION_COPY:
			case OpInputAction::ACTION_PASTE:
			case OpInputAction::ACTION_DELETE:
			case OpInputAction::ACTION_EDIT_PROPERTIES:
			case OpInputAction::ACTION_RENAME:
			case OpInputAction::ACTION_RELOAD:
			case OpInputAction::ACTION_EXPORT_MAIL_INDEX:
				return TRUE;
			default:
				return FALSE;
		}
	}

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_COMPOSE_MAIL:
		{
			if (IndexTypes::FIRST_CONTACT <= item->GetID() && item->GetID() < IndexTypes::LAST_CONTACT)
			{
				// Try to compose a mail to the selected contact
				Index* m2index = g_m2_engine->GetIndexById(item->GetID());
				OpString contact_address, empty, formatted_address;
				HotlistModelItem* contact_item = NULL;
				OpINT32Vector id_list;

				if (m2index &&
					m2index->GetContactAddress(contact_address) == OpStatus::OK)
				{
					// Check if this is a 'real' contact so that we can display formatted name and address - if not, use address directly
					if ((contact_item = g_hotlist_manager->GetContactsModel()->GetByEmailAddress(contact_address)) != NULL &&
						id_list.Add(contact_item->GetID()) == OpStatus::OK &&
						g_hotlist_manager->GetRFC822FormattedNameAndAddress(id_list, contact_address, FALSE))
					{
						MailTo mailto;
						mailto.Init(contact_address, empty, empty, empty, empty);
						MailCompose::ComposeMail(mailto);
						return TRUE;
					}
					else if (OpStatus::IsSuccess(MailFormatting::FormatListIdToEmail(contact_address, formatted_address)))
					{
						MailTo mailto;
						mailto.Init(formatted_address, empty, empty, empty, empty);
						MailCompose::ComposeMail(mailto);
						return TRUE;
					}
				}
			}
			return FALSE;
		}
		case OpInputAction::ACTION_READ_MAIL:
		{
			if (action->GetActionData() == 0)
			{
				g_application->GoToMailView(item->GetID(), NULL, NULL, TRUE, FALSE, TRUE, action->IsKeyboardInvoked());
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_MARK_ALL_AS_READ:
		{
			if (item && item->GetType() == OpTreeModelItem::INDEX_TYPE)
			{
				g_m2_engine->IndexRead(item->GetID());
			}
			return TRUE;
		}

		case OpInputAction::ACTION_CUT:
		case OpInputAction::ACTION_COPY:
		case OpInputAction::ACTION_PASTE:
			{
				 //used to be stolen by the mail view, causing cut of messages (!)
				return TRUE;
			}
		case OpInputAction::ACTION_DELETE:
			{
				UINT32 id = item->GetID();
				if (IsImapInbox(id))
					return TRUE;
				if ((id >= IndexTypes::FIRST_FOLDER && id < IndexTypes::LAST_FOLDER) ||
					(id >= IndexTypes::FIRST_IMAP && id < IndexTypes::LAST_IMAP) ||
					(id >= IndexTypes::FIRST_ARCHIVE && id < IndexTypes::LAST_ARCHIVE) ||
					(id >= IndexTypes::FIRST_NEWSFEED && id < IndexTypes::LAST_NEWSFEED) ||
					(id >= IndexTypes::FIRST_UNIONGROUP && id < IndexTypes::LAST_UNIONGROUP))
				{
					DeleteFolderDialog* dialog = OP_NEW(DeleteFolderDialog, ());
					if (dialog)
						dialog->Init(item->GetID(), GetParentDesktopWindow());
				}
				else if (id >= IndexTypes::FIRST_CONTACT && id < IndexTypes::LAST_CONTACT)
				{
					if (g_m2_engine->IsIndexMailingList(id))
					{
						DeleteFolderDialog* dialog = OP_NEW(DeleteFolderDialog, ());
						if (dialog)
							dialog->Init(item->GetID(), GetParentDesktopWindow());
					}
					else 
					{
						Index* m2index = g_m2_engine->GetIndexById(item->GetID());
						OpString contact_address;
						if (m2index &&
							m2index->GetContactAddress(contact_address) == OpStatus::OK)
						{
							m2index->ToggleWatched(FALSE);
							ContactModelItem *contact_item = static_cast<ContactModelItem*>(g_hotlist_manager->GetContactsModel()->GetByEmailAddress(contact_address));
							if (contact_item)
							{
								contact_item->ChangeImageIfNeeded();
							}
						}
					}
				}
				else if (id >= IndexTypes::FIRST_NEWSGROUP && id < IndexTypes::LAST_NEWSGROUP)
				{
					Index* index = g_m2_engine->GetIndexById(id);
					if (index)
					{
						IndexSearch* search = index->GetSearch();
						if (search)
						{
							if (search->GetSearchText().Find(UNI_L("@")) != KNotFound)
							{
								// remove Message-Id access points without asking
								g_m2_engine->RemoveIndex(id);
								return TRUE;
							}
						}
					}
					DeleteFolderDialog* dialog = OP_NEW(DeleteFolderDialog, ());
					if (dialog)
						dialog->Init(item->GetID(), GetParentDesktopWindow());
				}
				else
				{
					g_m2_engine->RemoveIndex(id);
				}
			}
			return TRUE;

		case OpInputAction::ACTION_SEARCH_MAIL:
		{
			SearchMailDialog* dialog = OP_NEW(SearchMailDialog, ());
			if (dialog)
				dialog->Init(0, item->GetID());
			return TRUE;
		}

		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			UINT32 id = item->GetID();
			// Edit Properties for Contacts
			if (id >= IndexTypes::FIRST_CONTACT && id < IndexTypes::LAST_CONTACT)
			{
				Index* m2index = g_m2_engine->GetIndexById(id);

				if (m2index)
				{
					OpString contact_address, formatted_address;

					if (m2index->GetContactAddress(contact_address) == OpStatus::OK)
					{
						MailFormatting::FormatListIdToEmail(contact_address, formatted_address);
						g_hotlist_manager->EditItem_ByEmailOrAddIfNonExistent(formatted_address, GetParentDesktopWindow());
					}
				}
			}
			// Edit Properties for filters, spam and the filter index
			else if ((id >= IndexTypes::FIRST_FOLDER && id < IndexTypes::LAST_FOLDER) ||
					 (id >= IndexTypes::FIRST_SEARCH && id < IndexTypes::LAST_SEARCH) ||
					 (id >= IndexTypes::FIRST_LABEL  && id < IndexTypes::LAST_LABEL)  ||
					 (id == IndexTypes::SPAM) ||
					 (id == IndexTypes::CATEGORY_LABELS ))
			{
				LabelPropertiesController* controller = OP_NEW(LabelPropertiesController, (id));
				ShowDialog(controller, g_global_ui_context, GetParentDesktopWindow());
			}
			// Edit Properties for imap folders and newsgroups
			else if ((id >= IndexTypes::FIRST_IMAP && id < IndexTypes::LAST_IMAP) ||
				(id >= IndexTypes::FIRST_NEWSGROUP && id < IndexTypes::LAST_NEWSGROUP))
			{
				Index* index = g_m2_engine->GetIndexById(id);
				if (index)
				{
					AccountSubscriptionDialog* dialog = OP_NEW(AccountSubscriptionDialog, ());
					if (dialog)
						dialog->Init(index->GetAccountId(), GetParentDesktopWindow());
					// Add to the init the index id so that it scroll down to the right index
				}
			}
			// Edit Properties for accounts (imap, rss, pop, etc)
			else if (id >= IndexTypes::FIRST_ACCOUNT && id < IndexTypes::LAST_ACCOUNT)
			{
				Account* account = g_m2_engine->GetAccountById(id-IndexTypes::FIRST_ACCOUNT);
				if (account)
				{
					if (account->GetIncomingIndexType() == IndexTypes::NEWSFEED_INDEX)
					{
						AccountSubscriptionDialog* dialog = OP_NEW(AccountSubscriptionDialog, ());
						if (dialog)
							dialog->Init(AccountTypes::RSS, GetParentDesktopWindow());
					}
					else
					{
						AccountPropertiesDialog* dialog = OP_NEW(AccountPropertiesDialog, (FALSE));
						if (dialog)
							dialog->Init(account->GetAccountId(),GetParentDesktopWindow());
					}
				}
			}
			else
			{
				// This sould only happen for feeds, it will hopefully soon become the feed properties dialog
				MailIndexPropertiesDialog *dialog = OP_NEW(MailIndexPropertiesDialog, ());
				if (dialog)
					dialog->Init(GetParentDesktopWindow(), item->GetID());
			}
			return TRUE;
		}

		case OpInputAction::ACTION_EXPORT_MAIL_INDEX:
		{
			if (item && item->GetType() == OpTreeModelItem::INDEX_TYPE)
			{
				if (!g_m2_engine->HasAllBodiesDownloaded(item->GetID()))
				{
					DownloadRemainingBodiesController* controller = OP_NEW(DownloadRemainingBodiesController, (item->GetID(), *m_mailpanel));
					
					ShowDialog(controller,g_global_ui_context,GetParentDesktopWindow());
					
					return TRUE;
				}
				
				m_mailpanel->ExportMailIndex(item->GetID());
				
			}
			return TRUE;
		}
		case OpInputAction::ACTION_RELOAD:
		{
			if(!g_application->AskEnterOnlineMode(TRUE))
			{
				return TRUE;
			}
			g_m2_engine->RefreshFolder(item->GetID());
			return TRUE;
		}
		case OpInputAction::ACTION_RENAME:
		{
				INT32 pos = GetItemByID(item->GetID());
				EditItem(pos);
				return TRUE;
		}
	}

	return FALSE;
}

/*****************************************************************************
**
**	CreateNewFolder
**
*****************************************************************************/

OP_STATUS AccessTreeView::CreateNewFolder(index_gid_t parent_id)
{
	Index* index;
	if (parent_id == IndexTypes::CATEGORY_LABELS || (parent_id >= IndexTypes::FIRST_FOLDER && parent_id <= IndexTypes::LAST_FOLDER))
	{
		index = g_m2_engine->CreateIndex(parent_id, TRUE);
	}
	else
	{
		RETURN_IF_ERROR(g_m2_engine->GetIndexer()->CreateFolderGroupIndex(index, parent_id));
	}
	
	if (!index)
		return OpStatus::ERR_NO_MEMORY;

	INT32 pos = GetItemByID(index->GetId());

	EditItem(pos);
	return OpStatus::OK;
}

/*****************************************************************************
 **
 **	OnDragStart
 **
 *****************************************************************************/

void AccessTreeView::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	SetEnabled(FALSE);
	ZeroCapturedWidget();
	SetEnabled(TRUE);
	m_listener.OnDragStart(widget, pos, x, y);
}

/*****************************************************************************
**
**	OnDragMove -- OpWidgetListener class
**
*****************************************************************************/

void AccessTreeView::OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (drag_object->GetType() != DRAG_TYPE_MAIL_INDEX && drag_object->GetType() != DRAG_TYPE_MAIL)
		SetDropPos(-1);

	m_listener.OnDragMove(widget, drag_object, pos, x, y);
}

/*****************************************************************************
 **
 **	OnDragMove -- OpWidget class
 **
 *****************************************************************************/

void AccessTreeView::OnDragMove(OpDragObject* op_drag_object, const OpPoint& point)
{
	OpTreeView::OnDragMove(op_drag_object, point);

	if (op_drag_object->GetType() == WIDGET_TYPE_ACCORDION_ITEM)
		SetDropPos(-1);
}

/*****************************************************************************
**
**	SetSelectedItem
**
*****************************************************************************/

void AccessTreeView::SetSelectedItem(INT32 pos, BOOL changed_by_mouse, BOOL send_onchange, BOOL allow_parent_fallback)
{
	OpTreeView::SetSelectedItem(pos, changed_by_mouse, send_onchange, allow_parent_fallback);
	if (GetSelectedItem())
	{
		m_mailpanel->SetSelectedIndex(GetSelectedItem()->GetID());
	}
}

/*****************************************************************************
**
**	OnChange
**
*****************************************************************************/

void AccessTreeView::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpTreeModelItem* item;
	item = GetSelectedItem();
	if (!item)
		return;
	g_application->GoToMailView(item->GetID(), NULL, NULL, FALSE);
}

/*****************************************************************************
**
**	ChangeItemOpenState
**
*****************************************************************************/

void AccessTreeView::ChangeItemOpenState(TreeViewModelItem* item, BOOL open)
{
	OpTreeView::ChangeItemOpenState(item, open);
	Index* index = g_m2_engine->GetIndexById(item->GetID()); 
	if (index) 
		index->SetExpanded(open != FALSE);
}

/*****************************************************************************
**
**	ScrollToLine
**
*****************************************************************************/

void AccessTreeView::ScrollToLine(INT32 line, BOOL force_to_center)
{
	OpTreeView::ScrollToLine(line, force_to_center);
	OpWidget* accordion_item = OpWidget::GetParent();
	if (accordion_item->GetType() == WIDGET_TYPE_ACCORDION_ITEM)
	{
		OpAccordion* accordion = static_cast<OpAccordion*>(accordion_item->GetParent());
		accordion->EnsureVisibility(static_cast<OpAccordion::OpAccordionItem*>(accordion_item), GetLineHeight()*(line-1), GetLineHeight()*3);
	}
}

BOOL AccessTreeView::IsScrollable(BOOL vertical)
{
	OpWidget* accordion_item = OpWidget::GetParent();
	if (accordion_item->GetType() == WIDGET_TYPE_ACCORDION_ITEM)
	{
		OpAccordion* accordion = static_cast<OpAccordion*>(accordion_item->GetParent());
		if(accordion)
			return accordion->IsScrollable(vertical);
	}
	return OpTreeView::IsScrollable(vertical);
}

BOOL AccessTreeView::OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
{
	OpWidget* accordion_item = OpWidget::GetParent();
	if (accordion_item->GetType() == WIDGET_TYPE_ACCORDION_ITEM)
	{
		OpAccordion* accordion = static_cast<OpAccordion*>(accordion_item->GetParent());
		if(accordion)
			return accordion->OnScrollAction(delta, vertical, smooth);
	}
	return OpTreeView::OnScrollAction(delta, vertical, smooth);
}

/*****************************************************************************
**
**	IsImapInbox
**
*****************************************************************************/

BOOL AccessTreeView::IsImapInbox(UINT32 index_id)
{
	if(index_id >= IndexTypes::FIRST_IMAP && index_id < IndexTypes::LAST_IMAP)
	{
		OpString name;
		Index* index = g_m2_engine->GetIndexById(index_id);
		Account* account;
		UINT32 account_id = index ? index->GetAccountId() : 0;

		if (account_id &&
			OpStatus::IsSuccess(g_m2_engine->GetAccountManager()->GetAccountById(account_id, account)) &&
			account &&
			OpStatus::IsSuccess(index->GetName(name)))
		{
			return name.CompareI(UNI_L("INBOX")) == 0;
		}
	}

	return FALSE;
}


#endif // M2_SUPPORT
