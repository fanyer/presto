/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 * 
 * perarne@opera.com
 * 
 */

#include "core/pch.h"

#include "modules/history/direct_history.h"
#include "modules/widgets/OpEdit.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/url/url_socket_wrapper.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/dragdrop/dragdrop_data_utils.h"

#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/controller/FavoritesOverlayController.h"
#include "adjunct/quick/controller/AddressbarOverlayController.h"
#include "adjunct/quick/dialogs/PasswordDialog.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/models/HistoryAutocompleteModel.h"
#include "adjunct/quick/widgets/OpProgressField.h"
#include "adjunct/quick/widgets/OpTrustAndSecurityButton.h"
#include "adjunct/quick/widgets/FavoritesCompositeListener.h"
#include "adjunct/quick/widgets/URLFavMenuButton.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/search_engine/WordHighlighter.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick/models/Bookmark.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/managers/DesktopSecurityManager.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/desktop_util/skin/SkinUtils.h"
#include "adjunct/desktop_util/string/i18n.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/widget_utils/WidgetUtils.h"

#ifdef QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW
# include "adjunct/quick_toolkit/widgets/OpPage.h"
#endif // QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW

#include "adjunct/desktop_util/string/stringutils.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES

#ifdef ENABLE_USAGE_REPORT
#include "adjunct/quick/usagereport/UsageReport.h"
#endif

#ifdef _MACINTOSH_
#include "modules/skin/OpSkinManager.h"
#endif // _MACINTOSH_

#ifdef EXTENSION_SUPPORT
#include "modules/gadgets/OpGadgetManager.h"
#endif

#include "adjunct/quick/widgets/DocumentView.h"
#include "adjunct/quick/managers/TipsManager.h"

#ifndef SEARCH_SUGGEST_DELAY
#define SEARCH_SUGGEST_DELAY	250		// wait 250ms after the last letter has been entered before doing a search suggestion call
#endif // SEARCH_SUGGEST_DELAY

// don't immediately remove the highlight when moving mouse over the address bar
// to prevent flickering
#ifndef ADDRESS_HIGHLIGHTING_DELAY
#define ADDRESS_HIGHLIGHTING_DELAY	150	
#endif // ADDRESS_HIGHLIGHTING_DELAY

//Uncomment this if needed
//#define REMOVE_HIGHLIGHT_ONHOVER

#define SEARCH_LENGTH_THRESHOLD 3
#define SUGGESTIONS_INITIAL_PRIORITY 6
#define MAX_ROWS_IN_DROPDOWN 9

#ifdef SUPPORT_START_BAR

/***********************************************************************************
**
**	PopupWindow - a WidgetWindow subclass for implementing the popup window
**
***********************************************************************************/
class PopupWindow;
class PopupToolbar : public OpToolbar
{
public:

	static OP_STATUS Construct(PopupToolbar** obj, PopupWindow* popup_window);
	PopupToolbar(PopupWindow* popup_window) : m_popup_window(popup_window) {}

	virtual void	OnAlignmentChanged();
	virtual void	OnSettingsChanged() { OnAlignmentChanged(); }

private:

	PopupWindow*	m_popup_window;
};

OP_STATUS PopupToolbar::Construct(PopupToolbar** obj, PopupWindow* popup_window)
{
	*obj = OP_NEW(PopupToolbar, (popup_window));
	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

class PopupWindow : public DesktopWidgetWindow, public SettingsListener
{
public:
	PopupWindow(OpAddressDropDown* address_dropdown) :
		init_status(OpStatus::OK),
		m_address_dropdown(address_dropdown)
	{
		OP_STATUS status;

		g_application->AddSettingsListener(this);

		status = Init(OpWindow::STYLE_CHILD, "Address Popup Window", m_address_dropdown->GetParentOpWindow(), NULL, OpWindow::EFFECT_TRANSPARENT);
		CHECK_STATUS(status);

		SetParentInputContext(m_address_dropdown);

		GetWidgetContainer()->SetParentDesktopWindow(m_address_dropdown->GetParentDesktopWindow());

		OpWidget* root = GetWidgetContainer()->GetRoot();

		root->GetBorderSkin()->SetImage("Start Popup Skin", "Edit Skin");

		status = PopupToolbar::Construct(&m_toolbar, this);
		CHECK_STATUS(status);
		root->AddChild(m_toolbar);

		m_toolbar->GetBorderSkin()->SetImage("Startbar Skin");
		m_toolbar->SetName("Start Toolbar");

		// This is a hack, but the only way to get the start bar to work properly.
		// The problem is that there is no parent-child relationship between the
		// popup window and the address bar. Therefore we force the toolbar into
		// the widget container of the address bar. And wowo everything works. adamm
		m_address_widget_collection = address_dropdown->GetNamedWidgetsCollection(); // this is the window one, so now GetWidgetByName works
		m_address_widget_collection->WidgetAdded(m_toolbar);

		GetWindow()->Raise(); // OpWindow* WidgetWindow::GetWindow()
	}

	void PlacePopupWindow()
	{
		if (m_toolbar->GetResultingAlignment() == OpBar::ALIGNMENT_OFF)
		{
			Show(FALSE);
			return;
		}

		OpToolbar* parent_toolbar = (OpToolbar*) m_address_dropdown->GetParent();

		// Crash fix when closing the appearance dialog
		if (!parent_toolbar)
			return;

		OpRect rect = m_address_dropdown->GetRect(TRUE);

		INT32 height = m_toolbar->GetHeightFromWidth(rect.width);

		if (parent_toolbar->GetAlignment() == OpBar::ALIGNMENT_BOTTOM)
			rect.y -= height - 1;
		else
			rect.y += rect.height - 1;

		rect.height = height;

#ifdef _MACINTOSH_
		rect.y += 3;
#endif

		Show(TRUE, &rect);

		rect.x = rect.y = 0;
		m_toolbar->SetRect(rect);
	}

	// This is a hack, but the only way to get the start bar to work properly.
	// The problem is that there is no parent-child relationship between the
	// popup window and the address bar. Therefore we force the toolbar into
	// the widget container of the address bar. And wowo everything works. adamm
	void OnClose(BOOL user_initiated)
	{
		m_address_widget_collection->WidgetRemoved(m_toolbar);
		DesktopWidgetWindow::OnClose(user_initiated);
	}

	virtual ~PopupWindow()
	{
		m_address_dropdown->m_popup_window = NULL;
	}

	virtual BOOL			IsInputContextAvailable(FOCUS_REASON reason) {return TRUE;}
	virtual void			OnSettingsChanged(DesktopSettings* settings)
	{
		GetWidgetContainer()->GetRoot()->GenerateOnSettingsChanged(settings);
	}
	OP_STATUS init_status;

private:

	OpAddressDropDown*			m_address_dropdown;
	OpNamedWidgetsCollection*	m_address_widget_collection;
	PopupToolbar*				m_toolbar;
};

void PopupToolbar::OnAlignmentChanged()
{
	if (IsOn())
		m_popup_window->PlacePopupWindow();
	else
		m_popup_window->Show(FALSE);
}

#endif // SUPPORT_START_BAR

/***********************************************************************************
**
**	OpPageCursor
**
***********************************************************************************/

#ifdef QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW

OpPageCursor::OpPageCursor()
	: m_listener(NULL), m_page(NULL)
{

}

OP_STATUS OpPageCursor::SetPage(OpPage& page)
{
	OpWindowCommander* old_commander = NULL;

	if(m_page)
	{
		old_commander = m_page->GetWindowCommander();
		m_page->RemoveListener(this);
	}

	OP_STATUS status = page.AddListener(this);

	m_page = &page;

	m_listener->OnPageWindowChanging(old_commander, page.GetWindowCommander());

	return status;
}

void OpPageCursor::OnPageDocumentIconAdded(OpWindowCommander* commander)
{
	m_listener->OnTabChanged();
}

void OpPageCursor::OnPageStartLoading(OpWindowCommander* commander)
{
	m_listener->OnTabChanged();
	m_listener->OnTabLoading();
}

void OpPageCursor::OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL was_stopped_by_user)
{
	m_listener->OnTabInfoChanged();
}

void OpPageCursor::OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url)
{
	m_listener->OnPageURLChanged(commander, url);
}

void OpPageCursor::OnPageTitleChanged(OpWindowCommander* commander, const uni_char* title)
{
	m_listener->OnTabChanged();
}

void OpPageCursor::OnPageSecurityModeChanged(OpWindowCommander* commander, OpDocumentListener::SecurityMode mode, BOOL inline_elt)
{
	m_listener->OnTabInfoChanged();
}

#ifdef TRUST_RATING
void OpPageCursor::OnPageTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating)
{
	m_listener->OnTabInfoChanged();
}
#endif // TRUST_RATING

OpWindowCommander* OpPageCursor::GetWindowCommander()
{
	return m_page ? m_page->GetWindowCommander() : NULL;
}

BOOL OpPageCursor::IsTargetWindow(DocumentDesktopWindow* document_window)
{
	if(document_window)
		return IsTargetWindow(document_window->GetWindowCommander());
	return FALSE;
}

BOOL OpPageCursor::IsTargetWindow(OpWindowCommander* windowcommander)
{
	return windowcommander && (GetWindowCommander() == windowcommander);
}

OP_STATUS OpPageCursor::GetSecurityInformation(OpString& text)
{
	return m_page->GetSecurityInformation(text);
}

const OpWidgetImage* OpPageCursor::GetWidgetImage()
{
	return m_page->GetDocumentIcon();
}

const OpWidgetImage* OpPageCursor::GetWidgetImage(OpWindowCommander* windowcommander)
{
	if(windowcommander)
	{
		OP_ASSERT(m_page->GetWindowCommander() == windowcommander);

		if(m_page->GetWindowCommander() == windowcommander)
			return m_page->GetDocumentIcon();
	}

	return NULL;
}

#endif // QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW

/***********************************************************************************
**
**	OpOldCursor
**
***********************************************************************************/

OpOldCursor::OpOldCursor() :
	m_current_target(NULL),
	m_workspace(NULL)
{
}

OpOldCursor::~OpOldCursor()
{
	UnHookCursor(FALSE);
}

void OpOldCursor::BroadcastTabInfoChanged()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		OpTabCursor::Listener* item = m_listeners.GetNext(iterator);
		item->OnTabInfoChanged();
	}
}

void OpOldCursor::BroadcastTabLoading()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		OpTabCursor::Listener* item = m_listeners.GetNext(iterator);
		item->OnTabLoading();
	}
}

void OpOldCursor::BroadcastTabChanged()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		OpTabCursor::Listener* item = m_listeners.GetNext(iterator);
		item->OnTabChanged();
	}
}

void OpOldCursor::BroadcastTabWindowChanging(DocumentDesktopWindow* old_target_window,
											 DocumentDesktopWindow* new_target_window)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		OpTabCursor::Listener* item = m_listeners.GetNext(iterator);
		item->OnTabWindowChanging(old_target_window, new_target_window);
	}
}

void OpOldCursor::BroadcastTabClosing(DocumentDesktopWindow* desktop_window)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		OpTabCursor::Listener* item = m_listeners.GetNext(iterator);
		item->OnTabClosing(desktop_window);
	}
}

void OpOldCursor::BroadcastTabURLChanged(DocumentDesktopWindow* document_window,
										 const OpStringC & url)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		OpTabCursor::Listener* item = m_listeners.GetNext(iterator);
		item->OnTabURLChanged(document_window, url);
	}
}

void OpOldCursor::HookUpCursor(OpInputContext * input_context)
{
	// Hook up to the current page :
	SetSpyInputContext(input_context, FALSE);
}

void OpOldCursor::UnHookCursor(BOOL send_change)
{
	// Unhook the current page :
	SetSpyInputContext(NULL, send_change);

	// Stop listening to the workspace :
	SetWorkspace(NULL);
}

BOOL OpOldCursor::IsTargetWindow(DocumentDesktopWindow* document_window)
{
	return document_window == m_current_target;
}

BOOL OpOldCursor::IsTargetWindow(OpWindowCommander* windowcommander)
{
	if(windowcommander && m_current_target)
		return m_current_target->GetWindowCommander() == windowcommander;

	return FALSE;
}

OpWindowCommander* OpOldCursor::GetWindowCommander()
{
	if(m_current_target)
		return m_current_target->GetWindowCommander();
	return NULL;
}

OP_STATUS OpOldCursor::GetSecurityInformation(OpString& text)
{
	if(m_current_target && 
	   m_current_target->GetBrowserView())
		return m_current_target->GetBrowserView()->GetCachedSecurityButtonText(text);

	return OpStatus::ERR;
}

const OpWidgetImage* OpOldCursor::GetWidgetImage()
{
	return m_current_target ? m_current_target->GetWidgetImage() : NULL;
}

// This horrible function is meant to die when OpPage wins over DocumentDesktopWindow
const OpWidgetImage* OpOldCursor::GetWidgetImage(OpWindowCommander* windowcommander)
{
	if(!windowcommander)
		return NULL;

	OpWindow* op_window = windowcommander->GetOpWindow();

	DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowFromOpWindow(op_window);

	if (desktop_window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		return NULL;

	DocumentDesktopWindow* document_window = static_cast<DocumentDesktopWindow*>(desktop_window);

	return document_window->GetWidgetImage();
}

OP_STATUS OpOldCursor::AddListener(OpTabCursor::Listener *listener)
{
	return m_listeners.Add(listener);
}

OP_STATUS OpOldCursor::RemoveListener(OpTabCursor::Listener *listener)
{
	return m_listeners.Remove(listener);
}

// Set the workspace to listen to
OP_STATUS OpOldCursor::SetWorkspace(OpWorkspace* workspace)
{
	OP_STATUS status = OpStatus::OK;

	if(m_workspace)
	{
		m_workspace->RemoveListener(this);

		OpVector<DesktopWindow> windows;
		RETURN_IF_ERROR(m_workspace->GetDesktopWindows(windows, OpTypedObject::WINDOW_TYPE_DOCUMENT));
		for (UINT32 i = 0; i < windows.GetCount(); i++)
		{
			DocumentDesktopWindow* document = static_cast<DocumentDesktopWindow*>(windows.Get(i));
			document->RemoveDocumentWindowListener(this);
		}
	}

	m_workspace = workspace;

	if(m_workspace)
	{
		status = m_workspace->AddListener(this);

		OpVector<DesktopWindow> windows;
		RETURN_IF_ERROR(m_workspace->GetDesktopWindows(windows, OpTypedObject::WINDOW_TYPE_DOCUMENT));

		for (UINT32 i = 0; i < windows.GetCount(); i++)
		{
			DocumentDesktopWindow* document = static_cast<DocumentDesktopWindow*>(windows.Get(i));
			document->AddDocumentWindowListener(this);
		}
	}

	return status;
}

// Implementing DocumentDesktopWindowSpy
void OpOldCursor::OnStartLoading(DocumentDesktopWindow* document_window)
{
	// We are only interested in start loading on the current target page
	if(IsTargetWindow(document_window))
		BroadcastTabLoading();
}

// Implementing DocumentDesktopWindowSpy
void OpOldCursor::OnLoadingFinished(DocumentDesktopWindow* document_window,
									OpLoadingListener::LoadingFinishStatus status,
									BOOL was_stopped_by_user)
{
	// We are only interested in loading finished on the current target page
	if(IsTargetWindow(document_window))
		BroadcastTabInfoChanged();
}

// Implementing DocumentDesktopWindowSpy
void OpOldCursor::OnTargetDocumentDesktopWindowChanged(DocumentDesktopWindow* target_window)
{
	DocumentDesktopWindow* current = m_current_target;
	m_current_target = target_window;
	SetTargetDocumentDesktopWindow(m_current_target, FALSE);
	BroadcastTabWindowChanging(current, target_window);
}

// Implementing DocumentDesktopWindowSpy
void OpOldCursor::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	// if the current window is closing we need to reset the m_current_target
	// (same as m_document_desktop_window is reset in DocumentDesktopWindowSpy::OnDesktopWindowClosing below)
	if (desktop_window == this->GetTargetDocumentDesktopWindow())
		m_current_target = NULL;

	DocumentDesktopWindowSpy::OnDesktopWindowClosing(desktop_window, user_initiated);

	if (desktop_window->GetType() == OpTypedObject::WINDOW_TYPE_DOCUMENT)
		BroadcastTabClosing((DocumentDesktopWindow*) desktop_window);
}

// Implementing DocumentDesktopWindowSpy
void OpOldCursor::OnUrlChanged(DocumentDesktopWindow* document_window,
							   const uni_char* url)
{
	// If the url changed for another window than the current one
	// the hashmap in OpAddressDropdown will be wrong.
	BroadcastTabURLChanged(document_window, url);
}

// Implementing DocumentDesktopWindowSpy
void OpOldCursor::OnDesktopWindowChanged(DesktopWindow* desktop_window)
{
	if (desktop_window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		return;

	OP_ASSERT(IsTargetWindow(static_cast<DocumentDesktopWindow*>(desktop_window)));

	if(IsTargetWindow(static_cast<DocumentDesktopWindow*>(desktop_window)))
		BroadcastTabChanged();
}

// Implementing DocumentDesktopWindowSpy
void OpOldCursor::OnSecurityModeChanged(DocumentDesktopWindow* document_window)
{
	// We are only interested in changes in security on the current target page
	if(IsTargetWindow(document_window))
		BroadcastTabInfoChanged();
}

// Implementing DocumentDesktopWindowSpy
void OpOldCursor::OnTrustRatingChanged(DocumentDesktopWindow* document_window)
{
	// We are only interested in changes in trust on the current target page
	if(IsTargetWindow(document_window))
		BroadcastTabInfoChanged();
}

void OpOldCursor::OnOnDemandPluginStateChange(DocumentDesktopWindow* document_window)
{
	if(IsTargetWindow(document_window))
		BroadcastTabInfoChanged();
}

// Implementing OpWorkspace::WorkspaceListener
void OpOldCursor::OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	if(desktop_window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		return;

	DocumentDesktopWindow* document = static_cast<DocumentDesktopWindow*>(desktop_window);

	RETURN_VOID_IF_ERROR(document->AddDocumentWindowListener(this));
}

// Implementing OpWorkspace::WorkspaceListener
void OpOldCursor::OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	if(desktop_window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		return;

	DocumentDesktopWindow* document = static_cast<DocumentDesktopWindow*>(desktop_window);

	document->RemoveDocumentWindowListener(this);
}

// Implementing OpWorkspace::WorkspaceListener
void OpOldCursor::OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate)
{
	if(!activate)
		return;

	// DocumentWindowSpy will take care of switching between document windows, 
	// but when when one or both of the windows are not document windows we
	// won't get callback from DocumentWindowSpy and need to handle it here
	if(desktop_window->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		OnTargetDocumentDesktopWindowChanged(NULL);
	else if (!m_current_target || m_current_target->GetType() != OpTypedObject::WINDOW_TYPE_DOCUMENT)
		OnTargetDocumentDesktopWindowChanged(static_cast<DocumentDesktopWindow*>(desktop_window));
}

// Implementing OpWorkspace::WorkspaceListener
void OpOldCursor::OnWorkspaceDeleted(OpWorkspace* workspace)
{
	// Stop listening to the workspace :
	SetWorkspace(NULL);
}

/***********************************************************************************
**
**	OpAddressDropDown
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpAddressDropDown)

OpAddressDropDown::OpAddressDropDown() :
	OpTreeViewDropdown()
	, m_host_resolver(NULL)
	, m_progress_field(NULL)
	, m_url_fav_menu(NULL)
	, m_bookmark_status_label(NULL)
	, m_rss_button(NULL)
	, m_ondemand_button(NULL)
	, m_bookmark_button(NULL)
	, m_protocol_button(NULL)
#ifdef SUPPORT_START_BAR
	, m_popup_window(NULL)
#endif // SUPPORT_START_BAR
	, m_url_page_mismatch(FALSE)
	, m_show_all_item(NULL)
	, m_tab_cursor(NULL)
	, m_open_page_in_tab(TRUE)
	, m_in_addressbar(FALSE)
	, m_delayed_progress_visibility(FALSE)
	, m_delayed_layout_in_progress(FALSE)
	, m_addressbar_overlay_ctl(NULL)
	, m_favorites_overlay_ctl(NULL)	
	, m_show_search_suggestions(TRUE)
	, m_show_speed_dial_pages(true)
	, m_process_search_results(FALSE)
	, m_lock_current_text(0)
	, m_is_quick_completed(false)
	, m_tab_mode(FALSE)
	, m_search_suggest_in_progress(FALSE)
	, m_shall_show_addr_bar_badge(TRUE)
	, m_search_suggest(NULL)
	, m_mh(NULL)
	, m_query_error_counter(0)
#if defined(_X11_SELECTION_POLICY_)
	, m_select_start(0)
	, m_select_end(0)
	, m_caret_offset(-1)
#endif
	, m_use_display_url(FALSE)
	, m_item_to_select(NULL)
	, m_url_fav_composite_listener(NULL)
	, m_show_favorites_button(false)
	, m_regex(NULL)
	, m_search_for_item(NULL)
	, m_selected_item(NULL)
	, m_ignore_edit_focus(false)
	, m_block_focus_timer(NULL)
	, m_notification_timer(NULL)
{
	RETURN_VOID_IF_ERROR(init_status);

	OP_STATUS status = OpStatus::OK;

	m_tab_cursor = OP_NEW(OpOldCursor, ());
	if (!m_tab_cursor)
		CHECK_STATUS(OpStatus::ERR_NO_MEMORY);

	status = SetTabMode(TRUE);
	CHECK_STATUS(status);

	m_tab_cursor->HookUpCursor(this);

	m_text_content.SetOwner(this);

	status = g_application->AddAddressBar(this);
	CHECK_STATUS(status);

	TRAPD(err,g_pcui->RegisterListenerL(this));

	//--------------------------------------------------
	// Security :
	//--------------------------------------------------

	// For updating the security ui :
	SetUpdateNeededWhen(UPDATE_NEEDED_WHEN_VISIBLE);

	//--------------------------------------------------
	// Widget settings :
	//--------------------------------------------------

	SetAlwaysInvoke(TRUE);

	SetGrowValue(2);

	SetListener(this);

	ShowButton(FALSE);

	GetBorderSkin()->SetImage("Dropdown Addressfield Skin", "Dropdown Skin");

	// Avoid streched favicons :
	SetRestrictImageSize(TRUE);

	//--------------------------------------------------
	// Initialize the ui widgets :
	//--------------------------------------------------

	OpString button_text;

	// Edit field
	status = InitializeEditField();
	CHECK_STATUS(status);

	// Progress field
	status = InitializeProgressField();
	CHECK_STATUS(status);

	// Protocol button
	status = InitializeProtocolButton();
	CHECK_STATUS(status);

	// Plugin On-demand activation button
	g_languageManager->GetString(Str::S_ON_DEMAND_PLUGINS_ACTIVATE_TOOLTIP, button_text);
	status = InitializeButton(&m_ondemand_button, "Activate on-demand plugins,,,,\"Ondemand\"", button_text.CStr());

	m_ondemand_button->SetName(WIDGET_NAME_ADDRESSFIELD_BUTTON_ONDEMAND);
	CHECK_STATUS(status);

	// RSS button
	g_languageManager->GetString(Str::D_FEED_LIST, button_text);
	status = InitializeButton(&m_rss_button, 
							  "Go to link element, \"application/atom+xml\",,,\"RSS\" | Go to link element, \"application/rss+xml\",,,\"RSS\"",
							  button_text.CStr());

	m_rss_button->SetName(WIDGET_NAME_ADDRESSFIELD_BUTTON_RSS);
	CHECK_STATUS(status);

	// Note button
	status = InitializeButton(&m_bookmark_button, "Highlight note,,,,\"Note\"", 0);
	CHECK_STATUS(status);

	// Dropdown button
	status = InitializeButton(&m_dropdown_button, "Show dropdown,,,,\"Down arrow\"", 0);

	m_dropdown_button->SetName(WIDGET_NAME_ADDRESSFIELD_BUTTON_DROPDOWN);
	CHECK_STATUS(status);

	m_mh = OP_NEW(MessageHandler, (NULL));
	if (!m_mh)
		CHECK_STATUS(OpStatus::ERR_NO_MEMORY);

	m_search_suggest = OP_NEW(SearchSuggest, (this));
	if (!m_search_suggest)
		CHECK_STATUS(OpStatus::ERR_NO_MEMORY);

	// Register for callbacks :

	m_url_fav_composite_listener = OP_NEW(FavoritesCompositeListener, (this));
	if (!m_url_fav_composite_listener)
		CHECK_STATUS(OpStatus::ERR_NO_MEMORY);

	// use global for content search
	g_main_message_handler->SetCallBack(this, MSG_VPS_SEARCH_RESULT, reinterpret_cast<MH_PARAM_1>(static_cast<MessageObject *>(this)));

	// local for other stuff
	m_mh->SetCallBack(this, MSG_QUICK_REMOVE_ADDRESSFIELD_HISTORY_ITEM, 0);
	m_mh->SetCallBack(this, MSG_QUICK_START_SEARCH_SUGGESTIONS, 0);

	// set up the delayed trigger
	m_delayed_trigger.SetDelayedTriggerListener(this);
	m_delayed_trigger.SetTriggerDelay(0, 500);

	m_highlight_delayed_trigger.SetDelayedTriggerListener(this);
	m_highlight_delayed_trigger.SetTriggerDelay(ADDRESS_HIGHLIGHTING_DELAY, ADDRESS_HIGHLIGHTING_DELAY);
}


OpAddressDropDown::~OpAddressDropDown()
{
	OP_DELETE(m_block_focus_timer);
	OP_DELETE(m_notification_timer);
	OP_DELETE(m_mh);
	OP_DELETE(m_search_suggest);
	OP_DELETE(m_host_resolver);
	OP_DELETE(m_tab_cursor);
	OP_DELETE(m_show_all_item);
	OP_DELETE(m_url_fav_composite_listener);
	OP_DELETE(m_regex);
	OP_DELETE(m_search_for_item);
	DeleteItems(m_search_suggestions_items);
	DeleteItems(m_items_to_delete);
}

/***********************************************************************************
**
**  Model converters
**
***********************************************************************************/

inline void OpAddressDropDown::SetModel(HistoryAutocompleteModel * tree_model, BOOL items_deletable)
{
	OpTreeViewDropdown::SetModel(tree_model, items_deletable);
	SetTreeViewName( WIDGET_NAME_ADDRESSFIELD_TREVIEW );
}

HistoryAutocompleteModel * OpAddressDropDown::GetModel()
{
	return static_cast<HistoryAutocompleteModel*>(OpTreeViewDropdown::GetModel());
}

HistoryAutocompleteModelItem * OpAddressDropDown::GetSelectedItem(int * position)
{
	return static_cast<HistoryAutocompleteModelItem*>(OpTreeViewDropdown::GetSelectedItem(position));
}

OP_STATUS OpAddressDropDown::SetTabMode(BOOL mode)
{
	OP_STATUS rc = OpStatus::OK;
	if (m_tab_mode != mode)
	{
		rc = mode ? m_tab_cursor->AddListener(this) : m_tab_cursor->RemoveListener(this);
		if (OpStatus::IsSuccess(rc))
			m_tab_mode = mode;
	}
	return rc;
}


#ifdef QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW

OP_STATUS OpAddressDropDown::SetPage(OpPage& page)
{
	if(m_tab_cursor->GetCursorType() == OpTabCursor::OLD_TYPE)
	{
		OpAutoPtr<OpTabCursor> tab_cursor(OP_NEW(OpPageCursor, ()));

		if(!tab_cursor.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(tab_cursor->AddListener(this));

		OP_DELETE(m_tab_cursor);
		m_tab_cursor = tab_cursor.release();
	}

	return m_tab_cursor->SetPage(page);
}
#endif // QUICK_TOOLKIT_CAP_HAS_PAGE_VIEW

OP_STATUS OpAddressDropDown::InitializeEditField()
{
	OpString ghost_text;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_GO_TO_WEB_ADDRESS_OR_SEARCH_HERE, ghost_text));

	RETURN_VALUE_IF_NULL(edit,OpStatus::ERR);

	edit->SetForceTextLTR(TRUE);
	edit->SetShowGhostTextWhenFocused(TRUE);
	edit->SetGhostTextJusifyWhenFocused(JUSTIFY_LEFT);
	RETURN_IF_ERROR(edit->SetGhostText(ghost_text.CStr()));

#ifndef _MACINTOSH_
	edit->SetDoubleClickSelectsAll(TRUE);
#endif // ! _MACINTOSH_
	edit->SetReplaceAllOnDrop(TRUE);
	edit->SetHasCssBackground(TRUE);
	edit->GetBorderSkin()->SetImage("Dropdown Addressfield Edit Skin", "Edit Skin");

	PageBasedAutocompleteItem::SetFontWidth(WidgetUtils::GetFontWidth(edit->font_info));
	UINT32 color;
	if (OpStatus::IsSuccess(g_skin_manager->GetTextColor("Ghost Text Foreground Skin", &color, 0)))
		edit->SetGhostTextFGColorWhenFocused(color);
	
	return OpStatus::OK;
}

/***********************************************************************************
**
**  InitializeProgressField
**
***********************************************************************************/

OP_STATUS OpAddressDropDown::InitializeProgressField()
{
	OP_STATUS status = OpProgressField::Construct(&m_progress_field, OpProgressField::PROGRESS_TYPE_GENERAL);

	if(OpStatus::IsSuccess(status))
	{
		AddChild(m_progress_field, FALSE, TRUE);
		m_progress_field->SetVisibility(FALSE);
		m_progress_field->GetForegroundSkin()->SetImage("Addressbar Progress Full Skin");
		m_progress_field->GetBorderSkin()->SetImage("Addressbar Progress Skin");
	}

	return status;
}

OP_STATUS OpAddressDropDown::InitializeBookmarkMenu()
{
	OpString button_text;
	OpAutoPtr<URLFavMenuButton> fav_btn(OP_NEW(URLFavMenuButton, ()));
	RETURN_OOM_IF_NULL(fav_btn.get());
	OpAutoPtr<OpInputAction> action;
	if (g_desktop_op_system_info->SupportsContentSharing())
	{
		RETURN_IF_ERROR(OpButton::Construct(&m_bookmark_status_label, OpButton::TYPE_TOOLBAR, OpButton::STYLE_TEXT));
		m_bookmark_status_label->SetName(WIDGET_NAME_ADDRESSFIELD_FAV_NOTIFICAITON);
		m_bookmark_status_label->SetVisibility(FALSE);
		AddChild(m_bookmark_status_label, FALSE, TRUE);

		m_notification_timer = OP_NEW(OpTimer, ());
		RETURN_OOM_IF_NULL(m_notification_timer);

		RETURN_IF_ERROR(fav_btn->Init(NULL,"SharePage"));
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU));
		RETURN_OOM_IF_NULL(action.get());
		action->SetActionDataString(UNI_L("Page Sharing Menu"));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_URL_SHARE_MENU_TT, button_text));
	}
	else
	{
		RETURN_IF_ERROR(fav_btn->Init(NULL, "NonBookmarked URL"));
		action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN));
		RETURN_OOM_IF_NULL(action.get());
		action->SetActionDataString(UNI_L("URL Fav Menu"));
		RETURN_IF_ERROR(g_languageManager->GetString(Str::S_URL_BOOKMARK_MENU_TT, button_text));
	}

	fav_btn->SetAction(action.release());

	RETURN_IF_ERROR(fav_btn->SetText(button_text.CStr()));
	fav_btn->SetName(WIDGET_NAME_ADDRESSFIELD_FAV_MENU_BUTTON);
	fav_btn->SetVisibility(FALSE);

	AddChild(fav_btn.get(), FALSE, TRUE);

	m_url_fav_menu = fav_btn.release();

	return OpStatus::OK;
}

OP_STATUS OpAddressDropDown::InitializeProtocolButton()
{
	m_protocol_button = OP_NEW(OpProtocolButton, ());
	RETURN_OOM_IF_NULL(m_protocol_button);
	RETURN_IF_ERROR(m_protocol_button->Init());

	AddChild(m_protocol_button, FALSE, TRUE);
	m_protocol_button->SetVisibility(FALSE);

	return OpStatus::OK;
}

OpWidget* OpAddressDropDown::GetWidgetByName(const char* name)
{
	OpWidget* widget = OpTreeViewDropdown::GetWidgetByName(name);

	if (widget)
		return widget;

#ifdef SUPPORT_START_BAR
	if (m_popup_window)
		return m_popup_window->GetWidgetContainer()->GetRoot()->GetWidgetByName(name);
#endif // SUPPORT_START_BAR

	return NULL;
}

#ifdef SUPPORT_START_BAR

void OpAddressDropDown::CreatePopupWindow()
{
	OpWidget *parent = GetParent();
	if (!parent || parent->GetType() != WIDGET_TYPE_TOOLBAR)
		return;

	OpToolbar* parent_toolbar = (OpToolbar*)parent;

	if (parent_toolbar->IsLocked() ||
		parent_toolbar->IsNamed("Start toolbar") ||
		g_application->IsCustomizingToolbars())
//		GetParentDesktopWindow() == (DesktopWindow*) g_application->GetCustomizeToolbarsDialog())
		return;

	if (!m_popup_window)
	{
		m_popup_window = OP_NEW(PopupWindow, (this));
		if (m_popup_window && OpStatus::IsError(m_popup_window->init_status))
		{
			OP_DELETE(m_popup_window);
			m_popup_window = NULL;
		}
	}
}

void OpAddressDropDown::PlacePopupWindow()
{
	if (!m_popup_window)
		return;

	m_popup_window->PlacePopupWindow();
}

void OpAddressDropDown::ShowPopupWindow(BOOL show)
{
	if (!m_popup_window || /* Bug 170735 & 248675 & friends*/ g_application->GetMenuHandler()->IsShowingMenu())
		return;

	if (show)
		PlacePopupWindow();
	else
		m_popup_window->Show(FALSE);
}

void OpAddressDropDown::DestroyPopupWindow(BOOL close_immediately)
{
	if (m_popup_window)
	{
		m_popup_window->SetParentInputContext(NULL);
		m_popup_window->Close(close_immediately);
	}

	//	Keep the m_popup_window here - it will be NULLed by ~PopupWindow()
	//	This is to ensure that a delayed closing (due to focus removal) can still be overridden
	//	with the necessary immediate closing when this OpAddressDropDown is deleted.
}
#endif // SUPPORT_START_BAR

OP_STATUS OpAddressDropDown::InitializeButton(OpButton ** button,
											  const char * action_string,
											  const uni_char * button_text)
{
	*button = new OpAddressFieldButton();

	if(*button)
	{
		OpInputAction* widget_action;
		OP_STATUS status = OpInputAction::CreateInputActionsFromString(action_string, widget_action);

		if (OpStatus::IsSuccess(status))
			(*button)->SetAction(widget_action);

		(*button)->SetButtonStyle(OpButton::STYLE_IMAGE);
		(*button)->SetFixedImage(FALSE);

		if(button_text)
			(*button)->SetText(button_text);

		AddChild(*button, FALSE, TRUE);
		(*button)->SetVisibility(FALSE);

		return status;
	}

	return OpStatus::ERR_NO_MEMORY;
}

void OpAddressDropDown::OnDeleted()
{
	OP_DELETE(m_url_fav_composite_listener);
	m_url_fav_composite_listener = NULL;

	m_mh->UnsetCallBacks(this);
	g_main_message_handler->UnsetCallBacks(this);

	if (DesktopHistoryModel::GetInstance())
		DesktopHistoryModel::GetInstance()->RemoveDesktopHistoryModelListener(this);
#ifdef SUPPORT_START_BAR
	DestroyPopupWindow(TRUE);
#endif // SUPPORT_START_BAR

	if (m_url_fav_menu)
	{
		m_url_fav_menu->Remove();
		OP_DELETE(m_url_fav_menu);
		m_url_fav_menu = NULL;
	}

	if (m_addressbar_overlay_ctl)
	{
		m_addressbar_overlay_ctl->SetAnchorWidget(NULL);
		m_addressbar_overlay_ctl->SetListener(NULL);
		CloseOverlayDialog();
	}

	OpTreeViewDropdown::OnDeleted();
	m_tab_cursor->RemoveListener(this);
	m_tab_cursor->UnHookCursor(FALSE);
	g_application->RemoveAddressBar(this);
	TRAPD(err,g_pcui->UnregisterListener(this));
}

void OpAddressDropDown::OnKeyboardInputGained(OpInputContext* new_input_context,
											  OpInputContext* old_input_context,
											  FOCUS_REASON reason)
{
	if (new_input_context == m_url_fav_menu
			|| (m_favorites_overlay_ctl && m_favorites_overlay_ctl->IsDialogVisible() && new_input_context != edit && new_input_context != this))
		return;

	OpTreeViewDropdown::OnKeyboardInputGained(new_input_context, old_input_context, reason);

	UpdateButtons(FALSE);
#ifdef SUPPORT_START_BAR
	ShowPopupWindow(TRUE);
#endif // SUPPORT_START_BAR

	// _X11_SELECTION_POLICY_ in this function: Restore selection (without copying to the X11 
	// selection buffer) and cursor position as it was when widget lost focus. This allows the user
	// midclick paste data into the edit field.

#if defined(_X11_SELECTION_POLICY_)
	int pos = edit->GetCaretOffset();
#endif

	if (GetFocused() == edit)
	{
		m_protocol_button->HideText();
	}

	// The edit get focused, show the full address(including protocol) if it wasn't edited
	if (!m_url_page_mismatch)
	{
		RestoreOriginalAddress();

		// If protocol was hidden we need to offset the selecting start
		if (!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullURL))
			edit->selecting_start += m_address.GetProtocol().Length();

#if defined(_X11_SELECTION_POLICY_)
		if (reason == FOCUS_REASON_MOUSE || reason == FOCUS_REASON_ACTIVATE)
		{
			if (!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullURL))
				pos += m_address.GetProtocol().Length();

			if (m_select_start != m_select_end)
				edit->SetSelection(m_select_start, m_select_end);

			if (m_caret_offset != -1 && reason == FOCUS_REASON_ACTIVATE)
				pos = m_caret_offset;
			m_caret_offset = -1;
		}
#else
		edit->SelectAll();
#endif
	}

#if defined(_X11_SELECTION_POLICY_)
	if (reason == FOCUS_REASON_MOUSE || reason == FOCUS_REASON_ACTIVATE)
	{
		if (reason == FOCUS_REASON_MOUSE && vis_dev->GetView()->GetMouseButton(MOUSE_BUTTON_1)) 
		{
			edit->is_selecting = 1;
			edit->SetCaretOffset(pos, FALSE);
			edit->string.sel_start = edit->string.sel_stop = edit->GetCaretOffset();
		}
		else
			edit->SetCaretOffset(pos);
	}
	else
	{
		// Select all text without copying to clipboard buffer
		OpString text;
		GetText(text);
		edit->SetSelection(0, text.Length());
		edit->SetCaretOffset(0);
	}
	m_select_start = m_select_end = 0;
#endif

	UpdateAddressFieldHighlight(FALSE, TRUE);
	
}

void OpAddressDropDown::OnKeyboardInputLost(OpInputContext* new_input_context,
											OpInputContext* old_input_context,
											FOCUS_REASON reason)
{
	if (m_favorites_overlay_ctl && m_favorites_overlay_ctl->IsDialogVisible())
		return;

	if (new_input_context != this &&
		!IsParentInputContextOf(new_input_context) &&
		!g_application->IsCustomizingToolbars()    &&
		!g_drag_manager->IsDragging())
	{
		UpdateButtons(FALSE);

		// Delay closing of the popup so that all functions in the call chain
		// referencing it can finish first (bug #185028)
#ifdef SUPPORT_START_BAR
		if(new_input_context != this)
		{
			ShowPopupWindow(FALSE);
		}
#endif // SUPPORT_START_BAR
	}


	// _X11_SELECTION_POLICY_ in this function: Save selection region and keep it visible 
	// so that 1) a user knows what can be midclick pasted elsewhere by looking at the text 
	// and 2) the selection can be properly restored when the widget gains focus

#if defined(_X11_SELECTION_POLICY_)
	SELECTION_DIRECTION direction;
	edit->GetSelection(m_select_start, m_select_end, direction);
	int caret_offset = edit->GetCaretOffset();
#endif

	// When edit is losing focus we want to show the short address but only if the address wasn't edited
	if (GetFocused() != edit && GetWindowCommander())
	{
		m_protocol_button->ShowText();

		OpString text;
		GetText(text);

		const uni_char* current_url = GetWindowCommander()->IsLoading() ?
			GetWindowCommander()->GetLoadingURL(FALSE) :
			GetWindowCommander()->GetCurrentURL(FALSE);

		if (text.Compare(current_url) == 0)
		{
			/* The family of WindowCommander's GetFooURL() methods use the same
			   buffer for strings they return meaning that they will overwrite
			   the string that was obtained in the previous call, fooling
			   comparisons that are done throughout this code. We need to copy
			   the string. */
			OpString url;
			if (OpStatus::IsSuccess(url.Set(current_url)))
			{
				SetText(url.CStr());

#if defined(_X11_SELECTION_POLICY_)
				m_caret_offset = caret_offset; // Save offset as it was before SetText
				if (!g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullURL))
				{
					if (m_select_start != m_select_end)
					{
						int l = m_address.GetProtocol().Length();
						edit->SetSelection(m_select_start-l, m_select_end-l);
					}
				}
#endif
			}
		}
	}

	UpdateAddressFieldHighlight();
	// Caret should be reset to 0 when focus lost, this is standard
	// behavior for other browsers too.
#if !defined(_X11_SELECTION_POLICY_)
	if (reason != FOCUS_REASON_RELEASE)
		edit->SetCaretOffset(0);
#endif

	OpTreeViewDropdown::OnKeyboardInputLost(new_input_context, old_input_context, reason);
}

BOOL OpAddressDropDown::BelongsToADocumentDesktopWindow()
{
	// Try to find a DocumentDesktopWindow on our input context path
	return GetParentDesktopWindowType() == OpTypedObject::WINDOW_TYPE_DOCUMENT;
}

void OpAddressDropDown::OnAdded()
{
	OpWidget* grand_parent = GetParent()->GetParent();

	if (grand_parent->GetType() == PANEL_TYPE_START)
	{
		m_tab_cursor->UnHookCursor(TRUE);
		GetForegroundSkin()->SetImage(NULL);
	}

	m_in_addressbar = GetParentDesktopWindowType() == WINDOW_TYPE_DOCUMENT || GetParentDesktopWindowType() == WINDOW_TYPE_BROWSER;
	if (m_in_addressbar && m_protocol_button)
		m_protocol_button->SetVisibility(TRUE);
	
	OpInputAction *act = OP_NEW(OpInputAction, (OpInputAction::ACTION_GO_TO_TYPED_ADDRESS));

	if (act)
	{
		SetAction(act);
	}

	OpTreeViewDropdown::OnAdded();

#ifdef SUPPORT_START_BAR
	CreatePopupWindow();
#endif // SUPPORT_START_BAR
}

void OpAddressDropDown::ForwardWorkspace()
{
	if(m_tab_cursor->HasWorkspace() || BelongsToADocumentDesktopWindow())
		return;

	// We are a tab on a status or main bar, and we have not yet started listening
	// to the workspace. Make sure we listen now.
	m_tab_cursor->SetWorkspace(GetWorkspace());
}

void OpAddressDropDown::OnTrigger(OpDelayedTrigger* trigger)
{
	if (trigger == &m_delayed_trigger)
	{
		DesktopWindow *window = GetParentDesktopWindow();
		if(window && window->GetType() == WINDOW_TYPE_DOCUMENT && ((DocumentDesktopWindow *)window)->IsLoading())
		{
			return;
		}
		m_progress_field->SetVisibility(m_delayed_progress_visibility);
		Relayout();
		m_delayed_layout_in_progress = FALSE;
	}
	else if (trigger == &m_highlight_delayed_trigger)
	{
		UpdateAddressFieldHighlight(FALSE, TRUE);
	}
}

void OpAddressDropDown::UpdateButtons(BOOL new_url)
{
	BOOL progress_visibility			= FALSE;
	BOOL security_visibility			= FALSE;
	BOOL security_button_changed		= FALSE;
	BOOL on_demand_visibility			= FALSE;
	BOOL rss_visibility					= FALSE;
	BOOL widget_visibility				= FALSE;
	BOOL bookmark_visibility			= FALSE;
	BOOL protocol_button_visibility		= FALSE;
	BOOL url_fav_menu_visibility		= FALSE;

	m_dropdown_button->SetVisibility(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowDropdownButtonInAddressfield));

	OpWindowCommander* win_comm = m_tab_cursor->GetWindowCommander();
	if (!win_comm)
		return;

	if(new_url)
	{
		if (m_url_fav_menu)
			m_url_fav_menu->SetVisibility(FALSE);
	}

	// Update status of the protocol button
	if (m_in_addressbar && new_url)
	{
		UpdateProtocolStatus();
		protocol_button_visibility = GetProtocolStatus() != OpProtocolButton::NotSet;
	}

	// Check the type of progress : 
	if (win_comm->IsLoading()) // Only display progress when you should
	{
		progress_visibility = g_pcui->GetIntegerPref(PrefsCollectionUI::ProgressPopup) == OpProgressField::PROGRESS_TYPE_TOTAL;
	}
	else
	{
		// URL Bookmark Menu
		if (m_show_favorites_button)
		{
			URL url = WindowCommanderProxy::GetMovedToURL(win_comm);
			url_fav_menu_visibility = !url.IsEmpty();
			if (url_fav_menu_visibility)
			{
				const uni_char* url = m_tab_cursor->GetWindowCommander()->GetCurrentURL(FALSE);
				if (url)
				{
					if (!DocumentView::IsUrlRestrictedForViewFlag(url, DocumentView::ALLOW_ADD_TO_SPEED_DIAL) &&
						!DocumentView::IsUrlRestrictedForViewFlag(url, DocumentView::ALLOW_ADD_TO_BOOKMARK))
						url_fav_menu_visibility = FALSE;
				}
			}
		}

		// Note button
		HotlistModel* model = g_hotlist_manager->GetNotesModel();
		HotlistModelItem* item = model->GetByURL(WindowCommanderProxy::GetMovedToURL(win_comm));
		bookmark_visibility = (item != NULL && !item->GetIsInsideTrashFolder());

		// Plugin On-demand button :
		OpInputAction* current_action = m_ondemand_button->GetAction();
		while (current_action && !on_demand_visibility)
		{
			on_demand_visibility = (g_input_manager->GetActionState(current_action, this) &
				OpInputAction::STATE_ENABLED) != 0;
			current_action = current_action->GetNextInputAction();
		}

		// RSS button
		current_action = m_rss_button->GetAction();
		while (current_action && !rss_visibility)
		{
			rss_visibility = (g_input_manager->GetActionState(current_action, this) &
				OpInputAction::STATE_ENABLED) != 0;
			current_action = current_action->GetNextInputAction();
		}
	}

	// Turn them off if the user is using the edit field :
	if (GetFocused() == OpDropDown::edit)
	{
		security_visibility = FALSE;
		on_demand_visibility= FALSE;
		rss_visibility      = FALSE;
		widget_visibility   = FALSE;
		bookmark_visibility = FALSE;
		url_fav_menu_visibility = FALSE;
	}

	BOOL relayout = FALSE;

	if (m_progress_field->IsVisible() != progress_visibility)
	{
		if (ShallMaskUrlFromDisplay(m_address.GetFullAddress()))
			progress_visibility = FALSE;

		if(progress_visibility) // if it's not visible and needs to be, make it visible right away
			m_progress_field->SetVisibility(progress_visibility);
		else
		{
			// If it needs to be hidden, delay hiding it a bit just in case we're going to show it again right again
			m_delayed_progress_visibility = progress_visibility;
			m_delayed_layout_in_progress = TRUE;
		}
	}

	if (m_url_fav_menu && m_url_fav_menu->IsVisible() != url_fav_menu_visibility)
	{
		m_url_fav_menu->SetVisibility(url_fav_menu_visibility);
		relayout = TRUE;
	}

	if (security_button_changed && security_visibility)
	{
		relayout = TRUE;
	}

	if (m_ondemand_button->IsVisible() != on_demand_visibility)
	{
		m_ondemand_button->SetVisibility(on_demand_visibility);
		relayout = TRUE;
	}

	if (m_rss_button->IsVisible() != rss_visibility)
	{
		m_rss_button->SetVisibility(rss_visibility);
		relayout = TRUE;
	}

	if (m_bookmark_button->IsVisible() != bookmark_visibility)
	{
		m_bookmark_button->SetVisibility(bookmark_visibility);
		relayout = TRUE;
	}

	if (m_protocol_button->IsVisible() != protocol_button_visibility)
	{
		m_protocol_button->SetVisibility(protocol_button_visibility);
		relayout = TRUE;
	}

#ifdef EXTENSION_SUPPORT
	SetDead(m_protocol_button->GetStatus() == OpProtocolButton::Widget);
#endif
	
	// Update url fav menu icon
	if (!win_comm->IsLoading())
		UpdateFavoritesFG(!relayout && !m_delayed_layout_in_progress);

	if (m_delayed_layout_in_progress)
		m_delayed_trigger.InvokeTrigger(OpDelayedTrigger::INVOKE_DELAY);
	else if (relayout)// relayout will happen in the OpTrigger anyway
		Relayout();
}

void OpAddressDropDown::GetPadding(INT32* left,
								   INT32* top,
								   INT32* right,
								   INT32* bottom)
{
	OpTreeViewDropdown::GetPadding(left, top, right, bottom);

	// Left aligned buttons
	if (m_protocol_button->IsVisible())
		*left += m_protocol_button->GetTargetWidth();

	// Right aligned buttons

	OpWidget* buttons[] = {
		m_progress_field,
		m_url_fav_menu,
		m_rss_button,
		m_ondemand_button,
		m_bookmark_button,
		m_dropdown_button,
		m_bookmark_status_label,
	};
	for (unsigned i = 0; i < ARRAY_SIZE(buttons); ++i)
		if (buttons[i] != NULL && buttons[i]->IsVisible())
			*right += buttons[i]->GetWidth();
}


void OpAddressDropDown::OnLayout()
{
	OpRect rect = GetBounds();

	INT32 left, top, right, bottom;

	OpTreeViewDropdown::GetPadding(&left, &top, &right, &bottom);

	rect.x += left;
	rect.y += top;
	rect.width -= left + right;
	rect.height -= top + bottom;

	rect.width -= GetRTL() ? GetInfo()->GetDropdownLeftButtonWidth(this) : GetInfo()->GetDropdownButtonWidth(this);
	rect.x += rect.width;

	INT32 height, width;
	
	width = rect.width;
	
	// Layout right- (left- for RTL) aligned buttons

	if (m_dropdown_button->IsVisible())
	{
		m_dropdown_button->GetRequiredSize(rect.width, height);
		rect.x -= rect.width;
		SetChildRect(m_dropdown_button, rect);
	}

	rect.width = width;

	if (m_progress_field->IsVisible())
	{
		m_progress_field->GetRequiredSize(rect.width, height);
		rect.x -= rect.width;
		SetChildRect(m_progress_field, rect);
	}

	height = rect.height;

	if (m_url_fav_menu && m_url_fav_menu->IsVisible())
	{
		m_url_fav_menu->GetRequiredSize(rect.width, height);
		rect.x -= rect.width;
		SetChildRect(m_url_fav_menu, rect);
	}

	if (m_rss_button->IsVisible())
	{
		m_rss_button->GetRequiredSize(rect.width, height);
		rect.x -= rect.width;
		SetChildRect(m_rss_button, rect);
	}

	if (m_ondemand_button->IsVisible())
	{
		m_ondemand_button->GetRequiredSize(rect.width, height);
		rect.x -= rect.width;
		SetChildRect(m_ondemand_button, rect);
	}


	if (m_bookmark_button->IsVisible())
	{
		m_bookmark_button->GetRequiredSize(rect.width, height);
		rect.x -= rect.width;
		SetChildRect(m_bookmark_button, rect);
	}

	// status notification text should be the leftmost element of righ alignment buttons
	if (m_bookmark_status_label)
	{
		m_bookmark_status_label->GetRequiredSize(rect.width, height);
		rect.x -= rect.width;
		SetChildRect(m_bookmark_status_label, rect);
	}

	// Layout left aligned buttons, but leave the button alone when it's animating
	m_protocol_button->SetVisibility(m_in_addressbar); // only want this button in real address bar

	// Either Window Document Icon or the Badge should be shown
	if (!m_protocol_button->IsVisible() && m_shall_show_addr_bar_badge)
		GetForegroundSkin()->SetImage("Window Document Icon");
	else
		GetForegroundSkin()->SetImage(NULL);

	if (m_protocol_button->IsVisible() && !m_protocol_button->GetAnimation())
	{
		UpdateProtocolStatus();

		// width, don't resize the width of the button, it'll take care of itself
		INT32 dummy;
		m_protocol_button->GetRequiredSize(rect.width, dummy);

		// height and margins
		INT32 margin_left, margin_right, margin_bottom, margin_top;
		m_protocol_button->GetBorderSkin()->GetMargin(&margin_left, &margin_top, &margin_right, &margin_bottom);
		rect.y += margin_top;
		rect.height -= margin_top + margin_bottom;
		rect.x = left + margin_left;

		SetChildRect(m_protocol_button, rect);
	}

	OpTreeViewDropdown::OnLayout();

	// Once the dropdown and the OpEdit have been layouted, the text may have scrolled
	// if it didn't fit completely. To prevent url spoofing, we always want to show the
	// protocol and domain, and therefore scroll back to beginning of the url.
	if (GetFocused() != edit)
	{
		edit->ResetScroll();
	}

#ifdef SUPPORT_START_BAR
	if ((g_application->IsCustomizingToolbars() ||
		 g_drag_manager->IsDragging()) &&
		GetParentDesktopWindow() &&
		(GetParentDesktopWindow()->IsActive() || !GetParentWorkspace()))
		ShowPopupWindow(TRUE);
	else if (IsFocused(TRUE) && 
		g_input_manager->GetKeyboardInputContext() != m_url_fav_menu)
		ShowPopupWindow(TRUE);
	else
		ShowPopupWindow(FALSE);
#endif // SUPPORT_START_BAR
}

void OpAddressDropDown::OnResize(INT32* new_w, INT32* new_h)
{
	OpTreeViewDropdown::OnResize(new_w, new_h);

	// Notify open password dialogs about resize
	DesktopWindow *dw = GetParentDesktopWindow();
	if (!dw || dw->GetType() != WINDOW_TYPE_DOCUMENT)
		return;
	DocumentDesktopWindow *ddw = (DocumentDesktopWindow*) dw;
    for (int i = 0; i < ddw->GetDialogCount(); i++)
    {
        if (ddw->GetDialog(i)->GetType() == DIALOG_TYPE_PASSWORD)
		{
			PasswordDialog* pwdialog = (PasswordDialog*)ddw->GetDialog(i);
            pwdialog->OnAddressDropdownResize();
		}
    }
}

void OpAddressDropDown::OnUpdateActionState()
{
	OpWindowCommander* win_comm = m_tab_cursor->GetWindowCommander();
	if (!win_comm)
		return;

	if (!WindowCommanderProxy::HasConsistentRating(win_comm) // There is a change in trust rating
			 // The server has blacklisted paths, and we have to check this one.
			 // (This is a bit hackish, ideally the path itself should be checked,
			 // but we want this piece of code to be cheap because it runs often.
			 // Hitting a fraud server, on the other hand, is rare, and we can afford
			 // doing a bit of double work in those cases.)
			 || (!WindowCommanderProxy::IsPhishing(win_comm) && !WindowCommanderProxy::FraudListIsEmpty(win_comm)))
	{
		win_comm->CheckDocumentTrust(FALSE, TRUE);
	}
}

BOOL OpAddressDropDown::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_STOP:
					child_action->SetEnabled(edit->IsFocused());
					return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_PREVIOUS_ITEM:
		case OpInputAction::ACTION_NEXT_ITEM:
		{
			if (!DropdownIsOpen())
			{
				ShowMenu();
				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_DELETE_SELECTED_ITEM:
		{
			int position = -1;
			HistoryAutocompleteModelItem* item = GetSelectedItem(&position);
			if(item && position >= 0)
			{
				DeleteItem(item, TRUE);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_STOP:
		case OpInputAction::ACTION_CLOSE_DROPDOWN:
		{
			if ( m_tab_cursor->GetWindowCommander() )
			{
				BOOL close_menu = action->GetAction() == OpInputAction::ACTION_CLOSE_DROPDOWN && DropdownIsOpen();

				OpWindowCommander *win_comm = m_tab_cursor->GetWindowCommander();
				if (action->IsKeyboardInvoked() && (!win_comm->IsLoading() || close_menu) && edit->IsFocused())
				{
					// The user pressed Esc :

					// If the dropdown is open - close it :
					if (DropdownIsOpen())
					{
						ShowDropdown(FALSE);
						return TRUE;
					}

					// Get the text in the edit field :
					OpString url;
					GetText(url);

					// Get the url of the current page or typed history :
					OpString page_url;
					if (m_tab_mode)
						page_url.Set(win_comm->GetCurrentURL(FALSE));
					else
						page_url.Set(directHistory->GetFirst());

					if (url.Compare(page_url.CStr()) == 0)
					{
						if (m_tab_mode)
						{
							// If they are the same focus the page :
							ReleaseFocus(FOCUS_REASON_OTHER);
							g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);

							return TRUE;
						}
						else
						{
							// Let it be possible to close the dialog when pressing escape
							return FALSE;
						}
					}
					else
					{
						// Reset the edit field to the page url and select it:
						SetText(page_url.CStr());
						edit->UnsetForegroundColor();
						edit->SelectText();
						return TRUE;
					}
				}
			}
			break;
		}
		case OpInputAction::ACTION_AUTOCOMPLETE_SERVER_NAME:
		{
			OpString url;

			if (m_is_quick_completed)
				url.Set(m_text_before_quick_completion);
			else
				GetText(url);

			url.Strip();

			if (url.HasContent())
			{
				// start completing the url unless it's:
				// 1) empty
				// 2) already appears to be complete
				// 3) looks like a search query eg. "g norske opera"
				if	(url.Find(UNI_L("://")) == KNotFound && url.FindFirstOf(' ') == KNotFound)
				{
					OpString prefix, suffix;

					// add prefix to the url. f.i. "www." (suppplied as data string in action)
					prefix.Set(action->GetActionDataString());

					if (prefix.HasContent())
					{
						prefix.Append(UNI_L("."));
						url.Insert(0, prefix);
					}

					// append suffix to the url. f.i. ".com" (suppplied as parameter in action)
					suffix.Set(action->GetActionDataStringParameter());

					if (suffix.HasContent())
						url.AppendFormat(UNI_L(".%s"), suffix.CStr());
				}

				// set new url in the address edit field
				SetText(url.CStr());

				if (m_open_page_in_tab)
				{
					// Force the url to be loaded in the current active page.
					// So the shift, ctrl modifiers do not change this behaviour
					// (they conflict with keyboard shortcuts for this function).
					g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_CURRENT_PAGE, 1, url.CStr(), this);
				}
				else
				{
					g_input_manager->InvokeAction(OpInputAction::ACTION_OK);
				}

				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_GO_TO_TYPED_ADDRESS:
		{
			if (ActivateShowAllItem(action->IsMouseInvoked() != FALSE))
			{
				// Consume the action if this invoked the "show all" item.
				return TRUE;
			}

#ifdef ENABLE_USAGE_REPORT
			if(g_usage_report_manager && g_usage_report_manager->GetSearchReport())
			{
				int pos = -1;
				HistoryAutocompleteModelItem * item  = GetSelectedItem(&pos);
				g_usage_report_manager->GetSearchReport()->SetWasSuggestFromAddressDropdown(item && item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM);
				g_usage_report_manager->GetSearchReport()->SetWasSelectedFromAddressDropdown(pos > 0);
				if(GetModel())
				{
					int count = GetModel()->GetCount();
					g_usage_report_manager->GetSearchReport()->SetSelectedDefaultFromAddressDropdown(pos == count - 1);
				}
			}
#endif // ENABLE_USAGE_REPORT

			break;
		}
		case OpInputAction::ACTION_SHOW_DROPDOWN:
		{
			if (edit)
				edit->SetFocus(FOCUS_REASON_ACTION);
			break;
		}
		case OpInputAction::ACTION_SHOW_ADDRESSBAR_OVERLAY:
		{
			GetWindowCommander()->GetUserConsent(DocumentDesktopWindow::ASK_FOR_PERMISSION_ON_USER_REQUEST);
			return TRUE;
		}
		case OpInputAction::ACTION_OPEN:
			if (action->IsActionDataStringEqualTo(UNI_L("URL Fav Menu")))
			{
				m_favorites_overlay_ctl = OP_NEW(FavoritesOverlayController, (m_url_fav_menu));
				if (m_favorites_overlay_ctl)
				{
					m_favorites_overlay_ctl->SetListener(this);
					m_url_fav_menu->SetDialogContext(m_favorites_overlay_ctl);
					if (OpStatus::IsSuccess(ShowDialog(m_favorites_overlay_ctl, g_global_ui_context, GetParentDesktopWindow())))
					{
						OnShowFavoritesDialog();						
					}
				}
				return TRUE;
			}
			break;
	}

	return OpTreeViewDropdown::OnInputAction(action);
}

void OpAddressDropDown::OnChange(OpWidget *widget,
								 BOOL changed_by_mouse)
{
	if (widget == edit)
	{
		m_actual_url.Empty();
		m_selected_item = NULL;
		OpString current_text;
		GetText(current_text);
		current_text.Strip(TRUE, TRUE);


		if (GetWindowCommander())
		{

			// text could be short or full address
			OpString show_address;
			m_address.GetShowAddress(show_address);
			m_url_page_mismatch = current_text.Compare(show_address) != 0 &&
									current_text.Compare(GetWindowCommander()->GetCurrentURL(FALSE)) != 0 &&
									current_text.Compare(GetWindowCommander()->GetLoadingURL(FALSE)) != 0;

			UpdateProtocolStatus();
		}

		if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AutoDropDown) == TRUE)
		{
			//--------------------------------------------------
			// The user has typed something - do autocompletion :
			//--------------------------------------------------
			m_url_proto_www_prefix.Empty();

			//--------------------------------------------------
			// If there is nothing - then do nothing :
			//--------------------------------------------------
			if(current_text.IsEmpty())
			{
				m_query_error_counter = 0;
				Clear();
				ShowDropdown(FALSE);
				return;
			}

			//--------------------------------------------------
			// Store the user text :
			//--------------------------------------------------

			SetUserString(current_text.CStr());

			//--------------------------------------------------
			// Quick "inline" completion of previosly typed urls.
			//--------------------------------------------------

			// FIX: We should probably not exclude local file urls. We should instead complete
			//      them if the user press tab! (same with all quick completed urls)
			DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();
			OpString stripped_local_file_url;
			BOOL is_local_file_url = history_model->IsLocalFileURL(current_text, stripped_local_file_url);
			OpString speed_dial_url, speed_dial_title;

			uni_char * previously_typed_url = NULL;
			if (!edit->IsComposing() &&
				edit->IsLastUserChangeAnInsert() &&
				// Only if the pref is enabled.
				g_pcui->GetIntegerPref(PrefsCollectionUI::AddressbarInlineAutocompletion) &&
				// Only if the caret is at the end, so the user can edit other parts without jumping to the end.
				edit->GetCaretOffset() == current_text.Length() &&
				!is_local_file_url &&
				!g_hotlist_manager->HasNickname(current_text.CStr(), NULL) &&
				!g_speeddial_manager->IsAddressSpeedDialNumber(current_text.CStr(), speed_dial_url, speed_dial_title))
			{
				SearchEngineManager::SearchSetting setting;
				setting.m_target_window = GetParentDesktopWindow();
				setting.m_keyword.Set(current_text);
				if (!g_searchEngineManager->CanSearch(setting) || g_searchEngineManager->IsSingleWordSearch(current_text.CStr()))
				{
					// Iterate through all typed history and look for last typed match.
					// After that, look for the shortest match matching the last typed match.
					uni_char * candidate = NULL;
					int candidate_len = 0;
					OpString candidate_url_proto_www_prefix;

					uni_char * address = directHistory->GetFirst();

					while (address)
					{
						// DeduceProtocolOrSubdomain leaves prefix for this address in m_url_proto_www_prefix
						m_url_proto_www_prefix.Empty();
						address = DeduceProtocolOrSubdomain(current_text, address);

						// Check if it's a match
						if (current_text.Compare(address, current_text.Length()) == 0 &&
							// Experiment: Do we wan't to complete really long urls? They're probably pasted and
							// not subject for re-entering anyway which might look odd with to long urls anyway.
							uni_strlen(address) < 40 &&
							// Don't quick-complete other things than urls
							g_searchEngineManager->IsUrl(address))
						{
							OpString domain;
							unsigned domain_offset = 0;
							if (OpStatus::IsError(StringUtils::FindDomain(address, domain, domain_offset)))
								continue;

							int address_len = uni_strlen(address);

							// look for the shortest address matching the first domain provided by direct history
							// (direct history provides entries sorted by last asscess time) : DSK-366279
							if (!candidate || (address_len < candidate_len && domain.Compare(candidate,domain.Length()) == 0))
							{
								candidate = address;
								candidate_len = address_len;
								OpStatus::Ignore(candidate_url_proto_www_prefix.Set(m_url_proto_www_prefix));
							}
						}

						address = directHistory->GetNext();
					}

					// We're done
					OpStatus::Ignore(m_url_proto_www_prefix.Set(candidate_url_proto_www_prefix));
					previously_typed_url = candidate;
				}
			}

			// Insert the inline completion in the textfield.
			if (previously_typed_url)
			{
				m_text_before_quick_completion.Set(current_text);
				m_is_quick_completed = true;

				// Experiment: If we don't want this, we should make a item and put it first instead.
				OP_ASSERT(current_text.Length() <= (int)uni_strlen(previously_typed_url));
				if (OpStatus::IsSuccess(OpDropDown::SetText(previously_typed_url, TRUE, TRUE)))
				{
					edit->SetSelection(current_text.Length(), uni_strlen(previously_typed_url));
					edit->SetCaretOffset(current_text.Length());
				}
			}
			else
			{
				m_is_quick_completed = false;
			}
			//--------------------------------------------------
			// If you are supposed to do browse engine module then wait until that is done
			// before populating the dropdown - otherwise just populate it
			//--------------------------------------------------

			// Start a new one
			if(current_text.Length() >= SEARCH_LENGTH_THRESHOLD)
			{
				QuerySearchEngineModule(current_text);
			}
			else
			{
				PopulateDropdown();
			}
		}
	}
	else if(widget == GetTreeView())
	{
		HistoryAutocompleteModelItem* item  = GetSelectedItem();

		if (item && item->GetIndex() >0)
		{
			m_selected_item = item;
		}
		//--------------------------------------------------
		// If the user has selected an item show the address
		// Only as a result of a keyboard action :
		//--------------------------------------------------
		if (!changed_by_mouse && item)
		{
			if(item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM)
			{
				if (!m_lock_current_text)
				{
					OpString formated_string;

					if(OpStatus::IsSuccess(GetFormattedSuggestedSearch(static_cast<SearchSuggestionAutocompleteItem*>(item), formated_string)))
					{
						SetText(formated_string);
					}
				}
			}
			else
			{
				// Get the address to go to
				OpString address;
				if (!m_lock_current_text && OpStatus::IsSuccess(item->GetAddress(address)))
				{
					if (m_use_display_url && item->HasDisplayAddress())
					{
						m_actual_url.Set(address);
						item->GetDisplayAddress(address);
						SetText(address.CStr());
					}
					else
					{
						SetText(address.CStr());
					}
				}
			}
		}
	}
}

void OpAddressDropDown::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	int position = -1;
	GenericTreeModelItem* item  = GetSelectedItem(&position);
	if(item && nclicks == 1 && widget->GetType() == WIDGET_TYPE_TREEVIEW)
	{
		if(((OpTreeView*)widget)->IsHoveringAssociatedImageItem(pos) && !down)
		{
			// we clicked the close button on the history item, remove it
			m_mh->PostDelayedMessage(MSG_QUICK_REMOVE_ADDRESSFIELD_HISTORY_ITEM, (MH_PARAM_1)item, 0, 0);
			return;
		}

	}

	OpTreeViewDropdown::OnMouseEvent(widget, pos, x, y, button, down, nclicks);

	// Set the address as it was in none focus and eventually add query field in order to get the 
	// right caret offset when focus is gained
	if (widget->IsOfType(WIDGET_TYPE_EDIT) && down && GetFocused()!=edit && m_address.HasContent() &&
		!m_url_page_mismatch && !g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullURL))
	{
		OpString show_address;
		m_address.GetShowAddress(show_address);
		show_address.Append(m_address.GetQueryField());
		show_address.Append(UNI_L("/")); // compensate for the removal of the ending '/'
		OpTreeViewDropdown::SetText(show_address);
	}
}

void OpAddressDropDown::OnMouseMove(OpWidget *widget, const OpPoint &point)
{
#ifdef REMOVE_HIGHLIGHT_ONHOVER
	UpdateAddressFieldHighlight();
#endif
}

void OpAddressDropDown::OnMouseLeave(OpWidget *widget)
{
#ifdef REMOVE_HIGHLIGHT_ONHOVER
	UpdateAddressFieldHighlight(TRUE);
#endif
}

void OpAddressDropDown::DeleteHistoryModelItem(const uni_char *str)
{
	HistoryModelItem* history_item = DesktopHistoryModel::GetInstance()->GetItemByUrl(str);
	if(history_item)
	{
		// Delete the item in the source
		DesktopHistoryModel::GetInstance()->Delete(history_item);
	}
}

void OpAddressDropDown::RemovePossibleContentSearchItems(OpStringC url)
{
	DeleteHistoryModelItem(url);

	// DSK-355955 Removing both http and https address from history when deleting one variant
	int pos = url.Find(UNI_L("://"));
	if (pos != KNotFound && url.Find("http") == 0)
	{
		OpString address_variant;
		RETURN_VOID_IF_ERROR(address_variant.Set(pos == 4 ? UNI_L("https") : UNI_L("http")));
		RETURN_VOID_IF_ERROR(address_variant.Append(url.SubString(pos)));

		DeleteHistoryModelItem(address_variant);
	}

	// Remove similar item too (Similar items won't be visible in the list, but we should remove them)
	// This should match the behaviour of HasSimilarItem.
	pos = url.Find(UNI_L("://www."));
	if (pos != KNotFound)
	{
		OpString similar_address;
		RETURN_VOID_IF_ERROR(similar_address.Set(url));
		similar_address.Delete(pos + 3, 4); // Delete "www." from the url.
		DeleteHistoryModelItem(similar_address);
	}
}

void OpAddressDropDown::DeleteItem(HistoryAutocompleteModelItem *item, BOOL call_on_change)
{
	if (!DropdownIsOpen() || !item || !item->IsUserDeletable())
		return;

	OpString url;
	RETURN_VOID_IF_ERROR(item->GetAddress(url));

	if (item->IsInDirectHistory())
	{
		if (item->IsInDesktopHistoryModel())
		{
			DeleteFromDirectHistory(url);
		}
		else if (directHistory->Delete(url.CStr()))
		{
			INT32 pos = m_items_to_delete.Find(item);
			if (pos != -1)
				m_items_to_delete.Remove(pos);
			item->Delete();
			item = NULL;
		}
	}
	if (item && item->IsInDirectHistory())
	{
		OP_ASSERT(m_items_to_delete.Find(item) == -1);

		RemovePossibleContentSearchItems(url);
	}

	// Show again to automatically resize the dropdown
	ShowDropdown(GetModel()->GetCount() > 0);

	if (call_on_change)
	{
		// Call OnChange so we update info to the new selected item.
		OnChange(GetTreeView(), FALSE);
	}
}

void OpAddressDropDown::DeleteFromDirectHistory(OpStringC full_address)
{
	// Due to performance issues there are deleted only full address, domain,
	// domain/ if there is no '.' (user has to put domains without '.'
	// with '/' at the end to prevent searching domain on default search
	// engine) and www.domain. This should cover most of the cases of typed
	// addresses.

	directHistory->Delete(full_address);

	OpString address_to_delete;
	unsigned starting_position;

	RETURN_VOID_IF_ERROR(StringUtils::FindDomain(full_address, address_to_delete, starting_position));

	// Do not delete anything else if the full address goes to sub page of
	// given domain.
	if (address_to_delete.IsEmpty() || (address_to_delete.Length() + starting_position + 1 < unsigned(full_address.Length())))
		return;

	directHistory->Delete(address_to_delete);

	if (address_to_delete.Find(UNI_L(".")) == KNotFound)
	{
		RETURN_VOID_IF_ERROR(address_to_delete.Append(UNI_L("/")));
		directHistory->Delete(address_to_delete);
		address_to_delete.Delete(address_to_delete.Length() - 1);
	}

	RETURN_VOID_IF_ERROR(address_to_delete.Insert(0, UNI_L("www.")));

	directHistory->Delete(address_to_delete);
}

void OpAddressDropDown::GetBookmarks(const OpString& current_text, OpVector<HistoryAutocompleteModelItem>& result)
{
	// this is probably some random text pasted accidentally into address bar and
	// highlighting with large number of phrases may freeze UI for a few seconds (DSK-340411)
	if (current_text.Length() > 1024)
		return;

	HotlistModel* bookmarks = g_hotlist_manager->GetBookmarksModel();

	WordHighlighter word_highlighter;

	for(int i = 0; (i < bookmarks->GetCount()) && (result.GetCount() < 20); i++)
	{
		// Get the bookmark
		HotlistModelItem* item = bookmarks->GetItemByIndex(i);
		if(item->GetType() != OpTypedObject::BOOKMARK_TYPE)
			continue;

		// Cast it to the correct type
		Bookmark* bookmark = static_cast<Bookmark*>(item);

		// Get the core history item for this bookmark
		HistoryPage* page = bookmark->GetHistoryItem();
		if(!page)
			continue;

		// Get the HistoryModelPage item for the core item
		HistoryModelPage* history_item = DesktopHistoryModel::GetInstance()->GetPage(page);
		if(!history_item)
			continue;

		// Get the simple item
		PageBasedAutocompleteItem * simple_item = history_item->GetSimpleItem();

		if(result.Find(simple_item) != -1)
			continue;

		// If it is not already in the model - check if it matches
		if(simple_item && !simple_item->GetModel())
		{
			if (word_highlighter.Empty())
			{
				RETURN_VOID_IF_ERROR(word_highlighter.Init(current_text.CStr()));
				if (word_highlighter.Empty())
					return;
			}
			if(MatchAndHighlight(bookmark, history_item, word_highlighter))
			{
				simple_item->SetSearchType(PageBasedAutocompleteItem::BOOKMARK);
				result.Add(simple_item);
			}
		}
	}
}

BOOL OpAddressDropDown::MatchAndHighlight(Bookmark* bookmark,
										  HistoryModelPage* page,
										  WordHighlighter& word_highlighter)
{
	if(!bookmark || !page)
		return FALSE;

	// Get the url:
	OpString address;
	RETURN_VALUE_IF_ERROR(address.Set(bookmark->GetDisplayUrl()), FALSE);

	// Make the haystack
	OpString haystack;
	RETURN_VALUE_IF_ERROR(haystack.AppendFormat(UNI_L("%s %s %s"),
												StringUtils::GetNonNullString(address),
												StringUtils::GetNonNullString(bookmark->GetName()),
												StringUtils::GetNonNullString(bookmark->GetDescription())), FALSE);

	// Check that it is a match
	if(!word_highlighter.ContainsWords(haystack))
		return FALSE;

	// Do the default highlighting
	HistoryAutocompleteModelItem * simple_item = page->GetSimpleItem();

	// Use the bookmark title for this hit
	simple_item->SetBookmark(bookmark);

	// Use the description for associated text, and highlight it
	if (bookmark->GetName())
	{
		// Copy the description
		OpString bookmark_name;
		RETURN_VALUE_IF_ERROR(bookmark_name.Set(bookmark->GetName()), FALSE);

		// Make highlighted name
		OpString highlighted_name;
		RETURN_VALUE_IF_ERROR(word_highlighter.AppendHighlight(highlighted_name,
															   bookmark_name.CStr(),
															   KAll,
															   UNI_L("<b>"),
															   UNI_L("</b>"),
															   -1), FALSE);


		RETURN_VALUE_IF_ERROR(simple_item->SetAssociatedText(highlighted_name), FALSE);
	}
	else if(bookmark->GetDescription().HasContent())
	{
		// Copy the description
		OpString bookmark_description;
		RETURN_VALUE_IF_ERROR(bookmark_description.Set(bookmark->GetDescription()), FALSE);

		// Make highlighted description
		OpString highlighted_description;
		RETURN_VALUE_IF_ERROR(word_highlighter.AppendHighlight(highlighted_description,
															   bookmark_description.CStr(),
															   KAll,
															   UNI_L("<b>"),
															   UNI_L("</b>"),
															   -1), FALSE);


		RETURN_VALUE_IF_ERROR(simple_item->SetAssociatedText(highlighted_description), FALSE);
	}

	return TRUE;
}

bool OpAddressDropDown::ShouldQuerySearchSuggestions(const OpStringC& current_text)
{
	if (!m_show_search_suggestions || current_text.Length() < SEARCH_LENGTH_THRESHOLD || 
		(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowSearchesInAddressfieldAutocompletion) == 0 && !g_searchEngineManager->IsSearchString(current_text)))
		return false;

	DocumentDesktopWindow* window = g_application->GetActiveDocumentDesktopWindow();
	if (window && window->PrivacyMode())
		return false;

	if (!m_regex)
	{
		OpREFlags flags;
		flags.multi_line = FALSE;
		flags.case_insensitive = TRUE;
		flags.ignore_whitespace = FALSE;
		RETURN_VALUE_IF_ERROR(OpRegExp::CreateRegExp(&m_regex, UNI_L("^(htt(ps?)?|(www)|(ftp))$"), &flags), false);

	}
	OpREMatchLoc match_pos;
	RETURN_VALUE_IF_ERROR(OpStatus::IsSuccess(m_regex->Match(current_text, &match_pos)), false);

	if (match_pos.matchloc == 0)
		return false;

	if (current_text.FindFirstOf(UNI_L(" ")) != KNotFound)
		return true;

	if (current_text.Find(UNI_L(".")) == KNotFound && current_text.Find(UNI_L(":")) == KNotFound && current_text.Find(UNI_L("/")) == KNotFound)
		return  true;
	
	return false;
}

void OpAddressDropDown::QuerySearchSuggestions(const OpStringC& search_phrase, SearchTemplate* search_template)
{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	if (!search_template)
	{
		search_template = g_searchEngineManager->GetDefaultSearch();
	}
	if (!search_template)
	{
		return;
	}

	if(!m_search_suggest_in_progress && search_phrase.HasContent() && m_search_suggest->HasSuggest(search_template->GetUniqueGUID()))
	{
		RETURN_VOID_IF_ERROR(m_search_phrase.Set(search_phrase));
		StartSearchSuggest();
	}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
}


void OpAddressDropDown::HandleSearchEngineModuleCallback(const OpStringC& search_text, OpAutoVector<VisitedSearch::SearchResult>* result)
{
	if(!result)
	{
		return;
	}

	m_search_engine_items.Clear();

	for(unsigned int i = 0; i < result->GetCount(); i++)
	{
		VisitedSearch::SearchResult * search_result = result->Get(i);
		OpString url;
		url.SetFromUTF8(search_result->url.CStr());

		//--------------------------------------------------
		//  Find the history item - note it _should_ always exist :
		//--------------------------------------------------
		DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();
		HistoryModelPage * item = history_model->GetItemByUrl(url);
		bool page_content_search_enabled = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::PageContentSearch) == 1;

		if(item && item->IsInHistory())
		{
			PageBasedAutocompleteItem* simple_item = item->GetSimpleItem();

			OpString title;
			RETURN_VOID_IF_ERROR(simple_item->GetTitle(title));

			if (search_text.FindI(" ") != KNotFound)
			{
				bool found = false;
				UniString uni_input;
				uni_input.SetConstData(search_text.CStr());
				OpAutoPtr< OtlCountedList<UniString> > parts(uni_input.Split(' '));
				
				for (OtlList<UniString>::Iterator part = parts->Begin(); part != parts->End(); ++part)
				{
					if (part->IsEmpty())
						continue;

					if (title.FindI(part->Data(true)) == KNotFound && url.FindI(part->Data(true)) == KNotFound)
					{
						found = false;
						break;
					}

					found = true;
				}

				if (found)
				{
					m_search_engine_items.Add(simple_item);
					return;
				}
			}

			if (title.FindI(search_text) != KNotFound || url.FindI(search_text) != KNotFound)
			{
				m_search_engine_items.Add(simple_item);
			}
			else if (page_content_search_enabled)
			{
				simple_item->SetSearchType(PageBasedAutocompleteItem::PAGE_CONTENT);

				if (search_result->excerpt.CStr())
					simple_item->SetAssociatedText(search_result->excerpt);
				m_search_engine_items.Add(simple_item);
			}
		}
	}
}

void OpAddressDropDown::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(msg == MSG_QUICK_START_SEARCH_SUGGESTIONS)
	{
		if(m_search_suggest_in_progress)
		{
			// already in progress - repost it
			StartSearchSuggest();
			return;
		}
		OpString key, value;
		if(m_search_phrase.HasContent())
		{
			SearchEngineManager::SplitKeyValue(m_search_phrase, key, value);
			SearchTemplate* search_template = NULL;
			if (key.HasContent())
			{
				search_template = g_searchEngineManager->GetSearchEngineByKey(key);
			}
			else
			{
				search_template = g_searchEngineManager->GetDefaultSearch();
			}

			if (!search_template)
			{
				return;
			}

			if (value.HasContent())
			{
				OpString guid;
				if (OpStatus::IsSuccess(search_template->GetUniqueGUID(guid)))
					m_search_suggest->Connect(value, guid, SearchTemplate::ADDRESS_BAR);
			}
			else
			{
				if (GetModel())
				{
					GetModel()->RemoveAll();
				}
				
				DeleteItems(m_search_suggestions_items);
				PopulateDropdown(false);
			}
		}
		return;
	}
	else if (msg == MSG_VPS_SEARCH_RESULT)
	{
		// make sure we don't leak even if we don't process the search result
		OpAutoPtr<OpAutoVector<VisitedSearch::SearchResult> > results(reinterpret_cast<OpAutoVector<VisitedSearch::SearchResult>*>(par2));

		if (m_process_search_results)
		{
			OpString current_text;
			OP_STATUS status = OpStatus::OK;

			if (GetModel())
			{
				GetModel()->RemoveAll();
			}

			if (m_is_quick_completed)
			{
				status = current_text.Set(m_text_before_quick_completion);
			}
			else
			{
				status = GetText(current_text);
				current_text.Strip();
			}
			if (OpStatus::IsSuccess(status))
			{
				if (ShouldQuerySearchSuggestions(current_text))
				{
					QuerySearchSuggestions(current_text);
				}
				else
				{
					DeleteItems(m_search_suggestions_items);
				}
				HandleSearchEngineModuleCallback(current_text, results.get());
			}
			PopulateDropdown();
		}
	}
	else if (msg == MSG_QUICK_REMOVE_ADDRESSFIELD_HISTORY_ITEM)
	{
		HistoryAutocompleteModelItem *item = reinterpret_cast<HistoryAutocompleteModelItem*>(par1);
		if(item && GetModel() && GetModel()->GetIndexByItem(item) > -1)
			DeleteItem(item, FALSE);
	}
	else
		OpTreeViewDropdown::HandleCallback(msg, par1, par2);
}

void OpAddressDropDown::QuerySearchEngineModule(const OpStringC& current_text)
{
#ifdef VPS_WRAPPER

	OpString search_text;
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	// Strip away the history search prefix
	OpString key;
	OpString value;
	SearchEngineManager::SplitKeyValue(current_text, key, value);
	RETURN_VOID_IF_ERROR(search_text.Set(value.CStr()));

#else
	RETURN_VOID_IF_ERROR(search_text.Set(current_text.CStr()));
#endif // DESKTOP_UTIL_SEARCH_ENGINES

	RETURN_VOID_IF_ERROR(g_visited_search->Search(search_text.CStr(),
											  VisitedSearch::RankSort,
											  10,
											  300,
											  UNI_L("<b>"),
											  UNI_L("</b>"),
											  16,
											  this));
	m_process_search_results = TRUE;

#endif // VPS_WRAPPER
}

bool OpAddressDropDown::HasSimilarItem(const HistoryAutocompleteModelItem* item, const OpVector<HistoryAutocompleteModelItem>& items)
{
	OP_ASSERT(!item->GetModel());

	for(unsigned i = 0; i < items.GetCount(); i++)
	{
		HistoryAutocompleteModelItem *added_item = items.Get(i);

		OpString address, added_address;
		item->GetAddress(address);
		added_item->GetAddress(added_address);

		// Compare the addresses without www. since we get lots of duplicates with and without www.

		// Is there any way to make this cheaper? m_page m_core_page seems to have a m_key which may be what we want?
		INT32 pos = address.Find(UNI_L("://www."));
		if (pos != -1)
			address.Delete(pos + 3, 4);
		pos = added_address.Find(UNI_L("://www."));
		if (pos != -1)
			added_address.Delete(pos + 3, 4);

		if (added_address == address)
			return TRUE;
	}
	return FALSE;
}

void OpAddressDropDown::PopulateDropdown(bool refresh_history)
{
	//--------------------------------------------------
	// If we don't have focus do not display the dropdown
	// This message is too late - ignore it
	//--------------------------------------------------
	if(!HasFocus())
		return;

	OpString stripped;
	OpString current_text;

	if (m_is_quick_completed)
	{
		current_text.Set(m_text_before_quick_completion);
	}
	else
	{
		GetText(current_text);
	}

	//--------------------------------------------------
	// If there is nothing - then do nothing :
	//--------------------------------------------------
	if(current_text.IsEmpty())
	{
		Clear();
		ShowDropdown(FALSE);
		return;
	}

	//--------------------------------------------------
	// Initialize the dropdown :
	//--------------------------------------------------

	RETURN_VOID_IF_ERROR(InitTreeViewDropdown("Addressfield Treeview Window Skin"));

	if (refresh_history)
	{
		m_history_items.Clear();
		m_item_to_select = NULL;
	}

	if (current_text.Length()<SEARCH_LENGTH_THRESHOLD)
	{
		DeleteItems(m_search_suggestions_items);
		m_search_engine_items.Clear();
	}

	m_more_items.Clear();

	HistoryAutocompleteModel* tree_model = NULL;

	tree_model = OP_NEW(HistoryAutocompleteModel, ());

	if (!tree_model)
	{
		return;
	}
	SetModel(tree_model, FALSE);

	if(!GetTreeView())
	{
		return;
	}

	GetTreeView()->SetExtraLineHeight(4);
	GetTreeView()->SetExpandOnSingleClick(FALSE);
	GetTreeView()->SetOnlyShowAssociatedItemOnHover(TRUE);
	GetTreeView()->SetAllowWrappingOnScrolling(TRUE);
	GetTreeView()->SetPaintBackgroundLine(FALSE);



	if (IsOperaPage(current_text))
	{
		AddOperaPages(current_text);
	}
	else if (refresh_history)
	{
		// ... then - if old searches are obsolete - start new search
		if (ShouldQuerySearchSuggestions(current_text))
		{
			OpString key;
			OpString value;
			SearchEngineManager::SplitKeyValue(current_text, key, value);
			SearchTemplate* search_template = g_searchEngineManager->GetSearchEngineByKey(key);
			QuerySearchSuggestions(current_text, search_template);
		}
	}


	if (AddSearchForItem(current_text))
	{
		m_item_to_select = m_search_for_item;
	}
			
	OpString search_for_address;
	if (m_search_for_item)
		m_search_for_item->GetAddress(search_for_address);

	for (unsigned i = 0; i < m_search_suggestions_items.GetCount(); ++i)
	{
		SearchSuggestionAutocompleteItem* item = m_search_suggestions_items.Get(i);
				
		OpString current_address;
		if (OpStatus::IsSuccess(item->GetAddress(current_address)) && current_address == search_for_address)
			continue;

		item->SetShowSearchEngine();
		GetModel()->AddLast(item);
	}

	AddPreviousSearches(current_text);

	if (refresh_history)
		PrepareHistory(current_text);
	
	AddSpeedDialAddress(current_text);

	OpVector<HistoryAutocompleteModelItem> combined_items;

	for (unsigned i = 0; i < m_history_items.GetCount(); ++i)
	{
		HistoryAutocompleteModelItem* item = m_history_items.Get(i);
		if (refresh_history)
			item->UpdateRank(current_text);
		combined_items.Add(item);
	}

	combined_items.QSort(HistoryAutocompleteModelItem::Cmp);

	unsigned max_items = MAX_ROWS_IN_DROPDOWN - MAX_SUGGESTIONS_ADDRESS_FIELD_SEARCH;

	for (unsigned i = 0; i < combined_items.GetCount(); ++i)
	{
		HistoryAutocompleteModelItem* item = combined_items.Get(i);

		if (i < max_items || (m_more_items.GetCount() == 0 && combined_items.GetCount() == i + 1))
		{
			GetModel()->AddLast(item);
			OpString item_address;
			if (OpStatus::IsSuccess(item->GetAddress(item_address)) && item_address == current_text)
			{
				m_item_to_select = item;
			}
		}
		else
			m_more_items.Add(item);
	}

	if (m_more_items.GetCount() > 0)
		AddExpander();
	
	if (!m_is_quick_completed && m_item_to_select && !g_searchEngineManager->IsUrl(current_text.CStr()))
	{
		++m_lock_current_text;
		SetSelectedItem(m_item_to_select, TRUE);
		--m_lock_current_text;
	}
	else if (m_selected_item && !refresh_history)
	{
		++m_lock_current_text;
		SetSelectedItem(m_selected_item, TRUE);
		--m_lock_current_text;
	}


	ShowDropdown(GetModel()->GetCount() > 0);
}

bool OpAddressDropDown::AddSearchForItem(const OpStringC& current_text)
{
	if (m_search_for_item || !m_show_speed_dial_pages)
		return false;

	OpString key;
	OpString value;
	OpString search_label;
	OpString search_string;
	OpString text;

	SearchEngineManager::SplitKeyValue(current_text, key, value);

	OpAutoPtr<SearchSuggestionAutocompleteItem> item(OP_NEW(SearchSuggestionAutocompleteItem, (GetModel())));

	if (!item.get())
		return false;
	
	item->SetIsSearchWith();
	RETURN_VALUE_IF_ERROR(item->SetHighlightText(value),false);
	RETURN_VALUE_IF_ERROR(item->SetSearchSuggestion(value.CStr()), false);

	SearchTemplate* search_template = NULL;
	if (g_searchEngineManager->IsSearchString(current_text))
		search_template  = g_searchEngineManager->GetSearchEngineByKey(key);
	else
		search_template  = g_searchEngineManager->GetDefaultSearch();

	if (!search_template)
		return false;

	RETURN_VALUE_IF_ERROR(item->SetSearchProvider(search_template->GetUniqueGUID()), false);

	RETURN_VALUE_IF_ERROR(item->SetAddress(search_string), false);
	RETURN_VALUE_IF_ERROR(item->SetItemData(1, text.CStr()), false);
	item->SetBadgeWidth(m_protocol_button->GetWidth());

	if (GetModel()->AddLast(item.get()) >= 0)
	{
		m_search_for_item = item.release();
		return true;
	}
	return false;
}

void OpAddressDropDown::PrepareHistory(const OpString& current_text)
{
	OpVector<HistoryAutocompleteModelItem> history_items;
	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

	OpStatus::Ignore(history_model->GetSimpleItems(current_text, history_items));
	if (OpStatus::IsError(history_model->AddDesktopHistoryModelListener(this)))
		return;

	GetBookmarks(current_text, history_items);

	for (unsigned i = 0; i < m_search_engine_items.GetCount(); ++i)
	{
		OpStatus::Ignore(history_items.Add(m_search_engine_items.Get(i)));
	}

	for (unsigned int i = 0; i < history_items.GetCount(); i++)
	{
		HistoryAutocompleteModelItem* item_to_add = history_items.Get(i);

		OpString display_address;
		if (OpStatus::IsError(item_to_add->GetDisplayAddress(display_address)))
			continue;
		if (IsOperaPage(display_address))
			continue;

		if (HasSimilarItem(item_to_add, m_history_items))
			continue;

		bool found = false;
		for (unsigned j = 0; j < m_history_items.GetCount(); ++j)
		{
			OpString address;
			if (OpStatus::IsError(m_history_items.Get(j)->GetDisplayAddress(address)))
			{
				continue;
			}
			if (display_address == address)
			{
				found = true;
				break;
			}
		}
		if (found)
			continue;

		item_to_add->SetHighlightText(current_text);
		item_to_add->SetBadgeWidth(m_protocol_button->GetWidth());

		m_history_items.Add(item_to_add);
	}
}

void OpAddressDropDown::AddOperaPages(const OpString& current_text)
{
	int colon_position = current_text.Find(":");
	OP_ASSERT(colon_position == 1 || colon_position == 5);
	OpStringC opera_page = current_text.SubString(colon_position + 1);
	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();
	OpVector<PageBasedAutocompleteItem> opera_pages;
	history_model->GetOperaPages(opera_page, opera_pages);

	unsigned lines_to_add = min((unsigned)GetMaxLines(), opera_pages.GetCount());
	for (unsigned i = 0; i < lines_to_add; ++i)
	{
		PageBasedAutocompleteItem* item = opera_pages.Get(i);
		item->SetSearchType(PageBasedAutocompleteItem::OPERA_PAGE);
		item->SetBadgeWidth(m_protocol_button->GetWidth());
		GetModel()->AddLast(item);
	}
}

bool OpAddressDropDown::AddSpeedDialAddress(const OpString& current_text)
{
	if (!m_show_speed_dial_pages)
	{
		return false;
	}
	OpString speed_dial_url;
	OpString speed_dial_title;

	if (!g_speeddial_manager->IsAddressSpeedDialNumber(current_text, speed_dial_url, speed_dial_title))
	{
		return false;
	}

	SimpleAutocompleteItem* item = OP_NEW(SimpleAutocompleteItem, (GetModel()));
	if (!item || OpStatus::IsError(m_items_to_delete.Add(item)))
	{
		OP_DELETE(item);
		return false;
	}

	OpString str;
	RETURN_VALUE_IF_ERROR(g_languageManager->GetString(Str::S_ADDRESS_FIELD_OPEN_DIAL, str),false);
	OpString info;
	RETURN_VALUE_IF_ERROR(info.AppendFormat(str.CStr(), uni_atoi(current_text.CStr())),false);
	RETURN_VALUE_IF_ERROR(item->SetItemData(1, info.CStr()),false);
	int color = SkinUtils::GetTextColor("Address DropDown URL Label");
	if (color >= 0)
	{
		item->SetItemColor(1, color);
	}
	item->SetBadgeWidth(m_protocol_button->GetWidth());
	RETURN_VALUE_IF_ERROR(item->SetItemData(1, speed_dial_title.CStr()),false);
	RETURN_VALUE_IF_ERROR(item->SetItemImage("Window Document Icon"),false);
	RETURN_VALUE_IF_ERROR(item->SetAddress(current_text.CStr()),false);
	if (GetModel()->AddLast(item) == -1)
		return false;

	m_item_to_select = item;
	return true;
}

void OpAddressDropDown::AddExpander()
{
	OP_DELETE(m_show_all_item);
	m_show_all_item = OP_NEW(SimpleAutocompleteItem, (GetModel()));
	if (!m_show_all_item)
	{
		return;
	}

	OpString str;
	OpString info;
	if (OpStatus::IsError(g_languageManager->GetString(Str::S_ADDRESS_FIELD_SHOW_MORE, str)))
	{
		return;
	}
	if (OpStatus::IsError(info.AppendFormat(str.CStr(), m_more_items.GetCount())))
	{
		return;
	}
	m_show_all_item->SetShowAll(TRUE);
	m_show_all_item->SetItemData(1, info.CStr());
	m_show_all_item->SetBadgeWidth(m_protocol_button->GetWidth());
	int color = SkinUtils::GetTextColor("Address DropDown URL Label");
	if (color >= 0)
	{
		m_show_all_item->SetItemColor(1, color);
	}
	GetModel()->AddLast(m_show_all_item);
	for(UINT32 i = 0; i < m_more_items.GetCount(); i++)
	{
		if(!m_more_items.Get(i)->GetModel())
		{
			m_show_all_item->AddChildLast(m_more_items.Get(i));
		}
	}
}

void OpAddressDropDown::AddPreviousSearches(const OpStringC& search_text)
{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	OpString key;
	OpString value;

	SearchEngineManager::SplitKeyValue(search_text, key, value);
	SearchTemplate* search_template = g_searchEngineManager->GetSearchEngineByKey(key);
	if(!search_template)
		return;

	int len = search_text.Length();
	uni_char * address = directHistory->GetFirst();

	// Add up to three previous searches
	for (unsigned count = 0; address && count < 3;)
	{
		if(uni_strncmp(search_text.CStr(), address, len) == 0)
		{
			count++;
			OpString search_name;
			RETURN_VOID_IF_ERROR(search_template->GetEngineName(search_name));

			SimpleAutocompleteItem * simple_item = OP_NEW(SimpleAutocompleteItem, (GetModel()));

			if (!simple_item || OpStatus::IsError(simple_item->SetAddress(address)) ||
					OpStatus::IsError(m_items_to_delete.Add(simple_item)))
			{
				OP_DELETE(simple_item);
				return;
			}

			simple_item->SetInDirectHistory(TRUE);
			OpString previous_search_text;
			previous_search_text.Set(address);
			previous_search_text.Append(" - ");
			previous_search_text.Append(search_name.CStr());
			simple_item->SetItemData(1, previous_search_text);

			simple_item->SetBadgeWidth(m_protocol_button->GetWidth());
			int color = SkinUtils::GetTextColor("Address DropDown URL Label");
			if (color >= 0)
			{
				simple_item->SetItemColor(1, color);
			}
			simple_item->SetFavIconKey(search_template->GetUrl());
			simple_item->SetItemImage(search_template->GetSkinIcon());
			GetModel()->AddLast(simple_item);
		}
		address = directHistory->GetNext();
	}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
}

bool OpAddressDropDown::IsOperaPage(const OpString& address) const
{
	// Check if address begins either with opera: or with o:
	return address.CompareI("opera:", 6) == 0 || address.CompareI("o:", 2) == 0;
}

void OpAddressDropDown::StartIntranetTest(const uni_char *address)
{
	OP_ASSERT(g_searchEngineManager->IsIntranet(address));
	OP_DELETE(m_host_resolver);
	m_host_resolver = NULL;

	RETURN_VOID_IF_ERROR(m_host_resolve_address.Set(address));
	if (OpStatus::IsSuccess(SocketWrapper::CreateHostResolver(&m_host_resolver, this)))
		m_host_resolver->Resolve(address);
}

void OpAddressDropDown::OnHostResolved(OpHostResolver* host_resolver)
{
	DesktopWindow *dw = GetParentDesktopWindow();
	if (!dw || dw->GetType() != WINDOW_TYPE_DOCUMENT)
		return;

	DocumentDesktopWindow *ddw = (DocumentDesktopWindow*) dw;
	ddw->ShowGoToIntranetBar(m_host_resolve_address.CStr());

	// Cleanup right away
	OP_DELETE(m_host_resolver);
	m_host_resolver = NULL;
}

void OpAddressDropDown::OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error)
{
	// Cleanup right away
	OP_DELETE(m_host_resolver);
	m_host_resolver = NULL;
}

/***********************************************************************************
**
**  TextIsNotUrl - used by TabTextContent
**
***********************************************************************************/
BOOL OpAddressDropDown::TextIsNotUrl(const OpStringC& text, OpWindowCommander* windowcommander)
{
	OpString url;
	OpStatus::Ignore(WindowCommanderProxy::GetMovedToURL(windowcommander).GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url));
	return (text.Compare(url) != 0);
}

/***********************************************************************************
**
**  ShowDropdown
**
***********************************************************************************/
void OpAddressDropDown::ShowDropdown(BOOL show)
{
	// we want to process search results as long as the dropdown is open
	m_process_search_results = show;
	
	OpTreeViewDropdown::ShowDropdown(show);
}

/***********************************************************************************
**
**  OnClear
**
***********************************************************************************/
void OpAddressDropDown::OnClear()
{
	DeleteItems(m_items_to_delete);
	OP_DELETE(m_search_for_item);
	m_search_for_item = NULL;
}

void OpAddressDropDown::OnDropdownMenu(OpWidget *widget, BOOL show)
{
	if (show)
	{
		//--------------------------------------------------
		// Initialize the dropdown :
		//--------------------------------------------------
		RETURN_VOID_IF_ERROR(InitTreeViewDropdown("Addressfield Treeview Window Skin"));
		OP_ASSERT(m_items_to_delete.GetCount() == 0); // It should be empty now
		GetTreeView()->SetOnlyShowAssociatedItemOnHover(TRUE);
		GetTreeView()->SetAllowWrappingOnScrolling(TRUE);

		//--------------------------------------------------
		// Make sure we have a model :
		//--------------------------------------------------

		// The dropdown is responsible for deleting the items
		BOOL items_deletable = TRUE;
		HistoryAutocompleteModel *tree_model = OP_NEW(HistoryAutocompleteModel, ());
		if (!tree_model)
			return;
		SetModel(tree_model, items_deletable);

		uni_char * address = directHistory->GetFirst();
		int num_items = 0;

		while (address)
		{
			OpString key;
			OpString value;
			SearchEngineManager::SplitKeyValue(address, key, value);

			SimpleAutocompleteItem* simple_item = OP_NEW(SimpleAutocompleteItem, (GetModel()));
			if (!simple_item)
				return;

			simple_item->SetAddress(address);
			simple_item->SetItemData(1, address, 0, 0);
			simple_item->SetBadgeWidth(m_protocol_button->GetWidth());
			int color = SkinUtils::GetTextColor("Address DropDown URL Label");
			if (color >= 0)
			{
				simple_item->SetItemColor(1, color);
			}
			simple_item->SetUserDeletable(TRUE);
			simple_item->SetInDirectHistory(TRUE);
			simple_item->SetIsTypedHistory();

			GetModel()->AddLast(simple_item, -1);

			simple_item->SetFavIconKey(address);
			simple_item->SetItemImage("Window Document Icon");

			num_items++;
			address = directHistory->GetNext();
		}

		ShowDropdown(num_items > 0);
	}
	else
	{
		Clear();
	}
}

/************************************************************************************/

bool OpAddressDropDown::ActivateShowAllItem(bool mouse_invoked)
{
	HistoryAutocompleteModelItem* item = GetSelectedItem();
	if (item && item->IsShowAllItem())
	{
		// Remove "show all" item and reparent all children to its parent.
		GetModel()->Remove(item->GetIndex(), FALSE);

		// Show again to automatically resize the dropdown to fit more items
		ShowDropdown(TRUE);

		// Call OnChange so we update info to the new selected item.
		if (!mouse_invoked)
			OnChange(GetTreeView(), FALSE);

		return true;
	}
	return false;
}

/************************************************************************************/

OP_STATUS OpAddressDropDown::GetFullAddress(OpString& text) 
{ 
	// If the edit field has focus or there is no text use the edit field otherwise
	// go for the full adress
	if (GetFocused() == edit || !m_address.HasContent() || GetMisMatched())
	{
		RETURN_IF_ERROR(GetText(text));
		return text.Insert(0, m_url_proto_www_prefix);
	}

	text.Set(m_address.GetFullAddress()); 
	return OpStatus::OK;
}

void OpAddressDropDown::GetQueryField(OpString& text)
{
	if (GetMisMatched())
		text.Empty();
	else
		text.Set(m_address.GetQueryField());
}

void OpAddressDropDown::GetProtocolField(OpString& text)
{	
	if (GetMisMatched())
		text.Empty();
	else
		text.Set(m_address.GetProtocol());
}

void OpAddressDropDown::GetHighlightDomain(OpString& text)
{
	if (edit->string.m_highlight_len != -1 && edit->string.m_highlight_ofs != -1)
		text.Set(edit->string.Get() + edit->string.m_highlight_ofs, edit->string.m_highlight_len);
	else
		text.Empty();
}

OP_STATUS OpAddressDropDown::GetText(OpString& text)
{
	RETURN_IF_ERROR(OpDropDown::GetText(text));

	if (text.Length() > 4096)
			text[4096] = 0;
	return OpStatus::OK;
}

OP_STATUS OpAddressDropDown::EmulatePaste(OpString& text)
{
	if (edit && edit->IsFocused())
	{
		OpString edit_text;
		RETURN_IF_ERROR(edit->GetText(edit_text));
		INT32 start, stop;
		SELECTION_DIRECTION direction;
		edit->GetSelection(start, stop, direction);
		INT32 pos = edit->GetCaretOffset();
		if (start >= 0 && stop > start)
		{
			if (pos > start)
				pos -= pos > stop ? stop-start : pos-start;
			edit_text.Delete(start, stop-start);
		}
		RETURN_IF_ERROR(edit_text.Insert(pos, text));
		RETURN_IF_ERROR(text.Set(edit_text));
	}
	return OpStatus::OK;
}

bool OpAddressDropDown::IsOnBottom() const
{
	OpWidget* parent = GetParent();
	if (parent && parent->GetType() == WIDGET_TYPE_TOOLBAR)
	{
		OpToolbar* toolbar = static_cast<OpToolbar*>(parent);
		if (toolbar->GetAlignment() == OpBar::ALIGNMENT_BOTTOM)
		{
			return true;
		}
	}
	return false;
}

/************************************************************************************/

BOOL OpAddressDropDown::OnBeforeInvokeAction(BOOL invoked_on_user_typed_string)
{
	if (ActivateShowAllItem(true))
		return FALSE;
	return TRUE;
}

/***********************************************************************************
**
**	OnInvokeAction - OpTreeViewDropdown Hook
**
***********************************************************************************/

void OpAddressDropDown::OnInvokeAction(BOOL invoked_on_user_typed_string)
{
	if (GetSelectedItem() && GetSelectedItem()->IsShowAllItem())
		return; // don't save stuff or start intranet testing if show all item is invoked.

	OpString text;
	GetText(text);

	BOOL is_nick = g_hotlist_manager->HasNickname(text, NULL);

	if (g_searchEngineManager->IsIntranet(text.CStr()) &&
		!is_nick &&
		!g_searchEngineManager->IsDetectedIntranetHost(text.CStr()))
	{
		StartIntranetTest(text.CStr());
	}

	// If the action is being invoked on the string the user
	// typed/pasted then it should be added to typed history
	//--------------------

	if (!invoked_on_user_typed_string && DropdownIsOpen())
	{
		HistoryAutocompleteModelItem * item = GetSelectedItem();
		if (item && item->GetForceAddHistory())
			invoked_on_user_typed_string = TRUE;
	}

	DirectHistory::ItemType type = DirectHistory::TEXT_TYPE;

	if (!invoked_on_user_typed_string)
		type = DirectHistory::SELECTED_TYPE;
	
	// Save type to the lower 8 bits and m_in_addressbar to the 9th bit
	if (GetAction())
	{
		OpString full_url;
		GetFullAddress(full_url);
		GetAction()->SetActionData(((m_in_addressbar ? TRUE:FALSE) << 8) + type);
		GetAction()->SetActionDataString(full_url);

		//SetTitle
		HistoryAutocompleteModelItem* item = GetSelectedItem();
		if (item)
		{
			OpString str;
			item->GetTitle(str); //Allow empty string to pass as a parameter
			GetAction()->SetActionDataStringParameter(str);
		}
	}
}

/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpAddressDropDown::OnDragStart(OpWidget* widget,
									INT32 pos,
									INT32 x,
									INT32 y)
{
	DesktopDragObject* drag_object = GetDragObject(g_application->IsDragCustomizingAllowed() ?
											  DRAG_TYPE_ADDRESS_DROPDOWN : DRAG_TYPE_LINK, x, y);
	if(!drag_object)
		return;

	if (widget == m_protocol_button)
	{
		if ( m_tab_cursor->GetWindowCommander() )
		{
			OpWindowCommander *win_comm = m_tab_cursor->GetWindowCommander();
			drag_object->SetURL(win_comm->GetCurrentURL(FALSE));
			drag_object->SetTitle(win_comm->GetCurrentTitle());

			OpString description;
			WindowCommanderProxy::GetDescription(m_tab_cursor->GetWindowCommander(), description);
			DragDrop_Data_Utils::SetText(drag_object, description.CStr());
		}
	}
	if (g_application->IsDragCustomizingAllowed())
	{
		drag_object->SetObject(this);
	}
	g_drag_manager->StartDrag(drag_object, NULL, FALSE);
}

/***********************************************************************************
** GetTextForSelectedItem
**
**
***********************************************************************************/

OP_STATUS OpAddressDropDown::GetTextForSelectedItem(OpString& item_text)
{
	HistoryAutocompleteModelItem* item = GetSelectedItem();

	if (!item)
		return OpStatus::ERR;

	if (m_use_display_url && item->HasDisplayAddress())
	{
		RETURN_IF_ERROR(item->GetAddress(m_actual_url));
		return item->GetDisplayAddress(item_text);
	}
	else
	{
		return item->GetAddress(item_text);
	}
}

void OpAddressDropDown::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	TipsConfigGeneralInfo tip_info;
	if (g_tips_manager && g_tips_manager->CanTipBeDisplayed(UNI_L("Addressbar General Search Tip"), tip_info) 
		|| g_tips_manager->CanTipBeDisplayed(UNI_L("Addressbar Search Tip"), tip_info))
	{
		OpStatus::Ignore(g_tips_manager->GrabTipsToken(tip_info));
	}

	if (m_dropdown_button)
		m_dropdown_button->OnMouseLeave();
}

/***********************************************************************************
** OpTabCursor::Listener callback function
**
**
***********************************************************************************/
void OpAddressDropDown::OnTabInfoChanged()
{
	UpdateButtons();
}

/***********************************************************************************
** OpTabCursor::Listener callback function
**
**
***********************************************************************************/
void OpAddressDropDown::OnTabLoading()
{
	UpdateButtons();
}

/***********************************************************************************
** OpTabCursor::Listener callback function
**
**
***********************************************************************************/
void OpAddressDropDown::OnTabChanged()
{
	//GetForegroundSkin()->SetWidgetImage(m_tab_cursor->GetWidgetImage());
}

/***********************************************************************************
** OpTabCursor::Listener callback function
**
** We are changing tabs, store any user text if there is any in the urlfield,
** clear the ui and find out what we need to show for the new tab.
***********************************************************************************/
void OpAddressDropDown::OnTabWindowChanging(DocumentDesktopWindow* old_target_window,
											DocumentDesktopWindow* new_target_window)
{
	OpWindowCommander* old_window_commander = old_target_window ? old_target_window->GetWindowCommander() : NULL;
	OpWindowCommander* new_window_commander = new_target_window ? new_target_window->GetWindowCommander() : NULL;

	OnPageWindowChanging(old_window_commander, new_window_commander);
}

void OpAddressDropDown::OnPageWindowChanging(OpWindowCommander* old_window_commander,
											 OpWindowCommander* new_window_commander)
{
	ForwardWorkspace();

	if(!new_window_commander)
	{
		SetText(NULL);
	}
	else if (OpStatus::IsError(m_text_content.PopTab(new_window_commander)))
	{
		OpString url;
		OpStatus::Ignore(WindowCommanderProxy::GetMovedToURL(new_window_commander).GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url));
		SetText(url.CStr());
	}

	edit->UnsetForegroundColor();

	UpdateButtons();
}

/***********************************************************************************
** OpTabCursor::Listener callback function
**
**
***********************************************************************************/
void OpAddressDropDown::OnTabClosing(DocumentDesktopWindow* desktop_window)
{
	OpWindowCommander* window_commander = desktop_window ? desktop_window->GetWindowCommander() : NULL;
	OnPageClosing(window_commander);
}

void OpAddressDropDown::OnPageClosing(OpWindowCommander* window_commander)
{
	if(!window_commander)
		return;

	OpString old_text;
	OpStatus::Ignore(m_text_content.RemoveContent(window_commander, old_text));
}

/***********************************************************************************
** OpTabCursor::Listener callback function
**
**
***********************************************************************************/
void OpAddressDropDown::OnTabURLChanged(DocumentDesktopWindow* document_window,
										const OpStringC & url)
{
	OpWindowCommander* window_commander = document_window ? document_window->GetWindowCommander() : NULL;
	OnPageURLChanged(window_commander, url);
}

void OpAddressDropDown::OnPageURLChanged(OpWindowCommander* window_commander, const OpStringC & url)
{
	if(!m_tab_cursor->IsTargetWindow(window_commander))
	{
		OpStatus::Ignore(m_text_content.UpdateContent(window_commander, url));
		return;
	}

	// see SetIgnoreEditFocus() description for more details
	if (!edit->IsFocused() || m_ignore_edit_focus)
	{
		SetText(url.CStr());
		edit->UnsetForegroundColor();

		// The url in the addressbar is updated to match the page in the window.
		// The url might be set to blank temporarily (when going back to speeddial).
		// There is then a mismatch between the url displayed and the document loaded
		m_url_page_mismatch = url.IsEmpty();
		m_ignore_edit_focus = false;
	}
	else
	{
		// The url in the addressbar is not updated to match the page in the window.
		// The security button must be hidden to avoid confusion about the server name
		// and and security level. Bug 226176.
		// But we must also keep in mind that this callback might come without the url
		// actually changing (for instance on reload). If that is the case, there is
		// no mismatch after all. Bug 289608.
		OpString text;
		this->GetText(text);
		if (text.Compare(url))
			m_url_page_mismatch = TRUE;
	}

	UpdateAddressFieldHighlight();
	UpdateButtons(TRUE);
}

void OpAddressDropDown::StartSearchSuggest()
{
	m_mh->RemoveDelayedMessage(MSG_QUICK_START_SEARCH_SUGGESTIONS, 0, 0);
	int n = 2 << m_query_error_counter;
	if (n > 25)
		n = 25;
	m_mh->PostDelayedMessage(MSG_QUICK_START_SEARCH_SUGGESTIONS, 0, 0, 100 * n);
}

void OpAddressDropDown::OnQueryComplete(const OpStringC& search_id, OpVector<SearchSuggestEntry>& entries)
{
	OpString filter;

	m_search_suggest_in_progress = FALSE;

	if(!GetModel())
		return;

	UINT32 cnt;

	// add a maximum of MAX_SUGGESTIONS_ADDRESS_FIELD_SEARCH hits to the drop down
	UINT32 max_entries = min(unsigned(MAX_SUGGESTIONS_ADDRESS_FIELD_SEARCH), entries.GetCount());

	// reset query error counter, as this attempt has been successful
	m_query_error_counter = 0;

	GetModel()->RemoveAll();
	DeleteItems(m_search_suggestions_items);
	for(cnt = 0; cnt < max_entries; cnt++)
	{
		SearchSuggestEntry *entry = entries.Get(cnt);
		if(entry)
		{
			SearchSuggestionAutocompleteItem* suggest_item = OP_NEW(SearchSuggestionAutocompleteItem, (GetModel()));
			if(!suggest_item)
				continue;

			suggest_item->SetSearchSuggestion(entry->GetData());
			suggest_item->SetSearchProvider(search_id.CStr());
			suggest_item->SetBadgeWidth(m_protocol_button->GetWidth());
			suggest_item->SetHighlightText(m_search_phrase);
			if (OpStatus::IsError(m_search_suggestions_items.Add(suggest_item)))
			{
				OP_DELETE(suggest_item);
			}
		}
	}
	PopulateDropdown(false);
}

void OpAddressDropDown::PrefChanged(OpPrefsCollection::Collections id, int pref,int newvalue)
{
	if (OpPrefsCollection::UI == id)
	{
		if (pref == PrefsCollectionUI::HideURLParameter || pref == PrefsCollectionUI::ShowFullURL)
		{
			// Update the url if it wasn't edited
			if (GetFocused() != edit && !m_url_page_mismatch)
			{
				OpString str;
				str.Set(m_address.GetFullAddress()); // we're updating this object so save the text first
				SetText(str);
			}

			if (pref == PrefsCollectionUI::ShowFullURL)
			{
				bool vis = newvalue == 0 && GetFocused() != edit;
				m_protocol_button->SetTextVisibility(vis);
			}
		}
		
		if(pref == 	PrefsCollectionUI::ShowDropdownButtonInAddressfield)
		{
			m_dropdown_button->SetVisibility(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowDropdownButtonInAddressfield));
		}
	}
}

OP_STATUS OpAddressDropDown::GetFormattedSuggestedSearch(SearchSuggestionAutocompleteItem* suggest_item, OpString& formated_string)
{
	SearchTemplate *search_template = g_searchEngineManager->GetByUniqueGUID(suggest_item->GetSearchProvider());
	if(search_template)
	{
		OpString guid;

		RETURN_IF_ERROR(search_template->GetUniqueGUID(guid));

		// construct a search string suitable for history
		return formated_string.AppendFormat(UNI_L("%s %s"), search_template->GetKey().CStr(), suggest_item->GetSearchSuggestion());
	}
	return OpStatus::OK;
}

void OpAddressDropDown::UpdateAddressFieldHighlight(BOOL leave, BOOL no_delay)
{
	OpString text;
	GetText(text);

	BOOL highlighted = edit->string.GetHighlightType() != OpWidgetString::None;

	BOOL should_highlight = m_address.GetHighlight()
		&& !m_url_page_mismatch									// don't when url doesn't match the page
		&& GetFocused() != edit									// don't when edit is focused
#ifdef REMOVE_HIGHLIGHT_ONHOVER
		&& (g_widget_globals->hover_widget != edit || leave)		// don't when hovered 
																// in OnMouseLeave the edit may still be the hover_widget, but will soon not
#endif
		;

	// Highlighting happens immediately if needed, removing the highlight
	// when hovering is delayed for a short while to prevent url flashing when
	// the user is just moving mouse over the address bar, not actually hovering.
	if (!should_highlight && highlighted && !no_delay)
	{
		m_highlight_delayed_trigger.InvokeTrigger(OpDelayedTrigger::INVOKE_DELAY);
	}
	else
	{
		// Update the range to highlight
		edit->string.SetHighlightType(should_highlight ? OpWidgetString::RootDomain : OpWidgetString::None);
		edit->string.NeedUpdate();
		edit->InvalidateAll();
	}
}

void OpAddressDropDown::RestoreOriginalAddress()
{
	if (m_address.HasContent())
	{
		// The edit get focused, show the full address(including protocol)
		OpTreeViewDropdown::SetText(m_address.GetFullAddress());
	}
}

OP_STATUS OpAddressDropDown::SetText(const uni_char* text)
{
	m_url_proto_www_prefix.Empty();
	m_address.Clear();

	// Return if URL is maskable from display
	if (MaskUrlFromDisplay(text))
		return OpStatus::OK;

	const uni_char* current_loading_url = GetWindowCommander() ? GetWindowCommander()->GetLoadingURL(FALSE) : NULL;

	// Text changes, update m_url_page_mismatch
	if (current_loading_url && !ShallMaskUrlFromDisplay(current_loading_url))
		m_address.SetAddress(current_loading_url);

	m_url_page_mismatch = !m_address.Compare(text);
	m_actual_url.Empty();

	// We need to know the status of the button to determine whether show the protocol or not
	UpdateProtocolStatus();
	if (GetFocused() == edit)
	{
		m_protocol_button->SetTextVisibility(FALSE);
	}

	if (!text)
		OpTreeViewDropdown::SetText(NULL);
	else if (!m_in_addressbar || GetFocused() == edit || m_url_page_mismatch) // Don't hide protocol when not needed
		OpTreeViewDropdown::SetText(text);
	else
	{
		OpString show_address;
		// If there is a page and the badge is Empty then probably the page is
		// an loading failed page, in this case we don't want the Web badge
		// or hide protocol
		if (m_protocol_button->HideProtocol())
			m_address.GetShowAddress(show_address);
		else
			show_address.Set(m_address.GetFullAddress());

		OpTreeViewDropdown::SetText(show_address);

		edit->SetCaretOffset(0); // Compensate for the behavior in OpTreeViewDropdown::SetText
	}

	UpdateAddressFieldHighlight(FALSE, TRUE);

#if defined(_X11_SELECTION_POLICY_)
	m_caret_offset = -1;
#endif

	return OpStatus::OK;
}

UINT OpAddressDropDown::GetProtocolWidth()
{
	OpString text, tmp;
	INT text_len, text_with_protocol_len;
	GetText(text);

	// Width without protocol
	OpString show_address;
	m_address.GetShowAddress(show_address);
	tmp.Set(show_address);
	edit->string.Set(tmp, edit);
	text_len = edit->GetStringWidth();

	// Width with protocol
	tmp.Insert(0, m_address.GetProtocol());
	edit->string.Set(tmp, edit);
	text_with_protocol_len = edit->GetStringWidth();

	edit->string.Set(text, edit);

	return text_with_protocol_len - text_len;
}

void OpAddressDropDown::EnableFavoritesButton(bool show_fav_btn)
{ 
	m_show_favorites_button = show_fav_btn; 
	if (m_show_favorites_button && !m_url_fav_menu)
		OpStatus::Ignore(InitializeBookmarkMenu());
}

OP_STATUS OpAddressDropDown::ListPopupWindowWidgets(OpVector<OpWidget> &widgets) 
{
#ifdef SUPPORT_START_BAR
	if (!m_popup_window)
		return OpStatus::ERR;
	
	OpWidget* widget = m_popup_window->GetWidgetContainer()->GetRoot(); 
	if (!widget)
	{
		return OpStatus::ERR;
	}
	
	OpWidget* child = widget->GetFirstChild();
	if (!child)
		return OpStatus::ERR;
	do {
		widgets.Add(child);
		ListWidgets(child, widgets);
	} while (child = child->GetNextSibling());
	
	return OpStatus::OK;
}

OP_STATUS OpAddressDropDown::ListWidgets(OpWidget* widget, OpVector<OpWidget> &widgets)
{
	if (!widget)
		return OpStatus::ERR;
	OpWidget* child = widget->GetFirstChild();
	if (!child)
		return OpStatus::ERR;
	do {
		widgets.Add(child);
		ListWidgets(child, widgets);
	} while (child = child->GetNextSibling());
#endif // SUPPORT_START_BAR
	return OpStatus::OK;
}

OP_STATUS OpAddressDropDown::GetTypedText(OpString& typed_text)
{
	if (m_is_quick_completed)
	{
		RETURN_IF_ERROR(typed_text.Set(m_text_before_quick_completion));
	}
	else
	{
		RETURN_IF_ERROR(GetText(typed_text));
	}
	return OpStatus::OK;
}

void OpAddressDropDown::UpdateProtocolStatus()
{
	OpWindowCommander* win_comm = m_tab_cursor->GetWindowCommander();

	BOOL hide_protocol = m_protocol_button->HideProtocol();
	m_protocol_button->UpdateStatus(win_comm, m_url_page_mismatch);
	
	if (win_comm)
	{
		// When loading failed the badge switches from a normal size Web to minimal size Empty,
		// the protocol is hidden in this case so we want to show it again
		if (hide_protocol != m_protocol_button->HideProtocol())
		{
			// Update url, add or remove protocol
			if (!GetMisMatched() && GetFocused() != edit)
			{
				/* The family of WindowCommander's GetFooURL() methods use the
				   same buffer for strings they return meaning that they will
				   overwrite the string that was obtained in the previous call,
				   fooling comparisons that are done throughout this code. We
				   need to copy the string. */
				OpString current_url;
				if (OpStatus::IsSuccess(current_url.Set(win_comm->GetCurrentURL(FALSE))))
					SetText(current_url.CStr());
			}
		}
	}
}

void OpAddressDropDown::UpdateFavoritesFG(bool reflow)
{
	if (!m_url_fav_menu)
		return;

	bool is_item_bookmarked = false;
	is_item_bookmarked = IsAddressInBookmarkList();
	if (!is_item_bookmarked)
		is_item_bookmarked = IsAddressInSpeedDialList();

	const char *updated_img;
	if (g_desktop_op_system_info->SupportsContentSharing())
		updated_img = "SharePage";
	else
		updated_img = is_item_bookmarked ? "Bookmarked URL" : "NonBookmarked URL";
	OpStringC8 img(m_url_fav_menu->GetForegroundSkin()->GetImage());
	if (img.CompareI(updated_img, strlen(updated_img)))
	{
		// Update button image
		m_url_fav_menu->GetForegroundSkin()->SetImage(updated_img);
		if (reflow)
			Relayout();
	}
}

void OpAddressDropDown::CloseOverlayDialog()
{
	if (m_addressbar_overlay_ctl)
	{
		m_addressbar_overlay_ctl->CancelDialog();
	}
}

void OpAddressDropDown::DisplayOverlayDialog(const OpVector<OpPermissionListener::ConsentInformation>& info)
{
	DocumentDesktopWindow* window = g_application->GetActiveDocumentDesktopWindow();
	if (window && !window->PermissionControllerDisplayed() && m_protocol_button && !m_addressbar_overlay_ctl)
	{
		OpWidget* root = GetWidgetContainer()->GetRoot();
		m_addressbar_overlay_ctl = OP_NEW(AddressbarOverlayController, (*this, *root, GetWindowCommander(), info));
		if (!m_addressbar_overlay_ctl)
		{
			return;
		}
		m_addressbar_overlay_ctl->SetListener(this);
		if (OpStatus::IsError(ShowDialog(m_addressbar_overlay_ctl, g_global_ui_context, GetParentDesktopWindow())))
		{
			return;
		}
		m_addressbar_overlay_ctl->SetFocus(FOCUS_REASON_MOUSE);
		if (IsOnBottom())
		{
			m_addressbar_overlay_ctl->SetIsOnBottom();
		}
	}
}

void OpAddressDropDown::OnShowFavoritesDialog()
{
	m_protocol_button->HideText();
	RestoreOriginalAddress();
}

void OpAddressDropDown::OnCloseFavoritesDialog()
{
	if (GetFocused() == edit)
		return;

	m_protocol_button->ShowText();

	/* When url's fav menu loses focus the short-address will be displayed only 
	if current displayed address wasn't edited */
	OpString text;
	RETURN_VOID_IF_ERROR(GetText(text));
	if (GetWindowCommander())
	{
		const uni_char* current_url = GetWindowCommander()->GetCurrentURL(FALSE);
		if (text.Compare(current_url) == 0)
		{
			/* The family of WindowCommander's GetFooURL() methods use the same
			   buffer for strings they return meaning that they will overwrite
			   the string that was obtained in the previous call, fooling
			   comparisons that are done throughout this code. We need to copy
			   the string. */
			OpString url;
			if (OpStatus::IsSuccess(url.Set(current_url)))
				SetText(url.CStr());
		}
	}

	// Set focus to document
	g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);

}

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// AddressHighlight
//////////////////////////////////////////////////////////////////////////////////////////

void AddressHighlight::Clear()
{
	m_protocol.Empty();
	m_short_address.Empty();
	m_original_address.Empty();
}

void AddressHighlight::SetAddress(OpStringC url)
{
	Clear();

	m_original_address.Set(url);
	m_short_address.Set(url);
	m_highlight = TRUE;

	UINT protocol_len = 0;
	BOOL remove_parameter = FALSE;
	if (m_short_address.CompareI(UNI_L("http://"), 7) == 0)
	{
		protocol_len = 7;
		remove_parameter = TRUE;
	}
	else if (m_short_address.CompareI(UNI_L("https://"), 8) == 0)
	{
		protocol_len = 8;
		remove_parameter = TRUE;
	}
	else if (m_short_address.CompareI(UNI_L("ftp://"), 6) == 0)
	{
		protocol_len = 6;
	}
	else if (m_short_address.CompareI(UNI_L("opera:"), 6) == 0 
		&& m_short_address.CompareI(UNI_L("opera:blank")) != 0) // we show Empty badge for this
	{
		protocol_len = 6;
		m_highlight = FALSE;
	}
	else if (m_short_address.CompareI(UNI_L("file://"), 7) == 0)
	{
		protocol_len = 7;
		m_highlight = FALSE;
	}
	else if (m_short_address.CompareI(UNI_L("attachment:"), 11) == 0)
	{
		protocol_len = 11;
		m_highlight = FALSE;
	}
	else if (m_short_address.CompareI(UNI_L("widget://"), 9) == 0)
	{
		protocol_len = 0;
		m_highlight = FALSE;
		SetAddressForExtensions();
	}

	// Remove the parameters for http and https
	if (remove_parameter && g_pcui->GetIntegerPref(PrefsCollectionUI::HideURLParameter))
	{
		int parameter_start = m_short_address.Find(UNI_L("?"));
		if (parameter_start != -1)
		{
			m_query_field.Set(m_short_address.CStr() + parameter_start);
			m_short_address.Delete(parameter_start);
		}
	}

	// Remove the protocol. http://opera.com/ -> opera.com/
	m_protocol.Set(m_short_address, protocol_len);
	m_short_address.Delete(0, protocol_len);

	// Remove the ending '/'.  i.e. opera.com/ -> opera.com
	if (m_short_address.Length() && m_short_address.Find("/") == m_short_address.Length() - 1)
		m_short_address.CStr()[m_short_address.Length() - 1] = 0;
}


/***********************************************************************************
 ** DesktopHistoryModelListener callback function
 **
 **
 ***********************************************************************************/
void OpAddressDropDown::OnDesktopHistoryModelItemDeleted(HistoryModelItem* item)
{
	HistoryAutocompleteModelItem *simple_item = item->GetHistoryAutocompleteModelItem();
	if (simple_item)
	{
		HistoryAutocompleteModel *model = GetModel();
		if (model)
		{
			INT32 remove_pos;
			remove_pos = model->GetIndexByItem(simple_item);
			if (remove_pos >= 0)
				model->Remove(remove_pos);
		}
	}
}	


void AddressHighlight::GetShowAddress(OpString& address)
{
	BOOL show_full_url = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullURL);

	if (show_full_url)
		address.Set(m_original_address);
	else
		address.Set(m_short_address);
}

BOOL AddressHighlight::Compare(OpStringC url)
{
	if (m_original_address.CompareI(url) == 0)
		return TRUE;

	if (m_short_address.CompareI(url) == 0)
		return TRUE;
	
	return FALSE;
}


uni_char* OpAddressDropDown::DeduceProtocolOrSubdomain(const OpString& current_text, uni_char* address)
{
	// Strip away parts that shouldn't matter. This gives the user hits on modified urls too,
	// since modifying a incorrectly typed url also has the http://www. part (but the user usually doesn't type it).
	const uni_char* http = UNI_L("http://");
	const uni_char* https = UNI_L("https://");
	const uni_char* www = UNI_L("www.");

	if (uni_strncmp(current_text.CStr(), http, MIN(current_text.Length(), 7)) != 0 &&
		uni_strncmp(address, http, 7) == 0)
	{
		m_url_proto_www_prefix.Set(http);
		address += 7;
	}
	else if (uni_strncmp(current_text.CStr(), https, MIN(current_text.Length(), 8)) != 0 &&
			 uni_strncmp(address, https, 8) == 0)
	{
		m_url_proto_www_prefix.Set(https);
		address += 8;
	}

	if (uni_strncmp(current_text.CStr(), www, MIN(current_text.Length(), 4)) != 0 &&
		uni_strncmp(address, www, 4) == 0)
	{
		m_url_proto_www_prefix.Append(www);
		address += 4;
	}
	return address;
}

void AddressHighlight::SetAddressForExtensions()
{
	int wuid_end = m_short_address.Find(UNI_L("/"), 9);
	if (wuid_end != KNotFound)
	{
		OpGadget * dst = g_gadget_manager->FindGadget(GADGET_FIND_BY_ID, m_short_address.SubString(9, wuid_end - 9).CStr(), FALSE);
		OpString url;
		url.Append(m_short_address.SubString(wuid_end+1).CStr());
		m_short_address.Empty();

		if (dst)
		{
			OpString format;
			if (url.Compare(UNI_L("options.html")) == 0)
				g_languageManager->GetString(Str::S_ADDRESS_BUTTON_ADDON_OPTIONS, format);
			else
				g_languageManager->GetString(Str::S_ADDRESS_BUTTON_ADDON_UNKNOWN, format);

			const uni_char* text = dst->GetAttribute(WIDGET_NAME_SHORT);
			if (!text || !*text) text = dst->GetAttribute(WIDGET_NAME_TEXT);
			if (text && *text)
			{
				m_short_address.AppendFormat(format.CStr(), text, url.CStr());
			}
			else
			{
				OpString path;
				dst->GetGadgetPath(path);
				int leaf = path.FindLastOf(PATHSEPCHAR);
				if (leaf == -1)
					leaf = path.FindLastOf(UNI_L('/'));

				m_short_address.AppendFormat(format.CStr(), path.SubString(leaf + 1).CStr(), url.CStr()); //will take whole path, if there are no separators...
			}
		}
		else
			m_short_address.Append(url);

		m_original_address.Empty();
	}
}

void OpAddressDropDown::OnDialogClosing(DialogContext* context)
{
	if (context == m_addressbar_overlay_ctl)
	{
		m_addressbar_overlay_ctl = NULL;
	}
	else if (context == m_favorites_overlay_ctl)
	{
		OnCloseFavoritesDialog();
		m_favorites_overlay_ctl = NULL;
		m_url_fav_menu->SetDialogContext(NULL);
	}
}

bool OpAddressDropDown::IsAddressInBookmarkList()
{
	OpString doc_url;
	GetFullAddress(doc_url);
	if (doc_url.IsEmpty())
		return false;

	return g_hotlist_manager->GetBookmarksModel()->GetByURL(doc_url, false, true) != NULL;
}

bool OpAddressDropDown::IsAddressInSpeedDialList()
{
	OpString doc_url;
	GetFullAddress(doc_url);
	if (doc_url.IsEmpty())
		return false;

	return g_speeddial_manager->FindSpeedDialByUrl(doc_url, true) != -1;
}

OpAddressDropDown::InitInfo::InitInfo()
	: m_max_lines(1)
	, m_show_dropdown_on_activation(false)
	, m_invoke_action_on_click(false)
	, m_open_page_in_tab(false)
	, m_tab_mode(false)
	, m_shall_show_addr_bar_badge(false)
	, m_use_display_url(true)
	, m_show_search_suggestions(false)
	, m_show_speed_dial_pages(false)
{

}

void OpAddressDropDown::InitAddressDropDown(OpAddressDropDown::InitInfo &info)
{
	SetMaxLines(info.m_max_lines);
	SetShowDropdownOnActivation(info.m_show_dropdown_on_activation);
	SetInvokeActionOnClick(info.m_invoke_action_on_click);
	SetOpenInTab(info.m_open_page_in_tab);
	SetTabMode(info.m_tab_mode);
	EnableAddressBarBadge(info.m_shall_show_addr_bar_badge);
	SetUseDisplayUrl(info.m_use_display_url);
	m_show_search_suggestions = info.m_show_search_suggestions;
	m_show_speed_dial_pages = info.m_show_speed_dial_pages;

	// Get current page URL
	if (info.m_url_string.HasContent())
		OpStatus::Ignore(SetText(info.m_url_string));

	/*
	The styling of the edit field should be the same as for the title edit
	field. It seems that using the same skin for op_drop_down as for
	title_edit makes this work without breaking anything.  Changing the skin
	of op_drop_down->edit has no effect.  See DSK-330160.

	If this does break something, we should probably make a new skin for the
	"speed dial config addressfield dropdown", which copies the necessary
	data from "Edit Skin", "Addressfield Dropdown Skin" and "Dropdown Skin".
	*/
	GetBorderSkin()->SetImage("Edit Skin");
}

void OpAddressDropDown::BlockFocus()
{
	if (m_block_focus_timer == NULL)
		m_block_focus_timer = OP_NEW(OpTimer, ());

	if (m_block_focus_timer && !m_block_focus_timer->IsStarted())
	{
		m_block_focus_timer->SetTimerListener(this);
		m_block_focus_timer->Start(400);
		if(edit)
			edit->SetDead(TRUE);
	}
}

void OpAddressDropDown::OnTimeOut(OpTimer* timer)
{
	if(m_block_focus_timer == timer)
	{
		OP_DELETE(m_block_focus_timer);
		m_block_focus_timer = NULL;
		if(edit)
			edit->SetDead(FALSE);
	}
	else if (m_notification_timer == timer)
	{
		AnimateBookmarkNotification(false, false);
	}
	else
		OpTreeViewDropdown::OnTimeOut(timer);
}

void OpAddressDropDown::AnimateBookmarkNotification(bool show, bool added)
{
	// ignore text when hiding
	if (show)
	{
		OpString text_to_show;
		g_languageManager->GetString(added ? Str::S_URL_ADDED_NOTIFICATION : Str::S_URL_REMOVED_NOTIFICATION,
									 text_to_show);
		m_bookmark_status_label->SetText(text_to_show);
	}

	QuickAnimationWidget* animation = m_bookmark_status_label->GetAnimation();
	if (animation && animation->IsAnimating())
		g_animation_manager->abortAnimation(animation);

	m_bookmark_status_label->SetVisibility(show);

	m_bookmark_status_label->SetVisibility(TRUE);

	animation = OP_NEW(QuickAnimationWidget,
					   (m_bookmark_status_label,
						show ? ANIM_MOVE_NONE : ANIM_MOVE_SHRINK,
						true,
						show ? ANIM_FADE_IN : ANIM_FADE_OUT));

	if (animation)
	{
		// we might consider depending animation time and timeout on text length
		// but this would require some studies about how fast ppl read
		// text basing on text length and complexity
		g_animation_manager->startAnimation(animation, ANIM_CURVE_LINEAR, 300);

		// if another show/hide notification would occur when current animation
		// is in progress, then we can easly stop current timer, since new timer
		// will be more important then previous one

		m_notification_timer->Stop();

		if (show)
		{
			m_notification_timer->SetTimerListener(this);
			m_notification_timer->Start(900);
		}
	}
}

bool OpAddressDropDown::MaskUrlFromDisplay(const uni_char* url)
{
	if (!ShallMaskUrlFromDisplay(url))
		return false;

	OpTreeViewDropdown::SetText(NULL);
	return true;
}

bool OpAddressDropDown::ShallMaskUrlFromDisplay(const uni_char* url)
{
	if (url && *url)
		return !DocumentView::IsUrlRestrictedForViewFlag(url, DocumentView::ALLOW_SHOW_URL);

	return false;
}
