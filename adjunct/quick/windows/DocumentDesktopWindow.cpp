/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/windows/DocumentDesktopWindow.h"

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/desktop_util/string/stringutils.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/quick/usagereport/UsageReport.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/string/htmlify_more.h"
#include "adjunct/desktop_util/string/i18n.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/managers/DesktopSecurityManager.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/FindTextManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/OperaTurboManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "adjunct/quick/controller/AutoWindowReloadController.h"
#include "adjunct/quick/controller/PermissionPopupController.h"
#include "adjunct/quick/dialogs/CustomizeToolbarDialog.h"
#include "adjunct/m2_ui/dialogs/NewsfeedSubscribeDialog.h"
#include "adjunct/quick/dialogs/PreferencesDialog.h"
#include "adjunct/quick/dialogs/SecurityInformationDialog.h"
#include "adjunct/quick/dialogs/MidClickDialog.h"
#ifdef DOM_GEOLOCATION_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"
#include "adjunct/quick/dialogs/GeolocationPrivacyDialog.h"
#endif // DOM_GEOLOCATION_SUPPORT
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/panels/BookmarksPanel.h"
#include "adjunct/quick/panels/NotesPanel.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick/widgets/OpPluginCrashedBar.h"
#include "adjunct/quick/widgets/OpSearchDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/widgets/OpInfobar.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick_toolkit/contexts/DialogContext.h"
#include "adjunct/quick/widgets/OpStatusField.h"
#include "adjunct/quick/widgets/WebhandlerBar.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#ifdef SUPPORT_VISUAL_ADBLOCK
#include "adjunct/quick/dialogs/ContentBlockDialog.h"
#endif // SUPPORT_VISUAL_ADBLOCK
#include "adjunct/quick/managers/ToolbarManager.h"



#include "modules/display/VisDevListeners.h"
#include "modules/display/styl_man.h"
#include "modules/display/vis_dev.h"
#include "modules/doc/doc.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/formats/uri_escape.h"
#include "modules/forms/form.h"
#include "modules/forms/piforms.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/base/opstatus.h"
#include "modules/hardcore/base/op_types.h"
#include "modules/img/src/imagemanagerimp.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/logdoc/savewithinline.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpWindow.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/style/css.h"
#include "modules/thumbnails/thumbnailmanager.h"
#ifdef THUMBNAILS_CAP_CREATE_THUMBNAIL_GENERATOR
# include "modules/thumbnails/thumbnailgenerator.h"
#endif
#include "modules/url/url2.h"
#include "modules/util/excepts.h"
#include "modules/util/filefun.h"
#include "modules/util/handy.h"
#include "modules/wand/wandmanager.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/widgets/InstallPersonabar.h"
#include "adjunct/quick/widgets/OpCollectionView.h"
#include "adjunct/quick/widgets/OpSpeedDialView.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"
#include "adjunct/quick/widgets/OpWandStoreBar.h"
#include "adjunct/quick/widgets/OpGoToIntranetBar.h"
#include "adjunct/quick/widgets/OpLocalStorageBar.h"
#include "adjunct/quick/widgets/PagebarButton.h"

#if defined(_PRINT_SUPPORT_)
# include "modules/display/prn_info.h"
#endif // _PRINT_SUPPORT_

#if defined(_VALIDATION_SUPPORT_)
#include "adjunct/quick/dialogs/ValidateSourceDialog.h"
#endif

// Action handling
#if defined(_X11_)
#include "platforms/unix/product/x11quick/x11_specific_actions.h"
#elif defined(_UNIX_DESKTOP_)
#include "platforms/unix/product/quick/toplevelwindow.h"
#endif

#ifdef _MACINTOSH_
#include "platforms/mac/model/AppleEventManager.h"
#include "platforms/mac/model/CMacSpecificActions.h"
#include "adjunct/desktop_util/actions/delayed_action.h"
#endif

#include "modules/content_filter/content_filter.h"
#include "modules/ns4plugins/src/plugin.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/src/TransferManagerDownload.h"

#ifdef AB_CAP_HAS_OP_PRIVACY_MODE
#include "modules/about/opprivacymode.h"
#endif

#ifdef PLUGIN_AUTO_INSTALL
#include "adjunct/autoupdate/updatablefile.h"
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/quick/widgets/OpPluginMissingBar.h"
#endif // PLUGIN_AUTO_INSTALL

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/quick/widgets/DownloadExtensionBar.h"

// MacHacksIncluded: Mac Hacks

const unsigned int DocumentDesktopWindow::MAX_PLUGIN_MISSING_BAR_COUNT = 3;

namespace DocumentDesktopWindowConsts
{
	const int PRIORITY_NORMAL = 20;
	const int PRIORITY_ACTIVE = 0;
	const int PRIORITY_IN_STACK = 50;
	const int PRIORITY_ERROR = 100;
}

/***********************************************************************************
**
**	DocumentDesktopWindow
**
***********************************************************************************/

DocumentDesktopWindow::InitInfo::InitInfo()
	: m_show_toolbars(true)
	, m_focus_document(false)
	, m_no_speeddial(false)
	, m_private_browsing(false)
	, m_staged_init(false)
	, m_delayed_addressbar(false)
	, m_window_commander(NULL)
	, m_url_settings(NULL)
{
}

DocumentDesktopWindow::DocumentDesktopWindow(OpWorkspace* parent_workspace,
                                             OpWindow* parent_window,
                                             InitInfo* info /* = NULL */)
	: m_address_bar(NULL)
	, m_progress_bar(NULL)
	, m_popup_progress_bar(NULL)
#ifdef SUPPORT_NAVIGATION_BAR
	, m_navigation_bar(NULL)
#endif // SUPPORT_NAVIGATION_BAR
	, m_view_bar(NULL)
	, m_local_storage_bar(NULL)
	, m_webhandler_bar(NULL)
	, m_wandstore_bar(NULL)
	, m_intranet_bar(NULL)
	, m_plugin_crashed_bar(NULL)
#ifdef SUPPORT_VISUAL_ADBLOCK
	, m_popup_adblock_bar(NULL)
	, m_adblock_edit_mode(FALSE)
	, m_content_to_block(1)
	, m_content_to_unblock(1)
#endif // SUPPORT_VISUAL_ADBLOCK
	, m_permission_controller(NULL)
	, m_document_view(NULL)
	, m_show_toolbars(true)
	, m_is_read_document(false)
	, m_focus_document(false)
	, m_private_browsing(false)
	, m_popup_progress_window(NULL)
	, m_cycles_accesskeys_popupwindow(NULL)
	, m_handled_initial_document_view_activation(FALSE)
#ifdef WEB_TURBO_MODE
	, m_turboTransferredBytes(0)
	, m_turboOriginalTransferredBytes(0)
	, m_prev_turboTransferredBytes(0)
	, m_prev_turboOriginalTransferredBytes(0)
	// Usage report
	, m_transferredBytes(0)
	, m_transferStart(0)
#endif //WEB_TURBO_MODE
	, m_loading_failed(FALSE)
	, m_has_OnPageLoadingFinished_been_called(false)
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	, m_mh(0)
	, m_search_suggest(0)
	, m_search_suggestions_callback(0)
#endif
	, m_current_trust_mode(OpDocumentListener::TRUST_NOT_SET)
#ifdef EXTENSION_SUPPORT
	, m_is_extension_document(MAYBE)
#endif // EXTENSION_SUPPORT
	, m_has_plugin_placeholders(FALSE)
	, m_delayed_title_change(false)
	, m_staged_init(false)
	, m_mockup_icon(NULL)
	, m_doc_history(NULL)
	, m_active_doc_view_index(-1)
{
	InitInfo init_settings;
	if (info)
		init_settings = *info;
	m_show_toolbars    = init_settings.m_show_toolbars;
	m_focus_document   = init_settings.m_focus_document;
	m_private_browsing = init_settings.m_private_browsing;
	m_staged_init      = init_settings.m_staged_init;
	if (init_settings.m_url_settings)
		m_openurl_setting = *init_settings.m_url_settings;

	OP_ASSERT(!(parent_window && parent_workspace));
	if (parent_window)
		init_status = DesktopWindow::Init(OpWindow::STYLE_CHILD, parent_window);
	else
		init_status = DesktopWindow::Init(OpWindow::STYLE_DESKTOP, parent_workspace);
	CHECK_STATUS(init_status);

	if (!m_staged_init)
		init_status = Init(init_settings);
}

OP_STATUS DocumentDesktopWindow::Init(InitInfo& init_settings)
{
	OP_PROFILE_METHOD("Document window initialized");

	/**
	 * Beware:
	 * Changing the order in which widget children are added will change TAB selection order.
	 */
	OpWidget* root_widget = GetWidgetContainer()->GetRoot();

	RETURN_IF_ERROR(OpToolbar::Construct(&m_address_bar, PrefsCollectionUI::AddressbarAlignment));
	root_widget->AddChild(m_address_bar);
	EnableTransparentSkin(GetWorkspace() && GetWorkspace()->IsTransparentSkinsEnabled());

	m_address_bar->SetName("Document Toolbar");
	m_address_bar->SetReportUsageStats(TRUE);
	OpAddressDropDown *dropdown = GetAddressDropdown();
	if (dropdown)
	{
		dropdown->EnableFavoritesButton(true);
		if (init_settings.m_delayed_addressbar)
			dropdown->BlockFocus();
	}

	RETURN_IF_ERROR(OpToolbar::Construct(&m_progress_bar));
	root_widget->AddChild(m_progress_bar);
	m_progress_bar->GetBorderSkin()->SetImage("Progressbar Skin");
	m_progress_bar->SetName("Progress Toolbar");

	RETURN_IF_ERROR(OpToolbar::Construct(&m_popup_progress_bar));
	root_widget->AddChild(m_popup_progress_bar);
	m_popup_progress_bar->GetBorderSkin()->SetImage("Progressbar Popup Skin", "Progressbar Skin");
	m_popup_progress_bar->SetName("Popup Progress Toolbar");
	m_popup_progress_bar->SetVisibility(FALSE);

	OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::PersonalbarInline, root_widget, TRUE);
	if(personalbar)
	{
		personalbar->GetBorderSkin()->SetImage("Personalbar Inline Skin");
		personalbar->SetName("Personalbar Inline");
	}

#ifdef SUPPORT_NAVIGATION_BAR
	RETURN_IF_ERROR(OpToolbar::Construct(&m_navigation_bar, PrefsCollectionUI::NavigationbarAlignment,
													 PrefsCollectionUI::NavigationbarAutoAlignment));
	root_widget->AddChild(m_navigation_bar);
	m_navigation_bar->GetBorderSkin()->SetImage("Navigationbar Skin");
	m_navigation_bar->SetName("Site Navigation Toolbar");
#endif // SUPPORT_NAVIGATION_BAR

	RETURN_IF_ERROR(OpToolbar::Construct(&m_view_bar));
	root_widget->AddChild(m_view_bar);
	m_view_bar->GetBorderSkin()->SetImage("Viewbar Skin");
	m_view_bar->SetName("Document View Toolbar");

	OpFindTextBar* search_bar = GetFindTextBar();
	root_widget->AddChild(search_bar);

#ifdef SUPPORT_VISUAL_ADBLOCK
	RETURN_IF_ERROR(OpInfobar::Construct(&m_popup_adblock_bar));
	root_widget->AddChild(m_popup_adblock_bar);

	RETURN_IF_ERROR(m_popup_adblock_bar->Init("Content Block Toolbar"));
	m_popup_adblock_bar->GetBorderSkin()->SetImage("Content Block Toolbar Skin");
	m_popup_adblock_bar->SetWrapping(OpBar::WRAPPING_NEWLINE);
	m_popup_adblock_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
#endif

	RETURN_IF_ERROR(WebhandlerBar::Construct(&m_webhandler_bar));
	root_widget->AddChild(m_webhandler_bar);

	RETURN_IF_ERROR(m_webhandler_bar->Init("Web Handler Toolbar"));
	m_webhandler_bar->GetBorderSkin()->SetImage("Web Handler Toolbar Skin");
	m_webhandler_bar->SetAlignment(OpBar::ALIGNMENT_OFF);

	RETURN_IF_ERROR(OpButton::Construct(&m_show_addressbar_button));
	root_widget->AddChild(m_show_addressbar_button);
	m_show_addressbar_button->SetListener(this);
	m_show_addressbar_button->SetButtonTypeAndStyle(OpButton::TYPE_TOOLBAR, OpButton::STYLE_TEXT);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	m_show_addressbar_button->AccessibilityPrunesMe(ACCESSIBILITY_PRUNE_WHEN_INVISIBLE);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
	FontAtt font; // Force small font size
	g_op_ui_info->GetUICSSFont(CSS_VALUE_message_box, font);
	UINT32 fontnr = font.GetFontNumber();
	const OpFontInfo* fontinfo = styleManager->GetFontInfo(fontnr);
	m_show_addressbar_button->SetFontInfo(fontinfo, min(font.GetHeight(), 11), font.GetItalic(), font.GetWeight(), JUSTIFY_CENTER);

	RETURN_IF_ERROR(OpWandStoreBar::Construct(&m_wandstore_bar));
	root_widget->AddChild(m_wandstore_bar);

	RETURN_IF_ERROR(OpGoToIntranetBar::Construct(&m_intranet_bar));
	root_widget->AddChild(m_intranet_bar);
	RETURN_IF_ERROR(m_intranet_bar->Init("Go To Intranet Toolbar"));

	RETURN_IF_ERROR(OpLocalStorageBar::Construct(&m_local_storage_bar));
	root_widget->AddChild(m_local_storage_bar);
	RETURN_IF_ERROR(m_local_storage_bar->Init("Local Storage Toolbar"));

	RETURN_IF_ERROR(OpPluginCrashedBar::Construct(&m_plugin_crashed_bar));
	root_widget->AddChild(m_plugin_crashed_bar);
	RETURN_IF_ERROR(m_plugin_crashed_bar->Init());

	if (!m_show_toolbars)
	{
		m_address_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
#ifdef SUPPORT_NAVIGATION_BAR
		m_navigation_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
#endif
		m_view_bar->SetAlignment(OpBar::ALIGNMENT_OFF);

		OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::PersonalbarInline, root_widget, TRUE);
		if(personalbar)
			personalbar->SetAlignment(OpBar::ALIGNMENT_OFF);
		m_wandstore_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
		m_intranet_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
		m_local_storage_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
		m_plugin_crashed_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
	}

	// BrowserView
	RETURN_IF_ERROR(CreateDocumentView(DocumentView::DOCUMENT_TYPE_BROWSER, init_settings.m_window_commander));
	m_document_view = static_cast<OpBrowserView*>(GetDocumentViewFromType(DocumentView::DOCUMENT_TYPE_BROWSER)->GetOpWidget());
	RETURN_OOM_IF_NULL(m_document_view);
	RETURN_IF_ERROR(m_document_view->AddAdvancedListener(this));

	m_doc_history = OP_NEW(DocumentHistory, (GetWindowCommander()));
	RETURN_OOM_IF_NULL(m_doc_history);
	RETURN_IF_ERROR(AddDocumentWindowListener(m_doc_history));

	WindowCommanderProxy::CopySettingsToWindow(GetWindowCommander(), m_openurl_setting);
	if (m_private_browsing)
	{
		GetWindowCommander()->SetTurboMode(FALSE);
		GetWindowCommander()->SetPrivacyMode(TRUE);
	}

	if (init_settings.m_window_commander)
		SetSavePlacementOnClose(FALSE);

	if (!m_staged_init)
		UpdateWindowImage(TRUE);

	if (m_focus_document)
		m_document_view->SetFocus(FOCUS_REASON_OTHER);

	GetWindowCommander()->SetPermissionListener(this);
	g_main_message_handler->SetCallBack(this, MSG_QUICK_CLOSE_ACCESSKEY_WINDOW, (MH_PARAM_1) this);
	g_main_message_handler->SetCallBack(this, MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) this);

	// Add the scope listener for each window
	if (g_scope_manager->desktop_window_manager)
		AddDocumentWindowListener(g_scope_manager->desktop_window_manager);

	RETURN_IF_ERROR(SetupInitialView(init_settings.m_no_speeddial));

	m_staged_init = false;

	// execute previously deferred fullscreen
	if (IsFullscreen())
		OnFullscreen(TRUE);

	return OpStatus::OK;
}

int DocumentDesktopWindow::GetPriority() const
{
	using namespace DocumentDesktopWindowConsts;
	OP_ASSERT(m_session_window);

	bool active = false;
	bool hidden = false;
	TRAPD(err,
		active = m_session_window->GetValueL("active") == 1;
		hidden = m_session_window->GetValueL("hidden") == 1;
	);

	if (OpStatus::IsError(err))
		return PRIORITY_ERROR;

	// active documents are initialized first
	// then normal documents
	// then documents in stacks

	if (active)
		return PRIORITY_ACTIVE;

	if (hidden)
		return PRIORITY_IN_STACK;

	return PRIORITY_NORMAL;
}

OP_STATUS DocumentDesktopWindow::InitPartial()
{
	if (!m_staged_init)
		return OpStatus::ERR_NOT_SUPPORTED;

	RETURN_VALUE_IF_NULL(m_session_window, OpStatus::ERR_NULL_POINTER);

	PagebarButton *button = GetPagebarButton();
	// if there's no button, then there is no point in creating mockup
	// since there is no graphical representation of window
	if (!button)
		return OpStatus::OK;

	OpAutoVector<OpString> history_title_list;
	OpAutoVector<OpString> history_url_list;
	unsigned current_history, max_history;
	bool has_speeddial_in_history;
	bool locked = false;

	TRAP_AND_RETURN(err,
		m_session_window->GetStringArrayL("history title", history_title_list);
		m_session_window->GetStringArrayL("history url", history_url_list);
		current_history = m_session_window->GetValueL("current history");
		max_history = m_session_window->GetValueL("max history");
		has_speeddial_in_history = m_session_window->GetValueL("has speeddial in history") == 1;
		locked = m_session_window->GetValueL("locked") == 1;
	);

	SetLockedByUser(locked);

	bool speeddial_mockup = false;
	if (!current_history && (max_history || has_speeddial_in_history))
	{
		speeddial_mockup = true;
	}

	current_history -= 1; // history pos is indexed starting with 1 instead of 0

	if (speeddial_mockup)
	{
		OpString title;
		g_languageManager->GetString(Str::S_SPEED_DIAL, title);
		SetDelayedTitle(title.CStr());
		SetIcon("Window Speed Dial Icon");
	}
	else if (current_history < history_title_list.GetCount() && current_history < max_history)
	{
		const OpString *title = history_title_list.Get(current_history);
		const OpString *document_url = history_url_list.Get(current_history);

		if (title)
		{
			SetDelayedTitle(title->CStr());
		}
		else
		{
			SetDelayedTitle(NULL); // will result in "Blank Page" title
		}

		if (document_url)
		{
			Image img = g_favicon_manager->Get(document_url->CStr());

			if (img.IsEmpty())
			{
				SetIcon("Window Document Icon");
			}
			else
			{
				m_mockup_icon = OP_NEW(OpWidgetImage, ());
				RETURN_OOM_IF_NULL(m_mockup_icon);
				m_mockup_icon->SetBitmapImage(img, FALSE);
				SetWidgetImage(m_mockup_icon);
			}
		}
	}
	else
	{
		// this can happen only with broken session file or
		// when speed dial is disabled/hidden

		SetDelayedTitle(NULL);
		SetIcon("Window Document Icon");
	}

	button->UpdateTextAndIcon(TRUE, TRUE, FALSE);
	button->UpdateCompactStatus();

	return OpStatus::OK;
}

OP_STATUS DocumentDesktopWindow::InitObject()
{
	if (!m_staged_init)
		return OpStatus::ERR_NOT_SUPPORTED;

	RETURN_IF_ERROR(init_status);
	InitInfo info;
	info.m_no_speeddial = true;
	init_status = Init(info);

	const OpSessionWindow* session_window = GetSessionWindow();

	RETURN_VALUE_IF_NULL(session_window, OpStatus::ERR_NULL_POINTER);

	OpRect rect;
	OpWindow::State state;
	bool active = false;
	bool locked = false;

	TRAP_AND_RETURN(err,
		active = session_window->GetValueL("active") == 1;
		rect.x = session_window->GetValueL("x");
		rect.y = session_window->GetValueL("y");
		rect.width  = session_window->GetValueL("w");
		rect.height = session_window->GetValueL("h");
		state = static_cast<OpWindow::State>(session_window->GetValueL("state"));
		locked = m_session_window->GetValueL("locked") == 1;
	);

	TRAPD(status, OnSessionReadL(session_window));

	SetLockedByUser(locked);

	Show(TRUE, &rect, state, !active, TRUE);

	// we won't need session any more
	SetSessionWindow(NULL);

	return status;
}

OP_STATUS DocumentDesktopWindow::AddPluginMissingBar(const OpStringC& mime_type)
{
	OP_ASSERT(g_plugin_install_manager);

	if (mime_type.IsEmpty())
		return OpStatus::ERR;

	if (m_plugin_missing_bars.GetCount() >= MAX_PLUGIN_MISSING_BAR_COUNT)
		return OpStatus::OK;

	// We need to respect the "Never for this plugin" setting, see DSK-341267
	if (g_plugin_install_manager->GetDontShowToolbar(mime_type))
		return OpStatus::OK;

# ifdef PLUGIN_AUTO_INSTALL
	// The missing plugin item should have the "available" status at this point, meaning that
	// the item should exist and it should have the resource.
	PIM_PluginItem* added_item = g_plugin_install_manager->GetItemForMimeType(mime_type);
	if (NULL == added_item)
		return OpStatus::ERR;

	if (NULL == added_item->GetResource())
		return OpStatus::ERR;

	OpString added_plugin_name;
	RETURN_IF_ERROR(added_item->GetResource()->GetAttrValue(URA_PLUGIN_NAME, added_plugin_name));

	// Check if we are already showing a bar for this plugin.
	// One plugin may be served by a couple of mime-types, meaning that we can show multiple plugin missing
	// bars in case the mime-types for different objects/embeds on the page differ.
	// We hold a list of all plugin *names* that we have triggered the missing plugin bar for this window.
	// This will work as long as the autoupdate server uses exactly the same name for the same plugin served
	// with different mime-types, which it has no reason to not do.
	for (UINT32 i=0; i < m_plugin_missing_names.GetCount(); i++)
	{
		OP_ASSERT(m_plugin_missing_names.Get(i));
		if (added_plugin_name.CompareI(m_plugin_missing_names.Get(i)->CStr()) == 0)
			return OpStatus::OK;
	}

	OpString* new_plugin_name = OP_NEW(OpString, (added_plugin_name));
	RETURN_OOM_IF_NULL(new_plugin_name);
	if (OpStatus::IsError(m_plugin_missing_names.Add(new_plugin_name)))
	{
		OP_DELETE(new_plugin_name);
		return OpStatus::ERR;
	}

	OpWidget* root_widget = GetWidgetContainer()->GetRoot();

	OpPluginMissingBar* new_bar = NULL;
	RETURN_IF_ERROR(OpPluginMissingBar::Construct(&new_bar));
	RETURN_OOM_IF_NULL(new_bar);

	OP_ASSERT(root_widget);
	root_widget->AddChild(new_bar);

	if (OpStatus::IsError(new_bar->Init("Plugin Missing Toolbar")))
	{
		new_bar->Delete();
		return OpStatus::ERR;
	}

	if (OpStatus::IsError(new_bar->SetMimeType(mime_type)))
	{
		new_bar->Delete();
		return OpStatus::ERR;
	}

	if (OpStatus::IsError(g_plugin_install_manager->AddListener(new_bar)))
	{
		new_bar->Delete();
		return OpStatus::ERR;
	}

	if (OpStatus::IsError(AddDocumentWindowListener(new_bar)))
	{
		// Not much we can do about this
		OpStatus::Ignore(g_plugin_install_manager->RemoveListener(new_bar));
		new_bar->Delete();
		return OpStatus::ERR;
	}

	if (OpStatus::IsError(m_plugin_missing_bars.Add(new_bar)))
	{
		// Not much we can do about this
		OpStatus::Ignore(g_plugin_install_manager->RemoveListener(new_bar));
		RemoveDocumentWindowListener(new_bar);
		new_bar->Delete();
		return OpStatus::ERR;
	}

	new_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
	new_bar->ShowWhenLoaded();

	return OpStatus::OK;
# else
	return OpStatus::ERR;
# endif // PLUGIN_AUTO_INSTALL
}

OP_STATUS DocumentDesktopWindow::DeletePluginMissingBars()
{
	for (UINT32 i=0; i<m_plugin_missing_bars.GetCount(); i++)
	{
		OpPluginMissingBar* current_bar = m_plugin_missing_bars.Get(i);
		OP_ASSERT(current_bar);
		RemoveDocumentWindowListener(current_bar);
		RETURN_IF_ERROR(g_plugin_install_manager->RemoveListener(current_bar));
		current_bar->Delete();
	}
	m_plugin_missing_bars.Empty();
	m_plugin_missing_names.DeleteAll();

	return OpStatus::OK;
}

/***********************************************************************************
**
**	~DocumentDesktopWindow
**
***********************************************************************************/

DocumentDesktopWindow::~DocumentDesktopWindow()
{
#ifdef PLUGIN_AUTO_INSTALL
	OpStatus::Ignore(DeletePluginMissingBars());
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_REMOVE_WINDOW, NULL, this));
#endif // PLUGIN_AUTO_INSTALL
	OP_DELETE(m_cycles_accesskeys_popupwindow);

	ToolbarManager::GetInstance()->UnregisterToolbars(GetWidgetContainer()->GetRoot());

	if (m_popup_progress_window)
	{
		m_popup_progress_window->Close(TRUE);
		m_popup_progress_window = NULL;
	}
#ifdef SUPPORT_VISUAL_ADBLOCK
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
	m_content_to_block.DeleteAll();
	m_content_to_unblock.DeleteAll();
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
#endif // SUPPORT_VISUAL_ADBLOCK

	g_main_message_handler->UnsetCallBacks(this);

	// Deny the request
	if (m_local_storage_bar)
		m_local_storage_bar->Hide(FALSE);

	//Clear private tab's blocked popups
	DesktopWindow* dt_wnd = GetParentDesktopWindow();
	if (dt_wnd && dt_wnd->GetType() == WINDOW_TYPE_BROWSER && PrivacyMode())
		static_cast<BrowserDesktopWindow*>(dt_wnd)->RemoveBlockedPopup(this);

	if (GetWindowCommander())
		GetWindowCommander()->SetPermissionListener(NULL);

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	if (m_mh)
	{
		m_mh->UnsetCallBacks(this);
		OP_DELETE(m_mh);
	}
	OP_DELETE(m_search_suggest);

	if (m_search_suggestions_callback)
		m_search_suggestions_callback->SetListener(0);
#endif

	OP_DELETE(m_mockup_icon);

	RemoveDocumentWindowListener(m_doc_history);
	OP_DELETE(m_doc_history);

	if (m_document_view)
		m_document_view->RemoveAdvancedListener(this);
}

/***********************************************************************************
**
**
**
***********************************************************************************/
void DocumentDesktopWindow::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(msg == MSG_QUICK_CLOSE_ACCESSKEY_WINDOW)
	{
		if(m_cycles_accesskeys_popupwindow)
		{
			m_cycles_accesskeys_popupwindow->Close(TRUE);
		}
	}
	else if(msg == MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK && par1 == (MH_PARAM_1) this)
	{
		DesktopPermissionCallback *callback = m_permission_callbacks.First();
		if (!callback || m_permission_controller)
		{
			return;
		}

		switch (callback->GetPermissionCallback()->Type())
		{
			case PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST:
			case PermissionCallback::PERMISSION_TYPE_CAMERA_REQUEST:
			{
				if (OpStatus::IsError(ShowPermissionPopup(callback)))
				{
					/** Something went wrong (dialog has not been displayed).
					  * In such case we deny permission but with persistence
					  * set to PERSISTENCE_TYPE_NONE so user will be asked
					  * again after page is reloaded.
					*/
					m_permission_controller = NULL;
					HandleCurrentPermission(false, OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_NONE);
				}

				break;
			}
			case PermissionCallback::PERMISSION_TYPE_WEB_HANDLER_REGISTRATION:
			{
				HandleWebHandlerRequest(callback->GetAccessingHostname(), callback->GetWebHandlerRequest());
				break;
			}
		}
	}
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	else if (msg == MSG_QUICK_START_SEARCH_SUGGESTIONS)
	{
		if (OpStatus::IsError(StartSuggestionsSearch(m_next_search.CStr())))
			m_search_suggestions_callback = 0;
	}
#endif

	DesktopWindow::HandleCallback(msg, par1, par2);
}

void DocumentDesktopWindow::UpdateMDETransparency()
{
	MDE_OpView * view = static_cast<MDE_OpView*>(GetWidgetContainer()->GetOpView());
	bool transparent = g_skin_manager->HasPersonaSkin() && IsSpeedDialActive();
	view->GetMDEWidget()->SetTransparent(transparent);
}

BOOL DocumentDesktopWindow::HasDocument()
{
	return WindowCommanderProxy::HasCurrentDoc(GetWindowCommander());
}

BOOL DocumentDesktopWindow::HasDocumentInHistory()
{
	return m_doc_history->GetHistoryLength() > 0 ? TRUE : FALSE;
}

BOOL DocumentDesktopWindow::IsLoading()
{
	OpWindowCommander *wc = GetWindowCommander();
	if (!wc)
		return FALSE;
	return wc->IsLoading();
}

void DocumentDesktopWindow::OnActivate(BOOL activate, BOOL first_time)
{
	// if the active window was opened in the background, clear the OpenURLSetting.m_in_background member so
	// the activation is right
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::ActivateTabOnClose) == 1 && !activate)
	{
		// if the closing window was opened in the foreground, then don't use the regular configuration
		// to activate the next to the right
		if (GetOpenURLSetting().m_in_background == NO)
		{
			GetOpenURLSetting().m_in_background = YES;
		}
	}
	if (activate)
	{
		if (!m_mockup_icon)
			UpdateWindowImage(FALSE);

		OpAddressDropDown *dropdown = GetAddressDropdown();
		if (dropdown)
		{
			dropdown->UpdateFavoritesFG(false);
		}

		if (first_time && !m_focus_document && RestoreFocusWhenActivated())
		{
			DesktopWindow *parent_wnd = GetParentDesktopWindow();
			if (parent_wnd)
			{
				parent_wnd->SyncLayout();
			}
			SyncLayout(); // makes sure we can focus the address dropdown in a new window

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
			AccessibilitySendEvent(Accessibility::Event(Accessibility::kAccessibilityStateFocused));
#endif
			if (dropdown)
			{
				dropdown->SetFocus(FOCUS_REASON_OTHER);
			}
			else if (m_document_view)
			{
				m_document_view->SetFocus(FOCUS_REASON_OTHER);
			}
		}

		if (!m_permission_controller && m_permission_callbacks.GetCount())
		{
			g_main_message_handler->PostMessage(MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) this, 0);
		}
	}
	else
	{
		OpAddressDropDown* dropdown = GetAddressDropdown();
		if (dropdown)
			dropdown->CloseOverlayDialog();

		/**
		 * Deletes/releases document views.
		 * Document views which are of not type DOCUMENT_TYPE_BROWSER are destroyed
		 * if they are far behind/ahead of current history position. This ensures
		 * more mem and requires recreation of deleted views if they are navigated
		 * back.
		 */
		if (IsBrowserViewActive())
			ReleaseDocumentView(true);

		if (GetWindowCommander() && GetWindowCommander()->GetAccessKeyMode())
		{
			GetWindowCommander()->SetAccessKeyMode(FALSE);
			if(m_cycles_accesskeys_popupwindow)
			{
				m_cycles_accesskeys_popupwindow->Close(TRUE);
			}
		}
	}
}

bool DocumentDesktopWindow::UpdateWindowImage(BOOL force, BOOL notify)
{
	bool changed = false;
	DocumentView* doc_view = GetActiveDocumentView();

	if (doc_view)
	{
		changed = true;
		if  (PrivacyMode())
			SetIcon("Window Private Icon");
		else
			SetWidgetImage(doc_view->GetFavIcon(force));
	}

	if (notify && changed)
		BroadcastDesktopWindowChanged();

	return changed;
}

void DocumentDesktopWindow::RestoreFocus(FOCUS_REASON reason)
{
	// if restoring focus to document desktop window and there is really
	// no sensible restore point, look for address dropdown

	if (!GetRecentKeyboardChildInputContext())
	{
		if (m_document_view && HasDocument())
		{
			m_document_view->SetFocus(FOCUS_REASON_OTHER);
			return;
		}
		else if (GetAddressDropdown())
		{
			GetAddressDropdown()->SetFocus(FOCUS_REASON_OTHER);
			return;
		}
	}

	DesktopWindow::RestoreFocus(reason);
}

void DocumentDesktopWindow::ShowWandStoreBar(WandInfo* info)
{
	if (m_wandstore_bar)
		m_wandstore_bar->Show(info);
}

void DocumentDesktopWindow::ShowGoToIntranetBar(const uni_char *address)
{
	if (m_intranet_bar)
		m_intranet_bar->Show(address, !m_private_browsing);
}

void DocumentDesktopWindow::CancelApplicationCacheBar(UINTPTR id)
{
	if (m_local_storage_bar && m_local_storage_bar->GetRequestID() == id)
		m_local_storage_bar->Cancel();
}

OpLocalStorageBar* DocumentDesktopWindow::GetLocalStorageBar()
{
	return m_local_storage_bar;
}

void DocumentDesktopWindow::SetTitle(const uni_char* title)
{
	if (m_delayed_title_change)
		return;

	if (title)
		m_title.Set(title);
	else
		g_languageManager->GetString(Str::SI_BLANK_PAGE_NAME, m_title);

	OpString new_title;

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowAddressInCaption))
	{
		// show the url in the window-title
		m_current_url.GetAttribute(URL::KUniName_Username_Password_Hidden, new_title);
	}
	else
	{
		new_title.Set(m_title);
	}
	
	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowWinSize))
	{
		UINT32 width, height;
		GetInnerSize(width, height);

		INT32 left = 0;
		INT32 right = 0;
		INT32 top = 0;
		INT32 bottom = 0;

		//Fix for Bug 161935: Window size in title bar wrong
		// The Browser Skin padding will be added to the size of the window
		// giving an error in size in the title bar.
		g_skin_manager->GetPadding("Browser Skin", &left, &top, &right, &bottom);

		width  -= (left+right);
		height -= (top+bottom);

		uni_char tmp[20];
		if( new_title.HasContent() )
			uni_snprintf(tmp, 20, UNI_L("%d:%d - "), width, height);
		else
			uni_snprintf(tmp, 20, UNI_L("%d:%d"), width, height);
		new_title.Insert(0, tmp);
	}

	DesktopWindow::SetTitle(new_title.CStr());
}

const uni_char*	DocumentDesktopWindow::GetTitle(BOOL no_extra_info)
{
	if (no_extra_info)
		return m_title.CStr();
	else
		return DesktopWindow::GetTitle(no_extra_info);
}

void DocumentDesktopWindow::SetDelayedTitle(const uni_char* new_title)
{
	SetTitle(new_title);
	if (new_title)
		m_delayed_title.Set(new_title);
	else
		m_delayed_title.Empty();
	m_delayed_title_change = true;
}

void DocumentDesktopWindow::GoToPage(const uni_char* url, BOOL url_came_from_address_field, BOOL reloadOption, URL_CONTEXT_ID context_id)
{
	OpenURLSetting setting;

	setting.m_address.Set(url);
	setting.m_from_address_field = url_came_from_address_field;
	setting.m_save_address_to_history = url_came_from_address_field;
	setting.m_context_id = context_id;

	GoToPage(setting, reloadOption);
}

void DocumentDesktopWindow::GoToPage( const OpenURLSetting& setting, BOOL reloadOption )
{
#ifdef EXTENSION_SUPPORT
	CheckExtensionDocument();
#endif // EXTENSION_SUPPORT

	BroadcastDocumentChanging();

	/**
	 * URIs which are of not type DOCUMENT_TYPE_BROWSER is forced to reload.
	 * This way DocumentViewGenerator::QuickGenerate() get called by Core.
	 * Without this, forcing to reload, would results in blank-page if such
	 * URIs are loaded in some other tabs.
	 */
	if (setting.m_address.HasContent()
		&& DocumentView::GetType(setting.m_address.CStr()) != DocumentView::DOCUMENT_TYPE_BROWSER)
		reloadOption = TRUE;

	/**
	 * ReleaseDocumentViews Deletes/releases document views.
	 * Under this special circumstance it is important to check if such view exits
	 * and if so then set that view active, and if not then recreate and set active.
	 */
	DocumentView::DocumentViewTypes type = DocumentView::GetType(setting.m_address);
	if (GetDocumentViewFromType(type))
		OpStatus::Ignore(SetActiveDocumentView(type));
	else
	{
		/*Recreating view and setting it active*/
		RETURN_VOID_IF_ERROR(CreateDocumentView(type));
		OpStatus::Ignore(SetActiveDocumentView(type));
	}

	GetActiveDocumentView()->SetFocus(FOCUS_REASON_OTHER);
	m_focus_document = TRUE;

	WindowCommanderProxy::CopySettingsToWindow(GetWindowCommander(), setting);

	if (reloadOption)
	{
		URL url = urlManager->GetURL(setting.m_address.CStr());
		url.Unload();
		urlManager->EmptyDCache();
	}

	URL_CONTEXT_ID context_id = setting.m_context_id;

	// all private urls should use this context id.
	// but currently search urls don't support context id yet.
	// SEARCH_WITH_URL_CONTEXT needs to be finished?
	if (GetWindowCommander()->GetPrivacyMode())
		context_id = g_windowManager->GetPrivacyModeContextId();

	if (!setting.m_url.IsEmpty() || !setting.m_ref_url.IsEmpty() || setting.m_document)
	{
		if(context_id == (URL_CONTEXT_ID)-1)
		{
			context_id = GetWindowCommander()->GetURLContextID();
		}
		URL	new_url = setting.m_url.IsEmpty() ? g_url_api->GetURL(setting.m_address.CStr(), context_id) : setting.m_url;

		URL ref_url = setting.m_document ? setting.m_document->GetURL() : setting.m_ref_url;

		WindowCommanderProxy::OpenURL(GetWindowCommander(), new_url, ref_url, TRUE, FALSE, -1, TRUE, FALSE, setting.m_document ?  NotEnteredByUser : WasEnteredByUser, setting.m_is_remote_command);
	}
	else
	{
		OpString address;
		address.Set(setting.m_address.CStr());
		address.Strip();

#if defined(_UNIX_DESKTOP_)
		if(address.CStr() && uni_strncmp(address.CStr(), UNI_L("~"), 1) == 0 )
		{
			// Bug #276444
			OpString tmp;
			tmp.Reserve( address.Length() + 100 );
			g_op_system_info->ExpandSystemVariablesInString(address.CStr(), &tmp);
			address.Set(tmp);

		}
#endif

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		SearchEngineManager::SearchSetting search_setting(setting);
		search_setting.m_target_window = this;
		search_setting.m_keyword.Set(address);
		search_setting.m_new_window = search_setting.m_new_page = search_setting.m_in_background = NO;
		// We are already in the correct page. Search code calls ClassicApplication::OpenURL() which
		// uses m_src_commander to open yet a new page if not set to 0 (DSK-338853)
		search_setting.m_src_commander = 0;
		if (g_searchEngineManager->CheckAndDoSearch(search_setting))
		{
			return;
		}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

		if( address.CStr() )
		{
			// Fix some common spell errors in URIs
			if ( setting.m_save_address_to_history )
			{
				// The idea behind this test is to catch a common typo made by users (',' instead of '.')
				// The test is only to be made when user has typed in manually in the address field
				// which the 'm_save_address_to_history' flag indicates.

				// Check server name. Would perfer a URL util function for this [espen 2004-11-26]

				// Start processing after first "://" but only if the first ":"
				// is part of the "://" substring [bug #180948]
				const uni_char* p1 = uni_strstr(address.CStr(), UNI_L(":") );
				if( p1 && uni_strncmp(p1+1, UNI_L("//"), 2) != 0 )
				{
					p1 = 0;
				}

				// Stop processing after first '/' or ':' (after protocol) [bug #161635 and #173499]
				const uni_char* p2 = uni_strstr( p1 ? p1+3 : address.CStr(), UNI_L("/") );
				const uni_char* p3 = uni_strstr( p1 ? p1+3 : address.CStr(), UNI_L(":") );
				p2 = (p2 < p3) ? (p2 ? p2 : p3) : (p3 ? p3 : p2);

				int start = p1 ? p1 - address.CStr() + 3 : 0;
				int stop  = p2 ? p2 - address.CStr() : address.Length();
				if( start < stop )
				{
					for( INT32 i=start; i<stop; i++ )
						if( address.CStr()[i] == ',' )
							address.CStr()[i] = '.';
				}
			}

			if( setting.m_from_address_field )
			{
				// Add "http://" to URLs that target a network port but lack a protocol:
				// For instance, "www.opera.com:80" -> "http://www.opera.com:80"	[huibk 30-08-2006]
				//
				// Background: Opera can handle unknown protocols schemas, an address like "servername:port"
				// could theoretically be an unkonwn protocol. Because we don't expect users to launch
				// unknown protocols from the address bar we prepend "http://" in front of these
				// types of addresses, ie. make it "http://servername:port". See bug #201020.
				uni_char* str = address.CStr();

				//step through the server/domain name in the address
				while (uni_isalnum(*str) || (*str == '.') || (*str == '-'))
					++str;
				if ( (*str == ':') && uni_isdigit(str[1]))
				{
					++str;
					// found a domain name, a colon and one digit. Let's check if nothing but digits follow
					do
						++str;
					while (uni_isdigit(*str));
					if (*str == '\0' || *str == '/')
					{
						// When the string ends after the last digit, or there's a slash
						// and possibly additional characters -> we have a match!
						address.Insert(0, UNI_L("http://"));
					}
				}
			}
		}

		// This was added to deal with the '#' problem in local filenames [espen 2004-04-19]
		EscapeURLIfNeeded(address);

		OpWindowCommander::OpenURLOptions options;
		/* There's probably more to "entered by user" than "remote
		 * commands", but I don't know what.
		 */
		options.entered_by_user = !setting.m_is_remote_command;
		options.requested_externally = setting.m_is_remote_command;
		options.context_id = context_id;

		GetWindowCommander()->OpenURL(address, options);
	}

	if (setting.m_save_address_to_history)
	{
		WindowCommanderProxy::SetURLCameFromAddressField(GetWindowCommander(), TRUE);
	}
}

OP_STATUS DocumentDesktopWindow::GetURL(OpString &url)
{
	if (GetWindowCommander() == NULL)
		return OpStatus::ERR;

	URL curl = WindowCommanderProxy::GetCurrentURL(GetWindowCommander());
	return curl.GetAttribute(URL::KUniName, url);
}

void DocumentDesktopWindow::EnsureAllowedOuterSize(UINT32& width, UINT32& height) const
{
	// Fix for DSK-360921 - Small windows can be used to trick users with opera:config
	// To prevent small popup windows, window size cannot be below minimum dimensions
	width = MAX(width, DocumentDesktopWindowConsts::MINIMUM_WINDOW_WIDTH);
	height = MAX(height, DocumentDesktopWindowConsts::MINIMUM_WINDOW_HEIGHT);
}

void DocumentDesktopWindow::OnLayout()
{
	if (m_staged_init)
		return;

	// layout_rect: the rect that can be used to layout a toolbar, margin will be
	// took into account, the means if a toolbar has negative margin the next toolbar
	// with the same alignment will overlap with it.
	// document_rect: the rect that can be used for the document view, margin of toolbars
	// next to document view is ignored.
	OpRect layout_rect, document_rect;
	BOOL use_toolbar_margins = g_skin_manager->GetOptionValue("Allow Toolbar Margins", 0);

	GetBounds(layout_rect);
	document_rect.Set(layout_rect);

	LayoutToolbar(m_address_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);

	OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::PersonalbarInline, GetWidgetContainer()->GetRoot(), TRUE);
	if(personalbar)
	{
		LayoutToolbar(personalbar, layout_rect, document_rect, FALSE, use_toolbar_margins);
	}
#ifdef SUPPORT_NAVIGATION_BAR
	LayoutToolbar(m_navigation_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);
#endif // SUPPORT_NAVIGATION_BAR
	LayoutToolbar(m_view_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);

	if (HasShowAddressBarButton())
	{
		m_show_addressbar_button->SetRect(OpRect(document_rect.x, document_rect.y, document_rect.width, 13));
		m_show_addressbar_button->SetVisibility(TRUE);
		m_show_addressbar_button->GetBorderSkin()->SetType(SKINTYPE_TOP);
		document_rect.y += 13;
		document_rect.height -= 13;
		layout_rect.Set(document_rect); // margins of toolbars next to this button is ignored
	}
	else
	{
		m_show_addressbar_button->SetVisibility(FALSE);
	}

#ifdef SUPPORT_VISUAL_ADBLOCK
	LayoutToolbar(m_popup_adblock_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);
#endif // SUPPORT_VISUAL_ADBLOCK

	LayoutToolbar(GetFindTextBar(), layout_rect, document_rect, FALSE, use_toolbar_margins);
	LayoutToolbar(m_wandstore_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);
	LayoutToolbar(m_intranet_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);
	LayoutToolbar(m_local_storage_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);
	LayoutToolbar(m_plugin_crashed_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);

	LayoutToolbar(m_webhandler_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);

#ifdef PLUGIN_AUTO_INSTALL
	for (UINT32 i=0; i<m_plugin_missing_bars.GetCount(); i++)
	{
		OpPluginMissingBar* current_bar = m_plugin_missing_bars.Get(i);
		OP_ASSERT(current_bar);
		LayoutToolbar(current_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);
	}
#endif // PLUGIN_AUTO_INSTALL

	InstallPersonaBar *personabar = static_cast<InstallPersonaBar *>(ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::InstallPersonabar, GetWidgetContainer()->GetRoot()));
	if(personabar)
	{
		LayoutToolbar(personabar, layout_rect, document_rect, FALSE, use_toolbar_margins);
	}
	DownloadExtensionBar *extensionbar = GetDownloadExtensionBar(FALSE);
	if(extensionbar)
	{
		LayoutToolbar(extensionbar, layout_rect, document_rect, FALSE, use_toolbar_margins);
	}

	// progress_type
	// 0 = inside addressbar
	// 1 = popup at bottom
	// 2 = minimal inside adressfield
	// 3 = off
	int progress_type =	g_pcui->GetIntegerPref(PrefsCollectionUI::ProgressPopup);
	BOOL show_progress = (IsLoading() && progress_type <= 1 && (GetActiveDocumentView() == NULL || GetActiveDocumentView()->GetDocumentType() != DocumentView::DOCUMENT_TYPE_SPEED_DIAL))
		|| g_application->IsCustomizingHiddenToolbars();

	// show address bar?
	if (m_address_bar->IsVisible())
	{
		m_address_bar->SetVisibility(!show_progress || progress_type != 0);
		m_progress_bar->SetRect(m_address_bar->GetRect());
		m_progress_bar->SetVisibility(show_progress && progress_type == 0);
	}
	else
		m_progress_bar->SetVisibility(FALSE);

	DocumentView* doc_view = GetActiveDocumentView();
	if (doc_view && doc_view->GetDocumentType() != DocumentView::DOCUMENT_TYPE_BROWSER)
		doc_view->SetRect(document_rect);

	//julienp: We need to make sure that the document view always has the correct rectangle,
	//even if it isn't visible. If it doesn't and becomes visible for loading a page,
	//the visual device will probably get locked before the document view gets the correct
	//size and gets the occasion to be painted. See bug 275162
	doc_view = GetDocumentViewFromType(DocumentView::DOCUMENT_TYPE_BROWSER);
	if (doc_view)
		doc_view->SetRect(document_rect);

	// delete popup?

	if (m_popup_progress_window && (!show_progress || progress_type != 1))
	{
		m_popup_progress_bar->SetVisibility(FALSE);
		m_popup_progress_bar->Remove();
		GetWidgetContainer()->GetRoot()->AddChild(m_popup_progress_bar);
		m_popup_progress_window->Close(TRUE);
		m_popup_progress_window = NULL;
	}

	// show popup

	if (!m_popup_progress_window && show_progress && progress_type == 1)
	{
		m_popup_progress_window = OP_NEW(DesktopWidgetWindow, ());
		if (!m_popup_progress_window)
			return;

		if (OpStatus::IsError(m_popup_progress_window->Init(OpWindow::STYLE_CHILD, "Popup Progress Window", GetOpWindow())))
		{
			OP_DELETE(m_popup_progress_window);
			m_popup_progress_window = NULL;
			return;
		}
		m_popup_progress_window->GetWidgetContainer()->GetRoot()->GetBorderSkin()->SetImage(GetSkin());
		m_popup_progress_window->GetWidgetContainer()->SetParentInputContext(this);
		m_popup_progress_window->GetWidgetContainer()->SetParentDesktopWindow(this);
		m_popup_progress_bar->Remove();
		m_popup_progress_window->GetWidgetContainer()->GetRoot()->AddChild(m_popup_progress_bar);
		m_popup_progress_window->GetWidgetContainer()->GetRoot()->SetRTL(GetWidgetContainer()->GetRoot()->GetRTL());
		m_popup_progress_bar->SetVisibility(TRUE);
	}

	// Only send the height when the toolbar is on the top && visible and it has a transparent background
	if (m_address_bar->IsVisible() && m_address_bar->GetResultingAlignment() == OpBar::ALIGNMENT_TOP &&
		!((!m_address_bar->GetBorderSkin() || !m_address_bar->GetBorderSkin()->GetSkinElement() || !m_address_bar->GetBorderSkin()->GetSkinElement()->HasTransparentState()) &&
		  (!m_address_bar->GetBorderSkin() || !m_address_bar->GetBorderSkin()->GetSkinElement() || !m_address_bar->GetBorderSkin()->GetSkinElement()->HasTransparentState())))
	{
		OpRect r = m_address_bar->GetRect(TRUE);
		BroadcastTransparentAreaChanged(r.height);
	}
}

/***********************************************************************************
**
**  OnLayoutAfterChildren
**
***********************************************************************************/

void DocumentDesktopWindow::OnLayoutAfterChildren()
{
	if (m_staged_init)
		return;
	LayoutPopupProgress();
}

/***********************************************************************************
**
**	LayoutPopupProgress
**
***********************************************************************************/
void DocumentDesktopWindow::LayoutPopupProgress()
{
	OpRect visible_rect = WindowCommanderProxy::GetVisualDeviceVisibleRect(m_document_view->GetWindowCommander());
	LayoutPopupProgress(visible_rect);
}

void DocumentDesktopWindow::LayoutPopupProgress(OpRect visible_rect)
{
	// layout progress

	if (m_popup_progress_window)
	{
		OpRect rect = m_document_view->GetRect(TRUE);

		m_document_view->GetOpWindow()->GetInnerPos(&rect.x, &rect.y);

		rect.width = visible_rect.width;
		rect.height = visible_rect.height;
		rect.x += visible_rect.x;
		rect.y += visible_rect.y;

		INT32 height = m_popup_progress_bar->GetHeightFromWidth(rect.width);

		rect.y += rect.height - height;
		rect.height = height;

#ifdef VEGA_OPPAINTER_SUPPORT
		// Don't allow the popup progress window to steal focus. fix for DSK-298142
		MDE_OpWindow* win = static_cast<MDE_OpWindow*>(m_popup_progress_window->GetWindow());
		if (win)
			win->SetAllowAsActive(FALSE);
#endif
		m_popup_progress_window->Show(TRUE, &rect);
		m_popup_progress_window->GetWindow()->Raise(); // OpWindow* WidgetWindow::GetWindow()

		rect.x = rect.y = 0;
		m_popup_progress_bar->SetRect(rect);
	}
}

/***********************************************************************************
**
**	HasShowAddressBarButton
**
***********************************************************************************/

BOOL DocumentDesktopWindow::HasShowAddressBarButton()
{
#ifdef EXTENSION_SUPPORT
	if (m_is_extension_document == YES)
	{
		return FALSE;
	}
#endif // EXTENSION_SUPPORT

	BOOL normally_shows_address_bar = OpBar::ALIGNMENT_OFF != (OpBar::Alignment)g_pcui->GetIntegerPref(PrefsCollectionUI::AddressbarAlignment);
	BOOL shows_mainbar = OpBar::ALIGNMENT_OFF != (OpBar::Alignment)g_pcui->GetIntegerPref(PrefsCollectionUI::MainbarAlignment);
	BOOL kiosk_mode = KioskManager::GetInstance()->GetEnabled();
	BOOL detached_window = GetWorkspace() == NULL;

	if (shows_mainbar && detached_window)
	{
		// the user probably shows the address field on the main bar
		normally_shows_address_bar = TRUE;
	}

	return (m_address_bar->GetAlignment() == OpBar::ALIGNMENT_OFF &&
			!IsFullscreen() &&
			normally_shows_address_bar &&
			!g_application->IsEmBrowser() &&
			!kiosk_mode);
}

/***********************************************************************************
**
**	GetSizeFromBrowserViewSize
**
***********************************************************************************/

void DocumentDesktopWindow::GetSizeFromBrowserViewSize(INT32& width, INT32& height)
{
	BOOL use_toolbar_margins = g_skin_manager->GetOptionValue("Allow Toolbar Margins", 0);

	m_document_view->GetSizeFromBrowserViewSize(width, height);

	OpRect layout_rect(0, 0, width, height);
	OpRect document_rect(0, 0, width, height);

	// Make sure toolbars are layouted with the same order as in OnLayout

	LayoutToolbar(m_address_bar, layout_rect, document_rect, TRUE, use_toolbar_margins);

	OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::PersonalbarInline, GetWidgetContainer()->GetRoot(), TRUE);
	if(personalbar)
	{
		LayoutToolbar(personalbar, layout_rect, document_rect, TRUE, use_toolbar_margins);
	}
#ifdef SUPPORT_NAVIGATION_BAR
	LayoutToolbar(m_navigation_bar, layout_rect, document_rect, TRUE, use_toolbar_margins);
#endif // SUPPORT_NAVIGATION_BAR
	LayoutToolbar(m_view_bar, layout_rect, document_rect, TRUE, use_toolbar_margins);

	if (HasShowAddressBarButton())
	{
		document_rect.y += 13;
		document_rect.height -= 13;
		layout_rect.Set(document_rect); // margins of toolbars next to this button is ignored
	}

	LayoutToolbar(GetFindTextBar(), layout_rect, document_rect, TRUE, use_toolbar_margins);
	LayoutToolbar(m_wandstore_bar, layout_rect, document_rect, TRUE, use_toolbar_margins);
	LayoutToolbar(m_intranet_bar, layout_rect, document_rect, TRUE, use_toolbar_margins);
	LayoutToolbar(m_local_storage_bar, layout_rect, document_rect, TRUE, use_toolbar_margins);
	LayoutToolbar(m_plugin_crashed_bar, layout_rect, document_rect, TRUE, use_toolbar_margins);

#ifdef SUPPORT_VISUAL_ADBLOCK
	LayoutToolbar(m_popup_adblock_bar, layout_rect, document_rect, TRUE, use_toolbar_margins);
#endif // SUPPORT_VISUAL_ADBLOCK

	LayoutToolbar(m_webhandler_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);

#ifdef PLUGIN_AUTO_INSTALL
	for (UINT32 i=0; i<m_plugin_missing_bars.GetCount(); i++)
	{
		OpPluginMissingBar* current_bar = m_plugin_missing_bars.Get(i);
		OP_ASSERT(current_bar);
		LayoutToolbar(current_bar, layout_rect, document_rect, FALSE, use_toolbar_margins);
	}
#endif // PLUGIN_AUTO_INSTALL

	// watch for arithmetic here, uint and int conversion, also overflow issues : DSK-371041
	width += width - document_rect.width;
	if (width < 0 || width < DocumentDesktopWindowConsts::MINIMUM_WINDOW_WIDTH)
		width = DocumentDesktopWindowConsts::MINIMUM_WINDOW_WIDTH;

	height += height - document_rect.height;
	if (height < 0 || height < DocumentDesktopWindowConsts::MINIMUM_WINDOW_HEIGHT)
		height = DocumentDesktopWindowConsts::MINIMUM_WINDOW_HEIGHT;
}

void DocumentDesktopWindow::OnResize(INT32 width, INT32 height)
{
	OnLayout();

	DocumentView* doc_view = GetActiveDocumentView();
	if (doc_view)
		doc_view->Layout();

	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowWinSize))
		SetTitle(GetWindowCommander()->GetCurrentTitle());
}

/***********************************************************************************
**
**	OnFullscreen
**
***********************************************************************************/

void DocumentDesktopWindow::OnFullscreen(BOOL fullscreen)
{
	if (m_staged_init)
		return; // the window is not ready yet for full screen, will be handled during finalization of init

	OpBar::Alignment fullscreen_alignment = ( CommandLineManager::GetInstance()->GetArgument(CommandLineManager::MediaCenter) || KioskManager::GetInstance()->GetKioskButtons()) ? m_address_bar->GetAlignment() : OpBar::ALIGNMENT_OFF;
	m_address_bar->OnFullscreen(fullscreen, fullscreen_alignment);

	OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::PersonalbarInline, GetWidgetContainer()->GetRoot(), TRUE);
	if(personalbar)
	{
		personalbar->OnFullscreen(fullscreen);
	}
#ifdef SUPPORT_NAVIGATION_BAR
	m_navigation_bar->OnFullscreen(fullscreen);
#endif // SUPPORT_NAVIGATION_BAR
	m_view_bar->OnFullscreen(fullscreen);
	GetFindTextBar()->OnFullscreen(fullscreen);
	m_wandstore_bar->OnFullscreen(fullscreen);
	m_intranet_bar->OnFullscreen(fullscreen);
	m_local_storage_bar->OnFullscreen(fullscreen);
	m_plugin_crashed_bar->OnFullscreen(fullscreen);

#ifdef SUPPORT_VISUAL_ADBLOCK
	if(m_adblock_edit_mode)
	{
		m_popup_adblock_bar->OnFullscreen(fullscreen);
	}
#endif // SUPPORT_VISUAL_ADBLOCK

	if (m_permission_callbacks.GetCount() && m_permission_callbacks.HasType(PermissionCallback::PERMISSION_TYPE_WEB_HANDLER_REGISTRATION))
	{
		m_webhandler_bar->OnFullscreen(fullscreen);
	}

#ifdef PLUGIN_AUTO_INSTALL
	for (UINT32 i=0; i<m_plugin_missing_bars.GetCount(); i++)
	{
		OpPluginMissingBar* current_bar = m_plugin_missing_bars.Get(i);
		OP_ASSERT(current_bar);
		current_bar->OnFullscreen(fullscreen);
	}
#endif // PLUGIN_AUTO_INSTALL

}

/***********************************************************************************
**
**	OnSessionReadL
**
***********************************************************************************/

void DocumentDesktopWindow::OnSessionReadL(const OpSessionWindow* session_window)
{
	WindowCommanderProxy::OnSessionReadL(GetWindowCommander(),
										 this,
										 session_window,
										 m_focus_document);
#ifdef WEBSERVER_SUPPORT
	OpStringC gadget_id(session_window->GetStringL("gadget id"));
	if (gadget_id.HasContent())
	{
		if (g_hotlist_manager->GetUniteServicesModel())
		{
			HotlistModelItem * item = g_hotlist_manager->GetUniteServicesModel()->GetByGadgetIdentifier(gadget_id);
			if (item && item->IsUniteService())
			{
				static_cast<UniteServiceModelItem*>(item)->SetServiceWindow(this);
			}
		}
	}
#endif // WEBSERVER_SUPPORT
	
	m_is_read_document = session_window->GetValueL("read document") ? true : false ;
}

/***********************************************************************************
**
**	OnSessionWriteL
**
***********************************************************************************/

void DocumentDesktopWindow::OnSessionWriteL(OpSession* session, OpSessionWindow* sessionwindow, BOOL shutdown)
{
	OP_ASSERT(sessionwindow);

	if (m_staged_init)
	{
		// During loading of big sessions, call to OnSessionWrite may happen
		// before all document windows were initialized (only mockup exists).
		// This may be either result of normal session writing or browser
		// shutdown.
		//
		// In such case windowcommander does not exist, but we can simply
		// use copy of session window from this dispatchable object.
		//
		OP_ASSERT(GetSessionWindow());
		CopySessionWindowL(GetSessionWindow(), sessionwindow);
		return;
	}

	WindowCommanderProxy::OnSessionWriteL(GetWindowCommander(),
										  this,
										  session,
										  sessionwindow,
										  shutdown);

#ifdef WEBSERVER_SUPPORT
	if (m_gadget_identifier.HasContent())
	{
		sessionwindow->SetStringL("gadget id", m_gadget_identifier);
	}
#endif // WEBSERVER_SUPPORT

	sessionwindow->SetValueL("read document", !HasAttention());
}

void DocumentDesktopWindow::CopySessionWindowL(const OpSessionWindow* in, OpSessionWindow* out) const
{
#define COPY_VAL(prop) out->SetValueL(prop, in->GetValueL(prop))
#define COPY_STR(prop) out->SetStringL(prop, in->GetStringL(prop))
#define COPY_VAL_ARRAY(prop, name) OpAutoVector<UINT32> name; ANCHOR(OpAutoVector<UINT32>, name); \
	in->GetValueArrayL(prop, name); \
	out->SetValueArrayL(prop, name)
#define COPY_STR_ARRAY(prop, name) OpAutoVector<OpString> name; ANCHOR(OpAutoVector<OpString>, name); \
	in->GetStringArrayL(prop, name); \
	out->SetStringArrayL(prop, name)

	COPY_VAL("x");
	COPY_VAL("y");
	COPY_VAL("w");
	COPY_VAL("h");

	out->SetTypeL(OpSessionWindow::DOCUMENT_WINDOW);
	COPY_VAL("window id");
	COPY_VAL("max history");
	COPY_VAL("current history");
	COPY_VAL("has speeddial in history");

	out->ClearStringArrayL();
	COPY_STR_ARRAY("history url", history_url_list);
	COPY_VAL_ARRAY("history document type", history_doctype_list);
	COPY_STR_ARRAY("history title", history_title_list);
	COPY_VAL_ARRAY("history scrollpos list", history_scrollpos_list);

	COPY_STR("encoding");
	COPY_VAL("show img");
	COPY_VAL("load img");
	COPY_VAL("image mode");
	COPY_VAL("user auto reload enable");
	COPY_VAL("user auto reload sec user setting");
	COPY_VAL("user auto reload only if expired");
	COPY_VAL("output associated window");
	COPY_VAL("CSS mode");
	COPY_VAL("handheld mode");
	COPY_VAL("scale");
	COPY_VAL("show scrollbars");
	COPY_VAL("read document");

#undef COPY_STR_ARRAY
#undef COPY_VAL_ARRAY
#undef COPY_STR
#undef COPY_VAL
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/
BOOL DocumentDesktopWindow::OnInputAction(OpInputAction* action)
{
	OpWindowCommander *win_comm = GetWindowCommander();

	if (HandleImageActions(action, win_comm))
		return TRUE;

	if (HandleFrameActions(action, win_comm))
		return TRUE;

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
 				case OpInputAction::ACTION_VIEW_DOCUMENT_SOURCE:
 				case OpInputAction::ACTION_PRINT_DOCUMENT:
 				case OpInputAction::ACTION_PRINT_PREVIEW:
 				case OpInputAction::ACTION_REFRESH_DISPLAY:
 				{
 					child_action->SetEnabled(HasDocument() && IsBrowserViewActive());
 					return TRUE;
 				}

				case OpInputAction::ACTION_MANAGE_SEARCH_ENGINES:
				{
					child_action->SetEnabled(IsSpeedDialActive());
					return TRUE;
				}

				case OpInputAction::ACTION_FORWARD:
				case OpInputAction::ACTION_BACK:
				{
					bool is_back = child_action->GetAction() == OpInputAction::ACTION_BACK;
					BOOL enable = m_doc_history && is_back ? m_doc_history->HasBackHistory() : m_doc_history->HasForwardHistory();
					child_action->SetEnabled(enable);
					if (enable)
					{
						const DocumentHistoryInformation& info = m_doc_history-> GetAdjacentHistoryInfo(is_back);
						if (info.title.HasContent())
							child_action->GetActionInfo().SetStatusText(info.title.CStr());
						else
							child_action->GetActionInfo().Empty();
					}
					else
						child_action->GetActionInfo().Empty();

					return TRUE;
				}

				case OpInputAction::ACTION_FAST_FORWARD:
				{
					BOOL enable = IsLoading() == FALSE && m_doc_history->HasFastForwardHistory();
					child_action->SetEnabled(enable);
					return TRUE;
				}

				case OpInputAction::ACTION_REWIND:
				{
					BOOL enable = m_doc_history->HasRewindHistory();
					child_action->SetEnabled(enable);
					return TRUE;
				}

				case OpInputAction::ACTION_ADD_TO_BOOKMARKS:
				{
					// DSK-351304: disable until bookmarks.adr is loaded
					if (!g_desktop_bookmark_manager->GetBookmarkModel()->Loaded())
					{
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
				}
				// fall through
				case OpInputAction::ACTION_RELOAD_ALL_PAGES:
				{
					child_action->SetEnabled(HasDocument() && GetOpWindow() /* GetOpWindow()->IsActiveTopmostWindow()*/);
					return TRUE;
				}

				case OpInputAction::ACTION_NO_SECURITY:
				{
					child_action->GetActionInfo().SetStatusText(WindowCommanderProxy::GetSecurityStateText(win_comm));
					OpDocumentListener::SecurityMode mode = win_comm->GetSecurityMode();
					child_action->SetSelected(mode == OpDocumentListener::NO_SECURITY || mode == OpDocumentListener::UNKNOWN_SECURITY);
					return TRUE;
				}

				case OpInputAction::ACTION_LOW_SECURITY:
				{
					child_action->GetActionInfo().SetStatusText(WindowCommanderProxy::GetSecurityStateText(win_comm));
					OpDocumentListener::SecurityMode mode = win_comm->GetSecurityMode();
					child_action->SetSelected(mode == OpDocumentListener::LOW_SECURITY);
					return TRUE;
				}

				case OpInputAction::ACTION_MEDIUM_SECURITY:
				{
					child_action->GetActionInfo().SetStatusText(WindowCommanderProxy::GetSecurityStateText(win_comm));
					OpDocumentListener::SecurityMode mode = win_comm->GetSecurityMode();
					child_action->SetSelected(mode == OpDocumentListener::MEDIUM_SECURITY);
					return TRUE;
				}

				case OpInputAction::ACTION_HIGH_SECURITY:
				{
					child_action->GetActionInfo().SetStatusText(WindowCommanderProxy::GetSecurityStateText(win_comm));
					OpDocumentListener::SecurityMode mode = win_comm->GetSecurityMode();
					child_action->SetSelected(mode == OpDocumentListener::HIGH_SECURITY);
					return TRUE;
				}

				case OpInputAction::ACTION_EXTENDED_SECURITY:
				{
					child_action->GetActionInfo().SetStatusText(WindowCommanderProxy::GetSecurityStateText(win_comm));
					OpDocumentListener::SecurityMode mode = win_comm->GetSecurityMode();
					child_action->SetSelected(g_desktop_security_manager->IsEV(mode));
					return TRUE;
				}

				case OpInputAction::ACTION_DISABLE_AUTOMATIC_RELOAD:
				{
					child_action->SetSelected(!WindowCommanderProxy::GetUserAutoReload(win_comm)->GetOptIsEnabled());
					return TRUE;
				}

				case OpInputAction::ACTION_SET_AUTOMATIC_RELOAD:
				{
					int timeout = child_action->GetActionData();
					// Only set the tick if reload every is enabled
					if (WindowCommanderProxy::GetUserAutoReload(win_comm)->GetOptIsEnabled())
					{
						// Check if the current timeout setting is a standard one from the menu
						if (IsStandardTimeoutMenuItem(WindowCommanderProxy::GetUserAutoReload(win_comm)->GetOptSecUserSetting()))
						{
							child_action->SetSelected(WindowCommanderProxy::GetUserAutoReload(win_comm)->GetOptSecUserSetting() == timeout);
						}
						else
						{
							// Not a standard timeout
							if (!timeout)
								child_action->SetSelected(TRUE);
						}
					}
					else
						child_action->SetSelected(FALSE);
					child_action->SetEnabled(TRUE);
					return TRUE;
				}

				case OpInputAction::ACTION_PASTE_AND_GO:
				case OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND:
				{
					child_action->SetEnabled(g_desktop_clipboard_manager->HasText());
					return TRUE;
				}

#ifdef _X11_SELECTION_POLICY_
				case OpInputAction::ACTION_PASTE_SELECTION_AND_GO:
				case OpInputAction::ACTION_PASTE_SELECTION_AND_GO_BACKGROUND:
				{
					child_action->SetEnabled(g_desktop_clipboard_manager->HasText(true));
					return TRUE;
				}
#endif
				case OpInputAction::ACTION_VALIDATE_DOCUMENT_SOURCE:
				case OpInputAction::ACTION_VALIDATE_FRAME_SOURCE:
					child_action->SetEnabled(HasDocument());
					return TRUE;

#ifdef SUPPORT_VISUAL_ADBLOCK
				case OpInputAction::ACTION_CONTENTBLOCK_MODE_ON:
				case OpInputAction::ACTION_CONTENTBLOCK_MODE_OFF:
				{
					if(KioskManager::GetInstance()->GetEnabled())
					{
						child_action->SetEnabled(FALSE);
						return TRUE;
					}

					// don't save stuff in privacy mode
					if(GetWindowCommander()->GetPrivacyMode())
					{
						child_action->SetEnabled(FALSE);
						return TRUE;
					}

					//Content can only be blocked if there is a page loaded
					// and it's not generated by opera (opera:*)
					URL url = WindowCommanderProxy::GetMovedToURL(GetWindowCommander());

					OpString location;
					location.Set(url.GetAttribute(URL::KHostName));

					BOOL content_block_possible =
						WindowCommanderProxy::HasCurrentDoc(GetWindowCommander()) &&
						g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableContentBlocker, location.CStr());

					URLType type = (URLType)url.GetAttribute(URL::KType);
					BOOL is_http = type == URL_HTTP || type == URL_HTTPS;
					content_block_possible &= is_http || !url.GetAttribute(URL::KIsGeneratedByOpera);
					child_action->SetEnabled(content_block_possible);

					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_CONTENTBLOCK_MODE_ON, m_adblock_edit_mode);
					return TRUE;
				}
#endif // SUPPORT_VISUAL_ADBLOCK

				case OpInputAction::ACTION_ENABLE_SCROLL_BARS:
				case OpInputAction::ACTION_DISABLE_SCROLL_BARS:
				{
    				child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_SCROLL_BARS, WindowCommanderProxy::ShowScrollbars(win_comm));
					return TRUE;
				}

				case OpInputAction::ACTION_DUPLICATE_PAGE:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}

				case OpInputAction::ACTION_GO_TO_LINK_ELEMENT:
				{
					if (child_action->GetActionDataString())
					{
						LinkElementInfo information;
						UINT32 count = win_comm->GetLinkElementCount();
						UINT i;
						for (i = 0; i < count; i++)
						{
							win_comm->GetLinkElementInfo(i, &information);
							if (information.rel && (
								uni_stricmp(child_action->GetActionDataString(),information.rel) == 0 ||
								(uni_stricmp(UNI_L("alternate"),information.rel) == 0 &&
								information.type &&
								uni_stricmp(child_action->GetActionDataString(),information.type) == 0)) )
							{
								URL url = g_url_api->GetURL(information.href);

								if (url.GetAttribute(URL::KType) == URL_HTTP || url.GetAttribute(URL::KType) == URL_HTTPS)
								{
									child_action->SetEnabled(TRUE);
									return TRUE;
								}
							}
						}
					}
					child_action->SetEnabled(FALSE);
					return TRUE;
				}

				case OpInputAction::ACTION_ENABLE_MSR:
				case OpInputAction::ACTION_DISABLE_MSR:
				{
					child_action->SetEnabled(IsBrowserViewActive());
					child_action->SetSelected(win_comm->GetLayoutMode() == OpWindowCommander::ERA);
					return TRUE;
				}

				case OpInputAction::ACTION_GO_TO_HISTORY:
				{
					INTPTR data = child_action->GetActionData();
					child_action->SetSelected(data == -1);

					// Set status text to be a URL of the item.
					DocumentHistory::Position history_pos;
					if (data < 0)
						history_pos = DocumentHistory::Position(DocumentHistory::Position::FAST_FORWARD, - 1 - data);
					else
						history_pos = DocumentHistory::Position(DocumentHistory::Position::NORMAL, data);

					HistoryInformation history_item;
					if (OpStatus::IsSuccess(m_doc_history->GetHistoryInformation(history_pos, &history_item)))
						child_action->GetActionInfo().SetStatusText(history_item.url);

					return TRUE;
				}

				case OpInputAction::ACTION_HIGHLIGHT_BOOKMARK:
				{
					HotlistModel* model = g_hotlist_manager->GetBookmarksModel();
					HotlistModelItem* item = model->GetByURL( m_current_url );
					child_action->SetEnabled(item != NULL);
					return TRUE;
				}

				case OpInputAction::ACTION_HIGHLIGHT_NOTE:
				{
					HotlistModel* model = g_hotlist_manager->GetNotesModel();
					HotlistModelItem* item = model->GetByURL( m_current_url );
					child_action->SetEnabled(item != NULL && !item->GetIsInsideTrashFolder());
					return TRUE;
				}

				case OpInputAction::ACTION_SELECT_USER_CSS_FILE:
				case OpInputAction::ACTION_DESELECT_USER_CSS_FILE:
				{
					WindowCommanderProxy::ToggleUserCSS(GetWindowCommander(), child_action);
					return TRUE;
				}

				case OpInputAction::ACTION_SELECT_ALTERNATE_CSS_FILE:
				{
					WindowCommanderProxy::SelectAlternateCSSFile(GetWindowCommander(), child_action);
					return TRUE;
				}

				case OpInputAction::ACTION_MANAGE_MODES:
				case OpInputAction::ACTION_SET_ENCODING:
				{
					child_action->SetEnabled(IsBrowserViewActive());
					return TRUE;
				}

				case OpInputAction::ACTION_MEDIA_PLAY:
				case OpInputAction::ACTION_MEDIA_PAUSE:
				case OpInputAction::ACTION_MEDIA_MUTE:
				case OpInputAction::ACTION_MEDIA_UNMUTE:
				case OpInputAction::ACTION_MEDIA_SHOW_CONTROLS:
				case OpInputAction::ACTION_MEDIA_HIDE_CONTROLS:
				{
					BOOL enabled = FALSE;
					OpDocumentContext* ctx = g_application->GetLastSeenDocumentContext();
					if (ctx && ctx->IsMediaAvailable())
					{
						switch (child_action->GetAction())
						{
						case OpInputAction::ACTION_MEDIA_PLAY:
						case OpInputAction::ACTION_MEDIA_PAUSE:
							enabled = ctx->IsMediaPlaying() != (child_action->GetAction() == OpInputAction::ACTION_MEDIA_PLAY);
							break;
						case OpInputAction::ACTION_MEDIA_MUTE:
						case OpInputAction::ACTION_MEDIA_UNMUTE:
							enabled = ctx->IsMediaMuted() != (child_action->GetAction() == OpInputAction::ACTION_MEDIA_MUTE);
							break;
						case OpInputAction::ACTION_MEDIA_SHOW_CONTROLS:
						case OpInputAction::ACTION_MEDIA_HIDE_CONTROLS:
							enabled = ctx->HasMediaControls() != (child_action->GetAction() == OpInputAction::ACTION_MEDIA_SHOW_CONTROLS);
							break;
						}
					}
					child_action->SetEnabled(enabled);
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_GET_TYPED_OBJECT:
		{
			if (action->GetActionData() == GetType())
			{
				action->SetActionObject(this);
				return TRUE;
			}
			break;
		}

		case OpInputAction::ACTION_SAVE_FRAME_AS:
		{
			if (IsBrowserViewActive())
				SaveDocument(TRUE);

			return TRUE;
		}

		case OpInputAction::ACTION_SAVE_DOCUMENT:
		case OpInputAction::ACTION_SAVE_DOCUMENT_AS:
		{
			if (IsBrowserViewActive())
				SaveActiveDocumentAsFile();

			return TRUE;
		}

		case OpInputAction::ACTION_SHOW_FINDTEXT:
		{
			if (!IsBrowserViewActive())
				break;

			ShowFindTextBar(action->GetActionData());
			return TRUE;
		}

		case OpInputAction::ACTION_RELOAD:
		case OpInputAction::ACTION_FORCE_RELOAD:
		{
			OpAddressDropDown* add = GetAddressDropdown();
			if (add)
				add->SetIgnoreEditFocus();

			if (IsBrowserViewActive())
				ShowPluginCrashedBar(false);

			break; // Action will bubbles up in the chain
		}

		case OpInputAction::ACTION_MANAGE_SEARCH_ENGINES:
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_PREFERENCES, NewPreferencesDialog::SearchPage);
			return TRUE;
		}

		case OpInputAction::ACTION_ENTER_FULLSCREEN:
		{
			if( !GetWorkspace() )
			{
				if (!IsFullscreen())
				{
					LockUpdate(TRUE);
					Fullscreen(TRUE);
					LockUpdate(FALSE);
					return TRUE;
				}
				return FALSE;
			}
			break;
		}
		case OpInputAction::ACTION_LEAVE_FULLSCREEN:
		{
			if( !GetWorkspace() )
			{
				if (IsFullscreen())
				{
					LockUpdate(TRUE);
					Fullscreen(FALSE);
					LockUpdate(FALSE);
					return TRUE;
				}
				return FALSE;
			}
			break;
		}
#ifdef _PRINT_SUPPORT_
		case OpInputAction::ACTION_LEAVE_PRINT_PREVIEW:
		{
			if(WindowCommanderProxy::GetPreviewMode(GetWindowCommander()))
			{
				WindowCommanderProxy::TogglePrintMode(GetWindowCommander());
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_PRINT_PREVIEW:
		case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_AS_SCREEN:
		{
			if (!WindowCommanderProxy::GetPreviewMode(GetWindowCommander()))
			{
				WindowCommanderProxy::SetFramesPrintType(GetWindowCommander(), PRINT_AS_SCREEN, FALSE);
				WindowCommanderProxy::TogglePrintMode(GetWindowCommander());
				return TRUE;
			}
			else if (action->GetAction() == OpInputAction::ACTION_PRINT_PREVIEW)
			{
				WindowCommanderProxy::TogglePrintMode(GetWindowCommander());
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ONE_FRAME_PER_SHEET:
		{
			if (WindowCommanderProxy::HasFramesPrintType(GetWindowCommander(), PRINT_AS_SCREEN))
			{
				WindowCommanderProxy::SetFramesPrintType(GetWindowCommander(), PRINT_ALL_FRAMES, TRUE);
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ACTIVE_FRAME:
		{
			if (WindowCommanderProxy::HasFramesPrintType(GetWindowCommander(), PRINT_ALL_FRAMES))
			{
				WindowCommanderProxy::SetFramesPrintType(GetWindowCommander(), PRINT_ACTIVE_FRAME, TRUE);

				if (!WindowCommanderProxy::HasFramesPrintType(GetWindowCommander(), PRINT_ALL_FRAMES))
				{
					// not a frames doc - use only two states
					return TRUE;
				}
			}
			return FALSE;
		}
#endif // _PRINT_SUPPORT_
#ifdef ACTION_TOGGLE_SYSTEM_FULLSCREEN_ENABLED
		case OpInputAction::ACTION_TOGGLE_SYSTEM_FULLSCREEN:
		{
			if( !GetWorkspace() )
			{
				if (IsFullscreen())
					g_input_manager->InvokeAction(OpInputAction::ACTION_LEAVE_FULLSCREEN);
				else if (IsMaximized())
					Restore();
				else
					Maximize();
				return TRUE;
			}
			break;
		}
#endif // ACTION_TOGGLE_SYSTEM_FULLSCREEN_ENABLED
		case OpInputAction::ACTION_SELECT_USER_CSS_FILE:
		case OpInputAction::ACTION_DESELECT_USER_CSS_FILE:
		{

			UINT32 index = action->GetActionData();
			BOOL is_active = g_pcfiles->IsLocalCSSActive(index);
			BOOL set_active = action->GetAction() == OpInputAction::ACTION_SELECT_USER_CSS_FILE;

			if (is_active == set_active)
				return FALSE;

			TRAPD(err, g_pcfiles->WriteLocalCSSActiveL(index, set_active));
			if (err == OpStatus::ERR_OUT_OF_RANGE)
			{/*Do we want to do something?*/}

			WindowCommanderProxy::UpdateWindow(GetWindowCommander());
			return TRUE;
		}
		case OpInputAction::ACTION_SELECT_ALTERNATE_CSS_FILE:
		{
			if(action->GetActionDataString())
				GetWindowCommander()->SetAlternateCss(action->GetActionDataString());
			return TRUE;
		}
		case OpInputAction::ACTION_GO_TO_LINK_ELEMENT:
		{
			OpAutoVector<OpString> newsfeed_links;

			OpInputAction* cur_action = action;
			while (cur_action != 0)
			{
				if (cur_action->GetActionDataString())
				{
					LinkElementInfo information;
					UINT32 count = win_comm->GetLinkElementCount();
					UINT i;
					for (i = 0; i < count; i++)
					{
						win_comm->GetLinkElementInfo(i, &information);

						if (information.rel &&
							information.type &&
							(uni_stricmp(UNI_L("alternate"),information.rel) == 0 ||
							uni_stricmp(UNI_L("service.feed"),information.rel) == 0) &&
							uni_stricmp(cur_action->GetActionDataString(), information.type) == 0 )
						{
							URL url = g_url_api->GetURL(information.href);

							if (url.GetAttribute(URL::KType) == URL_HTTP || url.GetAttribute(URL::KType) == URL_HTTPS)
							{
								// Add this item to the vector of newsfeed links.
								// The vector contains alternate pairs of title
								// and href.
								OpString *title_string = OP_NEW(OpString, ());
								if (title_string)
								{
									OpString *href_string = OP_NEW(OpString, ());
									if (href_string)
									{
										href_string->Set(information.href);
										title_string->Set(information.title);

										newsfeed_links.Add(title_string);
										newsfeed_links.Add(href_string);
									}
									else
									{
										OP_DELETE(title_string);
									}
								}
							}
						}
						else if ((information.rel && uni_stricmp(cur_action->GetActionDataString(),information.rel) == 0) ||
							(information.rel && information.type &&
							 uni_stricmp(UNI_L("alternate"),information.rel) == 0 &&
							 uni_stricmp(cur_action->GetActionDataString(), information.type) == 0)
							 )
						{
							{
								GoToPage(information.href, FALSE);
								return TRUE;
							}
						}
					}
				}
				cur_action = cur_action->GetNextInputAction();
			}

			if (newsfeed_links.GetCount() == 2)
			{
				const OpStringC* href_string = newsfeed_links.Get(1);
				OP_ASSERT(href_string != 0);

				g_application->GoToPage(href_string->CStr());

				return TRUE;
			}
			else if (newsfeed_links.GetCount() > 2)
			{
				// Show a popup menu with the feeds to choose from.
				g_application->GetMenuHandler()->ShowPopupMenu("Internal Select Subscribe Feed",
						PopupPlacement::AnchorAtCursor(), INTPTR(&newsfeed_links), action->IsKeyboardInvoked());

				return TRUE;
			}

			  return FALSE;
		}

		case OpInputAction::ACTION_PASTE_AND_GO:
		case OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND:
		{
			OpString text;
			g_desktop_clipboard_manager->GetText(text);
			if (text.HasContent())
			{
				// Emulate a paste into the edit field without changing its contents
				// as we may open the result in a new tab/window
				OpAddressDropDown* dropdown = GetAddressDropdown();
				if (dropdown)
					dropdown->EmulatePaste(text);

				// This is a bit tricky. Paste&Go has a default shortcut that includes Ctrl and Shift which
				// normally redirects a url to a new tab. That is why we test IsKeyboardInvoked() below. In
				// addition the 'NewWindow' pref tells us to open in a new tab/window no matter what (DSK-313944)
				BOOL bg = action->GetAction() == OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND;
				if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
				{
					if (g_application->IsSDI())
						g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW, 0, text.CStr());
					else
						g_application->GoToPage(text, TRUE, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
				}
				else
					g_application->GoToPage(text.CStr(), bg, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
			}
			return TRUE;
		}

#ifdef _X11_SELECTION_POLICY_
		case OpInputAction::ACTION_PASTE_SELECTION_AND_GO:
		case OpInputAction::ACTION_PASTE_SELECTION_AND_GO_BACKGROUND:
		{
			OpString text;
			g_desktop_clipboard_manager->GetText(text, true);
			if (text.HasContent())
			{
				// Emulate a paste into the edit field without changing its contents
				// as we may open the result in a new tab/window
				OpAddressDropDown* dropdown = GetAddressDropdown();
				if (dropdown)
					dropdown->EmulatePaste(text);

				BOOL bg = action->GetAction() == OpInputAction::ACTION_PASTE_SELECTION_AND_GO_BACKGROUND;
				if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
				{
					if (g_application->IsSDI())
						g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW, 0, text.CStr());
					else
						g_application->GoToPage(text, TRUE, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
				}
				else
					g_application->GoToPage(text.CStr(), bg, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
			}
			return TRUE;
		}
#endif

		case OpInputAction::ACTION_VALIDATE_DOCUMENT_SOURCE:
		case OpInputAction::ACTION_VALIDATE_FRAME_SOURCE:
		{
#ifdef _VALIDATION_SUPPORT_
			if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowValidationDialog))
			{
				ValidateSourceDialog* dialog = OP_NEW(ValidateSourceDialog, ());
				if (dialog)
					dialog->Init(g_application->GetActiveDesktopWindow(), win_comm);
			}
			else
			{
				WindowCommanderProxy::UploadFileForValidation(win_comm);
			}
#endif
			return TRUE;
		}

		case OpInputAction::ACTION_SET_AUTOMATIC_RELOAD:
		{
			int timeout = action->GetActionData();

			if (timeout == 0)
			{			
				AutoWindowReloadController* controller = OP_NEW(AutoWindowReloadController, (win_comm,NULL));
	
				RETURN_IF_ERROR(ShowDialog(controller, g_global_ui_context, g_application->GetActiveDesktopWindow()));
			}
			else
			{
				WindowCommanderProxy::GetUserAutoReload(win_comm)->SetSecUserSetting(timeout);
				WindowCommanderProxy::GetUserAutoReload(win_comm)->Enable( TRUE);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_ADD_TO_BOOKMARKS:
		{
			INT32 id = -1;
			BookmarkItemData item_data;
			WindowCommanderProxy::AddToBookmarks(win_comm, item_data, action->GetActionData(), id);
			g_desktop_bookmark_manager->NewBookmark(item_data, id, this, TRUE);
			return TRUE;
		}

		case OpInputAction::ACTION_BACK:
		case OpInputAction::ACTION_FORWARD:
		{
#ifdef DISABLE_NAVIGATION_FROM_EDIT_WIDGET
			if (action->IsKeyboardInvoked() && action->GetFirstInputContext()->GetType() == WIDGET_TYPE_EDIT)
				return TRUE;
#endif // DISABLE_NAVIGATION_FROM_EDIT_WIDGET

			bool is_back = action->GetAction() == OpInputAction::ACTION_BACK;
			if (is_back && !m_doc_history->HasBackHistory() || !is_back && !m_doc_history->HasForwardHistory())
				return FALSE;

			OpAddressDropDown* add = GetAddressDropdown();
			if (add)
				add->SetIgnoreEditFocus();

			INT32 pos = m_doc_history->MoveInHistory(action->GetAction() == OpInputAction::ACTION_BACK);

			HistoryInformation result;
			if (OpStatus::IsSuccess(m_doc_history->GetHistoryInformation(DocumentHistory::Position(DocumentHistory::Position::NORMAL, pos), &result)))
				OpStatus::Ignore(SetActiveDocumentView(result.url));

			return TRUE;
		}

		case OpInputAction::ACTION_FAST_FORWARD:
		{
#ifdef DISABLE_NAVIGATION_FROM_EDIT_WIDGET
			if (action->IsKeyboardInvoked() && action->GetFirstInputContext()->GetType() == WIDGET_TYPE_EDIT)
				return TRUE;
#endif // DISABLE_NAVIGATION_FROM_EDIT_WIDGET

			DocumentHistory::Position pos = m_doc_history->MoveInFastHistory(false);
			HistoryInformation result;
			if (OpStatus::IsSuccess(m_doc_history->GetHistoryInformation(pos, &result)))
				OpStatus::Ignore(SetActiveDocumentView(result.url));

			return TRUE;
		}

		case OpInputAction::ACTION_REWIND:
		{
			DocumentHistory::Position pos = m_doc_history->MoveInFastHistory(true);

			HistoryInformation result;
			if (OpStatus::IsSuccess(m_doc_history->GetHistoryInformation(pos, &result)))
				OpStatus::Ignore(SetActiveDocumentView(result.url));

			return TRUE;
		}

		case OpInputAction::ACTION_GO_TO_HISTORY:
		{
			INTPTR data = action->GetActionData();
			if (data < 0)
				m_doc_history->GotoHistoryPos(DocumentHistory::Position(DocumentHistory::Position::FAST_FORWARD, - 1 - data));
			else
				m_doc_history->GotoHistoryPos(DocumentHistory::Position(DocumentHistory::Position::NORMAL, data));

			return TRUE;
		}

		case OpInputAction::ACTION_SHOW_ADDRESS_DROPDOWN:
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_ADDRESS_FIELD);
			g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_DROPDOWN);
			return TRUE;
		}

		case OpInputAction::ACTION_FOCUS_ADDRESS_FIELD:
		{
			if (GetAddressDropdown())
			{
				GetAddressDropdown()->SetFocus(FOCUS_REASON_ACTION);
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_FOCUS_SEARCH_FIELD:
		{
			if (GetSearchDropdown())
			{
				GetSearchDropdown()->SetFocus(FOCUS_REASON_ACTION);
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_FOCUS_PAGE:
		{
			WindowCommanderProxy::FocusPage(win_comm);
			m_document_view->SetFocus(FOCUS_REASON_ACTION);
			return TRUE;
		}

		case OpInputAction::ACTION_ENABLE_HANDHELD_MODE:
		case OpInputAction::ACTION_DISABLE_HANDHELD_MODE:
		{
			// typically ends here when focus is not in a BrowserView
			win_comm->SetLayoutMode(win_comm->GetLayoutMode() == OpWindowCommander::CSSR ? OpWindowCommander::NORMAL : OpWindowCommander::CSSR);
			return TRUE;
		}

		case OpInputAction::ACTION_ENABLE_MSR:
		case OpInputAction::ACTION_DISABLE_MSR:
			win_comm->SetLayoutMode((win_comm->GetLayoutMode() == OpWindowCommander::ERA) ? OpWindowCommander::NORMAL : OpWindowCommander::ERA);
			return TRUE;

		case OpInputAction::ACTION_ENABLE_SCROLL_BARS:
		case OpInputAction::ACTION_DISABLE_SCROLL_BARS:
		{
    		win_comm->SetShowScrollbars(!WindowCommanderProxy::ShowScrollbars(win_comm));
			return TRUE;
		}

		case OpInputAction::ACTION_DUPLICATE_PAGE:
		{
			Window *new_window=0;
			WindowCommanderProxy::CloneWindow(win_comm, &new_window);
			return TRUE;
		}

		case OpInputAction::ACTION_ENTER_ACCESS_KEY_MODE:
		{
			if (!win_comm->GetAccessKeyMode())
			{
				win_comm->SetAccessKeyMode(TRUE);
				if(!m_cycles_accesskeys_popupwindow)
				{
					m_cycles_accesskeys_popupwindow = OP_NEW(CycleAccessKeysPopupWindow, (this));
					if (m_cycles_accesskeys_popupwindow)
					{
						if (OpStatus::IsSuccess(m_cycles_accesskeys_popupwindow->init_status) &&
							OpStatus::IsSuccess(m_cycles_accesskeys_popupwindow->Init()))
						{
							WindowCommanderProxy::UpdateAccessKeys(GetWindowCommander(), m_cycles_accesskeys_popupwindow);
							m_cycles_accesskeys_popupwindow->UpdateDisplay();
							m_cycles_accesskeys_popupwindow->Show(TRUE);
							// Revert focus to window. Needed for UNIX as (in evenes) OpWindow::STYLE_POPUP windows will set input context 
							// and in this case we do not want that.
							RestoreFocus(FOCUS_REASON_OTHER);
						}
						else
						{
							OP_DELETE(m_cycles_accesskeys_popupwindow);
							m_cycles_accesskeys_popupwindow = NULL;
						}
					}
				}
				else
				{
					WindowCommanderProxy::UpdateAccessKeys(GetWindowCommander(), m_cycles_accesskeys_popupwindow);
					m_cycles_accesskeys_popupwindow->UpdateDisplay();
					m_cycles_accesskeys_popupwindow->Show(TRUE);
				}
				return TRUE;
			}
			break;
		}

		case OpInputAction::ACTION_LEAVE_ACCESS_KEY_MODE:
		{
			if (win_comm->GetAccessKeyMode())
			{
				win_comm->SetAccessKeyMode(FALSE);
				if(m_cycles_accesskeys_popupwindow)
				{
					m_cycles_accesskeys_popupwindow->Close(TRUE);
				}
				return TRUE;
			}
			break;
		}

		case OpInputAction::ACTION_DISABLE_AUTOMATIC_RELOAD:
		{
			WindowCommanderProxy::GetUserAutoReload(win_comm)->Enable(FALSE);
			return TRUE;
		}

		case OpInputAction::ACTION_CREATE_SEARCH:
		{
			WindowCommanderProxy::CreateSearch(GetWindowCommander(), this, GetTitle(TRUE));
			return TRUE;
		}

		case OpInputAction::ACTION_HIGHLIGHT_BOOKMARK:
		{
			HotlistModel* model = g_hotlist_manager->GetBookmarksModel();
			HotlistModelItem* item = model->GetByURL( m_current_url );

			BrowserDesktopWindow* bdw = ((BrowserDesktopWindow*)GetParentDesktopWindow());

			if (item && bdw)
			{
				Hotlist* hotlist = bdw->GetHotlist();
				BookmarksPanel* panel = (BookmarksPanel*)hotlist->GetPanelByType(Hotlist::PANEL_TYPE_BOOKMARKS);

				if (panel)
				{
					OpTreeModelItem* selected = panel->GetSelectedItem();

					if (selected == item && hotlist->GetAlignment() != OpBar::ALIGNMENT_OFF &&
						hotlist->GetPanel(hotlist->GetSelectedPanel()) == panel)
					{
						hotlist->SetAlignment(OpBar::ALIGNMENT_OFF, TRUE);
						return TRUE;
					}
					panel->SetSelectedItemByID(item->GetID());
					g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PANEL,0, UNI_L("Bookmarks"));
					return TRUE;
				}
			}
			return FALSE;
		}
		case OpInputAction::ACTION_HIGHLIGHT_NOTE:
		{
			HotlistModel* model = g_hotlist_manager->GetNotesModel();
			HotlistModelItem* item = model->GetByURL( m_current_url );
			BrowserDesktopWindow* bdw = ((BrowserDesktopWindow*)GetParentDesktopWindow());

			if (item && bdw)
			{
				Hotlist* hotlist = bdw->GetHotlist();
				NotesPanel* panel = (NotesPanel*)hotlist->GetPanelByType(Hotlist::PANEL_TYPE_NOTES);

				if (panel)
				{
					//OpTreeView* view = panel->GetHotlistView()->GetItemView();
					OpTreeModelItem* selected = panel->GetSelectedItem();

					if (selected == item && hotlist->GetAlignment() != OpBar::ALIGNMENT_OFF &&
						hotlist->GetPanel(hotlist->GetSelectedPanel()) == panel)
					{
						hotlist->SetAlignment(OpBar::ALIGNMENT_OFF, TRUE);
						return TRUE;
					}
					panel->SetSelectedItemByID(item->GetID());
					g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PANEL,0, UNI_L("Notes"));
					return TRUE;
				}
			}
			return FALSE;
		}

#ifdef SUPPORT_VISUAL_ADBLOCK
		case OpInputAction::ACTION_CONTENTBLOCK_MODE_ON:
		case OpInputAction::ACTION_CONTENTBLOCK_MODE_OFF:
		{
			if(KioskManager::GetInstance()->GetEnabled())
			{
				return TRUE;
			}

			WindowCommanderProxy::ToggleAdblock(GetWindowCommander(), m_popup_adblock_bar, m_adblock_edit_mode, m_content_to_block);

			if(m_popup_adblock_bar)
			{
				OpString status_text;
				g_languageManager->GetString(Str::S_CLICK_ON_CONTENT_TO_BLOCK, status_text);
				m_popup_adblock_bar->SetStatusText(status_text);
			}
			else if (m_focus_document)
			{
				m_document_view->SetFocus(FOCUS_REASON_OTHER);
			}

			return TRUE;
		}

		case OpInputAction::ACTION_CONTENTBLOCK_DETAILS:
		{
			BrowserDesktopWindow::OpenContentBlockDetails(this, action, m_content_to_block);
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_CONTENTBLOCK_CANCEL:
		case OpInputAction::ACTION_CONTENTBLOCK_ACCEPT:
		{
			BOOL accept = (action->GetAction() == OpInputAction::ACTION_CONTENTBLOCK_ACCEPT);
			WindowCommanderProxy::AdblockEditDone(GetWindowCommander(),
												  m_popup_adblock_bar,
												  m_adblock_edit_mode,
												  accept,
												  m_content_to_block,
												  m_content_to_unblock);

			if (m_focus_document)
			{
				m_document_view->SetFocus(FOCUS_REASON_OTHER);
			}
			return TRUE;
		}
#endif // SUPPORT_VISUAL_ADBLOCK

		case OpInputAction::ACTION_ALWAYS_USE_WEBHANDLER:
		case OpInputAction::ACTION_NEVER_USE_WEBHANDLER:
		case OpInputAction::ACTION_DO_NOT_USE_WEBHANDLER_NOW:
		{
			m_webhandler_bar->Hide();

			DesktopPermissionCallback* callback = m_permission_callbacks.First();
			if (callback && callback->GetPermissionCallback())
			{
				PermissionCallback* permission_callback = callback->GetPermissionCallback();
				BOOL remember = action->GetAction() != OpInputAction::ACTION_DO_NOT_USE_WEBHANDLER_NOW;

				int choices = action->GetAction() == OpInputAction::ACTION_ALWAYS_USE_WEBHANDLER ?
					permission_callback->AffirmativeChoices() : permission_callback->NegativeChoices();

				OpPermissionListener::PermissionCallback::PersistenceType reply;
				if (remember && (choices & OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS))
					reply = OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS;
				else if (!remember && (choices & OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME))
					reply = OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME;
				else if (!remember && (choices & OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_SESSION))
					reply = OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_SESSION;
				else if (!remember && (choices & OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_NONE))
					reply = OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_NONE;
				else
				{
					OP_ASSERT(!"No valid permission reply possible. Using PERSISTENCE_TYPE_ALWAYS");
					reply = OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS;
				}
				if (action->GetAction() == OpInputAction::ACTION_ALWAYS_USE_WEBHANDLER)
					permission_callback->OnAllowed(reply);
				else
					permission_callback->OnDisallowed(reply);
			}

			// Remove callback and restart if there are more requests pending
			if (callback)
				m_permission_callbacks.DeleteItem(callback);
			if (m_permission_callbacks.GetCount())
				g_main_message_handler->PostMessage(MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) this, 0);

			return TRUE;
		}
		break;

		case OpInputAction::ACTION_MEDIA_PLAY:
		case OpInputAction::ACTION_MEDIA_PAUSE:
		case OpInputAction::ACTION_MEDIA_MUTE:
		case OpInputAction::ACTION_MEDIA_UNMUTE:
		case OpInputAction::ACTION_MEDIA_SHOW_CONTROLS:
		case OpInputAction::ACTION_MEDIA_HIDE_CONTROLS:
		{
			// FIXME: We don't _really_ know which image the action refer to.
			OpDocumentContext* ctx = g_application->GetLastSeenDocumentContext();
			if (!ctx)
				return FALSE;

			switch (action->GetAction())
			{
			case OpInputAction::ACTION_MEDIA_PLAY:
			case OpInputAction::ACTION_MEDIA_PAUSE:
				OpStatus::Ignore(GetWindowCommander()->MediaPlay(*ctx, action->GetAction() == OpInputAction::ACTION_MEDIA_PLAY));
				break;
			case OpInputAction::ACTION_MEDIA_MUTE:
			case OpInputAction::ACTION_MEDIA_UNMUTE:
				OpStatus::Ignore(GetWindowCommander()->MediaMute(*ctx, action->GetAction() == OpInputAction::ACTION_MEDIA_MUTE));
				break;
			case OpInputAction::ACTION_MEDIA_SHOW_CONTROLS:
			case OpInputAction::ACTION_MEDIA_HIDE_CONTROLS:
				OpStatus::Ignore(GetWindowCommander()->MediaShowControls(*ctx, action->GetAction() == OpInputAction::ACTION_MEDIA_SHOW_CONTROLS));
				break;
			}
			return TRUE;
		}

#ifdef SCOPE_ECMASCRIPT_DEBUGGER

		case OpInputAction::ACTION_INSPECT_ELEMENT:
		{
			// Set up context so that we have a way to get to the active element that user
			// tries to inspect using keyboard or mouse button shortcut (without using context menu).
			DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());

			if (menu_context && menu_context->GetDocumentContext())
			{
				// Inspect using context provided by desktop
				OpStatus::Ignore(win_comm->DragonflyInspect(*menu_context->GetDocumentContext()));
			}
			else
			{
				// Inspect using context created for given position (only thing that works when invoking menu by keyboard for example)
				OpDocumentContext* context;
				OpRect position;
				action->GetActionPosition(position);
				if (OpStatus::IsSuccess(GetWindowCommander()->CreateDocumentContextForScreenPos(&context, OpPoint(position.Left(), position.Top()))))
				{
					OpStatus::Ignore(win_comm->DragonflyInspect(*context));
					OP_DELETE(context);
				}
			}

#ifdef INTEGRATED_DEVTOOLS_SUPPORT
			g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_DEVTOOLS_WINDOW);
#endif
			return TRUE;
		}
#endif // SCOPE_ECMASCRIPT_DEBUGGER
	}

	DocumentView* doc_view = GetActiveDocumentView();
	if (doc_view && doc_view->GetDocumentType() == DocumentView::DOCUMENT_TYPE_BROWSER)
	{
		if (!action->IsLowlevelAction() && doc_view->OnInputAction(action))
			return TRUE;
	}
	else if (doc_view && doc_view->OnInputAction(action))
		return TRUE;

	if (DesktopWindow::OnInputAction(action))
		return TRUE;

	return WindowCommanderProxy::HandlePlatformAction(GetWindowCommander(), action, TRUE);
}

void DocumentDesktopWindow::OnSettingsChanged(DesktopSettings* settings)
{
	short indicator_type = 0;
	bool is_camera_active = false;
	bool is_geolocation_active = false;
	if (settings->IsChanged(SETTINGS_TOOLBAR_CONTENTS) || settings->IsChanged(SETTINGS_TOOLBAR_SETUP))
	{
		// We must read current indicator state before we call DesktopWindow::OnSettingsChanged
		// as it will cause recreation of OpBar which contains address bar
		OpIndicatorButton* indicator_button = GetAddressDropdown() && GetAddressDropdown()->GetProtocolButton() ?
			GetAddressDropdown()->GetProtocolButton()->GetIndicatorButton() :
			NULL;
		if (indicator_button)
		{
			indicator_type = indicator_button->GetIndicatorType();
			is_camera_active = indicator_button->IsCameraActive();
			is_geolocation_active = indicator_button->IsGeolocationActive();
		}
	}

	DesktopWindow::OnSettingsChanged(settings);
	if (settings->IsChanged(SETTINGS_PERSONA))
	{
		UpdateMDETransparency();
	}

	if (settings->IsChanged(SETTINGS_LANGUAGE))
	{
		OpString str;
		DocumentView* doc_view = GetActiveDocumentView();
		if (doc_view && OpStatus::IsSuccess(doc_view->GetTitle(str)))
			SetTitle(str);
	}

	if (settings->IsChanged(SETTINGS_TOOLBAR_CONTENTS) || settings->IsChanged(SETTINGS_TOOLBAR_SETUP))
	{
		// Reinstate address's favorite/star button and badge indicator
		OpAddressDropDown *dropdown = GetAddressDropdown();
		if (dropdown)
		{
			dropdown->EnableFavoritesButton(true);
			if (indicator_type && dropdown->GetProtocolButton())
			{
				OpIndicatorButton* indicator_button = dropdown->GetProtocolButton()->GetIndicatorButton();
				indicator_button->SetIndicatorType(indicator_type);
				indicator_button->SetIconState(OpIndicatorButton::CAMERA, is_camera_active ?
						OpIndicatorButton::ACTIVE :
						OpIndicatorButton::INACTIVE);
				indicator_button->SetIconState(OpIndicatorButton::GEOLOCATION, is_geolocation_active ?
						OpIndicatorButton::ACTIVE :
						OpIndicatorButton::INACTIVE);
			}
		}
		return;
	}
}


BOOL DocumentDesktopWindow::HandleImageActions(OpInputAction* action, OpWindowCommander* wc)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_LOAD_IMAGE_NON_TURBO:
				{
					BOOL enabled = FALSE;
					if (child_action->GetActionData())
					{
						DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(child_action->GetActionData());
						if (dmctx->GetDocumentContext() && dmctx->GetDocumentContext()->HasImage() && !dmctx->GetDocumentContext()->HasFullQualityImage())
						{
							enabled = TRUE;
						}
					}
					child_action->SetEnabled(enabled);
					return TRUE;
				}
				case OpInputAction::ACTION_OPEN_IMAGE:
				case OpInputAction::ACTION_RELOAD_IMAGE:
				case OpInputAction::ACTION_OPEN_IMAGE_ADDRESS:
				case OpInputAction::ACTION_GO_TO_IMAGE:
				case OpInputAction::ACTION_LOAD_IMAGE:
				case OpInputAction::ACTION_COPY_IMAGE_ADDRESS:
				case OpInputAction::ACTION_SHOW_IMAGE_PROPERTIES:
				{
					BOOL enabled = FALSE;
					if (wc && child_action->GetActionData())
					{
						DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(child_action->GetActionData());
						enabled = dmctx->GetDocumentContext() ? dmctx->GetDocumentContext()->HasImage() : FALSE;
					}
					child_action->SetEnabled( enabled );
					return TRUE;
				}

				case OpInputAction::ACTION_USE_IMAGE_AS_SPEEDDIAL_BACKGROUND:
				{
					if (!g_speeddial_manager->HasLoadedConfig())
					{
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
				}
				// fall through

				case OpInputAction::ACTION_SAVE_MEDIA:
				case OpInputAction::ACTION_COPY_MEDIA_ADDRESS:
				case OpInputAction::ACTION_SAVE_IMAGE:
				case OpInputAction::ACTION_COPY_IMAGE:
				{
					BOOL enabled = FALSE;
					if (wc && child_action->GetActionData())
					{
						DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(child_action->GetActionData());
						if (dmctx->GetDocumentContext() &&
							((dmctx->GetDocumentContext()->HasImage() && dmctx->GetDocumentContext()->HasCachedImageData()) || dmctx->GetDocumentContext()->IsMediaAvailable()))
						{
							enabled = TRUE;
						}
					}
					child_action->SetEnabled( enabled );
					return TRUE;
				}
				case OpInputAction::ACTION_COPY_BACKGROUND_IMAGE_ADDRESS:
				case OpInputAction::ACTION_SHOW_BACKGROUND_IMAGE_PROPERTIES:
				case OpInputAction::ACTION_OPEN_BACKGROUND_IMAGE:
				{
					BOOL enabled = FALSE;
					if (wc && child_action->GetActionData())
					{
						DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(child_action->GetActionData());
						enabled = dmctx->GetDocumentContext() ? dmctx->GetDocumentContext()->HasBGImage() : FALSE;
					}
					child_action->SetEnabled( enabled );
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_BACKGROUND_IMAGE:
				case OpInputAction::ACTION_COPY_BACKGROUND_IMAGE:
				case OpInputAction::ACTION_SAVE_BACKGROUND_IMAGE:
				{
					BOOL enabled = FALSE;
					if (wc && child_action->GetActionData())
					{
						DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(child_action->GetActionData());
						enabled = dmctx->GetDocumentContext() ? dmctx->GetDocumentContext()->HasBGImage() && dmctx->GetDocumentContext()->HasCachedBGImageData() : FALSE;
					}
					child_action->SetEnabled( enabled );
					return TRUE;
				}
				case OpInputAction::ACTION_USE_IMAGE_AS_FOREGROUND_SKIN:
				case OpInputAction::ACTION_USE_IMAGE_AS_BACKGROUND_SKIN:
				case OpInputAction::ACTION_USE_BACKGROUND_IMAGE_AS_FOREGROUND_SKIN:
				case OpInputAction::ACTION_USE_BACKGROUND_IMAGE_AS_BACKGROUND_SKIN:
				{
					child_action->SetEnabled( FALSE );
					return TRUE;
				}
			}
			return FALSE;
		}
		break;

		case OpInputAction::ACTION_OPEN_IMAGE:
		{
			if (action->GetActionData())
			{
				DesktopMenuContext * dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext() && dmctx->GetDocumentContext()->HasImage())
				{
					OpString address;
					URL_CONTEXT_ID url_context = dmctx->GetDocumentContext()->GetImageAddressContext();
					dmctx->GetDocumentContext()->GetImageAddress(&address);
					if (address.HasContent())
						g_application->OpenURL(address, MAYBE, MAYBE, MAYBE, url_context);
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_OPEN_BACKGROUND_IMAGE:
		{
			if (action->GetActionData())
			{
				DesktopMenuContext * dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext() && dmctx->GetDocumentContext()->HasBGImage())
				{
					OpString address;
					URL_CONTEXT_ID url_context = dmctx->GetDocumentContext()->GetBGImageAddressContext();
					dmctx->GetDocumentContext()->GetBGImageAddress(&address);
					if (address.HasContent())
						g_application->OpenURL(address, MAYBE, MAYBE, MAYBE, url_context);
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_RELOAD_IMAGE:
		case OpInputAction::ACTION_OPEN_IMAGE_ADDRESS:
		case OpInputAction::ACTION_GO_TO_IMAGE:
		case OpInputAction::ACTION_LOAD_IMAGE:
		case OpInputAction::ACTION_LOAD_IMAGE_NON_TURBO:
		{
			if (wc && action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext())
				{
					BOOL disable_turbo = FALSE;
					if (action->GetAction() == OpInputAction::ACTION_LOAD_IMAGE_NON_TURBO && !dmctx->GetDocumentContext()->HasFullQualityImage())
					{
						disable_turbo = TRUE;
					}
					wc->LoadImage(*dmctx->GetDocumentContext(), disable_turbo);
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_BACKGROUND_IMAGE:
		{
			// TODO use context
			WindowCommanderProxy::ShowBackgroundImage(wc);
			return TRUE;
		}

		case OpInputAction::ACTION_SAVE_MEDIA:
		{
			if (wc && action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx && dmctx->GetDocumentContext() && dmctx->GetDocumentContext()->IsMediaAvailable())
				{
					OpString obj_addr;
					if (OpStatus::IsSuccess(dmctx->GetDocumentContext()->GetMediaAddress(&obj_addr)))
					{
						URL url = g_url_api->GetURL(obj_addr.CStr());
						if (!url.IsEmpty() && (!url.GetAttribute(URL::KMultimedia) || !url.IsExportAllowed()))
						{
							if (OpStatus::IsError(url.SetAttribute(URL::KUnique, TRUE)))
							{
								url = g_url_api->GetURL(obj_addr.CStr(), NULL, TRUE);
							}
						}
						if (!url.IsEmpty())
						{
#ifdef WIC_USE_DOWNLOAD_CALLBACK
							FramesDocument* doc = WindowCommanderProxy::GetTopDocument(wc);
							if(!doc)
								return TRUE;

							DocumentManager* document_manager=doc->GetDocManager();
							WindowCommander* wic = document_manager->GetWindow()->GetWindowCommander();
							
							ViewActionFlag view_action_flag;
							view_action_flag.Set(ViewActionFlag::SAVE_AS);

							TransferManagerDownloadCallback* download_callback = OP_NEW(TransferManagerDownloadCallback, (document_manager, url, NULL, view_action_flag));
							if(download_callback && wic)
							{
								wic->GetDocumentListener()->OnDownloadRequest(wic, download_callback);
								download_callback->Execute();
							}
							else
							{
								OP_DELETE(download_callback);
							}
#endif //WIC_USE_DOWNLOAD_CALLBACK
						}
					}
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SAVE_BACKGROUND_IMAGE:
		{
			// TODO use context
			return TRUE;
		}

		case OpInputAction::ACTION_COPY_MEDIA_ADDRESS:
		case OpInputAction::ACTION_COPY_IMAGE_ADDRESS:
		{
			if (wc && action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext() && ( dmctx->GetDocumentContext()->HasImage() || dmctx->GetDocumentContext()->IsMediaAvailable()))
				{
					OpString object_address;
					if(dmctx->GetDocumentContext()->HasImage())
						dmctx->GetDocumentContext()->GetImageAddress(&object_address);
					else
					{
						if(OpStatus::IsError(dmctx->GetDocumentContext()->GetMediaAddress(&object_address)))
						{
							return TRUE;
						}
					}

					const uni_char *p = object_address.CStr();
					if (p)
					{
						// Clipboard
						g_desktop_clipboard_manager->PlaceText(p, GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
						// Mouse selection
						g_desktop_clipboard_manager->PlaceText(p, GetClipboardToken(), true);
#endif
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_COPY_BACKGROUND_IMAGE_ADDRESS:
		{
			if (wc && action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext() && dmctx->GetDocumentContext()->HasBGImage())
				{
					OpString image_address;
					dmctx->GetDocumentContext()->GetBGImageAddress(&image_address);
					const uni_char *p = image_address.CStr();
					if (p)
					{
						// Clipboard
						g_desktop_clipboard_manager->PlaceText(p, GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
						// Mouse selection
						g_desktop_clipboard_manager->PlaceText(p, GetClipboardToken(), true);
#endif
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_COPY_IMAGE:
		case OpInputAction::ACTION_COPY_BACKGROUND_IMAGE:
		{
			if (action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext())
				{
					if (action->GetAction() == OpInputAction::ACTION_COPY_BACKGROUND_IMAGE)
						wc->CopyBGImageToClipboard(*dmctx->GetDocumentContext());
					else
						wc->CopyImageToClipboard(*dmctx->GetDocumentContext());
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_IMAGE_PROPERTIES:
		case OpInputAction::ACTION_SHOW_BACKGROUND_IMAGE_PROPERTIES:
		{
			if (action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext())
				{
					BOOL is_background = action->GetAction() == OpInputAction::ACTION_SHOW_BACKGROUND_IMAGE_PROPERTIES;
					WindowCommanderProxy::ShowImageProperties(wc, dmctx->GetDocumentContext(), is_background);
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_USE_IMAGE_AS_FOREGROUND_SKIN:
		case OpInputAction::ACTION_USE_IMAGE_AS_BACKGROUND_SKIN:
		case OpInputAction::ACTION_USE_BACKGROUND_IMAGE_AS_FOREGROUND_SKIN:
		case OpInputAction::ACTION_USE_BACKGROUND_IMAGE_AS_BACKGROUND_SKIN:
			return TRUE;

		case OpInputAction::ACTION_USE_IMAGE_AS_SPEEDDIAL_BACKGROUND:
		{
			OpString dest_filename;

			if (wc && action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext())
				{
					OpFileFolder picture_folder = g_desktop_op_system_info->GetDefaultSpeeddialPictureFolder();

					if(OpStatus::IsSuccess(WindowCommanderProxy::SavePictureToFolder(wc, dmctx->GetDocumentContext(), dest_filename, picture_folder)))
					{
						// update the speed dials with the new image
						if(OpStatus::IsSuccess(g_speeddial_manager->LoadBackgroundImage(dest_filename)))
						{
							g_speeddial_manager->SetBackgroundImageEnabled(true);

							g_speeddial_manager->Save();
						}
					}
					return TRUE;
				}
			}
		}
	}
	return FALSE;
}

BOOL DocumentDesktopWindow::HandleFrameActions(OpInputAction* action, OpWindowCommander* wc)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_ADD_FRAME_TO_BOOKMARKS:
					child_action->SetEnabled( g_desktop_bookmark_manager->GetBookmarkModel()->Loaded() && 
											  WindowCommanderProxy::HasActiveFrameDocument(wc) );
					return TRUE;
				case OpInputAction::ACTION_COPY_FRAME_ADDRESS:
				case OpInputAction::ACTION_VIEW_FRAME_SOURCE:
#ifdef M2_SUPPORT
				case OpInputAction::ACTION_SEND_FRAME_ADDRESS_IN_MAIL:
#endif
				case OpInputAction::ACTION_MAXIMIZE_FRAME:
				case OpInputAction::ACTION_OPEN_FRAME_IN_NEW_PAGE:
				case OpInputAction::ACTION_OPEN_FRAME_IN_BACKGROUND_PAGE:
					child_action->SetEnabled( WindowCommanderProxy::HasActiveFrameDocument(wc) );
					return TRUE;
			}

		}
		return FALSE;

		case OpInputAction::ACTION_COPY_FRAME_ADDRESS:
		{
			URL url = WindowCommanderProxy::GetActiveFrameDocumentURL(wc);

			if(!url.IsEmpty())
			{
				OpString url_string;
				url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, url_string);

				// Clipboard
				g_desktop_clipboard_manager->PlaceText(url_string.CStr(), GetClipboardToken());
#if defined(_X11_SELECTION_POLICY_)
				g_desktop_clipboard_manager->PlaceText(url_string.CStr(), GetClipboardToken(), true);
#endif
			}
		}
		return TRUE;

		case OpInputAction::ACTION_ADD_FRAME_TO_BOOKMARKS:
		{
			URL url = WindowCommanderProxy::GetActiveFrameDocumentURL(wc);

			if(!url.IsEmpty())
			{
				BookmarkItemData item_data;

				item_data.name.Set( wc->GetCurrentTitle() );
				url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, item_data.url);

				if( item_data.name.IsEmpty() )
				{
					item_data.name.Set( item_data.url );
				}
				else
				{
					// Bug #177155 (remove quotes)
					ReplaceEscapes( item_data.name.CStr(), item_data.name.Length(), TRUE );
				}

				item_data.name.Strip();

				int id = -1;
				if( g_application->GetActiveBrowserDesktopWindow() )
				{
					id = g_application->GetActiveBrowserDesktopWindow()->GetSelectedHotlistItemId(PANEL_TYPE_BOOKMARKS);
				}

				g_desktop_bookmark_manager->NewBookmark(item_data, id, this, TRUE);
			}
		}
		return TRUE;

		case OpInputAction::ACTION_MAXIMIZE_FRAME:
		case OpInputAction::ACTION_OPEN_FRAME_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_FRAME_IN_BACKGROUND_PAGE:
		{
			URL url = WindowCommanderProxy::GetActiveFrameDocumentURL(wc);

			if(!url.IsEmpty())
			{
				BOOL3 new_window     = NO;
				// Never open in a new page when just maximizing it. It does not feel right [espen 2003-06-04]
				BOOL3 new_page       = action->GetAction() == OpInputAction::ACTION_MAXIMIZE_FRAME ? NO/*MAYBE*/ : YES;
				BOOL3 in_background  = action->GetAction() == OpInputAction::ACTION_OPEN_FRAME_IN_BACKGROUND_PAGE ? YES : MAYBE;
				OpString url_string;
				url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, url_string);
				g_application->OpenURL(url_string.CStr(), new_window, new_page, in_background, 0, action->IsKeyboardInvoked());
			}
		}
		return TRUE;
	}

	return FALSE;
}

void DocumentDesktopWindow::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	DesktopWindow::OnMouseEvent(widget, pos, x, y, button, down, nclicks);

	DocumentView* doc_view = GetActiveDocumentView();
	if (doc_view)
		doc_view->OnMouseEvent(widget, pos, x, y, button, down, nclicks);
}

void DocumentDesktopWindow::OnPageUrlChanged(OpWindowCommander* commander, const uni_char* url)
{
	OP_ASSERT(m_document_view);
	m_has_OnPageLoadingFinished_been_called = false;

#ifdef EXTENSION_SUPPORT
	CheckExtensionDocument();
#endif // EXTENSION_SUPPORT

	/**
	 * ReleaseDocumentViews Deletes/releases document views.
	 * Under this special circumstance it is important to check if such view exits
	 * and if so then set that view active, and if not then recreate and set active.
	 */
	DocumentView::DocumentViewTypes type = DocumentView::GetType(url);
	if (GetDocumentViewFromType(type))
		OpStatus::Ignore(SetActiveDocumentView(type));
	else
	{
		/*Recreating view and setting it active*/
		OpStatus::Ignore(CreateDocumentView(type));
		OpStatus::Ignore(SetActiveDocumentView(type));
	}

	m_intranet_bar->OnUrlChanged();
	m_plugin_crashed_bar->Hide();

#ifdef PLUGIN_AUTO_INSTALL
	OpStatus::Ignore(DeletePluginMissingBars());
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_REMOVE_WINDOW, NULL, this));
#endif // PLUGIN_AUTO_INSTALL

	UpdateCollapsedAddressBar();

	/**
	 * Release document-views if they are far behind in navigation
	 * history w.r.t current history position
	 */
	ReleaseDocumentView();

	BroadcastUrlChanged(url);

#ifdef WEB_TURBO_MODE
	m_turboTransferredBytes = 0;
	m_turboOriginalTransferredBytes = 0;
	m_prev_turboTransferredBytes = 0;
	m_prev_turboOriginalTransferredBytes = 0;
#endif

	GetWindowCommander()->GetUserConsent(ASK_FOR_PERMISSION_ON_PAGE_LOAD);

	g_input_manager->UpdateAllInputStates();
}

/***********************************************************************************
**
**	BroadcastUrlChanged
**
***********************************************************************************/
void DocumentDesktopWindow::BroadcastUrlChanged(const OpStringC& url)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnUrlChanged(this, url.CStr());
	}
}


/***********************************************************************************
**
**	BroadcastDocumentChanging
**
***********************************************************************************/
void DocumentDesktopWindow::BroadcastDocumentChanging()
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnDocumentChanging(this);
	}
}

/***********************************************************************************
**
**	BroadcastTransparentAreaChanged
**
***********************************************************************************/
void DocumentDesktopWindow::BroadcastTransparentAreaChanged(int height)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnTransparentAreaChanged(this, height);
	}
}

#ifdef PLUGIN_AUTO_INSTALL
void DocumentDesktopWindow::OnPageNotifyPluginMissing(OpWindowCommander* commander, const OpStringC& a_mime_type)
{
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_PLUGIN_MISSING, a_mime_type, this));
}

void DocumentDesktopWindow::OnPagePluginDetected(OpWindowCommander*, const OpStringC& mime_type)
{
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_PLUGIN_DETECTED, mime_type, this));
	for (UINT32 i = 0; i < m_plugin_missing_bars.GetCount(); ++i)
	{
		OpPluginMissingBar* bar = m_plugin_missing_bars.Get(i);
		if (bar->GetMimeType() == mime_type)
		{
			RemoveDocumentWindowListener(bar);
			OpStatus::Ignore(g_plugin_install_manager->RemoveListener(bar));
			OpStatus::Ignore(bar->Hide());
			break;
		}
	}
}

void DocumentDesktopWindow::OnPageRequestPluginInfo(OpWindowCommander* commander, const OpStringC& mime_type, OpString& plugin_name, BOOL& available)
{
	OpStatus::Ignore(g_plugin_install_manager->GetPluginInfo(mime_type, plugin_name, available));
}

void DocumentDesktopWindow::OnPageRequestPluginInstallDialog(OpWindowCommander* commander, const PluginInstallInformation& information)
{
	OpString tmp_mime_type;
	OpStatus::Ignore(information.GetMimeType(tmp_mime_type));
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_PLUGIN_PLACEHOLDER_CLICKED, tmp_mime_type, NULL, NULL, &information));
}

void DocumentDesktopWindow::OnPluginAvailable(const OpStringC& mime_type)
{
	AddPluginMissingBar(mime_type);
}

#endif

void DocumentDesktopWindow::UpdateCollapsedAddressBar()
{

	URL urlobj;
	if (IsLoading())
		urlobj = WindowCommanderProxy::GetMovedToURL(GetWindowCommander());
	else
		urlobj = WindowCommanderProxy::GetCurrentURL(GetWindowCommander());

	ServerName* server_name = urlobj.GetServerName();
	OpString button_text;

	// Not all pages have a server name. In case we don't have one, display the full URL instead.
	if (server_name)
	{
		button_text.Set(server_name->UniName());
	}
	else
	{
		OpString8 protocol;
		urlobj.GetAttribute(URL::KProtocolName, protocol, TRUE);
		button_text.Set(protocol);
		button_text.Append(UNI_L(":"));
#ifdef URL_CAP_SEPARATE_QUERY_COMPONENT
		OpString path;
		urlobj.GetAttribute(URL::KUniPathAndQuery, path, TRUE);
		button_text.Append(path);
#else
		button_text.Append(urlobj.GetAttribute(URL::KPath, TRUE));
#endif
	}

	BOOL security_visibility = FALSE;

	OpDocumentListener::SecurityMode mode = GetWindowCommander()->GetSecurityMode();
	security_visibility =
		((mode == OpDocumentListener::HIGH_SECURITY) ||
		 (mode == OpDocumentListener::EXTENDED_SECURITY) ||
		 (mode == OpDocumentListener::SOME_EXTENDED_SECURITY)) &&
		!WindowCommanderProxy::IsPhishing(GetWindowCommander());

#ifdef _SECURE_INFO_SUPPORT
	if (security_visibility)
	{
		if (m_document_view)
		{
			OpString cached_button_text;
			m_document_view->GetCachedSecurityButtonText(cached_button_text);
			button_text.Append(" - ");
			button_text.Append(cached_button_text);
		}
		if(mode == OpDocumentListener::HIGH_SECURITY)
		{
			m_show_addressbar_button->GetBorderSkin()->SetImage("Secure Popup Header Skin");
		}
		else
		{
			m_show_addressbar_button->GetBorderSkin()->SetImage("High Assurance Security Button Skin");
		}
		m_show_addressbar_button->GetBorderSkin()->SetForeground(TRUE);
	}
	else
#endif // _SECURE_INFO_SUPPORT
	{
		m_show_addressbar_button->GetBorderSkin()->SetImage("Insecure Popup Header Skin");
		m_show_addressbar_button->GetBorderSkin()->SetForeground(TRUE);
	}

	m_show_addressbar_button->SetText(button_text.CStr());
}

void DocumentDesktopWindow::OnPageJSPopup(OpWindowCommander* commander,	const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location, BOOL refuse, BOOL unrequested, OpDocumentListener::JSPopupCallback *callback)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnPopup(this, url, name, left, top, width, height, scrollbars, location);
	}
}

#ifdef SUPPORT_VISUAL_ADBLOCK

void DocumentDesktopWindow::OnPageContentBlocked(OpWindowCommander* commander, const uni_char *image_url)
{
	ContentBlockChanged(commander, image_url, TRUE);
}

void DocumentDesktopWindow::OnPageContentUnblocked(OpWindowCommander* commander, const uni_char *image_url)
{
	ContentBlockChanged(commander, image_url, FALSE);
}

void DocumentDesktopWindow::OnPageSecurityModeChanged(OpWindowCommander* commander, OpDocumentListener::SecurityMode mode, BOOL inline_elt)
{
	// We are not supposed to get a Security Level Change on a different window
	OP_ASSERT(GetWindowCommander() == commander);

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnSecurityModeChanged(this);
	}
}

void DocumentDesktopWindow::OnPageTrustRatingChanged(OpWindowCommander* commander, TrustRating new_rating)
{
	// We are not supposed to get a Trust Level Change on a different window
	OP_ASSERT(GetWindowCommander() == commander);

	OpDocumentListener::TrustMode new_trust_mode = OpDocumentListener::TRUST_NOT_SET;
	switch(new_rating)
	{
	case Domain_Trusted:
		new_trust_mode = OpDocumentListener::TRUSTED_DOMAIN;
		break;
	case Unknown_Trust:
		new_trust_mode = OpDocumentListener::UNKNOWN_DOMAIN;
		break;
	case Phishing:
		new_trust_mode = OpDocumentListener::PHISHING_DOMAIN;
		break;
	case Malware:
		new_trust_mode = OpDocumentListener::MALWARE_DOMAIN;
		break;
	default:
		new_trust_mode = OpDocumentListener::TRUST_NOT_SET;
	}

	OpString url;
	if (OpStatus::IsSuccess(url.Set(commander->GetLoadingURL(FALSE))) && url.Compare(UNI_L("opera:site-warning")) == 0)
	{
		INT32 pos = m_doc_history->GetCurrentHistoryPos();
		if (pos > 0)
		{
			HistoryInformation result;
			if (OpStatus::IsSuccess(m_doc_history->GetHistoryInformation(DocumentHistory::Position(DocumentHistory::Position::NORMAL, pos), &result)) && url.Compare(result.url) == 0)
			{
				OpHistoryUserData *info;
				if (OpStatus::IsSuccess(commander->GetHistoryUserData(result.id, &info)) && info)
				{
					m_current_trust_mode = static_cast<TrustInfoHistory*>(info)->m_current_trust_mode;
					return;
				}
			}
		}

		// Core set Trust_Not_Set for opera:site-warning page, ignore.
		if ((m_current_trust_mode == OpDocumentListener::PHISHING_DOMAIN || m_current_trust_mode == OpDocumentListener::MALWARE_DOMAIN))
			return; // Workaround.If a url was reported as malicious you cannot reset it to innocent!
	}

	m_current_trust_mode = new_trust_mode;

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnTrustRatingChanged(this);
}

void DocumentDesktopWindow::OnPageOnDemandStateChangeNotify(OpWindowCommander* commander, BOOL has_placeholders)
{
	// No need to update if state didn't change.
	if (m_has_plugin_placeholders == has_placeholders)
		return;

	m_has_plugin_placeholders = has_placeholders;

	g_input_manager->UpdateAllInputStates();

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnOnDemandPluginStateChange(this);
	}
}

void DocumentDesktopWindow::OnUnexpectedComponentGone(OpWindowCommander* commander, OpComponentType type, const OpMessageAddress& address, const OpStringC& information)
{
	m_plugin_crashed_bar->ShowCrash(information, address);
}

void DocumentDesktopWindow::ContentBlockChanged(OpWindowCommander* commander, const uni_char* image_url, BOOL block)
{
	URL tmpurl = WindowCommanderProxy::GetMovedToURL(GetWindowCommander());
	URL url = urlManager->GetURL(image_url);
	Str::LocaleString str_id = Str::NOT_A_STRING;
	OpString url_string;
	OpString url_pattern;
	URL moved_to_url = url.GetAttribute(URL::KMovedToURL, TRUE);

	if(!moved_to_url.IsEmpty())
	{
		moved_to_url.GetAttribute(URL::KUniName, url_string, TRUE);
	}
	else
	{
		url.GetAttribute(URL::KUniName, url_string, TRUE);
	}

	INT32 state = g_op_system_info->GetShiftKeyState();

	if(state == SHIFTKEY_SHIFT)
	{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
		UINT32 cnt;

		for(cnt = 0; cnt < m_content_to_block.GetCount(); cnt++)
		{
			ContentFilterItem *item = m_content_to_block.Get(cnt);

			if(item->GetURL().Compare(url_string) == 0)
			{
				m_content_to_block.Delete(cnt);
			}
		}
#else
		INT32 cnt;

		for(cnt = 0; cnt < m_content_to_block.GetCount(); cnt++)
		{
			OpString tmp;

			tmp.Set(m_content_to_block.GetItemString(cnt));

			if(uni_strcmp(url_string.CStr(), tmp.CStr()) == 0)
			{
				m_content_to_block.Delete(cnt);
			}
		}
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
		if(block)
		{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
			ContentFilterItem *item = OP_NEW(ContentFilterItem, (url_string));
			if(item)
			{
				item->SetContentBlockedType(BLOCKED_EDITED);
				m_content_to_block.Add(item);
			}
#else
			INT32 pos = m_content_to_block.AddItem(url_string.CStr());
			SimpleTreeModelItem *item = m_content_to_block.GetItemByIndex(pos);
			if(item)
			{
				item->SetUserData((void *)BLOCKED_EDITED);
			}
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
			str_id = Str::S_BLOCKING_LABEL;
		}
		else
		{
			str_id = Str::S_UNBLOCKING_LABEL;
		}
	}
	else
	{
		BOOL found_blockable_element = FALSE;

		if(OpStatus::IsError(ContentBlockFilterCreation::CreateFilterFromURL(tmpurl, url_string.CStr(), url_pattern)))
		{
			return;
		}
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
		UINT32 cnt;
#else
		INT32 cnt;
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
		for(cnt = 0; cnt < m_content_to_block.GetCount(); )
		{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
			ContentFilterItem *item = m_content_to_block.Get(cnt);

			if(URLFilter::MatchUrlPattern(item->GetURL(), url_pattern))
			{
				m_content_to_block.Remove(cnt);
				cnt = 0;
				found_blockable_element = TRUE;
				m_content_to_unblock.Add(item);
				continue;
			}
#else
			OpString tmp;

			tmp.Set(m_content_to_block.GetItemString(cnt));

			if(URLFilter::MatchUrlPattern(tmp.CStr(), url_pattern.CStr()))
			{
				m_content_to_block.Delete(cnt);
				cnt = 0;
				found_blockable_element = TRUE;
				m_content_to_unblock.AddItem(tmp.CStr());
				continue;
			}
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
			cnt++;
		}

		if(!found_blockable_element)
		{
			BOOL load;

			// no blockable element found, it might be in the filter.ini list in which case we will mark it as unblocked

			WindowCommanderProxy::SetContentBlockEnabled(GetWindowCommander(), TRUE);
			WindowCommanderProxy::CheckURL(url_string.CStr(), load, GetWindowCommander());

			WindowCommanderProxy::SetContentBlockEnabled(GetWindowCommander(), FALSE);

			if(!load)
			{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
				ContentFilterItem *item = OP_NEW(ContentFilterItem, (url_string));
				if(item)
				{
					m_content_to_unblock.Add(item);
				}
#else
				m_content_to_unblock.AddItem(url_string.CStr());
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
			}
		}

		WindowCommanderProxy::AdBlockPattern(GetWindowCommander(), url_pattern, block, m_content_to_block);

		if(block)
		{
			str_id = Str::S_BLOCKING_LABEL;
		}
		else
		{
			str_id = Str::S_UNBLOCKING_LABEL;
		}
		url_string.Set(url_pattern.CStr());
	}

	OpString format_str;
	g_languageManager->GetString(str_id, format_str);

	format_str.Append(url_string);

	SetStatusText(format_str.CStr(), STATUS_TYPE_INFO);

}
#endif // SUPPORT_VISUAL_ADBLOCK

void DocumentDesktopWindow::OnPageStartLoading(OpWindowCommander* commander)
{
	if (commander->IsPrinting())
		return;

	m_webhandler_bar->Hide();
	m_loading_failed = FALSE;
	m_previous_url = m_current_url;
	m_current_url = WindowCommanderProxy::GetMovedToURL(GetWindowCommander());

	if (!m_mockup_icon)
		UpdateWindowImage(TRUE);

	Relayout();

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnStartLoading(this);

#ifdef WEB_TURBO_MODE
	m_turboTransferredBytes = 0;
	m_turboOriginalTransferredBytes = 0;

	m_transferredBytes = 0;
	m_transferStart = g_op_time_info->GetRuntimeMS();
#endif
}

void DocumentDesktopWindow::OnPageLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info)
{
	if (commander->IsPrinting())
		return;

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnLoadingProgress(this, info);

#ifdef WEB_TURBO_MODE
	m_turboTransferredBytes = info->turboTransferredBytes;
	m_turboOriginalTransferredBytes = info->turboOriginalTransferredBytes;
	m_transferredBytes = info->totalBytes;
#endif
}

void DocumentDesktopWindow::OnPageLoadingFinished(OpWindowCommander* commander, OpLoadingListener::LoadingFinishStatus status, BOOL stopped_by_user)
{
	if (commander->IsPrinting())
		return;

	INT32 pos = m_doc_history->GetCurrentHistoryPos();
	if (pos > 0)
	{
		HistoryInformation result;
		if (OpStatus::IsSuccess(m_doc_history->GetHistoryInformation(DocumentHistory::Position(DocumentHistory::Position::NORMAL, pos), &result)))
		{
			OpHistoryUserData *info;
			if (OpStatus::IsSuccess(commander->GetHistoryUserData(result.id, &info)))
			{
				if (!info && m_current_trust_mode == OpDocumentListener::MALWARE_DOMAIN || m_current_trust_mode == OpDocumentListener::PHISHING_DOMAIN)
					RETURN_VOID_IF_ERROR(commander->SetHistoryUserData(result.id, OP_NEW(TrustInfoHistory, (m_current_trust_mode))));
			}
		}
	}

	if (m_delayed_title_change)
	{
		m_delayed_title_change = false;
		SetTitle(m_delayed_title.CStr());
		m_delayed_title.Empty();
	}

	if (UpdateWindowImage(FALSE))
	{
		OP_DELETE(m_mockup_icon);
		m_mockup_icon = NULL;
	}

	/**
	 * It is possible that generating a DocumentView(for an example Collection
	 * Manager) may fail in which case Core generates an error page and it is
	 * important to switch active doc view to DOCUMENT_TYPE_BROWSER.
	 */
	if ((status == OpLoadingListener::LOADING_UNKNOWN || status == OpLoadingListener::LOADING_COULDNT_CONNECT)
		&& !IsBrowserViewActive())
		OpStatus::Ignore(SetActiveDocumentView(DocumentView::DOCUMENT_TYPE_BROWSER));

	// Do not update address if page is not loaded. Fix for DSK-359164.
	if (status != OpLoadingListener::LOADING_UNKNOWN)
	{
		// DSK-359388 - Updating OpAddressDropDown is a temporary fix,
		// the real problem is in CORE-26099.
		// TODO: When CORE-26099 is done then updating OpAddressDropDown
		// should be removed from here.
		OpAddressDropDown* address_bar = GetAddressDropdown();
		if (address_bar && commander)
		{
			OpString current_url;
			/* The family of WindowCommander's GetFooURL() methods use the same
			   buffer for strings they return meaning that they will overwrite
			   the string that was obtained in the previous call, fooling
			   comparisons that are done throughout this code. We need to copy
			   the string. */
			if (OpStatus::IsSuccess(current_url.Set(commander->GetCurrentURL(FALSE)))
				&& address_bar->GetActualUrl() != current_url)
				address_bar->OnTabURLChanged(this, current_url);
		}
		UpdateCollapsedAddressBar();
	}

	Relayout();

	BroadcastDesktopWindowContentChanged();

	m_loading_failed = status == OpLoadingListener::LOADING_COULDNT_CONNECT;

	if ((status == OpLoadingListener::LOADING_COULDNT_CONNECT || stopped_by_user) &&
		WindowCommanderProxy::GetURLCameFromAddressField(GetWindowCommander()) &&
		GetAddressDropdown())
	{
		OpAddressDropDown* addressBar = GetAddressDropdown();
		if(addressBar && !addressBar->IsFocused(TRUE))
			addressBar->SetFocus(FOCUS_REASON_OTHER);
	}
	else
	{
		BOOL visdev_focused = WindowCommanderProxy::IsVisualDeviceFocused(GetWindowCommander());
		BOOL visdev_child_focused = WindowCommanderProxy::IsVisualDeviceFocused(GetWindowCommander(), TRUE);

		if (!g_application->IsEmBrowser() && (visdev_focused || (visdev_child_focused && !g_input_manager->GetKeyboardInputContext()->IsInputContextAvailable(FOCUS_REASON_OTHER))))
		{
			m_document_view->FocusDocument(FOCUS_REASON_OTHER);
		}
		else if (!m_document_view->IsFocused(TRUE))
		{
			// Can only update input context if browser window is active.

			BrowserDesktopWindow* bdw = ((BrowserDesktopWindow*)GetParentDesktopWindow());
			if (!bdw || bdw->IsActive())
			{
				// Fix for bug #179371 ([Accessibility][Usability] Work area loses keyboard focus if automatic page reload happens in a background tab when Opera window is on inactive screen)
				g_input_manager->UpdateRecentKeyboardChildInputContext(m_document_view);
			}
		}
	}

	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnLoadingFinished(this, status, stopped_by_user);
	}

	if (GetParentDesktopWindow())
	{
		if (!m_has_OnPageLoadingFinished_been_called)
		{
			m_has_OnPageLoadingFinished_been_called = true;

			if (m_loading_failed || !m_is_read_document)
				SetAttention(TRUE);

			if (m_is_read_document)
				m_is_read_document = false;
		}
	}
	
#if defined(_MACINTOSH_) && defined(SUPPORT_SHARED_MENUS)
	if(!g_application->IsEmBrowser())
	{
		INT32 signature;
		OSStatus err;
		for(UINT i = 0; i < gInformApps.GetCount(); i++)
		{
			signature = gInformApps.Get(i);
			err = InformApplication(signature);
			if(err != noErr)
			{
				gInformApps.Remove(i);
			}
		}
	}
#endif // _MACINTOSH_ && SUPPORT_SHARED_MENUS

#ifdef WEB_TURBO_MODE
	if(m_turboTransferredBytes > 0 && m_turboOriginalTransferredBytes > 0)
	{
		g_opera_turbo_manager->AddCompressionRate(m_turboTransferredBytes, m_turboOriginalTransferredBytes, m_prev_turboTransferredBytes, m_prev_turboOriginalTransferredBytes);
		m_prev_turboTransferredBytes = m_turboTransferredBytes;
		m_prev_turboOriginalTransferredBytes = m_turboOriginalTransferredBytes;

#ifdef ENABLE_USAGE_REPORT
		// Usage report
		if (status == OpLoadingListener::LOADING_SUCCESS)
			g_opera_turbo_manager->AddToTotalPageViews(1);
#endif // ENABLE_USAGE_REPORT
	}
#ifdef ENABLE_USAGE_REPORT
	else
	{
		// Usage report
		if (status == OpLoadingListener::LOADING_SUCCESS)
			g_opera_turbo_manager->AddTototalBytes(m_transferredBytes, g_op_time_info->GetRuntimeMS() - m_transferStart);
	}
#endif // ENABLE_USAGE_REPORT

#endif // WEB_TURBO_MODE
}

void DocumentDesktopWindow::OnPageAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnAuthenticationRequired(commander, callback);
}

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
void DocumentDesktopWindow::OnDestroyed(OpSearchSuggestionsCallback* callback)
{ 
	m_search_suggestions_callback = 0;
	if (m_search_suggest && m_search_suggest->IsBusy())
		m_search_suggest->Abort();
}

void DocumentDesktopWindow::OnPageSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url_string, OpSearchSuggestionsCallback* callback)
{
	if (!m_search_suggest)
	{
		m_search_suggest = OP_NEW(SearchSuggest,(this));
		if (!m_search_suggest)
			return;
	}
	else
	{
		// Stop active request. The requestor wants another url examined.
		m_search_suggest->Abort();
	}

	if (OpStatus::IsError(StartSuggestionsSearch(url_string)))
		m_search_suggestions_callback = 0;
	else
	{
		// Important that we unregister to old listener first
		if (m_search_suggestions_callback)
			m_search_suggestions_callback->SetListener(0);
		m_search_suggestions_callback = callback;
		m_search_suggestions_callback->SetListener(this);
	}
}

void DocumentDesktopWindow::OnPageSearchSuggestionsUsed(OpWindowCommander* commander, BOOL suggestions)
{
	if (g_usage_report_manager && g_usage_report_manager->GetSearchReport())
	{
		// A bit complex here, but we use same logic in OpErrorPage. Ensures same result.
		OpSearchProviderListener* listener = g_windowCommanderManager->GetSearchProviderListener();
		OpSearchProviderListener::SearchProviderInfo* info = listener ? listener->OnRequestSearchProviderInfo(OpSearchProviderListener::REQUEST_ERROR_PAGE) : NULL;
		if (!info || !info->GetGUID())
			return;
		g_usage_report_manager->GetSearchReport()->AddSearch(
			suggestions ? SearchReport::SearchSuggestedFromErrorPage : SearchReport::SearchErrorPage, info->GetGUID());
	}
}


OP_STATUS DocumentDesktopWindow::StartSuggestionsSearch(const uni_char* url_string)
{
	// A bit complex here, but we use same logic in OpErrorPage. Ensures same result.
	OpSearchProviderListener* listener = g_windowCommanderManager->GetSearchProviderListener();
	OpSearchProviderListener::SearchProviderInfo* info = listener ? listener->OnRequestSearchProviderInfo(OpSearchProviderListener::REQUEST_ERROR_PAGE) : NULL;
	if( !info || !info->ProvidesSuggestions())
		return OpStatus::ERR;

	if (!m_search_suggest || !url_string)
		return OpStatus::ERR;

	if (m_search_suggest->IsBusy())
	{
		if (!m_mh)
		{
			m_mh = OP_NEW(MessageHandler, (NULL));
			if (!m_mh)
				return OpStatus::ERR_NO_MEMORY;
			int rc = m_mh->SetCallBack(this, MSG_QUICK_START_SEARCH_SUGGESTIONS, 0);
			if (OpStatus::IsError(rc))
			{
				OP_DELETE(m_mh);
				m_mh = 0;
				return rc;
			}
		}
		m_mh->RemoveDelayedMessage(MSG_QUICK_START_SEARCH_SUGGESTIONS, 0, 0);

		// Careful here: The argument 'url_string' can be taken directly from
		// m_next_search.CStr(). Using m_next_search.Set() on that string results
		// in 'my_string.Set(my_string.CStr())' which is not well defined.

		// This is not a string compare test, but a pointer compare test. See above.
		if (m_next_search.CStr() != url_string)
			RETURN_IF_ERROR(m_next_search.Set(url_string));

		m_mh->PostDelayedMessage(MSG_QUICK_START_SEARCH_SUGGESTIONS, 0, 0, 100);

		return OpStatus::OK;
	}

	RETURN_IF_ERROR(m_search_guid.Set(info->GetGUID()));

	if (m_search_suggest->HasSuggest(m_search_guid.CStr()))
	{
		OpString keyword, s;

		URL url = g_url_api->GetURL(url_string);
		RETURN_IF_ERROR(s.Set(url.GetAttribute(URL::KUniHostName)));

		if (s.IsEmpty())
		{
			// Fallback
			OpStringC u(url_string);
			int l = u.Length();

			int p = u.Find("opera:");
			if (p == 0 && l > 6)
			{
				// Perhaps we can provide something useful opera related
				RETURN_IF_ERROR(s.Set(u));
				for (p=0; p<l ; p++)
				{
					if (!uni_isalnum(s.CStr()[p]))
						s.CStr()[p]=' ';
				}
			}
			else
			{
				// Try to remove protocol and/or anything non-alphanum after first ':'
				p = u.Find(":");
				if (p != KNotFound)
				{
					for (; p<l && !uni_isalnum(u.CStr()[p]); p++)
						;
				}
				if (p == KNotFound || p == l)
					p = 0;
				RETURN_IF_ERROR(s.Set(&u.CStr()[p]));
			}
		}

		int p1 = 0;
		if (s.FindI("www.")==0)
			p1 += 4;
		// Add more skip tests here if needed

		int p2 = s.FindFirstOf(UNI_L("."), p1);
		if (p2 == -1)
			RETURN_IF_ERROR(keyword.Set(&s.CStr()[p1])); // whole string
		else
			RETURN_IF_ERROR(keyword.Set(&s.CStr()[p1], p2-p1));

		RETURN_IF_ERROR(m_search_suggest->Connect(keyword, m_search_guid, SearchTemplate::SEARCH_404));
	}

	m_next_search.Empty();

	return OpStatus::OK;
}

void DocumentDesktopWindow::OnQueryComplete(const OpStringC& search_id, OpVector<SearchSuggestEntry>& entries)
{
	SearchTemplate* item = g_searchEngineManager->GetByUniqueGUID(m_search_guid);
	if (item && m_search_suggestions_callback)
	{
		URL_CONTEXT_ID context = PrivacyMode() ? g_windowManager->GetPrivacyModeContextId() : 0;

		OpAutoVector<OpString> list;
		OpAutoPtr<OpString> s1;
		OpAutoPtr<OpString> s2;

		for (UINT32 i=0; i < entries.GetCount(); i++)
		{
			SearchSuggestEntry* entry = entries.Get(i);
			if (!entry->GetData())
				continue;
			URL url;
			if (!item->MakeSearchURL(url, entry->GetData(), FALSE, SearchEngineManager::SEARCH_REQ_ISSUER_OTHERS, UNI_L(""), context))
				continue;
			const uni_char* url_string = url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
			if (!url_string)
				continue;
			const uni_char* name_string = entry->GetData();
			if (!name_string)
				continue;

			s1 = OP_NEW(OpString,());
			s2 = OP_NEW(OpString,());
			if (!s1.get() || OpStatus::IsError(s1->Set(url_string)) || !s2.get() || OpStatus::IsError(s2->Set(name_string)))
				break;

			if (OpStatus::IsError(list.Add(s1.get())))
				break;
			s1.release();

			if (OpStatus::IsError(list.Add(s2.get())))
			{
				list.Delete(list.GetCount()-1);
				break;
			}
			s2.release();
		}

		if (list.GetCount())
			m_search_suggestions_callback->OnDataReceived(list);
	}
}
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT

/***********************************************************************************
**
**	OnTitleChanged
**
***********************************************************************************/

void DocumentDesktopWindow::OnPageTitleChanged(OpWindowCommander* commander, const uni_char* title)
{
	if (m_delayed_title_change)
		m_delayed_title.Set(title);
	else
		SetTitle(title);
}

/***********************************************************************************
**
**	OnDocumentIconAdded
**
***********************************************************************************/

void DocumentDesktopWindow::OnPageDocumentIconAdded(OpWindowCommander* commander)
{
	UpdateWindowImage(TRUE);
}

/***********************************************************************************
**
**	OnHover
**
***********************************************************************************/

void DocumentDesktopWindow::OnPageHover(OpWindowCommander* commander, const uni_char* url, const uni_char* link_title, BOOL is_image)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnHover(this, url, link_title, is_image);
	}
}

/***********************************************************************************
**
**	OnNoHover
**
***********************************************************************************/

void DocumentDesktopWindow::OnPageNoHover(OpWindowCommander* commander)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnNoHover(this);
	}
}

/***********************************************************************************
**
**	OnScaleChanged
**
***********************************************************************************/

void DocumentDesktopWindow::OnPageScaleChanged(OpWindowCommander* commander, UINT32 newScale)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
	{
		m_listeners.GetNext(iterator)->OnScaleChanged(this, newScale);
	}
}

/***********************************************************************************
**
**	Position / Size listeners
**
***********************************************************************************/

void DocumentDesktopWindow::OnPageInnerSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height)
{
/*	Core doesn't seem to use OnInnerSizeRequest for resize requests from scripts
	We therefore assume the request comes from the ResizeCorner in visual device. huibk 20.11.2006
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AllowScriptToResizeWindow))
		return; */

	if (g_drag_manager->IsDragging())
		return;

	// Test if it is possible to resize
	if (IsMaximized() || IsFullscreen())
	{
		DesktopWindow* pdw = GetParentDesktopWindow();
		if (pdw && pdw->IsMaximized() || pdw->IsFullscreen())
			return;
	}

	UINT32 outer_width, outer_height, inner_width, inner_height;

	//If the current window is maximized, the parent window (browserdesktopwindow) should actually be resized
	DesktopWindow* resizing_window = GetParentDesktopWindow() && IsMaximized() ? GetParentDesktopWindow() : this;

	//get the size difference between the resizing window and the core window
	resizing_window->GetOuterSize(outer_width, outer_height);
	m_document_view->GetOpWindow()->GetInnerSize(&inner_width, &inner_height);

	//add the difference to the new size of the core window
	if ((int)width < 0)	//documentlistener uses UINT32s, but the core Window class actually sends signed values (int). Ugly!
		width = 0;
	if ((int)height < 0)
		height = 0;
	width += outer_width - inner_width;
	height += outer_height - inner_height;

/* 	Not needed here either, users can resize whereto they want:
	EnsureAllowedOuterSize(width, height); */
	resizing_window->SetOuterSize(width, height);

	// Force syncing of all layouts (Widgets, Visual device). The resizeCorner relies on it
	resizing_window->SyncLayout();
	resizing_window->GetWidgetContainer()->GetRoot()->Sync();

	if (resizing_window != this)
	{
		SyncLayout();
		GetWidgetContainer()->GetRoot()->Sync();
	}
}

void DocumentDesktopWindow::OnPageGetInnerSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
		m_document_view->GetOpWindow()->GetInnerSize(width, height);
}

void DocumentDesktopWindow::OnPageOuterSizeRequest(OpWindowCommander* commander, UINT32 width, UINT32 height)
{
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AllowScriptToResizeWindow))
		return;

	if (g_drag_manager->IsDragging())
		return;

	EnsureAllowedOuterSize(width, height);
	SetOuterSize(width, height);

	SyncLayout();
	GetWidgetContainer()->GetRoot()->Sync();
}

void DocumentDesktopWindow::OnPageGetOuterSize(OpWindowCommander* commander, UINT32* width, UINT32* height)
{
	GetOuterSize(*width, *height);
}


void DocumentDesktopWindow::OnPageMoveRequest(OpWindowCommander* commander, INT32 x, INT32 y)
{
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AllowScriptToMoveWindow))
		return;

	if (g_drag_manager->IsDragging())
		return;

	// Make sure we don't move the window out of bounds.
	{

		OpRect bounds_rect;

#ifndef _MACINTOSH_ // Macintosh doesn't have MDI, so always use the screen rect there.
		if (GetWorkspace() != 0)
			bounds_rect = GetWorkspace()->GetBounds();
		else
#endif
			GetScreenRect(bounds_rect, TRUE);

		UINT32 outer_width = 0;
		UINT32 outer_height = 0;
		GetOuterSize(outer_width, outer_height);

		INT32 max_x = bounds_rect.width - outer_width;
		INT32 max_y = bounds_rect.height - outer_height;

		max_x = MAX(max_x, 0);
		max_y = MAX(max_y, 0);

		x = MAX(x, bounds_rect.x);
		y = MAX(y, bounds_rect.y);
		x = MIN(x, max_x);
		y = MIN(y, max_y);
	}

	SetOuterPos(x, y);
}

void DocumentDesktopWindow::OnPageGetPosition(OpWindowCommander* commander, INT32* x, INT32* y)
{
	GetOuterPos(*x, *y);
}

/*
	MacHack: Print Freezing Hack

	This stops Opera freezing when a javascript call calls, to print a window then close it straight after.

	This hack should really be removed and replaced with something much cleaner.
*/

#ifdef _MACINTOSH_
class DelayedClose : public OpDelayedAction
{
public:
	DelayedClose(DocumentDesktopWindow *win) : OpDelayedAction(100), m_win(win) {}
	void DoAction() {m_win->OnClose(FALSE); }
private:
	DocumentDesktopWindow* m_win;
};
#endif // _MACINTOSH_

void DocumentDesktopWindow::OnPageClose(OpWindowCommander* commander)
{
#ifdef _MACINTOSH_
	if (WindowCommanderProxy::GetPrintMode(GetWindowCommander()))
	{
		OP_NEW(DelayedClose, (this));
		return;
	}
#endif // _MACINTOSH_
	if (WindowCommanderProxy::IsContentBlockerEnabled(GetWindowCommander()))
	{
		WindowCommanderProxy::SetContentBlockEditMode(GetWindowCommander(), FALSE);
	}
	g_input_manager->InvokeAction(OpInputAction::ACTION_CLOSE_PAGE, 1, NULL, this);
#ifdef PLUGIN_AUTO_INSTALL
	OpStatus::Ignore(DeletePluginMissingBars());
	OpStatus::Ignore(g_plugin_install_manager->Notify(PIM_REMOVE_WINDOW, NULL, this));
#endif // PLUGIN_AUTO_INSTALL
}

void DocumentDesktopWindow::OnPageRaiseRequest(OpWindowCommander* commander)
{
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AllowScriptToRaiseWindow))
		return;

	if (g_drag_manager->IsDragging())
		return;

	Activate();
}

void DocumentDesktopWindow::OnPageLowerRequest(OpWindowCommander* commander)
{
	if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AllowScriptToLowerWindow))
		return;

	if (g_drag_manager->IsDragging())
		return;

	Lower();
}

BOOL DocumentDesktopWindow::IsClosableByUser()
{
	if (!DesktopWindow::IsClosableByUser())
		return FALSE;

#ifndef _MACINTOSH_
	if (g_pcui->GetIntegerPref(PrefsCollectionUI::AllowEmptyWorkspace))
		return TRUE;

	OpWorkspace* workspace = GetParentWorkspace();
	if (workspace && workspace->GetItemCount() == 1 && m_doc_history)
	{
		BOOL has_doc = HasDocument();
		if (m_doc_history->GetHistoryLength() > 0 && m_doc_history->GetCurrentHistoryPos() == 1)
		{
			HistoryInformation result;
			if (OpStatus::IsError(m_doc_history->GetHistoryInformation(DocumentHistory::Position(DocumentHistory::Position::NORMAL, 1), &result)))
				return TRUE;

			if (uni_strcmp(result.url, "opera:speeddial") == 0)
				has_doc = false;
		}

		if (!has_doc || IsPrivateIntroPageShown())
			return FALSE;
	}
#endif

	return TRUE;
}


/***********************************************************************************
**
**	SetHidden
**
***********************************************************************************/
void DocumentDesktopWindow::SetHidden(BOOL hidden)
{
	if (m_document_view)
		m_document_view->SetHidden(hidden);
}


/***********************************************************************************
**
**	GetAddressDropdown
**
***********************************************************************************/

OpAddressDropDown* DocumentDesktopWindow::GetAddressDropdown()
{
	return (OpAddressDropDown*) GetWidgetByType(WIDGET_TYPE_ADDRESS_DROPDOWN);
}

/***********************************************************************************
**
**	GetSearchDropdown
**
***********************************************************************************/

OpSearchDropDown* DocumentDesktopWindow::GetSearchDropdown()
{
	return (OpSearchDropDown*) GetWidgetByType(WIDGET_TYPE_SEARCH_DROPDOWN);
}


/***********************************************************************************
**
**	GetToolTipText
**
***********************************************************************************/
void DocumentDesktopWindow::GetToolTipThumbnailText(OpToolTip* tooltip, OpString& title, OpString& url_string, URL& url)
{
	DocumentView* doc_view = GetActiveDocumentView();
	if (doc_view)
		doc_view->GetTitle(title);
}

void DocumentDesktopWindow::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	DocumentView* doc_view = GetActiveDocumentView();
	if (!doc_view)
		return;

	RETURN_VOID_IF_ERROR(doc_view->GetTooltipText(text));

	if(tooltip && g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInTabTooltips))
	{
		Image image;
		doc_view->GetThumbnailImage(image);
		if(!image.IsEmpty())
			tooltip->SetImage(image, doc_view->HasFixedThumbnailImage() ? TRUE : FALSE);
	}
}

BOOL DocumentDesktopWindow::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	return FALSE;
}

OP_STATUS DocumentDesktopWindow::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == INFO_QUERY)
	{
		GetToolTipText(NULL, *item_data->info_query_data.info_text);
		return OpStatus::OK;
	}
	else if (item_data->query_type == URL_QUERY)
	{
		URL url = WindowCommanderProxy::GetMovedToURL(GetWindowCommander());
		if (!url.IsEmpty())	// only handle it if there is an url
		{
			const uni_char* url_text = GetWindowCommander()->GetCurrentURL(FALSE);
			RETURN_IF_ERROR(item_data->url_query_data.thumbnail_text->url.Set(url_text));
			RETURN_IF_ERROR(item_data->url_query_data.thumbnail_text->title.Set(GetWindowCommander()->GetCurrentTitle()));

			OpPoint logoStart;
			item_data->url_query_data.thumbnail_text->thumbnail = GetThumbnailImage(logoStart);
			item_data->url_query_data.thumbnail_text->fixed_image = HasFixedThumbnailImage();

			return OpStatus::OK;
		}
	}
	return DesktopWindow::GetItemData(item_data);
}

UINT32 DocumentDesktopWindow::GetClipboardToken()
{
	return PrivacyMode() ? windowManager->GetPrivacyModeContextId() : 0; 
}

/***********************************************************************************
**
**	DocumentDesktopWindowSpy
**
***********************************************************************************/

DocumentDesktopWindowSpy::DocumentDesktopWindowSpy() :
	m_input_state_listener(this),
	m_document_desktop_window(NULL),
	m_input_context(NULL)
{
}

DocumentDesktopWindowSpy::~DocumentDesktopWindowSpy()
{
	SetSpyInputContext(NULL, FALSE);
}

/***********************************************************************************
 **
 **	SetSpyInputContext
 **
 ***********************************************************************************/

void DocumentDesktopWindowSpy::SetSpyInputContext(OpInputContext* input_context, BOOL send_change)
{
	if (input_context == m_input_context)
		return;

	m_input_context = input_context;

	if (m_input_context)
	{
		m_input_state_listener.Enable(TRUE);
		if (send_change)
			UpdateTargetDocumentDesktopWindow();
	}
	else
	{
		m_input_state_listener.Enable(FALSE);
		SetTargetDocumentDesktopWindow(NULL, send_change);
	}
}

/***********************************************************************************
**
**	SetTargetDocumentDesktopWindow
**
***********************************************************************************/

void DocumentDesktopWindowSpy::SetTargetDocumentDesktopWindow(DocumentDesktopWindow* document_desktop_window, BOOL send_change)
{
	if (document_desktop_window == m_document_desktop_window)
		return;

	if (m_document_desktop_window)
	{
		m_document_desktop_window->RemoveListener(this);
		m_document_desktop_window->RemoveDocumentWindowListener(this);
	}

	m_document_desktop_window = document_desktop_window;

	if (m_document_desktop_window)
	{
		if (OpStatus::IsError(m_document_desktop_window->AddListener(this)))
		{
			m_document_desktop_window = NULL;
			return;
		}
		m_document_desktop_window->AddDocumentWindowListener(this);
	}

	if (send_change)
		OnTargetDocumentDesktopWindowChanged(m_document_desktop_window);
}

void DocumentDesktopWindowSpy::UpdateTargetDocumentDesktopWindow()
{
	OP_ASSERT(m_input_context);

	if(!m_input_context)
	{
		return;
	}

	DocumentDesktopWindow* document_desktop_window = (DocumentDesktopWindow*) g_input_manager->GetTypedObject(OpTypedObject::WINDOW_TYPE_DOCUMENT, m_input_context);

	if (!document_desktop_window && g_application)
	{
		document_desktop_window = g_application->GetActiveDocumentDesktopWindow();
	}

	SetTargetDocumentDesktopWindow(document_desktop_window);
}

void DocumentDesktopWindowSpy::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	UpdateTargetDocumentDesktopWindow();

	if (desktop_window == m_document_desktop_window)
	{
		if (m_document_desktop_window->GetCycleAccessKeysPopupWindow()) m_document_desktop_window->GetCycleAccessKeysPopupWindow()->Close(TRUE);
		m_document_desktop_window->RemoveListener(this);
		m_document_desktop_window->RemoveDocumentWindowListener(this);
		m_document_desktop_window = NULL;
	}

}

const char*	DocumentDesktopWindow::GetInputContextName()
{
	return "Document Window";
}

/***********************************************************************************
**
**	DisplayDefaultText
**
***********************************************************************************/

#ifdef USE_ABOUT_FRAMEWORK
#include "modules/about/opgenerateddocument.h"
class OpDefaultBannerHTMLView : public OpGeneratedDocument
{
public:
	OpDefaultBannerHTMLView(URL &url)
		: OpGeneratedDocument(url, HTML5)
	{}

	OP_STATUS GenerateData()
	{
		OpenDocument();
		m_url.WriteDocumentData(URL::KNormal, UNI_L("<style type=\"text/css\">body { border: 1px solid ThreeDShadow; padding-top: 3px; font: 10pt sans-serif; background: ThreeDFace; color: ButtonText; text-align: center; }\na {color: ButtonText;}</style>\n"));
		OpenBody(Str::NOT_A_STRING);
		m_url.WriteDocumentData(URL::KNormal, UNI_L("<p><a href=\"http://www.opera.com/buy/\"Opera</a></p>\n"));
		return CloseDocument();
	}
};
#endif


/***********************************************************************************
**
**	SaveActiveDocumentAsFile
**
***********************************************************************************/
OP_STATUS DocumentDesktopWindow::SaveActiveDocumentAsFile()
{
	return SaveDocument(FALSE);
}

/***********************************************************************************
**
**	SaveActiveDocumentAtLocation
**
***********************************************************************************/
OP_STATUS DocumentDesktopWindow::SaveActiveDocumentAtLocation(const uni_char* path)
{
	return GetWindowCommander()->SaveDocument(path, OpWindowCommander::SAVE_ONLY_DOCUMENT, FALSE);
}

OP_STATUS EscapeURLIfNeeded(OpString& address)
{
	BOOL escape_address = FALSE;
	OP_STATUS status = OpStatus::OK;

	if( address.Find(UNI_L("://")) != -1 )
	{
		//escape_address = address.Find(UNI_L("file://")) != -1;
	}
	else if( address.CStr() )
	{
		//check if it's a local file
		OpFile file;
		file.Construct(address.CStr());
		file.Exists(escape_address);
	}

	if( escape_address )
	{
		uni_char* escaped = OP_NEWA(uni_char, address.Length()*3+1);	//worst case
		if(escaped)
		{
#if defined(UNIX) || defined(_MACINTOSH_)
			UriEscape::Escape(escaped, address.CStr(), UriEscape::Filename | UriEscape::AmpersandQmark | UriEscape::ColonSemicolon);
#else
			UriEscape::Escape(escaped, address.CStr(), UriEscape::Filename);
#endif
			status = address.Set( escaped );
			OP_DELETEA(escaped);
		}
		else
			status = OpStatus::ERR_NO_MEMORY;
	}
	return status;
}

Image DocumentDesktopWindow::GetThumbnailImage(OpPoint &logoStart)
{
	Image image;
	DocumentView* doc_view = GetActiveDocumentView();
	if (!doc_view)
		return image;

	doc_view->GetThumbnailImage(image);
	return image;
}

Image DocumentDesktopWindow::GetThumbnailImage(UINT32 width, UINT32 height, BOOL high_quality, BOOL use_skin_image_if_present)
{
	Image thumbnail;
	DocumentView* doc_view = GetActiveDocumentView();
	if (!doc_view)
		return thumbnail;

	OpPoint logoStart;
	thumbnail = GetThumbnailImage(logoStart);
	if (doc_view->HasFixedThumbnailImage() || thumbnail.Width() == width && thumbnail.Height() == height)
		return thumbnail;

	/*
	TODO: We are required to return a "correctly sized" image here, so scale what we've got.
	*/

	return DesktopWindow::GetThumbnailImage(width, height, high_quality, use_skin_image_if_present);
}

BOOL DocumentDesktopWindow::HasFixedThumbnailImage()
{
	DocumentView* doc_view = GetActiveDocumentView();
	if (!doc_view)
		return DesktopWindow::HasFixedThumbnailImage();

	return doc_view->HasFixedThumbnailImage() ? TRUE : FALSE;
}

double DocumentDesktopWindow::GetWindowScale()
{
	DocumentView* doc_view = GetActiveDocumentView();
	if (doc_view)
		return doc_view->GetWindowScale();

	return DesktopWindow::GetWindowScale();
}

void DocumentDesktopWindow::QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values)
{
	DocumentView* doc_view = GetActiveDocumentView();
	if (doc_view)
		return doc_view->QueryZoomSettings(min, max, step, tick_values, num_tick_values);
}

void DocumentDesktopWindow::OnPageAccessKeyUsed(OpWindowCommander* /* commander */)
{
	g_main_message_handler->PostDelayedMessage(MSG_QUICK_CLOSE_ACCESSKEY_WINDOW, (MH_PARAM_1) this, 0, 0 );
}

void DocumentDesktopWindow::OnClick(OpWidget *widget, UINT32 id)
{
	if(widget == m_show_addressbar_button)
	{
		m_address_bar->SetAlignment(OpBar::ALIGNMENT_OLD_VISIBLE);
		Relayout();
		return;
	}
	DesktopWindow::OnClick(widget, id);
}

void DocumentDesktopWindow::OnAskForPermission(OpWindowCommander *commander, PermissionCallback *callback)
{
	switch (callback->Type())
	{
		case PermissionCallback::PERMISSION_TYPE_WEB_HANDLER_REGISTRATION:
		{
			OpAutoPtr<DesktopPermissionCallback> desktop_callback(OP_NEW(DesktopPermissionCallback, (callback)));
			if (!desktop_callback.get())
				return;

			OpString hostname;
			RETURN_VOID_IF_ERROR(hostname.Set(callback->GetURL()));
			if (hostname.IsEmpty())
				RETURN_VOID_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_DOWNLOAD_DLG_UNKNOWN_SERVER, hostname)); // Fallback

			RETURN_VOID_IF_ERROR(desktop_callback->SetAccessingHostname(hostname));

			WebHandlerPermission* web_permission = static_cast<WebHandlerPermission*>(callback);
			RETURN_VOID_IF_ERROR(desktop_callback->AddWebHandlerRequest(web_permission->GetName()));
			RETURN_VOID_IF_ERROR(desktop_callback->AddWebHandlerRequest(web_permission->GetHandlerURL()));
			RETURN_VOID_IF_ERROR(desktop_callback->AddWebHandlerRequest(web_permission->GetDescription()));
			RETURN_VOID_IF_ERROR(desktop_callback->AddWebHandlerRequest(web_permission->IsProtocolHandler() ? UNI_L("protocol") : UNI_L("mime")));

			if (m_permission_callbacks.GetCount())
				m_permission_callbacks.AddItem(desktop_callback.get()); // we already have callbacks waiting for input, delay this one
			else
			{
				m_permission_callbacks.AddItem(desktop_callback.get());
				g_main_message_handler->PostMessage(MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) this, 0);
			}
			desktop_callback.release();
			break;
		}
		case PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST:
		case PermissionCallback::PERMISSION_TYPE_CAMERA_REQUEST:
		{
			OpString hostname;      // will be set further down
			DesktopPermissionCallback *desktop_callback = OP_NEW(DesktopPermissionCallback, (callback, hostname));
			if(!desktop_callback)
			{
				return;
			}

			URL url = urlManager->GetURL(callback->GetURL());			
			if(!url.IsEmpty())
			{
				hostname.Set(url.GetServerName()->UniName());
			}
			if(hostname.IsEmpty())
			{
				g_languageManager->GetString(Str::SI_IDSTR_DOWNLOAD_DLG_UNKNOWN_SERVER, hostname);
			}
			desktop_callback->SetAccessingHostname(hostname);
			m_permission_callbacks.AddItem(desktop_callback);
			if (m_permission_callbacks.GetCount() == 1 && g_application->GetActiveDocumentDesktopWindow() == this)
			{
				// nothing else in the queue, show the toolbar now
				g_main_message_handler->PostMessage(MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) this, 0);
			}
			break;
		}
	}
}


OP_STATUS DocumentDesktopWindow::HandleWebHandlerRequest(const OpStringC& hostname, const OpVector<OpString>& request)
{
	UINT32 count = request.GetCount();
	if (count < 4)
		return OpStatus::ERR;

	OpStringC mime_or_protocol = *request.Get(0); // Mime or protocol string
	BOOL is_protocol = !request.Get(3)->CompareI("protocol");

	// Determine if Opera is the handler or not
	OpString8 handler;
	if (is_protocol)
	{
		TrustedProtocolData data;
		int index = g_pcdoc->GetTrustedProtocolInfo(mime_or_protocol, data);
		if (index >= 0)
		{
			switch (data.viewer_mode)
			{
				case UseInternalApplication:
					RETURN_IF_ERROR(handler.Set("opera"));
				break;

				case UseWebService:
					if (data.web_handler.HasContent())
					{
						URL url = g_url_api->GetURL(data.web_handler);
						RETURN_IF_ERROR(handler.Set(url.GetAttribute(URL::KHostName)));
					}
				break;

				case UseCustomApplication:
					if (data.filename.HasContent())
					{
						OpFile file;
						RETURN_IF_ERROR(file.Construct(data.filename));
						RETURN_IF_ERROR(handler.SetUTF8FromUTF16(file.GetName()));
					}
				break;
			}
		}
	}
	else
	{
		Viewer* viewer = g_viewers->FindViewerByMimeType(mime_or_protocol);
		if (viewer)
		{
			switch (viewer->GetAction())
			{
				case VIEWER_OPERA:
					RETURN_IF_ERROR(handler.Set("opera"));
				break;

				case VIEWER_WEB_APPLICATION:
				case VIEWER_APPLICATION:
				case VIEWER_REG_APPLICATION:
				case VIEWER_RUN:
				case VIEWER_PASS_URL:
				{
					OpStringC application = viewer->GetApplicationToOpenWith();
					if (application.HasContent())
					{
						if (viewer->GetAction() == VIEWER_WEB_APPLICATION)
						{
							URL url = g_url_api->GetURL(application);
							RETURN_IF_ERROR(handler.Set(url.GetAttribute(URL::KHostName)));
						}
						else
						{
							OpFile file;
							RETURN_IF_ERROR(file.Construct(application));
							RETURN_IF_ERROR(handler.SetUTF8FromUTF16(file.GetName()));
						}
					}
				}
				break;

				case VIEWER_PLUGIN:
				{
					PluginViewer* plugin_viewer = viewer->GetPluginViewer(0);
					if (plugin_viewer)
					{
						OpFile file;
						RETURN_IF_ERROR(file.Construct(plugin_viewer->GetPath()));
						RETURN_IF_ERROR(handler.SetUTF8FromUTF16(file.GetName()));
					}
				}
				break;
			}
		}
	}

	// Determine the text to display

	BOOL feed_reader = FALSE;
	Str::LocaleString friendly_id(Str::NOT_A_STRING);
	Str::LocaleString article_friendly_id(Str::NOT_A_STRING);

	if (!mime_or_protocol.CompareI("mailto"))
	{
		friendly_id = Str::D_EMAIL_CLIENT_STR;
		article_friendly_id = Str::D_EMAIL_CLIENT_STR_WITH_AN_ARTICLE;
	}
	else if (!mime_or_protocol.CompareI("nntp") || !mime_or_protocol.CompareI("news"))
	{
		friendly_id = Str::D_NEWS_CLIENT_STR;
		article_friendly_id = Str::D_NEWS_CLIENT_STR_WITH_AN_ARTICLE;
	}
	else if (!mime_or_protocol.CompareI("irc") || !mime_or_protocol.CompareI("ircs"))
	{
		friendly_id = Str::D_IRC_CLIENT_STR;
		article_friendly_id = Str::D_IRC_CLIENT_STR_WITH_AN_ARTICLE;
	}
	else if (!mime_or_protocol.CompareI("application/rss+xml") || !mime_or_protocol.CompareI("application/atom+xml"))
	{
		feed_reader = TRUE;
	}

	OpString message;
	Str::LocaleString question_id(Str::NOT_A_STRING);
	BOOL has_friendly_name = friendly_id != Str::NOT_A_STRING;

	if (feed_reader)
	{
		RETURN_IF_ERROR(I18n::Format(message, Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_FEED_READER, hostname));
	}
	else if (handler.HasContent())
	{
		if (!handler.CompareI("opera"))
		{
			if (has_friendly_name)
			{
				OpString argument;
				RETURN_IF_ERROR(g_languageManager->GetString(friendly_id, argument));
				RETURN_IF_ERROR(I18n::Format(message, Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_FRIENDLY_NAME_KNOWN_HANDLER_OPERA, argument));
				question_id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_FRIENDLY_NAME_KNOWN_HANDLER_QUESTION;
			}
			else
			{
				Str::LocaleString id(Str::NOT_A_STRING);
				if (is_protocol)
				{
					id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_NO_FRIENDLY_NAME_KNOWN_PROTOCOL_HANDLER_OPERA;
					question_id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_KNOWN_PROTOCOL_HANDLER_QUESTION;
				}
				else
				{
					id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_NO_FRIENDLY_NAME_KNOWN_CONTENT_HANDLER_OPERA;
					question_id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_KNOWN_CONTENT_HANDLER_QUESTION;
				}

				OpString format;
				RETURN_IF_ERROR(g_languageManager->GetString(id, format));
				if (!is_protocol)
					RETURN_IF_ERROR(format.Append(UNI_L("\n%1")));
				RETURN_IF_ERROR(I18n::Format(message, format.CStr(), mime_or_protocol));
			}
		}
		else
		{
			OpString other_handler;
			RETURN_IF_ERROR(other_handler.SetFromUTF8(handler));
			if (has_friendly_name)
			{
				OpString name;
				RETURN_IF_ERROR(g_languageManager->GetString(article_friendly_id, name));
				RETURN_IF_ERROR(I18n::Format(message, Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_FRIENDLY_NAME_KNOWN_HANDLER, other_handler, name));
				question_id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_FRIENDLY_NAME_KNOWN_HANDLER_QUESTION;
			}
			else
			{
				Str::LocaleString id(Str::NOT_A_STRING);
				if (is_protocol)
				{
					id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_NO_FRIENDLY_NAME_KNOWN_PROTOCOL_HANDLER;
					question_id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_KNOWN_PROTOCOL_HANDLER_QUESTION;
				}
				else
				{
					id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_NO_FRIENDLY_NAME_KNOWN_CONTENT_HANDLER;
					question_id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_KNOWN_CONTENT_HANDLER_QUESTION;
				}

				OpString format;
				RETURN_IF_ERROR(g_languageManager->GetString(id, format));
				if (!is_protocol)
					RETURN_IF_ERROR(message.Append(UNI_L("\n%2")));
				RETURN_IF_ERROR(I18n::Format(message, format.CStr(), other_handler, mime_or_protocol));
			}
		}
	}
	else
	{
		if (has_friendly_name)
		{
			OpString name;
			RETURN_IF_ERROR(g_languageManager->GetString(article_friendly_id, name));
			RETURN_IF_ERROR(I18n::Format(message, Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_FRIENDLY_NAME_UNKNOWN_HANDLER, name));
			question_id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_FRIENDLY_NAME_UNKNOWN_HANDLER_QUESTION;
		}
		else
		{
			Str::LocaleString id(Str::NOT_A_STRING);
			if (is_protocol)
			{
				id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_NO_FRIENDLY_NAME_UNKNOWN_PROTOCOL_HANDLER;
				question_id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_UNKNOWN_PROTOCOL_HANDLER_QUESTION;
			}
			else
			{
				id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_NO_FRIENDLY_NAME_UNKNOWN_CONTENT_HANDLER;
				question_id = Str::D_SECURITY_OPERATION_REGISTER_WEBHANDLER_UNKNOWN_CONTENT_HANDLER_QUESTION;
			}

			OpString format;
			RETURN_IF_ERROR(g_languageManager->GetString(id, format));
			if (!is_protocol)
				RETURN_IF_ERROR(format.Append(UNI_L("\n%1")));
			RETURN_IF_ERROR(I18n::Format(message, format, mime_or_protocol));
		}
	}

	m_webhandler_bar->SetStatusTextWrap(TRUE);
	m_webhandler_bar->SetStatusText(message);
	if (question_id != Str::NOT_A_STRING)
	{
		OpString question_argument;
		if (has_friendly_name)
			RETURN_IF_ERROR(g_languageManager->GetString(friendly_id, question_argument));

		OpString question;
		RETURN_IF_ERROR(I18n::Format(question, question_id, hostname, question_argument));
		m_webhandler_bar->SetQuestionText(question);
	}
	else
		m_webhandler_bar->ShowQuestionText(FALSE);

	m_webhandler_bar->Show();

	return OpStatus::OK;
}


void DocumentDesktopWindow::OnAskForPermissionCancelled(OpWindowCommander *wic, PermissionCallback *callback)
{
	for (DesktopPermissionCallback* desktop_callback = m_permission_callbacks.First();
			desktop_callback != NULL;
			desktop_callback = static_cast<DesktopPermissionCallback*>(desktop_callback->Suc()))
	{
		if (desktop_callback->GetPermissionCallback() == callback)
		{
			m_permission_callbacks.DeleteItem(desktop_callback);
			break;
		}
	}

	if (m_permission_controller)
	{
		m_permission_controller->DenyPermissionOnClosing(false);
		m_permission_controller->CancelDialog();
		m_permission_controller = NULL;
	}

	if (m_permission_callbacks.GetCount())
	{
		g_main_message_handler->PostMessage(MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) this, 0);
	}
}

void DocumentDesktopWindow::OnUserConsentInformation(OpWindowCommander* comm, const OpVector<OpPermissionListener::ConsentInformation>& info, INTPTR user_data)
{
	OP_ASSERT(comm == GetWindowCommander());
	if (user_data == ASK_FOR_PERMISSION_ON_USER_REQUEST)
	{
		GetAddressDropdown()->DisplayOverlayDialog(info);
	}
	else if (user_data == ASK_FOR_PERMISSION_ON_PAGE_LOAD)
	{
		PagebarButton* pagebar_button = GetPagebarButton();
		OpProtocolButton* protocol_button = GetAddressDropdown() ?  GetAddressDropdown()->GetProtocolButton() : NULL;
		if (pagebar_button)
		{
			pagebar_button->RemoveIndicator(OpIndicatorButton::GEOLOCATION);
		}
		if (protocol_button)
		{
			bool camera_allowed = false;
			bool geolocation_allowed = false;
			for (unsigned i = 0; i < info.GetCount(); ++i)
			{
				OpPermissionListener::ConsentInformation* information = info.Get(i);
				if (information->is_allowed)
				{
					if (information->permission_type == OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST)
					{
						geolocation_allowed = true;
					}
					else if (information->permission_type == OpPermissionListener::PermissionCallback::PERMISSION_TYPE_CAMERA_REQUEST)
					{
						camera_allowed = true;
					}
				}
			}
			if (geolocation_allowed)
			{
				protocol_button->AddIndicator(OpIndicatorButton::GEOLOCATION);
			}
			else
			{
				protocol_button->RemoveIndicator(OpIndicatorButton::GEOLOCATION);
			}
			if (camera_allowed)
			{
				protocol_button->AddIndicator(OpIndicatorButton::CAMERA);
			}
			else
			{
				protocol_button->RemoveIndicator(OpIndicatorButton::CAMERA);
			}
			OpIndicatorButton* indicator_button = protocol_button->GetIndicatorButton();
			indicator_button->SetIconState(OpIndicatorButton::GEOLOCATION, OpIndicatorButton::INACTIVE);
			indicator_button->SetIconState(OpIndicatorButton::CAMERA, OpIndicatorButton::INACTIVE);
		}
	}
	else
	{
		OP_ASSERT(!"Unknown user data");
	}
}

void DocumentDesktopWindow::ShowGeolocationLicenseDialog(OpPermissionListener::PermissionCallback::PersistenceType persistence)
{
	GeolocationPrivacyDialog* dialog = OP_NEW(GeolocationPrivacyDialog, (*this, persistence));
	if (dialog)
	{
		dialog->Init(g_application->GetActiveDesktopWindow());
	}
}

void DocumentDesktopWindow::SetUserConsent(const OpStringC hostname,
		OpPermissionListener::PermissionCallback::PermissionType type,
		BOOL3 allowed,
		OpPermissionListener::PermissionCallback::PersistenceType persistence)
{
	if (allowed != NO &&
			type == OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST &&
			g_pcui->GetIntegerPref(PrefsCollectionUI::ShowGeolocationLicenseDialog))
	{
		RETURN_VOID_IF_ERROR(m_permission_data.hostname.Set(hostname));
		m_permission_data.allowed = allowed;
		m_permission_data.persistence = persistence;
		ShowGeolocationLicenseDialog(persistence);
	}
	else
	{
		OpStatus::Ignore(GetWindowCommander()->SetUserConsent(type, persistence, hostname.CStr(), allowed));
		OpProtocolButton* protocol_button = GetAddressDropdown() ?  GetAddressDropdown()->GetProtocolButton() : NULL;
		if (protocol_button)
		{
			OpIndicatorButton::IndicatorType indicator_type =
				type == OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST ?
				OpIndicatorButton::GEOLOCATION :
				OpIndicatorButton::CAMERA;

			if (allowed == YES)
			{
				protocol_button->AddIndicator(indicator_type);
			}
			else
			{
				protocol_button->RemoveIndicator(indicator_type);
			}
		}
	}
}

void DocumentDesktopWindow::HandleCurrentPermission(bool allow, OpPermissionListener::PermissionCallback::PersistenceType persistence)
{
	m_permission_controller = NULL;
	DesktopPermissionCallback *callback = m_permission_callbacks.First();
	if (callback)
	{
		PermissionCallback *permission_callback = callback->GetPermissionCallback();
		OP_ASSERT(permission_callback);

		if (allow &&
			permission_callback->Type() == OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST &&
			g_pcui->GetIntegerPref(PrefsCollectionUI::ShowGeolocationLicenseDialog))
		{
			// user accepted the site's geolocation request, but hasn't been shown the dialog yet. Do that now.
			ShowGeolocationLicenseDialog(persistence);
			return;
		}

		if(allow)
		{
			permission_callback->OnAllowed(persistence);		
		}
		else
		{
			OpProtocolButton* protocol_button = GetAddressDropdown() ?  GetAddressDropdown()->GetProtocolButton() : NULL;
			if (protocol_button)
			{
				OpIndicatorButton::IndicatorType type =
					permission_callback->Type() == OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST ?
					OpIndicatorButton::GEOLOCATION :
					OpIndicatorButton::CAMERA;
				protocol_button->RemoveIndicator(type);
			}
			permission_callback->OnDisallowed(persistence);
		}
			
		m_permission_callbacks.DeleteItem(callback);
		if(m_permission_callbacks.GetCount())
		{
			// we have more to process but we must wait until previous dialog
			// is fully gone (fade out animation) before we show next dialog
			g_main_message_handler->PostDelayedMessage(MSG_QUICK_HANDLE_NEXT_PERMISSION_CALLBACK, (MH_PARAM_1) this, 0, DEFAULT_ANIMATION_DURATION * 1000);
		}
	}
	else
	{
		// if there is no callback on the queue request for permission change
		// must came from overlay dialog and all data are kept in m_permission_data

		if (allow)
		{
			OpStatus::Ignore(GetWindowCommander()->SetUserConsent(
					OpPermissionListener::PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST,
					m_permission_data.persistence,
					m_permission_data.hostname.CStr(),
					m_permission_data.allowed));

		}

		OpProtocolButton* protocol_button = GetAddressDropdown() ?  GetAddressDropdown()->GetProtocolButton() : NULL;
		if (protocol_button)
		{
			if (allow)
			{
				protocol_button->AddIndicator(OpIndicatorButton::GEOLOCATION);
			}
			else
			{
				protocol_button->RemoveIndicator(OpIndicatorButton::GEOLOCATION);
			}
		}

	}
}

void DocumentDesktopWindow::OnGeolocationAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts)
{
	if (hosts->GetCount())
	{
		PagebarButton* pagebar_button = GetPagebarButton();
		OpProtocolButton* protocol_button = GetAddressDropdown() ?  GetAddressDropdown()->GetProtocolButton() : NULL;
		if (pagebar_button)
		{
			pagebar_button->AddIndicator(OpIndicatorButton::GEOLOCATION);
		}
		if (protocol_button)
		{
			protocol_button->AddIndicator(OpIndicatorButton::GEOLOCATION);
			protocol_button->GetIndicatorButton()->SetIconState(OpIndicatorButton::GEOLOCATION, OpIndicatorButton::ACTIVE);
		}
	}
}

void DocumentDesktopWindow::OnCameraAccess(OpWindowCommander* commander, const OpVector<uni_char>* hosts)
{
	PagebarButton* pagebar_button = GetPagebarButton();
	OpProtocolButton* protocol_button = GetAddressDropdown() ?  GetAddressDropdown()->GetProtocolButton() : NULL;
	bool active = hosts->GetCount() > 0;
	if (pagebar_button)
	{
		if (active)
		{
			pagebar_button->AddIndicator(OpIndicatorButton::CAMERA);
		}
		else
		{
			pagebar_button->RemoveIndicator(OpIndicatorButton::CAMERA);
		}
	}
	if (protocol_button)
	{
		protocol_button->GetIndicatorButton()->SetIconState(OpIndicatorButton::CAMERA, active ?
				OpIndicatorButton::ACTIVE :
				OpIndicatorButton::INACTIVE);
		if (active)
		{
			protocol_button->AddIndicator(OpIndicatorButton::CAMERA);
		}
	}
}

OP_STATUS DocumentDesktopWindow::OnJSFullScreenRequest(OpWindowCommander* commander, BOOL enable_fullscreen)
{
	BrowserDesktopWindow* bdw = static_cast<BrowserDesktopWindow*>(GetParentDesktopWindow());
	if (bdw && bdw->SetFullscreenMode(enable_fullscreen ? true : false, true))
	{
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

void DocumentDesktopWindow::EnableTransparentSkin(BOOL enable)
{
	if (!m_address_bar)
		return;

	if (enable)
		m_address_bar->GetBorderSkin()->SetImage("Addressbar Transparent Skin", "Addressbar Skin");
	else
		m_address_bar->GetBorderSkin()->SetImage("Addressbar Skin");
}

bool DocumentDesktopWindow::IsPrivateIntroPageShown()
{
	return m_private_browsing
		&& GetWindowCommander() && m_doc_history->GetCurrentHistoryPos() > 0
		&& uni_strcmp(GetWindowCommander()->GetCurrentURL(FALSE), "opera:private") == 0;
}

// scope interface

/*
 * The Statusbar is not contained in the window, but it is (forced)
 * into the namedwidgetcollection of the window.
 * This hack is to ensure it is included in listing of widgets of the window
 * (making listing consistent with DoGetWidget which uses the 
 * namedcollection to look it up)
 */
OP_STATUS DocumentDesktopWindow::ListWidgets(OpVector<OpWidget> &widgets) 
{
	OpWidget* adr = GetWidgetByName("tba_address_field");
	if (adr)
	{
		((OpAddressDropDown*)adr)->ListPopupWindowWidgets(widgets);
	}

	return DesktopWindow::ListWidgets(widgets);
}

DownloadExtensionBar* DocumentDesktopWindow::GetDownloadExtensionBar(BOOL create_if_needed)
{
	return static_cast<DownloadExtensionBar *> (
		ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::DownloadExtensionbar,
		GetWidgetContainer()->GetRoot(),
		create_if_needed));
}

void DocumentDesktopWindow::LayoutToolbar(OpToolbar* toolbar, OpRect& rect_with_margin, OpRect& rect, BOOL compute_only, BOOL allow_margins)
{
	// Make sure this one is on top if it overlap with already layouted toolbars.
	if (!compute_only)
		toolbar->SetZ(OpWidget::Z_TOP);

	rect_with_margin = toolbar->LayoutToAvailableRect(rect_with_margin, compute_only);

	INT32 left = 0, top = 0, right = 0, bottom = 0;

	if(allow_margins)
		toolbar->GetBorderSkin()->GetMargin(&left,&top,&right,&bottom);

	if (toolbar->GetResultingAlignment() == OpBar::ALIGNMENT_LEFT)
	{
		rect.width = rect.Right() - rect_with_margin.x;
		rect.x = rect_with_margin.x;

		rect_with_margin.x += right;
		rect_with_margin.width -= right;
	}
	else if(toolbar->GetResultingAlignment() == OpBar::ALIGNMENT_RIGHT)
	{
		rect.width = rect_with_margin.Right() - rect.x;

		rect_with_margin.width -= left;
	}
	else if(toolbar->GetResultingAlignment() == OpBar::ALIGNMENT_TOP)
	{
		rect.height = rect.Bottom() - rect_with_margin.y;
		rect.y = rect_with_margin.y;

		rect_with_margin.y += bottom;
		rect_with_margin.height -= bottom;
	}
	else if(toolbar->GetResultingAlignment() == OpBar::ALIGNMENT_BOTTOM)
	{
		rect.height = rect_with_margin.Bottom() - rect.y;

		rect_with_margin.height -= top;
	}
}

void DocumentDesktopWindow::InstallSkin(const uni_char* url)
{
	InstallPersonaBar *toolbar = static_cast<InstallPersonaBar *>(ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::InstallPersonabar, GetWidgetContainer()->GetRoot(), TRUE));
	if(toolbar)
	{
		toolbar->SetURLInfo(url, URL_SKIN_CONFIGURATION_CONTENT);
	}
}

void DocumentDesktopWindow::ShowPluginCrashedBar(bool show)
{
	if (!m_plugin_crashed_bar)
		return;

	if (show)
		m_plugin_crashed_bar->Show();
	else
		m_plugin_crashed_bar->Hide();
}

#ifdef EXTENSION_SUPPORT
// workaround for DSK-319442 - should be removed when that bug is fixed
void DocumentDesktopWindow::CheckExtensionDocument()
{
	if (m_is_extension_document == MAYBE)
	{
		OpGadget* gadget = GetWindowCommander() ? GetWindowCommander()->GetGadget() : 0;
		if (gadget && gadget->IsExtension())
		{
			m_is_extension_document = YES;
			if (m_show_toolbars)
			{
				// address bar does not work with internal pages of extensions, other toolbars
				// may not work or may work only partially - just hide all of them
				m_show_toolbars = FALSE;
				m_address_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
#ifdef SUPPORT_NAVIGATION_BAR
				m_navigation_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
#endif // SUPPORT_NAVIGATION_BAR
				m_view_bar->SetAlignment(OpBar::ALIGNMENT_OFF);

				OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::PersonalbarInline, GetWidgetContainer()->GetRoot(), TRUE);
				if(personalbar)
				{
					personalbar->SetAlignment(OpBar::ALIGNMENT_OFF);
				}
				m_wandstore_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
				m_intranet_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
				m_local_storage_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
				m_plugin_crashed_bar->SetAlignment(OpBar::ALIGNMENT_OFF);
			}
		}
		else
		{
			m_is_extension_document = NO;
		}
	}
}
#endif

OP_STATUS DocumentDesktopWindow::GetHistoryList(OpAutoVector<DocumentHistoryInformation>& history_list, bool forward, bool fast) const
{
	return fast ? m_doc_history->GetFastHistoryList(history_list, forward) : m_doc_history->GetHistoryList(history_list, forward);
}

OP_STATUS DocumentDesktopWindow::SetupInitialView(BOOL no_speed_dial)
{
	// Decision between speeddial or private-tab intro page
	WindowCommanderProxy::CopySettingsToWindow(GetWindowCommander(), m_openurl_setting);
	bool has_private_page = false;
	if(m_private_browsing)
		has_private_page = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowPrivateIntroPage) && !m_openurl_setting.m_src_commander;

	if(!has_private_page
		&& !no_speed_dial
		&& !m_staged_init
		&& g_speeddial_manager->GetState() != SpeedDialManager::Disabled
		&& g_speeddial_manager->GetState() != SpeedDialManager::Folded)
	{
		OpString title;
		if (OpStatus::IsSuccess(g_languageManager->GetString(Str::S_SPEED_DIAL, title)))
			SetTitle(title.CStr());

		RETURN_IF_ERROR(WindowCommanderProxy::OpenURL(GetWindowCommander(), UNI_L("opera:speeddial"), TRUE, TRUE, TRUE));
	}
	else
	{
		RETURN_IF_ERROR(SetActiveDocumentView(DocumentView::DOCUMENT_TYPE_BROWSER));
		if (m_focus_document)
			m_document_view->SetFocus(FOCUS_REASON_OTHER);
		SetTitle();
	}

	if (m_private_browsing && has_private_page && GetWindowCommander())
	{
		if (!GetWindowCommander()->OpenURL(UNI_L("opera:private"), FALSE))
			return OpStatus::ERR;
	}

	if (!m_staged_init)
		UpdateWindowImage(TRUE);

	return OpStatus::OK;
}

OP_STATUS DocumentDesktopWindow::ShowPermissionPopup(DesktopPermissionCallback* callback)
{
	PermissionCallback* permission_callback = callback->GetPermissionCallback();
	OpWidget* root = GetWidgetContainer()->GetRoot();
	OpProtocolButton* protocol_button = GetAddressDropdown() ? GetAddressDropdown()->GetProtocolButton() : NULL;
	const uni_char* current_url = GetWindowCommander()->GetCurrentURL(FALSE);
	OpString toplevel_domain;
	unsigned starting_position = 0;
	RETURN_IF_ERROR(StringUtils::FindDomain(current_url, toplevel_domain, starting_position));
	OpString hostname;

	RETURN_IF_ERROR(hostname.Set(callback->GetAccessingHostname()));

	m_permission_controller =
		OP_NEW(PermissionPopupController, (permission_callback->Type(),
		*root,
		protocol_button,
		hostname.CStr(),
		this));
	RETURN_OOM_IF_NULL(m_permission_controller);

	RETURN_IF_ERROR(m_permission_controller->SetTopLevelDomain(toplevel_domain));

	if (protocol_button)
	{
		OpIndicatorButton* indicator = protocol_button->GetIndicatorButton();
		OpIndicatorButton::IndicatorType indicator_type;
		if (permission_callback->Type() == PermissionCallback::PERMISSION_TYPE_GEOLOCATION_REQUEST)
		{
			indicator_type = OpIndicatorButton::GEOLOCATION;
		}
		else
		{
			indicator_type = OpIndicatorButton::CAMERA;
		}
		protocol_button->AddIndicator(indicator_type);
		indicator->SetVisibility(TRUE);
	}

	RETURN_IF_ERROR(ShowDialog(m_permission_controller, g_global_ui_context, this));

	m_permission_controller->SetFocus(FOCUS_REASON_MOUSE);
	if (GetAddressDropdown() && GetAddressDropdown()->IsOnBottom())
	{
		m_permission_controller->SetIsOnBottom();
	}
	return OpStatus::OK;
}

DocumentView* DocumentDesktopWindow::GetDocumentViewFromType(DocumentView::DocumentViewTypes type)
{
	for (UINT i = 0; i < m_document_views.GetCount(); i++)
	{
		if (m_document_views.Get(i)->m_view->GetDocumentType() == type)
			return m_document_views.Get(i)->m_view;
	}

	return NULL;
}

OP_STATUS DocumentDesktopWindow::SetActiveDocumentView(const uni_char* url_str)
{
	if (!url_str || *url_str == '\0')
		return OpStatus::OK;

	return SetActiveDocumentView(DocumentView::GetType(url_str));
}

OP_STATUS DocumentDesktopWindow::SetActiveDocumentView(DocumentView::DocumentViewTypes type)
{
	if (m_document_views.GetCount() == 0)
		return OpStatus::ERR;

	DocumentViewInfo* info = NULL;
	if (m_active_doc_view_index != -1)
		info = m_document_views.Get(m_active_doc_view_index);

	if (info && info->m_view->GetDocumentType() == type)
	{
		info->m_view->Show(true);

		// Set focus in address bar input field when starting Opera with speeddial page
		if (type == DocumentView::DOCUMENT_TYPE_SPEED_DIAL && !m_handled_initial_document_view_activation)
		{
			OpAddressDropDown *dropdown = GetAddressDropdown();
			if (dropdown)
				dropdown->SetFocus(FOCUS_REASON_OTHER);
		}
		m_handled_initial_document_view_activation = TRUE;
		return OpStatus::OK;
	}

	UINT index;
	for (index = 0; index < m_document_views.GetCount() && m_document_views.Get(index)->m_view->GetDocumentType() != type; index++) { }
	if (!m_document_views.Get(index))
		return OpStatus::ERR;

	if (m_active_doc_view_index != -1)
		info->m_view->Show(false);

	m_active_doc_view_index = index;

	info = m_document_views.Get(m_active_doc_view_index);
	info->m_history_pos = m_doc_history->GetCurrentHistoryPos() ? m_doc_history->GetCurrentHistoryPos() : 1;
	info->m_view->Show(true);

	g_input_manager->UpdateAllInputStates();

	Relayout();
	BroadcastDesktopWindowContentChanged();
	UpdateWindowImage(FALSE);
	UpdateMDETransparency();

	return OpStatus::OK;
}

inline DocumentView* DocumentDesktopWindow::GetActiveDocumentView() const
{
	DocumentViewInfo *doc_view_info = m_document_views.Get(m_active_doc_view_index);

	return doc_view_info ? doc_view_info->m_view : NULL;
}

bool DocumentDesktopWindow::IsSpeedDialActive() const
{
	DocumentView *doc_view = GetActiveDocumentView();
	if (doc_view)
		return doc_view->GetDocumentType() == DocumentView::DOCUMENT_TYPE_SPEED_DIAL;

	return false;
}

inline bool DocumentDesktopWindow::IsBrowserViewActive() const
{
	DocumentView *doc_view = GetActiveDocumentView();
	if (doc_view)
		return doc_view->GetDocumentType() == DocumentView::DOCUMENT_TYPE_BROWSER;

	return false;
}

OP_STATUS DocumentDesktopWindow::CreateDocumentView(DocumentView::DocumentViewTypes type, OpWindowCommander* commander)
{
	switch(type)
	{
		case DocumentView::DOCUMENT_TYPE_BROWSER:
			 return InitDocumentView(OP_NEW(OpBrowserView, (!g_application->IsEmBrowser(), commander)));

		case DocumentView::DOCUMENT_TYPE_COLLECTION:
			 return InitDocumentView(OP_NEW(OpCollectionView, ()));

		case DocumentView::DOCUMENT_TYPE_SPEED_DIAL:
			{
				RETURN_IF_ERROR(InitDocumentView(OP_NEW(OpSpeedDialView, ())));
				#ifdef ENABLE_USAGE_REPORT
				for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
					m_listeners.GetNext(iterator)->OnSpeedDialViewCreated(this);
				#endif
				return OpStatus::OK;
			}

		default:
			OP_ASSERT(0);
			break;
	}

	return OpStatus::ERR;
}

OP_STATUS DocumentDesktopWindow::InitDocumentView(DocumentView* view)
{
	RETURN_OOM_IF_NULL(view);
	OpAutoPtr<DocumentView> holder(view);

	OpAutoPtr<DocumentViewInfo> doc_info(OP_NEW(DocumentViewInfo, ()));
	RETURN_OOM_IF_NULL(doc_info.get());

	RETURN_IF_ERROR(view->Create(GetWidgetContainer()->GetRoot()));
	RETURN_IF_ERROR(m_document_views.Add(doc_info.get()));
	doc_info->m_view = view;

	// Integral value 3 is derived from enum DocumentViewTypes.
	OP_ASSERT(m_document_views.GetCount() <= 3);

	doc_info.release();
	holder.release();
	return OpStatus::OK;
}

void DocumentDesktopWindow::ReleaseDocumentView(bool force_release)
{
	HistoryInformation result;
	INT32 pos = m_doc_history->GetCurrentHistoryPos();
	RETURN_VOID_IF_ERROR(m_doc_history->GetHistoryInformation(DocumentHistory::Position(DocumentHistory::Position::NORMAL, pos), &result));
	DocumentView* doc_view = GetDocumentViewFromType(DocumentView::GetType(result.url));
	if (!doc_view)
		return;

	DocumentViewInfo* active_info = m_document_views.Get(m_active_doc_view_index);
	OpVector<DocumentViewInfo> remove_list;

	for (UINT32 i = 0; i < m_document_views.GetCount(); i++)
	{
		DocumentViewInfo* info = m_document_views.Get(i);

		// Undeletable view
		if (info->m_view->GetDocumentType() == DocumentView::DOCUMENT_TYPE_BROWSER)
			continue;

		if (doc_view->GetDocumentType() == info->m_view->GetDocumentType())
			continue;

		/*
		 * Document views which are of not type DOCUMENT_TYPE_BROWSER are destroyed
		 * if they are far behind/ahead of current history position.
		 * Having that said, if difference is more than 3 w.r.t current history position
		 * document will be released.
		 */
		if (force_release || abs(pos - info->m_history_pos) > 3)
			RETURN_VOID_IF_ERROR(remove_list.Add(m_document_views.Get(i)));
	}

	// Delete DocumentViews and removed them from the list
	for (UINT32 i = 0; i < remove_list.GetCount(); i++)
	{
		OP_ASSERT(remove_list.Get(i)->m_view->GetOpWidget());
		remove_list.Get(i)->m_view->GetOpWidget()->Delete();
		m_document_views.Delete(remove_list.Get(i));
	}

	// Update active index after DocumentViews are removed
	if (remove_list.GetCount())
		m_active_doc_view_index = m_document_views.Find(active_info);
}

/***********************************************************************************
**
**	CycleAccessKeysPopupWindow - Used for cycling between the access keys defined on a page
**
***********************************************************************************/
CycleAccessKeysPopupWindow::CycleAccessKeysPopupWindow(DocumentDesktopWindow* document_window)
	:m_document_window(document_window)
{
}

OP_STATUS CycleAccessKeysPopupWindow::Init()
{	
	OP_STATUS status;

	status = DesktopWindow::Init(OpWindow::STYLE_POPUP, m_document_window->GetOpWindow(), OpWindow::EFFECT_DROPSHADOW);
	RETURN_IF_ERROR(status);

	m_browser_window = g_application->GetActiveBrowserDesktopWindow();

	WidgetContainer* widget_container = GetWidgetContainer();
	widget_container->SetEraseBg(TRUE);

	OpWidget* root_widget = widget_container->GetRoot();
	root_widget->SetHasCssBorder(FALSE);

	status = OpToolbar::Construct(&m_keys_list);
	RETURN_IF_ERROR(status);

	root_widget->AddChild(m_keys_list);

	m_keys_list->SetDeselectable(TRUE);
	m_keys_list->SetAlignment(OpBar::ALIGNMENT_LEFT);
	m_keys_list->SetWrapping(OpBar::WRAPPING_OFF);
	m_keys_list->SetSelector(TRUE);
	m_keys_list->SetFixedHeight(OpToolbar::FIXED_HEIGHT_STRETCH);
	m_keys_list->SetButtonType(OpButton::TYPE_TOOLBAR);
	m_keys_list->SetButtonStyle(OpButton::STYLE_TEXT);
	m_keys_list->GetBorderSkin()->SetImage("Accesskeys Cycler Skin");
	m_keys_list->SetSkinned(TRUE);

	return OpStatus::OK;
}

CycleAccessKeysPopupWindow::~CycleAccessKeysPopupWindow()
{
	BroadcastDesktopWindowClosing(FALSE);

	m_document_window->m_cycles_accesskeys_popupwindow = NULL;
	m_document_window->GetWindowCommander()->SetAccessKeyMode(FALSE);

	ClearAccessKeys();
}

void CycleAccessKeysPopupWindow::UpdateDisplay()
{
	INT32 num_columns = 1;
	INT32 list_width, list_height;
	INT32 width = 0, height = 0;
	OpRect window_rect;
	OpRect screen_rect;
	INT32 padding_left, padding_top, padding_right, padding_bottom;

	WidgetContainer* widget_container = GetWidgetContainer();
	OpWidget* root_widget = widget_container->GetRoot();

	if(m_browser_window)
	{
		m_browser_window->GetScreenRect(screen_rect, TRUE);
		m_browser_window->GetRect(window_rect);
	}
	else
	{
		m_document_window->GetScreenRect(screen_rect, TRUE);
		m_document_window->GetRect(window_rect);
	}
	m_keys_list->GetRequiredSize(list_width, list_height);

	INT32 max_list_height = ((screen_rect.height * 3) / 4);

	if( list_height > max_list_height)
	{
		// We have to use multiple columns

		m_keys_list->SetWrapping(OpBar::WRAPPING_NEWLINE);
		m_keys_list->SetFixedMaxWidth(150); // Maybe

		m_keys_list->GetRequiredSize(screen_rect.width, max_list_height, list_width, list_height);

		num_columns = m_keys_list->GetComputedColumnCount();

		list_width  = max(150 * num_columns, list_width);
		list_height = max_list_height;
	}
	else
	{
		if (list_width < 400)
			list_width = 400;
	}

	if( list_height > screen_rect.height )
		list_height = screen_rect.height;

	if( list_width > screen_rect.width )
		list_width = screen_rect.width;

	root_widget->GetBorderSkin()->GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom);

	SetInnerSize(list_width + padding_left + padding_right, list_height + padding_top + padding_bottom);
	GetOuterSize((UINT32&)width, (UINT32&)height);

	int posx = window_rect.x + (window_rect.width - width) / 2;

	if( posx + width >= screen_rect.x + screen_rect.width )
	{
		posx = screen_rect.x + screen_rect.width - width;
	}

	if( posx < screen_rect.x )
	{
		posx = screen_rect.x;
	}

	int posy = window_rect.y + (window_rect.height - height)/2;

	if( posy + height >= screen_rect.y + screen_rect.height  )
	{
		posy = screen_rect.y + screen_rect.height - height;
	}

	if( posy < screen_rect.y )
	{
		posy = screen_rect.y;
	}

	SetOuterPos(posx, posy);

	m_keys_list->SetGrowToFit( num_columns == 1 );
	m_keys_list->SetRect(OpRect(0, 0, list_width, list_height));
}

OP_STATUS CycleAccessKeysPopupWindow::AddAccessKey(const uni_char *title, const uni_char *url, const uni_char key)
{
	CycleAccessKeysPopupWindow::CycleAccessKeyItem *item = OP_NEW(CycleAccessKeysPopupWindow::CycleAccessKeyItem, ());
	if(!item)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	item->title.Set(title);
	item->url.Set(url);
	item->key = key;

	if(url == NULL && key == 0)
	{
		item->display_name.AppendFormat(UNI_L("%s"), item->title.CStr());
	}
	else
	{
		item->display_name.AppendFormat(UNI_L("(%c) %s"), item->key, item->title.Length() ? item->title.CStr() : item->url.CStr());
	}
	OpInputAction* input_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_INVOKE_ACCESS_KEY));
	if (input_action)
	{
		input_action->SetActionData((INTPTR)item);
	}
	OpButton* button = m_keys_list->AddButton(item->display_name.CStr(), NULL, input_action, (void*) item);
	if(!button)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	button->SetRelativeSystemFontSize(150);
	button->GetBorderSkin()->SetImage("Accesskeys Cycler Item Skin", "Toolbar Button Skin");
	button->font_info.justify = JUSTIFY_LEFT;
	button->SetEllipsis(g_pcui->GetIntegerPref(PrefsCollectionUI::EllipsisInCenter) == 1 ? ELLIPSIS_CENTER : ELLIPSIS_END);
	button->SetFixedTypeAndStyle(TRUE);

	return OpStatus::OK;
}

void CycleAccessKeysPopupWindow::ClearAccessKeys()
{
	INT32 cnt;

	for(cnt = 0; cnt < m_keys_list->GetWidgetCount(); cnt++)
	{
		CycleAccessKeysPopupWindow::CycleAccessKeyItem *item = (CycleAccessKeysPopupWindow::CycleAccessKeyItem *)m_keys_list->GetUserData(cnt);
		OP_DELETE(item);
	}
	while(m_keys_list->GetWidgetCount())
	{
		m_keys_list->RemoveWidget(0);
	}
}

BOOL CycleAccessKeysPopupWindow::OnInputAction(OpInputAction* action)
{
 	switch (action->GetAction())
 	{
		case OpInputAction::ACTION_INVOKE_ACCESS_KEY:
		{
			if(action->GetActionData())
			{
				CycleAccessKeysPopupWindow::CycleAccessKeyItem *item = (CycleAccessKeysPopupWindow::CycleAccessKeyItem *)action->GetActionData();

				if (m_document_window->GetWindowCommander()->GetAccessKeyMode())
				{
					OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED));
					if (action)
					{
						action->SetActionMethod(OpInputAction::METHOD_KEYBOARD);
						action->SetActionData((INTPTR)item->key);

						g_input_manager->InvokeAction(action);
					}
					// "this" is invalid at this point, so do nothing else
					return TRUE;
				}
			}
			break;
		}
 	}
 	return FALSE;
}
