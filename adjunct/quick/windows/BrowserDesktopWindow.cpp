/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2002-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/windows/BrowserDesktopWindow.h"

// Features

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES

#ifdef SUPPORT_VISUAL_ADBLOCK
# include "adjunct/quick/dialogs/ContentBlockDialog.h"
# include "modules/content_filter/content_filter.h"
# ifdef CF_CAP_HAS_CONTENT_BLOCK_FILTER_CREATION
# include "modules/content_filter/content_filter.h"
# endif // CF_CAP_HAS_CONTENT_BLOCK_FILTER_CREATION
#endif // SUPPORT_VISUAL_ADBLOCK

#ifdef ENABLE_USAGE_REPORT
# include "adjunct/quick/usagereport/UsageReport.h"
#endif // ENABLE_USAGE_REPORT

#ifdef M2_SUPPORT
# include "adjunct/m2_ui/windows/MailDesktopWindow.h"
# include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
# include "adjunct/m2_ui/windows/ChatDesktopWindow.h"
#endif

#ifdef MSWIN
# include "platforms/windows/windows_ui/res/#BuildNr.rci"	//for VER_BUILD_NUMBER_STR
#endif

// Adjunct
#include "adjunct/quick/application/SessionLoadContext.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/application/SessionLoadContext.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/WindowCommanderProxy.h"

// Desktop pi
#include "adjunct/desktop_pi/desktop_menu_context.h"

// Desktop util
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/prefs/PrefsUtils.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/string/stringutils.h"

// Dialogs
#include "adjunct/quick/dialogs/PreferencesDialog.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/controller/FullscreenPopupController.h"

// Hotlist
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/hotlist/HotlistManager.h"

// Managers
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/FindTextManager.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/quick/managers/ToolbarManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"

// Panels
#include "adjunct/quick/panels/BookmarksPanel.h"
#include "adjunct/quick/panels/ContactsPanel.h"
#include "adjunct/quick/panels/NotesPanel.h"
#include "adjunct/quick/panels/TransfersPanel.h"

// Widgets
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/OpThumbnailPagebar.h"
#include "adjunct/quick/widgets/OpDragScrollbar.h"
#include "adjunct/quick/widgets/OpSearchDropDown.h"
#include "adjunct/quick/widgets/OpStatusbar.h"
#include "adjunct/quick_toolkit/widgets/OpSplitter.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/widgets/OpToolTip.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"

// Windows
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/windows/DevToolsDesktopWindow.h"
#include "adjunct/quick/windows/HotlistDesktopWindow.h"

// PI
#include "adjunct/desktop_pi/desktop_notifier.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"

// Core
#include "modules/dochand/winman.h"
#include "modules/display/VisDevListeners.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/thumbnails/thumbnailmanager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef WIN_CE
# include "platforms/win_ce/res/buildnum.h"
#endif

#ifdef _UNIX_DESKTOP_
# include "platforms/unix/product/config.h"
#endif

#ifdef _MACINTOSH_
# include "platforms/mac/Resources/buildnum.h"
# include "platforms/mac/model/CMacSpecificActions.h"
#endif

BlockedPopup::BlockedPopup(DocumentDesktopWindow* source_window, const uni_char* title, const uni_char* url, OpDocumentListener::JSPopupOpener* opener)
{
	m_source_window = source_window;
	m_source_url = WindowCommanderProxy::GetMovedToURL(source_window->GetWindowCommander());
	m_title.Set(title);
	m_url.Set(url);
	m_opener = opener;
	m_id = OpTypedObject::GetUniqueID();
}

BOOL BlockedPopup::IsFrom(DocumentDesktopWindow* source_window)
{
	return source_window == m_source_window &&
		WindowCommanderProxy::GetMovedToURL(source_window->GetWindowCommander()) == m_source_url;
}


class CloseTabsDialogController : public SimpleDialogController
{
public:
	CloseTabsDialogController(BrowserDesktopWindow* browser_window, bool include_active)
		: SimpleDialogController(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_QUESTION,
			WINDOW_NAME_REALLY_CLOSE_ALL,
			include_active ? Str::D_REALLY_CLOSE_ALL_PAGES : Str::D_REALLY_CLOSE_ALL_BUT_ACTIVE_PAGE,
			include_active ? Str::MI_IDM_CloseAll : Str::M_BROWSER_WINDOW_MENU_CLOSEALL_BUT_ACTIVE)
		, m_browser_window(browser_window)
		, m_include_active(include_active)
	{}

	virtual void OnOk()
	{
		if (m_include_active)
			m_browser_window->CloseAllTabs();
		else
			m_browser_window->CloseAllTabsExceptActive();
	}

private:
	BrowserDesktopWindow* m_browser_window;
	bool m_include_active;
};



/***********************************************************************************
**
**	BrowserDesktopWindow
**
***********************************************************************************/

OP_STATUS BrowserDesktopWindow::Construct(BrowserDesktopWindow** browser_desktop_window
#ifdef DEVTOOLS_INTEGRATED_WINDOW
											, BOOL devtools_only
#endif // DEVTOOLS_INTEGRATED_WINDOW
											, BOOL privacy_browsing
											, BOOL lazy_init
								)
{
#ifdef DEVTOOLS_INTEGRATED_WINDOW
	*browser_desktop_window = OP_NEW(BrowserDesktopWindow, (devtools_only, privacy_browsing, lazy_init));
#else
	*browser_desktop_window = OP_NEW(BrowserDesktopWindow, (privacy_browsing));
#endif // DEVTOOLS_INTEGRATED_WINDOW

	if (*browser_desktop_window == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*browser_desktop_window)->init_status))
	{
		OP_DELETE(*browser_desktop_window);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

BrowserDesktopWindow::BrowserDesktopWindow(
#ifdef DEVTOOLS_INTEGRATED_WINDOW
	BOOL devtools_only,
#endif // DEVTOOLS_INTEGRATED_WINDOW
	BOOL privacy_browsing,
	BOOL lazy_init
) :
#ifdef SUPPORT_MAIN_BAR
	m_main_bar(NULL),
#endif // SUPPORT_MAIN_BAR
	m_page_bar(NULL),
	m_drag_scrollbar(NULL),
	m_side_drag_scrollbar(NULL),
	m_status_bar(NULL),
	m_splitter(NULL),
	m_workspace(NULL),
	m_timer(NULL),
#ifdef DEVTOOLS_INTEGRATED_WINDOW
	m_devtools_splitter(0),
	m_top_section(0),
	m_bottom_section(0),
	m_devtools_workspace(0),
	m_devtools_only(devtools_only),
#endif // DEVTOOLS_INTEGRATED_WINDOW
	m_find_text_manager(NULL),
	m_hotlist(NULL),
	m_hotlist_toggle_button(NULL),
	m_floating_hotlist_window(NULL),
	m_cycle_pages_popup(NULL),
	m_undo_session(NULL),
	m_fullscreen_popup(NULL),
	m_privacy_mode(privacy_browsing),
	m_transparent_top(0),
	m_transparent_bottom(0),
	m_transparent_left(0),
	m_transparent_right(0),
	m_transparent_skins_enabled(FALSE),
	m_transparent_skins_enabled_top_only(FALSE),
	m_transparent_skin_sections_available(FALSE),
	m_transparent_full_window(FALSE),
	m_fullscreen_is_scripted(false)
{
// Beware!!
// Changing the order in which widget children are added will change TAB selection order!!

#ifdef VEGA_OPPAINTER_SUPPORT
	QuickDisableAnimationsHere dummy;
#endif

	OP_STATUS status;
	status = DesktopWindow::Init(OpWindow::STYLE_DESKTOP);

	CHECK_STATUS(status);

	m_has_hidden_internals = (lazy_init != FALSE);

	m_timer = OP_NEW(OpTimer, ());
	if (!m_timer)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	m_timer->SetTimerListener(this);

	OpWidget* root_widget = GetWidgetContainer()->GetRoot();

	TRAPD(err, m_undo_session = g_session_manager->CreateSessionInMemoryL());
	OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what on error ? CHECK_STATUS ?

	{
		OP_PROFILE_METHOD("Initialized toolbars");
#ifdef DEVTOOLS_INTEGRATED_WINDOW
		// Construct a splitter that will envelop the whole window. The top will be the orignal old browser and
		// the bottom will hold the new devtools window
		status = OpSplitter::Construct(&m_devtools_splitter);
		CHECK_STATUS(status);
		root_widget->AddChild(m_devtools_splitter);
		m_devtools_splitter->GetBorderSkin()->SetImage("Hotlist Splitter skin");
		m_devtools_splitter->SetHorizontal(false);
		m_devtools_splitter->SetPixelDivision(true);

		// Load the size of the splitter from the prefs file
		m_devtools_splitter->SetDivision(g_pcui->GetIntegerPref(PrefsCollectionUI::DevToolsSplitter));

		// Widget to hold all of the elements in the top section of the splitter

		// (anders) Changed m_top_section to be an OpGroup, rather than an OpWidget.
		// We don't really need a group, but a plain widget will (based on OpTypedObject type)
		// be flagged as accessibility ignored.
		//m_top_section = new OpWidget();
		OpGroup::Construct(&m_top_section);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		m_top_section->AccessibilitySkipsMe();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

		m_devtools_splitter->AddChild(m_top_section);

		// Make the top section the root widget from now on so that all of the old (main) parts
		// of the window still go into the stop part of the window
		root_widget = m_top_section;

		// Create an OpWorkspace that will be attached to the bottom part of the splitter and
		// will hold the devtools window itself
		m_devtools_workspace = OP_NEW(OpWorkspace, (&GetModelItem()));
		m_devtools_workspace->SetMinHeight(DevToolsDesktopWindow::GetMinimumWindowHeight());
		m_devtools_splitter->AddChild(m_devtools_workspace);
		status = m_devtools_workspace->init_status;
		CHECK_STATUS(status);
		m_devtools_workspace->SetVisibility(FALSE);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		m_devtools_workspace->AccessibilityPrunesMe(ACCESSIBILITY_PRUNE_WHEN_INVISIBLE);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

		// Hide the top part for devtools only
		m_top_section->SetVisibility(!m_devtools_only);
#endif // DEVTOOLS_INTEGRATED_WINDOW

#ifdef SUPPORT_MAIN_BAR
		status = OpToolbar::Construct(&m_main_bar, PrefsCollectionUI::MainbarAlignment);
		CHECK_STATUS(status);
		root_widget->AddChild(m_main_bar);
		m_main_bar->GetBorderSkin()->SetImage("Mainbar Skin");
		m_main_bar->SetName("Browser Toolbar");
		m_main_bar_alignment = m_main_bar->GetAlignment();
#endif // SUPPORT_MAIN_BAR

		OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::Personalbar, m_top_section, TRUE);
		if(personalbar)
		{
			personalbar->GetBorderSkin()->SetImage("Personalbar Skin");
			personalbar->SetName("Personalbar");
		}

		status = OpStatusbar::Construct(&m_status_bar);
		CHECK_STATUS(status);
		root_widget->AddChild(m_status_bar);

		status = OpSplitter::Construct(&m_splitter);
		CHECK_STATUS(status);
		root_widget->AddChild(m_splitter);
		m_splitter->GetBorderSkin()->SetImage("Hotlist Splitter skin");
		m_splitter->SetDividerSkin("Hotlist Divider Skin");
		m_splitter->SetHorizontal(true);

		m_splitter->SetDivision(g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSplitter));

		if (!(m_hotlist = OP_NEW(Hotlist, (PrefsCollectionUI::HotlistAlignment))))
		{
			init_status = OpStatus::ERR_NO_MEMORY;
			return;
		}

		if (!(m_workspace = OP_NEW(OpWorkspace, (&GetModelItem()))))
		{
			OP_DELETE(m_hotlist);
			m_hotlist = NULL;
			init_status = OpStatus::ERR_NO_MEMORY;
			return;
		}

		m_splitter->AddChild(m_hotlist);
		status = m_hotlist->init_status;
		CHECK_STATUS(status);

		m_workspace->SetParentWindowID(GetID());
		status = m_workspace->AddListener(this);
		CHECK_STATUS(status);

#ifdef ENABLE_USAGE_REPORT
		if(g_workspace_report)
		{
			m_workspace->AddListener(g_workspace_report);
		}
#endif
		g_desktop_extensions_manager->StartListeningTo(m_workspace);

		m_splitter->AddChild(m_workspace);
		status = m_workspace->init_status;
		CHECK_STATUS(status);
		m_workspace->SetVisibility(TRUE);

		OpPagebar* page_bar = NULL;
		status = OpPagebar::Construct(&page_bar, m_workspace);
		CHECK_STATUS(status);

		m_page_bar = static_cast<OpThumbnailPagebar*>(page_bar);

		root_widget->AddChild(m_page_bar);

		status = OpDragScrollbar::Construct(&m_drag_scrollbar);
		CHECK_STATUS(status);

		m_drag_scrollbar->GetBorderSkin()->SetImage("Drag Scrollbar Skin", "Toolbar Skin");

		m_drag_scrollbar->SetName("Drag Scrollbar");
		m_drag_scrollbar->SetDragScrollbarTarget(m_page_bar);

		root_widget->AddChild(m_drag_scrollbar);

		status = OpDragScrollbar::Construct(&m_side_drag_scrollbar);
		CHECK_STATUS(status);

		m_side_drag_scrollbar->SetName("Side Drag Scrollbar");
		m_side_drag_scrollbar->SetDragScrollbarTarget(m_page_bar);

		root_widget->AddChild(m_side_drag_scrollbar);

		status = OpButton::Construct(&m_hotlist_toggle_button);
		CHECK_STATUS(status);
		root_widget->AddChild(m_hotlist_toggle_button);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
		m_hotlist_toggle_button->AccessibilityPrunesMe(ACCESSIBILITY_PRUNE_WHEN_INVISIBLE);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

		OpInputAction* toggle_action;
		if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString("Set alignment, \"hotlist\", 6 > Set alignment, \"hotlist\", 0", toggle_action)))
			m_hotlist_toggle_button->SetAction(toggle_action);
		m_hotlist_toggle_button->SetButtonTypeAndStyle(OpButton::TYPE_TOOLBAR, OpButton::STYLE_IMAGE);
		m_hotlist_toggle_button->GetBorderSkin()->SetImage("Panel Toggle Skin");
	}
	{
		OP_PROFILE_METHOD("Initialized find text manager");
		if (!(m_find_text_manager = OP_NEW(FindTextManager, ())))
		{
			init_status = OpStatus::ERR_NO_MEMORY;
			return;
		}
		status = m_find_text_manager->Init(this);
		CHECK_STATUS(status);
	}
	//m_header			= NULL;

	m_floating_hotlist_window	= NULL;

	if( KioskManager::GetInstance()->GetEnabled() && !KioskManager::GetInstance()->GetKioskWindows() )
	{
		m_page_bar->SetAlignment(OpBar::ALIGNMENT_OFF);

		m_drag_scrollbar->SetVisibility(FALSE);
		m_side_drag_scrollbar->SetVisibility(FALSE);
	}

	if( KioskManager::GetInstance()->GetNoMenu() )
	{
		GetOpWindow()->SetMenuBarVisibility(FALSE);
	}
	else
	{
		BOOL show_menu = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu);

		GetOpWindow()->SetMenuBarVisibility(show_menu);

		EnsureMenuButton(show_menu ? FALSE : TRUE);
	}

	if( KioskManager::GetInstance()->GetNoHotlist() )
	{
		ToolbarManager::GetInstance()->SetAlignment(ToolbarManager::Personalbar, m_top_section, OpBar::ALIGNMENT_OFF);

		m_hotlist->SetAlignment(OpBar::ALIGNMENT_OFF);
	}

	// Don't propagate this icon to OS, since application might already have a native icon
	SetIcon("Window Browser Icon", FALSE);

#ifdef DEVTOOLS_INTEGRATED_WINDOW
	// Show the devtools window if needed
	if (m_devtools_only)
		AttachDevToolsWindow(TRUE);
#endif

	m_transparent_full_window = g_skin_manager->GetOptionValue("Full Transparency", FALSE);
	m_transparent_skin_sections_available = m_transparent_full_window || g_skin_manager->GetOptionValue("Transparency", FALSE);

	{
		OP_PROFILE_METHOD("Enabled transparent skins");

#ifdef _MACINTOSH_
		EnableTransparentSkins(TRUE, m_transparent_skins_enabled_top_only);
#else
		// SetMenubarVisibility can trigger EnableTransparentSkins, so we need to make sure it is set again after m_transparent_skin_sections_available has been set
		EnableTransparentSkins(m_transparent_skins_enabled, m_transparent_skins_enabled_top_only, TRUE);
#endif // _MACINTOSH_
	}

	UpdateWindowTitle();

	if (!lazy_init)
	{
		status = InitHiddenInternals();
		CHECK_STATUS(status);
	}

	{
		OP_PROFILE_METHOD("Broadcast top level window created");

		g_application->BroadcastOnBrowserWindowCreated(this);
	}

	InitDispatchable();

}

/***********************************************************************************
**
**	~BrowserDesktopWindow
**
***********************************************************************************/

BrowserDesktopWindow::~BrowserDesktopWindow()
{
	OP_DELETE(m_find_text_manager);
	if(m_undo_session)
	{
		m_undo_session->Cancel();
		OP_DELETE(m_undo_session);
		m_undo_session = NULL;
	}
	OP_DELETE(m_timer);

	// just to make sure they're all gone
	if (GetWidgetContainer())
		ToolbarManager::GetInstance()->UnregisterToolbars(GetWidgetContainer()->GetRoot());
	ToolbarManager::GetInstance()->UnregisterToolbars(m_top_section);

	if (m_cycle_pages_popup)
	{
		m_cycle_pages_popup->Close(TRUE);
	}

#ifdef DEVTOOLS_INTEGRATED_WINDOW
	if (m_devtools_workspace)
	{
		m_devtools_workspace->RemoveListener(this);
		m_devtools_workspace->Delete();
	}
#endif // DEVTOOLS_INTEGRATED_WINDOW

	if(m_workspace)
	{
		m_workspace->RemoveListener(this);
		g_desktop_extensions_manager->StopListeningTo(m_workspace);
		m_workspace->Delete();
	}

#ifndef PREFS_NO_WRITE
	OP_STATUS err;

	if (m_splitter)
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::HotlistSplitter, (int)m_splitter->GetDivision()));

	if (m_devtools_splitter)
		TRAP(err, g_pcui->WriteIntegerL(PrefsCollectionUI::DevToolsSplitter, (int)m_devtools_splitter->GetDivision()));
#endif
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void BrowserDesktopWindow::OnSettingsChanged(DesktopSettings* settings)
{
	DesktopWindow::OnSettingsChanged(settings);

	if (settings->IsChanged(SETTINGS_LANGUAGE))
	{
		UpdateWindowTitle();
	}
	if (settings->IsChanged(SETTINGS_DELETE_PRIVATE_DATA))
	{
		if (PrivacyManager::GetInstance()->GetDeleteState(PrivacyManager::ALL_WINDOWS))
		{
			m_find_text_manager->SetSearchText(UNI_L(""));
		}
	}
	if(settings->IsChanged(SETTINGS_SKIN))
	{
		m_transparent_full_window = g_skin_manager->GetOptionValue("Full Transparency", FALSE);
		m_transparent_skin_sections_available = m_transparent_full_window || g_skin_manager->GetOptionValue("Transparency", FALSE);
	
		EnableTransparentSkins(m_transparent_skins_enabled, m_transparent_skins_enabled_top_only, TRUE);
		((DesktopOpWindow*)GetOpWindow())->SetFullTransparency(m_transparent_full_window);
	}
}

BOOL BrowserDesktopWindow::HasThumbnailsVisible()
{
	return m_page_bar->CanUseThumbnails();
}

void BrowserDesktopWindow::EnsureMenuButton(BOOL enable)
{
	if(m_page_bar)
	{
		m_page_bar->EnsureMenuButton(enable);
	}
}

// scope
BOOL BrowserDesktopWindow::GetMenubarQuickMenuInfoItems(OpScopeDesktopWindowManager_SI::QuickMenuInfo &out)
{
	if (static_cast<DesktopOpWindow*>(GetOpWindow())->GetMenubarQuickMenuInfoItems(out))
	{
		out.GetWindowIdRef().SetWindowID(GetID());
		return TRUE;
	}
	return FALSE;
}

Hotlist* BrowserDesktopWindow::GetHotlist()
{
	if (OpStatus::IsSuccess(InitHiddenInternals()))
		return m_hotlist;
	return NULL;
}

void BrowserDesktopWindow::UpdateWindowTitle(BOOL delay)
{
	if (delay)
	{
		m_timer->Start(1);
		return;
	}

	DesktopWindow* desktop_window = GetActiveDesktopWindow();

#ifdef DEVTOOLS_INTEGRATED_WINDOW
	if (m_devtools_only)
	{
		desktop_window = m_devtools_workspace->GetActiveDesktopWindow();
		if (desktop_window) {
			// The title of the detached devtools is the same as the page title
			SetTitle(desktop_window->GetTitle());
			desktop_window->UpdateWindowIcon();
		}
	}
	else
	{
#endif //DEVTOOLS_INTEGRATED_WINDOW
		OpString caption;
		caption.Set(g_pcui->GetStringPref(PrefsCollectionUI::ProgramTitle));

		if (caption.IsEmpty())
			RETURN_VOID_IF_ERROR(g_desktop_op_system_info->GetDefaultWindowTitle(caption));

		const uni_char *wcaption = desktop_window ? desktop_window->GetTitle(FALSE) : 0;

		// A user defined window caption may want to display the page title (%t)
		if (wcaption && caption.Find(UNI_L("%t")) != KNotFound)
		{
			StringUtils::Replace(caption, UNI_L("%t"), wcaption);
			wcaption = 0;
		}

		// A user defined window caption may want to display the build number (%s)...
		StringUtils::Replace(caption, UNI_L("%s"), UNI_L(VER_BUILD_NUMBER_STR));

		// ...or the browser version (%v)
		StringUtils::Replace(caption, UNI_L("%v"), UNI_L(VER_NUM_STR));

		// Prepend page title if not yet included
		if (caption.IsEmpty())
		{
			caption.Set(wcaption);
		}
		else if (wcaption && *wcaption)
		{
			caption.Insert(0, wcaption);
			caption.Insert(uni_strlen(wcaption), UNI_L(" - "));
		}

		SetTitle(caption.CStr());
#ifdef DEVTOOLS_INTEGRATED_WINDOW
	}
#endif //DEVTOOLS_INTEGRATED_WINDOW
}

#ifdef DEVTOOLS_INTEGRATED_WINDOW

/***********************************************************************************
**
**	AttachDevToolsWindow
**
***********************************************************************************/

void BrowserDesktopWindow::AttachDevToolsWindow(BOOL attach)
{
	if (attach)
	{
		// Nothing to do it's here already
		if (HasDevToolsWindowAttached())
			return;

		// Try and find the devtools window whereever it is!
		DesktopWindow *devtools_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByType(WINDOW_TYPE_DEVTOOLS);

		// There is a devtools window somewhere
		if (devtools_window)
		{
			// Move the devtools window to this window we are attaching to
			devtools_window->SetParentWorkspace(m_devtools_workspace);

#ifndef PREFS_NO_WRITE
			// save current attached/detached state
			TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::DevToolsIsAttached, TRUE));
#endif // PREFS_NO_WRITE
		}
		else
		{
			// No devtools window and you are trying to show one so create it!
			devtools_window = OP_NEW(DevToolsDesktopWindow, (m_devtools_workspace));
		}

		if (devtools_window)
		{
			// If showing then show it!
			devtools_window->Show(TRUE, NULL, OpWindow::MAXIMIZED);

			// Show the devtools workspace since the listener isn't hooked up yet
			m_devtools_workspace->SetVisibility(TRUE);

			// Listener can only be added after the first window is created
			RETURN_VOID_IF_ERROR(m_devtools_workspace->AddListener(this));
		}
	}
	else
	{
		// Check if this window has devtools window attached
		if (HasDevToolsWindowAttached())
		{
			// Move the window into it's own new window
			// The construction of the new window will take care of removing it from this window
			BrowserDesktopWindow *window;

			if (OpStatus::IsSuccess(BrowserDesktopWindow::Construct(&window, TRUE)))
			{
				if (OpStatus::IsError(window->init_status))
					window->Close(TRUE);
				else
				{
					window->Show(TRUE);
#ifndef PREFS_NO_WRITE
					TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::DevToolsIsAttached, FALSE));
#endif // PREFS_NO_WRITE
				}
			}

			// Compensate for the AddListener above
			m_devtools_workspace->RemoveListener(this);
		}
	}
}

#endif // DEVTOOLS_INTEGRATED_WINDOW

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

static inline void UpdateTransparentArea(OpWidget* tb, OpBar::Alignment alignment, int& top, int& bottom, int& left, int& right, unsigned int width, unsigned int height)
{
	if ((!tb->GetBorderSkin() || !tb->GetBorderSkin()->GetSkinElement() || !tb->GetBorderSkin()->GetSkinElement()->HasTransparentState()) &&
		(!tb->GetBorderSkin() || !tb->GetBorderSkin()->GetSkinElement() || !tb->GetBorderSkin()->GetSkinElement()->HasTransparentState()))
		return;

	OpRect r = tb->GetRect(TRUE);
	switch (alignment)
	{
	case OpBar::ALIGNMENT_TOP:
		top = max(top, r.y+r.height);
		break;
	case OpBar::ALIGNMENT_BOTTOM:
		bottom = max(bottom, (int)height-r.y);
		break;
	case OpBar::ALIGNMENT_LEFT:
		left = max(left, r.x+r.width);
		break;
	case OpBar::ALIGNMENT_RIGHT:
		right = max(right, (int)width-r.x);
		break;
	default:
		break;
	}
}

void BrowserDesktopWindow::OnLayout()
{
	if (!m_hotlist)
		return;

	// should we close or open floating?

	if (m_floating_hotlist_window && m_hotlist->GetAlignment() != OpBar::ALIGNMENT_FLOATING)
	{
		m_floating_hotlist_window->Close(TRUE);
	}
	else if (!m_floating_hotlist_window && m_hotlist->GetAlignment() == OpBar::ALIGNMENT_FLOATING)
	{
		m_hotlist->Remove();
		if (0 != (m_floating_hotlist_window = OP_NEW(HotlistDesktopWindow, (m_workspace, m_hotlist))))
		{
			if (OpStatus::IsSuccess(m_floating_hotlist_window->init_status))
				m_floating_hotlist_window->Show(TRUE);
			else
			{
				m_floating_hotlist_window->Close(TRUE);
				m_floating_hotlist_window = NULL;
			}
		}
	}

	int transparent_top = 0;
	int transparent_bottom = 0;
	int transparent_left = 0;
	int transparent_right = 0;

	m_hotlist->SetVisibility(m_hotlist->GetAlignment() != OpBar::ALIGNMENT_OFF);

	OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::Personalbar, m_top_section, FALSE);
	if(personalbar)
	{
		personalbar->SetVisibility(personalbar->GetAlignment() != OpBar::ALIGNMENT_OFF);
	}
	UINT32 width, height;

	GetInnerSize(width, height);

	OpRect rect(0, 0, width, height);

#ifdef DEVTOOLS_INTEGRATED_WINDOW
	// Set the spiltter rect to the whole window
	m_devtools_splitter->SetRect(rect);

	// If visible setup the rect for the devtools window
	if (m_devtools_workspace->IsVisible())
	{
		// Find where the splitter is currently
		UINT32 div = m_devtools_only ? 0 : m_devtools_splitter->GetDivision();
		UINT32 devToolsMinHeight = DevToolsDesktopWindow::GetMinimumWindowHeight();

		// The division cannot exceed the height
		div = MIN(div, height - devToolsMinHeight);

		// Set the size of the bottom rect for the devtools
		OpRect rect_b(0, 0, width, height - div);
		m_devtools_workspace->SetRect(rect_b);

		// Adjust the height for the top section
		rect.height = div;
	}

	// Doing anything with the top part is a complete waste of time
	// if it's not shown
	if (m_top_section->IsVisible())
	{
		// Set the rect for the main/top section
		m_top_section->SetRect(rect);
#endif // DEVTOOLS_INTEGRATED_WINDOW

		OpBar::Alignment alignment;
#ifdef SUPPORT_MAIN_BAR
		alignment = m_main_bar->GetAlignment();
		m_main_bar_alignment = alignment;
#endif // SUPPORT_MAIN_BAR

		if (m_transparent_skins_enabled_top_only)
			EnableTransparentSkins(m_transparent_skins_enabled, m_transparent_skins_enabled_top_only, TRUE);

		// Layout all bars
#ifdef SUPPORT_MAIN_BAR
		rect = m_main_bar->LayoutToAvailableRect(rect, FALSE);
		UpdateTransparentArea(m_main_bar, m_main_bar->GetResultingAlignment(), transparent_top, transparent_bottom, transparent_left, transparent_right, width, height);
#endif // SUPPORT_MAIN_BAR
		rect = m_status_bar->LayoutToAvailableRect(rect, FALSE);
		UpdateTransparentArea(m_status_bar, m_status_bar->GetResultingAlignment(), transparent_top, transparent_bottom, transparent_left, transparent_right, width, height);

		OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::Personalbar, m_top_section, FALSE);
		if(personalbar)
		{
			rect = personalbar->LayoutToAvailableRect(rect, FALSE);
			UpdateTransparentArea(personalbar, personalbar->GetResultingAlignment(), transparent_top, transparent_bottom, transparent_left, transparent_right, width, height);
		}	
		BOOL pagebarOnTop = FALSE;
		if (m_page_bar->GetResultingAlignment() == OpBar::ALIGNMENT_TOP && rect.y == 0)
			pagebarOnTop = TRUE;
		rect = m_page_bar->LayoutToAvailableRect(rect, FALSE);
		m_page_bar->UpdateIsTopmost(pagebarOnTop);

		UpdateTransparentArea(m_page_bar, m_page_bar->GetResultingAlignment(), transparent_top, transparent_bottom, transparent_left, transparent_right, width, height);

		alignment = m_page_bar->GetResultingAlignment();

		if(alignment == OpBar::ALIGNMENT_BOTTOM || alignment == OpBar::ALIGNMENT_TOP)
		{
			// show the main drag bar but hide the one on the sides
			m_drag_scrollbar->SetAlignment(m_page_bar->GetResultingAlignment());
			m_drag_scrollbar->SetVisibility(TRUE);
			m_side_drag_scrollbar->SetAlignment(OpBar::ALIGNMENT_OFF);
			m_side_drag_scrollbar->SetVisibility(FALSE);
		}
		else
		{
			// show the main drag bar but hide the one on the sides
			m_drag_scrollbar->SetAlignment(OpBar::ALIGNMENT_OFF);
			m_drag_scrollbar->SetVisibility(FALSE);
			m_side_drag_scrollbar->SetAlignment(m_page_bar->GetResultingAlignment());
			m_side_drag_scrollbar->SetVisibility(TRUE);
		}
		rect = m_drag_scrollbar->LayoutToAvailableRect(rect, FALSE);
		UpdateTransparentArea(m_drag_scrollbar, m_page_bar->GetResultingAlignment(), transparent_top, transparent_bottom, transparent_left, transparent_right, width, height);
		rect = m_side_drag_scrollbar->LayoutToAvailableRect(rect, FALSE);

		// place the panels toggle stripe
		BOOL show_toggle_button = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowPanelToggle) && !IsFullscreen() && !KioskManager::GetInstance()->GetNoHotlist();
		BOOL toggle_left = TRUE;

		if (m_hotlist->GetOldVisibleAlignment() == OpBar::ALIGNMENT_LEFT && show_toggle_button)
		{
			toggle_left = !m_hotlist->IsOff();
			m_hotlist_toggle_button->SetRect(OpRect(rect.x, rect.y, 4, rect.height));
			m_hotlist_toggle_button->SetVisibility(TRUE);
			rect.x += 4;
			rect.width -= 4;
		}
		else if (m_hotlist->GetOldVisibleAlignment() == OpBar::ALIGNMENT_RIGHT && show_toggle_button)
		{
			toggle_left = m_hotlist->IsOff();
			m_hotlist_toggle_button->SetRect(OpRect(rect.x + rect.width - 4, rect.y, 4, rect.height));
			m_hotlist_toggle_button->SetVisibility(TRUE);
			rect.width -= 4;
		}
		else
		{
			m_hotlist_toggle_button->SetVisibility(FALSE);
		}

		m_hotlist_toggle_button->GetBorderSkin()->SetType(toggle_left ? SKINTYPE_LEFT : SKINTYPE_RIGHT);

		// Use GetRTL() to compensate for auto-RTLization of OpSplitter: the
		// splitter here is a special case, it needs to depend on
		// GetAlignment() only.
		const bool reverse = (m_hotlist->GetAlignment() == OpBar::ALIGNMENT_RIGHT) ^ (m_splitter->GetRTL() != FALSE);
		m_splitter->SetReversed(reverse);
		m_splitter->SetPixelDivision(true, reverse);

		m_splitter->SetRect(rect);

		m_workspace->GetOpWindow()->SetWorkspaceMinimizedVisibility(!m_page_bar->IsVisible());

#ifdef DEVTOOLS_INTEGRATED_WINDOW
	}
#endif // DEVTOOLS_INTEGRATED_WINDOW
	((DesktopOpWindow*)GetOpWindow())->SetFullTransparency(m_transparent_full_window);

	if (transparent_top != m_transparent_top ||
		transparent_bottom != m_transparent_bottom ||
		transparent_left != m_transparent_left ||
		transparent_right != m_transparent_right)
	{
		m_transparent_top = transparent_top;
		m_transparent_bottom = transparent_bottom;
		m_transparent_left = transparent_left;
		m_transparent_right = transparent_right;

		// if m_transparent_full_window is set, this is just to trigger the extension to the full window
		((DesktopOpWindow*)GetOpWindow())->OnTransparentAreaChanged(m_transparent_top, m_transparent_bottom, m_transparent_left, m_transparent_right);
	}
}


/***********************************************************************************
**
**	ReopenPageL
**
***********************************************************************************/

BOOL BrowserDesktopWindow::ReopenPageL(OpSession* session, INT32 pos, BOOL check_if_available)
{
	if (!session || pos >= (INT32) session->GetWindowCountL())
		return FALSE;

	if (check_if_available)
		return TRUE;

	OpSessionWindow* session_window = session->GetWindowL(session->GetWindowCountL() - 1 - pos);

	OpRect rect;

	rect.x = session_window->GetValueL("x");
	rect.y = session_window->GetValueL("y");
	rect.width = session_window->GetValueL("w");
	rect.height = session_window->GetValueL("h");

	OpWindow::State state = (OpWindow::State) session_window->GetValueL("state");

	DesktopWindow* desktop_window = NULL;

	switch (session_window->GetTypeL())
	{
		case OpSessionWindow::DOCUMENT_WINDOW:
			{
				//create a new document desktop window, but without speed dial
				DocumentDesktopWindow::InitInfo info;
				info.m_no_speeddial = true;
				desktop_window = OP_NEW(DocumentDesktopWindow, (m_workspace, NULL, &info));
			}
			break;
#ifdef M2_SUPPORT
		case OpSessionWindow::MAIL_WINDOW:
			if( g_application->ShowM2() )
			{
				MailDesktopWindow *win;
				if (OpStatus::IsSuccess(MailDesktopWindow::Construct(&win, m_workspace)))
					desktop_window = win;
			}
			break;
		case OpSessionWindow::MAIL_COMPOSE_WINDOW:
			if( g_application->ShowM2() )
			{
				ComposeDesktopWindow *win;
				if (OpStatus::IsSuccess(ComposeDesktopWindow::Construct(&win, m_workspace)))
					desktop_window = win;
			}
			break;
#ifdef IRC_SUPPORT
		case OpSessionWindow::CHAT_WINDOW:
			if( g_application->ShowM2() )
			{
				desktop_window = OP_NEW(ChatDesktopWindow, (m_workspace));
			}
			break;
#endif // IRC_SUPPORT
#endif //M2_SUPPORT
	}

	if (desktop_window)
	{
		if (OpStatus::IsError(desktop_window->init_status))
		{
			desktop_window->Close(TRUE);
		}
		else
		{
			INT32 group_no = session_window->GetValueL("group");
			DesktopGroupModelItem* current_group = m_workspace->GetGroup(group_no);
			DesktopWindowCollection* model = desktop_window->GetModelItem().GetModel();
			
			if (current_group == NULL)
			{
				INT32 position_on_pagebar = session_window->GetValueL("position");
				model->ReorderByPos(desktop_window->GetModelItem(), &GetModelItem(), position_on_pagebar);
			}
			else
			{
				INT32 position_in_group = session_window->GetValueL("group_position");
				model->MergeInto(desktop_window->GetModelItem(), *current_group, current_group->GetActiveDesktopWindow());
				model->ReorderByPos(desktop_window->GetModelItem(), current_group, position_in_group);
			}

			desktop_window->SetSavePlacementOnClose(session_window->GetValueL("saveonclose"));
			desktop_window->OnSessionReadL(session_window);

			desktop_window->Show(TRUE, &rect, state, FALSE, TRUE);
		}
	}

	session->DeleteWindowL(session_window);

	return TRUE;
}

/***********************************************************************************
**
**	OnDesktopWindowAdded
**
***********************************************************************************/

void BrowserDesktopWindow::OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	if (desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		DocumentDesktopWindow* document_window = (DocumentDesktopWindow*) desktop_window;

		document_window->AddDocumentWindowListener(this);
	}

#ifdef DEVTOOLS_INTEGRATED_WINDOW
	if (desktop_window->GetType() == WINDOW_TYPE_DEVTOOLS)
	{
		// Show the devtools workspace
		m_devtools_workspace->SetVisibility(TRUE);
	}
#endif // DEVTOOLS_INTEGRATED_WINDOW
}

/***********************************************************************************
**
**	OnDesktopWindowRemoved
**
***********************************************************************************/

void BrowserDesktopWindow::OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	if (desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		DocumentDesktopWindow* document_window = (DocumentDesktopWindow*) desktop_window;

		document_window->RemoveDocumentWindowListener(this);
	}
#ifdef DEVTOOLS_INTEGRATED_WINDOW
	if (desktop_window->GetType() == WINDOW_TYPE_DEVTOOLS)
	{
		// Hide the devtools
		m_devtools_workspace->SetVisibility(FALSE);

		// if this is a devtools only window and there are no windows in the devtools workspce left we need to
		// close the window
		if (m_devtools_only && m_devtools_workspace->GetItemCount() <= 1)
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_CLOSE_WINDOW, 0, NULL, this);
		}
	}
#endif // DEVTOOLS_INTEGRATED_WINDOW
}

/***********************************************************************************
**
**	OnDesktopWindowChanged
**
***********************************************************************************/

void BrowserDesktopWindow::OnDesktopWindowChanged(OpWorkspace* workspace, DesktopWindow* desktop_window)
{
	if (desktop_window == GetActiveDesktopWindow())
	{
		UpdateWindowTitle();
	}
}

/***********************************************************************************
**
**	OnDesktopWindowClosing
**
***********************************************************************************/

void BrowserDesktopWindow::OnDesktopWindowClosing(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL user_initiated)
{
	// Check if this was the last item in the workspace being closed
	DesktopWindowCollectionItem* item = workspace->GetModelItem();
	if (item)
	{
		UINT32 item_count = workspace->GetItemCount();
		DesktopWindowCollectionItem* first_child = item->GetChildItem();
		if (first_child && first_child->GetType() == OpTypedObject::WINDOW_TYPE_GROUP)
			first_child = first_child->GetChildItem();
		DesktopWindow* first_win = first_child ? first_child->GetDesktopWindow() : NULL;
		if ((item_count == 0 || item_count == 1 && first_win == desktop_window) &&
			desktop_window->GetType() != WINDOW_TYPE_DEVTOOLS)
		{
			INT32 browser_wnd_cnt = g_application->GetDesktopWindowCollection().GetCount(WINDOW_TYPE_BROWSER)
				- g_application->GetDesktopWindowCollection().GetCount(WINDOW_TYPE_DEVTOOLS);

			if( KioskManager::GetInstance()->GetEnabled() )
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_GO_TO_HOMEPAGE);
			}
			else
			{
#if !defined(_MACINTOSH_)
				// Close window if we do not allow empty workspaces and if there is more than
				// one browser window left. Closing the last browser window will exit Opera.
				// Dragging a tab from a window with one tab should close the source window, while
				// closing the last tab in the last browser window should not (espen 2010-01-11).

				if (!g_pcui->GetIntegerPref(PrefsCollectionUI::AllowEmptyWorkspace) && browser_wnd_cnt > 1 )
#endif
				{
					if (item_count > 0)
						OpStatus::Ignore(AddClosedSession());
					g_input_manager->InvokeAction(OpInputAction::ACTION_CLOSE_WINDOW, 0, NULL, this);
				}
			}

#ifndef _MACINTOSH_

			// Maybe force an empty document window to open
			if (user_initiated && !g_pcui->GetIntegerPref(PrefsCollectionUI::AllowEmptyWorkspace) &&  browser_wnd_cnt == 1)
			{
				// Inherit privacy mode from parent BrowserDesktopWindow
				g_application->CreateDocumentDesktopWindow(workspace,0,0,0,TRUE,0,0,0,0,PrivacyMode());
			}
#endif  // _MACINTOSH_
			UpdateWindowTitle(TRUE);
		}
	}

	// Add closing page to undo
	if (IsSessionUndoWindow(desktop_window))
	{
		if (m_undo_session && !desktop_window->PrivacyMode())
		{
			TRAPD(err, desktop_window->AddToSession(m_undo_session, GetID(), FALSE, TRUE));
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what on error ?

			UINT32 window_count;
			TRAP(err, window_count = m_undo_session->GetWindowCountL();
					  if (window_count > 100)
						  m_undo_session->DeleteWindowL((UINT32)0));
		}
	}

	if (desktop_window == m_floating_hotlist_window)
	{
		m_hotlist->SetVisibility(FALSE);
		m_hotlist->Remove();
		m_splitter->AddChild(m_hotlist, FALSE, TRUE);
		m_floating_hotlist_window = NULL;

		if (m_hotlist->GetAlignment() == OpBar::ALIGNMENT_FLOATING && user_initiated)
		{
			m_hotlist->SetAlignment(OpBar::ALIGNMENT_OFF, TRUE);
		}
	}
}

/***********************************************************************************
**
**	OnDesktopWindowActivated
**
***********************************************************************************/

void BrowserDesktopWindow::OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate)
{
	UpdateWindowTitle(TRUE);
}


void BrowserDesktopWindow::OnWorkspaceEmpty(OpWorkspace* workspace,BOOL has_minimized_window)
{
	UpdateWindowTitle(TRUE);
}

/***********************************************************************************
**
**	OnActivate
**
***********************************************************************************/

void BrowserDesktopWindow::OnActivate(BOOL activate, BOOL first_time)
{
	// Force the attention state for the active window to false when returning to Opera.
	if (activate)
	{
		g_application->BroadcastOnBrowserWindowActivated(this);
		if (GetActiveDesktopWindow())
			GetActiveDesktopWindow()->SetAttention(FALSE);
	}
	else
	{
		g_application->BroadcastOnBrowserWindowDeactivated(this);
	}

	// Propagate change to currently active desktop window
	DesktopWindow* active_desktop_window = GetActiveDesktopWindow();
	if (active_desktop_window)
		active_desktop_window->GetModelItem().Change();
}

/***********************************************************************************
**
**	OnFullscreen
**
***********************************************************************************/

void BrowserDesktopWindow::OnFullscreen(BOOL fullscreen)
{
#ifdef SUPPORT_MAIN_BAR
	OpBar::Alignment mainbar_fullscreen_alignment = OpBar::ALIGNMENT_OFF;
#endif // SUPPORT_MAIN_BAR
	OpBar::Alignment pagebar_fullscreen_alignment = OpBar::ALIGNMENT_OFF;

	if( KioskManager::GetInstance()->GetEnabled() )
	{
		if(KioskManager::GetInstance()->GetKioskButtons())
		{
#ifdef SUPPORT_MAIN_BAR
			mainbar_fullscreen_alignment = m_main_bar->GetAlignment() == OpBar::ALIGNMENT_OFF ? OpBar::ALIGNMENT_TOP : m_main_bar->GetAlignment();
#endif // SUPPORT_MAIN_BAR
		}
		if(KioskManager::GetInstance()->GetKioskWindows())
		{
			pagebar_fullscreen_alignment = m_page_bar->GetAlignment() == OpBar::ALIGNMENT_OFF ? OpBar::ALIGNMENT_TOP : m_page_bar->GetAlignment();
			m_drag_scrollbar->SetVisibility(pagebar_fullscreen_alignment != OpBar::ALIGNMENT_OFF);
			m_side_drag_scrollbar->SetVisibility(pagebar_fullscreen_alignment != OpBar::ALIGNMENT_OFF);
		}
	}

#ifdef SUPPORT_MAIN_BAR
	m_main_bar->OnFullscreen(fullscreen, mainbar_fullscreen_alignment);
#endif // SUPPORT_MAIN_BAR

	OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::Personalbar, m_top_section, FALSE);
	if(personalbar)
	{
		personalbar->OnFullscreen(fullscreen);
	}
	m_page_bar->OnFullscreen(fullscreen, pagebar_fullscreen_alignment);
	m_status_bar->OnFullscreen(fullscreen);
	m_hotlist->OnFullscreen(fullscreen);

	//Sync windows layout on returning from fullscreen mode and before restoring, or maximizing, or minimizing operation is performed.
	if (!fullscreen)
	{
		if (m_fullscreen_popup)
			m_fullscreen_popup->CancelDialog();

		SyncLayout();
	}
}

/***********************************************************************************
**
**	OnSessionReadL
**
***********************************************************************************/

void BrowserDesktopWindow::OnSessionReadL(const OpSessionWindow* session_window)
{
	if( KioskManager::GetInstance()->GetEnabled() && !KioskManager::GetInstance()->GetKioskWindows() )
	{
		m_drag_scrollbar->SetVisibility(FALSE);
		m_side_drag_scrollbar->SetVisibility(FALSE);
	}
	if( KioskManager::GetInstance()->GetNoMenu() )
	{
		GetOpWindow()->SetMenuBarVisibility(FALSE);
	}
}


/***********************************************************************************
**
**	OnSessionWriteL
**
***********************************************************************************/

void BrowserDesktopWindow::OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown)
{
	session_window->SetTypeL(
#ifdef DEVTOOLS_INTEGRATED_WINDOW
							IsDevToolsOnly() ? OpSessionWindow::DEVTOOLS_WINDOW :
#endif // DEVTOOLS_INTEGRATED_WINDOW
							OpSessionWindow::BROWSER_WINDOW);

	if (!m_workspace->GetModelItem() || !m_workspace->GetModelItem()->GetModel())
		return;

	// Walk through all children of the workspace, directly using the tree
	// structure to avoid OOMs
	DesktopWindowCollectionItem* parent = m_workspace->GetModelItem();
	DesktopWindowCollection* model = parent->GetModel();
	INT32 start = parent->GetIndex() + 1;

	for (INT32 index = start; index < start + parent->GetSubtreeSize(); index++)
	{
		DesktopWindow* window = model->GetItemByIndex(index)->GetDesktopWindow();
		if (!window || window->PrivacyMode() || window->GetParentWorkspace() != m_workspace)
			continue;

		window->AddToSession(session, GetID(), shutdown);
	}
}

/***********************************************************************************
 **
 **	OnContextMenu
 **
 ***********************************************************************************/

BOOL BrowserDesktopWindow::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	if( widget == m_hotlist_toggle_button )
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Hotlist Popup Menu", PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
		return TRUE;
	}
	return FALSE;
}

/***********************************************************************************
 *
 * AddAllPagesToBookmarks
 *
 * @param id -
 *
 **********************************************************************************/

void BrowserDesktopWindow::AddAllPagesToBookmarks(INT32 parent_id)
{
	BookmarkItemData item_data;
	OpVector<DesktopWindow> windows;
	OpStatus::Ignore(m_workspace->GetDesktopWindows(windows, WINDOW_TYPE_DOCUMENT));

	for (UINT32 i = 0; i < windows.GetCount(); i++)
	{
		DocumentDesktopWindow* dw = static_cast<DocumentDesktopWindow*>(windows.Get(i));
		if(WindowCommanderProxy::SavePageInBookmark(dw->GetWindowCommander(), item_data))
			g_desktop_bookmark_manager->NewBookmark(item_data, parent_id);
	}
}


/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL BrowserDesktopWindow::OnInputAction(OpInputAction* action)
{
	// GENERAL NOTE: Do you really want to add action support here or
	// in DocumentDesktopWindow or OpBrowserView. By adding a new action
	// here it will not work in a detached window [espen 2006-06-27]

	if (!m_hotlist)  // we're being destroyed.. avoid actions
		return FALSE;

	// maybe actions should go to hotlist

	if( !KioskManager::GetInstance()->GetNoHotlist() )
	{
		if (!action->IsMouseInvoked() && m_hotlist->OnInputAction(action))
			return TRUE;
	}

	// maybe actions should go to cycle popup

	if (m_cycle_pages_popup && m_cycle_pages_popup->OnInputAction(action))
		return TRUE;

	DesktopWindow* active_window = GetActiveDesktopWindow();

	OpWindowCommander* wc = NULL;

	if (active_window)
		wc = active_window->GetWindowCommander();

	if( HandleImageActions(action, wc ) )
		return TRUE;
	if( HandleFrameActions(action, wc ) )
		return TRUE;

	if(ToolbarManager::GetInstance()->HandleAction(m_top_section, action))
		return TRUE;

	if (active_window)
	{
		// actions that need active window

		switch (action->GetAction())
		{
			case OpInputAction::ACTION_GET_ACTION_STATE:
			{
				OpInputAction* child_action = action->GetChildAction();

				switch (child_action->GetAction())
				{
					case OpInputAction::ACTION_DELETE:
#ifdef M2_SUPPORT
					case OpInputAction::ACTION_COMPOSE_MAIL:
#endif // M2_SUPPORT
					case OpInputAction::ACTION_EDIT_PROPERTIES:
					{
						// let application handle it
						break;
					}
					case OpInputAction::ACTION_TOGGLE_TAB_THUMBNAILS:
					case OpInputAction::ACTION_INSPECT_ELEMENT:
					{
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
					default:
					{
						if (!action->IsLowlevelAction() && !action->IsMouseInvoked() && active_window->OnInputAction(action))
							return TRUE;
						break;
					}
				}
				break;

			}
		    case OpInputAction::ACTION_OPEN_LINK:
		    case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
		    case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
		    case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
		    case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
			{
				// this action reaching up here means we want to open something that has an ID.
				// For now, this can only be a bookmark..
				BOOL bookmarklinkaction = (	action->GetActionData() &&
											action->GetActionDataString() &&
											uni_strnicmp(action->GetActionDataString(), UNI_L("urlinfo"), 7) != 0);
				// This needs to be revisited if there is going to be still another data type as this is a fallback
				// url's will be picked up in the action handling in display (visdevactions).
				bookmarklinkaction |= (	action->GetActionData() && /* This is for backwards compatiblility */
										!action->GetActionDataString());

				if (bookmarklinkaction)
				{
					Hotlist* hotlist = GetHotlist();
					if (!hotlist)
						return FALSE;

					BookmarksPanel* panel = static_cast<BookmarksPanel*>(hotlist->GetPanelByType(Hotlist::PANEL_TYPE_BOOKMARKS));
					if (!panel)
						return FALSE;

					OpINT32Vector list;
					list.Add(action->GetActionData());

					switch (action->GetAction())
					{
						case OpInputAction::ACTION_OPEN_LINK:
							panel->OpenUrls(list, MAYBE, MAYBE, MAYBE);
							break;
						case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
							panel->OpenUrls(list, NO, YES, NO);
							break;
						case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
							panel->OpenUrls(list, YES, NO, NO);
							break;
						case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
							panel->OpenUrls(list, NO, YES, YES);
							break;
						case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
							panel->OpenUrls(list, YES, NO, YES);
							break;
					}

					return TRUE;
				}
				else
				{
					return FALSE; // Gestures must be allowed to do other things.
				}
			}

			case OpInputAction::ACTION_SELECT_USER_CSS_FILE:
			case OpInputAction::ACTION_DESELECT_USER_CSS_FILE:
			{
				UINT32 index = action->GetActionData();
				BOOL is_active = g_pcfiles->IsLocalCSSActive(index);
				BOOL set_active = action->GetAction() == OpInputAction::ACTION_SELECT_USER_CSS_FILE;

				if (wc == NULL || is_active == set_active)
					return FALSE;

				TRAPD(err, g_pcfiles->WriteLocalCSSActiveL(index, set_active));
				OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what on error ?
				if (err == OpStatus::ERR_OUT_OF_RANGE)
				{/*Do we want to do something?*/}

				WindowCommanderProxy::UpdateWindow(wc);
				return TRUE;
			}
			case OpInputAction::ACTION_SELECT_ALTERNATE_CSS_FILE:
			{
				if (active_window->GetWindowCommander() && action->GetActionDataString())
					active_window->GetWindowCommander()->SetAlternateCss(action->GetActionDataString());
				return TRUE;
			}
			case OpInputAction::ACTION_CREATE_LINKED_WINDOW:
			{
				WindowCommanderProxy::CreateLinkedWindow(wc, action->IsKeyboardInvoked());
				return TRUE;
			}

			case OpInputAction::ACTION_FIND:
			{
#if defined(MSWIN) || defined(_UNIX_DESKTOP_) || defined(_MACINTOSH_)
				BOOL integrated =
					g_pcui->GetIntegerPref(PrefsCollectionUI::UseIntegratedSearch);
				if (integrated)
				{
					DesktopWindow* desktop_window = GetActiveDesktopWindow();
					if (desktop_window)
					{
						int index = -1;
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
						for (unsigned int i = 0; i < g_searchEngineManager->GetSearchEnginesCount(); i++)
						{
							if (g_searchEngineManager->GetSearchEngine(i)->GetSearchType() == SEARCH_TYPE_INPAGE)
							{
								index = i;
								break;
							}
						}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

						OpWidget* widget = NULL;

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
						if (index > -1)
						{
							widget = desktop_window->GetWidgetByTypeAndId(OpTypedObject::WIDGET_TYPE_SEARCH_EDIT,
																		   g_searchEngineManager->GetItemByPosition(index)->GetID());
						}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

						if (widget)
						{
							widget->SetFocus(FOCUS_REASON_OTHER);
						}
						else
						{
/*
pettern 2010-03-08 - Integrated search from the search field is no longer supported after removal of inline search in the search dropdown
							OpSearchDropDown* search_dropdown =  (OpSearchDropDown*) desktop_window->GetWidgetByType(WIDGET_TYPE_SEARCH_DROPDOWN);
							if (search_dropdown)
							{
								if(index != -1)
								{
									search_dropdown->SetSearchFromType(SEARCH_TYPE_INPAGE);
									g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_SEARCH_FIELD);
								}
								else
								{
									integrated = FALSE;
								}
							}
							else
							*/
							{
								integrated = FALSE;
							}
						}
					}
				}

				if (!integrated)
#endif
				{
					m_find_text_manager->ResetOnlyLinksFlag();
					g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_FINDTEXT, 1);
				}
				return TRUE;
			}

			case OpInputAction::ACTION_FIND_INLINE:
			{
				if (action->GetActionData())
				{
					m_find_text_manager->StartTimedInlineFind(action->GetActionData() == 2);
				}
				else
				{
					m_find_text_manager->SearchInline(action->GetActionDataString());
				}
				return TRUE;
			}

			case OpInputAction::ACTION_FIND_NEXT:
			{
				BOOL is_inline_find = (BOOL)action->GetActionData();
				m_find_text_manager->SearchAgain(!(g_op_system_info->GetShiftKeyState() & SHIFTKEY_SHIFT), is_inline_find);
				return TRUE;
			}

			case OpInputAction::ACTION_FIND_PREVIOUS:
			{
				m_find_text_manager->SearchAgain(FALSE, FALSE);
				return TRUE;
			}
			case OpInputAction::ACTION_SHOW_FINDTEXT:
			{
				DocumentDesktopWindow* dw = GetActiveDocumentDesktopWindow();
				if (dw)
					// DocumentWindow is available but focus must have been somewhere else since we got here. So trig the find toolbar in the documentwindow.
					return dw->OnInputAction(action);

				if (action->GetActionData())
					m_find_text_manager->ShowFindTextDialog();
				return TRUE;
			}

			case OpInputAction::ACTION_DELETE:
#ifdef M2_SUPPORT
			case OpInputAction::ACTION_COMPOSE_MAIL:
#endif
			case OpInputAction::ACTION_EDIT_PROPERTIES:
			{
				// let application handle it
				break;
			}
#ifdef NEW_BOOKMARK_MANAGER
 			case OpInputAction::ACTION_MANAGE_BOOKMARKS:
 			{
 				OpenCollectionManager();
 				return TRUE;
 			}
#endif
			default:
			{
				if (!action->IsLowlevelAction() && !action->IsMouseInvoked() && active_window->OnInputAction(action))
					return TRUE;
				break;
			}
		}
	}

	// actions that need no active window

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{

#ifdef ACTION_SET_WINDOW_DECORATION_ENABLED
				case OpInputAction::ACTION_SET_WINDOW_DECORATION:
				{
					DesktopOpWindow* op_window = static_cast<DesktopOpWindow*>(GetOpWindow());
					child_action->SetSelected(op_window->HasWMDecoration());
					return TRUE;
				}
				break;
#endif

#if defined(_UNIX_DESKTOP_)
				// These are never used on unix.
				case OpInputAction::ACTION_SHOW_PRINT_SETUP:
				case OpInputAction::ACTION_SHOW_PRINT_OPTIONS:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}
#endif
#ifdef M2_SUPPORT
				case OpInputAction::ACTION_EMPTY_PAGE_TRASH:
				{
					if (m_undo_session)
					{
						UINT32 window_count = 0;
						TRAPD(err, window_count = m_undo_session->GetWindowCountL());
						child_action->SetEnabled(window_count || m_blocked_popups.GetCount() || g_session_manager->GetClosedSessionCount());
					}
					return TRUE;
				}
#endif
				case OpInputAction::ACTION_REOPEN_PAGE:
				{
					if (m_undo_session)
					{
						int pos = child_action->GetActionData();
						OpSessionWindow *session_window =  m_undo_session->GetWindowL(m_undo_session->GetWindowCountL() - 1 - pos);
						BOOL enabled = (session_window != NULL);
						child_action->SetEnabled(enabled);
					}
					return TRUE;
				}
				case OpInputAction::ACTION_SHOW_POPUP_MENU:
				{
					if (child_action->IsActionDataStringEqualTo(UNI_L("Closed Pages Menu"))
					 || child_action->IsActionDataStringEqualTo(UNI_L("Internal Blocked Popup List")))
					{
						DocumentDesktopWindow* dw = GetActiveDocumentDesktopWindow();

						for (INT32 i = 0; i < (INT32) m_blocked_popups.GetCount(); i++)
						{
							BlockedPopup* popup = m_blocked_popups.Get(i);

							if (popup->IsFrom(dw))
							{
								child_action->SetAttention(TRUE);
								return TRUE;
							}
						}
					}
					return FALSE;
				}
				case OpInputAction::ACTION_UNDO:
				{
					BOOL got_undo = FALSE;
					TRAPD(err, got_undo = ReopenPageL(m_undo_session, 0, TRUE));
					child_action->SetEnabled(got_undo);
					return TRUE;
				}

				case OpInputAction::ACTION_CREATE_LINKED_WINDOW:
				{
					child_action->SetEnabled(wc != NULL);
					return TRUE;
				}

				case OpInputAction::ACTION_SET_AUTOMATIC_RELOAD:
					if (wc == NULL)
						child_action->SetEnabled(FALSE);
					return TRUE;

				case OpInputAction::ACTION_SHOW_PANEL_TOGGLE:
				case OpInputAction::ACTION_HIDE_PANEL_TOGGLE:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SHOW_PANEL_TOGGLE, g_pcui->GetIntegerPref(PrefsCollectionUI::ShowPanelToggle));
					return TRUE;
				}

				case OpInputAction::ACTION_CASCADE:
				case OpInputAction::ACTION_TILE_VERTICALLY:
				case OpInputAction::ACTION_TILE_HORIZONTALLY:
				case OpInputAction::ACTION_CLOSE_ALL:
				case OpInputAction::ACTION_MAXIMIZE_ALL:
				case OpInputAction::ACTION_MINIMIZE_ALL:
				case OpInputAction::ACTION_RESTORE_ALL:
				{
					child_action->SetEnabled(m_workspace->GetItemCount() > 0);
					return TRUE;
				}

				case OpInputAction::ACTION_CLOSE_OTHER:
				{
					child_action->SetEnabled(m_workspace->GetItemCount() > 1);
					return TRUE;
				}

				case OpInputAction::ACTION_FAST_FORWARD:
				{
					child_action->SetEnabled(FALSE);
					child_action->SetActionText(NULL);
					return TRUE;
				}

				case OpInputAction::ACTION_VIEW_DOCUMENT_SOURCE:
				case OpInputAction::ACTION_FIND_NEXT:
				case OpInputAction::ACTION_FIND_PREVIOUS:
				case OpInputAction::ACTION_FIND:
				case OpInputAction::ACTION_PRINT_DOCUMENT:
				case OpInputAction::ACTION_PRINT_PREVIEW:
				case OpInputAction::ACTION_REFRESH_DISPLAY:
					child_action->SetEnabled( WindowCommanderProxy::HasCurrentDoc(wc) );
					return TRUE;

				case OpInputAction::ACTION_REWIND:
				case OpInputAction::ACTION_FORWARD:
				case OpInputAction::ACTION_BACK:
				case OpInputAction::ACTION_STOP:
				case OpInputAction::ACTION_RELOAD:
				case OpInputAction::ACTION_FORCE_RELOAD:
				case OpInputAction::ACTION_RELOAD_FRAME:
				case OpInputAction::ACTION_RELOAD_ALL_PAGES:
				case OpInputAction::ACTION_LOAD_ALL_IMAGES:
				case OpInputAction::ACTION_ADD_TO_BOOKMARKS:
				case OpInputAction::ACTION_ENABLE_HANDHELD_MODE:
				case OpInputAction::ACTION_DISABLE_HANDHELD_MODE:
				case OpInputAction::ACTION_ENABLE_MSR:
				case OpInputAction::ACTION_DISABLE_MSR:
				case OpInputAction::ACTION_SAVE_DOCUMENT:
				case OpInputAction::ACTION_SAVE_DOCUMENT_AS:
				case OpInputAction::ACTION_ENABLE_SCROLL_BARS:
				case OpInputAction::ACTION_DISABLE_SCROLL_BARS:
				case OpInputAction::ACTION_DUPLICATE_PAGE:
				case OpInputAction::ACTION_WAND:
				case OpInputAction::ACTION_GO_TO_SIMILAR_PAGE:
				case OpInputAction::ACTION_SELECT_USER_MODE:
				case OpInputAction::ACTION_SELECT_AUTHOR_MODE:
				case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_AS_SCREEN:
				case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ONE_FRAME_PER_SHEET:
				case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ACTIVE_FRAME:
				case OpInputAction::ACTION_LEAVE_PRINT_PREVIEW:
				case OpInputAction::ACTION_REDO:
				case OpInputAction::ACTION_ENABLE_DISPLAY_IMAGES:
				case OpInputAction::ACTION_DISPLAY_CACHED_IMAGES_ONLY:
				case OpInputAction::ACTION_DISABLE_DISPLAY_IMAGES:
				case OpInputAction::ACTION_SHOW_ZOOM_POPUP_MENU:
				case OpInputAction::ACTION_ZOOM_TO:
				case OpInputAction::ACTION_ZOOM_IN:
				case OpInputAction::ACTION_ZOOM_OUT:
				case OpInputAction::ACTION_ENABLE_AUTOMATIC_RELOAD:
				case OpInputAction::ACTION_DISABLE_AUTOMATIC_RELOAD:
				case OpInputAction::ACTION_GO_TO_LINK_ELEMENT:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}

				case OpInputAction::ACTION_ADD_ALL_TO_BOOKMARKS:
				{
					BOOL enable = FALSE;
					if (g_desktop_bookmark_manager->GetBookmarkModel()->Loaded())
					{
						OpVector<DesktopWindow> windows;
						OpStatus::Ignore(m_workspace->GetDesktopWindows(windows, WINDOW_TYPE_DOCUMENT));

						for (UINT32 i = 0; !enable && i < windows.GetCount(); i++)
						{
							DocumentDesktopWindow* dw = static_cast<DocumentDesktopWindow*>(windows.Get(i));
							enable = WindowCommanderProxy::HasCurrentDoc(dw->GetWindowCommander());
						}
					}
					child_action->SetEnabled(enable);
					return TRUE;
				}

				case OpInputAction::ACTION_ADD_PANEL:
				{
					// DSK-351304: adding a panel creates new bookmark - disable until bookmarks are loaded
					child_action->SetEnabled(g_desktop_bookmark_manager->GetBookmarkModel()->Loaded());
					return TRUE;
				}

				case OpInputAction::ACTION_ENABLE_MENU_BAR:
				{
					child_action->SetSelected(GetOpWindow()->GetMenuBarVisibility());
					return TRUE;
				}

				case OpInputAction::ACTION_DISABLE_MENU_BAR:
				{
					child_action->SetSelected(!GetOpWindow()->GetMenuBarVisibility());
					return TRUE;
				}

				case OpInputAction::ACTION_SELECT_USER_CSS_FILE:
				case OpInputAction::ACTION_DESELECT_USER_CSS_FILE:
				{
					if (wc != NULL)
						WindowCommanderProxy::ToggleUserCSS(wc, child_action);
					return TRUE;
				}
				case OpInputAction::ACTION_SELECT_ALTERNATE_CSS_FILE:
				{
					if (wc != NULL)
						WindowCommanderProxy::SelectAlternateCSSFile(wc, child_action);
					return TRUE;
				}
#ifdef MDE_DEBUG_INFO
				case OpInputAction::ACTION_DEBUG_INVALIDATION:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
#endif // MDE_DEBUG_INFO
			}
			break;
		}

		case OpInputAction::ACTION_SHOW_PANEL_TOGGLE:
		case OpInputAction::ACTION_HIDE_PANEL_TOGGLE:
		{
			if (!PrefsUtils::SetPrefsToggleByAction(action, OpInputAction::ACTION_SHOW_PANEL_TOGGLE, g_pcui, PrefsCollectionUI::ShowPanelToggle))
				return FALSE;

			Relayout();
			return TRUE;
		}

#ifdef M2_SUPPORT
		case OpInputAction::ACTION_EMPTY_PAGE_TRASH:
		{
			OP_DELETE(m_undo_session);
			TRAPD(err, m_undo_session = g_session_manager->CreateSessionInMemoryL());
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what on error ?
			m_blocked_popups.DeleteAll();
			g_session_manager->EmptyClosedSessions();
			return TRUE;
		}
#endif

		case OpInputAction::ACTION_UNDO:
		{
			BOOL result = FALSE;
			TRAPD(err, result = ReopenPageL(m_undo_session, 0, FALSE));
			return result;
		}

		case OpInputAction::ACTION_OPEN_BLOCKED_POPUP:
		{
			INT32 i = action->GetActionData();

			// if index < 0, then index is really -id, so search for id;

			if (i < 0)
			{
				INT32 id = -i;

				for (i = 0; i < (INT32) m_blocked_popups.GetCount(); i++)
				{
					if (m_blocked_popups.Get(i)->GetID() == id)
						break;
				}
			}

			BlockedPopup* popup = m_blocked_popups.Remove(i);

			if (!popup)
				return FALSE;

			popup->Open();
			OP_DELETE(popup);
			return TRUE;
		}

		case OpInputAction::ACTION_REOPEN_PAGE:
		{
			BOOL result = FALSE;
			TRAPD(err, result = ReopenPageL(m_undo_session, action->GetActionData(), FALSE));
			return result;
		}

		case OpInputAction::ACTION_GET_TYPED_OBJECT:
		{
			DesktopWindow* dw = GetActiveNonHotlistDesktopWindow();

			if (dw)
				return dw->OnInputAction(action);

			break;
		}

		case OpInputAction::ACTION_ACTIVATE_HOTLIST_WINDOW:
		{
			Hotlist* hotlist = GetHotlist();
			if (!hotlist)
				return FALSE;

			if (hotlist->GetAlignment() == OpBar::ALIGNMENT_OFF)
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_SET_ALIGNMENT, OpBar::ALIGNMENT_OLD_VISIBLE, UNI_L("hotlist"));
			}
			else if (hotlist->GetAlignment() == OpBar::ALIGNMENT_FLOATING && m_floating_hotlist_window)
			{
				m_floating_hotlist_window->Activate();
			}
			hotlist->RestoreFocus(FOCUS_REASON_ACTION);
			return TRUE;
		}
		case OpInputAction::ACTION_FOCUS_PERSONAL_BAR:
		{
			GetWidgetContainer()->GetRoot()->SetHasFocusRect(TRUE);

			OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::Personalbar, m_top_section, FALSE);
			if(personalbar)
			{
				personalbar->RestoreFocus(FOCUS_REASON_ACTION);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_FOCUS_ADDRESS_FIELD:
		{
			OpAddressDropDown* a_drop_down = GetAddressDropdown();

			if (!a_drop_down)
				return FALSE;

			a_drop_down->SetFocus(FOCUS_REASON_ACTION);
			return TRUE;
		}
		case OpInputAction::ACTION_FOCUS_SEARCH_FIELD:
		{
			OpSearchDropDown* search_dd = GetSearchDropdown();

			if (!search_dd)
				return FALSE;

			search_dd->SetFocus(FOCUS_REASON_ACTION);
			return TRUE;
		}
		
		case OpInputAction::ACTION_ENABLE_MENU_BAR:
		{
			return SetMenuBarVisibility(TRUE);
		}

		case OpInputAction::ACTION_DISABLE_MENU_BAR:
		{
			return SetMenuBarVisibility(FALSE);
		}
		

		case OpInputAction::ACTION_SHOW_MAIN_MENU:
		{
			if (!GetOpWindow()->GetMenuBarVisibility())
			{
				if(m_page_bar)
				{
					m_page_bar->ShowMenuButtonMenu();
				}
				else
				{
					g_application->GetMenuHandler()->ShowPopupMenu("Browser Button Menu Bar", PopupPlacement::AnchorAtCursor(), 0, action->IsKeyboardInvoked());
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_ENTER_FULLSCREEN:
		{
			return SetFullscreenMode(true, false);
		}
		case OpInputAction::ACTION_LEAVE_FULLSCREEN:
		{
			return SetFullscreenMode(false, false);
		}
#ifdef ACTION_TOGGLE_SYSTEM_FULLSCREEN_ENABLED
		case OpInputAction::ACTION_TOGGLE_SYSTEM_FULLSCREEN:
		{
			if (GetOpWindow())
				GetOpWindow()->Fullscreen();
			return TRUE;
		}
#endif // ACTION_TOGGLE_SYSTEM_FULLSCREEN_ENABLED

#ifdef ACTION_SET_WINDOW_DECORATION_ENABLED
		case OpInputAction::ACTION_SET_WINDOW_DECORATION:
		{
			DesktopOpWindow* op_window = static_cast<DesktopOpWindow*>(GetOpWindow());
			op_window->SetShowWMDecoration(!op_window->HasWMDecoration());
			return TRUE;
		}
		break;
#endif
		case OpInputAction::ACTION_ADD_ALL_TO_BOOKMARKS:
		{
			// Action data set in OpBookmarkView
			AddAllPagesToBookmarks(action->GetActionData());
			return TRUE;
		}

		case OpInputAction::ACTION_ADD_PANEL:
		{
			BookmarkItemData item_data;
			if (wc != NULL)
			{
				const URL url = WindowCommanderProxy::GetCurrentURL(wc);
				url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI, item_data.url);

				if (wc->GetCurrentTitle())
					item_data.name.Set(wc->GetCurrentTitle());

				if (item_data.name.IsEmpty())
				{
					item_data.name.Set(item_data.url);
				}
				else
				{
					// Bug #177155 (remove quotes)
					ReplaceEscapes(item_data.name.CStr(), item_data.name.Length(), TRUE);
				}
			}

			item_data.name.Strip();
			item_data.panel_position = 0; // add to panel

			g_desktop_bookmark_manager->NewBookmark(item_data, -1, this, TRUE);
			return TRUE;
		}

		case OpInputAction::ACTION_CASCADE:
		{
			m_workspace->CascadeAll();
			return TRUE;
		}
		case OpInputAction::ACTION_TILE_VERTICALLY:
		{
			m_workspace->TileAll(TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_TILE_HORIZONTALLY:
		{
			m_workspace->TileAll(FALSE);
			return TRUE;
		}
#ifndef _MACINTOSH_
		case OpInputAction::ACTION_CLOSE_ALL:
		{
			if(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCloseAllDialog))
			{
				SimpleDialogController* controller = OP_NEW(CloseTabsDialogController, (this, true));
				if (controller)
				{
					controller->ConnectDoNotShowAgain(*g_pcui, PrefsCollectionUI::ShowCloseAllDialog);
					OpStatus::Ignore(ShowDialog(controller, g_global_ui_context, this));
				}
			}
			else
			{
				CloseAllTabs();
			}
			return TRUE;
		}
#endif
		case OpInputAction::ACTION_CLOSE_OTHER:
		{
			if(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowCloseAllButActiveDialog))
			{
				SimpleDialogController* controller = OP_NEW(CloseTabsDialogController, (this, false));
				if (controller)
				{
					controller->ConnectDoNotShowAgain(*g_pcui, PrefsCollectionUI::ShowCloseAllButActiveDialog);
					OpStatus::Ignore(ShowDialog(controller, g_global_ui_context, this));
				}
			}
			else
			{
				CloseAllTabsExceptActive();
			}
			return TRUE;
		}
		case OpInputAction::ACTION_MAXIMIZE_ALL:
		{
	        m_workspace->MaximizeAll();
			return TRUE;
		}
		case OpInputAction::ACTION_MINIMIZE_ALL:
		{
	        m_workspace->MinimizeAll();
			return TRUE;
		}
		case OpInputAction::ACTION_RESTORE_ALL:
		{
	        m_workspace->RestoreAll();
			return TRUE;
		}
		case OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE:
		case OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE:
		{
			CyclePages(action);
			return TRUE;
		}
		case OpInputAction::ACTION_SWITCH_TO_NEXT_PAGE:
		{
			m_page_bar->ActivateNext(TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_SWITCH_TO_PREVIOUS_PAGE:
		{
			m_page_bar->ActivateNext(FALSE);
			return TRUE;
		}
		case OpInputAction::ACTION_RELOAD_ALL_PAGES:
		{
			//probably shouldn't be windowManager. FIXME!!
			windowManager->ReloadAll();
			return TRUE;
		}
#ifdef SUPPORT_VISUAL_ADBLOCK
		case OpInputAction::ACTION_CONTENTBLOCK_DETAILS:
		{
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
			OpVector<ContentFilterItem> content_to_block;
#else
			ContentBlockTreeModel content_to_block;
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
			BrowserDesktopWindow::OpenContentBlockDetails(this, action, content_to_block);
			return TRUE;
		}
		break;
#endif // SUPPORT_VISUAL_ADBLOCK
		case OpInputAction::ACTION_ENABLE_TAB_THUMBNAILS:
		case OpInputAction::ACTION_TOGGLE_TAB_THUMBNAILS:
		{
			Relayout();
			return TRUE;
		}

#ifdef MDE_DEBUG_INFO
		case OpInputAction::ACTION_DEBUG_INVALIDATION:
		{
			MDE_Screen *screen = static_cast<MDE_OpWindow*>(GetOpWindow())->GetMDEWidget()->m_screen;
			screen->SetDebugInfo(screen->m_debug_flags ^ MDE_DEBUG_INVALIDATE_RECT);
			return TRUE;
		}
#endif // MDE_DEBUG_INFO

	}

	if (DesktopWindow::OnInputAction(action))
		return TRUE;

	return WindowCommanderProxy::HandlePlatformAction(wc, action);
}

bool BrowserDesktopWindow::SetFullscreenMode(bool go_fullscreen, bool is_scripted)
{
	if (go_fullscreen == !!IsFullscreen())
		return false;

	LockUpdate(TRUE);

	// First set the fullscreen mode of the BrowserDesktopWindow
	if (!Fullscreen(go_fullscreen))
	{
		LockUpdate(FALSE);
		return FALSE;
	}

	m_fullscreen_is_scripted = is_scripted;
	DesktopWindow* dw = GetActiveDesktopWindow();

	if (go_fullscreen && dw)
	{
		dw->Activate(FALSE);

		// Can only activate one window here. Takes easily too long time otherwise
		if (dw->Fullscreen(TRUE))
		{
			// Tell core about the change
			if (dw->GetWindowCommander())
				dw->GetWindowCommander()->SetDocumentFullscreenState(m_fullscreen_is_scripted ? OpWindowCommander::FULLSCREEN_NORMAL : OpWindowCommander::FULLSCREEN_PROJECTION);

			// Show fullscreen popup notification
			if(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullscreenExitInformation))
			{
				m_fullscreen_popup = OP_NEW(FullscreenPopupController, (GetWidgetContainer()->GetRoot()));
				if (m_fullscreen_popup)
				{
					m_fullscreen_popup->SetListener(this);
					m_fullscreen_popup->Show();
				}
			}
		}

		dw->Activate();
	}
	else if (!go_fullscreen)
	{
#if defined(_UNIX_DESKTOP_)
		// Prevents bug #182714. The DesktopWindow::Fullscreen(FALSE)
		// will restore old window mode, and on UNIX we will when restoring
		// or maximizing a window always activate it unless it happens in the
		// background. I am not sure whether OpWindow::SetOpenInBackground()
		// have the same effect on other platforms [espen 2006-05-09]
		m_workspace->SetOpenInBackground(TRUE);
#else
		if (dw)
			dw->Activate(FALSE);
#endif

		// For any DesktopWindow in fullscreen, take it out of fullscreen
		OpVector<DesktopWindow> windows;
		OpStatus::Ignore(m_workspace->GetDesktopWindows(windows));
		for (UINT32 i = 0; i < windows.GetCount(); i++)
		{
			if (windows.Get(i)->IsFullscreen() && windows.Get(i)->Fullscreen(FALSE))
			{
				// Tell core about the change
				if (windows.Get(i)->GetWindowCommander())
					windows.Get(i)->GetWindowCommander()->SetDocumentFullscreenState(OpWindowCommander::FULLSCREEN_NONE);
			}
		}

#if defined(_UNIX_DESKTOP_)
		m_workspace->SetOpenInBackground(FALSE);
#else
		if (dw)
			dw->Activate();
#endif
	}

	LockUpdate(FALSE);

	return TRUE;
}

void BrowserDesktopWindow::OnDialogClosing(DialogContext* context)
{
	if (context == m_fullscreen_popup)
		m_fullscreen_popup = NULL;
}

OP_STATUS BrowserDesktopWindow::InitHiddenInternals()
{
	if (!m_hotlist->IsInitialized())
		RETURN_IF_ERROR(m_hotlist->Init());
	m_has_hidden_internals = false;
	return OpStatus::OK;
}

BOOL BrowserDesktopWindow::SetMenuBarVisibility(BOOL visible)
{
#ifdef _MACINTOSH_
	// Hiding the menu bar is not possible on Mac and if run will cause a freeze with the PrefChanged function
	if (!visible)
		return FALSE;
#endif // _MACINTOSH_

	if (GetOpWindow()->GetMenuBarVisibility() == visible)
		return FALSE;

	// Keep all windows in sync
	OpVector<DesktopWindow> list;
	if (OpStatus::IsSuccess(g_application->GetDesktopWindowCollection().GetDesktopWindows(WINDOW_TYPE_BROWSER, list)))
	{
		for (UINT32 i = 0; i < list.GetCount(); i++)
		{
			BrowserDesktopWindow* bw = static_cast<BrowserDesktopWindow*>(list.Get(i));
			bw->GetOpWindow()->SetMenuBarVisibility(visible);
			if (bw->m_page_bar)
				bw->m_page_bar->EnsureMenuButton(!visible);
		}
	}

	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowMenu, visible));
	return TRUE;
}

BOOL BrowserDesktopWindow::HandleImageActions(OpInputAction* action, OpWindowCommander* wc )
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
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
				case OpInputAction::ACTION_SAVE_IMAGE:
				case OpInputAction::ACTION_COPY_IMAGE:
				{
					BOOL enabled = FALSE;
					if (wc && child_action->GetActionData())
					{
						DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(child_action->GetActionData());
						enabled = dmctx->GetDocumentContext() ? dmctx->GetDocumentContext()->HasImage() && dmctx->GetDocumentContext()->HasCachedImageData() : FALSE;
					}
					child_action->SetEnabled( enabled );
					return TRUE;
				}
				case OpInputAction::ACTION_FOLLOW_IMAGE_DESCRIPTION_URL:
				{
					BOOL enabled = FALSE;
					if (wc && child_action->GetActionData())
					{
						DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(child_action->GetActionData());
						OpString longdesc;
						if (dmctx->GetDocumentContext())
							dmctx->GetDocumentContext()->GetImageLongDesc(&longdesc);
						enabled = longdesc.HasContent() ? TRUE : FALSE;
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
		{
			if (wc && action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext())
					wc->LoadImage(*dmctx->GetDocumentContext());
			}
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_BACKGROUND_IMAGE:
		{
			WindowCommanderProxy::ShowBackgroundImage(wc);
			return TRUE;
		}
		case OpInputAction::ACTION_SAVE_IMAGE:
		{
			return TRUE;
		}
		case OpInputAction::ACTION_SAVE_BACKGROUND_IMAGE:
		{
			return TRUE;
		}
		case OpInputAction::ACTION_COPY_IMAGE_ADDRESS:
		{
			if (action->GetActionData())
			{
				DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (dmctx->GetDocumentContext() && dmctx->GetDocumentContext()->HasImage())
				{
					OpString image_address;
					dmctx->GetDocumentContext()->GetImageAddress(&image_address);
					const uni_char *p = image_address.CStr();
					if (p)
					{
						// Clipboard
						g_desktop_clipboard_manager->PlaceText(p, dmctx->GetDocumentContext()->GetImageAddressContext());
#if defined(_X11_SELECTION_POLICY_)
						// Mouse selection
						g_desktop_clipboard_manager->PlaceText(p, dmctx->GetDocumentContext()->GetImageAddressContext(), true);
#endif
					}
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_COPY_BACKGROUND_IMAGE_ADDRESS:
		{
			if (action->GetActionData())
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
						g_desktop_clipboard_manager->PlaceText(p, dmctx->GetDocumentContext()->GetBGImageAddressContext());
#if defined(_X11_SELECTION_POLICY_)
						// Mouse selection
						g_desktop_clipboard_manager->PlaceText(p, dmctx->GetDocumentContext()->GetBGImageAddressContext(), true);
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

		case OpInputAction::ACTION_FOLLOW_IMAGE_DESCRIPTION_URL:
			{
				if (action->GetActionData())
				{
					DesktopMenuContext* dmctx = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
					OpString longdesc;
					if (dmctx->GetDocumentContext())
						dmctx->GetDocumentContext()->GetImageLongDesc(&longdesc);
					if (longdesc.HasContent())
					{
						OpString longdesc_url;
						longdesc_url.Set(longdesc.CStr());

						URL url = WindowCommanderProxy::GetCurrentURL(wc);
						URL longdesc_URL = g_url_api->GetURL(url, longdesc.CStr());
						longdesc_URL.GetAttribute(URL::KUniName_With_Fragment, longdesc_url, TRUE);

						g_application->GoToPage(longdesc_url,TRUE);
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
	}

	return FALSE;
}

BOOL BrowserDesktopWindow::HandleFrameActions(OpInputAction* action, OpWindowCommander* wc )
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
				g_desktop_clipboard_manager->PlaceText(url_string.CStr(), url.GetContextId());
#if defined(_X11_SELECTION_POLICY_)
				g_desktop_clipboard_manager->PlaceText(url_string.CStr(), url.GetContextId(), true);
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

				INT32 id = GetSelectedHotlistItemId(PANEL_TYPE_BOOKMARKS);

				g_desktop_bookmark_manager->NewBookmark(item_data, id, g_application->GetActiveBrowserDesktopWindow(), TRUE);
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
				g_application->OpenURL(url_string.CStr(), new_window, new_page, in_background );
			}
		}
		return TRUE;
	}

	return FALSE;
}

/***********************************************************************************
**
**	CyclePages
**
***********************************************************************************/
void BrowserDesktopWindow::CyclePages(OpInputAction* action)
{
	if (m_workspace->GetDesktopWindowCount() <= 1)
		return;

	WindowCycleType type = (WindowCycleType)
		g_pcui->GetIntegerPref(PrefsCollectionUI::WindowCycleType);

	switch (type)
	{
		case WINDOW_CYCLE_TYPE_MDI:
		{
			if(g_pcui->GetIntegerPref(PrefsCollectionUI::AlternativePageCycleMode) == 1)
			{
				// This is the behavior for all Unix versions < 7 [espen 2003-03-15]
				m_page_bar->ActivateNext(action->GetAction() == OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE);
			}
			else
			{
				if (action->GetAction() == OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE)
				{
					m_workspace->GetDesktopWindowFromStack(0)->Lower();
				}
				else
				{
					m_workspace->GetDesktopWindowFromStack(m_workspace->GetDesktopWindowCount() - 1)->Activate();
				}
			}
			break;
		}

		case WINDOW_CYCLE_TYPE_STACK:
		case WINDOW_CYCLE_TYPE_LIST:
		{
			// action lands here only when cycler popup is uninitialized
			OP_ASSERT(!m_cycle_pages_popup);
			if (!m_cycle_pages_popup)
			{
				m_cycle_pages_popup = OP_NEW(CyclePagesPopupWindow, (this));
				if(!m_cycle_pages_popup)
					break;

				if (OpStatus::IsSuccess(m_cycle_pages_popup->init_status) && 
					OpStatus::IsSuccess(m_cycle_pages_popup->Init(type)))
				{
					m_cycle_pages_popup->Show(TRUE);
					OP_ASSERT(m_cycle_pages_popup && "we expect cycler popup not to be modal");
					m_cycle_pages_popup->OnInputAction(action);
				}
				else
				{
					m_cycle_pages_popup->Close(TRUE);
					m_cycle_pages_popup = NULL;
				}
			}
			break;
		}
	}
}


INT32 BrowserDesktopWindow::GetSelectedHotlistItemId( OpTypedObject::Type type )
{
	INT32 id = -1;

	Hotlist* hotlist = GetHotlist();
	if (!hotlist)
		return id;

	HotlistPanel* panel = hotlist->GetPanelByType(type);
	if (panel)
	{
		if( panel->GetType() == PANEL_TYPE_BOOKMARKS )
		{
			BookmarksPanel* p = (BookmarksPanel*)panel;
			id = p->GetSelectedFolderID();
		}
		else if( panel->GetType() == PANEL_TYPE_CONTACTS )
		{
			ContactsPanel* p = (ContactsPanel*)panel;
			id = p->GetSelectedFolderID();
		}
		else if (panel->GetType() == PANEL_TYPE_NOTES )
		{
			NotesPanel* p = (NotesPanel*)panel;
			id = p->GetSelectedFolderID();
		}
	}
	return id;
}


void BrowserDesktopWindow::SetFocus(FOCUS_REASON reason)
{
	if (m_hotlist->IsVisible())
	{
		m_hotlist->RestoreFocus(reason);
	}
	else
	{
		DesktopWindow::SetFocus(reason);
	}
}

void BrowserDesktopWindow::OnTransparentAreaChanged(DocumentDesktopWindow* document_window, int height)
{
	// Get the position of the splitter and add the height of the toolbar
	OpRect r = m_splitter->GetRect(TRUE);
	int transparent_top = r.y + height;

	if (transparent_top != m_transparent_top)
	{
		m_transparent_top = transparent_top;
		// Add the height and send the OnTransparentAreaChanged call
		((DesktopOpWindow*)GetOpWindow())->OnTransparentAreaChanged(m_transparent_top, m_transparent_bottom, m_transparent_left, m_transparent_right);
	}
}

OpAddressDropDown* BrowserDesktopWindow::GetAddressDropdown()
{
	return (OpAddressDropDown*) GetWidgetByType(WIDGET_TYPE_ADDRESS_DROPDOWN);
}

OpSearchDropDown* BrowserDesktopWindow::GetSearchDropdown()
{
	return (OpSearchDropDown*) GetWidgetByType(WIDGET_TYPE_SEARCH_DROPDOWN);
}


void BrowserDesktopWindow::OnPopup(DocumentDesktopWindow* document_window, const uni_char *url, const uni_char *name, int left, int top, int width, int height, BOOL3 scrollbars, BOOL3 location)
{
	OpDocumentListener::JSPopupOpener*	opener = NULL;

	document_window->GetBrowserView()->ContinuePopup(OpDocumentListener::JSPopupCallback::POPUP_ACTION_DEFAULT, &opener);

	if (opener)
	{
		// something was blocked

		OpString title;

		title.Set(document_window->GetTitle(TRUE));

#ifndef _MACINTOSH_
		if (title.Length() > 20)
		{
			title.Delete(20);
			title.Append(UNI_L("..."));
		}
#endif  // _MACINTOSH_

		BlockedPopup* blocked_popup = OP_NEW(BlockedPopup, (document_window, title.CStr(), url, opener));

		if (blocked_popup)
			m_blocked_popups.Insert(0, blocked_popup);

		if (m_blocked_popups.GetCount() > 10)
		{
			m_blocked_popups.Delete(m_blocked_popups.GetCount() - 1);
		}

		OpString text, text_append;
#ifdef _MACINTOSH_
		text.Append(title.CStr());
#else
		g_languageManager->GetString(Str::S_POPUP_BLOCKED_FROM, text);
		text.Append(title.CStr());
		g_languageManager->GetString(Str::S_POPUP_BLOCKED_CLICK_TO_OPEN_IT, text_append);
#endif
		text.Append(text_append.CStr());

		if (IsActive() && g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNotificationForBlockedPopups))
		{
			OpInputAction* open_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_BLOCKED_POPUP));
			if (!open_action)
				return;
			open_action->SetActionData(-blocked_popup->GetID());

			g_notification_manager->ShowNotification(DesktopNotifier::BLOCKED_POPUP_NOTIFICATION, 
					text, "Blocked", open_action);
		}

		g_input_manager->UpdateAllInputStates();
	}
}


#ifdef SUPPORT_VISUAL_ADBLOCK

#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
void BrowserDesktopWindow::OpenContentBlockDetails(DesktopWindow *parent, OpInputAction* action, OpVector<ContentFilterItem>& content_to_block)
#else
void BrowserDesktopWindow::OpenContentBlockDetails(DesktopWindow *parent, OpInputAction* action, ContentBlockTreeModel& content_to_block)
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
{
	INT32 started_from_menu = action->GetActionData();
#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
	OpVector<ContentFilterItem> pattern_content;
#else
	SimpleTreeModel pattern_content;
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
	ContentBlockDialog *dlg = OP_NEW(ContentBlockDialog, (started_from_menu ? TRUE : FALSE));
	if (!dlg)
		return;

	ContentBlockFilterCreation filters(&content_to_block, URL());

	if(OpStatus::IsSuccess(filters.GetCreatedFilterURLs(pattern_content)))
	{
		content_to_block.DeleteAll();

#ifdef CF_CAP_REMOVED_SIMPLETREEMODEL
		UINT32 cnt;

		for(cnt = 0; cnt < pattern_content.GetCount(); cnt++)
		{
			ContentFilterItem *item = pattern_content.Get(cnt);

			content_to_block.Add(item);
		}
		dlg->Init(parent, &content_to_block);
#else
		INT32 cnt;

		for(cnt = 0; cnt < pattern_content.GetCount(); cnt++)
		{
			const uni_char *pattern = pattern_content.GetItemString(cnt);
			SimpleTreeModelItem *item = pattern_content.GetItemByIndex(cnt);
			if(item)
			{
				content_to_block.AddItem(pattern, NULL, 0, -1, item->GetUserData());
			}
		}
		dlg->Init(parent, &content_to_block);
#endif // CF_CAP_REMOVED_SIMPLETREEMODEL
	}
}

#endif // SUPPORT_VISUAL_ADBLOCK

void BrowserDesktopWindow::EnableTransparentSkins(BOOL enabled, BOOL top_only, BOOL force_update)
{
	if (enabled == m_transparent_skins_enabled && top_only == m_transparent_skins_enabled_top_only && !force_update)
		return;

#ifdef SUPPORT_MAIN_BAR
	// m_main_bar is just a OpToolbar so we need to handle the skin here
	if (enabled && m_transparent_skin_sections_available && (!top_only || m_main_bar->GetResultingAlignment() == OpBar::ALIGNMENT_TOP))
		m_main_bar->GetBorderSkin()->SetImage("Mainbar Transparent Skin", "Mainbar Skin");
	else
		m_main_bar->GetBorderSkin()->SetImage("Mainbar Skin");
#endif // SUPPORT_MAIN_BAR

	m_status_bar->EnableTransparentSkin(enabled && m_transparent_skin_sections_available && (!top_only || m_status_bar->GetResultingAlignment() == OpBar::ALIGNMENT_TOP));
	OpToolbar *personalbar = ToolbarManager::GetInstance()->FindToolbar(ToolbarManager::Personalbar, m_top_section, FALSE);
	if(personalbar)
	{
		personalbar->EnableTransparentSkin(enabled && m_transparent_skin_sections_available && (!top_only || personalbar->GetResultingAlignment() == OpBar::ALIGNMENT_TOP));
	}
	m_page_bar->EnableTransparentSkin(enabled && m_transparent_skin_sections_available && (!top_only || m_page_bar->GetResultingAlignment() == OpBar::ALIGNMENT_TOP));

	m_drag_scrollbar->EnableTransparentSkin(enabled && m_transparent_skin_sections_available);
	if (enabled && m_transparent_skin_sections_available)
	{
		m_drag_scrollbar->GetBorderSkin()->SetImage("Drag Scrollbar Transparent Skin", "Toolbar Transparent Skin");
	}
	else
	{
		m_drag_scrollbar->GetBorderSkin()->SetImage("Drag Scrollbar Skin", "Toolbar Skin");
	}

	m_side_drag_scrollbar->EnableTransparentSkin(enabled && m_transparent_skin_sections_available);
	m_workspace->EnableTransparentSkins(enabled && m_transparent_skin_sections_available);
	m_transparent_skins_enabled = enabled;
	m_transparent_skins_enabled_top_only = top_only;
}

void BrowserDesktopWindow::RemoveBlockedPopup(DocumentDesktopWindow *source)
{
	for (UINT32 i = 0; i < m_blocked_popups.GetCount(); i++)
	{
		BlockedPopup* popup = m_blocked_popups.Get(i);
		if (popup && popup->IsFrom(source))
		{
			m_blocked_popups.Remove(i);
			OP_DELETE(popup);
			i--; //Move the index to one back as Remove() adjusts element order after its been removed.
		}
	}
}

OP_STATUS BrowserDesktopWindow::AddClosedSession()
{
	OpAutoPtr<OpSession> session;
	RETURN_IF_LEAVE(session.reset(g_session_manager->CreateSessionInMemoryL()));
	RETURN_IF_ERROR(AddToSession(session.get(), GetID(), FALSE, TRUE));
	g_session_manager->GetSessionLoadContext()->AddClosedSession(session.release());
	return OpStatus::OK;
}

BOOL BrowserDesktopWindow::IsSessionUndoWindow(DesktopWindow* desktop_window)
{
	if (!desktop_window)
		return FALSE;
	
	bool is_session_wnd = false;

	OpTypedObject::Type type = desktop_window->GetType();
	
	if (type == WINDOW_TYPE_DOCUMENT)
	{
		OpBrowserView *doc_view = static_cast<DocumentDesktopWindow*>(desktop_window)->GetBrowserView();

		// Need to check for NULL here as the window might be closed due to OOM and thus not fully initialised
		if (doc_view)
		{
			OpWindowCommander* commander = doc_view->GetWindowCommander();

			int min, max;
			commander->GetHistoryRange(&min, &max);
			int history_count = max - min + 1;
			if (history_count > 0)
				is_session_wnd = true;

			if (history_count == 1 && commander)
			{
				const uni_char* url_str = commander->GetCurrentURL(FALSE);
				if (!DocumentView::IsUrlRestrictedForViewFlag(url_str, DocumentView::ALLOW_ADD_TO_TRASH_BIN))
					is_session_wnd = false;
			}
		}
	}

	if (is_session_wnd ||
		type == WINDOW_TYPE_CHAT    ||
		type == WINDOW_TYPE_CHAT_ROOM    ||
		type == WINDOW_TYPE_MAIL_VIEW    ||
		type == WINDOW_TYPE_MAIL_COMPOSE)
	{
		is_session_wnd = true;
	}

	return is_session_wnd;
}


void BrowserDesktopWindow::EmptyClosedTabsAndPopups()
{
	OP_DELETE(m_undo_session);
	TRAPD(err, m_undo_session = g_session_manager->CreateSessionInMemoryL());
	m_blocked_popups.DeleteAll();
}

#ifdef NEW_BOOKMARK_MANAGER
void BrowserDesktopWindow::OpenCollectionManager()
{
	DesktopWindow* ddw_last_found = NULL;
	OpVector<DesktopWindow> windows;
	if (OpStatus::IsSuccess(GetWorkspace()->GetDesktopWindows(windows, WINDOW_TYPE_DOCUMENT)))
	{
		UINT32 count = windows.GetCount();
		for (UINT32 i = 0; i < count; i++)
		{
			DocumentView* doc_view = static_cast<DocumentDesktopWindow*>(windows.Get(i))->GetActiveDocumentView();
			if (doc_view && doc_view->GetDocumentType() == DocumentView::DOCUMENT_TYPE_COLLECTION)
				ddw_last_found = windows.Get(i);
		}
	}

	if (ddw_last_found)
		ddw_last_found->Activate(TRUE);
	else
		g_application->GoToPage(UNI_L("opera:collectionview"), TRUE, FALSE, FALSE, NULL, -1, TRUE);
}
#endif

void BrowserDesktopWindow::CloseAllTabsExceptActive()
{
	LockUpdate(TRUE);

	OpVector<DesktopWindow> close_windows;
	OpStatus::Ignore(m_workspace->GetDesktopWindows(close_windows));

	for(UINT32 i = 0; i < close_windows.GetCount(); i++)
	{
		if(close_windows.Get(i) != m_workspace->GetActiveDesktopWindow())
			close_windows.Get(i)->Close(TRUE, TRUE, FALSE);
	}

	LockUpdate(FALSE);
}


/***********************************************************************************
**
**	CyclePagesPopupWindow
**
***********************************************************************************/
CyclePagesPopupWindow::CyclePagesPopupWindow(BrowserDesktopWindow* browser_window) :
	m_pages_list(NULL)
	,m_browser_window(browser_window)
	,m_has_activated_window(FALSE)
{
}


CyclePagesPopupWindow::~CyclePagesPopupWindow()
{
	m_browser_window->m_cycle_pages_popup = NULL;
}


OP_STATUS CyclePagesPopupWindow::Init(WindowCycleType type)
{
	OpRect image_rect;
	INT32 num_columns = 1;

	OP_STATUS status = DesktopWindow::Init(OpWindow::STYLE_POPUP, m_browser_window->GetOpWindow(), OpWindow::EFFECT_TRANSPARENT);
    RETURN_IF_ERROR(status);

	WidgetContainer* widget_container = GetWidgetContainer();
	OpWidget* root_widget = widget_container->GetRoot();

	status = OpToolbar::Construct(&m_pages_list);
	RETURN_IF_ERROR(status);

	widget_container->SetEraseBg(TRUE);

	root_widget->SetHasCssBorder(FALSE);

	m_pages_list->SetDeselectable(TRUE);
	m_pages_list->SetAlignment(OpBar::ALIGNMENT_LEFT);
	m_pages_list->SetWrapping(OpBar::WRAPPING_OFF);
	m_pages_list->SetSelector(TRUE);
	m_pages_list->SetFixedHeight(OpToolbar::FIXED_HEIGHT_BUTTON);
	m_pages_list->SetButtonType(OpButton::TYPE_TOOLBAR);
	m_pages_list->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	m_pages_list->GetBorderSkin()->SetImage("Cycler Pages Skin");
	m_pages_list->SetSkinned(TRUE);

	root_widget->AddChild(m_pages_list);

	if (type == WINDOW_CYCLE_TYPE_STACK)
	{
		OpWorkspace* workspace = m_browser_window->m_workspace;
		for (INT32 i = 0; i < workspace->GetDesktopWindowCount(); ++i)
		{
			AddWindow(workspace->GetDesktopWindowFromStack(i));
		}
	}
	else
	{
		OpVector<DesktopWindow> windows;
		RETURN_IF_ERROR(m_browser_window->GetWorkspace()->GetDesktopWindows(windows));
		for (UINT32 i = 0; i < windows.GetCount(); ++i)
		{
			AddWindow(windows.Get(i));
		}
	}

	INT32 list_width, list_height;
	INT32 width = 0, height = 0;
	OpRect window_rect;
	OpRect screen_rect;

	m_browser_window->GetScreenRect(screen_rect, TRUE);
	m_browser_window->GetRect(window_rect);
	m_pages_list->GetRequiredSize(list_width, list_height);

	if (!SupportsExternalCompositing())
	{
		// Tab cycler window is a transparent window which can not be shown outside 
		// opera if compositing is not supported. Do the best out of it (bug DSK-297393)

		if (m_browser_window->GetOpWindow()->GetMenuBarVisibility())
		{
			window_rect.y += 20;
			window_rect.height -= 20;
		}
		screen_rect = window_rect;
	}

	INT32 max_list_height = ((screen_rect.height * 3) / 4);

	if( list_height > max_list_height)
	{
		// We have to use multiple columns

		m_pages_list->SetWrapping(OpBar::WRAPPING_NEWLINE);
		m_pages_list->SetFixedMaxWidth(150); // Maybe

		m_pages_list->GetRequiredSize(screen_rect.width, max_list_height, list_width, list_height);

		num_columns = m_pages_list->GetComputedColumnCount();

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

#ifdef SUPPORT_GENERATE_THUMBNAILS
	INT32 image_width = 0;
	INT32 image_height = 0;
	INT32 padding_left, padding_top, padding_right, padding_bottom;

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInWindowCycle))
	{
		status = OpButton::Construct(&m_image_button);
		RETURN_IF_ERROR(status);

		m_image_button->SetDead(TRUE);
		m_image_button->SetButtonType(OpButton::TYPE_CUSTOM);
		m_image_button->SetButtonStyle(OpButton::STYLE_IMAGE);
		m_image_button->GetBorderSkin()->SetImage("Cycler Button Skin");
		m_image_button->SetSkinned(TRUE);
		m_image_button->SetRestrictImageSize(FALSE);

		m_image_button->GetBorderSkin()->GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom);

		SetImages();

		m_image_button->GetRequiredSize(image_width, image_height);

		image_rect.width = max(image_width, THUMBNAIL_WIDTH) + 2;
		image_rect.height = max(image_height, THUMBNAIL_HEIGHT) + 2;
		image_rect.x = list_width;
		image_rect.y = list_height > image_height ? (list_height - image_height) / 2 : 0;

		image_rect.height += (padding_top + padding_bottom);
		image_rect.width += (padding_left + padding_right);

		root_widget->GetBorderSkin()->GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom);

		image_rect.x += padding_left;
		image_rect.y += padding_top;

		root_widget->AddChild(m_image_button);

		root_widget->SetChildRect(m_image_button, image_rect);

		SetInnerSize(list_width + image_rect.width + padding_left + padding_right, max(list_height, image_rect.height) + padding_top + padding_bottom);
	}
	else
	{
		root_widget->GetBorderSkin()->GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom);

		SetInnerSize(list_width + padding_left + padding_right, list_height + padding_top + padding_bottom);
	}
#else
	SetInnerSize(list_width, list_height);
#endif // SUPPORT_GENERATE_THUMBNAILS

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

	m_pages_list->SetGrowToFit( num_columns == 1 );
#ifdef SUPPORT_GENERATE_THUMBNAILS
	if(g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInWindowCycle))
	{
		int offsetY = list_height < image_height ? padding_top + (image_height - list_height) / 2 : padding_top;

		root_widget->SetChildRect(m_pages_list, OpRect(padding_left, offsetY, list_width, list_height));
	}
	else
	{
		root_widget->SetChildRect(m_pages_list, OpRect(padding_left, padding_top, list_width, list_height));
	}
#else
	root_widget->SetChildRect(m_pages_list, OpRect(0, 0, list_width, list_height));
#endif // SUPPORT_GENERATE_THUMBNAILS

	return status;
}


void CyclePagesPopupWindow::AddWindow(DesktopWindow* desktop_window)
{
	OpInputAction* input_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_GO_TO_CYCLER_PAGE));
	if (input_action)
	{
		input_action->SetActionData(desktop_window->GetID());
	}

	INTPTR id = desktop_window->GetID();
	OpButton* button = m_pages_list->AddButton(desktop_window->GetTitle(), NULL, input_action, reinterpret_cast<void*>(id));

	button->SetFixedImage(TRUE);
	button->GetForegroundSkin()->SetWidgetImage(desktop_window->GetWidgetImage());
	// Opera 9.0 skins didn't have a specific section and used Toolbar Button Skin, so we need to fall back to that one.
	button->GetBorderSkin()->SetImage("Cycler Page Button Skin", "Toolbar Button Skin");
	button->SetRestrictImageSize(TRUE);
	button->font_info.justify = GetWidgetContainer()->GetRoot()->GetRTL() ? JUSTIFY_RIGHT : JUSTIFY_LEFT;
	button->SetEllipsis(g_pcui->GetIntegerPref(PrefsCollectionUI::EllipsisInCenter) == 1 ? ELLIPSIS_CENTER : ELLIPSIS_END);
	button->SetFixedTypeAndStyle(TRUE);

	INT32 index = m_browser_window->m_page_bar->FindWidgetByUserData(desktop_window);
	if (index >= 0 && ((OpButton*)m_browser_window->m_page_bar->GetWidget(index))->packed2.has_attention)
	{
		button->SetAttention(TRUE);

		UINT32 color;
		if(OpStatus::IsSuccess(g_skin_manager->GetTextColor(button->GetBorderSkin()->GetImage(), &color, button->GetBorderSkin()->GetState())))
		{
			button->SetForegroundColor(color);
		}
	}
	if (desktop_window == m_browser_window->GetActiveDesktopWindow())
	{
		button->SetBold(TRUE);
		m_pages_list->SetSelected(m_pages_list->GetWidgetCount() - 1);
	}
}


void CyclePagesPopupWindow::OnShow(BOOL show)
{
	if (!show && !m_has_activated_window)
	{
		DesktopWindow* desktop_window = m_browser_window->GetActiveDesktopWindow();
		if (desktop_window && desktop_window->IsActive())
		{
			// Ensure window gets activated (to restore input context)
			desktop_window->Activate(FALSE);
			desktop_window->Activate(TRUE);
		}
	}
}


BOOL CyclePagesPopupWindow::OnInputAction(OpInputAction* action)
{
	if (!m_pages_list)
		return FALSE;

	INT32 selected = m_pages_list->GetSelected();

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_GO_TO_CYCLER_PAGE:
				{
					child_action->SetEnabled(child_action->GetActionData()
						&& g_application->GetDesktopWindowCollection().GetDesktopWindowByID(child_action->GetActionData()));

					// if the buttton with this action is the one selected, set selected on it
					// this is necessary for the skin to work correctly
					child_action->SetSelected((INTPTR)m_pages_list->GetUserData(m_pages_list->GetSelected()) == child_action->GetActionData() );

					return TRUE;
				}
			}
			break;
		}
		case OpInputAction::ACTION_GO_TO_CYCLER_PAGE:
		{
			if (action->GetActionData())
			{
				DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID(action->GetActionData());

				if (desktop_window)
				{
					m_has_activated_window = TRUE;

					// A workspace window will not be visible if it starts up minimized. This "works" in
					// windows because that port will revert a minimized window to a restored on startup
					// (which is a bug). For now I force the visible flag to TRUE [espen 2005-09-29]
					// desktop_window->HackSetVisibleFlagToTrue();

					// Ensure window gets activated (to restore input context)
					if (desktop_window->IsActive())
						desktop_window->Activate(FALSE);

					desktop_window->Activate(TRUE);

					Close();
				}
			}
			return TRUE;
		}
		case OpInputAction::ACTION_CLOSE_CYCLER:
		{
			Close();
			if (action->GetActionData())
			{
				DesktopWindow* desktop_window = g_application->GetDesktopWindowCollection().GetDesktopWindowByID((INTPTR) m_pages_list->GetUserData(m_pages_list->GetSelected()));

				if (desktop_window)
				{
					m_has_activated_window = TRUE;

					// A workspace window will not be visible if it starts up minimized. This "works" in
					// windows because that port will revert a minimized window to a restored on startup
					// (which is a bug). For now I force the visible flag to TRUE [espen 2005-09-29]
					// desktop_window->HackSetVisibleFlagToTrue();

					// Ensure window gets activated (to restore input context)
					if (desktop_window->IsActive())
						desktop_window->Activate(FALSE);

					desktop_window->Activate(TRUE);
				}
			}
			else
			{
				// Prevent loosing keyboard focus if Esc is pressed when window is open and Ctrl pressed
				DesktopWindow* desktop_window = m_browser_window->GetActiveDesktopWindow();
				if (desktop_window && desktop_window->IsActive())
				{
					// Ensure window gets activated (to restore input context)
					desktop_window->Activate(FALSE);
					desktop_window->Activate(TRUE);
					m_has_activated_window = TRUE;
				}
			}

			return TRUE;
		}

		case OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE:
		{
			selected++;
			m_pages_list->SetSelected(selected >= m_pages_list->GetWidgetCount() ? 0 : selected);
#ifdef SUPPORT_GENERATE_THUMBNAILS
			if(g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInWindowCycle))
			{
				SetImages();
			}
#endif // SUPPORT_GENERATE_THUMBNAILS

			return TRUE;
		}
		case OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE:
		{
			selected--;
			m_pages_list->SetSelected(selected < 0 ? m_pages_list->GetWidgetCount() - 1 : selected);
#ifdef SUPPORT_GENERATE_THUMBNAILS
			if(g_pcui->GetIntegerPref(PrefsCollectionUI::UseThumbnailsInWindowCycle))
			{
				SetImages();
			}
#endif // SUPPORT_GENERATE_THUMBNAILS
			return TRUE;
		}
	}

	return FALSE;
}

#ifdef SUPPORT_GENERATE_THUMBNAILS

void CyclePagesPopupWindow::SetImages()
{
	DesktopWindow *desk_win = g_application->GetDesktopWindowCollection().GetDesktopWindowByID((INTPTR) m_pages_list->GetUserData(m_pages_list->GetSelected()));

	OP_ASSERT(desk_win);

	if(!desk_win)
	{
		return;
	}

	Image thumbnail_image = desk_win->GetThumbnailImage();

	if(thumbnail_image.IsEmpty())
	{
		OpBitmap *bitmap = NULL;

		if(OpStatus::IsSuccess(OpBitmap::Create(&bitmap,
			THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT, FALSE, FALSE, 0, 0, TRUE)))
		{
			OpPainter *painter = bitmap->GetPainter();

			if(painter)
			{
				OpString title;

				RETURN_VOID_IF_ERROR(title.Set((uni_char *)desk_win->GetTitle()));

				painter->SetColor(OP_RGB(0xEE, 0xEE, 0xEE));
				painter->FillRect(OpRect(0, 0, THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT));
				painter->SetColor(m_image_button->GetForegroundColor(OP_RGB(0xff, 0xff, 0xff), 0));

				OpFontManager* fontManager = styleManager->GetFontManager();

				if(fontManager)
				{
					FontAtt font_att;
					font_att.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SANSSERIF));
					font_att.SetSize(13);
					font_att.SetWeight(7);

					OpFont* font = g_font_cache ? g_font_cache->GetFont(font_att, 100) : NULL;

					if(font)
					{
						painter->SetFont(font);
						painter->DrawString(OpPoint(5, 5), title.CStr(), title.Length());
						g_font_cache->ReleaseFont(font);
					}
				}
				bitmap->ReleasePainter();
			}
			thumbnail_image = imgManager->GetImage(bitmap);
		}
		else
		{
			OP_DELETE(bitmap);
		}
	}
	m_image_button->GetForegroundSkin()->SetBitmapImage(thumbnail_image);
	m_image_button->InvalidateAll();
}

#endif // SUPPORT_GENERATE_THUMBNAILS
