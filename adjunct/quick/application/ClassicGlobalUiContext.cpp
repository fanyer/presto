/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_util/actions/delayed_action.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/desktop_util/prefs/PrefsUtils.h"
#include "adjunct/desktop_util/search/search_field_history.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/application/ClassicGlobalUiContext.h"
#include "adjunct/quick/controller/ClearPrivateDataController.h"
#include "adjunct/quick/controller/ModeManagerController.h"
#include "adjunct/quick/controller/ScriptOptionsController.h"
#include "adjunct/quick/dialogs/CertificateManagerDialog.h"
#include "adjunct/quick/dialogs/ChangeMasterPasswordDialog.h"
#include "adjunct/quick/dialogs/ContactManagerDialog.h"
#include "adjunct/quick/dialogs/CustomizeToolbarDialog.h"
#include "adjunct/quick/dialogs/GoToPageDialog.h"
#include "adjunct/quick/dialogs/InputManagerDialog.h"
#include "adjunct/quick/dialogs/HomepageDialog.h"
#include "adjunct/quick/dialogs/LinkManagerDialog.h"
#include "adjunct/quick/dialogs/MidClickDialog.h"
#include "adjunct/quick/dialogs/OperaMenuDialog.h"
#include "adjunct/quick/dialogs/PreferencesDialog.h"
#include "adjunct/quick/dialogs/ReportSiteProblemDialog.h"
#include "adjunct/quick/dialogs/ProxyExceptionDialog.h"
#ifdef _PRINT_SUPPORT_
# include "adjunct/quick/dialogs/PrintOptionsDialog.h"
#endif
#include "adjunct/quick/dialogs/SaveSessionDialog.h"
#include "adjunct/quick/dialogs/ServerManagerDialog.h"
#ifdef WEBSERVER_SUPPORT
#include "adjunct/quick/dialogs/ServerWhiteListDialog.h"
#endif
#include "adjunct/quick/dialogs/SetupApplyDialog.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/dialogs/WebSearchDialog.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "adjunct/quick/managers/MessageConsoleManager.h"
#ifdef WEB_TURBO_MODE
#include "adjunct/quick/managers/OperaTurboManager.h"
#endif
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/PluginInstallManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/panels/NotesPanel.h"
#ifdef ENABLE_USAGE_REPORT
# include "adjunct/quick/usagereport/UsageReport.h"
#endif
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/widgets/OpSpeedDialView.h"
#ifdef M2_SUPPORT
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/chatinfo.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2_ui/controllers/LabelPropertiesController.h"
#include "adjunct/m2_ui/dialogs/AccountManagerDialog.h"
#include "adjunct/m2_ui/dialogs/JoinChatDialog.h"
#include "adjunct/m2_ui/dialogs/SearchMailDialog.h"
#include "adjunct/m2_ui/windows/ChatDesktopWindow.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/desktop_util/mail/mailcompose.h"
#include "adjunct/m2_ui/panels/AccordionMailPanel.h"
#endif // M2_SUPPORT
#include "adjunct/quick/managers/LaunchManager.h" 
#include "adjunct/desktop_pi/launch_pi.h"

#include "modules/database/opdatabasemanager.h"
#include "modules/dochand/winman.h"
#include "modules/formats/uri_escape.h"
#include "modules/history/direct_history.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/url/uamanager/ua.h"
#include "modules/util/filefun.h"
#include "modules/util/handy.h"
#include "modules/viewers/plugins.h"

#ifdef MSWIN
#include "platforms/windows/win_handy.h"
#endif
#ifdef _MACINTOSH_
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#endif


namespace
{
//============================================================================
// A collection of DesktopFileChooserListeners
//============================================================================

class MusicSelectionListener : public DesktopFileChooserListener
{
		OpMultimediaPlayer * player;
	public:
		DesktopFileChooserRequest	request;
		DesktopFileChooserRequest&	GetRequest() { return request; }
		MusicSelectionListener(OpMultimediaPlayer * player)
			: player(player) {}

		void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
			{
				if (result.files.GetCount())
				{
					player->LoadMedia(result.files.Get(0)->CStr());
					player->PlayMedia();
				}
			}
};

class DownloadUrlSelectionListener : public DesktopFileChooserListener
{
	public:
		DesktopFileChooserRequest&	GetRequest() { return request; }
		DownloadUrlSelectionListener(URL url)
			: url(url) {}

		void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
			{
				if (result.files.GetCount())
				{
					// need to set the timestamp, this is used for expiry in list when read from rescuefile.
					// This should handled in the TransferManager though. LATER
					time_t loaded = g_timecache->CurrentTime();
					url.SetAttribute(URL::KVLocalTimeLoaded, &loaded);

					if(OpStatus::IsSuccess(((TransferManager*)g_transferManager)->AddTransferItem(url, result.files.Get(0)->CStr())))
					{
						url.LoadToFile(result.files.Get(0)->CStr());
					}
				}
			}

	private:
		URL url;
		DesktopFileChooserRequest request;
};

class OpenDocumentsSelectionListener : public DesktopFileChooserListener
{
	public:
		explicit OpenDocumentsSelectionListener(Application& application)
			: m_application(&application)
		{}

		DesktopFileChooserRequest&	GetRequest() { return request; }
		void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
			{
				for(unsigned int i=0; i < result.files.GetCount(); i++)
				{
					BOOL3 new_page = i == 0 ? MAYBE : YES;
					m_application->OpenURL( *result.files.Get(i) , NO, new_page, NO );
				}
				if (result.active_directory.HasContent())
				{
					TRAPD(err, g_pcfiles->WriteDirectoryL(OPFILE_OPEN_FOLDER, result.active_directory.CStr()));
				}
			}

	private:
		DesktopFileChooserRequest request;
		Application* m_application;
};

class ImportNewsFeedSelectionListener : public DesktopFileChooserListener
{
	public:
		DesktopFileChooserRequest&	GetRequest() { return request; }
		void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
			{
				if (result.files.GetCount() && result.files.Get(0)->HasContent())
					g_m2_engine->ImportOPML(*(result.files.Get(0)));
			}

	private:
		DesktopFileChooserRequest request;
};

class ExportNewsFeedSelectionListener : public DesktopFileChooserListener
{
	public:
		DesktopFileChooserRequest&	GetRequest() { return request; }
		void OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
			{
				if (result.files.GetCount() && result.files.Get(0)->HasContent())
					g_m2_engine->ExportOPML(*(result.files.Get(0)));
			}

	private:
		DesktopFileChooserRequest request;
};


//============================================================================
// Callbacks passed to Application::AskEnterOnlineMode
//============================================================================

class GetMailOnlineModeCallback : public Application::OnlineModeCallback
{
public:
	GetMailOnlineModeCallback(UINT16 account_id, BOOL full_sync) : m_account_id(account_id), m_full_sync(full_sync) {}

	virtual void OnOnlineMode()
	{
		if (m_account_id)
			g_m2_engine->FetchMessages(m_account_id, TRUE);
		else
			g_m2_engine->FetchMessages(TRUE, m_full_sync);
	}

private:
	UINT16 m_account_id;
	BOOL m_full_sync;
};


class SendMailOnlineModeCallback : public Application::OnlineModeCallback
{
public:
	virtual void OnOnlineMode() { g_m2_engine->SendMessages(); }
};


class CheckForUpdateOnlineModeCallback : public Application::OnlineModeCallback
{
public:
	virtual void OnOnlineMode()
	{
#ifdef AUTO_UPDATE_SUPPORT
		OpStatus::Ignore(g_autoupdate_manager->CheckForUpdate());
#endif
	}
};


//============================================================================
// Helper functions
//============================================================================

BOOL SetProxyEnabled(BOOL enable)
{
	BOOL http, https, ftp, socks;

	http = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPProxy);
	https= g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPSProxy);
	ftp  = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseFTPProxy);
    socks = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSOCKSProxy);

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	BOOL auto_proxy = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticProxyConfig);
#endif

	BOOL is_enabled = http || https || ftp || socks
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		|| auto_proxy
#endif
		;

	if (enable == is_enabled)
		return FALSE;

	if (!enable)
	{
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPProxy, FALSE);
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPSProxy, FALSE);
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseFTPProxy, FALSE);
        g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseSOCKSProxy, FALSE);

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AutomaticProxyConfig, FALSE);
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
	}
	else
	{
		OpString proxy;

		// Simply enable all settings where a server is specified
		http = !g_pcnet->GetStringPref(PrefsCollectionNetwork::HTTPProxy).IsEmpty();
		https= !g_pcnet->GetStringPref(PrefsCollectionNetwork::HTTPSProxy).IsEmpty();
		ftp  = !g_pcnet->GetStringPref(PrefsCollectionNetwork::FTPProxy).IsEmpty();
        socks= !g_pcnet->GetStringPref(PrefsCollectionNetwork::SOCKSProxy).IsEmpty();

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		auto_proxy = !g_pcnet->GetStringPref(PrefsCollectionNetwork::AutomaticProxyConfigURL).IsEmpty();
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::AutomaticProxyConfig, auto_proxy);
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPProxy, http);
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseHTTPSProxy, https);
		g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseFTPProxy, ftp);
        g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseSOCKSProxy, socks);
	}

	return TRUE;
}

} // end of anonymous namespace


//============================================================================
// ClassicGlobalUiContext
//============================================================================


ClassicGlobalUiContext::ClassicGlobalUiContext(Application& application)
	: m_enabled(FALSE)
    , m_application(&application)
	, m_chooser(NULL)
	, m_chooser_listener(NULL)
	, m_most_recent_spyable_desktop_window(NULL)
	, m_multimedia_player(NULL)
{
}

ClassicGlobalUiContext::~ClassicGlobalUiContext()
{
	OP_DELETE(m_chooser_listener);
	m_chooser_listener = NULL;

	OP_DELETE(m_chooser);
	m_chooser = NULL;

	OP_DELETE(m_multimedia_player);
	m_multimedia_player = NULL;

}


void ClassicGlobalUiContext::OnDesktopWindowActivated(
		DesktopWindow* desktop_window, BOOL activate)
{
	if (activate)
	{
		if (desktop_window->GetBrowserView() || desktop_window->GetWorkspace())
		{
			m_most_recent_spyable_desktop_window = desktop_window;
		}
	}
}

void ClassicGlobalUiContext::OnDesktopWindowClosing(
		DesktopWindow* desktop_window, BOOL user_initiated)
{
	desktop_window->RemoveListener(this);

	if (desktop_window == m_most_recent_spyable_desktop_window)
	{
		m_most_recent_spyable_desktop_window = NULL;
	}
}

void ClassicGlobalUiContext::SetEnabled(BOOL enabled)
{
	m_enabled = enabled;
}

BOOL ClassicGlobalUiContext::IsInputDisabled()
{
	// Actions in the global context (Application) are disabled when Opera is
	// not yet fully running.
	return !m_enabled;
}


BOOL ClassicGlobalUiContext::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_SET_ALIGNMENT:
			case OpInputAction::ACTION_SET_AUTO_ALIGNMENT:
			case OpInputAction::ACTION_SET_COLLAPSE:
			case OpInputAction::ACTION_SET_WRAPPING:
			case OpInputAction::ACTION_SET_BUTTON_STYLE:
			case OpInputAction::ACTION_ENABLE_LARGE_IMAGES:
			case OpInputAction::ACTION_DISABLE_LARGE_IMAGES:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}
			case OpInputAction::ACTION_MANAGE_SEARCH_ENGINES:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
			case OpInputAction::ACTION_RESTORE_TO_DEFAULTS:
				{
					OpInputContext *context = action->GetFirstInputContext();
					if(context && (context->GetType() == WIDGET_TYPE_BUTTON || context->GetType() == WIDGET_TYPE_TOOLBAR))
					{
						child_action->SetEnabled(TRUE);
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}

			case OpInputAction::ACTION_EMPTY_PAGE_TRASH:
				{
					// If a modal dialog is open, the pagebar trashcan will indicate there
					// are tabs/popups that can be closed even when there are none. Unless
					// we set it to FALSE here.
					child_action->SetEnabled(FALSE);
					return TRUE;
				}

			case OpInputAction::ACTION_SHOW_POPUP_MENU:
			case OpInputAction::ACTION_SHOW_POPUP_MENU_WITH_MENU_CONTEXT:
				{
					OpString8 menu;
					menu.Set(child_action->GetActionDataString());
					child_action->SetEnabled(QuickMenu::IsMenuEnabled(this,
								menu, child_action->GetActionData(),
								action->GetFirstInputContext()));
					return TRUE;
				}

			case OpInputAction::ACTION_VIEW_PROGRESS_BAR:
				{
					child_action->SetSelected(child_action->GetActionData() == g_pcui->GetIntegerPref(PrefsCollectionUI::ProgressPopup));
					return TRUE;
				}

			case OpInputAction::ACTION_SET_SHOW_TRANSFERWINDOW:
				{
					child_action->SetSelected(child_action->GetActionData() == g_pcui->GetIntegerPref(PrefsCollectionUI::TransWinActivateOnNewTransfer));
					return TRUE;
				}

			case OpInputAction::ACTION_IDENTIFY_AS:
				{
					int uaid = (int)g_uaManager->GetUABaseId();

					switch (child_action->GetActionData())
					{
					case 0:
						child_action->SetSelected(uaid == UA_Opera);
						return TRUE;
					case 1:
					case 2:
					case 3:
						child_action->SetSelected(uaid == UA_Mozilla);
						return TRUE;
					case 4:
						child_action->SetSelected(uaid == UA_MSIE);
						return TRUE;

					default:
						return FALSE;
					}
				}
			case OpInputAction::ACTION_ATTACH_DEVTOOLS_WINDOW:
				{
					BrowserDesktopWindow *browser_window = m_application->GetActiveBrowserDesktopWindow();
					DesktopWindow *devtools_window = m_application->GetDesktopWindowCollection().GetDesktopWindowByType(WINDOW_TYPE_DEVTOOLS);
					// Enable attach only if there is no devtools window or it's attached to other window
					child_action->SetEnabled(!devtools_window || (browser_window && !browser_window->HasDevToolsWindowAttached()));

					return TRUE;
				}
			case OpInputAction::ACTION_DETACH_DEVTOOLS_WINDOW:
				{
					BrowserDesktopWindow *browser_window = m_application->GetActiveBrowserDesktopWindow();

					// Enable detach only if devtools window is attached to current browser window
					child_action->SetEnabled(browser_window && browser_window->HasDevToolsWindowAttached());

					return TRUE;
				}
			case OpInputAction::ACTION_OPEN_DEVTOOLS_WINDOW:
				{
					DesktopWindow *devtools_window = m_application->GetDesktopWindowCollection().GetDesktopWindowByType(WINDOW_TYPE_DEVTOOLS);
					child_action->SetSelected(devtools_window && !devtools_window->IsClosing());

					return TRUE;
				}
			case OpInputAction::ACTION_CLOSE_DEVTOOLS_WINDOW:
				{
					// Enable close only if there is devtools window somewhere and it's not closing
					DesktopWindow *devtools_window = m_application->GetDesktopWindowCollection().GetDesktopWindowByType(WINDOW_TYPE_DEVTOOLS);
					child_action->SetEnabled(devtools_window && !devtools_window->IsClosing());

					return TRUE;
				}
			case OpInputAction::ACTION_ENABLE_COOKIES:
			case OpInputAction::ACTION_DISABLE_COOKIES:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_COOKIES, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled) != COOKIE_NONE);
					return TRUE;
				}

			case OpInputAction::ACTION_ENABLE_REFERRER_LOGGING:
			case OpInputAction::ACTION_DISABLE_REFERRER_LOGGING:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_REFERRER_LOGGING, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled));
					return TRUE;
				}
			case OpInputAction::ACTION_ENABLE_JAVASCRIPT:
			case OpInputAction::ACTION_DISABLE_JAVASCRIPT:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_JAVASCRIPT, g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled));
					return TRUE;
				}
			case OpInputAction::ACTION_ENABLE_PLUGINS:
			case OpInputAction::ACTION_DISABLE_PLUGINS:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_PLUGINS, g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled));
					return TRUE;
				}
			case OpInputAction::ACTION_ENABLE_POPUP_WINDOWS:
				{
					int type = 0;
					if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget))
					{
						type = 1;
					}
					else if (g_pcjs->GetIntegerPref(PrefsCollectionJS::TargetDestination) == POPUP_WIN_BACKGROUND)
					{
						type = 2;
					}
					child_action->SetSelected(
						type == 0 &&
						!g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups)
						);
					return TRUE;
				}
			case OpInputAction::ACTION_DISABLE_POPUP_WINDOWS:
				{
					int type = 0;
					if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget))
					{
						type = 1;
					}
				    else if (g_pcjs->GetIntegerPref(PrefsCollectionJS::TargetDestination) == POPUP_WIN_BACKGROUND)
					{
						type = 2;
					}
					child_action->SetSelected(type == 1);
					return TRUE;
				}
			case OpInputAction::ACTION_ENABLE_POPUP_WINDOWS_IN_BACKGROUND:
				{
					int type = 0;
					if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget))
					{
						type = 1;
					}
					else if (g_pcjs->GetIntegerPref(PrefsCollectionJS::TargetDestination) == POPUP_WIN_BACKGROUND)
					{
						type = 2;
					}
					child_action->SetSelected(type == 2);
					return TRUE;
				}
			case OpInputAction::ACTION_ENABLE_REQUESTED_POPUP_WINDOWS:
				{
					int type = 0;
					if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget))
					{
						type = 1;
					}

					child_action->SetSelected(type == 0 && g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups));
					return TRUE;
				}
			case OpInputAction::ACTION_ENABLE_GIF_ANIMATION:
			case OpInputAction::ACTION_DISABLE_GIF_ANIMATION:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_GIF_ANIMATION, g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowAnimation));
					return TRUE;
				}
			case OpInputAction::ACTION_ACTIVATE_WINDOW:
				{
					DesktopWindow* window = m_application->GetDesktopWindowCollection().GetDesktopWindowByID(child_action->GetActionData());
					child_action->SetSelected(window && window->IsActive());
					child_action->SetAttention(window && window->HasAttention());
					return TRUE;
				}

			case OpInputAction::ACTION_HIDE_OPERA:
				{
					child_action->SetEnabled(m_application->IsShowingOpera());
					return TRUE;
				}

			case OpInputAction::ACTION_SET_SKIN_COLORING:
				{
					child_action->SetSelected(g_pccore->GetIntegerPref(PrefsCollectionCore::ColorSchemeMode) == 2 &&
						g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_SKIN)
						== (UINT32)child_action->GetActionData());
					return TRUE;
				}

			case OpInputAction::ACTION_DISABLE_SKIN_COLORING:
				{
					child_action->SetSelected(g_pccore->GetIntegerPref(PrefsCollectionCore::ColorSchemeMode) == 0);
					return TRUE;
				}

			case OpInputAction::ACTION_USE_SYSTEM_SKIN_COLORING:
				{
					child_action->SetSelected(g_pccore->GetIntegerPref(PrefsCollectionCore::ColorSchemeMode) == 1);
					return TRUE;
				}
			case OpInputAction::ACTION_OPEN_URL_IN_CURRENT_PAGE:
			case OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE:
			case OpInputAction::ACTION_OPEN_URL_IN_NEW_BACKGROUND_PAGE:
			case OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW:
				{
					const uni_char * action_data = child_action->GetActionDataString();
					child_action->SetEnabled(action_data != NULL && uni_strlen(action_data) > 0);
					return TRUE;
				}
			case OpInputAction::ACTION_SHOW_PANEL:
			case OpInputAction::ACTION_HIDE_PANEL:
				{
					if (child_action->GetActionData() == -1)
						return TRUE;

					if (child_action->HasActionDataString())
					{
#ifdef M2_SUPPORT
						// only show mail/chat items when they are available
						if ((uni_stricmp(child_action->GetActionDataString(), UNI_L("Mail")) == 0 && !m_application->HasMail() && !m_application->HasFeeds()) ||
							(uni_stricmp(child_action->GetActionDataString(), UNI_L("Contacts")) == 0 && !m_application->HasMail() && !m_application->HasChat()) ||
							(uni_stricmp(child_action->GetActionDataString(), UNI_L("Chat")) == 0 && !m_application->HasChat()))
						{
							child_action->SetEnabled(FALSE);
							return TRUE;
						}
#endif
						if (m_application->GetActiveBrowserDesktopWindow())
						{
							Hotlist* hotlist = m_application->GetActiveBrowserDesktopWindow()->GetHotlist();
							child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SHOW_PANEL,
									hotlist ? hotlist->IsPanelVisible(child_action->GetActionDataString()) : FALSE);
						}
						child_action->SetEnabled(TRUE);
					}
					else
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
			case OpInputAction::ACTION_ADD_REMOVE_PANELS:
				{
					return m_application->GetActiveBrowserDesktopWindow() && m_application->GetActiveBrowserDesktopWindow()->GetHotlist();
				}
#ifdef M2_SUPPORT
#ifdef IRC_SUPPORT
			case OpInputAction::ACTION_SET_CHAT_STATUS:
				{
					INT32 account_id = child_action->GetActionData();

					if (child_action->IsActionDataStringEqualTo(UNI_L("online-any")))
					{
						child_action->SetSelected(m_application->GetChatStatus(account_id) != AccountTypes::OFFLINE);
						child_action->SetEnabled(m_application->SetChatStatus(account_id, AccountTypes::ONLINE, TRUE));
					}
					else
					{
						AccountTypes::ChatStatus chat_status = m_application->GetChatStatusFromString(child_action->GetActionDataString());
						child_action->SetSelected(m_application->GetChatStatus(account_id) == chat_status);
						child_action->SetEnabled(m_application->SetChatStatus(account_id, chat_status, TRUE));
					}
					return TRUE;
				}
#endif // IRC_SUPPORT
			case OpInputAction::ACTION_STOP_MAIL:
				{
					child_action->SetEnabled(m_application->ShowM2());
					return TRUE;
				}
			case OpInputAction::ACTION_STOP_SEND_MAIL:
				{
					child_action->SetEnabled(m_application->ShowM2() && g_m2_engine->GetAccountManager() && g_m2_engine->GetAccountManager()->IsBusy(FALSE));
					return TRUE;
				}
			case OpInputAction::ACTION_COMPOSE_MAIL:
				{
					child_action->SetEnabled(m_application->ShowM2() && g_m2_engine->GetStore()->IsNextM2IDReady());
					return TRUE;
				}
			case OpInputAction::ACTION_READ_MAIL:
			case OpInputAction::ACTION_GET_MAIL: //Fallthrough
			case OpInputAction::ACTION_FULL_SYNC:
			case OpInputAction::ACTION_SEND_QUEUED_MAIL:
			case OpInputAction::ACTION_NEW_ACCOUNT:
			case OpInputAction::ACTION_MANAGE_ACCOUNTS:
			case OpInputAction::ACTION_SUBSCRIBE_TO_GROUPS:
			case OpInputAction::ACTION_SEARCH_MAIL:
			case OpInputAction::ACTION_SHOW_MAIL_FILTERS:
			case OpInputAction::ACTION_LIST_CHAT_ROOMS:
			case OpInputAction::ACTION_NEW_CHAT_ROOM:
			case OpInputAction::ACTION_IMPORT_NEWSFEED_LIST:
			case OpInputAction::ACTION_GOTO_MESSAGE:
				{
					child_action->SetEnabled(m_application->ShowM2());
					return TRUE;
				}
			case OpInputAction::ACTION_EXPORT_NEWSFEED_LIST:
				{
					const BOOL enable = m_application->ShowM2() &&
						(g_m2_engine->GetAccountManager() != 0) &&
						(g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE) != 0);

					child_action->SetEnabled(enable);
					return TRUE;
				}
			case OpInputAction::ACTION_EXPORT_CONTACTS:
				{
					HotlistModel* contacts =  g_hotlist_manager->GetContactsModel();
					child_action->SetEnabled(contacts &&
											 (contacts->GetCount() > 1
											  || !contacts->GetTrashFolder())
);
					return TRUE;

				}
			case OpInputAction::ACTION_IMPORT_MAIL:
				{
					child_action->SetEnabled(m_application->ShowM2() && !g_m2_engine->ImportInProgress());
					return TRUE;
				}

			case OpInputAction::ACTION_SHOW_ACCOUNT:
				{
					if (m_application->ShowM2())
					{
						AccountManager* manager = g_m2_engine ? g_m2_engine->GetAccountManager() : NULL;
						if (manager)
						{
							int active_account;
							BOOL match = FALSE;

							OpString active_account_category;
							OpStatus::Ignore(manager->GetActiveAccount(active_account, active_account_category));

							if (child_action->HasActionDataString())
							{
								match = active_account == -1 &&
									active_account_category.CompareI(child_action->GetActionDataString()) == 0;
							}
							else
							{
								match = active_account == child_action->GetActionData();
							}

							child_action->SetSelected(match);
						}
					}
					return TRUE;
				}
#ifdef M2_CAP_ACCOUNTMGR_LOW_BANDWIDTH_MODE
			case OpInputAction::ACTION_DISABLE_LOW_BANDWIDTH_MODE:
				{
					if (m_application->ShowM2())
					{
						AccountManager* manager = g_m2_engine->GetAccountManager();
						if (manager)
						{
							child_action->SetEnabled(manager->IsLowBandwidthModeEnabled());
							child_action->SetSelected(manager->IsLowBandwidthModeEnabled());
						}
					}
					return TRUE;
				}

			case OpInputAction::ACTION_ENABLE_LOW_BANDWIDTH_MODE:
				{
					if (m_application->ShowM2())
					{
						AccountManager* manager = g_m2_engine->GetAccountManager();
						if (manager)
						{
							child_action->SetEnabled(!manager->IsLowBandwidthModeEnabled());
							child_action->SetSelected(FALSE);
						}
					}
					return TRUE;
				}
#endif // M2_CAP_ACCOUNTMGR_LOW_BANDWIDTH_MODE
#endif // M2_SUPPORT
			case OpInputAction::ACTION_ENABLE_SPECIAL_EFFECTS:
			case OpInputAction::ACTION_DISABLE_SPECIAL_EFFECTS:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_SPECIAL_EFFECTS, g_pccore->GetIntegerPref(PrefsCollectionCore::SpecialEffects));
					return TRUE;
				}
			case OpInputAction::ACTION_SELECT_SKIN:
				{
					child_action->SetSelected(g_skin_manager->GetSelectedSkin() == child_action->GetActionData());
					return TRUE;
				}
			case OpInputAction::ACTION_SHOW_SEARCH:
			case OpInputAction::ACTION_HIDE_SEARCH:
				{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
					SearchTemplate* search = g_searchEngineManager->SearchFromID(child_action->GetActionData());
					if (search)
						child_action->SetSelectedByToggleAction(OpInputAction::ACTION_SHOW_SEARCH, search->GetPersonalbarPos() != -1);
#endif // DESKTOP_UTIL_SEARCH_ENGINES
					return TRUE;
				}

#ifdef SUPPORT_DATA_SYNC
			case OpInputAction::ACTION_SET_UP_SYNC:
				{
					child_action->SetEnabled(g_sync_manager->SyncModuleEnabled() && !g_sync_manager->HasUsedSync());
					return TRUE;
				}
			case OpInputAction::ACTION_SHOW_SYNC_STATUS:
			case OpInputAction::ACTION_OPEN_SYNC_SETTINGS:
				{
					child_action->SetEnabled(g_sync_manager->SyncModuleEnabled() && g_sync_manager->HasUsedSync());
					return TRUE;
				}
			case OpInputAction::ACTION_SYNC_LOGIN:
				{
					if (g_sync_manager->SyncModuleEnabled())
					{
						child_action->SetEnabled(!g_sync_manager->IsLinkEnabled() || (g_sync_manager->IsLinkEnabled() && !g_sync_manager->IsCommWorking()));
					}
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}

			case OpInputAction::ACTION_SYNC_LOGOUT:
				{
					if (g_sync_manager->SyncModuleEnabled())
					{
						child_action->SetEnabled(g_sync_manager->IsLinkEnabled() && g_sync_manager->IsCommWorking());
					}
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
#ifdef TEST_SYNC_NOW
			case OpInputAction::ACTION_SYNC_NOW:
				{
					if (g_sync_manager->SyncModuleEnabled())
					{
						child_action->SetEnabled(g_sync_manager->IsLinkEnabled());
					}
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
#endif // TEST_SYNC_NOW
#endif // SUPPORT_DATA_SYNC

#ifdef WEBSERVER_SUPPORT
#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
			case OpInputAction::ACTION_ENABLE_SERVICE_DISCOVERY_NOTIFICATIONS:
			case OpInputAction::ACTION_DISABLE_SERVICE_DISCOVERY_NOTIFICATIONS:
				{
#ifdef DU_CAP_PREFS
					if (g_webserver_manager->IsFeatureAllowed())
					{
						child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_SERVICE_DISCOVERY_NOTIFICATIONS, g_pcui->GetIntegerPref(PrefsCollectionUI::EnableServiceDiscoveryNotifications));
					}
					else
#endif // DU_CAP_PREFS
					{
						child_action->SetEnabled(FALSE);
					}
					return TRUE;
				}
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT
				// ACTION -------------------
			case OpInputAction::ACTION_OPERA_UNITE_ENABLE:
				{
					child_action->SetEnabled(g_webserver_manager->IsFeatureAllowed() && !g_webserver_manager->IsFeatureEnabled());
					return TRUE;
				}
			case OpInputAction::ACTION_SHOW_WEBSERVER_STATUS:
			case OpInputAction::ACTION_OPERA_UNITE_DISABLE:
			case OpInputAction::ACTION_OPERA_UNITE_RESTART:
				{
					child_action->SetEnabled(g_webserver_manager->IsFeatureEnabled());
					return TRUE;
				}
			case OpInputAction::ACTION_SET_UP_WEBSERVER:
				{
					child_action->SetEnabled(g_webserver_manager->IsFeatureAllowed() && !g_webserver_manager->HasUsedWebServer());
					return TRUE;
				}
			case OpInputAction::ACTION_OPEN_WEBSERVER_SETTINGS:
				{
					child_action->SetEnabled(g_webserver_manager->HasUsedWebServer());
					return TRUE;
				}
			case OpInputAction::ACTION_SHOW_OPERA_UNITE_ROOT:
				{
					if (g_desktop_account_manager->IsInited() && (g_desktop_account_manager->GetLoggedIn() || !g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount)))
						child_action->SetEnabled(g_webserver_manager->IsFeatureEnabled());
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
#endif // WEBSERVER_SUPPORT

			case OpInputAction::ACTION_WORK_OFFLINE:
			case OpInputAction::ACTION_WORK_ONLINE:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_WORK_OFFLINE, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode));
					return TRUE;
				}
			case OpInputAction::ACTION_ENABLE_PROXY_SERVERS:
			case OpInputAction::ACTION_DISABLE_PROXY_SERVERS:
				{
					BOOL http, https, ftp, socks;

					http = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPProxy);
					https= g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseHTTPSProxy);
					ftp  = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseFTPProxy);
                    socks = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseSOCKSProxy);

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
					BOOL auto_proxy;
					auto_proxy = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AutomaticProxyConfig);
#endif
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_PROXY_SERVERS, http || https || ftp || socks
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
						|| auto_proxy
#endif
						);
					return TRUE;
				}
			case OpInputAction::ACTION_SHOW_WINDOW_LIST:
				{
					return TRUE;
				}

#ifdef _MACINTOSH_
				case OpInputAction::ACTION_CLOSE_ALL:
				{
					child_action->SetEnabled(m_application->GetDesktopWindowCollection().GetCount() > 0);
					return TRUE;
				}
				case OpInputAction::ACTION_SPECIAL_CHARACTERS:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
#endif
			case OpInputAction::ACTION_CLOSE_WINDOW:
				{
					DesktopWindow* window = m_application->GetDesktopWindowCollection().GetDesktopWindowByID(child_action->GetActionData());

					if (!window)
						return FALSE;

					child_action->SetSelected(window->IsActive());
					return TRUE;
				}

			case OpInputAction::ACTION_MINIMIZE_WINDOW:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}

			case OpInputAction::ACTION_OPEN_DOCUMENT:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}

			case OpInputAction::ACTION_USE_IMAGE_AS_DESKTOP_BACKGROUND:
			case OpInputAction::ACTION_USE_BACKGROUND_IMAGE_AS_DESKTOP_BACKGROUND:
				{
					BOOL enabled = FALSE;
#if !defined(_UNIX_DESKTOP_)
					if (child_action->GetActionData())
					{
						DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(child_action->GetActionData());
						OpDocumentContext* ctx = menu_context->GetDocumentContext();
						if (ctx)
						{
							if (child_action->GetAction() == OpInputAction::ACTION_USE_BACKGROUND_IMAGE_AS_DESKTOP_BACKGROUND && ctx->HasBGImage() ||
								child_action->GetAction() == OpInputAction::ACTION_USE_IMAGE_AS_DESKTOP_BACKGROUND && ctx->HasImage())
							{
								enabled = TRUE;
							}
						}
					}
#endif
					child_action->SetEnabled( enabled );
					return TRUE;
				}

#ifdef SPELLCHECK_SUPPORT
			case OpInputAction::ACTION_SPELL_CHECK:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}
#endif

			case OpInputAction::ACTION_PASTE_AND_GO:
			case OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND:
			case OpInputAction::ACTION_PASTE_TO_NOTE:
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

			case OpInputAction::ACTION_ENABLE_INLINE_FIND:
			case OpInputAction::ACTION_DISABLE_INLINE_FIND:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_INLINE_FIND, g_pcui->GetIntegerPref(PrefsCollectionUI::UseIntegratedSearch));
					return TRUE;
				}
			case OpInputAction::ACTION_REPORT_SITE_PROBLEM:
				{
					BOOL enable = FALSE;
					DocumentDesktopWindow* ddw = m_application->GetActiveDocumentDesktopWindow();
					if (ddw)
					{
						URL url = WindowCommanderProxy::GetMovedToURL(ddw->GetWindowCommander());
						enable = !url.IsEmpty();
					}

					child_action->SetEnabled(enable);
					return TRUE;
				}
			case OpInputAction::ACTION_SET_PREFERENCE:
				{
					// accepts string of format "User Prefs|User Javascript=1"

					OpString8 data;
					data.Set(child_action->GetActionDataString());
					int sectionlen = data.FindFirstOf('|');
					int keylen = data.FindFirstOf('=');

					if (sectionlen != KNotFound && sectionlen < data.Length() &&
						keylen != KNotFound && keylen > sectionlen && keylen < data.Length())
					{
						data.CStr()[sectionlen] = 0;
						data.CStr()[keylen] = 0;
						const OpStringC value(child_action->GetActionDataString()+keylen+1);
						OpString current;

						BOOL success;
						TRAPD (err, success = g_prefsManager->GetPreferenceL(data.CStr(), data.CStr()+sectionlen+1, current));
						OP_ASSERT(OpStatus::IsSuccess(err) && success); // FIXME: do what if err is bad ?
						child_action->SetSelected(current.CompareI(value) == 0);
					}
					return TRUE;
				}
#ifdef SUPPORT_SPEED_DIAL
			case OpInputAction::ACTION_GOTO_SPEEDDIAL:
				{
					const DesktopSpeedDial* sd = g_speeddial_manager->GetSpeedDial(*child_action);
					child_action->SetEnabled(sd != NULL && !sd->IsEmpty());
					return TRUE;
				}
				break;

#endif // SUPPORT_SPEED_DIAL
#ifdef _PRINT_SUPPORT_
			case OpInputAction::ACTION_PRINT_PREVIEW:
			case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_AS_SCREEN:
			case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ONE_FRAME_PER_SHEET:
			case OpInputAction::ACTION_SHOW_PRINT_PREVIEW_ACTIVE_FRAME:
			case OpInputAction::ACTION_LEAVE_PRINT_PREVIEW:
			case OpInputAction::ACTION_PRINT_DOCUMENT:
			case OpInputAction::ACTION_SHOW_PRINT_OPTIONS:
				{
					child_action->SetEnabled(m_application->GetActiveDesktopWindow(FALSE) != NULL);
					return TRUE;
				}
#endif // _PRINT_SUPPORT_
#ifdef _MACINTOSH_
			case OpInputAction::ACTION_ADD_TO_BOOKMARKS:
				{
					child_action->SetEnabled(FALSE);
					return TRUE;
				}
			case OpInputAction::ACTION_MANAGE:
				{
					BrowserDesktopWindow* browser = m_application->GetActiveBrowserDesktopWindow();
					if (browser)
					{
						if(!browser->GetOpWindow()->IsActiveTopmostWindow())
						{
							child_action->SetEnabled(FALSE);
						}
						else
						{
							child_action->SetEnabled(TRUE);
						}
					}
					return TRUE;
				}
#endif
#ifdef AUTO_UPDATE_SUPPORT
			case OpInputAction::ACTION_RESTORE_AUTO_UPDATE_DIALOG:
			case OpInputAction::ACTION_RESTART_OPERA:
				{
					return g_autoupdate_manager->OnInputAction(action);
				}
#endif
#ifdef WEB_TURBO_MODE
			case OpInputAction::ACTION_ENABLE_OPERA_WEB_TURBO:
			case OpInputAction::ACTION_DISABLE_OPERA_WEB_TURBO:
			{
				child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_OPERA_WEB_TURBO, g_pcui->GetIntegerPref(PrefsCollectionUI::OperaTurboMode) != OperaTurboManager::OperaTurboOff);
				return TRUE;
			}
			case OpInputAction::ACTION_SET_OPERA_WEB_TURBO_MODE:
			{
				int turbo_mode = child_action->GetActionData();
				child_action->SetSelected(turbo_mode == g_pcui->GetIntegerPref(PrefsCollectionUI::OperaTurboMode));
				return TRUE;
			}
			case OpInputAction::ACTION_CLOSE_ALL_PRIVATE:
			{
				OpVector<DesktopWindow> windows;
				m_application->GetDesktopWindowCollection()
					.GetDesktopWindows(OpTypedObject::WINDOW_TYPE_DOCUMENT, windows);
				for(UINT32 i=0; i<windows.GetCount();i++)
				{
					if(windows.Get(i)->PrivacyMode())
						return TRUE;
				}

				// no private tab
				child_action->SetEnabled(FALSE);
				return TRUE;
			}
#endif // WEB_TURBO_MODE
			case OpInputAction::ACTION_ENABLE_FULL_URL_IN_ADDRESS_FIELD:
			case OpInputAction::ACTION_DISABLE_FULL_URL_IN_ADDRESS_FIELD:
			{	
				child_action->SetSelectedByToggleAction(OpInputAction::ACTION_ENABLE_FULL_URL_IN_ADDRESS_FIELD,
					g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullURL));
				return TRUE;
			}

			case OpInputAction::ACTION_IMPORT_BOOKMARKS:
			case OpInputAction::ACTION_IMPORT_NETSCAPE_BOOKMARKS:
			case OpInputAction::ACTION_IMPORT_EXPLORER_FAVORITES:
			case OpInputAction::ACTION_IMPORT_KONQUEROR_BOOKMARKS:
			case OpInputAction::ACTION_IMPORT_KDE1_BOOKMARKS:
			{
				// DSK-351304: disable until bookmarks.adr is loaded
				child_action->SetEnabled(g_desktop_bookmark_manager->GetBookmarkModel()->Loaded());
				return TRUE;
			}

			}
			break;
		} // ACTION_GET_ACTION_STATE

	    case OpInputAction::ACTION_DESKTOP_GADGET_CALLBACK:
	    case OpInputAction::ACTION_DESKTOP_GADGET_CANCEL_CALLBACK:
		{
			GadgetNotificationCallbackWrapper* wrapper = (GadgetNotificationCallbackWrapper*)action->GetActionData();
			HotlistGenericModel* model =
					(HotlistGenericModel*)g_hotlist_manager->GetUniteServicesModel();

			if (model)
			{
				if (action->GetAction() == OpInputAction::ACTION_DESKTOP_GADGET_CALLBACK)
					model->HandleGadgetCallback((OpDocumentListener::GadgetShowNotificationCallback*)wrapper->m_callback /*action->GetActionData()*/,
												OpDocumentListener::GadgetShowNotificationCallback::REPLY_ACKNOWLEDGED);
				else
					model->HandleGadgetCallback((OpDocumentListener::GadgetShowNotificationCallback*)wrapper->m_callback /*action->GetActionData()*/,
												OpDocumentListener::GadgetShowNotificationCallback::REPLY_IGNORED);
			}

			return TRUE;
		}
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
			{
				OpInputAction* child = action->GetChildAction();

				// Close m_application->GetPopupDesktopWindow() if needed
				DesktopWindow* win = m_application->GetPopupDesktopWindow();
				if (win && win->IsClosableByUser()
						&& child && child->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_DOWN
						&& child->GetActionMethod() == OpInputAction::METHOD_MOUSE)
				{
					OpRect rect, win_rect;
					child->GetActionPosition(rect);
					win->GetAbsoluteRect(win_rect);

					if (!win_rect.Contains(rect.TopLeft()))
						m_application->SetPopupDesktopWindow(NULL); //Close the window
				}

#ifdef ENABLE_USAGE_REPORT
				if(g_action_report)
				{
					g_action_report->ActionFilter(child);
				}
#endif
				return m_application->KioskFilter(child);
			}


		case OpInputAction::ACTION_OPEN_BLOCKED_POPUP:
		{
			// Catch requests from notifier window.
			BrowserDesktopWindow* browser_window = m_application->GetActiveBrowserDesktopWindow();
			if (browser_window)
			{
				return browser_window->OnInputAction(action);
			}
		}
		break;

#ifdef SUPPORT_DATA_SYNC
		case OpInputAction::ACTION_SHOW_SYNC_STATUS:
			{
				g_sync_manager->ShowStatus();
				return TRUE;
			}
		case OpInputAction::ACTION_SET_UP_SYNC:
			{
				g_sync_manager->ShowSetupWizard();
				return TRUE;
			}
		case OpInputAction::ACTION_OPEN_SYNC_SETTINGS:
			{
				g_sync_manager->ShowSettingsDialog();
				return TRUE;
			}
		case OpInputAction::ACTION_SYNC_LOGIN:
			{
				if (g_sync_manager->SyncModuleEnabled())
                {
					BOOL is_enabled;
					g_sync_manager->EnableIfRequired(is_enabled);
					return TRUE;
			    }
			    else
					return FALSE;
			}

		case OpInputAction::ACTION_SYNC_LOGOUT:
		{
				if (g_sync_manager->SyncModuleEnabled())
                {
					// Do the final sync and logout
					g_sync_manager->SyncNow(SyncManager::Logout);

					// Logout the Link Service
					g_sync_manager->SetLinkEnabled(FALSE, FALSE, TRUE);

					return TRUE;
			    }
			    else
					return FALSE;
		}
#ifdef TEST_SYNC_NOW
		case OpInputAction::ACTION_SYNC_NOW:
			{
				// This is for testing only and should NEVER be in the UI
				g_sync_manager->SyncNow(SyncManager::Now);
				return TRUE;
			}
#endif // TEST_SYNC_NOW
#endif // SUPPORT_DATA_SYNC

#ifdef WEBSERVER_SUPPORT
#ifdef SHOW_DISCOVERED_DEVICES_SUPPORT
		case OpInputAction::ACTION_ENABLE_SERVICE_DISCOVERY_NOTIFICATIONS:
		case OpInputAction::ACTION_DISABLE_SERVICE_DISCOVERY_NOTIFICATIONS:
		{
#ifdef DU_CAP_PREFS
			return PrefsUtils::SetPrefsToggleByAction(action, OpInputAction::ACTION_ENABLE_SERVICE_DISCOVERY_NOTIFICATIONS, g_pcui, PrefsCollectionUI::EnableServiceDiscoveryNotifications);
#else
			return TRUE;
#endif // DU_CAP_PREFS
		}
#endif // SHOW_DISCOVERED_DEVICES_SUPPORT
#endif // WEBSERVER_SUPPORT

		case OpInputAction::ACTION_MANAGE_WHITELIST:
		{
			ServerWhiteListDialog* dialog = OP_NEW(ServerWhiteListDialog, ());
			if (dialog)
				dialog->Init(m_application->GetActiveDesktopWindow());

			return TRUE;
		}

		case OpInputAction::ACTION_GET_TYPED_OBJECT:
			{
				if (action->GetActionData() == WINDOW_TYPE_DESKTOP_WITH_BROWSER_VIEW && m_most_recent_spyable_desktop_window && m_most_recent_spyable_desktop_window->OnInputAction(action))
				{
					return TRUE;
				}
				return FALSE;
			}

		case OpInputAction::ACTION_ADD_MUSIC:
			{
				if (!m_chooser)
					RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

				OpMultimediaPlayer* player = GetMultimediaPlayer();
				if (player)
				{
					MusicSelectionListener *selection_listener = OP_NEW(MusicSelectionListener, (player));
					if (selection_listener)
					{
						m_chooser_listener = selection_listener;
						DesktopFileChooserRequest& request = selection_listener->GetRequest();
						request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
						DesktopWindow* parent = m_application->GetActiveDesktopWindow(FALSE);
						OpString tmp_storage;
						request.initial_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_OPEN_FOLDER, tmp_storage));
						OpString filter;
						OP_STATUS rc = g_languageManager->GetString(Str::S_OPEN_FILE_FILTER_MP3, filter);
						if (OpStatus::IsSuccess(rc) && filter.HasContent())
						{
							StringFilterToExtensionFilter(filter.CStr(), &request.extension_filters);
						}
						if (OpStatus::IsSuccess(m_chooser->Execute(parent ? parent->GetOpWindow() : 0, selection_listener, request)))
							return TRUE;
					}
				}
				return TRUE;
			}

		case OpInputAction::ACTION_PLAY_MUSIC:
			{
				OpMultimediaPlayer* player = GetMultimediaPlayer();

				if (player)
				{
					player->PlayMedia();
				}

				return TRUE;
			}

		case OpInputAction::ACTION_PAUSE_MUSIC:
			{
				OpMultimediaPlayer* player = GetMultimediaPlayer();

				if (player)
				{
					player->PauseMedia();
				}

				return TRUE;
			}

		case OpInputAction::ACTION_STOP_MUSIC:
			{
				OpMultimediaPlayer* player = GetMultimediaPlayer();

				if (player)
				{
					player->StopMedia();
				}

				return TRUE;
			}

		case OpInputAction::ACTION_SAVE_LINK:
			{
				// I cannot really see the difference between ACTION_DOWNLOAD_URL_AS and this...
				DesktopMenuContext * menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (menu_context && menu_context->GetDocumentContext() && menu_context->GetDocumentContext()->HasLink())
				{
					URLInformation* url_info;
					if (OpStatus::IsSuccess(menu_context->GetDocumentContext()->CreateLinkURLInformation(&url_info)))
					{
						if (OpStatus::IsSuccess(url_info->DownloadTo(NULL, NULL))) // TODO: use second parameter to define download path
							url_info = NULL; // url_info part is taken over by core...
						OP_DELETE(url_info);
					}
					return TRUE;
				}
				return FALSE;
			}

		case OpInputAction::ACTION_ENABLE_PROXY_SERVERS:
			{
				return SetProxyEnabled(TRUE);
			}
		case OpInputAction::ACTION_DISABLE_PROXY_SERVERS:
			{
				return SetProxyEnabled(FALSE);
			}

		case OpInputAction::ACTION_MANAGE:
			{
				m_application->GetPanelDesktopWindow(Hotlist::GetPanelTypeByName(action->GetActionDataString()),
														 TRUE,
														 FALSE,
														 action->GetActionData() == 1,
														 action->GetActionDataStringParameter());
				return TRUE;
			}

		case OpInputAction::ACTION_SHOW_WINDOW_LIST:
			{
				// maybe implement
				return TRUE;
			}

		case OpInputAction::ACTION_VIEW_PROGRESS_BAR:
			{
				g_pcui->WriteIntegerL(PrefsCollectionUI::ProgressPopup, action->GetActionData());
				return TRUE;
			}

#ifdef WEBSERVER_SUPPORT
		case OpInputAction::ACTION_OPERA_UNITE_ENABLE:
			{
				if(g_webserver_manager->IsFeatureAllowed() && !g_webserver_manager->IsFeatureEnabled())
				{
					BOOL has_started;
					g_webserver_manager->EnableIfRequired(has_started, FALSE, FALSE);
					return TRUE;
				}
				else
					return FALSE;
			}
		case OpInputAction::ACTION_OPERA_UNITE_DISABLE:
			{
				if(g_webserver_manager->IsFeatureEnabled())
				{
					g_webserver_manager->DisableFeature();
					return TRUE;
				}
				else
					return FALSE;
			}
		case OpInputAction::ACTION_OPERA_UNITE_RESTART:
			{
				if(g_webserver_manager->IsFeatureEnabled())
				{
					g_webserver_manager->Restart();
					return TRUE;
				}
				else
					return FALSE;
			}
		case OpInputAction::ACTION_SET_UP_WEBSERVER:
			{
				if (g_webserver_manager->IsFeatureAllowed())
				{
					g_webserver_manager->ShowSetupWizard();
				}
				return TRUE;
			}
		case OpInputAction::ACTION_OPEN_WEBSERVER_SETTINGS:
			{
				g_webserver_manager->ShowSettingsDialog();
				return TRUE;
			}
		case OpInputAction::ACTION_SHOW_WEBSERVER_STATUS:
			{
				g_webserver_manager->ShowStatus();
				return TRUE;
			}

		case OpInputAction::ACTION_SHOW_OPERA_UNITE_ROOT:
			{
				g_webserver_manager->OpenRootService();

				return TRUE;
			}
#endif // WEBSERVER_SUPPORT

		case OpInputAction::ACTION_WORK_OFFLINE:
		case OpInputAction::ACTION_WORK_ONLINE:
			{
				BOOL go_offline = action->GetAction() == OpInputAction::ACTION_WORK_OFFLINE;
				BOOL is_offline = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode);

				if (is_offline == go_offline)
					return FALSE;

				UpdateOfflineMode(TRUE);
				return TRUE;
			}

		case OpInputAction::ACTION_SHOW_MESSAGE_CONSOLE:
			{
#ifdef OPERA_CONSOLE
 				// FIXME: OOM
				g_message_console_manager->OpenDialog();
				return TRUE;
#endif // OPERA_CONSOLE
			}

#ifdef _PRINT_SUPPORT_
		case OpInputAction::ACTION_SHOW_PRINT_OPTIONS:
			{
				PrintOptionsDialog* dialog = OP_NEW(PrintOptionsDialog, ());
				if (dialog)
					dialog->Init(m_application->GetDesktopWindowFromAction(action));
				return TRUE;
			}
#endif // _PRINT_SUPPORT_

		case OpInputAction::ACTION_SAVE_WINDOW_SETUP:
			{
				SaveSessionDialog* dialog = OP_NEW(SaveSessionDialog, ());
				if (dialog)
					dialog->Init(m_application->GetDesktopWindowFromAction(action));
				return TRUE;
			}

		case OpInputAction::ACTION_DELETE_PRIVATE_DATA:
			{
				ShowDialog(OP_NEW(ClearPrivateDataController, ()), this,
						m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_SHOW_SEARCH:
		case OpInputAction::ACTION_HIDE_SEARCH:
			{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
				SearchTemplate* search = g_searchEngineManager->SearchFromID(action->GetActionData());
				if (!search)
					return FALSE;

				BOOL is_shown = search->GetPersonalbarPos() != -1;
				BOOL show = action->GetAction() == OpInputAction::ACTION_SHOW_SEARCH;

				if (is_shown == show)
					return FALSE;

				search->SetPersonalbarPos(show ? 0 : -1);

				// Do ChangeItem on the ID instead?

				int index = g_searchEngineManager->SearchIDToIndex(action->GetActionData());

				g_searchEngineManager->ChangeItem(search, index, SearchEngineManager::FLAG_PERSONALBAR);
				g_searchEngineManager->Write();
#endif // DESKTOP_UTIL_SEARCH_ENGINES
				return TRUE;
			}

		case OpInputAction::ACTION_ENABLE_SPECIAL_EFFECTS:
		case OpInputAction::ACTION_DISABLE_SPECIAL_EFFECTS:
			{
				if (!PrefsUtils::SetPrefsToggleByAction(action, OpInputAction::ACTION_ENABLE_SPECIAL_EFFECTS, g_pccore, PrefsCollectionCore::SpecialEffects))
					return FALSE;

				m_application->SettingsChanged(SETTINGS_SPECIAL_EFFECTS);
				return TRUE;
			}

		case OpInputAction::ACTION_SET_SKIN_COLORING:
			{
				g_pccore->WriteIntegerL(PrefsCollectionCore::ColorSchemeMode, 2);
				g_pcfontscolors->WriteColorL(OP_SYSTEM_COLOR_SKIN, action->GetActionData());
				g_skin_manager->SetColorSchemeMode(OpSkin::COLOR_SCHEME_MODE_CUSTOM);
				g_skin_manager->SetColorSchemeColor(action->GetActionData());
				m_application->SettingsChanged(SETTINGS_COLOR_SCHEME);
				return TRUE;
			}

		case OpInputAction::ACTION_DISABLE_SKIN_COLORING:
			{
				g_pccore->WriteIntegerL(PrefsCollectionCore::ColorSchemeMode, 0);
				g_skin_manager->SetColorSchemeMode(OpSkin::COLOR_SCHEME_MODE_NONE);
				m_application->SettingsChanged(SETTINGS_COLOR_SCHEME);
				return TRUE;
			}

		case OpInputAction::ACTION_USE_SYSTEM_SKIN_COLORING:
			{
				g_pccore->WriteIntegerL(PrefsCollectionCore::ColorSchemeMode, 1);
				g_skin_manager->SetColorSchemeMode(OpSkin::COLOR_SCHEME_MODE_SYSTEM);
				m_application->SettingsChanged(SETTINGS_COLOR_SCHEME);
				return TRUE;
			}

		case OpInputAction::ACTION_RELOAD_SKIN:
			{
				OpFile skinfile;
				g_pcfiles->GetFileL(PrefsCollectionFiles::ButtonSet, skinfile);
				OpStatus::Ignore(g_skin_manager->SelectSkinByFile(&skinfile));

				// Call m_application->SettingsChanged since SelectSkinByFile doesn't do it if the skin is not changed.
				// This is needed to redraw windows.
				m_application->SettingsChanged(SETTINGS_SKIN);
				return TRUE;
			}

		case OpInputAction::ACTION_SHOW_PANEL:
		case OpInputAction::ACTION_HIDE_PANEL:
			{
				BrowserDesktopWindow* browser = m_application->GetActiveBrowserDesktopWindow();
				if (!browser)
					return FALSE;

				Hotlist* hotlist = browser->GetHotlist();
				if (!hotlist)
					return FALSE;

				const bool show_panel = action->GetAction() == OpInputAction::ACTION_SHOW_PANEL;
				return hotlist->ShowPanelByName(action->GetActionDataString(), show_panel);
			}
		case OpInputAction::ACTION_ADD_REMOVE_PANELS:
			{
				m_application->GetMenuHandler()->ShowPopupMenu(
						"Hotlist AddRemove Item Popup Menu", PopupPlacement::AnchorAtCursor());
				return TRUE;
			}
		case OpInputAction::ACTION_CUSTOMIZE_TOOLBARS:
			{
				return m_application->CustomizeToolbars(NULL, action->GetActionData() == -3 ? CUSTOMIZE_PAGE_TOOLBARS : 0);
			}

		case OpInputAction::ACTION_MANAGE_LINKS:
			{
				LinkManagerDialog* dialog = OP_NEW(LinkManagerDialog, ());
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_MANAGE_CONTACTS:
			{
				ContactManagerDialog* dialog = OP_NEW(ContactManagerDialog, ());
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_MANAGE_MOUSE:
			{
				InputManagerDialog* dialog = OP_NEW(InputManagerDialog, (action->GetActionData(), OPMOUSE_SETUP));
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_MANAGE_KEYBOARD:
			{
				InputManagerDialog* dialog = OP_NEW(InputManagerDialog, (action->GetActionData(), OPKEYBOARD_SETUP));
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_MANAGE_WAND:
			{
				ServerManagerDialog* dialog = OP_NEW(ServerManagerDialog, (FALSE, TRUE));
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_MANAGE_COOKIES:
			{
				ServerManagerDialog* dialog = OP_NEW(ServerManagerDialog, (TRUE, FALSE));
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_MANAGE_SITES:
			{
				ServerManagerDialog* dialog = OP_NEW(ServerManagerDialog, (FALSE, FALSE));
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_MANAGE_CERTIFICATES:
			{
				CertificateManagerDialog* dialog = OP_NEW(CertificateManagerDialog, ());
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_CHANGE_MASTERPASSWORD:
			{
				ChangeMasterPasswordDialog* dialog = OP_NEW(ChangeMasterPasswordDialog, ());
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_EXECUTE_PROGRAM:
			{
				if (action->GetActionDataString())
				{
					OpString parameters;
					
					if (action->GetActionData() == 1)
					{
						m_application->ExecuteProgram(action->GetActionDataString(), action->GetActionDataStringParameter());
					}
					else
					{
						StringUtils::ArrayExpansionResult result(TRUE);
						DesktopMenuContext * menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
						if (OpStatus::IsSuccess(StringUtils::ExpandParameters(
										GetWindowCommander(),
										action->GetActionDataStringParameter(),
										result,menu_context)))
						{
							int argc = 0;
							const char** argv = NULL;
							if (OpStatus::IsSuccess(result.GetResult(argc, argv)))
							{
								LaunchPI* launch_pi = LaunchPI::Create();
								if (launch_pi)
								{
									launch_pi->Launch(action->GetActionDataString(), argc, argv);
									delete launch_pi; // launch_pi is allocated using new operator
								}
							}
						}
					}
				}
				return TRUE;
			}
		case OpInputAction::ACTION_OPEN_FILE_IN:
			{
				static_cast<DesktopOpSystemInfo*>(g_op_system_info)->OpenFileInApplication(
						action->GetActionDataString(),
						action->GetActionDataStringParameter());
				return TRUE;
			}
		case OpInputAction::ACTION_OPEN_IMAGE_IN:
			{
				if (action->GetActionData() && action->GetActionDataString())
				{
#ifdef WIC_CAP_URLINFO_OPENIN
					DesktopMenuContext * menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
					OpDocumentContext* document_context = menu_context->m_context;
					if (document_context)
					{
						URLInformation* image_url_info = NULL;
						if (OpStatus::IsSuccess(document_context->CreateImageURLInformation(&image_url_info))
						{
							if (OpStatus::IsSuccess(image_url_info->OpenIn(this, action->GetActionDataString()))
								menu_info->image_url_info = NULL;
						}
					}
#endif // WIC_CAP_URLINFO_OPENIN
				}
				return TRUE;
			}

		case OpInputAction::ACTION_GO_TO_HOMEPAGE:
			{
				m_application->GoToPage(g_pccore->GetStringPref(PrefsCollectionCore::HomeURL), FALSE, FALSE, FALSE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
				return TRUE;
			}

		case OpInputAction::ACTION_SET_HOMEPAGE:
			{
#ifdef PERMANENT_HOMEPAGE_SUPPORT
				BOOL permanent_homepage = (1 == g_pcui->GetIntegerPref(PrefsCollectionUI::PermanentHomepage));
				if (permanent_homepage)
				{
					return TRUE;
				}
#endif //PERMANENT_HOMEPAGE_SUPPORT
				HomepageDialog* dialog = OP_NEW(HomepageDialog, ());
				if (dialog)
					dialog->Init(m_application->GetActiveDesktopWindow(), WindowCommanderProxy::GetActiveWindowCommander());
				return TRUE;
			}

#ifdef M2_SUPPORT
#ifdef IRC_SUPPORT
		case OpInputAction::ACTION_SET_CHAT_STATUS:
			{
				INT32 account_id = action->GetActionData();

				if (action->IsActionDataStringEqualTo(UNI_L("online-any")))
				{
					if (m_application->GetChatStatus(account_id) != AccountTypes::OFFLINE)
						return FALSE;

					return m_application->SetChatStatus(account_id, AccountTypes::ONLINE);
				}
				else
				{
					return m_application->SetChatStatus(account_id, m_application->GetChatStatusFromString(action->GetActionDataString()));
				}
			}

		case OpInputAction::ACTION_JOIN_PRIVATE_CHAT:
			{
				OpString name;

				name.Set(action->GetActionDataString());

				ChatInfo chat_info(name, OpStringC());
				m_application->GoToChat(action->GetActionData(), chat_info, FALSE);
				return TRUE;
			}

		case OpInputAction::ACTION_NEW_CHAT_ROOM:
		case OpInputAction::ACTION_JOIN_CHAT_ROOM:
			{
				if( m_application->ShowM2() )
				{
					if (action->GetActionDataString() && action->GetAction() == OpInputAction::ACTION_JOIN_CHAT_ROOM)
					{
						OpString name;

						name.Set(action->GetActionDataString());

						ChatInfo chat_info(name, OpStringC());

						m_application->GoToChat(action->GetActionData(), chat_info, TRUE);
					}
					else
					{
						if (!m_application->HasChat())
							return m_application->ShowAccountNeededDialog(AccountTypes::IRC);

						JoinChatDialog* dialog = OP_NEW(JoinChatDialog, ());
						if (dialog)
							dialog->Init(m_application->GetDesktopWindowFromAction(action), action->GetActionData());
					}
				}
				return TRUE;
			}

		case OpInputAction::ACTION_LEAVE_CHAT_ROOM:
			{
				if(m_application->ShowM2())
				{
					OpString name;

					name.Set(action->GetActionDataString());
					return m_application->LeaveChatRoom(action->GetActionData(), name);
				}
				else
				{
					return TRUE;
				}
			}
#endif // IRC_SUPPORT
		case OpInputAction::ACTION_COMPOSE_MAIL:
			{
				OpString8 raw_mailto;
				BOOL force_in_opera = FALSE;

				if (action->GetActionData())
				{
					DesktopMenuContext * dmctx = reinterpret_cast<DesktopMenuContext *>(action->GetActionData());
					if (dmctx->GetDocumentContext() && dmctx->GetDocumentContext()->HasMailtoLink())
					{
						dmctx->GetDocumentContext()->GetLinkData(OpDocumentContext::AddressForUIEscaped, &raw_mailto);
						MailTo mailto;
						mailto.Init(raw_mailto.CStr());
						MailCompose::ComposeMail(mailto, force_in_opera);
					}
				}
				else if (g_application->GetActiveDesktopWindow(FALSE)->GetType() == WINDOW_TYPE_MAIL_VIEW)
				{
					return g_application->GetActiveDesktopWindow(FALSE)->OnInputAction(action);
				}
				else
				{
					// Making a mailto URL without to address is a nice hack :)
					raw_mailto.Set("mailto:");
					force_in_opera = TRUE;
					MailTo mailto;
					mailto.Init(raw_mailto.CStr());
					MailCompose::ComposeMail(mailto, force_in_opera);
				}
				return TRUE;
			}
		case OpInputAction::ACTION_RESET_MAIL_PANEL:
		{
			if (m_application->ShowM2() && m_application->GetActiveBrowserDesktopWindow())
			{
				Hotlist* hotlist = m_application->GetActiveBrowserDesktopWindow()->GetHotlist();
				if (!hotlist)
					return FALSE;

				MailPanel* panel = static_cast<MailPanel*>(hotlist->GetPanelByType(PANEL_TYPE_MAIL));
				if (panel)
					panel->PopulateCategories(TRUE);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_NEW_ACCOUNT:
			{
				OpString8 account;
				account.Set(action->GetActionDataString());
				return m_application->CreateAccount(AccountTypes::UNDEFINED, account.CStr(), m_application->GetDesktopWindowFromAction(action));
			}
		case OpInputAction::ACTION_IMPORT_NEWSFEED_LIST:
			{
				if (!m_chooser)
					RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

				ImportNewsFeedSelectionListener *selection_listener = OP_NEW(ImportNewsFeedSelectionListener, ());
				if (selection_listener)
				{
					m_chooser_listener = selection_listener;
					DesktopFileChooserRequest& request = selection_listener->GetRequest();
					request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;
					request.initial_path.Set("opera-newsfeeds.opml");
					OpWindow* parent = m_application->GetActiveDesktopWindow()->GetOpWindow();
					g_languageManager->GetString(Str::D_NEWSFEED_IMPORT_OPEN_DIALOG_TITLE, request.caption);
					OpString filter;
					OP_STATUS rc = g_languageManager->GetString(Str::D_NEWSFEED_IMPORT_EXPORT_FILTER, filter);
					if (OpStatus::IsSuccess(rc) && filter.HasContent())
					{
						StringFilterToExtensionFilter(filter.CStr(), &request.extension_filters);
					}
					OpStatus::Ignore(m_chooser->Execute(parent, selection_listener, request));
				}
				return TRUE;
			}
		case OpInputAction::ACTION_EXPORT_NEWSFEED_LIST:
			{
				if (!m_chooser)
					RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

				ExportNewsFeedSelectionListener *selection_listener = OP_NEW(ExportNewsFeedSelectionListener, ());
				if (selection_listener)
				{
					m_chooser_listener = selection_listener;
					DesktopFileChooserRequest& request = selection_listener->GetRequest();
					request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE_PROMPT_OVERWRITE;
					request.initial_path.Set("opera-newsfeeds.opml");
					OpWindow* parent = m_application->GetActiveDesktopWindow()->GetOpWindow();
					g_languageManager->GetString(Str::D_NEWSFEED_EXPORT_SAVE_DIALOG_TITLE, request.caption);
					OpString filter;
					OP_STATUS rc = g_languageManager->GetString(Str::D_NEWSFEED_IMPORT_EXPORT_FILTER, filter);
					if (OpStatus::IsSuccess(rc) && filter.HasContent())
					{
						StringFilterToExtensionFilter(filter.CStr(), &request.extension_filters);
					}
					OpStatus::Ignore(m_chooser->Execute(parent, selection_listener, request));
				}
				return TRUE;
			}
		case OpInputAction::ACTION_IMPORT_MAIL:
			{
				return m_application->CreateAccount(AccountTypes::IMPORT);
			}
		case OpInputAction::ACTION_MANAGE_ACCOUNTS:
			{
				if (m_application->ShowM2())
				{
					if (!m_application->HasChat() && !m_application->HasMail())
						return m_application->ShowAccountNeededDialog(AccountTypes::UNDEFINED);

					AccountManagerDialog* dialog = OP_NEW(AccountManagerDialog, ());
					if (dialog)
						dialog->Init(m_application->GetActiveDesktopWindow());
				}
				return TRUE;
			}
		case OpInputAction::ACTION_EDIT_ACCOUNT:
			{
				return m_application->EditAccount(action->GetActionData(), m_application->GetDesktopWindowFromAction(action));
			}
		case OpInputAction::ACTION_MANAGE_MODES:
			{
				ShowDialog(OP_NEW(ModeManagerController, ()), this, m_application->GetActiveDesktopWindow());
				return TRUE;
			}
		case OpInputAction::ACTION_MANAGE_SCRIPT_OPTIONS:
			{
				ShowDialog(OP_NEW(ScriptOptionsController, ()), this,
						m_application->GetDesktopWindowFromAction(action));
				return TRUE;
			}
		case OpInputAction::ACTION_MANAGE_MIDDLE_CLICK_OPTIONS:
			{
				MidClickDialog::Create(m_application->GetDesktopWindowFromAction(action));
				return TRUE;
			}
		case OpInputAction::ACTION_SHOW_ACCOUNT:
			{
				if (m_application->ShowM2())
				{
					AccountManager* manager = g_m2_engine->GetAccountManager();
					if (manager)
					{
						manager->SetActiveAccount(action->GetActionData(), action->HasActionDataString() ? action->GetActionDataString() : UNI_L(""));
					}
				}
				return TRUE;
			}
#ifdef M2_CAP_ACCOUNTMGR_LOW_BANDWIDTH_MODE
		case OpInputAction::ACTION_DISABLE_LOW_BANDWIDTH_MODE:
			{
				if (m_application->ShowM2())
				{
					AccountManager* manager = g_m2_engine->GetAccountManager();
					if (manager)
					{
						manager->SetLowBandwidthMode(FALSE);
					}
				}
				return TRUE;
			}

		case OpInputAction::ACTION_ENABLE_LOW_BANDWIDTH_MODE:
			{
				if (m_application->ShowM2())
				{
					AccountManager* manager = g_m2_engine->GetAccountManager();
					if (manager)
					{
						manager->SetLowBandwidthMode(TRUE);
					}
				}
				return TRUE;
			}
#endif // M2_CAP_ACCOUNTMGR_LOW_BANDWIDTH_MODE
		case OpInputAction::ACTION_LIST_CHAT_ROOMS:
		case OpInputAction::ACTION_SUBSCRIBE_TO_GROUPS:
			{
				OpString8 type_name;
				type_name.Set(action->GetActionDataString());
				return m_application->SubscribeAccount(action->GetAction() == OpInputAction::ACTION_SUBSCRIBE_TO_GROUPS ? AccountTypes::NEWS : AccountTypes::IRC, type_name.CStr(), action->GetActionData(), m_application->GetDesktopWindowFromAction(action));
			}
		case OpInputAction::ACTION_START_SEARCH:
			{
				OpString text;

				text.Set(action->GetActionDataString());

				if (text.HasContent())
				{
					index_gid_t id;

					g_m2_engine->GetIndexer()->StartSearch(text, SearchTypes::EXACT_PHRASE, SearchTypes::ENTIRE_MESSAGE, 0, UINT32(-1), id, 0);
					m_application->GoToMailView(id, NULL, NULL, TRUE, FALSE, TRUE, action->IsKeyboardInvoked());
				}
				return TRUE;
			}
		case OpInputAction::ACTION_SEARCH_MAIL:
			{
				if (m_application->ShowM2())
				{
					SearchMailDialog* dialog = OP_NEW(SearchMailDialog, ());
					if (dialog)
						dialog->Init(m_application->GetActiveMailDesktopWindow(), action->GetActionData());
				}
				return TRUE;
			}
		case OpInputAction::ACTION_SHOW_MAIL_FILTERS:
			{
				if (m_application->ShowM2())
				{
					LabelPropertiesController* controller = OP_NEW(LabelPropertiesController, (0));
					ShowDialog(controller, this, m_application->GetActiveDesktopWindow());
				}
				return TRUE;
			}

		case OpInputAction::ACTION_STOP_MAIL:
			{
				if (m_application->ShowM2())
				{
					g_m2_engine->StopFetchingMessages();
					g_m2_engine->StopSendingMessages();
					return TRUE;
				}
				return FALSE;
			}
		case OpInputAction::ACTION_FULL_SYNC:
		case OpInputAction::ACTION_GET_MAIL:
			{
				if (m_application->ShowM2())
				{
					if (!m_application->HasMail())
						return m_application->ShowAccountNeededDialog(AccountTypes::UNDEFINED);

					UINT16 account_id = (UINT16)action->GetActionData();
					BOOL sync = action->GetAction() == OpInputAction::ACTION_FULL_SYNC;
					GetMailOnlineModeCallback* callback = OP_NEW(GetMailOnlineModeCallback, (account_id, sync));
					if (callback)
						m_application->AskEnterOnlineMode(TRUE, callback);
				}
				return TRUE;
			}
		case OpInputAction::ACTION_SEND_QUEUED_MAIL:
			{
				if (m_application->ShowM2())
				{
					if (!m_application->HasMail())
						return m_application->ShowAccountNeededDialog(AccountTypes::UNDEFINED);

					SendMailOnlineModeCallback* callback = OP_NEW(SendMailOnlineModeCallback, ());
					if (callback)
						m_application->AskEnterOnlineMode(TRUE, callback);
				}
				return TRUE;
			}
		case OpInputAction::ACTION_STOP_SEND_MAIL:
			{
				if (m_application->ShowM2())
				{
					if (action->GetActionData())
					{
						g_m2_engine->StopSendingMessages((UINT16) action->GetActionData());
					}
					else
					{
						g_m2_engine->StopSendingMessages();
					}
					return TRUE;
				}
				return FALSE;
			}
		case OpInputAction::ACTION_READ_MAIL:
			{
				if (m_application->ShowM2())
				{
					INT32 index_id = action->GetActionData();

					if (action->IsActionDataStringEqualTo(UNI_L("rss")))
					{
						g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE, &index_id);
					}

					if (index_id == 0)
						index_id = IndexTypes::UNREAD_UI;

					m_application->GoToMailView(index_id, NULL, NULL, TRUE, FALSE, TRUE, action->IsKeyboardInvoked());
				}

				return TRUE;
			}
		case OpInputAction::ACTION_GOTO_MESSAGE:
			{
				Message message;

				if (OpStatus::IsSuccess(g_m2_engine->GetMessage(message, action->GetActionData())))
				{
					// Go to the correct view
					MailDesktopWindow* mail_window = GoToMailView(&message);

					// Select the message
					if (mail_window)
						mail_window->GoToMessage(message.GetId());
				}
				return TRUE;
			}
		case OpInputAction::ACTION_READ_NEWSFEED:
			{
				m_application->GoToMailView(action->GetActionData(), NULL, NULL, TRUE, FALSE, TRUE, action->IsKeyboardInvoked());
				return TRUE;
			}
		case OpInputAction::ACTION_SUBSCRIBE_NEWSFEED:
			{
				if (m_application->GetActiveDocumentDesktopWindow())
				{
#ifdef WEBFEEDS_DISPLAY_SUPPORT
					m_application->GetActiveDocumentDesktopWindow()->GoToPage(action->GetActionDataString(), TRUE);
#else
					NewsfeedSubscribeDialog* dialog = OP_NEW(NewsfeedSubscribeDialog, ());
					if (!dialog || OpStatus::IsError(dialog->Init(m_application->GetActiveDocumentDesktopWindow(), m_application->GetActiveDocumentDesktopWindow()->GetTitle(), action->GetActionDataString())))
						return FALSE;
#endif // WEBFEEDS_DISPLAY_SUPPORT

					return TRUE;
				}
				return FALSE;
			}
		case OpInputAction::ACTION_VIEW_MESSAGES_FROM_CONTACT:
			{
				HotlistManager::ItemData item_data;
				g_hotlist_manager->GetItemValue( action->GetActionData(), item_data );
				if( !item_data.mail.IsEmpty() )
				{
					m_application->GoToMailView(0, item_data.mail.CStr(), NULL, TRUE, FALSE, TRUE, action->IsKeyboardInvoked());
				}
				return TRUE;
			}

#endif // M2_SUPPORT
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
					if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK )
					{
						g_hotlist_manager->OpenUrl(action->GetActionData(), MAYBE, MAYBE, MAYBE);
					}
					else if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE )
					{
						g_hotlist_manager->OpenUrl(action->GetActionData(), NO, YES, NO );
					}
					else if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW )
					{
						g_hotlist_manager->OpenUrl(action->GetActionData(), YES, NO, NO );
					}
					else if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE )
					{
						g_hotlist_manager->OpenUrl(action->GetActionData(), NO, YES, YES );
					}
					else if( action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW )
					{
						g_hotlist_manager->OpenUrl(action->GetActionData(), YES, NO, YES );
					}

					return TRUE;

				}
				else
				{
					OpenURLSetting setting;
					DesktopMenuContext * menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
					if (menu_context)
					{
						DesktopWindow* desktop_window = menu_context->GetDesktopWindow();
						if (desktop_window)
							setting.m_src_commander = desktop_window->GetWindowCommander();

						setting.m_new_window    = action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW || action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW ? YES : NO;
						setting.m_new_page      = action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE || action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE ? YES : NO;
						setting.m_in_background = action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE || action->GetAction() == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW ? YES : NO;

						if (OpStatus::IsError(menu_context->GetDocumentContext()->GetLinkData(OpDocumentContext::AddressNotForUI, &setting.m_address)))
							break;

						setting.m_target_window = desktop_window;

						if(setting.m_target_window && setting.m_target_window->PrivacyMode())
							setting.m_is_privacy_mode = TRUE;

						if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled) && desktop_window)
						{
							setting.m_ref_url = WindowCommanderProxy::GetActiveFrameDocumentURL(desktop_window->GetWindowCommander());
						}

						Type type = desktop_window ? desktop_window->GetType() : WINDOW_TYPE_UNKNOWN;

						if (action->IsGestureInvoked())
						{
							OP_DELETE(menu_context);
							menu_context = NULL;
						}

						if (action->GetAction() == OpInputAction::ACTION_OPEN_LINK
							&&	(	type == WINDOW_TYPE_MAIL_VIEW
								||	type == WINDOW_TYPE_MAIL_COMPOSE
								||	type == WINDOW_TYPE_CHAT
								||	type == WINDOW_TYPE_CHAT_ROOM))
						{
							// Don't open link in this page, request a new one.
							setting.m_new_page = YES;
						}
#ifdef GADGET_SUPPORT
						else if (	type == WINDOW_TYPE_GADGET
								 && (setting.m_new_window || setting.m_new_page)
								 && uni_strncmp(UNI_L("file:"), setting.m_address.CStr(), 5) == 0)
							return TRUE;
#endif // GADGET_SUPPORT
						else
						{
							return m_application->OpenResolvedURL(setting);
						}
						return TRUE;
					}
					return FALSE; // Gestures must be allowed to do other things.
				}
			}

#ifdef GADGET_SUPPORT
		case OpInputAction::ACTION_OPEN_WIDGET:
			{
				// Trash not included in menu, so open without further checking
				return g_hotlist_manager->OpenGadget(action->GetActionData());
			}
	    case OpInputAction::ACTION_INSTALL_WIDGET:
			{
				if (action->GetActionDataString())
				{
					OpString text;
					text.Set(action->GetActionDataString());

					text.Strip();

					m_application->GoToPage(text, FALSE, FALSE, FALSE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());

					return TRUE;
				}
				return FALSE;
			}
#endif // GADGET_SUPPORT
		case OpInputAction::ACTION_OPEN_URL_IN_CURRENT_PAGE:
		case OpInputAction::ACTION_OPEN_URL_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_URL_IN_NEW_BACKGROUND_PAGE:
		case OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW:

			if ( action->GetActionDataString() )
			{
				OpenURLSetting setting;

				setting.m_address.Set( action->GetActionDataString() );			//url
				setting.m_from_address_field = action->GetActionData();			//misc flag for user inputted urls
				setting.m_save_address_to_history = action->GetActionData();

				if( action->GetAction() == OpInputAction::ACTION_OPEN_URL_IN_CURRENT_PAGE )
					setting.m_new_page = NO;
				else
					setting.m_new_page = YES;

				if( action->GetAction() == OpInputAction::ACTION_OPEN_URL_IN_NEW_BACKGROUND_PAGE )
					setting.m_in_background = YES;
				else
					setting.m_in_background = NO;

				if( action->GetAction() == OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW )
					setting.m_new_window = YES;
				else
					setting.m_new_window = NO;


				m_application->OpenURL( setting );

				return TRUE;
			}
			return FALSE;

		case OpInputAction::ACTION_EXPORT_BOOKMARKS:
			g_hotlist_manager->SaveAs( HotlistModel::BookmarkRoot, HotlistManager::SaveAs_Export );
			return TRUE;

		case OpInputAction::ACTION_EXPORT_BOOKMARKS_TO_HTML:
			g_hotlist_manager->SaveAs( HotlistModel::BookmarkRoot, HotlistManager::SaveAs_Html );
			return TRUE;

	    case OpInputAction::ACTION_OPEN_BOOKMARKS_FILE:
			{
				g_hotlist_manager->Open( HotlistModel::BookmarkRoot );
				return TRUE;
			}
		case OpInputAction::ACTION_EXPORT_CONTACTS:
			g_hotlist_manager->SaveAs( HotlistModel::ContactRoot, HotlistManager::SaveAs_Export );
			return TRUE;

		case OpInputAction::ACTION_NEW_CONTACTS_FILE:
			g_hotlist_manager->New( HotlistModel::ContactRoot );
			return TRUE;

		case OpInputAction::ACTION_OPEN_CONTACTS_FILE:
			g_hotlist_manager->Open( HotlistModel::ContactRoot );
			return TRUE;

		case OpInputAction::ACTION_IMPORT_BOOKMARKS:
		case OpInputAction::ACTION_IMPORT_CONTACTS:
		case OpInputAction::ACTION_IMPORT_NETSCAPE_BOOKMARKS:
		case OpInputAction::ACTION_IMPORT_EXPLORER_FAVORITES:
		case OpInputAction::ACTION_IMPORT_KONQUEROR_BOOKMARKS:
		case OpInputAction::ACTION_IMPORT_KDE1_BOOKMARKS:
			{
				BrowserDesktopWindow* browser = m_application->GetActiveBrowserDesktopWindow();
				INT32 id = browser ? browser->GetSelectedHotlistItemId(action->GetAction() == OpInputAction::ACTION_IMPORT_CONTACTS ? PANEL_TYPE_CONTACTS : PANEL_TYPE_BOOKMARKS) : -1;

				HotlistModel::DataFormat format;
				switch (action->GetAction())
				{
					case OpInputAction::ACTION_IMPORT_CONTACTS:				format = HotlistModel::OperaContact; break;
					case OpInputAction::ACTION_IMPORT_NETSCAPE_BOOKMARKS:	format = HotlistModel::NetscapeBookmark; break;
					case OpInputAction::ACTION_IMPORT_EXPLORER_FAVORITES:	format = HotlistModel::ExplorerBookmark; break;
					case OpInputAction::ACTION_IMPORT_KONQUEROR_BOOKMARKS:	format = HotlistModel::KonquerorBookmark; break;
					case OpInputAction::ACTION_IMPORT_KDE1_BOOKMARKS:		format = HotlistModel::KDE1Bookmark; break;
					default:	format = HotlistModel::OperaBookmark;
				}

				g_hotlist_manager->Import(id, format);
				return TRUE;
			}

		case OpInputAction::ACTION_NEW_BROWSER_WINDOW:
			{
				DocumentDesktopWindow* page = 0;
				m_application->GetBrowserDesktopWindow(TRUE, FALSE, TRUE, &page, NULL, 0, 0, TRUE, FALSE, action->IsKeyboardInvoked());
				return TRUE;
			}

		case OpInputAction::ACTION_NEW_PRIVATE_BROWSER_WINDOW:
			{
				DocumentDesktopWindow* page = 0;
				BrowserDesktopWindow* browser = m_application->GetBrowserDesktopWindow(TRUE, FALSE, TRUE, &page, NULL, 0, 0, TRUE, FALSE, action->IsKeyboardInvoked(),FALSE,0,TRUE);
				if(browser)
				{
					browser->SetPrivacyMode(TRUE);// the above line just create a private tab, this line make sure the whole window is pirvate
				}
				return TRUE;
			}
		
		case OpInputAction::ACTION_NEW_PAGE:
			{
				DocumentDesktopWindow* page = 0;
#ifdef NEW_WINDOW_DEFAULT_OPEN_IN_BACKGROUND
				BOOL ignore_modifiers = TRUE;
#else
				BOOL ignore_modifiers = action->IsKeyboardInvoked();
#endif
				// First param = BOOL force_new_window (default FALSE)
				m_application->GetBrowserDesktopWindow(m_application->IsSDI() && !action->GetActionData(), FALSE, TRUE, &page, NULL, 0, 0, TRUE, FALSE, ignore_modifiers);

                // When the action is triggered from tray menu we should activate Opera
				if (page)
					page->Activate();

				return TRUE;
			}

		case OpInputAction::ACTION_NEW_PRIVATE_PAGE:
			{
				DocumentDesktopWindow* page = 0;
#ifdef NEW_WINDOW_DEFAULT_OPEN_IN_BACKGROUND
				BOOL ignore_modifiers = TRUE;
#else
				BOOL ignore_modifiers = action->IsKeyboardInvoked();
#endif
				m_application->GetBrowserDesktopWindow(m_application->IsSDI() && !action->GetActionData(), FALSE, TRUE, &page, NULL, 0, 0, TRUE, FALSE, ignore_modifiers, FALSE, 0, TRUE);
		
				// When the action is triggered from tray menu we should activate Opera
				if (page)
					page->Activate();

                return TRUE;
			}

	    case OpInputAction::ACTION_COPY_TO_NOTE:
 		{
			INT32 id = HotlistModel::NoteRoot;

			BrowserDesktopWindow* browser = m_application->GetActiveBrowserDesktopWindow();
			if (browser)
			{
				id = browser->GetSelectedHotlistItemId(PANEL_TYPE_NOTES);

				if (id == -1) // No item selected
				{
					id = HotlistModel::NoteRoot;
				}
			}

			OpWindowCommander* commander = GetWindowCommander();

			if (commander && commander->HasSelectedText())
			{
				HotlistManager::ItemData item_data;
				uni_char* text       = commander->GetSelectedText();
				const uni_char* url  = commander->GetCurrentURL(PASSWORD_HIDDEN);

				if (text)
					item_data.name.Set(text);
				if (url)
					item_data.url.Set(url);

				// created

				if (text)
				{
					if (id != HotlistModel::NoteRoot)
					{
						HotlistModel* model = g_hotlist_manager->GetNotesModel();
						if (model)
						{
							HotlistModelItem* item = model->GetItemByID(id);

							if (!item)
							{
								id = HotlistModel::NoteRoot;
							}
							else if (!item->IsFolder())
							{
								HotlistModelItem* folder = item->GetParentFolder();
								if (folder)
								{
									id = folder->GetID();
								}
								else
								{
									id = HotlistModel::NoteRoot;

								}
							}
						}

					}

					g_hotlist_manager->NewNote(item_data, id, NULL, NULL);
					OP_DELETEA(text);
				}
				return TRUE;
			}
			return FALSE;
 		}

		case OpInputAction::ACTION_PASTE_AND_GO:
		case OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND:
		case OpInputAction::ACTION_PASTE_TO_NOTE:
			{
				OpString text;
				g_desktop_clipboard_manager->GetText(text);
				if (text.HasContent())
				{
					if (action->GetAction() == OpInputAction::ACTION_PASTE_TO_NOTE)
						NotesPanel::NewNote(text);
					else
					{
						// This is a bit tricky. Paste&Go has a default shortcut that includes Ctrl and Shift which
						// normally redirects a url to a new tab. That is why we test IsKeyboardInvoked() below. In
						// addition the 'NewWindow' pref tells us to open in a new tab/window no matter what (DSK-313944)
						BOOL bg = action->GetAction() == OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND;
						if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
						{
							if (m_application->IsSDI())
								g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW, 0, text.CStr());
							else
								m_application->GoToPage(text, TRUE, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
						}
						else
							m_application->GoToPage(text.CStr(), bg, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
					}
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
					BOOL bg = action->GetAction() == OpInputAction::ACTION_PASTE_SELECTION_AND_GO_BACKGROUND;
					if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
					{
						if (m_application->IsSDI())
							g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_URL_IN_NEW_WINDOW, 0, text.CStr());
						else
							m_application->GoToPage(text, TRUE, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
					}
					else
						m_application->GoToPage(text.CStr(), bg, bg, TRUE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
				}
				return TRUE;
			}
#endif

		case OpInputAction::ACTION_GO_TO_SIMILAR_PAGE:
		case OpInputAction::ACTION_GO_TO_PAGE:
		case OpInputAction::ACTION_GO:
		case OpInputAction::ACTION_GO_TO_NICKNAME:
			{
				if (OpStringC(action->GetActionDataString()).HasContent())
				{
					OpString text;

					text.Set(action->GetActionDataString());

					text.Strip();

					OpString expanded_url;

					BOOL url_came_from_address_field = action->GetActionData() == 1;

					OpWindowCommander* window_commander = GetWindowCommander();

					if (!url_came_from_address_field)
					{
						DesktopMenuContext * menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
						if (OpStatus::IsError(StringUtils::ExpandParameters(
										window_commander,
										text.CStr(), expanded_url, FALSE,
										menu_context)))
							return TRUE;
					}

					m_application->GoToPage(url_came_from_address_field ? text.CStr() : expanded_url.CStr(), FALSE, FALSE, url_came_from_address_field, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());

					return TRUE;
				}
				else
				{
					GoToPageDialog* dialog = OP_NEW(GoToPageDialog, (action->GetAction() == OpInputAction::ACTION_GO_TO_NICKNAME));
					if (dialog)
						dialog->Init(m_application->GetActiveDesktopWindow());

					return TRUE;
				}
			}
		
		case OpInputAction::ACTION_GO_TO_TYPED_ADDRESS:
			{
				if (action->GetActionDataString())
				{
					OpenURLSetting url;
					url.m_address.Set(action->GetActionDataString());
					url.m_address.Strip();
					url.m_typed_address.Set(url.m_address);
					url.m_save_address_to_history = TRUE;
					url.m_from_address_field = (action->GetActionData() >> 8) == 1; // Extract the 9th bit
					url.m_new_window = MAYBE;
					url.m_new_page = MAYBE;
					url.m_in_background = MAYBE;

					m_application->AdjustForAction(url, action, m_application->GetActiveDocumentDesktopWindow());

					switch (action->GetActionData() & 0xFF) // The lower 8 bits is DirectHistory::ItemType
					{
					case 0:
						break;
					case 1:
						url.m_type = DirectHistory::SEARCH_TYPE;
						break;
					case 2:
						url.m_type = DirectHistory::SELECTED_TYPE;
						break;
					case 3:
						url.m_type = DirectHistory::NICK_TYPE;
						break;
					default:
						OP_ASSERT(FALSE);
					}

					m_application->OpenURL(url);
				}

				return TRUE;
			}

		case OpInputAction::ACTION_SHOW_HELP:
			{
				if (!action->GetActionDataString())
				{
					return TRUE;
				}

				OpString url;

				if (uni_strnicmp(action->GetActionDataString(), UNI_L("http"), 4))
				{
					url.Set("opera:/help/");
				}

				url.Append(action->GetActionDataString());

				// Need to escape the url to prevent stripping of anchors
				OpString8 finalurl;
				finalurl.Set(url.CStr());

				int escapes = UriEscape::CountEscapes(finalurl.CStr(), UriEscape::StandardUnsafe);
				// NOTE: url_ds.cpp thinks we shouldn't escape javascript: and news URLs.
				if (escapes > 0)
				{
					char* escaped_url = OP_NEWA(char, finalurl.Length() + escapes*2 +1);
					if (escaped_url)
					{
						UriEscape::Escape(escaped_url, finalurl.CStr(), UriEscape::StandardUnsafe);

						url.Set(escaped_url);
						OP_DELETEA(escaped_url);
					}
				}

				if (action->GetActionData())
					m_application->GoToPage(url.CStr());
				else
				{

					DocumentDesktopWindow* document_window = NULL;

					m_application->GetBrowserDesktopWindow(TRUE, FALSE, TRUE, &document_window, NULL, 0, 0, TRUE, FALSE, action->IsKeyboardInvoked());

					if (document_window)
						document_window->GoToPage(url.CStr(), FALSE);
				}

				return TRUE;
			}

		case OpInputAction::ACTION_DOWNLOAD_URL_AS:
		case OpInputAction::ACTION_DOWNLOAD_URL:
			if (action->GetActionData())
			{
				// If a data pointer is defined in the action it shall be:
				DesktopMenuContext * menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				OpDocumentContext* context = menu_context->GetDocumentContext();
				if (context)
				{
					URLInformation* url_info;
					if (OpStatus::IsSuccess(context->CreateLinkURLInformation(&url_info)))
					{
						OP_STATUS sts = OpStatus::ERR_FILE_NOT_FOUND;
						if (action->GetAction() == OpInputAction::ACTION_DOWNLOAD_URL)
						{
							sts = url_info->DownloadDefault(NULL, NULL); // TODO: use second parameter too
							if (OpStatus::IsSuccess(sts) || sts != OpStatus::ERR_FILE_NOT_FOUND)
							{
								if (OpStatus::IsSuccess(sts))
								{
									url_info = NULL;
									// url_info part is taken over by core...
								}
							}
						}

						if (sts == OpStatus::ERR_FILE_NOT_FOUND)
						{
							// ACTION_DOWNLOAD_URL_AS or ACTION_DOWNLOAD_URL and file not found
							OP_ASSERT(url_info);
							sts = url_info->DownloadTo(NULL, NULL); // TODO: use second parameter to define download path
							if (OpStatus::IsSuccess(sts))
							{
								url_info = NULL;
								// url_info part is taken over by core...
							}
						}
						OP_DELETE(url_info);
					}
				}
				return TRUE;
			}
			else
			{
				URL durl;
				OpString filename;

				if (!action->GetActionDataString())
					break;

				OpString resolved_url_name;
				OpStringC expanded_url_name(action->GetActionDataString());

				g_url_api->ResolveUrlNameL(expanded_url_name, resolved_url_name);

				if(resolved_url_name.IsEmpty())
				{
					return TRUE;
				}

				URL durl1 = g_url_api->GetURL(resolved_url_name.CStr(),NULL,TRUE);

				URL durl2 = durl1.GetAttribute(URL::KMovedToURL, TRUE);

				durl = (durl2.IsEmpty() ? durl1 : durl2);

				// use the active document-url as referrer
				DocumentDesktopWindow* active_docwin = m_application->GetActiveDocumentDesktopWindow();

				OpString referrer_url_name;

				if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled) && active_docwin && active_docwin->HasDocument())
				{
					URL referrer_url = WindowCommanderProxy::GetCurrentDocURL(active_docwin->GetWindowCommander());
					referrer_url.GetAttribute(URL::KUniName_Username_Password_Hidden, referrer_url_name, TRUE);
				}

				URL referrer_url = g_url_api->GetURL(referrer_url_name.CStr(),NULL,TRUE);
				URL referer_url;

				CommState res = durl.Load(g_main_message_handler, referer_url);

				// Resolve the address before adding to transferspanel
				if(res == COMM_REQUEST_FAILED)
					break;

				OpString download_folder_opstring;
				RETURN_VALUE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_DOWNLOAD_FOLDER, download_folder_opstring), TRUE);
				const OpStringC downloaddirectory(download_folder_opstring.CStr());
				BOOL filename_valid = TRUE;

				filename.Set(downloaddirectory);

				OpString suggested_filename;
				TRAPD(err, durl.GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE));
				RETURN_VALUE_IF_ERROR(err, TRUE);

				if( suggested_filename.IsEmpty() )
				{
					filename_valid = FALSE;
				}
				else
				{
					// URL::GetAttribute(URL::KSuggestedFileName) can return a filename that is illegal on this system
					OpString illegal_chars;
					static_cast<DesktopOpSystemInfo*>(g_op_system_info)->GetIllegalFilenameCharacters(&illegal_chars);

					filename_valid = (NULL == uni_strpbrk(suggested_filename.CStr(), illegal_chars.CStr()));
					filename.Append(suggested_filename);
				}

				if( !filename_valid || action->GetAction() == OpInputAction::ACTION_DOWNLOAD_URL_AS)
				{
					if (!m_chooser)
						RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

					if( action->GetAction() == OpInputAction::ACTION_DOWNLOAD_URL_AS )
					{
						// Do not use kDownloadFolder with interactive download
						filename.Set(suggested_filename);
					}

					DownloadUrlSelectionListener *selection_listener = OP_NEW(DownloadUrlSelectionListener, (durl));
					if (selection_listener)
					{
						m_chooser_listener = selection_listener;
						DesktopFileChooserRequest& request = selection_listener->GetRequest();
						g_languageManager->GetString(Str::S_SAVE_AS_CAPTION, request.caption);
						request.initial_path.Set(filename.CStr());
						request.action = DesktopFileChooserRequest::ACTION_FILE_SAVE;
						OpString filter;
						OP_STATUS rc = g_languageManager->GetString(Str::SI_IDSTR_ALL_FILES_ASTRIX, filter);
						if (OpStatus::IsSuccess(rc) && filter.HasContent())
						{
							filter.Append(UNI_L("|*.*|"));
							StringFilterToExtensionFilter(filter.CStr(), &request.extension_filters);
						}
						DesktopWindow* parent = action->GetActionData() ? (DesktopWindow*)action->GetActionData() : m_application->GetActiveDesktopWindow(FALSE);
						OpStatus::Ignore(m_chooser->Execute(parent ? parent->GetOpWindow() : 0, selection_listener, request));
					}
				}
				else
				{
					OP_STATUS err;
					err = CreateUniqueFilename(filename);
					OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?

					// need to set the timestamp, this is used for expiry in list when read from rescuefile.
					// This should handled in the TransferManager though. LATER
					time_t loaded = g_timecache->CurrentTime();
					durl.SetAttribute(URL::KVLocalTimeLoaded, &loaded);

					if(OpStatus::IsSuccess(((TransferManager*)g_transferManager)->AddTransferItem(durl, filename.CStr())))
					{
						durl.LoadToFile(filename.CStr());
					}

				}
				return TRUE;
			}
		case OpInputAction::ACTION_SHOW_OPERA:
			{
		 	//	if (!IsBrowserStarted())
			//		StartBrowser();
				return m_application->ShowOpera(TRUE, TRUE);
			}

		case OpInputAction::ACTION_HIDE_OPERA:
			{
				return m_application->ShowOpera(FALSE);
			}

#ifdef _MACINTOSH_
		case OpInputAction::ACTION_SPECIAL_CHARACTERS:
			{
				ShowSpecialCharactersPalette();
				return TRUE;
			}
#endif
		case OpInputAction::ACTION_CLOSE_WINDOW:
			{
				DesktopWindow* window = m_application->GetDesktopWindowCollection().GetDesktopWindowByID(action->GetActionData());

				if (!window)
					return FALSE;

				window->Close(FALSE, TRUE, FALSE);
				return TRUE;
			}
		case OpInputAction::ACTION_ACTIVATE_WINDOW:
			{
				DesktopWindow* window = action->GetActionData() ? m_application->GetDesktopWindowCollection().GetDesktopWindowByID(action->GetActionData()) : m_application->GetBrowserDesktopWindow(FALSE, FALSE, FALSE, NULL, NULL, 0, 0, TRUE, FALSE, action->IsKeyboardInvoked());

				if (window)
				{
					window->Activate();

					// We cannot do Raise() in Activate() in unix because focus-follows-mouse
					// focus mode shall activate but not auto raise a window [espen 2003-09-09]
#if defined(_UNIX_DESKTOP_)
					if (window->GetType() == WINDOW_TYPE_BROWSER)
					{
						window->Raise();

						// Sometimes activate and raise still don't result in window decoration
						// being focused. Force it. [pobara 2012-03-08]
						window->GetOpWindow()->SetFocus(TRUE);
					}
					else if (window->GetParentDesktopWindow())
					{
						window->GetParentDesktopWindow()->Raise();
						window->Raise();
					}
#endif
				}
				return TRUE;
			}

		case OpInputAction::ACTION_REFRESH_DISPLAY:
		{
			INT32 window_id;

			// Check for action data, this holds the ID of the window to refresh (if used)
			if ((window_id = action->GetActionData()) != 0)
			{
				// Find the window
				DesktopWindow *desktop_window = m_application->GetDesktopWindowCollection().GetDesktopWindowByID((INT32)window_id);

				if (desktop_window)
				{
					OpWindowCommander *wc = desktop_window->GetWindowCommander();

					if (wc)
					{
#ifdef _PRINT_SUPPORT_
						if(WindowCommanderProxy::GetPreviewMode(wc))
						{
							WindowCommanderProxy::TogglePrintMode(wc);
						}
#endif // _PRINT_SUPPORT_
						WindowCommanderProxy::UpdateCurrentDoc(wc);

						return TRUE;
					}
				}
			}
			return FALSE;
		}
		break;

		case OpInputAction::ACTION_IDENTIFY_AS:
			{
				switch (action->GetActionData())
				{
				case 0:
					g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UABaseId, UA_Opera);
					break;
				case 1:
				case 2:
				case 3:
					g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UABaseId, UA_Mozilla);
					break;
				case 4:
					g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UABaseId, UA_MSIE);
					break;
				default:
					return FALSE;
				}

				m_application->SettingsChanged(SETTINGS_IDENTIFY_AS);
				return TRUE;
			}
#ifdef _ASK_COOKIE
		case OpInputAction::ACTION_ENABLE_COOKIES:
			{
				if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled) == COOKIE_NONE)
					{
						g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CookiesEnabled, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DisabledCookieState));
						return TRUE;
					}
					return FALSE;
			}
		case OpInputAction::ACTION_DISABLE_COOKIES:
			{
				if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled) != COOKIE_NONE)
					{
						// Save away copy of current setting, then set current to no cookies
						g_pcnet->WriteIntegerL(PrefsCollectionNetwork::DisabledCookieState, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled));
						g_pcnet->WriteIntegerL(PrefsCollectionNetwork::CookiesEnabled, COOKIE_NONE);
						return TRUE;
					}
					return FALSE;
			}
		case OpInputAction::ACTION_ENABLE_REFERRER_LOGGING:
		case OpInputAction::ACTION_DISABLE_REFERRER_LOGGING:
			{
				BOOL is_enabled =
					g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ReferrerEnabled);
				BOOL enable = action->GetAction() == OpInputAction::ACTION_ENABLE_REFERRER_LOGGING;

				if (is_enabled == enable)
					return FALSE;

				g_pcnet->WriteIntegerL(PrefsCollectionNetwork::ReferrerEnabled, enable);
				return TRUE;
			}

#endif // _ASK_COOKIE
		case OpInputAction::ACTION_ENABLE_JAVASCRIPT:
		case OpInputAction::ACTION_DISABLE_JAVASCRIPT:
			{
				return PrefsUtils::SetPrefsToggleByAction(action, OpInputAction::ACTION_ENABLE_JAVASCRIPT, g_pcjs, PrefsCollectionJS::EcmaScriptEnabled);
			}
		case OpInputAction::ACTION_ENABLE_PLUGINS:
		case OpInputAction::ACTION_DISABLE_PLUGINS:
			{
				if (!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled) &&
					action->GetAction() == OpInputAction::ACTION_ENABLE_PLUGINS)
				{
					g_plugin_viewers->RefreshPluginViewers(FALSE);
				}

				return PrefsUtils::SetPrefsToggleByAction(action, OpInputAction::ACTION_ENABLE_PLUGINS, g_pcdisplay, PrefsCollectionDisplay::PluginsEnabled);
			}
		case OpInputAction::ACTION_ENABLE_POPUP_WINDOWS:
			{
				g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, FALSE);
				g_pcjs->WriteIntegerL(PrefsCollectionJS::TargetDestination, POPUP_WIN_NEW);
				g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, FALSE);
				return TRUE;
			}
		case OpInputAction::ACTION_DISABLE_POPUP_WINDOWS:
			{
				g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, TRUE);
				g_pcjs->WriteIntegerL(PrefsCollectionJS::TargetDestination, POPUP_WIN_IGNORE);
				g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, FALSE);
				return TRUE;
			}
		case OpInputAction::ACTION_ENABLE_POPUP_WINDOWS_IN_BACKGROUND:
			{
				g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, FALSE);
				g_pcjs->WriteIntegerL(PrefsCollectionJS::TargetDestination, POPUP_WIN_BACKGROUND);
				g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, FALSE);
				return TRUE;
			}
		case OpInputAction::ACTION_ENABLE_REQUESTED_POPUP_WINDOWS:
			{
				g_pcdoc->WriteIntegerL(PrefsCollectionDoc::IgnoreTarget, FALSE);
				g_pcjs->WriteIntegerL(PrefsCollectionJS::TargetDestination, POPUP_WIN_NEW);
				g_pcjs->WriteIntegerL(PrefsCollectionJS::IgnoreUnrequestedPopups, TRUE);
				return TRUE;
			}
		case OpInputAction::ACTION_ENABLE_GIF_ANIMATION:
		case OpInputAction::ACTION_DISABLE_GIF_ANIMATION:
			{
				return PrefsUtils::SetPrefsToggleByAction(action, OpInputAction::ACTION_ENABLE_GIF_ANIMATION, g_pcdoc,
					PrefsCollectionDoc::ShowAnimation
					);
			}
#ifdef QUICK_NEW_OPERA_MENU
		case OpInputAction::ACTION_SHOW_MENU:
			{
				OperaMenuDialog *menu = OP_NEW(OperaMenuDialog, ());
				if(menu && OpStatus::IsError(menu->Init(m_application->GetActiveDesktopWindow(TRUE))))
				{
					OP_DELETE(menu);
				}
			}
			break;
#endif // QUICK_NEW_OPERA_MENU
		case OpInputAction::ACTION_SHOW_POPUP_MENU:
		case OpInputAction::ACTION_SHOW_HIDDEN_POPUP_MENU:
		case OpInputAction::ACTION_SHOW_POPUP_MENU_WITH_MENU_CONTEXT:
			{
#if defined (_DEBUG) && defined (MSWIN)
				// showing a menu on F12 in Visual Studio tends to create problems
				if (action->GetActionDataString() && uni_strcmp(action->GetActionDataString(), "Quick Preferences Menu") == 0)
					return TRUE;
#endif // _DEBUG && MSWIN

				// copy parameters from action
				OpRect rect;
				action->GetActionPosition(rect);

				// 0,0,0,0 rect means cursor pos, unless action was keyboard initiated, then it is center
				PopupPlacement placement = PopupPlacement::AnchorAtCursor();
				if (rect.x || rect.y || rect.width || rect.height)
					placement = PopupPlacement::AnchorBelow(rect);
				else if (action->IsKeyboardInvoked())
					placement = PopupPlacement::CenterInWindow();

				OpAutoPtr<DesktopMenuContext> menu_context;
				int spell_session_id = 0;
				bool need_to_set_spell_session_id = false;

				if (action->GetAction() == OpInputAction::ACTION_SHOW_POPUP_MENU_WITH_MENU_CONTEXT)
				{
					menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
					need_to_set_spell_session_id = true;
					spell_session_id = menu_context->GetSpellSessionID();
				}

				OpString8 menu_name;
				menu_name.Set(action->GetActionDataString());

				DesktopMenuHandler* menu_handler = m_application->GetMenuHandler();
				if (need_to_set_spell_session_id)
					menu_handler->SetSpellSessionID(spell_session_id);
				menu_handler->ShowPopupMenu(menu_name.CStr(), placement, action->GetActionData(), action->IsKeyboardInvoked(), menu_context.get());

				return TRUE;
			}

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
		case OpInputAction::ACTION_HANDLE_DOC_CONTEXT_MENU_CLICK:
			QuickMenu::OnPopupMessageMenuItemClick(action);
			return TRUE;
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

		case OpInputAction::ACTION_EXIT:
			m_application->Exit(0 != action->GetActionData());
			return TRUE;

		case OpInputAction::ACTION_OPEN_TRANSFER:
		case OpInputAction::ACTION_OPEN_TRANSFER_FOLDER:
			{
				g_desktop_transfer_manager->OpenItemByID(action->GetActionData(), action->GetAction() == OpInputAction::ACTION_OPEN_TRANSFER_FOLDER);
				return TRUE;
			}
		case OpInputAction::ACTION_SELECT_SKIN:
			{
				g_skin_manager->SelectSkin(action->GetActionData());
				return TRUE;
			}
		case OpInputAction::ACTION_SHOW_TRANSFERPANEL_DETAILS:
			{
				if (!g_pcui->GetIntegerPref(PrefsCollectionUI::TransWinShowDetails))
					{
						g_pcui->WriteIntegerL(PrefsCollectionUI::TransWinShowDetails, TRUE);
						return TRUE;
					}
					return FALSE;
			}
		case OpInputAction::ACTION_HIDE_TRANSFERPANEL_DETAILS:
			{
				if (g_pcui->GetIntegerPref(PrefsCollectionUI::TransWinShowDetails))
					{
						g_pcui->WriteIntegerL(PrefsCollectionUI::TransWinShowDetails, FALSE);
						return TRUE;
					}
					return FALSE;
			}
		case OpInputAction::ACTION_SET_SHOW_TRANSFERWINDOW:
			{
				g_pcui->WriteIntegerL(PrefsCollectionUI::TransWinActivateOnNewTransfer, action->GetActionData());
				return TRUE;
			}
		case OpInputAction::ACTION_SHOW_NEW_TRANSFERS_ON_TOP:
			{
				if (!g_pcui->GetIntegerPref(PrefsCollectionUI::TransferItemsAddedOnTop))
					{
						g_pcui->WriteIntegerL(PrefsCollectionUI::TransferItemsAddedOnTop, TRUE);
						return TRUE;
					}
					return FALSE;
			}
		case OpInputAction::ACTION_SHOW_NEW_TRANSFERS_ON_BOTTOM:
			{
				if (g_pcui->GetIntegerPref(PrefsCollectionUI::TransferItemsAddedOnTop))
					{
						g_pcui->WriteIntegerL(PrefsCollectionUI::TransferItemsAddedOnTop, FALSE);
						return TRUE;
					}
					return FALSE;
			}
		case OpInputAction::ACTION_CHECK_FOR_UPDATE:
			{
				// Only do this at all if it's not disabled
				if (!g_pcui->GetIntegerPref(PrefsCollectionUI::DisableOperaPackageAutoUpdate))
				{
					CheckForUpdateOnlineModeCallback* callback = OP_NEW(CheckForUpdateOnlineModeCallback, ());
					if (callback)
						m_application->AskEnterOnlineMode(TRUE, callback, Str::D_OFFLINE_MODE_TOGGLE_QUESTION, Str::D_OFFLINE_MODE_TOGGLE_HEADER);
				}
				return TRUE;
			}
#ifdef AUTO_UPDATE_SUPPORT
		case OpInputAction::ACTION_RESTORE_AUTO_UPDATE_DIALOG:
		case OpInputAction::ACTION_RESTART_OPERA:
			{
				return g_autoupdate_manager->OnInputAction(action);
			}
#endif
		case OpInputAction::ACTION_OPEN_DOCUMENT:
			{
				if (!m_chooser)
					RETURN_VALUE_IF_ERROR(DesktopFileChooser::Create(&m_chooser), TRUE);

				OpenDocumentsSelectionListener *selection_listener = OP_NEW(OpenDocumentsSelectionListener, (*m_application));
				if (selection_listener)
				{
					m_chooser_listener = selection_listener;
					DesktopFileChooserRequest& request = selection_listener->GetRequest();
					request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN_MULTI;
					OpString tmp_storage;
					request.initial_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_OPEN_FOLDER, tmp_storage));
					request.fixed_filter = TRUE;
					OpString filter;
					OP_STATUS rc = g_languageManager->GetString(Str::SI_OPEN_FILE_TYPES, filter);
					if (OpStatus::IsSuccess(rc) && filter.HasContent())
					{
						StringFilterToExtensionFilter(filter.CStr(), &request.extension_filters);
					}
					DesktopWindow* parent = m_application->GetActiveDesktopWindow(FALSE);
					OpStatus::Ignore(m_chooser->Execute(parent ? parent->GetOpWindow() : 0, selection_listener, request));
				}

				return TRUE;
			}
		case OpInputAction::ACTION_USE_IMAGE_AS_DESKTOP_BACKGROUND:
			{
				if (action->GetActionData() == 0)
					return FALSE;

				DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (menu_context->GetDocumentContext() == NULL)
					return FALSE;

				if (!menu_context->GetDocumentContext()->HasImage())
					return FALSE;

				OpString img_address;
				RETURN_VALUE_IF_ERROR(menu_context->GetDocumentContext()->GetImageAddress(&img_address), FALSE);
				URL_CONTEXT_ID url_context_id = menu_context->GetDocumentContext()->GetImageAddressContext();

				if (!img_address.HasContent())
					return FALSE;

				URL url = g_url_api->GetURL(img_address, url_context_id);
				OpStatus::Ignore(DesktopPiUtil::SetDesktopBackgroundImage(url));
				return TRUE;
			}
		case OpInputAction::ACTION_USE_BACKGROUND_IMAGE_AS_DESKTOP_BACKGROUND:
			{
				if (action->GetActionData() == 0)
					return FALSE;

				DesktopMenuContext* menu_context = reinterpret_cast<DesktopMenuContext*>(action->GetActionData());
				if (menu_context->GetDocumentContext() == NULL)
					return FALSE;

				if (!menu_context->GetDocumentContext()->HasBGImage())
					return FALSE;

				OpString img_address;
				RETURN_VALUE_IF_ERROR(menu_context->GetDocumentContext()->GetBGImageAddress(&img_address), FALSE);
				URL_CONTEXT_ID url_context_id = menu_context->GetDocumentContext()->GetBGImageAddressContext();

				if (!img_address.HasContent())
					return FALSE;

				URL url = g_url_api->GetURL(img_address, url_context_id);
				OpStatus::Ignore(DesktopPiUtil::SetDesktopBackgroundImage(url));
				return TRUE;
			}
#if defined (GENERIC_PRINTING)
		case OpInputAction::ACTION_PRINT_DOCUMENT:
			{
				DesktopWindow* active_desktop_window = m_application->GetActiveDesktopWindow(FALSE);
#ifdef WIC_CAP_USER_PRINT
				if (active_desktop_window && active_desktop_window->GetWindowCommander())
				{
					active_desktop_window->GetWindowCommander()->UserPrint();
					return TRUE;
				}
#else // !WIC_CAP_USER_PRINT
				if (active_desktop_window)
				{
					active_desktop_window->GetWindowCommander()->StartPrinting();
					return TRUE;
				}
#endif // !WIC_CAP_USER_PRINT
			}
			break;
#endif

		case OpInputAction::ACTION_SHOW_PREFERENCES:
			{
				// ActionData		Number of the page that should be opened on default
				// ActionDataString	Whether the page specified in ActionData should be the only one visible
				const BOOL hide_other_pages = action->GetActionDataString() ? *action->GetActionDataString() : FALSE;
				if (action->GetActionData() != 100)
				{
					NewPreferencesDialog* dialog = OP_NEW(NewPreferencesDialog, ());
					if (dialog)
						dialog->Init(m_application->GetActiveDesktopWindow(), action->GetActionData(), hide_other_pages );
				}
#if 0 // should be reenabled when new prefs are reactivated
				else
				{
					PreferencesDialog* dialog = OP_NEW(PreferencesDialog, ());
					if (dialog)
						dialog->Init(m_application->GetActiveDesktopWindow(), action->GetActionData(), hide_other_pages );
				}
#endif
				return TRUE;
			}
			break;

		case OpInputAction::ACTION_REPORT_SITE_PROBLEM:
			{
				if (m_application->GetActiveDocumentDesktopWindow())
				{
					ReportSiteProblemDialog* dialog = OP_NEW(ReportSiteProblemDialog, ());
					if (dialog)
						dialog->Init(m_application->GetActiveDocumentDesktopWindow());
				}
				return TRUE;
			}

		case OpInputAction::ACTION_SHOW_WEB_SEARCH:
			{
				WebSearchDialog::Create(m_application->GetActiveDesktopWindow());
				return TRUE;
			}

		case OpInputAction::ACTION_ENABLE_INLINE_FIND:
		case OpInputAction::ACTION_DISABLE_INLINE_FIND:
			{
				return PrefsUtils::SetPrefsToggleByAction(action, OpInputAction::ACTION_ENABLE_INLINE_FIND, g_pcui, PrefsCollectionUI::UseIntegratedSearch);
			}

		case OpInputAction::ACTION_CLEAR_DISK_CACHE:
			{
				g_url_api->CleanUp(TRUE);
				g_url_api->PurgeData(FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE);
				return TRUE;
			}

		case OpInputAction::ACTION_CLEAR_TYPED_IN_HISTORY:
			{
				g_directHistory->DeleteAllItems(); // Empties file on disk as well
				return TRUE;
			}

		case OpInputAction::ACTION_CLEAR_VISITED_HISTORY:
			{
				DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

				if(history_model)
				{
					history_model->DeleteAllItems(); // Empties file on disk as well
#ifdef MSWIN
					g_main_message_handler->PostMessage(MSG_WIN_RECREATE_JUMPLIST, 0, 0);
#endif //MSWIN
				}

				return TRUE;
			}

		case OpInputAction::ACTION_CLEAR_PREVIOUS_SEARCHES:
			{
				g_search_field_history->DeleteAllItems();
				OpStatus::Ignore(g_search_field_history->Write());
				return TRUE;
			}

		case OpInputAction::ACTION_SET_PREFERENCE:
			{
				// accepts string of format "User Prefs|User Javascript=1"

				OpString8 data;
				data.Set(action->GetActionDataString());
				int sectionlen = data.FindFirstOf('|');
				int keylen = data.FindFirstOf('=');

				if (sectionlen != KNotFound && sectionlen < data.Length() &&
					keylen != KNotFound && keylen > sectionlen && keylen < data.Length())
				{
					data.CStr()[sectionlen] = 0;
					data.CStr()[keylen] = 0;
					const OpStringC value(action->GetActionDataString()+keylen+1);

					INT32 state = g_input_manager->GetActionState(
							action, this);
					if (state & OpInputAction::STATE_SELECTED)
					{
						return FALSE; // nothing to do
					}

					TRAPD(err, g_prefsManager->WritePreferenceL(data.CStr(), data.CStr()+sectionlen+1, value));
					OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?
					TRAP(err, g_prefsManager->CommitL());
					OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if err is bad ?

					// Update multimedia, fonts&colors and language:
					g_windowManager->UpdateWindows(TRUE);
					// Update zoom of open windows etc.
					g_windowManager->UpdateAllWindowDefaults(
						g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowScrollbars),
						g_pcui->GetIntegerPref(PrefsCollectionUI::ShowProgressDlg),
						FALSE,
						g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::Scale),
						g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowWinSize));

				}
				return TRUE;
			}

		case OpInputAction::ACTION_GOTO_SPEEDDIAL:
			{
				const DesktopSpeedDial* sd = g_speeddial_manager->GetSpeedDial(*action);
				if (sd != NULL && sd->GetURL().HasContent())
				{					
					if (sd->HasInternalExtensionURL())
					{
						g_desktop_extensions_manager->OpenExtensionUrl(sd->GetURL(), sd->GetExtensionWUID());
						return TRUE;
					}										
					m_application->GoToPage(sd->GetURL(), FALSE, FALSE, FALSE, NULL, (URL_CONTEXT_ID)-1, action->IsKeyboardInvoked());
					
				}
				return TRUE;
			}
			break;

#ifdef WEBSERVER_SUPPORT
/*
		case OpInputAction::ACTION_OPEN_LOCAL_UNITE_SERVICE_WINDOW:
			{
				// Try and find the unite window if it exists somewhere
				DesktopWindow * window = m_application->GetDesktopWindowCollection().GetDesktopWindowByType(WINDOW_TYPE_UNITE);

				if (window)
				{
					// If the devtools window is found then just activate it!
					window->Activate();
				}
				else
				{
					window = OP_NEW(UniteDesktopWindow, (0));
					if (window)
					{
						if (OpStatus::IsError(window->init_status))
							window->Close(TRUE);
						else
							window->Show(TRUE);
					}
				}
				return TRUE;
			}
			break;
*/
#endif // WEBSERVER_SUPPORT

#ifdef INTEGRATED_DEVTOOLS_SUPPORT
#ifdef DEVTOOLS_INTEGRATED_WINDOW
		case OpInputAction::ACTION_OPEN_DEVTOOLS_WINDOW:
#else
		case OpInputAction::ACTION_ATTACH_DEVTOOLS_WINDOW:
#endif // DEVTOOLS_INTEGRATED_WINDOW
			{
				// Try and find the devtools window if it exists somewhere
				DesktopWindow * window = m_application->GetDesktopWindowCollection().GetDesktopWindowByType(WINDOW_TYPE_DEVTOOLS);

				if (window)
				{
					// If the devtools window is found then just activate it!
					window->Activate();
					return FALSE;
				}
				else
				{
#ifdef DEVTOOLS_INTEGRATED_WINDOW
					BrowserDesktopWindow *window;
					BOOL attach = g_pcui->GetIntegerPref(PrefsCollectionUI::DevToolsIsAttached);

					if (attach)
					{
						// Make attached devtools window
						window = m_application->GetActiveBrowserDesktopWindow();

						if (window && !window->HasDevToolsWindowAttached())
						{
							window->AttachDevToolsWindow(TRUE);

							return TRUE;
						}
						return FALSE;
					}
					else
					{
						// Make an external devtools window
						if (OpStatus::IsSuccess(BrowserDesktopWindow::Construct(&window, TRUE)))
						{
							if (OpStatus::IsError(window->init_status))
								window->Close(TRUE);
							else
								window->Show(TRUE);
						}
					}
#else
					window = OP_NEW(DevToolsDesktopWindow, (0));
					if (window) {
						if (OpStatus::IsError(window->init_status))
							window->Close(TRUE);
						else
							window->Show(TRUE);
					}
#endif // DEVTOOLS_INTEGRATED_WINDOW
				}
				return TRUE;
			}

#ifdef DEVTOOLS_INTEGRATED_WINDOW
		case OpInputAction::ACTION_CLOSE_DEVTOOLS_WINDOW:
			{
				// Try and find the devtools window whereever it is!
				DesktopWindow *devtools_window = m_application->GetDesktopWindowCollection().GetDesktopWindowByType(WINDOW_TYPE_DEVTOOLS);

				// If we found a window just close it
				if (devtools_window)
				{
					devtools_window->Close(FALSE);
					return TRUE;
				}

				return FALSE;
			}

		case OpInputAction::ACTION_ATTACH_DEVTOOLS_WINDOW:
			{
				BrowserDesktopWindow *browser_window = m_application->GetActiveBrowserDesktopWindow();

				if (browser_window && !browser_window->HasDevToolsWindowAttached())
				{
					browser_window->AttachDevToolsWindow(TRUE);

					return TRUE;
				}
				return FALSE;
			}

		case OpInputAction::ACTION_DETACH_DEVTOOLS_WINDOW:
			{
				BrowserDesktopWindow *browser_window = m_application->GetActiveBrowserDesktopWindow();

				// Don't detach the devtools window if that's all there is
				if (browser_window && !browser_window->IsDevToolsOnly())
				{
					browser_window->AttachDevToolsWindow(FALSE);

					return TRUE;
				}
				return FALSE;
			}
#endif // DEVTOOLS_INTEGRATED_WINDOW
#endif // INTEGRATED_DEVTOOLS_SUPPORT
#ifdef _DEBUG
		case OpInputAction::ACTION_DEBUG:
			{
				extern void TruncateDebugFiles();

				TruncateDebugFiles();
			}
#endif
		case OpInputAction::ACTION_MANAGE_SEARCH_ENGINES:
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_PREFERENCES, NewPreferencesDialog::SearchPage);
				return TRUE;
			}
#ifdef WEB_TURBO_MODE
		case OpInputAction::ACTION_OPEN_OPERA_WEB_TURBO_DIALOG:
		{
			g_opera_turbo_manager->ShowNotificationDialog(m_application->GetActiveBrowserDesktopWindow());
			return TRUE;
		}

		case OpInputAction::ACTION_ENABLE_OPERA_WEB_TURBO:
		case OpInputAction::ACTION_DISABLE_OPERA_WEB_TURBO:
		{
			bool enable = action->GetAction() == OpInputAction::ACTION_ENABLE_OPERA_WEB_TURBO;
			return g_opera_turbo_manager->EnableOperaTurbo(enable) ? TRUE : FALSE;
		}
		case OpInputAction::ACTION_SET_OPERA_WEB_TURBO_MODE:
		{
			int turbo_mode = action->GetActionData();
			g_opera_turbo_manager->SetOperaTurboMode(turbo_mode);
			return TRUE;
		}
#endif // WEB_TURBO_MODE
		case OpInputAction::ACTION_CLOSE_ALL_PRIVATE:
		{
			BrowserDesktopWindow* active =
					m_application->GetActiveBrowserDesktopWindow();
			if(active)
				active->LockUpdate(TRUE);

			m_application->SetLockActiveWindow(TRUE);

			// Close windows which only contains private tabs, except the active one
			OpVector<DesktopWindow> browser_windows;
			m_application->GetDesktopWindowCollection()
				.GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, browser_windows);
			for(UINT32 i=0; i<browser_windows.GetCount();i++)
			{
				BrowserDesktopWindow* window = static_cast<BrowserDesktopWindow*>(browser_windows.Get(i));
				if (window == active)
					continue;

				OpVector<DesktopWindow> document_windows;
				OpStatus::Ignore(window->GetWorkspace()->GetDesktopWindows(document_windows, WINDOW_TYPE_DOCUMENT));

				BOOL close_win = TRUE;
				for(UINT32 k = 0; close_win && k < document_windows.GetCount(); k++)
					close_win = document_windows.Get(k)->PrivacyMode();

				if(close_win)
					browser_windows.Get(i)->Close();
			}

			// private tabs
			OpVector<DesktopWindow> windows;
			m_application->GetDesktopWindowCollection()
				.GetDesktopWindows(OpTypedObject::WINDOW_TYPE_DOCUMENT, windows);
			for(UINT32 i=0; i<windows.GetCount();i++)
			{
				if(windows.Get(i)->PrivacyMode())
					windows.Get(i)->Close(TRUE, TRUE, FALSE);
			}

			// Create a new page if the active window is empty
			if(!active->GetActiveDesktopWindow())
			{
				DocumentDesktopWindow* page = 0;
				m_application->GetBrowserDesktopWindow(FALSE, FALSE,
						TRUE, &page, NULL, 0, 0, TRUE, FALSE,
						!action->GetActionData()
								&& action->IsKeyboardInvoked(),
						FALSE, 0, TRUE);
			}

			m_application->SetLockActiveWindow(FALSE);
			if(active)
				active->LockUpdate(FALSE);

			return TRUE;					
		}
		case OpInputAction::ACTION_DELETE_LOCAL_STORAGE:
		{
			g_windowCommanderManager->DeleteAllApplicationCaches();
			PersistentStorageCommander::GetInstance()->DeleteAllData(PersistentStorageCommander::ALL_CONTEXT_IDS);
			
			return TRUE;
		}

		case OpInputAction::ACTION_ENABLE_FULL_URL_IN_ADDRESS_FIELD:
		case OpInputAction::ACTION_DISABLE_FULL_URL_IN_ADDRESS_FIELD:
		{
			if (g_pcui->GetIntegerPref(PrefsCollectionUI::ShowFullURL) ==
				(action->GetAction() == OpInputAction::ACTION_ENABLE_FULL_URL_IN_ADDRESS_FIELD))
				return FALSE;

			TRAPD(err,g_pcui->WriteIntegerL(PrefsCollectionUI::ShowFullURL,
					action->GetAction() == OpInputAction::ACTION_ENABLE_FULL_URL_IN_ADDRESS_FIELD));
			return TRUE;
		}
		case OpInputAction::ACTION_MANAGE_PROXY_EXCEPTION_LIST:
		{
			ProxyExceptionDialog* dialog = OP_NEW(ProxyExceptionDialog, ());
			if (dialog)
				dialog->Init(m_application->GetActiveDesktopWindow());
		}
		case OpInputAction::ACTION_PLUGIN_INSTALL_SHOW_DIALOG:
			{
				OpString mime_type;
				RETURN_VALUE_IF_ERROR(mime_type.Set(action->GetActionDataString()), FALSE);
				g_plugin_install_manager->Notify(PIM_INSTALL_DIALOG_REQUESTED, mime_type);
				return TRUE;
			}
		case OpInputAction::ACTION_OPEN_FOLDER:
			{
				OpString file_path;
				RETURN_VALUE_IF_ERROR(file_path.Set(action->GetActionDataString()), FALSE);
				g_desktop_op_system_info->OpenFileFolder(file_path);
				return TRUE;
			}
	}

	// Forward some actions to browser window, when just closed a popup window
	// the context may goes to this class instead of the browser, in which case
	// ctrl+tab won't work. see DSK-317784
	switch(action->GetAction())
	{
		case OpInputAction::ACTION_GO_TO_CYCLER_PAGE:
		case OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE:
		case OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE:
		case OpInputAction::ACTION_CLOSE_CYCLER:
		{
			BrowserDesktopWindow* browser_window = m_application->GetActiveBrowserDesktopWindow();
			if (browser_window != NULL && !browser_window->IsInputDisabled())
				return browser_window->OnInputAction(action);
		}
	}

	return FALSE;
}


OpMultimediaPlayer*	ClassicGlobalUiContext::GetMultimediaPlayer()
{
	if (NULL == m_multimedia_player)
	{
		OpMultimediaPlayer::Create(&m_multimedia_player);
	}

	return m_multimedia_player;
}


OpWindowCommander* ClassicGlobalUiContext::GetWindowCommander()
{
	OpWindowCommander* commander;

	DocumentDesktopWindow *active_deskwin =
			m_application->GetActiveDocumentDesktopWindow();
	if (!active_deskwin
			|| (commander = active_deskwin->GetWindowCommander()) == NULL)
	{
		MailDesktopWindow *active_mailwin =
				m_application->GetActiveMailDesktopWindow();
		if (!active_mailwin
				|| (commander = active_mailwin->GetWindowCommander()) == NULL)
		{
#ifdef IRC_SUPPORT
			ChatDesktopWindow *active_chatwin = GetActiveChatDesktopWindow();
			commander = active_chatwin
					? active_chatwin->GetWindowCommander()
					: NULL;
#endif // IRC_SUPPORT
		}
	}

	return commander;
}


ChatDesktopWindow* ClassicGlobalUiContext::GetActiveChatDesktopWindow()
{
#ifdef IRC_SUPPORT
	DesktopWindow* top_level_desktop_window =
			m_application->GetActiveDesktopWindow();
	if (NULL != top_level_desktop_window
			&& top_level_desktop_window->GetType() == WINDOW_TYPE_CHAT_ROOM)
	{
		return static_cast<ChatDesktopWindow*>(top_level_desktop_window);
	}

	BrowserDesktopWindow* browser =
			m_application->GetActiveBrowserDesktopWindow();
	if (browser)
	{
		return browser->GetActiveChatDesktopWindow();
	}
#endif // IRC_SUPPORT
	return NULL;
}


MailDesktopWindow* ClassicGlobalUiContext::GoToMailView(Message* message)
{
	if (!message)
		return NULL;
#ifdef M2_CAP_HAS_ENGINE_RECOVERY_AND_INDEX_NOT_VISIBLE
	MailDesktopWindow* mail_window = m_application->GetActiveMailDesktopWindow();

	if (mail_window && g_m2_engine->GetIndexById(mail_window->GetIndexID()) &&
		!g_m2_engine->GetIndexById(mail_window->GetIndexID())->MessageNotVisible(message->GetId()))
		return mail_window;

	BrowserDesktopWindow* browser = m_application->GetActiveBrowserDesktopWindow();
	if (browser)
	{
		OpVector<DesktopWindow> mailwindows;
		OpStatus::Ignore(browser->GetWorkspace()->GetDesktopWindows(mailwindows, WINDOW_TYPE_MAIL_VIEW));
		for (UINT32 i = 0; i < mailwindows.GetCount(); i++)
		{
			mail_window = static_cast<MailDesktopWindow*>(mailwindows.Get(i));
			if (g_m2_engine->GetIndexById(mail_window->GetIndexID()) &&
				!g_m2_engine->GetIndexById(mail_window->GetIndexID())->MessageNotVisible(message->GetId()))
			{
				mail_window->Activate();
				return mail_window;
			}
		}
	}

	// try to look in the unread index
	if (!g_m2_engine->GetIndexById(IndexTypes::UNREAD_UI)->MessageNotVisible(message->GetId()))
		return m_application->GoToMailView(IndexTypes::UNREAD_UI);
#endif // M2_CAP_HAS_ENGINE_RECOVERY_AND_INDEX_NOT_VISIBLE
	return m_application->GoToMailView(IndexTypes::FIRST_ACCOUNT + message->GetAccountId());
}
