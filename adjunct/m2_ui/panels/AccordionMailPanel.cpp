/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/

#include "core/pch.h" 
 
#ifdef M2_SUPPORT 
#include "adjunct/desktop_pi/desktop_file_chooser.h" 
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h" 
#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"
#include "adjunct/m2/src/engine/engine.h" 
#include "adjunct/m2/src/engine/accountmgr.h" 
#include "adjunct/m2/src/engine/indexer.h" 
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2_ui/controllers/LabelPropertiesController.h"
#include "adjunct/m2_ui/dialogs/AccountPropertiesDialog.h"
#include "adjunct/m2_ui/dialogs/AccountSubscriptionDialog.h"
#include "adjunct/m2_ui/models/accessmodel.h" 
#include "adjunct/m2_ui/panels/AccordionMailPanel.h" 
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h" 
#include "adjunct/quick_toolkit/widgets/OpSplitter.h" 
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h" 
#include "adjunct/desktop_util/treemodel/optreemodelitem.h" 
#include "modules/locale/oplanguagemanager.h" 
#include "modules/prefs/prefsmanager/collections/pc_m2.h" 
#include "modules/prefs/prefsmanager/collections/pc_ui.h" 
 
/*********************************************************************************** 
** 
**  MailPanel 
** 
***********************************************************************************/ 
 
MailPanel::MailPanel() 
  : m_status_bar_main(0) 
  , m_status_bar_stop(0) 
  , m_status_bar_sub(0) 
  , m_status_timer_started(FALSE) 
  , m_accordion(0) 
  , m_needs_attention(FALSE) 
  , m_chooser(0) 
  , m_export_index(0) 
  , m_unread_badge_width(0)
{ 
} 
 
void MailPanel::OnDeleted() 
{ 
	g_m2_engine->RemoveEngineListener(this);
	g_m2_engine->GetIndexer()->RemoveCategoryListener(this);
    g_m2_engine->GetMasterProgress().RemoveListener(this); 
    g_m2_engine->GetMasterProgress().RemoveNotificationListener(this); 

    OP_DELETE(m_chooser); 
 
    HotlistPanel::OnDeleted(); 
} 
 
/*********************************************************************************** 
** 
**  GetPanelText 
** 
***********************************************************************************/ 
 
void MailPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type) 
{ 
    INT32 unread_count = g_m2_engine->GetUnreadCount();

    if (unread_count && text_type != Hotlist::PANEL_TEXT_NAME) 
    { 
		OpString languagestring; 
		g_languageManager->GetString(Str::S_MAIL_PANEL_BUTTON_TEXT, languagestring); 
        text.AppendFormat(languagestring.CStr(), unread_count); 
    }
	else
	{
		g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_EMAIL, text); 
	}
} 
 
/*********************************************************************************** 
** 
**  Init 
** 
***********************************************************************************/ 
OP_STATUS MailPanel::Init() 
{ 
    OpString buttontext; 

    RETURN_IF_ERROR(OpProgressBar::Construct(&m_status_bar_main)); 
    AddChild(m_status_bar_main, TRUE); 
 
    RETURN_IF_ERROR(OpButton::Construct(&m_status_bar_stop, OpButton::TYPE_TOOLBAR, OpButton::STYLE_IMAGE)); 
    AddChild(m_status_bar_stop); 
 
    RETURN_IF_ERROR(OpProgressBar::Construct(&m_status_bar_sub)); 
    AddChild(m_status_bar_sub, TRUE); 

	RETURN_IF_ERROR(OpAccordion::Construct(&m_accordion)); 
    AddChild(m_accordion, TRUE); 
	
	m_accordion->SetOpAccordionListener(this);
	m_accordion->SetListener(this);
 
    RETURN_IF_ERROR(PopulateCategories()); 
 
    m_status_bar_main->SetType(OpProgressBar::Only_Label); 
    m_status_bar_main->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL); 
    m_status_bar_main->SetText(UNI_L("")); 
    m_status_bar_main->SetJustify(JUSTIFY_CENTER, TRUE); 
    m_status_bar_main->SetVisibility(FALSE); 
 
    m_status_bar_stop->GetForegroundSkin()->SetImage("Stop mail"); 
	m_status_bar_stop->GetBorderSkin()->SetImage("Toolbar Fallback Button Skin");
	m_status_bar_main->SetProgressBarEmptySkin("Progress Empty Skin");
	m_status_bar_sub->SetProgressBarEmptySkin("Progress Empty Skin");

    m_status_bar_stop->SetAction(OP_NEW(OpInputAction, (OpInputAction::ACTION_STOP_MAIL))); 
    m_status_bar_stop->SetVisibility(FALSE); 
 
    m_status_bar_sub->SetType(OpProgressBar::Normal); 
    m_status_bar_sub->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL); 
    m_status_bar_sub->SetText(UNI_L("")); 
    m_status_bar_sub->SetJustify(JUSTIFY_CENTER, TRUE); 
    m_status_bar_sub->SetVisibility(FALSE); 
 
	g_m2_engine->GetIndexer()->AddCategoryListener(this);
	g_m2_engine->AddEngineListener(this);
	
    RETURN_IF_ERROR(g_m2_engine->GetMasterProgress().AddListener(this)); 
    RETURN_IF_ERROR(g_m2_engine->GetMasterProgress().AddNotificationListener(this)); 
    m_status_timer.SetTimerListener(this); 
    SetName("Mail");
 
    return OpStatus::OK; 
} 
 
/*********************************************************************************** 
** 
**  OnFullModeChanged 
** 
***********************************************************************************/ 
 
void MailPanel::OnFullModeChanged(BOOL full_mode) 
{ 
    for (UINT32 id = 0; id < m_access_views.GetCount(); id ++) 
    { 
		AccessTreeView* access_view = m_access_views.Get(id); 
        access_view->OnFullModeChanged(full_mode);
	}
} 
 
/*********************************************************************************** 
** 
**  OnLayout 
** 
***********************************************************************************/ 
 
void MailPanel::OnLayoutPanel(OpRect& rect) 
{ 
	const int status_bar_row_height = 23;
    int status_bar_height = 0; 
 
    if (m_status_bar_main->IsVisible()) 
    { 
        if (m_status_bar_sub->IsVisible()) 
        { 
            status_bar_height += status_bar_row_height; 
            m_status_bar_sub->SetRect(OpRect(rect.x, rect.y + rect.height - status_bar_height, rect.width, status_bar_row_height)); 
        } 
        status_bar_height += status_bar_row_height; 
		int status_bar_stop_width = 0;
		if (m_status_bar_stop->IsVisible())
			status_bar_stop_width = status_bar_row_height;
		SetChildRect(m_status_bar_main, OpRect(rect.x, rect.y + rect.height - status_bar_height, rect.width - status_bar_stop_width, status_bar_row_height));
		if (status_bar_stop_width > 0)
			SetChildRect(m_status_bar_stop, OpRect(rect.x + rect.width - status_bar_row_height, rect.y + rect.height - status_bar_height, status_bar_stop_width, status_bar_row_height));
    } 
 
    m_accordion->SetRect(OpRect(rect.x, rect.y, rect.width, rect.height));
} 
 
void MailPanel::OnFocus(BOOL focus,FOCUS_REASON reason) 
{ 
    if (focus) 
    { 
        m_accordion->SetFocus(reason); 
    } 
} 

BOOL MailPanel::OnInputAction(OpInputAction* action)
{
	index_gid_t id = action->GetActionData();

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			id = child_action->GetActionData();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_MAIL_SHOW_INDEX:
				{
					BOOL enabled = FALSE;
					if ((IndexTypes::FIRST_CATEGORY <= id && id <= IndexTypes::LAST_CATEGORY) ||
						(IndexTypes::FIRST_ACCOUNT <= id && id <= IndexTypes::LAST_ACCOUNT))
					{
						if (!(id >= IndexTypes::FIRST_ACCOUNT && id < IndexTypes::LAST_ACCOUNT && !g_m2_engine->GetAccountManager()->IsAccountActive((UINT16)id-IndexTypes::FIRST_ACCOUNT)))
							enabled = child_action->GetActionDataString() || !g_m2_engine->GetIndexer()->GetCategoryVisible(id);
					}
					else
					{
						enabled = child_action->GetActionDataString() || !g_m2_engine->GetIndexer()->GetIndexById(id)->IsVisible();
					}
					child_action->SetEnabled(enabled);
					return TRUE;
				}
				case OpInputAction::ACTION_MAIL_HIDE_INDEX:
				{
					if (!child_action->GetActionDataString())
					{
						BOOL visible, enabled = TRUE;
						if ((IndexTypes::FIRST_CATEGORY <= id && id <= IndexTypes::LAST_CATEGORY) ||
							(IndexTypes::FIRST_ACCOUNT <= id && id <= IndexTypes::LAST_ACCOUNT))
						{
							visible = g_m2_engine->GetIndexer()->GetCategoryVisible(id);
							if ((id >= IndexTypes::FIRST_ACCOUNT && id < IndexTypes::LAST_ACCOUNT) && 
								(!g_m2_engine->GetAccountManager()->IsAccountActive((UINT16)id-IndexTypes::FIRST_ACCOUNT) || (g_application->HasFeeds() && !g_application->HasMail())))
							{
								enabled = FALSE;
							}
						}
						else
						{
							visible = g_m2_engine->GetIndexer()->GetIndexById(id)->IsVisible();
						}
						child_action->SetSelected(visible && child_action->GetActionOperator() != OpInputAction::OPERATOR_NONE);
						child_action->SetEnabled(visible && enabled);
					}
					else
					{
						if (g_m2_engine->GetIndexer()->GetSubscribedFolderIndex(g_m2_engine->GetAccountById(id), child_action->GetActionDataString(), 0, NULL, FALSE, FALSE))
						{
							child_action->SetSelected(TRUE);
							child_action->SetEnabled(TRUE);
						}
						else
						{
							child_action->SetEnabled(FALSE);
						}
					}

					return TRUE;
				}
				case OpInputAction::ACTION_NEW_FOLDER:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_RELOAD:
				{
					child_action->SetEnabled(g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE) != NULL);
					return TRUE;
				}
				case OpInputAction::ACTION_EDIT_PROPERTIES:
				{
					child_action->SetEnabled(g_m2_engine->GetIndexer()->IsCategory(child_action->GetActionData()));
					return TRUE;
				}
				case OpInputAction::ACTION_RENAME:
				case OpInputAction::ACTION_EXPORT_MAIL_INDEX:
				case OpInputAction::ACTION_DELETE:
				case OpInputAction::ACTION_MARK_ALL_AS_READ:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}
			}
			break;
		}
		case OpInputAction::ACTION_MAIL_SHOW_INDEX:
		{
			if (action->GetActionDataString())
				g_m2_engine->GetIndexer()->GetSubscribedFolderIndex(g_m2_engine->GetAccountById(id), action->GetActionDataString(), 0, action->GetActionText(), TRUE, FALSE);
			else
			{
				if ((IndexTypes::FIRST_CATEGORY <= id && id <= IndexTypes::LAST_CATEGORY) ||
					(IndexTypes::FIRST_ACCOUNT <= id && id <= IndexTypes::LAST_ACCOUNT))
				{
					g_m2_engine->GetIndexer()->SetCategoryVisible(id, TRUE);
				}
				else
				{
					g_m2_engine->GetIndexer()->GetIndexById(id)->SetVisible(TRUE);
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_MAIL_HIDE_INDEX:
		{
			if (g_input_manager->GetActionState(action,this) & OpInputAction::STATE_ENABLED)
			{
				if (action->GetActionDataString())
				{
					Index* index = g_m2_engine->GetIndexer()->GetSubscribedFolderIndex(g_m2_engine->GetAccountById(id), action->GetActionDataString(), 0, action->GetActionText(), FALSE, FALSE);	
					if (index)
						OpStatus::Ignore(g_m2_engine->GetAccountById(id)->RemoveSubscribedFolder(index->GetId()));
				}
				else
				{
					if ((IndexTypes::FIRST_CATEGORY <= id && id <= IndexTypes::LAST_CATEGORY) ||
						(IndexTypes::FIRST_ACCOUNT <= id && id <= IndexTypes::LAST_ACCOUNT))
					{
						g_m2_engine->GetIndexer()->SetCategoryVisible(id, FALSE);
					}
					else
					{
						g_m2_engine->GetIndexer()->GetIndexById(id)->SetVisible(FALSE);
					}
				}
				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_NEW_NEWSFEED:
		{
			OpAccordion::OpAccordionItem * accordion_item = m_accordion->GetItemById(action->GetActionData());
			if (accordion_item)
			{
				m_accordion->ExpandItem(accordion_item, TRUE);
				accordion_item->GetWidget()->OnInputAction(action);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_NEW_FOLDER:
		{
			index_gid_t parent_id = 0;
			if (action->GetActionData())
			{
				parent_id = action->GetActionData();
			}
			else if (g_application->HasMail()) // clicking new folder outside an accordion item when you have a mail account should default back to new label
			{
				parent_id = IndexTypes::CATEGORY_LABELS;
			}
			else if (g_application->HasFeeds()) // but when there is no mail account, it should make a new feed folder
			{
				parent_id = g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE)->GetAccountId() + IndexTypes::FIRST_ACCOUNT;
			}
			if (!parent_id)
				return TRUE;

			OpAccordion::OpAccordionItem * accordion_item = m_accordion->GetItemById(parent_id);

			if (!accordion_item) // the section is hidden... the action can still be performed in any accesstreeview, but won't make it appear
				accordion_item = m_accordion->GetItemByIndex(0);

			if (accordion_item)
			{
				m_accordion->ExpandItem(accordion_item, TRUE);
				static_cast<AccessTreeView*>(accordion_item->GetWidget())->CreateNewFolder(parent_id);
			}

			return TRUE;
		}
		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			index_gid_t id = action->GetActionData();
			// Edit Properties for filters
			if (id == IndexTypes::CATEGORY_LABELS )
			{
				LabelPropertiesController* controller = OP_NEW(LabelPropertiesController, (id));
				ShowDialog(controller, g_global_ui_context, GetParentDesktopWindow());
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
			return TRUE;
		}
		case OpInputAction::ACTION_RELOAD:
		{
			Account* rss_account = g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE);
			if (rss_account)
				g_m2_engine->RefreshFolder(rss_account->GetAccountId()+IndexTypes::FIRST_ACCOUNT);
			return TRUE;
		}		
		case OpInputAction::ACTION_EMPTY_TRASH:
		{
			g_m2_engine->EmptyTrash();
			return TRUE;
		}
		case OpInputAction::ACTION_EMPTY_SPAM:
		{
			Index* spam = g_m2_engine->GetIndexById(IndexTypes::SPAM);
			OpINT32Vector message_ids;

			for (INT32SetIterator it(spam->GetIterator()); it; it++)
			{
				message_gid_t message_id = it.GetData();

				if (g_m2_engine->GetAccountManager()->IsAccountActive(g_m2_engine->GetStore()->GetMessageAccountId(message_id)))
					message_ids.Add(message_id);
			}

			g_m2_engine->RemoveMessages(message_ids, FALSE);

			return TRUE;
		}
		
		case OpInputAction::ACTION_START_SEARCH:
		{
			OpString text;

			text.Set(action->GetActionDataString());

			if (text.IsEmpty() == FALSE)
			{
				index_gid_t id;

				g_m2_engine->GetIndexer()->StartSearch(text, SearchTypes::EXACT_PHRASE, SearchTypes::ENTIRE_MESSAGE, 0, UINT32(-1), id, 0, NULL);

				SetSelectedIndex(id);
				g_application->GoToMailView(id, NULL, NULL, TRUE, FALSE, TRUE, action->IsKeyboardInvoked());
			}
			return TRUE;
		}
	}
	return FALSE;
}

/*********************************************************************************** 
** 
**  OnStatusChanged
** 
***********************************************************************************/ 
void MailPanel::OnStatusChanged(const ProgressInfo& progress)
{
	AccountManager *mgr = g_m2_engine->GetAccountManager();
	Account *account = NULL;
	for (int i = 0; i < mgr->GetAccountCount(); i++)
	{
		account = mgr->GetAccountByIndex(i);
		if (&progress == &account->GetProgress(TRUE))
		{
			UINT32 index_id = account->GetAccountId() + IndexTypes::FIRST_ACCOUNT;
			OpAccordion::OpAccordionItem *item = m_accordion->GetItemById(index_id);
			if (item)
			{
				const char * image = GetNewIconForAccount(index_id);
				item->GetButton()->GetForegroundSkin()->SetImage(image);
			}
		}
	}
}

/*********************************************************************************** 
** 
**  OnProgressChanged 
** 
***********************************************************************************/ 
void MailPanel::OnProgressChanged(const ProgressInfo& progress) 
{ 
    OpString progress_text; 
 
    // The status timer needs to be stopped when progress changed 
    if (m_status_timer_started) 
    { 
        m_status_timer.Stop(); 
        m_status_timer_started = FALSE; 
    } 
 
    // Check if we want to show progress 
    if (progress.GetCurrentAction() == ProgressInfo::NONE) 
    { 
        if (m_status_bar_main->IsVisible() || m_status_bar_sub->IsVisible()) 
        { 
            m_status_bar_main->SetVisibility(FALSE); 
            m_status_bar_stop->SetVisibility(FALSE); 
            m_status_bar_sub->SetVisibility(FALSE); 
            OnRelayout(); 
        } 
        return; 
    } 
    // Get the text and type for on the status bar 
    RETURN_VOID_IF_ERROR(progress.GetProgressString(progress_text)); 
    if (progress.GetTotalCount() > 1 && progress.GetCount() <= progress.GetTotalCount()) 
    { 
        m_status_bar_main->SetType(OpProgressBar::Normal); 
		if (progress.DisplayAsPercentage())
			progress_text.AppendFormat(UNI_L(" (%u%%)"), (unsigned)(((double)progress.GetCount() / (double)progress.GetTotalCount()) * 100));
		else
			progress_text.AppendFormat(UNI_L(" (%u/%u)"), progress.GetCount(), progress.GetTotalCount()); 
    } 
    else 
    { 
        m_status_bar_main->SetType(OpProgressBar::Only_Label); 
    } 
    m_status_bar_main->SetText(progress_text.CStr()); 
 
    // Get the progress for this action 
    m_status_bar_main->SetProgress(progress.GetCount(), progress.GetTotalCount()); 
 
    // Redraw widget 
    if (!m_status_bar_main->IsVisible()) 
    { 
        m_status_bar_main->SetVisibility(TRUE);
		if (progress.GetCurrentAction() != ProgressInfo::LOADING_DATABASE)
			m_status_bar_stop->SetVisibility(TRUE); 
        OnRelayout(); 
    } 
    m_status_bar_main->InvalidateAll(); 
} 

/*********************************************************************************** 
** 
**  OnSubProgressChanged 
** 
***********************************************************************************/ 
void MailPanel::OnSubProgressChanged(const ProgressInfo& progress) 
{ 
    OpString sub_text; 
 
    // Get sub progress 
    if (progress.GetSubCount() > 0 && progress.GetSubCount() <= progress.GetSubTotalCount()) 
    { 
        // Get text for sub progress 
        sub_text.AppendFormat(UNI_L("%u%%"), (unsigned)(((double)progress.GetSubCount() / (double)progress.GetSubTotalCount()) * 100)); 
		m_status_bar_sub->SetType(OpProgressBar::Normal); 
        m_status_bar_sub->SetProgress(progress.GetSubCount(), progress.GetSubTotalCount()); 
 
        // Check if we want to display sub progress 
        if (!m_status_bar_sub->IsVisible()) 
        { 
            // Check if we want to start the timer 
            if (!m_status_timer_started) 
            { 
                m_status_timer.Start(2000); 
                m_status_timer_started = TRUE; 
            } 
            return; 
        } 
    } 
    else 
    { 
        m_status_bar_sub->SetType(OpProgressBar::Only_Label); 
        m_status_bar_sub->SetProgress(0, 0); 
        if (m_status_timer_started) 
        { 
            m_status_timer.Stop(); 
            m_status_timer_started = FALSE; 
        } 
    } 
 
    // Set text for sub progress 
    m_status_bar_sub->SetText(sub_text.CStr()); 
 
    // Redraw widget 
    if (m_status_bar_sub->IsVisible()) 
        m_status_bar_sub->InvalidateAll(); 
} 
 
 
/*********************************************************************************** 
** 
**  NeedNewMessagesNotification 
** 
***********************************************************************************/ 
void MailPanel::NeedNewMessagesNotification(Account* account, unsigned count) 
{ 
    m_needs_attention = TRUE; 
    PanelChanged(); 
} 
 
 
/*********************************************************************************** 
** 
**  OnMailViewActivated 
** 
***********************************************************************************/ 
void MailPanel::OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active) 
{ 
    if (active && m_needs_attention) 
    { 
        m_needs_attention = FALSE; 
        PanelChanged(); 
    } 
} 
 
 
/*********************************************************************************** 
** 
**  OnTimeOut 
** 
***********************************************************************************/ 
void MailPanel::OnTimeOut(OpTimer* timer) 
{ 
    if(timer == &m_status_timer) 
    { 
        m_status_bar_sub->SetVisibility(TRUE); 
        m_status_timer_started = FALSE; 
        OnRelayout(); 
    } 
    else 
    { 
        HotlistPanel::OnTimeOut(timer); 
    } 
} 
 
/***************************************************************************** 
** 
**  SetSelectedIndex 
** 
*****************************************************************************/ 
 
void MailPanel::SetSelectedIndex(index_gid_t index_id) 
{ 
	// unselect other indexes in all access views and select it if not selected already
    for (UINT32 i = 0; i < m_access_views.GetCount(); i++) 
    { 
		if (m_access_views.Get(i)->GetItemByID(index_id) != -1)
		{
		    if (!m_access_views.Get(i)->GetSelectedItem() || 
				(m_access_views.Get(i)->GetSelectedItem() && (index_gid_t)m_access_views.Get(i)->GetSelectedItem()->GetID() != index_id))
			{
				m_access_views.Get(i)->SetSelectedItemByID(index_id, TRUE, FALSE);
				
				// Expand the accordion item to make it visible
				UINT32 category_id = static_cast<AccessModel*>(m_access_views.Get(i)->GetTreeModel())->GetCategoryID();
				if (!m_accordion->GetItemById(category_id)->IsExpanded())
					m_accordion->ExpandItem(m_accordion->GetItemById(category_id), TRUE);
			}
		}
		else
		{
			if (m_access_views.Get(i)->GetSelectedItem())
				m_access_views.Get(i)->SetSelectedItem(-1, FALSE, FALSE);
		}
    } 
} 
 
/***************************************************************************** 
** 
**  ExportMailIndex 
** 
*****************************************************************************/ 
 
OP_STATUS MailPanel::ExportMailIndex(UINT32 index_id) 
{ 
    if (!m_chooser) 
        RETURN_IF_ERROR(DesktopFileChooser::Create(&m_chooser)); 
 
    m_request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE; 
    RETURN_IF_ERROR(g_languageManager->GetString(Str::D_EXPORT_MESSAGES, m_request.caption)); 
    Index* index = g_m2_engine->GetIndexById(index_id); 
    if (index) 
    { 
        RETURN_IF_ERROR(index->GetName(m_request.initial_path)); 
    } 
    else 
    { 
        RETURN_IF_ERROR(m_request.initial_path.Set(UNI_L("export"))); 
    } 
    m_request.fixed_filter = TRUE; 
    RETURN_IF_ERROR(m_request.initial_path.Append(UNI_L(".mbs"))); 
    OpString filter; 
    RETURN_IF_ERROR(g_languageManager->GetString(Str::S_EXPORT_MBOX_FILTER, filter)); 
    RETURN_IF_ERROR(StringFilterToExtensionFilter(filter.CStr(), &m_request.extension_filters)); 
    DesktopWindow *parent = g_application->GetActiveDesktopWindow(FALSE); 
    m_export_index = index_id; 
 
    return m_chooser->Execute(parent ? parent->GetOpWindow() : 0, this, m_request); 
} 
 
/*********************************************************************************** 
** 
**  MailPanel::OnFileChoosingDone 
** 
***********************************************************************************/ 
void MailPanel::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result) 
{ 
    if (result.files.GetCount()) 
    { 
        g_m2_engine->ExportToMbox(m_export_index, *result.files.Get(0)); 
    } 
} 
 
/*********************************************************************************** 
** 
**  MailPanel::AddCategory 
** 
***********************************************************************************/ 
OP_STATUS MailPanel::AddCategory(UINT32 index_id) 
{ 
	if (m_accordion->GetItemById(index_id) != NULL)
		return OpStatus::OK;

    Index* index = g_m2_engine->GetIndexById(index_id); 

	if (!index)
		return OpStatus::ERR_NULL_POINTER;

	if (index->GetId() >= IndexTypes::FIRST_ACCOUNT && index->GetId() < IndexTypes::LAST_ACCOUNT && !g_m2_engine->GetAccountManager()->IsAccountActive((UINT16)(index->GetId()-IndexTypes::FIRST_ACCOUNT)))
		return OpStatus::OK;
	OpString title; 

    RETURN_IF_ERROR(title.Set(index->GetName())); 

	// create the treeview and add it to the accordion with title and image 
	AccessTreeView* tree_view;
    RETURN_IF_ERROR(AccessTreeView::Construct(&tree_view)); 
	if (OpStatus::IsError(m_access_views.Add(tree_view)))
	{
		OP_DELETE(tree_view);
		return OpStatus::ERR;
	}
    RETURN_IF_ERROR(tree_view->Init(this, index_id));
	tree_view->OnFullModeChanged(IsFullMode());

	const char* image = NULL;
	if (index_id >= IndexTypes::FIRST_ACCOUNT && index_id <= IndexTypes::LAST_ACCOUNT) 
	{
		image = GetNewIconForAccount(index_id);
	}
 
	RETURN_IF_ERROR(m_accordion->AddAccordionItem(index_id, title, image, tree_view, g_m2_engine->GetIndexer()->GetCategoryPosition(index_id)));
	
	if (g_m2_engine->GetIndexer()->GetCategoryOpen(index_id))
		m_accordion->ExpandItem(m_accordion->GetItemById(index_id), TRUE, FALSE);
	UINT32 unread = g_m2_engine->GetIndexer()->GetCategoryUnread(index_id);
	if (unread > 0)
		m_accordion->GetItemById(index_id)->SetAttention(TRUE, unread);

	// Adding these actions to the buttons:
	// Button = Open Item | Close Item + Show Popup Menu "Mail Panel Category Menu"

	OpInputAction* action_open = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_ACCORDION_ITEM));
	action_open->SetActionOperator(OpInputAction::OPERATOR_OR);

	OpInputAction* action_close = OP_NEW(OpInputAction, (OpInputAction::ACTION_CLOSE_ACCORDION_ITEM));
	action_close->SetActionOperator(OpInputAction::OPERATOR_PLUS);
	action_open->SetNextInputAction(action_close);

	OpInputAction* next_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU));
	if (!next_action)
		return OpStatus::ERR_NO_MEMORY;
	next_action->SetActionData(index_id);
	next_action->SetActionDataString(UNI_L("Internal Mail Category Menu"));

	action_close->SetNextInputAction(next_action);
	m_accordion->GetItemById(index_id)->GetButton()->SetAction(action_open);
	m_accordion->GetItemById(index_id)->GetButton()->SetDropdownImage("Accordion button dropdown");
	return OpStatus::OK;
} 
 
/*********************************************************************************** 
** 
**  MailPanel::PopulateCategories 
** 
***********************************************************************************/ 
OP_STATUS MailPanel::PopulateCategories(BOOL reset) 
{ 
	if (g_application->HasFeeds() && !g_application->HasMail())
	{
		SetToolbarName("Feed Panel Toolbar", "Mail Full Toolbar"); 
		RETURN_IF_ERROR(m_accordion->SetFallbackPopupMenu("Feed Index Item Popup Menu"));
	}
	else
	{
		SetToolbarName("Mail Panel Toolbar", "Mail Full Toolbar"); 
		RETURN_IF_ERROR(m_accordion->SetFallbackPopupMenu("Index Item Popup Menu"));
	}

	if (reset)
		RETURN_IF_ERROR(g_m2_engine->GetIndexer()->ResetToDefaultIndexes());

	m_accordion->DeleteAll();
	m_access_views.Clear();
	OpINT32Vector categories;
	RETURN_IF_ERROR(g_m2_engine->GetIndexer()->GetCategories(categories));

	for (UINT32 i=0; i < categories.GetCount(); i++)
	{
		OpINT32Vector children;
		// never add categories without children
		if (g_m2_engine->GetIndexer()->GetCategoryVisible(categories.Get(i)))
			OpStatus::Ignore(AddCategory(categories.Get(i)));
	}

    return OpStatus::OK; 
} 

/*********************************************************************************** 
** 
**  MailPanel::OnItemExpanded 
** 
***********************************************************************************/ 
void MailPanel::OnItemExpanded( UINT32 id, BOOL expanded)
{
	g_m2_engine->GetIndexer()->SetCategoryOpen(id, expanded);
}

/*********************************************************************************** 
** 
**  MailPanel::OnItemMoved 
** 
***********************************************************************************/ 
void MailPanel::OnItemMoved(UINT32 id, UINT32 new_position)
{
	OpStatus::Ignore(g_m2_engine->GetIndexer()->MoveCategory(id, new_position));
}

/*********************************************************************************** 
** 
**  MailPanel::CategoryRemoved 
** 
***********************************************************************************/ 
OP_STATUS MailPanel::CategoryRemoved(UINT32 index_id)
{
	for (UINT32 idx = 0; idx < m_access_views.GetCount(); idx++)
	{
		if (static_cast<AccessModel*>(m_access_views.Get(idx)->GetTreeModel())->GetCategoryID() == index_id)
		{
			m_access_views.Remove(idx);
			break;
		}
	}
	m_accordion->RemoveAccordionItem(index_id); return OpStatus::OK; 
}
/*********************************************************************************** 
** 
**  MailPanel::CategoryUnreadChanged 
** 
***********************************************************************************/ 
OP_STATUS MailPanel::CategoryUnreadChanged(UINT32 index_id, UINT32 unread_messages)
{
	OpAccordion::OpAccordionItem* accordion_item = m_accordion->GetItemById(index_id);
	if (accordion_item)
	{
		accordion_item->SetAttention(unread_messages > 0, unread_messages);
	}
	if (index_id == IndexTypes::CATEGORY_MY_MAIL && GetHotlist())
	{
		GetHotlist()->OnChange(this, FALSE);
	}

	return OpStatus::OK;
}

/*********************************************************************************** 
** 
**  MailPanel::CategoryNameChanged 
** 
***********************************************************************************/ 
OP_STATUS MailPanel::CategoryNameChanged(UINT32 index_id)
{
	Index* index = g_m2_engine->GetIndexer()->GetIndexById(index_id);
	if (!index)
		return OpStatus::ERR_NULL_POINTER;
	
	OpAccordion::OpAccordionItem* accordion_item = m_accordion->GetItemById(index_id);
	if (!accordion_item)
		return OpStatus::ERR;
	else
	{
		OpString name;
		RETURN_IF_ERROR(name.Set(index->GetName()));
		return accordion_item->SetButtonText(name);
	}
}

/*********************************************************************************** 
** 
**  MailPanel::CategoryVisibleChanged 
** 
***********************************************************************************/ 
OP_STATUS MailPanel::CategoryVisibleChanged(UINT32 index_id, BOOL visible)
{
	if (visible)
	{
		return AddCategory(index_id);
	}
	else
	{
		return CategoryRemoved(index_id);
	}
}

/*********************************************************************************** 
** 
**  MailPanel::GetUnreadBadgeWidth 
** 
***********************************************************************************/ 
OP_STATUS MailPanel::GetUnreadBadgeWidth(INT32& w)
{
	if (m_unread_badge_width == 0)
	{
		OpButton* label;
		INT32 h;
		RETURN_IF_ERROR(OpButton::Construct(&label, OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT));
		label->GetBorderSkin()->SetImage("Mail Notification Label Skin");
		VisualDeviceHandler vis_dev_handler(label);
		RETURN_IF_ERROR(label->SetText(UNI_L("0000")));
		label->GetRequiredSize(m_unread_badge_width, h);	
		label->Delete();
	}
	w = m_unread_badge_width;
	return OpStatus::OK;
}

const char* MailPanel::GetNewIconForAccount(UINT32 index_id)
{
	Account *account = NULL; 
	if (OpStatus::IsSuccess(MessageEngine::GetInstance()->GetAccountManager()->GetAccountById(index_id - IndexTypes::FIRST_ACCOUNT, account)) && account)
	{
		return account->GetIcon(TRUE);
	}
	return NULL;
}

#endif // M2_SUPPORT
