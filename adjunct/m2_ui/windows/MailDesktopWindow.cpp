/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include <limits.h>
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/desktop_util/file_utils/filenameutils.h"
#include "adjunct/desktop_util/handlers/DownloadItem.h"
#include "adjunct/desktop_util/mail/mailformatting.h"
#include "adjunct/desktop_util/prefs/PrefsUtils.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/desktop_util/sessions/opsession.h"

#include "adjunct/m2/src/composer/attachmentholder.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2_ui/controllers/DefaultMailViewPropertiesController.h"
#include "adjunct/m2_ui/controllers/IndexViewPropertiesController.h"
#include "adjunct/m2_ui/controllers/LabelPropertiesController.h"
#include "adjunct/m2_ui/models/indexmodel.h"
#include "adjunct/m2_ui/dialogs/SearchMailDialog.h"
#include "adjunct/m2_ui/panels/AccordionMailPanel.h"
#include "adjunct/m2_ui/windows/ChatDesktopWindow.h"
#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/panels/ContactsPanel.h"

#include "adjunct/quick_toolkit/widgets/OpImageWidget.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/widgets/OpStatusField.h"
#include "adjunct/quick_toolkit/widgets/OpSplitter.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"

#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"

class CancelMessageDialog : public SimpleDialog
{
	private:
		UINT32 m_message_id;

	public:
		OP_STATUS Init(UINT32 message_id, DesktopWindow* parent_window)
		{
			m_message_id = message_id;

			OpString title;
			OpString message;

			g_languageManager->GetString(Str::S_CANCEL_MESSAGE, title);
			g_languageManager->GetString(Str::S_CANCLE_MESSAGE_WARNING, message);

			return SimpleDialog::Init(WINDOW_NAME_CANCEL_MESSAGE, title, message, parent_window, TYPE_YES_NO);
		}

		UINT32 OnOk()
		{
			g_m2_engine->CancelMessage(m_message_id);
			return 0;
		}
};

class ActionReloadOnlineModeCallback : public Application::OnlineModeCallback
{
public:
	ActionReloadOnlineModeCallback(index_gid_t folder_id) : m_folder_id(folder_id) {}
	virtual void OnOnlineMode() { g_m2_engine->RefreshFolder(m_folder_id); }
private:
	index_gid_t m_folder_id;
};

class FetchCompleteMessageOnlineModeCallback : public Application::OnlineModeCallback
{
public:
	OP_STATUS Init(OpINT32Vector& items) { return m_items.DuplicateOf(items); }

	virtual void OnOnlineMode()
	{
		if (m_items.GetCount() > 0)
		{
			for (int i = m_items.GetCount() - 1; i >= 0; i--)
			{
				UINT32 id = m_items.Get(i);
				g_m2_engine->FetchCompleteMessage(id);
			}
		}
	}

private:
	OpINT32Vector m_items;
};

/***********************************************************************************
**
**	MailDesktopWindow
**
***********************************************************************************/
OP_STATUS MailDesktopWindow::Construct(MailDesktopWindow** obj, OpWorkspace* parent_workspace)
{
	*obj = OP_NEW(MailDesktopWindow, (parent_workspace));
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		(*obj)->Close(TRUE);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}


MailDesktopWindow::MailDesktopWindow(OpWorkspace* parent_workspace)
: m_saveas_message_id(0),
  m_old_session_message_id(0),
  m_message_read_timer(0),
  m_mail_selected_timer(0),
  m_index_id(0),
  m_splitter(0),
  m_index_view(0),
  m_running_searches(0),
  m_mail_view(NULL),
  m_loading_image(NULL),
  m_mailview_message(NULL),
  m_index_view_group(NULL),
  m_mail_and_headers_group(NULL),
  m_search_in_toolbar(NULL),
  m_left_top_toolbar(NULL),
  m_search_in_index(0),
  m_current_undo_redo_position(0),
  m_current_history_position(0),
  m_show_raw_message(FALSE),
  m_old_selected_panel(PANEL_HIDDEN),
  m_allow_external_embeds(FALSE)
{

// Beware!!
// Changing the order in which widget children are added will change TAB selection order!!

	m_mailview_message = OP_NEW(Message,());

	OP_STATUS status = DesktopWindow::Init(OpWindow::STYLE_DESKTOP, parent_workspace);
	CHECK_STATUS(status);


	OpWidget* root_widget = GetWidgetContainer()->GetRoot();
	root_widget->GetBorderSkin()->SetImage("Mail Window Background Skin");
	status = OpSplitter::Construct(&m_splitter);
	if (OpStatus::IsError(status))
	{
		m_splitter = NULL;			// accessed in destructor, might have dangling pointer from ::Construct
		init_status = status;
		return;
	}
	m_splitter->SetDividerSkin("Mail Window Divider Skin");
	root_widget->AddChild(m_splitter);

	// Create a group to handle switching between m_index_view_group and
	// m_loading_image.
	OpGroup* index_and_loading_image_group = NULL;
	status = OpGroup::Construct(&index_and_loading_image_group);
	CHECK_STATUS(status);

	status = OpGroup::Construct(&m_index_view_group);
	CHECK_STATUS(status);
	index_and_loading_image_group->AddChild(m_index_view_group);
	m_splitter->AddChild(index_and_loading_image_group);

	status = OpToolbar::Construct(&m_left_top_toolbar);
	CHECK_STATUS(status);
	m_left_top_toolbar->SetName("Mail Top Toolbar");
	m_left_top_toolbar->GetBorderSkin()->SetImage("Mailbar skin");
	m_index_view_group->AddChild(m_left_top_toolbar);

	status = OpInfobar::Construct(&m_search_in_toolbar);
	CHECK_STATUS(status);
	m_index_view_group->AddChild(m_search_in_toolbar);
	status = m_search_in_toolbar->Init("Mail Search Toolbar");
	CHECK_STATUS(status);
	m_search_in_toolbar->GetBorderSkin()->SetImage("Mail Search Toolbar");
	m_search_in_toolbar->SetAlignment(OpBar::ALIGNMENT_OFF);

	if (g_application->HasFeeds() && !g_application->HasMail())
	{
		OpButton* tb_all_mail = static_cast<OpButton*> (m_search_in_toolbar->GetWidgetByNameInSubtree("tb_all_mail"));
		if (tb_all_mail)
		{
			OpString all_feeds;
			g_languageManager->GetString(Str::S_SEARCH_ALL_FEEDS, all_feeds);
			tb_all_mail->SetText(all_feeds);
		}
	}

	status = OpTreeView::Construct(&m_index_view);
	if (OpStatus::IsError(status))
	{
		m_index_view = NULL;		// accessed in destructor, might have dangling pointer from ::Construct
		init_status = status;
		return;
	}

	m_index_view_group->AddChild(m_index_view);

	m_mail_and_headers_group = OP_NEW(MessageHeaderGroup, (this));
	if (!m_mail_and_headers_group)
		return;

	m_splitter->AddChild(m_mail_and_headers_group);

	OpFindTextBar* search_bar = GetFindTextBar();
	m_mail_and_headers_group->AddFindTextToolbar(search_bar);

	INT32 view_mode = g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode);

	if (view_mode == MailLayout::LIST_ON_LEFT_MESSAGE_ON_RIGHT)
	{
		m_left_top_toolbar->SetName("Mail Left Toolbar");
		m_splitter->SetHorizontal(true);
	}
	else if (view_mode == MailLayout::LIST_ONLY)
	{
		m_left_top_toolbar->SetName("Mail List Only Toolbar");
		m_mail_and_headers_group->SetVisibility(FALSE);
	}

	m_index_view->SetDragEnabled(TRUE);
	m_index_view->SetReselectWhenSelectedIsRemoved(TRUE);
	m_index_view->SetMultiselectable(TRUE);
	m_index_view->SetShowThreadImage(TRUE);
	m_index_view->SetRootOpensAll(TRUE);
	m_index_view->SetAllowSelectByLetter(FALSE);
	m_index_view->SetFocus(FOCUS_REASON_OTHER);
	m_index_view->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_index_view->SetExpandOnSingleClick(FALSE);
	m_index_view->SetAsyncMatch(TRUE);
	m_index_view->AddTreeViewListener(this);
	m_index_view->GetBorderSkin()->SetImage("Mail Window Treeview Skin");
	m_index_view->SetCustomThreadIndentation(10);

	// Set message handler
	g_main_message_handler->SetCallBack(this, MSG_M2_SEARCH_RESULTS, (MH_PARAM_1)this);

	m_splitter->SetDivision(g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewListSplitter));

	if (!(m_message_read_timer = OP_NEW(OpTimer, ())))
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	if (!(m_mail_selected_timer = OP_NEW(OpTimer, ())))
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}

	m_message_read_timer->SetTimerListener(this);
	m_mail_selected_timer->SetTimerListener(this);

	status = g_hotlist_manager->GetContactsModel()->AddListener(this);
	CHECK_STATUS(status);
	status = g_m2_engine->AddMessageListener(this);
	CHECK_STATUS(status);
	status = g_m2_engine->AddEngineListener(this);
	CHECK_STATUS(status);

	status = g_pcm2->RegisterListener(this);
	CHECK_STATUS(status);

	g_languageManager->GetString(Str::S_SAVE_AS_CAPTION, m_request.caption);
	m_request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE;
	OpString filter;
	g_languageManager->GetString(Str::S_IMPORT_MBOX_FILTER, filter);
	if (filter.HasContent())
		StringFilterToExtensionFilter(filter.CStr(), &m_request.extension_filters);
	status = g_pcdoc->RegisterListener(this);
	CHECK_STATUS(status);
}

// ----------------------------------------------------

MailDesktopWindow::~MailDesktopWindow()
{
	g_pcm2->UnregisterListener(this);
	g_main_message_handler->UnsetCallBacks(this);

#ifndef PREFS_NO_WRITE
	OP_STATUS err;

	if (m_splitter)
		TRAP(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::MailViewListSplitter, (int)m_splitter->GetDivision()));

#endif
	g_m2_engine->RemoveEngineListener(this);
	g_m2_engine->RemoveMessageListener(this);
	RemoveMimeListeners();

	g_hotlist_manager->GetContactsModel()->RemoveListener(this);
	OP_DELETE(m_message_read_timer);
	OP_DELETE(m_mail_selected_timer);
	OP_DELETE(m_mailview_message);

	if (m_index_view)
	{
		m_index_view->RemoveTreeViewListener(this);

		message_gid_t selected_message = 0;
		OpTreeModelItem* item = m_index_view->GetSelectedItem();
		if (item)
			selected_message = item->GetID();

		OpTreeModel* old_model = m_index_view->GetTreeModel();
		m_index_view->SetTreeModel(NULL); // avoid crash in ~OpTreeView()
		OpStatus::Ignore(g_m2_engine->ReleaseIndexModel(old_model));

		Index* index = g_m2_engine->GetIndexById(m_index_id == IndexTypes::UNREAD_UI ? IndexTypes::UNREAD : m_index_id);
		if (index)
		{
			index->SetModelSelectedMessage(selected_message);
			g_m2_engine->GetIndexer()->SaveRequest();
		}
	}
	g_pcdoc->UnregisterListener(this);
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

void MailDesktopWindow::Init(index_gid_t index_id, const uni_char* address, const uni_char* window_title, BOOL message_only_view)
{
	SetMailViewToIndex(index_id, address, window_title, TRUE, TRUE, message_only_view, message_only_view);
}

/***********************************************************************************
**
**	IsCorrectMailView
**
***********************************************************************************/

BOOL MailDesktopWindow::IsCorrectMailView(index_gid_t index_id, const uni_char* address)
{
	if (address)
	{
		return m_contact_address.HasContent() ? uni_stricmp(m_contact_address.CStr(), address) == 0 : FALSE;
	}
	else
	{
		return index_id == m_index_id;
	}
}


/***********************************************************************************
**
**	SetMessageOnlyView
**
***********************************************************************************/
void MailDesktopWindow::SetMessageOnlyView()
{
	GetIndexAndLoadingImageGroup()->SetVisibility(FALSE);
	m_index_view->SetSize(0,0);
	m_mail_and_headers_group->SetVisibility(TRUE);
	if (m_mail_view)
		m_mail_view->SetFocus(FOCUS_REASON_OTHER);
}

void MailDesktopWindow::SetListOnlyView()
{
	GetIndexAndLoadingImageGroup()->SetVisibility(TRUE);
	m_mail_and_headers_group->SetVisibility(FALSE);
}

void MailDesktopWindow::SetListAndMessageView(bool horizontal_splitter)
{
	GetIndexAndLoadingImageGroup()->SetVisibility(TRUE);
	m_mail_and_headers_group->SetVisibility(TRUE);
	m_splitter->SetHorizontal(horizontal_splitter);
}

/***********************************************************************************
**
**	SetMailViewToIndex
**
***********************************************************************************/

void MailDesktopWindow::SetMailViewToIndex(index_gid_t index_id, const uni_char* address, const uni_char* window_title, BOOL changed_by_mouse, BOOL force, BOOL focus, BOOL message_only_view)
{
	if (!force && ((index_id && index_id == m_index_id) || (address && m_contact_address.Compare(address) == 0)))
		return;

	if (focus)
		m_index_view->SetFocus(FOCUS_REASON_OTHER);

	m_contact_address.Set(address);

	if (m_contact_address.HasContent())
	{
		UpdateTreeview(g_m2_engine->GetIndexIDByAddress(m_contact_address));
	}
	else
	{
		UpdateTreeview(index_id);
	}


	Index* m2index = g_m2_engine->GetIndexById(m_index_id);
	if (m2index)
	{
		const char* image = NULL;
		Image bitmap_image;

		m2index->GetImages(image, bitmap_image);

		SetIcon(image);
		SetIcon(bitmap_image);

		// set picture

		OpString contact_address;
		OpString picture_url;

		if (m2index->GetContactAddress(contact_address) == OpStatus::OK)
		{
			HotlistManager::ItemData item_data;
			if( g_hotlist_manager->GetItemValue_ByEmail(contact_address,item_data) )
			{
				picture_url.Set( item_data.pictureurl );
			}
		}

		UpdateStatusText();
		SetCorrectToolbar(m_index_id);
	}
	
	if (message_only_view)
	{
		SetMessageOnlyView();
	}
	else
	{
		// for message only view the title is updated in ShowSelectedMail
		UpdateTitle();
	}

	ShowSelectedMail(TRUE);
	Hotlist* hotlist = g_application->GetActiveBrowserDesktopWindow() ? g_application->GetActiveBrowserDesktopWindow()->GetHotlist(): NULL;

	if (hotlist)
	{
		MailPanel* mail_panel = static_cast<MailPanel*>(hotlist->GetPanelByType(PANEL_TYPE_MAIL));
		if (mail_panel)
		{
			// make sure the mail panel is focusing the correct index
			mail_panel->SetSelectedIndex(m_index_id);
		}
	}

	g_input_manager->UpdateAllInputStates();
}

void MailDesktopWindow::UpdateTreeview(index_gid_t new_index_id)
{
	OpTreeModel* index_model, *old_model;

	message_gid_t old_selected_message = 0;
	index_gid_t old_index_id = m_index_id;
	m_index_id = new_index_id;
	if (new_index_id < IndexTypes::FIRST_SEARCH || new_index_id > IndexTypes::LAST_SEARCH)
		m_search_in_index = 0;

	// keep a copy of old model for later release:
	old_model = ( m_index_view ? m_index_view->GetTreeModel() : NULL);
	if (old_model)
	{
		OpTreeModelItem* item = m_index_view->GetSelectedItem();
		if (item)
			old_selected_message = item->GetID();

		g_m2_engine->ReleaseIndexModel(old_model);
		if (g_m2_engine->GetIndexById(old_index_id))
		{
			g_m2_engine->GetIndexById(old_index_id == IndexTypes::UNREAD_UI ? IndexTypes::UNREAD : old_index_id)->SetModelSelectedMessage(old_selected_message);
			g_m2_engine->GetIndexer()->SaveRequest();
		}
	}

	INT32 start_pos = -1;
	OP_STATUS status = g_m2_engine->GetIndexModel(&index_model, m_index_id, start_pos);

	if (status == OpStatus::OK)
	{
		// update history
		if (m_history.GetCount() == 0 ||
			m_index_id != static_cast<index_gid_t>(m_history.Get(m_current_history_position)))
		{
			if (m_current_history_position+1 < m_history.GetCount())
			{
				// remove future items
				UINT32 history_pos;
				for (history_pos = m_history.GetCount()-1; history_pos > m_current_history_position; history_pos--)
				{
					m_history.Remove(history_pos);
				}
			}

			m_current_history_position = m_history.GetCount();
			m_history.Add(m_index_id);
		}

		IndexTypes::ModelType type;
		IndexTypes::ModelAge age;
		IndexTypes::ModelSort sort_column;
		BOOL ascending;
		INT32 flags;
		message_gid_t selected_message;
		IndexTypes::GroupingMethods grouping_method;

		g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort_column, ascending, grouping_method, selected_message);

		bool two_lines = (g_pcm2->GetIntegerPref(PrefsCollectionM2::MailTwoLinedItems) == 1 && g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode) == MailLayout::LIST_ON_LEFT_MESSAGE_ON_RIGHT) || g_pcm2->GetIntegerPref(PrefsCollectionM2::MailTwoLinedItems) == 2;
		static_cast<IndexModel*>(index_model)->SetTwoLinedItems(two_lines);

		if (two_lines)
			m_index_view->SetShowColumnHeaders(FALSE);
		
		m_index_view->SetTreeModel(index_model, sort_column, ascending, "Mail View", old_index_id != m_index_id, TRUE, static_cast<IndexModel*>(index_model));
		
		m_index_view->SetCloseAllHeaders(type != IndexTypes::MODEL_TYPE_THREADED);

		if (two_lines)
		{
			m_index_view->SetColumnVisibility(IndexModel::FromColumn, FALSE);
			m_index_view->SetColumnVisibility(IndexModel::SubjectColumn, FALSE);
			m_index_view->SetColumnVisibility(IndexModel::TwoLineFromSubjectColumn, TRUE);
			m_index_view->SetColumnVisibility(IndexModel::LabelIconColumn, TRUE);
			m_index_view->SetColumnVisibility(IndexModel::LabelColumn, FALSE);
			m_index_view->SetColumnWeight(IndexModel::TwoLineFromSubjectColumn, 900);
			m_index_view->SetThreadColumn(IndexModel::TwoLineFromSubjectColumn);
			m_index_view->SetColumnVisibility(IndexModel::SizeColumn, FALSE);
			m_index_view->SetExtraLineHeight(3);
			m_index_view->SetColumnFixedCharacterWidth(IndexModel::SentDateColumn, 20);
			m_index_view->SetLinesKeepVisibleOnScroll(4);
		}
		else
		{
			m_index_view->SetColumnVisibility(IndexModel::FromColumn, TRUE);
			m_index_view->SetColumnVisibility(IndexModel::SubjectColumn, TRUE);
			m_index_view->SetColumnVisibility(IndexModel::TwoLineFromSubjectColumn, FALSE);
			m_index_view->SetColumnVisibility(IndexModel::LabelColumn, TRUE);
			m_index_view->SetColumnVisibility(IndexModel::LabelIconColumn, FALSE);
			m_index_view->SetColumnVisibility(IndexModel::SizeColumn, TRUE);
			m_index_view->SetColumnWeight(IndexModel::FromColumn, 400);
			m_index_view->SetColumnWeight(IndexModel::SubjectColumn, 900);
			m_index_view->SetThreadColumn(IndexModel::SubjectColumn);
			m_index_view->SetShowColumnHeaders(TRUE);
			m_index_view->SetExtraLineHeight(3);
			m_index_view->SetColumnHasDropdown(IndexModel::LabelColumn, TRUE);
			m_index_view->SetColumnWeight(IndexModel::SentDateColumn, 150);
			m_index_view->SetLinesKeepVisibleOnScroll(2);
		}

		// make the size column hidden by default
		m_index_view->SetColumnFixedWidth(IndexModel::StatusColumn, 20);
		m_index_view->SetColumnFixedWidth(IndexModel::LabelIconColumn, 20);
		m_index_view->SetColumnWeight(IndexModel::LabelColumn, 100);
		m_index_view->SetColumnWeight(IndexModel::SizeColumn, 70);

		if (m_index_view->GetLineCount() > 0)
		{
			if (m_index_id >= IndexTypes::FIRST_THREAD && m_index_id < IndexTypes::LAST_THREAD)
				m_index_view->OpenItem(0,TRUE);

			if (m_old_session_message_id != 0)
			{
				m_index_view->SetSelectedItemByID(m_old_session_message_id);
				m_old_session_message_id = 0;
			}
			else if (selected_message != 0)
			{
				m_index_view->SetSelectedItemByID(selected_message);
			}
			else
			{
				if (m_index_view->GetSortAscending())
				{
					start_pos = m_index_view->GetItemByLine(m_index_view->GetLineCount() - 1);
				}
				else
				{
					start_pos=0;
				}
				m_index_view->SetSelectedItem(start_pos, FALSE);

				OpTreeModelItem::ItemData item_data(UNREAD_QUERY);
				OpTreeModelItem* selected_item = m_index_view->GetSelectedItem();
				
				if (selected_item)
					selected_item->GetItemData(&item_data);

				if ((item_data.flags & OpTreeModelItem::FLAG_UNREAD) == 0 && g_m2_engine->GetIndexById(m_index_id)->UnreadCount())
					g_input_manager->InvokeAction(OpInputAction::ACTION_SELECT_NEXT_UNREAD);
			}
		}
	}
	else
	{
		m_index_view->SetTreeModel(NULL);
		if (!m_loading_image && status == OpStatus::ERR_YIELD)
		{
			status = OpButton::Construct(&m_loading_image, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
			CHECK_STATUS(status);
		
			m_loading_image->GetForegroundSkin()->SetImage("Mail Loading Database");
			m_loading_image->GetBorderSkin()->SetImage("Mail Window Treeview Skin");
			m_loading_image->SetVisibility(TRUE);
			m_loading_image->SetDead(TRUE);

			GetIndexAndLoadingImageGroup()->AddChild(m_loading_image);
			GetIndexAndLoadingImageGroup()->Relayout();
		}
	}
}

/***********************************************************************************
**
**	SetCorrectToolbar
**
***********************************************************************************/

void MailDesktopWindow::SetCorrectToolbar(index_gid_t new_index_id)
{
	OpString8 toolbar_name;
	if (g_m2_engine->IsIndexNewsfeed(new_index_id))
		RETURN_VOID_IF_ERROR(toolbar_name.Set("Feed "));
	else
		RETURN_VOID_IF_ERROR(toolbar_name.Set("Mail "));

	if (!m_mail_and_headers_group->IsVisible())
		RETURN_VOID_IF_ERROR(toolbar_name.Append("List Only Toolbar"));
	else if (m_splitter->IsHorizontal())
		RETURN_VOID_IF_ERROR(toolbar_name.Append("Left Toolbar"));
	else
		RETURN_VOID_IF_ERROR(toolbar_name.Append("Top Toolbar"));
	
	if (m_left_top_toolbar->GetName() != toolbar_name)
		m_left_top_toolbar->SetName(toolbar_name);
}

/***********************************************************************************
**
**	GoToMessage
**
***********************************************************************************/
void MailDesktopWindow::GoToMessage(message_gid_t message_id)
{
	m_index_view->SetSelectedItemByID(message_id);
	ShowSelectedMail(TRUE);
}

/***********************************************************************************
**
**	GoToThread
**
***********************************************************************************/
void MailDesktopWindow::GotoThread(message_gid_t message_id)
{
	Index *index = NULL;
	OpINTSet thread_ids;

	if (OpStatus::IsError(g_m2_engine->GetStore()->GetThreadIds(message_id,thread_ids)))
		thread_ids.Insert(message_id);
	g_m2_engine->GetIndexer()->CreateThreadIndex(thread_ids, index, FALSE);

	if (index)
	{
		SetMailViewToIndex(index->GetId(),NULL,NULL,FALSE);
		m_index_view->SetSelectedItemByID(message_id);
		m_index_view->OpenAllItems(TRUE);
	}
}

/***********************************************************************************
**
**	GetSelectedMessage
**
***********************************************************************************/

OP_STATUS MailDesktopWindow::GetSelectedMessage(BOOL full_message, BOOL force, BOOL timeout_request)
{
	OpTreeModelItem* selected_item = m_index_view->GetSelectedItem();

	// See if there actually is a message selected
	if (!selected_item)
		return OpStatus::ERR_NULL_POINTER;

	// Check if we already have this message cached
	if (!force && GetCurrentMessageID() == (message_gid_t)selected_item->GetID())
		return OpStatus::OK;

	// Get the message
	return MessageEngine::GetInstance()->GetMessage(*m_mailview_message, selected_item->GetID(), full_message, timeout_request);
}

/***********************************************************************************
**
**	SaveSortOrder
**
***********************************************************************************/

void MailDesktopWindow::SaveSortOrder(INT32 sort_by_column, BOOL sort_ascending, message_gid_t selected_item, index_gid_t index_id)
{
	IndexTypes::ModelType type;
	IndexTypes::ModelAge age;
	IndexTypes::ModelSort sort;
	BOOL ascending;
	INT32 flags;
	message_gid_t selected_message;
	IndexTypes::GroupingMethods grouping_method;

	g_m2_engine->GetIndexModelType(index_id, type, age, flags, sort, ascending, grouping_method, selected_message);

	sort = (IndexTypes::ModelSort)(sort_by_column);
	ascending = sort_ascending != FALSE;

	g_m2_engine->SetIndexModelType(index_id, type, age, flags, sort, ascending, grouping_method, selected_item);
}


/***********************************************************************************
**
**	OnSortingChanged
**
***********************************************************************************/

void MailDesktopWindow::OnSortingChanged(OpWidget *widget)
{
	if (widget != m_index_view)
	{
		return;
	}

	BOOL ascending = m_index_view->GetSortAscending();
	INT32 column = m_index_view->GetSortByColumn();
	message_gid_t selected_message = 0;

	OpTreeModelItem* item = m_index_view->GetSelectedItem();
	if (item)
		selected_message = item->GetID();
	SaveSortOrder(column, ascending, selected_message, m_index_id);
}

void MailDesktopWindow::UpdateSorting()
{
	IndexTypes::ModelType type;
	IndexTypes::ModelAge age;
	IndexTypes::ModelSort sort_column;
	BOOL ascending;
	INT32 flags;
	message_gid_t selected_message;
	IndexTypes::GroupingMethods grouping_method;

	g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort_column, ascending, grouping_method, selected_message);
	m_index_view->Sort(sort_column, ascending);
}

/***********************************************************************************
**
**	OnPageGetExternalEmbedPermission
**
***********************************************************************************/
void MailDesktopWindow::GetExternalEmbedPermission(OpWindowCommander* commander, BOOL& has_permission)
{
	has_permission = m_allow_external_embeds;
}

/***********************************************************************************
**
**	OnPageExternalEmbedBlocked
**
***********************************************************************************/
void MailDesktopWindow::OnExternalEmbendBlocked(OpWindowCommander* commander)
{
	OP_ASSERT(m_mailview_message);
	OpString from;
	m_mailview_message->GetFromAddress(from);
	RETURN_VOID_IF_ERROR(m_mail_and_headers_group->AddSuppressExternalEmbedsToolbar(from, g_m2_engine->GetIndexById(IndexTypes::SPAM)->Contains(GetCurrentMessageID())));
	m_mail_and_headers_group->Relayout();
}

/***********************************************************************************
**
**	OnTreeViewMatchChanged
**
***********************************************************************************/
void MailDesktopWindow::OnTreeViewMatchChanged(OpTreeView* treeview)
{
	// Check if there is something to search for
	if (treeview->GetMatchText() && *treeview->GetMatchText())
	{
		// Start the search
		m_running_searches++;
		OpStatus::Ignore(g_m2_engine->GetIndexer()->AsyncFindPartialWords(treeview->GetMatchText(), (MH_PARAM_1)this));
	}
	else
	{
		// No need to search for nothing, just update
		m_index_view->UpdateAfterMatch();
	}
}



/***********************************************************************************
**
**	OnTreeViewOpenStateChanged
**
***********************************************************************************/
void MailDesktopWindow::OnTreeViewOpenStateChanged(OpTreeView* treeview, INT32 id, BOOL open)
{
	if (treeview != m_index_view)
		return;

	IndexModel* model = static_cast<IndexModel*>(m_index_view->GetTreeModel());
	if (model)
		model->SetOpenState(id, open);
}


/***********************************************************************************
**
**	HandleCallback
**
***********************************************************************************/
void MailDesktopWindow::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
		case MSG_M2_SEARCH_RESULTS:
		{
			// Make sure that the result vector is deleted
			OpAutoPtr<OpINTSortedVector> results ((OpINTSortedVector*) par2);

			m_running_searches--;

			// Only update the view if this was the last search started by the user
			if (m_index_view->GetTreeModel() && m_running_searches == 0)
			{
				IndexModel* model = static_cast<IndexModel*>(m_index_view->GetTreeModel());
				model->SetCurrentMatch(results.release());
				m_index_view->UpdateAfterMatch();
			}
			break;
		}
		default:
			DesktopWindow::HandleCallback(msg, par1, par2);
			break;
	}
}


/***********************************************************************************
**
**	ShowSelectedMail
**
***********************************************************************************/

void MailDesktopWindow::ShowSelectedMail(BOOL delay, BOOL force, BOOL timeout_request)
{
	if (!m_mail_and_headers_group->IsVisible())
		return;

	// We are about to display a new message, so we need to stop the mark as read timer.
	m_message_read_timer->Stop();

	if (delay)
	{
		m_mail_selected_timer->Start(200);
		return;
	}

	OpTreeModelItem* item = m_index_view->GetSelectedItem();

	if (item && !force && GetCurrentMessageID() == (UINT32) item->GetID())
		return;

	m_mailview_message->Destroy();
	OP_DELETE(m_mailview_message);
	m_mailview_message = OP_NEW(Message,());

	if (item)
	{
		if (GetCurrentMessageID() != (message_gid_t)item->GetID())
		{
			m_show_raw_message = FALSE;
		}

		if (OpStatus::IsError(GetSelectedMessage(TRUE, force, timeout_request)))
			OpStatus::Ignore(GetSelectedMessage(FALSE, force, timeout_request));
	}
	else if (m_old_session_message_id != 0 && MessageEngine::GetInstance()->GetStore()->MessageAvailable(m_old_session_message_id))
	{
		RETURN_VOID_IF_ERROR(MessageEngine::GetInstance()->GetMessage(*m_mailview_message, m_old_session_message_id, TRUE, timeout_request));
		m_old_session_message_id = 0;
	}

	if (m_mailview_message && m_mailview_message->GetId())
	{
		RETURN_VOID_IF_ERROR(m_mail_and_headers_group->Create(m_mailview_message));
		
		m_mail_view = m_mail_and_headers_group->GetMailView();

		BroadcastDesktopWindowBrowserViewChanged();

		register const char *force_charset;

		force_charset = m_mail_view->GetWindow()->GetForceEncoding();
		if (force_charset != NULL && *force_charset != 0)
		{
			RETURN_VOID_IF_ERROR(m_mailview_message->SetCharset(force_charset, TRUE));
			m_mailview_message->SetForceCharset(TRUE);
		}

		if (!m_mailview_message->IsFlagSet(Message::IS_SEEN))
		{
			g_m2_engine->GetStore()->SetMessageFlag(GetCurrentMessageID(), Message::IS_SEEN, TRUE);
		}

		//Changed message. Remove listener from previous selected message
		RemoveMimeListeners();

		if (m_show_raw_message)
		{
			ShowRawMail(m_mailview_message);
		}
		else
		{
			//Start async decoding
			m_mailview_message->AddRFC822ToUrlListener(this);
		}
	}
	else
	{
		m_mail_and_headers_group->ShowNoMessageSelected();
		m_mail_view = NULL;
	}

	if (IsMessageOnlyView())
		OpStatus::Ignore(UpdateTitle());
	
	OpString tmp;
	index_gid_t index_id;
	Index* index = NULL;

	if(m_mailview_message->IsFlagSet(StoreMessage::IS_NEWSFEED_MESSAGE)
		|| m_mailview_message->IsFlagSet(StoreMessage::IS_OUTGOING))
	{
		m_allow_external_embeds = TRUE;
	}
	else if (OpStatus::IsSuccess(m_mailview_message->GetFromAddress(tmp))
		&& !tmp.IsEmpty()
		&& (index_id = g_m2_engine->GetIndexIDByAddress(tmp))
		&& (index = g_m2_engine->GetIndexById(index_id))
		&& !g_m2_engine->GetIndexById(IndexTypes::SPAM)->Contains(GetCurrentMessageID()))
	{
		m_allow_external_embeds = index->GetIndexFlag(IndexTypes::INDEX_FLAGS_ALLOW_EXTERNAL_EMBEDS);
	} 
	else 
	{
		m_allow_external_embeds = FALSE;
	}
	
	m_allow_external_embeds |= m_mailview_message->IsFlagSet(StoreMessage::ALLOW_EXTERNAL_EMBEDS)
		|| (!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SuppressExternalEmbeds) && !g_m2_engine->GetIndexById(IndexTypes::SPAM)->Contains(GetCurrentMessageID()));

	BroadcastDesktopWindowContentChanged();
}

void MailDesktopWindow::ShowRawMail(const Message* message)
{
	if (!message || !m_mail_view)
		return; //Nothing to do

	OpString8 text;
	message->GetRawMessage(text);

	URL url = urlManager->GetNewOperaURL();

	url.Unload();

	url.SetAttribute(URL::KMIME_ForceContentType, "text/plain"); // FIXME:OOM (returns OP_STATUS)

	url.WriteDocumentData(URL::KNormal, text.CStr());

	url.WriteDocumentDataFinished();
	url.ForceStatus(URL_LOADED);

	URL dummy;
	DocumentManager * doc_manager = m_mail_view->GetWindow()->DocManager();
#ifdef PASS_INVALID_URLS_AROUND
	doc_manager->OpenURL(url, DocumentReferrer(dummy), 0, TRUE, FALSE, FALSE, TRUE);
#else // !PASS_INVALID_URLS_AROUND
	doc_manager->OpenURL(url, DocumentReferrer(dummy), TRUE, FALSE, FALSE, TRUE);
#endif // !PASS_INVALID_URLS_AROUND
	doc_manager->SetAction(VIEWER_OPERA); // Force opera to be viewer to overide settings on text/plain mimetype.
	BroadcastDesktopWindowContentChanged();
}

OP_STATUS MailDesktopWindow::GetDecodedRawMessage(const Message* message, OpString& decoded_message)
{
	if (!message)
	{
		decoded_message.Empty();
		return OpStatus::OK; //Nothing to do
	}

	OpString8 message_encoding;
	const char* OP_MEMORY_VAR encoding = g_pcdisplay->GetForceEncoding();
	if (!encoding || !*encoding)
	{
		message->GetCharset(message_encoding);
		encoding = message_encoding.CStr();
	}

	OpString8 raw_message;
	message->GetRawMessage(raw_message);

	TRAPD(ret, SetFromEncodingL(&decoded_message, encoding, raw_message.CStr(), raw_message.Length()));
	return ret;
}

void MailDesktopWindow::SaveMessageAs(message_gid_t message_id)
{
	if (message_id == 0)
		return;

	m_saveas_message_id = message_id;

	OpStatus::Ignore(g_folder_manager->GetFolderPath(OPFILE_SAVE_FOLDER, m_request.initial_path));

	Message opm2message;
	if (OpStatus::IsSuccess(g_m2_engine->GetMessage(opm2message, m_saveas_message_id, FALSE)))
	{
		Header::HeaderValue header_value;
		opm2message.GetHeaderValue(Header::SUBJECT, header_value);

		OpString header_value_string;
		header_value_string.Set(header_value);
		header_value_string.Strip();
		if (header_value_string.IsEmpty())
		{
			Header::HeaderValue header_value;
			opm2message.GetHeaderValue(Header::MESSAGEID, header_value, TRUE);
			header_value_string.Set(header_value);
			header_value_string.Strip();
		}

		m_request.initial_path.Append(PATHSEP);
		m_request.initial_path.Append(header_value_string);
		m_request.initial_path.Append(UNI_L(".mbs"));


	}

	DesktopFileChooser *chooser;
	RETURN_VOID_IF_ERROR(GetChooser(chooser));

	OpStatus::Ignore(chooser->Execute(GetOpWindow(), this, m_request));
}

void MailDesktopWindow::OnSearchingStarted(const OpStringC& search_text)
{
	OpButton* current_title_button = static_cast<OpButton*> (m_search_in_toolbar->GetWidgetByNameInSubtree("tb_current_title"));
	OpLabel* label = static_cast<OpLabel*> (m_search_in_toolbar->GetWidgetByType(OpTypedObject::WIDGET_TYPE_LABEL, FALSE));

	if (label)
	{
		OpString search_in, search_for_text_in;
		RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_SEARCH_IN, search_in));
		RETURN_VOID_IF_ERROR(search_for_text_in.AppendFormat(search_in.CStr(), search_text.CStr()));
		RETURN_VOID_IF_ERROR(label->SetText(search_for_text_in.CStr()));
	}

	if (g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSearch) == 0)
	{
		if (current_title_button)
		{
			OpString title;
			if (OpStatus::IsSuccess(GetTitleForWindow(title)))
				OpStatus::Ignore(current_title_button->SetText(title));
		}
		m_index_view->SetMatchText(search_text.CStr());
	}
	else if (search_text.HasContent())
	{
		index_gid_t id;
		
		if (m_index_id < IndexTypes::FIRST_SEARCH || m_index_id > IndexTypes::LAST_SEARCH)
			m_search_in_index = m_index_id;
		Index* current_index = g_m2_engine->GetIndexById(m_search_in_index != 0 ? m_search_in_index : m_index_id);
		if (current_title_button && current_index)
			current_title_button->SetText(current_index->GetName());
		
		if (OpStatus::IsSuccess(g_m2_engine->GetIndexer()->StartSearch(search_text, SearchTypes::EXACT_PHRASE, SearchTypes::ENTIRE_MESSAGE, 0, UINT32(-1), id, 0, NULL)))
		{
			if (g_application->HasFeeds() && !g_application->HasMail())
			{
				Index* search_index = g_m2_engine->GetIndexById(id);
				search_index->SetAccountId(g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE)->GetAccountId());
			}
			SetMailViewToIndex(id, NULL, NULL, FALSE);
		}
	}
	m_search_in_toolbar->Show();
}

void MailDesktopWindow::OnSearchingFinished()
{
	if (g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSearch) == 0)
	{
		m_index_view->SetMatchText(UNI_L(""));
	}
	else
	{
		if (m_search_in_index != 0)
			SetMailViewToIndex(m_search_in_index, 0, 0, FALSE);
		m_search_in_index = 0;
	}

	m_search_in_toolbar->AnimatedHide();
}

//Save As
void MailDesktopWindow::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (!m_saveas_message_id || !result.files.GetCount())
	{
		m_saveas_message_id = 0;
		return;
	}

	//Find message
	Message message;
	RETURN_VOID_IF_ERROR(g_m2_engine->GetMessage(message, m_saveas_message_id, TRUE));

	//Find from-value
	OpString8 from;
	time_t recv_time;
	struct tm *gmt;
	const Header* from_header = message.GetHeader(Header::FROM);
	if (from_header)
	{
		const Header::From* from_item = from_header->GetFirstAddress();
		if (from_item)
			OpStatus::Ignore(from_item->GetIMAAAddress(from));
	}
	from.Append(" ");

	message.GetDate(recv_time);
	if (recv_time)
	{
		gmt = op_gmtime(&recv_time);
		if (gmt)
		{
			from.Append(asctime(gmt),24); // ignore return
		}
	}

	//Find newline-string
	//OpString new_line_string16;
	OpString8 new_line_string;
	//g_op_system_info->GetNewlineString(&new_line_string16);
	new_line_string.Set(NEWLINE);
	if (new_line_string.IsEmpty())
		new_line_string.Set("\r\n");

	if (new_line_string.HasContent())
	{
		//Find raw message
		OpString8 raw_message;
		if (message.GetRawMessage(raw_message) == OpStatus::OK)
		{
			//Write mbox
			OpFile file;
			if (OpStatus::IsError(file.Construct(result.files.Get(0)->CStr())) ||
				OpStatus::IsError(file.Open(OPFILE_WRITE|OPFILE_SHAREDENYWRITE)) ||
				OpStatus::IsError(file.Write("From ", 5)) ||
				(from.HasContent() && OpStatus::IsError(file.Write(from.CStr(), from.Length()))) ||
				OpStatus::IsError(file.Write(new_line_string.CStr(), new_line_string.Length())) ||
				OpStatus::IsError(file.Write(raw_message.CStr(), raw_message.Length())) ||
				OpStatus::IsError(file.Close()))
			{
				// DO nothing
			}
		}
	}
	if (result.active_directory.HasContent())
	{
		TRAPD(err, g_pcfiles->WriteDirectoryL(OPFILE_SAVE_FOLDER, result.active_directory.CStr()));
	}
}

OP_STATUS MailDesktopWindow::UpdateTitle()
{
   	OpString title;
	
	RETURN_IF_ERROR(GetTitleForWindow(title));

	if ((m_index_id >= IndexTypes::FIRST_CONTACT || m_index_id < IndexTypes::LAST_CONTACT))
	{
		OpString contact_address;
		Index* index = g_m2_engine->GetIndexById(m_index_id);

		if (index && OpStatus::IsSuccess(index->GetContactAddress(contact_address)))
		{
			OpString formatted_address;
			RETURN_IF_ERROR(MailFormatting::FormatListIdToEmail(contact_address, formatted_address));
			
			HotlistModelItem *item = g_hotlist_manager->GetContactsModel()->GetByEmailAddress(formatted_address);
			if (item && item->GetName().HasContent())
			{
				RETURN_IF_ERROR(title.Set(item->GetName()));
			}
		}
	}
	if (!IsMessageOnlyView() && (m_index_id < IndexTypes::FIRST_SEARCH || m_index_id > IndexTypes::LAST_SEARCH))
	{
		Index* index = g_m2_engine->GetIndexById(m_index_id);
		if (index && index->UnreadCount() > 0)
			RETURN_IF_ERROR(title.AppendFormat(UNI_L(" (%d)"), index->UnreadCount()));
	}

	SetTitle(title.CStr());
	return OpStatus::OK;
}

void MailDesktopWindow::RemoveMimeListeners()
{
	URL url = m_current_message_url_inuse.GetURL();

	if (m_mailview_message)
	{
		m_mailview_message->RemoveRFC822ToUrlListener(this);
		m_current_message_url_inuse.UnsetURL();
	}

	if (m_mail_and_headers_group)
	{
		OpStatus::Ignore(url.SetAttribute(URL::KMIME_RemoveAttachmentListener, static_cast<MIMEDecodeListener*>(m_mail_and_headers_group))); // FIXME: error handling
	}
}

void MailDesktopWindow::GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url)
{
	title.Empty();
	url_string.Empty();

	OpStatus::Ignore(GetTitleForWindow(title));
}

void MailDesktopWindow::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	OpString title;
	if (OpStatus::IsSuccess(GetTitleForWindow(title)))
		text.SetTooltipText(title.CStr());

#ifdef SUPPORT_GENERATE_THUMBNAILS
	if(tooltip && g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInTabTooltips))
	{
		Image image = GetThumbnailImage(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, FALSE, IsListOnlyView());

		if(!image.IsEmpty())
		{
			tooltip->SetImage(image, HasFixedThumbnailImage());
		}
	}
#endif // SUPPORT_GENERATE_THUMBNAILS
}


void MailDesktopWindow::OnUrlCreated(URL* url)
{
	if (!url || !m_mail_view)
		return;

	m_current_message_url_inuse.SetURL(*url);

	URL dummy;
#ifdef PASS_INVALID_URLS_AROUND
	m_mail_view->GetWindow()->DocManager()->OpenURL(*url, DocumentReferrer(dummy), 0, TRUE, FALSE, FALSE, TRUE, NotEnteredByUser, FALSE, TRUE, FALSE);
#else // !PASS_INVALID_URLS_AROUND
	m_mail_view->GetWindow()->DocManager()->OpenURL(*url, DocumentReferrer(dummy), TRUE, FALSE, FALSE, TRUE, NotEnteredByUser, FALSE, TRUE, FALSE);
#endif // !PASS_INVALID_URLS_AROUND

	OpStatus::Ignore(url->SetAttribute(URL::KMIME_AddAttachmentListener, static_cast<MIMEDecodeListener*>(m_mail_and_headers_group))); // FIXME: OOM and error handling
}

void MailDesktopWindow::OnUrlDeleted(URL* url)
{
	if (!url)
		return;

	OpStatus::Ignore(url->SetAttribute(URL::KMIME_RemoveAttachmentListener, static_cast<MIMEDecodeListener*>(m_mail_and_headers_group))); // FIXME: error handling
}

void MailDesktopWindow::OnRFC822ToUrlRestarted()
{
}

void MailDesktopWindow::OnRFC822ToUrlDone(OP_STATUS status)
{
	if (!m_mailview_message)
	{
		RemoveMimeListeners(); //Just to be safe
		return;
	}

	if (status == OpStatus::OK)
	{
		//Extra spam checking of fully decoded message when displayed
		//Should be done on download, but that isn't possible with HTML yet
		g_m2_engine->DelayedSpamFilter(GetCurrentMessageID());

		//Insert text in QuickReply
		Header::HeaderValue contact;

		RETURN_VOID_IF_ERROR(m_mailview_message->GetHeaderValue(Header::REPLYTO, contact, TRUE));
		if (contact.IsEmpty())
			RETURN_VOID_IF_ERROR(m_mailview_message->GetHeaderValue(Header::FROM, contact, TRUE));

		//If we have downloaded the message body, use timer to know that user has read this for certain amount of seconds
		const char* message_body = m_mailview_message->GetRawBody();
		if (message_body && *message_body)
		{
			int time = g_pcm2->GetIntegerPref(PrefsCollectionM2::MarkAsReadAutomatically);
			if (time && !m_mailview_message->IsFlagSet(Message::IS_READ))
				m_message_read_timer->Start(time);
		}

		// If it's a draft, check for attachments in the draft attachment folder
		if (m_mailview_message->IsFlagSet(StoreMessage::IS_OUTGOING) && !m_mailview_message->IsFlagSet(StoreMessage::IS_SENT))
		{
			SimpleTreeModel attachment_model(2);
			AttachmentHolder attachment_holder(attachment_model);
			OpString folder;
			if (OpStatus::IsSuccess(MailFiles::GetDraftsFolder(folder)))
				attachment_holder.Init(m_mailview_message->GetId(), folder);
			
			// loop through the items in the model
			for (INT32 i = attachment_holder.GetCount()-1; i >=0; i--)
			{
				OpString suggested_filename;
				URL attachment_url;
				if (OpStatus::IsSuccess(attachment_holder.GetAttachmentInfoByPosition(i, m_current_message_url_inuse->GetContextId(), suggested_filename, attachment_url)))
					m_mail_and_headers_group->AddAttachment(attachment_url, suggested_filename);
			}

		}
	}
	else
	{
		// how did I come here?
		OP_ASSERT(FALSE);
	}
}

void MailDesktopWindow::OnMessageBeingDeleted()
{
	RemoveMimeListeners();
}

/***********************************************************************************
**
**	OnShow
**
***********************************************************************************/

void MailDesktopWindow::OnShow(BOOL show)
{
	INT32 item = m_index_view->GetSelectedItemPos();

	if (show && item != -1)
	{
		m_index_view->ScrollToItem(item, TRUE);
	}

	MessageEngine::GetInstance()->GetMasterProgress().OnMailViewActivated(this, show);
}

/***********************************************************************************
**
**	OnClose
**
***********************************************************************************/

void MailDesktopWindow::OnClose(BOOL user_initiated)
{
	OnActivate(FALSE, FALSE);
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void MailDesktopWindow::OnLayout(OpWidget *widget)
{
	if (widget == GetWidgetContainer()->GetRoot())
	{
		OpRect rect;
		GetBounds(rect);
		m_splitter->SetRect(rect);

		DesktopWindow::OnLayout(widget);
	}
	else if (widget == GetIndexAndLoadingImageGroup())
	{
		OpRect rect = GetIndexAndLoadingImageGroup()->GetBounds();
		if (m_loading_image)
		{
			m_loading_image->SetRect(rect);
		}
		else
		{
			m_index_view_group->SetRect(rect);
			OpRect top_toolbar_rect(0,0, rect.width, rect.height);
			if (m_left_top_toolbar->IsVisible())
				top_toolbar_rect = m_left_top_toolbar->LayoutToAvailableRect(top_toolbar_rect);

			top_toolbar_rect = m_search_in_toolbar->LayoutToAvailableRect(top_toolbar_rect);
			m_index_view->SetRect(top_toolbar_rect);
		}
	}
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void MailDesktopWindow::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	// ignore OnChange during store loading, otherwise the loading message flashes!
	if (widget == m_index_view && !changed_by_mouse && g_m2_engine->GetStore()->HasFinishedLoading())
	{
		ShowSelectedMail(TRUE);
		return;
	}
}

void MailDesktopWindow::ShowLabelDropdownMenu(INT32 pos)
{
	OpRect rect;
	if (m_index_view->GetColumnVisibility(IndexModel::LabelColumn))
		m_index_view->GetCellRect(pos, IndexModel::LabelColumn, rect);
	else if (m_index_view->GetColumnVisibility(IndexModel::LabelIconColumn))
		m_index_view->GetCellRect(pos, IndexModel::LabelIconColumn, rect);
	else
		return;
	OpRect screen_rect = m_index_view->GetScreenRect();

	rect.x += screen_rect.x;
	rect.y += screen_rect.y;

	g_application->GetMenuHandler()->ShowPopupMenu("Mail Folder Menu", PopupPlacement::AnchorBelow(rect));
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void MailDesktopWindow::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget != m_index_view)
	{
		return;
	}

	if (nclicks == 1 && button == MOUSE_BUTTON_1)
	{
		if (pos != -1 &&  m_index_view->GetItemByPosition(pos))
		{
			GenericIndexModelItem *item = (GenericIndexModelItem*)m_index_view->GetItemByPosition(pos);
			if (item->IsHeader())
				return;
		}
		if ((GetWidgetContainer()->GetView()->GetShiftKeys() & (SHIFTKEY_SHIFT | SHIFTKEY_CTRL)) != 0 && 
			m_index_view->GetSelectedItemCount() != 0 && GetCurrentMessageID() != 0)
			return;
		if (!down)
			ShowSelectedMail(FALSE);
		OpRect rect;
		if (pos != -1 && m_index_view->GetCellRect(pos, IndexModel::LabelColumn, rect) && rect.Contains(OpPoint(x,y)))
		{
			ShowLabelDropdownMenu(pos);
			return; 
		}
		else if (!down && pos != -1 && m_index_view->GetCellRect(pos, IndexModel::StatusColumn, rect))
		{
			GenericIndexModelItem *item = (GenericIndexModelItem*)m_index_view->GetItemByPosition(pos);
			if (!item)
				return;
			rect.y += item->GetNumLines() > 1 ? m_index_view->GetLineHeight() : 0;
			if (rect.Contains(OpPoint(x,y)))
			{
				g_m2_engine->MessageFlagged(item->GetID(), !MessageEngine::GetInstance()->GetStore()->GetMessageFlag(item->GetID(), StoreMessage::IS_FLAGGED));
				m_index_view->OnItemChanged(m_index_view->GetTreeModel(), pos, FALSE);
				if (m_mail_and_headers_group->GetWidgetByName("hb_FlagMessage"))
					m_mail_and_headers_group->GetWidgetByName("hb_FlagMessage")->OnUpdateActionState();
				return;
			}
		}
	}
	if (!down && nclicks == 1 && button == MOUSE_BUTTON_2)
	{
		ShowContextMenu(OpPoint(x,y),FALSE,FALSE);
	}
	else if (nclicks == 2 && button == MOUSE_BUTTON_1 && pos != -1)
	{
		if (!m_index_view->IsThreadImage(OpPoint(x, y), pos) && !g_input_manager->InvokeAction(OpInputAction::ACTION_EDIT_DRAFT))
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_IN_MESSAGE_VIEW);
		}
	}
}

BOOL MailDesktopWindow::ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context)
{
	OpPoint p( point.x+m_index_view->GetScreenRect().x, point.y+m_index_view->GetScreenRect().y);
	if (m_index_view->GetSelectedItemCount() <= 1)
	{
			ShowSelectedMail(FALSE);
	}
	g_application->GetMenuHandler()->ShowPopupMenu("Mail Item Popup Menu", PopupPlacement::AnchorAt(p, center), 0, use_keyboard_context);
	return TRUE;
}



/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void MailDesktopWindow::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (widget == m_index_view)
	{
		DesktopDragObject* drag_object = m_index_view->GetDragObject(DRAG_TYPE_MAIL, x, y);

		if (drag_object)
		{
			drag_object->SetID(m_index_id);
			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}
}

/***********************************************************************************
**
**	OnDragMove
**
***********************************************************************************/

void MailDesktopWindow::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);
	if( widget == m_mail_view )
	{
		drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
		return;
	}
	if( widget == m_index_view && drag_object->GetType() != DRAG_TYPE_MAIL )
	{
		drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
		return;
	}
	if (drag_object->GetType() == DRAG_TYPE_MAIL &&
		((m_index_id >= IndexTypes::FIRST_FOLDER && m_index_id < IndexTypes::LAST_FOLDER) ||
		(m_index_id >= IndexTypes::FIRST_IMAP && m_index_id < IndexTypes::LAST_IMAP) ||
		(m_index_id >= IndexTypes::FIRST_ARCHIVE && m_index_id < IndexTypes::LAST_ARCHIVE) ||
		(m_index_id == IndexTypes::TRASH)))
	{
		Index* m2index = g_m2_engine->GetIndexById(m_index_id);
		if (m2index && m2index->IsReadOnly())
		{
			drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);
			return;
		}

		BOOL copy = GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_CTRL || !(drag_object->GetID() >= IndexTypes::FIRST_FOLDER && drag_object->GetID() < IndexTypes::LAST_FOLDER);

		drag_object->SetDesktopDropType(copy ? DROP_COPY : DROP_MOVE);

		return;
	}
	DesktopWindow::OnDragMove(widget, drag_object, pos, x,y);
}

/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/

void MailDesktopWindow::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (drag_object->IsDropAvailable())
	{
		BOOL copy = drag_object->GetDropType() == DROP_COPY;

		OpINT32Vector message_ids;

		for (int i = 0; i < drag_object->GetIDCount(); i++)
			RETURN_VOID_IF_ERROR(message_ids.Add(drag_object->GetID(i)));

		RETURN_VOID_IF_ERROR(g_m2_engine->ToClipboard(message_ids, drag_object->GetID(), !copy));
		RETURN_VOID_IF_ERROR(g_m2_engine->PasteFromClipboard(m_index_id));

		return;
	}
	DesktopWindow::OnDragDrop(widget, drag_object, pos, x,y);
}


/***********************************************************************************
**
**	AddUndoRedo
**
***********************************************************************************/

OpWorkspace* MailDesktopWindow::GetSDICorrectedParentWorkspace()
{
	if (g_application->IsSDI())
	{
		return g_application->GetBrowserDesktopWindow(TRUE)->GetWorkspace();
	}
	else
	{
		return GetParentWorkspace();
	}
}

/***********************************************************************************
**
**	GetTitleForWindow
**
***********************************************************************************/

OP_STATUS MailDesktopWindow::GetTitleForWindow(OpString& title)
{
	if( IsMessageOnlyView() )
	{
		Header::HeaderValue value;
		
		if (GetCurrentMessageID())
		{
			RETURN_IF_ERROR(m_mailview_message->GetHeaderValue(Header::SUBJECT, value));
			return title.Set(value);
		}
		else
		{
			Message message;
			OpTreeModelItem* selected_item = m_index_view->GetSelectedItem();
			RETURN_IF_ERROR(MessageEngine::GetInstance()->GetMessage(message, selected_item ? selected_item->GetID() : m_old_session_message_id, FALSE));
			RETURN_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, value));		
			return title.Set(value);
		}
	}
	
	Index* m2index = g_m2_engine->GetIndexById(m_index_id);

	if (!m2index)
		return OpStatus::ERR_NULL_POINTER;

	return m2index->GetName(title);
}


/***********************************************************************************
**
**	AddUndoRedo
**
***********************************************************************************/

void MailDesktopWindow::AddUndoRedo(OpInputAction* action, OpINT32Vector& items)
{
	index_gid_t		index_id = m_index_id;
	UndoRedoAction* additional_action = 0;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_DELETE_MAIL: // alias: ACTION_DELETE from keyboard, ACTION_DELETE_MAIL from UI
			// Additional action when deleting mail; this will mark as read as well
			additional_action = OP_NEW(UndoRedoAction, (OpInputAction::ACTION_MARK_AS_READ, index_id));
			if (additional_action)
			{
				// Only add unread items
				Index* unread = g_m2_engine->GetIndexById(IndexTypes::UNREAD);
				if (!unread)
				{
					OP_DELETE(additional_action);
					break;
				}

				for (unsigned i = 0; i < items.GetCount(); i++)
				{
					if (unread->Contains(items.Get(i)))
						additional_action->AddItem(items.Get(i));
				}
			}
			// fall through

		case OpInputAction::ACTION_REMOVE_FROM_VIEW:
		case OpInputAction::ACTION_ADD_TO_VIEW:
			// Some actions might save the index ID in action data
			if (action->GetActionData())
				index_id = (index_gid_t) action->GetActionData();
			// fall through

		case OpInputAction::ACTION_UNDELETE:
		case OpInputAction::ACTION_MARK_AS_READ:
		case OpInputAction::ACTION_MARK_AS_UNREAD:
		case OpInputAction::ACTION_MARK_AS_SPAM:
		case OpInputAction::ACTION_MARK_AS_NOT_SPAM:
//		case OpInputAction::ACTION_CUT:
//      TODO cut/copy/paste undo/redo needs some more code before it can be done
		{
			UndoRedoAction* undo = OP_NEW(UndoRedoAction, (action->GetAction(), index_id, additional_action));
			if (undo)
			{
				undo->SetItems(items);
				while (m_current_undo_redo_position < (INT32) m_undo_redo.GetCount())
				{
					m_undo_redo.Delete(m_undo_redo.GetCount() - 1);
				}

				m_undo_redo.Add(undo);
				m_current_undo_redo_position++;
			}
			break;
		}
	}
}

/***********************************************************************************
**
**	UndoRedo
**
***********************************************************************************/

BOOL MailDesktopWindow::UndoRedo(BOOL undo, BOOL check_if_available_only)
{
	UndoRedoAction* action = NULL;

	action = m_undo_redo.Get(undo ? m_current_undo_redo_position - 1 : m_current_undo_redo_position);

	if (!action)
		return FALSE;

	if (check_if_available_only)
		return TRUE;

	UndoRedo(action, undo);

	if (undo)
		m_current_undo_redo_position--;
	else
		m_current_undo_redo_position++;

	return TRUE;
}

void MailDesktopWindow::UndoRedo(UndoRedoAction* action, BOOL undo)
{
	OpINT32Vector items;
	if (OpStatus::IsError(items.DuplicateOf(action->GetItems())))
		return;

	if (undo)
	{
		OpInputAction::Action opposite_action = OpInputAction::ACTION_UNKNOWN;

		switch (action->GetAction())
		{
			case OpInputAction::ACTION_DELETE:
			case OpInputAction::ACTION_DELETE_MAIL: // alias: ACTION_DELETE from keyboard, ACTION_DELETE_MAIL from UI
				opposite_action = OpInputAction::ACTION_UNDELETE;
				break;

			case OpInputAction::ACTION_REMOVE_FROM_VIEW:
				opposite_action = OpInputAction::ACTION_ADD_TO_VIEW;
				break;
			case OpInputAction::ACTION_ADD_TO_VIEW:
				opposite_action = OpInputAction::ACTION_REMOVE_FROM_VIEW;
				break;

			case OpInputAction::ACTION_UNDELETE:
				opposite_action = OpInputAction::ACTION_DELETE;
				break;

			case OpInputAction::ACTION_MARK_AS_READ:
				opposite_action = OpInputAction::ACTION_MARK_AS_UNREAD;
				break;

			case OpInputAction::ACTION_MARK_AS_UNREAD:
				opposite_action = OpInputAction::ACTION_MARK_AS_READ;
				break;

			case OpInputAction::ACTION_MARK_AS_SPAM:
				opposite_action = OpInputAction::ACTION_MARK_AS_NOT_SPAM;
				break;

			case OpInputAction::ACTION_MARK_AS_NOT_SPAM:
				opposite_action = OpInputAction::ACTION_MARK_AS_SPAM;
				break;
		}

		OpInputAction input_action(opposite_action);
		DoActionOnItems(&input_action, items, action->GetIndex(), FALSE);
	}
	else
	{
		OpInputAction input_action(action->GetAction());
		DoActionOnItems(&input_action, items, action->GetIndex(), FALSE);
	}

	if (action->GetAdditionalUndoRedoAction())
		UndoRedo(action->GetAdditionalUndoRedoAction(), undo);
}

/***********************************************************************************
**
**	DoAllSelectedMails
**
***********************************************************************************/

void MailDesktopWindow::DoAllSelectedMails(OpInputAction* action)
{
	OpINT32Vector items;

	INT32 item_count = m_index_view->GetSelectedItems(items);

	if (item_count == 0)
	{
		return;
	}

	// Execute action
	DoActionOnItems(action, items, m_index_id, TRUE);

	return;
}

/***********************************************************************************
**
**	DoActionOnItems
**
***********************************************************************************/

void MailDesktopWindow::DoActionOnItems(OpInputAction* action, OpINT32Vector& items, INT32 index_id, BOOL add_undo)
{
	if (add_undo)
		AddUndoRedo(action, items);

	BOOL action_done = TRUE;

	// Check if there are actions performed on read-only items, and remove those items from the list
	CheckAndRemoveReadOnly(action, items, index_id);

	if (items.GetCount() == 0)
	{
		return;
	}

	MessageEngine::MultipleMessageChanger changer(items.GetCount());

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_REMOVE_FROM_VIEW:
		case OpInputAction::ACTION_ADD_TO_VIEW:
		{
			Index*      index    = g_m2_engine->GetIndexById(action->GetActionData() ? action->GetActionData() : index_id);

			// Check if this is a filter or label
			if (index && (index->IsFilter() || (IndexTypes::FIRST_LABEL <= m_index_id && m_index_id < IndexTypes::LAST_LABEL)))
			{
				// remove/add messages from/to view
				for (unsigned i = 0; i < items.GetCount(); i++)
				{
					if (action->GetAction() == OpInputAction::ACTION_REMOVE_FROM_VIEW)
						index->RemoveMessage(items.Get(i));
					else
						index->NewMessage(items.Get(i));
				}
			}
			break;
		}
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_DELETE_MAIL: // alias: ACTION_DELETE from keyboard, ACTION_DELETE_MAIL from UI
		{
			g_m2_engine->RemoveMessages(items, g_m2_engine->IsIndexNewsfeed(index_id) && !g_application->HasMail());
			// note: RSS message will be permanently deleted if mail panel is not available for easy access to Trash
			break;
		}
		case OpInputAction::ACTION_DELETE_PERMANENTLY:
		{
			g_m2_engine->RemoveMessages(items, TRUE);
			break;
		}
		case OpInputAction::ACTION_MARK_AS_READ:
			g_m2_engine->MessagesRead(items, TRUE);
			break;

		case OpInputAction::ACTION_MARK_AS_UNREAD:
			g_m2_engine->MessagesRead(items, FALSE);
			break;

		case OpInputAction::ACTION_COPY:
			g_m2_engine->ToClipboard(items, index_id, FALSE);
			break;

		case OpInputAction::ACTION_CUT:
			g_m2_engine->ToClipboard(items, index_id, TRUE);
			break;

		case OpInputAction::ACTION_COPY_MAIL_TO_FOLDER:
			g_m2_engine->ToClipboard(items, index_id, FALSE);
			g_m2_engine->PasteFromClipboard(action->GetActionData());
			break;

		case OpInputAction::ACTION_ARCHIVE_MESSAGE:
			//g_m2_engine->ToClipboard(
			break;

		case OpInputAction::ACTION_UNDELETE:
			{
				g_m2_engine->UndeleteMessages(items);

				// Mark the message as unread if we undelete a message in
				// a view where read messages are hidden

				IndexTypes::ModelType type;
				IndexTypes::ModelAge age;
				IndexTypes::ModelSort sort;
				BOOL ascending;
				INT32 flags;
				message_gid_t selected_message;
				IndexTypes::GroupingMethods grouping_method;

				g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);

				if ((flags&(1<<IndexTypes::MODEL_FLAG_READ))==0)
				{
					g_m2_engine->MessagesRead(items, FALSE);
				}
			}
			break;
		case OpInputAction::ACTION_MARK_AS_SPAM:	
		case OpInputAction::ACTION_MARK_AS_NOT_SPAM:
		{
			g_m2_engine->MarkAsSpam(items, action->GetAction() == OpInputAction::ACTION_MARK_AS_SPAM);

			break;
		}
		case OpInputAction::ACTION_FETCH_COMPLETE_MESSAGE:
		{
			FetchCompleteMessageOnlineModeCallback* callback = OP_NEW(FetchCompleteMessageOnlineModeCallback, ());
			if (callback && OpStatus::IsSuccess(callback->Init(items)))
			{
				g_application->AskEnterOnlineMode(TRUE, callback);
			}
			else
			{
				OP_DELETE(callback);
			}
			break;
		}

		default:
			action_done = FALSE;
	}

	if(!action_done)
	{
		for (int selected = items.GetCount() - 1; selected >= 0; selected--)
		{
			UINT32 id = items.Get(selected);

			switch (action->GetAction())
			{
				case OpInputAction::ACTION_CANCEL_NEWSMESSAGE:
					{
						if (action->GetActionData() != -1)
						{
							CancelMessageDialog* dialog = OP_NEW(CancelMessageDialog, ());
							if (dialog)
								dialog->Init(id, this);
						}
						break;
					}

				case OpInputAction::ACTION_ADD_CONTACT:
				{
					INT32 contact_parent_id = action->GetActionData();
					if (!g_hotlist_manager->GetItemByID(contact_parent_id))
					{
						contact_parent_id = HotlistModel::ContactRoot;
					}
					g_m2_engine->AddToContacts(id, contact_parent_id);
				}
				break;
			}
		}
	}
}

/***********************************************************************************
**
**	CheckAndRemoveReadOnly
**
***********************************************************************************/

BOOL MailDesktopWindow::CheckAndRemoveReadOnly(OpInputAction* action, OpINT32Vector& items, INT32 index_id)
{
	if(items.GetCount() == 0)
		return FALSE;

	BOOL readonly = FALSE;

	switch(action->GetAction())
	{
		// read-write actions
		case OpInputAction::ACTION_CUT:
		case OpInputAction::ACTION_MARK_AS_READ:
		case OpInputAction::ACTION_MARK_AS_UNREAD:
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_DELETE_MAIL: // alias: ACTION_DELETE from keyboard, ACTION_DELETE_MAIL from UI
		case OpInputAction::ACTION_REMOVE_FROM_VIEW:
		case OpInputAction::ACTION_DELETE_PERMANENTLY:
		case OpInputAction::ACTION_UNDELETE:
		{
			Index* m2index;
			if(index_id >= IndexTypes::FIRST_IMAP && index_id < IndexTypes::LAST_IMAP) // IMAP index, check for read-only
			{
				m2index = g_m2_engine->GetIndexById(index_id);
				readonly = m2index && m2index->IsReadOnly();
				if(readonly)
					items.Clear();
			}
			else
			{ // Non-IMAP index, message could also be listed in an IMAP index that is read-only
				for (INT32 it = -1; (m2index = g_m2_engine->GetIndexRange(IndexTypes::FIRST_IMAP, IndexTypes::LAST_IMAP, it)) != NULL; )
				{
					for(UINT32 j = 0; !readonly && j < items.GetCount(); j++)
					{
						readonly = m2index && m2index->Contains(items.Get(j)) && m2index->IsReadOnly();
						if(readonly)
							items.Remove(j);
					}
				}
			}
			break;
		}
		default:
			break;
	}

	if (readonly)
	{
		OpString title, message;
		g_languageManager->GetString(Str::S_READONLY_MESSAGE, title);
		g_languageManager->GetString(Str::S_READONLY_MESSAGE_WARNING, message);

		SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
		if (dialog)
			dialog->Init(WINDOW_NAME_READONLY_MESSAGE_WARNING, title, message, this, Dialog::TYPE_OK);

		return TRUE;
	}

	return FALSE;
}

/***********************************************************************************
**
**	OnTimeOut
**
***********************************************************************************/

void MailDesktopWindow::OnTimeOut(OpTimer* timer)
{
	if (timer == m_mail_selected_timer)
	{
		ShowSelectedMail(FALSE, FALSE, TRUE);
	}
	else if (timer == m_message_read_timer)
	{
		IndexTypes::ModelType type;
		IndexTypes::ModelAge age;
		IndexTypes::ModelSort sort;
		BOOL ascending;
		INT32 flags;
		message_gid_t selected_message;
		IndexTypes::GroupingMethods grouping_method;

		g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);

		if ((flags&(1<<IndexTypes::MODEL_FLAG_READ))==0)
		{
			return;
		}

		OpTreeModelItem* item = m_index_view->GetSelectedItem();

		if (!item)
			return;

		OpINT32Vector message_ids;
		message_ids.Add(item->GetID());
		g_m2_engine->MessagesRead(message_ids, TRUE);
	}
}

/***********************************************************************************
**
**	OnSessionReadL
**
***********************************************************************************/

void MailDesktopWindow::OnSessionReadL(const OpSessionWindow* session_window)
{
	INT32 index_id = session_window->GetValueL("mail index");
	OpString contact_address;
	contact_address.Set(session_window->GetStringL("contact address"));
	m_old_session_message_id = session_window->GetValueL("current message id");
	BOOL message_only_view = session_window->GetValueL("message only view");
	if (m_old_session_message_id != 0 &&
		index_id > IndexTypes::FIRST_THREAD && index_id < IndexTypes::LAST_THREAD
		&& !g_m2_engine->GetIndexById(index_id))
	{
		GotoThread(m_old_session_message_id);
	}
	else
	{
		Init(g_m2_engine->GetIndexById(index_id) ? index_id: IndexTypes::UNREAD_UI, contact_address.CStr(),0,message_only_view);
	}
}

/***********************************************************************************
**
**	OnSessionWriteL
**
***********************************************************************************/

void MailDesktopWindow::OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown)
{
	session_window->SetTypeL(OpSessionWindow::MAIL_WINDOW);
	session_window->SetValueL("mail index", (m_index_id >= IndexTypes::FIRST_SEARCH && m_index_id < IndexTypes::LAST_SEARCH) ? m_search_in_index : m_index_id);
	session_window->SetStringL("contact address", m_contact_address);
	session_window->SetValueL("current message id", GetCurrentMessageID());
	session_window->SetValueL("message only view", IsMessageOnlyView());
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL MailDesktopWindow::OnInputAction(OpInputAction* action)
{
	if (DesktopWindow::OnInputAction(action))
		return TRUE;

	if (m_index_id == 0) //MailDesktopWindow should always have a index_id (it will be 0 during construction)
		return FALSE;

	OpTreeModelItem* item;
	item = m_index_view->GetSelectedItem();
	int selected_items_count = m_index_view->GetSelectedItemCount();

	// fix for DSK-223210
	OpINT32Vector selection;
	if (!item && selected_items_count > 0)
	{
		m_index_view->GetSelectedItems(selection,FALSE);
		item = m_index_view->GetItemByPosition(selection.Get(0));
		
		//We don't want to perform any action on group headers
		if (item && static_cast<GenericIndexModelItem*>(item)->IsHeader())
		{
			item = NULL;
		}
	}

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_COMPOSE_MAIL:
					{
						child_action->SetEnabled(g_application->HasMail() && g_m2_engine->GetStore()->IsNextM2IDReady());
						return TRUE;
					}
				case OpInputAction::ACTION_REPLY:
				case OpInputAction::ACTION_REPLY_ALL:
				case OpInputAction::ACTION_REPLY_TO_SENDER:
				case OpInputAction::ACTION_FORWARD_MAIL:
				case OpInputAction::ACTION_REDIRECT_MAIL:
					{
						child_action->SetEnabled(item != NULL && g_m2_engine->GetStore()->IsNextM2IDReady() && g_m2_engine->GetAccountManager()->GetMailNewsAccountCount());
						return TRUE;
					}
				case OpInputAction::ACTION_GO_TO_THREAD:
				{
					if (child_action->GetActionData() == 1)
					{
						if (m_index_id > IndexTypes::FIRST_THREAD && m_index_id < IndexTypes::LAST_THREAD && m_current_history_position > 0)
						{
							OpString button_text;
							Index* previous_index = g_m2_engine->GetIndexById(m_history.Get(m_current_history_position-1));
							if (previous_index)
							{
								StringUtils::GetFormattedLanguageString(button_text, Str::S_MAIL_THREAD_GO_BACK_TO, previous_index->GetName().CStr());
								child_action->SetActionText(button_text.CStr());
								child_action->SetEnabled(TRUE);
								return TRUE;
							}
						}
						child_action->SetEnabled(FALSE);
					}
					else
					{
						child_action->SetEnabled(item != NULL && (m_index_id < IndexTypes::FIRST_THREAD || m_index_id > IndexTypes::LAST_THREAD));	
					}
					
					return TRUE;
				}
				case OpInputAction::ACTION_COPY_RAW_MAIL:
				case OpInputAction::ACTION_VIEW_MESSAGES_FROM_SELECTED_CONTACT:
				case OpInputAction::ACTION_SAVE_DOCUMENT:
				case OpInputAction::ACTION_SAVE_DOCUMENT_AS:
				{
					child_action->SetEnabled(item != NULL);
					return TRUE;
				}
				case OpInputAction::ACTION_DELETE:
				{
					child_action->SetEnabled(selected_items_count > 0 && item);
					return TRUE;
				}
				case OpInputAction::ACTION_DELETE_MAIL: // alias: ACTION_DELETE from keyboard, ACTION_DELETE_MAIL from UI
				{
					if (selected_items_count > 0 && item)
					{
						Index* trash = g_m2_engine->GetIndexById(IndexTypes::TRASH);
						child_action->SetEnabled(!trash || !trash->Contains(item->GetID()));
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
				case OpInputAction::ACTION_REMOVE_FROM_VIEW:
				{
					Index* index = g_m2_engine->GetIndexById(child_action->GetActionData()? child_action->GetActionData() : m_index_id );
					child_action->SetEnabled(index && item && index->IsFilter() && index->Contains(item->GetID()));
					return TRUE;
				}
				case OpInputAction::ACTION_ADD_TO_VIEW:
				{
					// Messages can be freely added to filters and labels
					Index* index = g_m2_engine->GetIndexById(child_action->GetActionData());
					if (index && item)
					{
						child_action->SetEnabled(index->IsFilter() && !index->Contains(item->GetID()));
						child_action->SetSelected(index->Contains(item->GetID()));
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
				case OpInputAction::ACTION_DELETE_PERMANENTLY:
				case OpInputAction::ACTION_COPY:
				case OpInputAction::ACTION_ADD_CONTACT:
				case OpInputAction::ACTION_COPY_MAIL_TO_FOLDER:
				case OpInputAction::ACTION_ARCHIVE_MESSAGE:
				{
					child_action->SetEnabled(selected_items_count > 0);
					return TRUE;
				}
				case OpInputAction::ACTION_UNDELETE:
				{
					if (selected_items_count == 1 && item)
					{
						Index* trash = g_m2_engine->GetIndexById(IndexTypes::TRASH);
						child_action->SetEnabled(trash && trash->Contains(item->GetID()));
					}
					else
					{
						child_action->SetEnabled(selected_items_count > 0);
					}
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_POPUP_MENU:
				{
					if (child_action->IsActionDataStringEqualTo(UNI_L("Internal Mail View"))
							|| child_action->IsActionDataStringEqualTo(UNI_L("Mail List Option Menu")))
					{
						child_action->SetEnabled(TRUE);
						return TRUE;
					}
					if (child_action->IsActionDataStringEqualTo(UNI_L("Internal Access Points")))
					{
						OpString text;
						Index* m2index = g_m2_engine->GetIndexById(m_index_id);
						if( m2index )
						{
							m2index->GetName(text);
							child_action->SetActionText(text.CStr());
						}
						return TRUE;
					}
					break;
				}
				case OpInputAction::ACTION_MARK_AS_SPAM:
				case OpInputAction::ACTION_MARK_AS_NOT_SPAM:
				{
					if (item)
					{
						Index* spam = g_m2_engine->GetIndexById(IndexTypes::SPAM);
						BOOL is_spam = spam->Contains(item->GetID());
						child_action->SetEnabled((is_spam && child_action->GetAction() == OpInputAction::ACTION_MARK_AS_NOT_SPAM) ||
												(!is_spam && child_action->GetAction() == OpInputAction::ACTION_MARK_AS_SPAM));
					}
					else
					{
						child_action->SetEnabled(selected_items_count > 0);
					}
					return TRUE;
				}
				case OpInputAction::ACTION_MARK_AS_READ:
				case OpInputAction::ACTION_MARK_AS_UNREAD:
				{
					if (selected_items_count > 0 && item)
					{
						BOOL read = g_m2_engine->GetStore()->GetMessageFlags(item->GetID()) & 1 << Message::IS_READ;

						if (child_action->GetAction() == OpInputAction::ACTION_MARK_AS_READ)
							child_action->SetEnabled(!read);
						else
							child_action->SetEnabled(read);
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
				case OpInputAction::ACTION_EDIT_DRAFT:
				{
					if (item && m_mailview_message && g_m2_engine->GetStore()->IsNextM2IDReady())
						child_action->SetEnabled(m_mailview_message->IsFlagSet(Message::IS_OUTGOING));
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_REPLY_TO_REPLYTO:
				{
					child_action->SetEnabled(item && m_mailview_message && (m_mailview_message->GetHeader(Header::REPLYTO) ||
						m_mailview_message->IsFlagSet(StoreMessage::IS_OUTGOING)) // hack to reply to the recipient of an outgoing message
						&& g_m2_engine->GetStore()->IsNextM2IDReady());
					return TRUE;
				}
				case OpInputAction::ACTION_REPLY_TO_LIST:
				{
					if (item && m_mailview_message && g_m2_engine->GetStore()->IsNextM2IDReady() &&
						(m_mailview_message->GetHeader(Header::LISTPOST) ||
						 m_mailview_message->GetHeader(Header::MAILINGLIST) ||
						 m_mailview_message->GetHeader(Header::XMAILINGLIST)))
					{
						child_action->SetEnabled(TRUE);
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
				case OpInputAction::ACTION_FORWARD:
				{
					child_action->SetEnabled(m_current_history_position + 1 < m_history.GetCount());
					return TRUE;
				}
				case OpInputAction::ACTION_BACK:
				{
					child_action->SetEnabled(m_current_history_position > 0);
					return TRUE;
				}
				case OpInputAction::ACTION_UNDO:
				{
					child_action->SetEnabled(UndoRedo(TRUE, TRUE));
					return TRUE;
				}
				case OpInputAction::ACTION_REDO:
				{
					child_action->SetEnabled(UndoRedo(FALSE, TRUE));
					return TRUE;
				}
				case OpInputAction::ACTION_CUT:
				{
					// Cut is only enabled in filters and IMAP folders and archives
					child_action->SetEnabled(
						m_index_view->GetSelectedItemCount() > 0 &&
						( (IndexTypes::FIRST_FOLDER  <= m_index_id && m_index_id < IndexTypes::LAST_FOLDER)  ||
						  (IndexTypes::FIRST_ARCHIVE <= m_index_id && m_index_id < IndexTypes::LAST_ARCHIVE) ||
						  (IndexTypes::FIRST_IMAP    <= m_index_id && m_index_id < IndexTypes::LAST_IMAP) ) );
					return TRUE;
				}
				case OpInputAction::ACTION_PASTE:
				{
					// small hack to pass the action on to other input contexts, somebody might want to handle this action
					if (g_desktop_clipboard_manager->HasText() && g_m2_engine->GetClipboardSource() == 0)
						return FALSE;
					// Pasting is only possible in filters and IMAP folders and archives
					child_action->SetEnabled(g_m2_engine->GetClipboardSource() != 0 &&
							( (IndexTypes::FIRST_FOLDER  <= m_index_id && m_index_id < IndexTypes::LAST_FOLDER)  ||
							  (IndexTypes::FIRST_ARCHIVE <= m_index_id && m_index_id < IndexTypes::LAST_ARCHIVE) ||
							  (IndexTypes::FIRST_IMAP    <= m_index_id && m_index_id < IndexTypes::LAST_IMAP) ) );
					return TRUE;
				}
				case OpInputAction::ACTION_FLAG_MESSAGE:
				{
					if (selected_items_count > 0 && item)
					{
						BOOL flagged = MessageEngine::GetInstance()->GetStore()->GetMessageFlag(item->GetID(), StoreMessage::IS_FLAGGED);
						child_action->SetEnabled(child_action->GetActionData() != flagged);
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}

				case OpInputAction::ACTION_WATCH_CONTACT:
				case OpInputAction::ACTION_WATCH_INDEX:
				{
					if (item)
					{
						Index *index = NULL;
						if (child_action->GetAction() == OpInputAction::ACTION_WATCH_INDEX)
						{
							index = g_m2_engine->GetIndexer()->GetThreadIndex(item->GetID());
						}
						else
						{
							OpString tmp;
							if (OpStatus::IsSuccess(m_mailview_message->GetFromAddress(tmp)) && !tmp.IsEmpty())
							{
								index_gid_t index_id = g_m2_engine->GetIndexIDByAddress(tmp);
								index = g_m2_engine->GetIndexById(index_id);
							}
						}
						if (index)
						{
							child_action->SetEnabled(!index->IsWatched() && !index->IsIgnored());
							child_action->SetSelected(FALSE);
						}
						else
						{
							child_action->SetEnabled(TRUE);
						}
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}

				case OpInputAction::ACTION_STOP_WATCH_CONTACT:
				case OpInputAction::ACTION_STOP_WATCH_INDEX:
				{
					BOOL enabled = FALSE;
					if (item)
					{
						Index *index = NULL;
						if (child_action->GetAction() == OpInputAction::ACTION_STOP_WATCH_INDEX)
						{
							index = g_m2_engine->GetIndexer()->GetThreadIndex(item->GetID());
						}
						else
						{
							OpString tmp;
							if (OpStatus::IsSuccess(m_mailview_message->GetFromAddress(tmp)) && !tmp.IsEmpty())
							{
								index_gid_t index_id = g_m2_engine->GetIndexIDByAddress(tmp);
								index = g_m2_engine->GetIndexById(index_id);
							}
						}

						if (index)
						{
							enabled = index->IsWatched() && !index->IsIgnored();
							child_action->SetSelected(index->IsWatched());
						}
						else
						{
							enabled = FALSE;
						}
					}

					child_action->SetEnabled(enabled);

					return TRUE;
				}

				case OpInputAction::ACTION_IGNORE_INDEX:
				case OpInputAction::ACTION_IGNORE_CONTACT:
				{
					if (item)
					{
						Index *index = NULL;
						if (child_action->GetAction() == OpInputAction::ACTION_IGNORE_INDEX)
						{
							index = g_m2_engine->GetIndexer()->GetThreadIndex(item->GetID());
						}
						else
						{
							OpString tmp;
							if (OpStatus::IsSuccess(m_mailview_message->GetFromAddress(tmp)) && !tmp.IsEmpty())
							{
								index_gid_t index_id = g_m2_engine->GetIndexIDByAddress(tmp);
								index = g_m2_engine->GetIndexById(index_id);
							}
						}
						if (index)
						{
							child_action->SetEnabled(!index->IsIgnored() && !index->IsWatched());
							child_action->SetSelected(FALSE);
						}
						else
						{
							child_action->SetEnabled(TRUE);
						}
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}

				case OpInputAction::ACTION_STOP_IGNORE_INDEX:
				case OpInputAction::ACTION_STOP_IGNORE_CONTACT:
				{
					BOOL enabled = FALSE;
					if (item)
					{
						Index *index = NULL;
						if (child_action->GetAction() == OpInputAction::ACTION_STOP_IGNORE_INDEX)
						{
							index = g_m2_engine->GetIndexer()->GetThreadIndex(item->GetID());
						}
						else
						{
							OpString tmp;
							if (OpStatus::IsSuccess(m_mailview_message->GetFromAddress(tmp)) && !tmp.IsEmpty())
							{
								index_gid_t index_id = g_m2_engine->GetIndexIDByAddress(tmp);
								index = g_m2_engine->GetIndexById(index_id);
							}
						}
						if (index)
						{
							enabled = index->IsIgnored() && !index->IsWatched();
							child_action->SetSelected(index->IsIgnored());
						}
						else
						{
							enabled = FALSE;
						}
					}

					child_action->SetEnabled(enabled);
					return TRUE;
				}

				case OpInputAction::ACTION_RELOAD:
				{
					if ((m_index_id >= IndexTypes::FIRST_NEWSGROUP && m_index_id < IndexTypes::LAST_NEWSGROUP)
						|| g_m2_engine->IsIndexNewsfeed(m_index_id) || m_index_id == IndexTypes::UNREAD_UI)
					{
						child_action->SetEnabled(TRUE);
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_SPLIT_VIEW:
				{
					int mode = g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode);
					child_action->SetSelected(!IsMessageOnlyView() && m_mail_and_headers_group->IsVisible() && mode < 2 && mode == child_action->GetActionData());
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_MESSAGE_VIEW:
				{
					child_action->SetSelected(IsMessageOnlyView() && m_mail_and_headers_group->IsVisible());
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_LIST_VIEW:
				{
					child_action->SetSelected(!IsMessageOnlyView() && !m_mail_and_headers_group->IsVisible());
					return TRUE;
				}
				case OpInputAction::ACTION_DUPLICATE_PAGE:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
				case OpInputAction::ACTION_SET_MAIL_SEARCH_TYPE:
				{
					BOOL selected = (g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSearch) == child_action->GetActionData());

					child_action->SetSelected(selected);
					return TRUE;
				}
				case OpInputAction::ACTION_SET_MAIL_VIEW_AGE:
				{
					IndexTypes::ModelType type;
					IndexTypes::ModelAge age;
					IndexTypes::ModelSort sort;
					BOOL ascending;
					INT32 flags;
					message_gid_t selected_message;
					IndexTypes::GroupingMethods grouping_method;

					g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);

					child_action->SetSelected(age == (IndexTypes::ModelAge)child_action->GetActionData());

					return TRUE;
				}
				case OpInputAction::ACTION_SET_MAIL_VIEW_FLAG:
				case OpInputAction::ACTION_CLEAR_MAIL_VIEW_FLAG:
				{
					IndexTypes::ModelType type;
					IndexTypes::ModelAge age;
					IndexTypes::ModelSort sort;
					BOOL ascending;
					INT32 flags;
					message_gid_t selected_message;
					IndexTypes::GroupingMethods grouping_method;

					g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);

					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SET_MAIL_VIEW_FLAG, (flags&(1<<child_action->GetActionData())) != 0);
					child_action->SetEnabled(m_index_id < IndexTypes::FIRST_FOLDER || m_index_id > IndexTypes::LAST_FOLDER || child_action->GetActionData() != IndexTypes::MODEL_FLAG_HIDDEN);

					return TRUE;
				}
				case OpInputAction::ACTION_CANCEL_NEWSMESSAGE:
				{
					BOOL allow_cancel = FALSE;
					if (item && m_mailview_message)
					{
						allow_cancel = m_mailview_message->IsFlagSet(Message::IS_OUTGOING) &&
									   m_mailview_message->IsFlagSet(Message::IS_NEWS_MESSAGE) &&
									  !m_mailview_message->IsFlagSet(Message::IS_CONTROL_MESSAGE);
					}

					child_action->SetEnabled(allow_cancel);
					return TRUE;
				}
				case OpInputAction::ACTION_FETCH_COMPLETE_MESSAGE:
				{
					if (item)
					{
						Message message;
						g_m2_engine->GetMessage(message,item->GetID());
						if (message.GetAccountPtr() && message.GetAccountPtr()->GetIncomingProtocol() == AccountTypes::IMAP)
						{
							child_action->SetEnabled(TRUE);
						}
						else
						{
							BOOL scheduled_for_fetch = message.GetAccountPtr() && message.GetAccountPtr()->IsScheduledForFetch(item->GetID());
							child_action->SetEnabled(item->GetID() &&
													(message.IsFlagSet(Message::PARTIALLY_FETCHED) ||
													 !g_m2_engine->GetStore()->GetMessageHasBody(item->GetID())) &&
													 !scheduled_for_fetch);
						}
					}
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
				case OpInputAction::ACTION_MARK_THREAD_AS_READ :
				case OpInputAction::ACTION_MARK_THREAD_AND_SELECT_NEXT_UNREAD :
				{
					child_action->SetEnabled(selected_items_count > 0);
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_RAW_MAIL:
				{
					// don't show raw mail when in list view
					child_action->SetEnabled(selected_items_count > 0 && !(m_index_view->IsVisible() && !m_mail_and_headers_group->IsVisible()));
					child_action->SetSelected(m_show_raw_message);
					return TRUE;
				}
				case OpInputAction::ACTION_ENABLE_MSR:
				case OpInputAction::ACTION_DISABLE_MSR:
				{
					child_action->SetEnabled(TRUE);
					child_action->SetSelected(g_pcm2->GetIntegerPref(PrefsCollectionM2::FitToWidth) == 1);
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_SHOW_POPUP_MENU:
			if (action->IsActionDataStringEqualTo(UNI_L("Mail List Option Menu")))
			{
				OpWidget* button = NULL;
				if (action->GetFirstInputContext() && action->GetFirstInputContext()->GetType() == OpTypedObject::WIDGET_TYPE_TOOLBAR)
					button = static_cast<OpWidget*>(action->GetFirstInputContext())->GetWidgetByNameInSubtree("tb_Mail_Sort_Menu");
				
				if (!button)
					button = GetWidgetByName("tb_Mail_Sort_Menu");
				
				DialogContext* controller = button
						? OP_NEW(IndexViewPropertiesController, (button, m_index_id, this))
						: NULL;
				ShowDialog(controller, g_global_ui_context, this);
				return TRUE;
			}
			else if (action->IsActionDataStringEqualTo(UNI_L("Internal Mail View")))
			{
				OpWidget* button = NULL;
				if (action->GetFirstInputContext() && action->GetFirstInputContext()->GetType() == OpTypedObject::WIDGET_TYPE_TOOLBAR)
					button = static_cast<OpWidget*>(action->GetFirstInputContext())->GetWidgetByNameInSubtree("tb_Mail_View");
				
				if (!button)
					button = GetGlobalViewButton();

				DialogContext* controller = button
						? OP_NEW(DefaultMailViewPropertiesController, (button, this))
						: NULL;
				ShowDialog(controller, g_global_ui_context, this);
				return TRUE;
			}
			break;

		case OpInputAction::ACTION_FOCUS_ADDRESS_FIELD:
		{
			m_index_view->SetFocus(FOCUS_REASON_ACTION);
			return TRUE;
		}
		case OpInputAction::ACTION_FOCUS_PAGE:
		{
			if (!m_mail_view)
				return FALSE;

			m_mail_view->SetFocus(FOCUS_REASON_ACTION);

			if (Window *mail_window = m_mail_view->GetWindow())
				if (FramesDocument *frm_doc = (FramesDocument*)mail_window->GetCurrentDoc())
				{
					int mail_body_id = frm_doc->GetNamedSubWinId(UNI_L("omf_body_part_1"));
					if (mail_body_id >= 0)
						if (FramesDocElm *frm = frm_doc->GetFrmDocElm(mail_body_id))
							frm_doc->SetActiveFrame(frm);
				}

			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_FINDTEXT:
		{
			ShowFindTextBar(action->GetActionData());
			return TRUE;
		}
		case OpInputAction::ACTION_FLAG_MESSAGE:
		{
			if (selected_items_count > 0)
			{
				// Fetch the selected threads and their messages
				OpINT32Vector selected_messages;
				m_index_view->GetSelectedItems(selected_messages, TRUE);

				for (unsigned i = 0; i < selected_messages.GetCount(); i++)
				{
					g_m2_engine->MessageFlagged(selected_messages.Get(i), action->GetActionData() == 1);
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_WATCH_INDEX:
		{
			// if this is invoked by a keyboard shortcut, check if the action is actually enabled first
			if (action->GetActionMethod() == OpInputAction::METHOD_KEYBOARD &&
				!(g_input_manager->GetActionState(action,this) & OpInputAction::STATE_ENABLED))
				return FALSE;

			if (selected_items_count > 0)
			{
				// Fetch the selected threads and their messages
				OpINT32Vector selected_threads;
				m_index_view->GetSelectedItems(selected_threads, TRUE);

				for (unsigned i = 0; i < selected_threads.GetCount(); i++)
				{
					Index *index = g_m2_engine->GetIndexer()->GetThreadIndex(selected_threads.Get(i));
					message_gid_t root_message_id = selected_threads.Get(i);

					if (!index)
					{
						OpINTSet thread_ids;
						g_m2_engine->GetStore()->GetThreadIds(root_message_id,thread_ids);
						g_m2_engine->GetIndexer()->CreateThreadIndex(thread_ids, index);
					}

					if (index && !index->IsWatched())
					{
						index->ToggleWatched(TRUE);
						IndexModel *index_model = static_cast <IndexModel*> (m_index_view->GetTreeModel());
						// Change the icon in the tree view for all messages
						OpINT32Vector messages;
						index->GetAllMessages(messages);
						for (i = 0; i < messages.GetCount(); i++)
						{
							if (index_model->GetPosition(messages.Get(i)) != -1)
								index_model->GetItemByIndex(index_model->GetPosition(messages.Get(i)))->Change(FALSE);
						}
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_STOP_WATCH_INDEX:
		{
			// if this is invoked by a keyboard shortcut, check if the action is actually enabled first
			if (action->GetActionMethod() == OpInputAction::METHOD_KEYBOARD &&
				!(g_input_manager->GetActionState(action,this) & OpInputAction::STATE_ENABLED))
				return FALSE;

			if (selected_items_count > 0)
			{
				// Fetch the selected threads and their messages
				OpINT32Vector selected_threads;
				m_index_view->GetSelectedItems(selected_threads, TRUE);

				for (unsigned i = 0;i < selected_threads.GetCount();i++)
				{
					Index *index = g_m2_engine->GetIndexer()->GetThreadIndex(selected_threads.Get(i));

					if (index)
					{
						index->ToggleWatched(FALSE);
						UINT32 i;
						INT32 start_pos;
						OpINT32Vector messages;
						index->ToggleIgnored(FALSE);
						OpTreeModel *tree_model = m_index_view->GetTreeModel();
						g_m2_engine->GetIndexModel(&tree_model,g_application->GetActiveMailDesktopWindow()->GetIndexID(),start_pos);
						IndexModel* index_model = static_cast<IndexModel*>(tree_model);
						index->GetAllMessages(messages);
						for (i = 0; i < messages.GetCount(); i++)
						{
							if (index_model->GetPosition(messages.Get(i)) != -1)
								index_model->GetItemByIndex(index_model->GetPosition(messages.Get(i)))->Change(FALSE);
						}
						g_m2_engine->RemoveIndex(index->GetId());
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_WATCH_CONTACT:
		case OpInputAction::ACTION_STOP_WATCH_CONTACT:
		case OpInputAction::ACTION_IGNORE_CONTACT:
		case OpInputAction::ACTION_STOP_IGNORE_CONTACT:
		{
			// if this is invoked by a keyboard shortcut, check if the action is actually enabled first
			if (action->GetActionMethod() == OpInputAction::METHOD_KEYBOARD &&
				!(g_input_manager->GetActionState(action,this) & OpInputAction::STATE_ENABLED))
				return FALSE;

			if (selected_items_count > 0)
			{
				// Fetch the selected threads and their messages
				OpINT32Vector selected_threads;
				m_index_view->GetSelectedItems(selected_threads, TRUE);
				bool update_model = false;

				for (unsigned i = 0; i < selected_threads.GetCount(); i++)
				{
					Message message;
					g_m2_engine->GetStore()->GetMessage(message, selected_threads.Get(i));

					OpString tmp;
					if (OpStatus::IsError(message.GetFromAddress(tmp)) || tmp.IsEmpty())
					{
						continue;
					}

					index_gid_t index_id = g_m2_engine->GetIndexIDByAddress(tmp);
					Index* index = g_m2_engine->GetIndexById(index_id);

					INT32 start_pos;
					OpTreeModel *tree_model = m_index_view->GetTreeModel();
					g_m2_engine->GetIndexModel(&tree_model,g_application->GetActiveMailDesktopWindow()->GetIndexID(),start_pos);
					IndexModel* index_model = static_cast<IndexModel*>(tree_model);

					if (index)
					{
						if (action->GetAction() == OpInputAction::ACTION_IGNORE_CONTACT && !index->IsIgnored())
						{
							g_m2_engine->IndexRead(index_id);
							index->ToggleIgnored(TRUE);
						}
						else if (action->GetAction() == OpInputAction::ACTION_STOP_IGNORE_CONTACT && index->IsIgnored())
							index->ToggleIgnored(FALSE);
						else if (action->GetAction() == OpInputAction::ACTION_WATCH_CONTACT && !index->IsWatched())
							index->ToggleWatched(TRUE);
						else if (action->GetAction() == OpInputAction::ACTION_STOP_WATCH_CONTACT && index->IsWatched())
							index->ToggleWatched(FALSE);
						else
							continue;

						if ((action->GetAction() == OpInputAction::ACTION_WATCH_CONTACT || action->GetAction() == OpInputAction::ACTION_IGNORE_CONTACT)
							&& !g_hotlist_manager->GetContactsModel()->GetByEmailAddress(tmp))
						{
							// create a contact when following or ignoring a contact that doesn't exist (so that it gets the right icon)
							Header* from_header = message.GetHeader(message.IsFlagSet(Message::IS_RESENT) ? Header::RESENTFROM : Header::FROM);
							if (!from_header)
								continue;
							const Header::From* from_address = from_header->GetFirstAddress();
							if (!from_address)
								continue;

							g_m2_engine->GetGlueFactory()->GetBrowserUtils()->AddContact(from_address->GetAddress(), from_address->GetName());
						}

						ContactModelItem * hotlist_item = static_cast<ContactModelItem*>(g_hotlist_manager->GetContactsModel()->GetByEmailAddress(tmp));
						if (hotlist_item)
						{
							hotlist_item->SetM2IndexId(index_id);
							hotlist_item->ChangeImageIfNeeded();
							update_model = true;
						}

						OpINT32Vector messages;
						index->GetAllMessages(messages);
						for (UINT32 i = 0; i < messages.GetCount(); i++)
						{
							if (index_model->GetPosition(messages.Get(i)) != -1)
								index_model->GetItemByIndex(index_model->GetPosition(messages.Get(i)))->Change(FALSE);
						}
					}
				}

				if (update_model)
				{
					g_hotlist_manager->GetContactsModel()->SetDirty(TRUE);
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_IGNORE_INDEX:
		{
			// if this is invoked by a keyboard shortcut, check if the action is actually enabled first
			if (action->GetActionMethod() == OpInputAction::METHOD_KEYBOARD &&
				!(g_input_manager->GetActionState(action,this) & OpInputAction::STATE_ENABLED))
				return FALSE;

			if (selected_items_count > 0)
			{

				// Fetch the selected threads and their messages
				OpINT32Vector selected_threads;

				IndexModel* model  = static_cast<IndexModel*>(m_index_view->GetTreeModel());
				m_index_view->GetSelectedItems(selected_threads, TRUE);

				for (unsigned i = 0; i < selected_threads.GetCount(); i++)
				{
					Index *index = g_m2_engine->GetIndexer()->GetThreadIndex(selected_threads.Get(i));
					message_gid_t root_message_id = selected_threads.Get(i);

					if (!index)
					{
						OpINTSet thread_ids;
						g_m2_engine->GetStore()->GetThreadIds(root_message_id,thread_ids);
						g_m2_engine->GetIndexer()->CreateThreadIndex(thread_ids, index);
						root_message_id = thread_ids.GetByIndex(0);
					}

					if (index && !index->IsIgnored())
					{
						index->ToggleIgnored(TRUE);
						g_m2_engine->IndexRead(index->GetId());
						if (model->GetPosition(root_message_id) > 0)
						{
							m_index_view->OpenItem(m_index_view->GetItemByID(root_message_id),FALSE);
							model->GetItemByIndex(m_index_view->GetItemByID(root_message_id))->Change(FALSE);
							m_index_view->SetSelectedItemByID(root_message_id);
						}
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_STOP_IGNORE_INDEX:
		{
			// if this is invoked by a keyboard shortcut, check if the action is actually enabled first
			if (action->GetActionMethod() == OpInputAction::METHOD_KEYBOARD &&
				!(g_input_manager->GetActionState(action,this) & OpInputAction::STATE_ENABLED))
				return FALSE;

			if (selected_items_count > 0)
			{
				// Fetch the selected threads and their messages
				OpINT32Vector selected_threads;
				m_index_view->GetSelectedItems(selected_threads, TRUE);

				for (unsigned i = 0;i < selected_threads.GetCount();i++)
				{
					Index *index = g_m2_engine->GetIndexer()->GetThreadIndex(selected_threads.Get(i));

					if (index)
					{
						UINT32 i;
						INT32 start_pos;
						OpINT32Vector messages;
						index->ToggleIgnored(FALSE);
						OpTreeModel *tree_model = m_index_view->GetTreeModel();
						g_m2_engine->GetIndexModel(&tree_model,g_application->GetActiveMailDesktopWindow()->GetIndexID(),start_pos);
						IndexModel* index_model = static_cast<IndexModel*>(tree_model);
						index->GetAllMessages(messages);
						for (i = 0; i < messages.GetCount(); i++)
						{
							if (index_model->GetPosition(messages.Get(i)) != -1)
								index_model->GetItemByIndex(index_model->GetPosition(messages.Get(i)))->Change(FALSE);
						}
						g_m2_engine->RemoveIndex(index->GetId());
					}
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_GO_TO_THREAD:
		{
			if (!action->GetActionData() && item)
			{
				GotoThread(item->GetID());
			}
			else if (action->GetActionData() == 1)
			{
				OpInputAction back(OpInputAction::ACTION_BACK);
				OnInputAction(&back);
			}

			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_MAIL_LABEL_MENU:
		{
			if (!IsMessageOnlyView())
			{
				INT32 pos = m_index_view->GetSelectedItemPos();
				if (pos != -1)
					ShowLabelDropdownMenu(pos);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SET_MAIL_DISPLAY_TYPE:
		{
			Message::TextpartSetting old_mode = (Message::TextpartSetting)g_pcm2->GetIntegerPref(PrefsCollectionM2::MailBodyMode);

			if (old_mode==Message::DECIDED_BY_ACCOUNT && item && m_mailview_message)
				old_mode = m_mailview_message->GetTextPartSetting();

			Message::TextpartSetting new_mode = (Message::TextpartSetting)action->GetActionData();
			TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::MailBodyMode, (int)new_mode));
			if (new_mode != old_mode)
			{
				ShowSelectedMail(FALSE, TRUE);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SET_ENCODING:
		{
            if (m_mail_view && OpStatus::IsSuccess(m_mail_view->SetEncoding(action->GetActionDataString())))
            {
                ShowSelectedMail(FALSE, TRUE);
                return TRUE;
            }
            return FALSE;
        }
		case OpInputAction::ACTION_SHOW_SPLIT_VIEW:
		{
			int mode = g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode);
			if (IsMessageOnlyView() || !m_mail_and_headers_group->IsVisible() || mode != action->GetActionData())
			{
				TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::MailViewMode, action->GetActionData()));
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SHOW_MESSAGE_VIEW:
		{
			if (!IsMessageOnlyView() || !m_mail_and_headers_group->IsVisible())
			{
				SetMessageOnlyView();
				UpdateTitle();
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SHOW_LIST_VIEW:
		{
			if (IsMessageOnlyView() || m_mail_and_headers_group->IsVisible())
			{
				TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::MailViewMode, MailLayout::LIST_ONLY));
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_DUPLICATE_PAGE:
		case OpInputAction::ACTION_OPEN_IN_MESSAGE_VIEW:
		{
			BrowserDesktopWindow* browser = g_application->GetBrowserDesktopWindow(FALSE, FALSE, FALSE, NULL, NULL, 0, 0, TRUE, FALSE, action->IsKeyboardInvoked());

			if (browser)
			{
				UINT32 selected_message = 0;
				OpTreeModelItem* item = m_index_view->GetSelectedItem();

				if (item)
				{
					selected_message = item->GetID();
				}
				if (!item || static_cast<GenericIndexModelItem*>(item)->IsHeader())
				{
					return TRUE;
				}

				MailDesktopWindow* mail_window;
				if (OpStatus::IsSuccess(MailDesktopWindow::Construct(&mail_window, browser->GetWorkspace())))
				{

					// If this an open in message view command force the view to message
					mail_window->Init(m_index_id, m_contact_address.CStr(), GetTitle(), action->GetAction() == OpInputAction::ACTION_OPEN_IN_MESSAGE_VIEW);
					mail_window->m_index_view->SetSelectedItemByID(selected_message);
					mail_window->Show(TRUE);
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SET_MAIL_SEARCH_TYPE:
		{
			if (g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSearch) != action->GetActionData())
			{
				if (action->GetActionData() == 0 && m_search_in_index != 0)
				{
					MailSearchField* search_field = GetMailSearchField();
					if (search_field)
						search_field->IgnoreMatchText(TRUE);
					SetMailViewToIndex(m_search_in_index, 0, 0, action->IsMouseInvoked());
					if (search_field)
						search_field->IgnoreMatchText(FALSE);
					m_search_in_index = 0;
				}
				TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::DefaultMailSearch, action->GetActionData()));
			}
			return TRUE;
		}
		case OpInputAction::ACTION_FOCUS_SEARCH_FIELD:
		{
			MailSearchField* search_field = GetMailSearchField();
			if (search_field)
			{
				search_field->SetFocus(FOCUS_REASON_ACTION);
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_SET_MAIL_VIEW_TYPE:
		{
			IndexTypes::ModelType type;
			IndexTypes::ModelAge age;
			IndexTypes::ModelSort sort;
			BOOL ascending;
			INT32 flags;
			message_gid_t selected_message;
			IndexTypes::GroupingMethods grouping_method;

			g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);
			OpTreeModelItem* item = m_index_view->GetSelectedItem();
			selected_message = (item ? item->GetID() : -1);

			if (type == (IndexTypes::ModelType) action->GetActionData())
			{
				return FALSE;
			}
			type = (IndexTypes::ModelType) action->GetActionData();
			g_m2_engine->SetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);
			m_index_view->SetCloseAllHeaders(type != IndexTypes::MODEL_TYPE_THREADED);
			return TRUE;
		}

		case OpInputAction::ACTION_SET_MAIL_VIEW_AGE:
		{
			IndexTypes::ModelType type;
			IndexTypes::ModelAge age;
			IndexTypes::ModelSort sort;
			BOOL ascending;
			INT32 flags;
			message_gid_t selected_message;
			IndexTypes::GroupingMethods grouping_method;

			g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);
			age = (IndexTypes::ModelAge) action->GetActionData();

			g_m2_engine->SetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);
			return TRUE;
		}

		case OpInputAction::ACTION_SET_MAIL_VIEW_FLAG:
		{
			IndexTypes::ModelType type;
			IndexTypes::ModelAge age;
			IndexTypes::ModelSort sort;
			BOOL ascending;
			INT32 flags;
			message_gid_t selected_message;
			IndexTypes::GroupingMethods grouping_method;

			g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);

			if ((flags&(1<<action->GetActionData()))!=0)
			{
				return FALSE;
			}

			flags = flags|(1<<action->GetActionData());
			g_m2_engine->SetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);

			return TRUE;
		}

		case OpInputAction::ACTION_CLEAR_MAIL_VIEW_FLAG:
		{
			IndexTypes::ModelType type;
			IndexTypes::ModelAge age;
			IndexTypes::ModelSort sort;
			BOOL ascending;
			INT32 flags;
			message_gid_t selected_message;
			IndexTypes::GroupingMethods grouping_method;

			g_m2_engine->GetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);

			if ((flags&(1<<action->GetActionData()))==0)
			{
				return FALSE;
			}

			flags=flags&~(1<<action->GetActionData());
			g_m2_engine->SetIndexModelType(m_index_id, type, age, flags, sort, ascending, grouping_method, selected_message);
			return TRUE;
		}

		case OpInputAction::ACTION_NEXT_ITEM:
		case OpInputAction::ACTION_PREVIOUS_ITEM:
		{
			if( IsFocused(TRUE) )
			{
				return m_index_view->OnInputAction(action);
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SEARCH_MAIL:
		{
			SearchMailDialog* dialog = OP_NEW(SearchMailDialog, ());
			if (dialog)
				dialog->Init(this, action->GetActionData());
			return TRUE;
		}

		case OpInputAction::ACTION_COMPOSE_MAIL:
		{
			ComposeDesktopWindow* window;
			if (OpStatus::IsError(ComposeDesktopWindow::Construct(&window, GetWorkspace())))
				return TRUE;

			Index* index = g_m2_engine->GetIndexById(m_index_id);

			UINT16 account_id = 0;

			if (index)
			{
				account_id = (UINT16)(index->GetAccountId());
				window->InitMessage(MessageTypes::NEW, 0, account_id);
				OpString address;

				IndexSearch* search = index->GetM2Search();
				if (search)
				{
					address.Set(search->GetSearchText());
				}

				if (m_index_id >= IndexTypes::FIRST_CONTACT && m_index_id < IndexTypes::LAST_CONTACT)
				{
					if (address.Find("@") == KNotFound)
					{
						int pos = address.Find(".");
						if (pos != KNotFound)
						{
							address[pos] = '@';
						}
					}
					window->SetHeaderValue(Header::TO, &address);
				}
				else if (m_index_id >= IndexTypes::FIRST_NEWSGROUP && m_index_id < IndexTypes::LAST_NEWSGROUP)
				{
					window->SetHeaderValue(Header::NEWSGROUPS, &address);
				}
			}
			else
			{
				window->InitMessage(MessageTypes::NEW);
			}
			
			window->Show(TRUE);
			return TRUE;
		}

		case OpInputAction::ACTION_PASTE:
		{
			g_m2_engine->PasteFromClipboard(m_index_id);
			return TRUE;
		}

		case OpInputAction::ACTION_MARK_ALL_AS_READ:
		{
			Index* index  = g_m2_engine->GetIndexById(m_index_id);
			Index* unread = g_m2_engine->GetIndexById(IndexTypes::UNREAD);
			if (!index || !unread)
				return FALSE;

			OpINT32Vector items;
			OpInputAction action(OpInputAction::ACTION_MARK_AS_READ);

			for (INT32SetIterator it(index->GetIterator()); it; it++)
			{
				if (unread->Contains(it.GetData()) && !index->MessageHidden(it.GetData()))
					items.Add(it.GetData());
			}

			DoActionOnItems(&action, items, m_index_id, TRUE);

			return TRUE;
		}

		case OpInputAction::ACTION_MARK_THREAD_AS_READ:
		case OpInputAction::ACTION_MARK_THREAD_AND_SELECT_NEXT_UNREAD:
		{
			if (selected_items_count > 0)
			{
				Index*      unread = g_m2_engine->GetIndexById(IndexTypes::UNREAD);
				IndexModel* model  = static_cast<IndexModel*>(m_index_view->GetTreeModel());

				// Fetch the selected threads and their messages
				OpINT32Vector selected_threads;
				OpINT32Vector mark_as_read;

				m_index_view->GetSelectedItems(selected_threads, TRUE);

				for (unsigned i = 0; i < selected_threads.GetCount(); i++)
				{
					OpINT32Vector thread_ids;
					model->GetThreadIds(selected_threads.Get(i), thread_ids);

					for (unsigned j = 0; j < thread_ids.GetCount(); j++)
					{
						if (unread->Contains(thread_ids.Get(j)))
							mark_as_read.Add(thread_ids.Get(j));
					}
				}

				if (action->GetAction() == OpInputAction::ACTION_MARK_THREAD_AND_SELECT_NEXT_UNREAD)
				{
					OpINT32Vector thread_ids;
					message_gid_t last_selected_message = 0;
					for (INT32 i = selected_threads.GetCount() - 1; i >= 0; i--)
					{
						if (model->GetItemByID(selected_threads.Get(i))->IsHeader())
						{
							continue;
						}
						last_selected_message = selected_threads.Get(i);
						model->GetThreadIds(last_selected_message, thread_ids);
						break;
					}

					Index* index = g_m2_engine->GetIndexById(m_index_id);
					message_gid_t message_id = last_selected_message;
					for (int i = thread_ids.GetCount() - 1; i >= 0; i--)
					{
						message_id = thread_ids.Get(i);
						if (index->Contains(message_id))
							break;
					}
					m_index_view->SetSelectedItemByID(message_id);
					m_index_view->SelectNext(TRUE,TRUE);
				}

				// Mark messages as read
				OpInputAction mark_action(OpInputAction::ACTION_MARK_AS_READ);
				DoActionOnItems(&mark_action, mark_as_read, m_index_id);

				// Select next unread thread if so desired.
				if (action->GetAction() == OpInputAction::ACTION_MARK_THREAD_AND_SELECT_NEXT_UNREAD)
				{
					if (item == m_index_view->GetSelectedItem())
						return g_input_manager->InvokeAction(OpInputAction::ACTION_SELECT_NEXT_UNREAD, 0, 0, m_index_view);
				}
			}

			return TRUE;
		}

		case OpInputAction::ACTION_BACK:
		{
			if (m_current_history_position)
			{
				m_current_history_position--;
				OpString empty;
				SetMailViewToIndex(m_history.Get(m_current_history_position), NULL, NULL, FALSE);
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_FORWARD:
		{
			if (m_current_history_position+1 < m_history.GetCount())
			{
				m_current_history_position++;
				OpString empty;
				SetMailViewToIndex(m_history.Get(m_current_history_position), NULL, NULL, FALSE);
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_UNDO:
		{
			return UndoRedo(TRUE, FALSE);
		}
		case OpInputAction::ACTION_REDO:
		{
			return UndoRedo(FALSE, FALSE);
		}
		case OpInputAction::ACTION_RELOAD:
		{
			ActionReloadOnlineModeCallback* callback = OP_NEW(ActionReloadOnlineModeCallback, (m_index_id));
			if (callback)
				g_application->AskEnterOnlineMode(TRUE, callback);

			return TRUE;
		}
		case OpInputAction::ACTION_NEW_FOLDER:
		{
			Index* index;
			index_gid_t parent = IndexTypes::CATEGORY_LABELS;

			if (action->GetActionData() >= IndexTypes::FIRST_FOLDER && action->GetActionData() < IndexTypes::LAST_FOLDER )
			{
				parent = action->GetActionData();
			}

			index = g_m2_engine->CreateIndex(parent, TRUE);
			if (index)
			{
				if (item)
					index->NewMessage(item->GetID());

				LabelPropertiesController* controller = OP_NEW(LabelPropertiesController, (index->GetId()));
				ShowDialog(controller, g_global_ui_context, this);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_MARK_AND_SELECT_NEXT_UNREAD:
		{
		    OpInputAction action(OpInputAction::ACTION_MARK_AS_READ);
		    DoAllSelectedMails(&action);

			GenericIndexModelItem* selected_item = static_cast<GenericIndexModelItem*>( m_index_view->GetSelectedItem());
			if (item == selected_item || (selected_item && selected_item->IsHeader()))
				g_input_manager->InvokeAction(OpInputAction::ACTION_SELECT_NEXT_UNREAD, 0, NULL, m_index_view);

			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_RAW_MAIL:
		{
			m_show_raw_message = !m_show_raw_message;

			if(m_show_raw_message)
			{
				GetSelectedMessage(TRUE);

				ShowRawMail(m_mailview_message);
			}
			else
			{
				ShowSelectedMail(FALSE,TRUE,FALSE);
			}

			return TRUE;
		}
		case OpInputAction::ACTION_ADD_FILTER_FROM_SEARCH:
		{
			Index* index = g_m2_engine->CreateIndex(IndexTypes::CATEGORY_LABELS, TRUE);
			if (g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSearch) == 0)
			{
				index_gid_t id;
				OpString search_text;
				RETURN_VALUE_IF_ERROR(search_text.Set((m_index_view->GetMatchText())), TRUE);
				RETURN_VALUE_IF_ERROR(index->SetName(search_text.CStr()), TRUE);
				RETURN_VALUE_IF_ERROR(g_m2_engine->GetIndexer()->StartSearch(search_text, SearchTypes::EXACT_PHRASE, SearchTypes::ENTIRE_MESSAGE, 0, UINT_MAX, id, m_index_id, index), TRUE);
			}
			else
			{
				OP_ASSERT(m_index_id < IndexTypes::FIRST_SEARCH || m_index_id > IndexTypes::LAST_SEARCH);
				IndexSearch *search = index->AddM2Search();
				Index* search_index = g_m2_engine->GetIndexById(m_index_id);
				IndexSearch *search_to_copy = search_index ? search_index->GetSearch(0) : 0;
				if (!search || !search_to_copy)
					return TRUE;
				*search = *search_to_copy;
				RETURN_VALUE_IF_ERROR(index->SetName(search->GetSearchText()), TRUE);
				for (INT32SetIterator it(search_index->GetIterator()); it; it++)
				{
					RETURN_IF_ERROR(index->NewMessage(it.GetData()));
				}
				RETURN_VALUE_IF_ERROR(g_m2_engine->UpdateIndex(index->GetId()), TRUE);
			}
			MailSearchField* search_field = GetMailSearchField();
			if (search_field)
			{
				search_field->IgnoreMatchText(TRUE);
			}
			SetMailViewToIndex(index->GetId(), NULL, NULL, TRUE);
			if (search_field)
			{
				RETURN_VALUE_IF_ERROR(search_field->SetText(UNI_L(""), TRUE), TRUE);
				m_search_in_index = 0;
				m_search_in_toolbar->AnimatedHide();
				search_field->IgnoreMatchText(FALSE);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_CANCEL:
		{
			MailSearchField* search_field = GetMailSearchField();
			if (search_field)
			{
				OpStatus::Ignore(search_field->SetText(UNI_L("")));
			}
			return TRUE;
		}
		case OpInputAction::ACTION_COPY_RAW_MAIL:
		{
			if (m_mailview_message)
			{
				OpString text;
				GetDecodedRawMessage(m_mailview_message, text);

				g_desktop_clipboard_manager->PlaceText(text.HasContent() ? text.CStr() : UNI_L(""), g_application->GetClipboardToken());
			}

			return TRUE;
		}
		case OpInputAction::ACTION_DELETE: // alias for keypress
		{
			if (action->GetAction() == OpInputAction::ACTION_DELETE && (m_index_id == IndexTypes::TRASH || g_m2_engine->GetIndexById(m_index_id)->GetSpecialUseType() == AccountTypes::FOLDER_TRASH))
			{
				OpInputAction replacement_action(OpInputAction::ACTION_DELETE_PERMANENTLY);
				DoAllSelectedMails(&replacement_action);
				return TRUE;
			}
			// fall through
		}
		case OpInputAction::ACTION_MARK_AS_SPAM:
		case OpInputAction::ACTION_MARK_AS_NOT_SPAM:
		case OpInputAction::ACTION_MARK_AS_READ:
		case OpInputAction::ACTION_MARK_AS_UNREAD:
		case OpInputAction::ACTION_DELETE_MAIL:
		case OpInputAction::ACTION_COPY:
		case OpInputAction::ACTION_CUT:
		case OpInputAction::ACTION_REMOVE_FROM_VIEW:
		case OpInputAction::ACTION_DELETE_PERMANENTLY:
		case OpInputAction::ACTION_CANCEL_NEWSMESSAGE:
		case OpInputAction::ACTION_FETCH_COMPLETE_MESSAGE:
		case OpInputAction::ACTION_ADD_CONTACT:
		case OpInputAction::ACTION_COPY_MAIL_TO_FOLDER:
		case OpInputAction::ACTION_ADD_TO_VIEW:
		case OpInputAction::ACTION_UNDELETE:
		case OpInputAction::ACTION_ARCHIVE_MESSAGE:
		{
			if (action->GetActionState() & OpInputAction::STATE_ENABLED)
			{
				DoAllSelectedMails(action);
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
			ShowContextMenu(m_splitter->GetBounds().Center(),TRUE,TRUE);
			return TRUE;
	}

	if (item == NULL)
	{
		if (action->IsLowlevelAction())
			return FALSE;

		if (m_mail_view && m_mail_view->OnInputAction(action))
			return TRUE;

		return m_index_view->OnInputAction(action);
	}

	// then only actions that need selected items

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_EDIT_DRAFT:
		{
			if (!m_mailview_message || !m_mailview_message->IsFlagSet(Message::IS_OUTGOING))
				return FALSE;

			ComposeDesktopWindow* window = NULL;
			BrowserDesktopWindow* browser = g_application->GetActiveBrowserDesktopWindow();

			if (browser && item->GetID())
			{
				OpVector<DesktopWindow> composers;
				OpStatus::Ignore(browser->GetWorkspace()->GetDesktopWindows(composers, WINDOW_TYPE_MAIL_COMPOSE));
				for (UINT32 i = 0; i < composers.GetCount(); i++)
				{
					window = static_cast<ComposeDesktopWindow*>(composers.Get(i));

					if (window->GetMessageId() == item->GetID())
					{
						window->Activate();
						return TRUE;
					}

					window = NULL;
				}
			}

			if (window == NULL)
			{
				if (OpStatus::IsError(ComposeDesktopWindow::Construct(&window, GetSDICorrectedParentWorkspace())))
					return TRUE;
			}
			if (window)
			{
				window->InitMessage(MessageTypes::REOPEN, item->GetID());
				window->Show(TRUE);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_REPLY_ALL:
		{
			OpenComposeWindow(MessageTypes::REPLYALL, item->GetID());
			return TRUE;
		}
		case OpInputAction::ACTION_REPLY_TO_SENDER:
		{		
			OpenComposeWindow(MessageTypes::REPLY_TO_SENDER, item->GetID());
			return TRUE;
		}
		case OpInputAction::ACTION_REPLY_TO_REPLYTO:
		case OpInputAction::ACTION_REPLY:
		{
			if ((action->GetActionState() & OpInputAction::STATE_DISABLED) == OpInputAction::STATE_DISABLED)
			{
				return FALSE;
			}
			else
			{
				OpenComposeWindow(MessageTypes::REPLY, item->GetID());
				return TRUE;
			}
		}
		case OpInputAction::ACTION_REPLY_TO_LIST:
		{
			if ((action->GetActionState() & OpInputAction::STATE_DISABLED) == OpInputAction::STATE_DISABLED)
			{
				return FALSE;
			}
			else
			{
				OpenComposeWindow(MessageTypes::REPLY_TO_LIST, item->GetID());
				return TRUE;
			}
		}
		case OpInputAction::ACTION_FORWARD_MAIL:
		{
			OpenComposeWindow(MessageTypes::FORWARD, item->GetID());
			return TRUE;
		}
		case OpInputAction::ACTION_REDIRECT_MAIL:
		{
			OpenComposeWindow(MessageTypes::REDIRECT, item->GetID());
			return TRUE;
		}
		case OpInputAction::ACTION_VIEW_MESSAGES_FROM_SELECTED_CONTACT:
		{
			OpString address;
			g_m2_engine->GetFromAddress(item->GetID(), address);

			if (address.HasContent())
			{
				SetMailViewToIndex(0, address.CStr(), NULL, FALSE);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SAVE_DOCUMENT:
		case OpInputAction::ACTION_SAVE_DOCUMENT_AS:
		{
			SaveMessageAs(item->GetID());
			return TRUE;
		}

		case OpInputAction::ACTION_DISPLAY_EXTERNAL_EMBEDS:
		{
			BOOL is_selected = FALSE;
			OpButton *checkbox =  static_cast<OpButton*>(GetWidgetByName("tb_checkbox_for_all"));
			if (checkbox)
			{
				is_selected = checkbox->GetValue();
			}
			if (is_selected)
			{
				OpString from;
				RETURN_VALUE_IF_ERROR(m_mailview_message->GetFromAddress(from), TRUE);
				index_gid_t index_id = g_m2_engine->GetIndexIDByAddress(from);
				Index* index = g_m2_engine->GetIndexById(index_id);
				RETURN_VALUE_IF_NULL(index, TRUE);
				index->SetIndexFlag(IndexTypes::INDEX_FLAGS_ALLOW_EXTERNAL_EMBEDS, TRUE);
			}
			else
			{
				g_m2_engine->GetStore()->SetMessageFlag(GetCurrentMessageID(), StoreMessage::ALLOW_EXTERNAL_EMBEDS, true);
				m_mailview_message->SetFlag(StoreMessage::ALLOW_EXTERNAL_EMBEDS, TRUE);
			}
			ShowSelectedMail(FALSE,TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_ENABLE_MSR:
		case OpInputAction::ACTION_DISABLE_MSR:
		{
			TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::FitToWidth, !g_pcm2->GetIntegerPref(PrefsCollectionM2::FitToWidth)));
			RETURN_VALUE_IF_ERROR(err, FALSE);
			OpWindowCommander *wc = GetWindowCommander();
			if (wc)
			{
				wc->SetLayoutMode(g_pcm2->GetIntegerPref(PrefsCollectionM2::FitToWidth) ? OpWindowCommander::AMSR : OpWindowCommander::NORMAL);
			}
			return TRUE;
		}
	}

	if (action->IsLowlevelAction())
		return FALSE;

	if (m_mail_and_headers_group && m_mail_and_headers_group->OnInputAction(action))
		return TRUE;

	return m_index_view->OnInputAction(action);
}

void MailDesktopWindow::OpenComposeWindow(MessageTypes::CreateType type, message_gid_t message_id)
{

	ComposeDesktopWindow* window;
	if (OpStatus::IsSuccess(ComposeDesktopWindow::Construct(&window, GetSDICorrectedParentWorkspace())))
	{
		window->InitMessage(type, message_id);
		window->Show(TRUE);
	}
}

/***********************************************************************************
**
**	OnMessageBodyChanged
**
***********************************************************************************/

void MailDesktopWindow::OnMessageBodyChanged(message_gid_t message_id)
{
	// Not possible to load a message here during store loading, because it needs to happen in between 2 store blocks 
	// (because of MultipleMessageHandler), handle that case in OnMessageChanged
	if (GetCurrentMessageID() == message_id && g_m2_engine->GetStore()->HasFinishedLoading())
	{
		ShowSelectedMail(FALSE, TRUE);
	}
	else if (message_id == UINT_MAX)
	{
		if (m_loading_image)
		{
			m_loading_image->Delete();
			m_loading_image = NULL;
		}
		// Store has finished loading, threads and sortings are now ready, let's display it correctly in the window
		if (m_old_session_message_id != 0 &&
			m_index_id > IndexTypes::FIRST_THREAD && m_index_id < IndexTypes::LAST_THREAD)
		{
			GotoThread(m_old_session_message_id);
		}
		else if (IsMessageOnlyView())
		{
			if (m_old_session_message_id != 0)
			{
				m_index_view->SetSelectedItemByID(m_old_session_message_id);
				ShowSelectedMail(FALSE,TRUE);
				m_old_session_message_id = 0;
			}
			else
			{
				m_index_view->SetSelectedItemByID(GetCurrentMessageID());
			}
		}
		else
		{
			Init(m_index_id, m_contact_address.CStr());
		}
	}
}

/***********************************************************************************
**
**	OnMessageChanged
**
***********************************************************************************/

void MailDesktopWindow::OnMessageChanged(message_gid_t message_id)
{
	// store is loaded in blocks of messages and receives a OnMessageChanged(UINT_MAX) when a block is ready
	// check if the message we want to display is available and display it
	if (message_id == UINT_MAX && m_old_session_message_id != 0 && g_m2_engine->GetStore()->MessageAvailable(m_old_session_message_id))
	{
		ShowSelectedMail(FALSE, TRUE);
	}
}

OP_STATUS MailDesktopWindow::UpdateStatusText()
{
	OpString info, format;
	Index* m2index = g_m2_engine->GetIndexById(m_index_id);
	if (!m2index)
		return OpStatus::ERR;

	if (m_index_id == IndexTypes::UNREAD_UI)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_UNREAD_MAIL_HEADER, format));
		RETURN_IF_ERROR(info.AppendFormat(format.CStr(), m2index->UnreadCount()));
	}
	else if (m_index_id == IndexTypes::TRASH || m2index->GetSpecialUseType() == AccountTypes::FOLDER_TRASH)
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_UNREAD_AND_TOTAL_MAIL_HEADER, format));
		RETURN_IF_ERROR(info.AppendFormat(format.CStr(), m2index->UnreadCount(), m2index->MessageCount()));
	}
	else
	{
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_UNREAD_AND_TOTAL_MAIL_HEADER, format));
		RETURN_IF_ERROR(info.AppendFormat(format.CStr(), m2index->UnreadCount(), m2index->MessageCount() - m2index->CommonCount(IndexTypes::TRASH) ));
	}

	SetStatusText(info, STATUS_TYPE_INFO);
	return OpStatus::OK;
}


/***********************************************************************************
**
**	OnIndexChanged
**
***********************************************************************************/

void MailDesktopWindow::OnIndexChanged(UINT32 index_id)
{
	if (m_index_id == index_id || index_id == UINT_MAX)
	{
		OpStatus::Ignore(UpdateStatusText());
		OpStatus::Ignore(UpdateTitle());
	}
}

/***********************************************************************************
**
**	PrefChanged
**
***********************************************************************************/

void MailDesktopWindow::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if(pref == PrefsCollectionM2::ShowQuickHeaders || pref == PrefsCollectionDoc::SuppressExternalEmbeds)
	{
		ShowSelectedMail(FALSE, TRUE);
	}
	else if (pref == PrefsCollectionM2::MailViewMode)
	{
		switch (newvalue)
		{
			case MailLayout::LIST_ON_TOP_MESSAGE_BELOW:
			case MailLayout::LIST_ON_LEFT_MESSAGE_ON_RIGHT:
			{
				SetListAndMessageView(newvalue == MailLayout::LIST_ON_LEFT_MESSAGE_ON_RIGHT);
				break;
			}
			case MailLayout::LIST_ONLY:
			{
				SetListOnlyView();
				break;
			}
		}
		SetCorrectToolbar(m_index_id);
		UpdateTreeview(m_index_id);
		ShowSelectedMail(TRUE, FALSE);
		UpdateTitle();
	}
	else if(pref == PrefsCollectionM2::DefaultMailSortingAscending || pref == PrefsCollectionM2::DefaultMailSorting)
	{
		UpdateSorting();
	}
}


/***********************************************************************************
**
**	OnActivate
**
***********************************************************************************/

void MailDesktopWindow::OnActivate(BOOL activate, BOOL first_time)
{
	MessageEngine::GetInstance()->GetMasterProgress().OnMailViewActivated(this, activate);

#ifdef M2_MAPI_SUPPORT
	if (activate && first_time)
		g_m2_engine->CheckDefaultMailClient();
#endif //M2_MAPI_SUPPORT

	if (!g_pcm2->GetIntegerPref(PrefsCollectionM2::AutoMailPanelToggle))
		return;

	if (!IsMessageOnlyView() && m_old_selected_panel != PANEL_HIDDEN_BY_USER)
	{
		BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();

		// when activating a mail window, open the mail panel, when disactivating the mail window, show the panel that used to be open (or none)
		Hotlist* hotlist = browser_window ? browser_window->GetHotlist(): NULL;

		if (activate)
		{
			if (hotlist)
			{
				RETURN_VOID_IF_ERROR(browser_window->InitHiddenInternals());

				m_old_selected_panel = hotlist->GetAlignment() != OpBar::ALIGNMENT_OFF ? hotlist->GetSelectedPanel() : PANEL_ALIGNMENT_OFF;

				MailPanel* mail_panel = static_cast<MailPanel*>(hotlist->GetPanelByType(PANEL_TYPE_MAIL));
				if (mail_panel)
				{
					// make sure the mail panel is focusing the correct index
					mail_panel->SetSelectedIndex(m_index_id);
				}

				// only show the mail panel if it's not shown already; otherwise we get focus issues DSK-309620
				if (m_old_selected_panel < 0 || (hotlist->GetPanel(hotlist->GetSelectedPanel())->GetType() != OpTypedObject::PANEL_TYPE_MAIL &&
					hotlist->GetPanel(hotlist->GetSelectedPanel())->GetType() != OpTypedObject::PANEL_TYPE_CONTACTS))
				{
					g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PANEL, 0, UNI_L("Mail"), g_application->GetActiveDesktopWindow());

					// now the focus is in the panel, it should be in the window
					if (!IsMessageOnlyView())
					{
						m_index_view->SetFocus(FOCUS_REASON_OTHER);
					}
					else if (m_mail_view)
					{
						m_mail_view->SetFocus(FOCUS_REASON_OTHER);
					}
				}
			}
		}
		else 		
		{
			if (g_application->GetActiveDesktopWindow(FALSE))
			{
				if (g_application->GetActiveDesktopWindow(FALSE)->GetType() != OpTypedObject::WINDOW_TYPE_MAIL_VIEW)
				{
					if (hotlist)
						RETURN_VOID_IF_ERROR(browser_window->InitHiddenInternals());

					// if the user has changed or closed the panel, we won't change back to what he used to have
					if (hotlist && hotlist->GetSelectedPanel() != PANEL_HIDDEN && hotlist->GetAlignment() != OpBar::ALIGNMENT_OFF && 
						hotlist->GetPanel(hotlist->GetSelectedPanel())->GetType() == OpTypedObject::PANEL_TYPE_MAIL)
					{
						if (m_old_selected_panel == PANEL_ALIGNMENT_OFF)
							hotlist->SetAlignment(OpBar::ALIGNMENT_OFF);
						else
							hotlist->SetSelectedPanel(m_old_selected_panel);
					}
					else
					{
						m_old_selected_panel = PANEL_HIDDEN_BY_USER;
					}
				}
				else
				{
					static_cast<MailDesktopWindow*>(g_application->GetActiveDesktopWindow(FALSE))->SetOldSelectedPanel(m_old_selected_panel);
				}
			}
		}
	}
}

OpWidget* MailDesktopWindow::GetGlobalViewButton()
{
	OpWidget* anchor_widget = m_left_top_toolbar->GetWidgetByNameInSubtree("tb_Mail_View");
	if (anchor_widget)
		return anchor_widget;
	anchor_widget = m_mail_and_headers_group->GetMessageToolbar()->GetWidgetByNameInSubtree("tb_Mail_View");
	
	if (anchor_widget)
		return anchor_widget;

	return GetWidgetByName("tb_Mail_View");
}

#endif // M2_SUPPORT
