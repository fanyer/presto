/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/ClassicApplication.h"

#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/application/BrowserAuthenticationListener.h"
#include "adjunct/quick/application/BrowserMenuHandler.h"
#include "adjunct/quick/application/BrowserUiWindowListener.h"
#include "adjunct/quick/application/ClassicGlobalUiContext.h"
#include "adjunct/quick/application/PageLoadDispatcher.h"
#include "adjunct/quick/application/SessionLoadContext.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/managers/DesktopClipboardManager.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"
#include "adjunct/desktop_pi/desktop_pi_util.h"
#include "adjunct/desktop_pi/desktop_menu_context.h"
#include "adjunct/desktop_pi/DesktopGlobalApplication.h"
#include "adjunct/desktop_pi/DesktopMultimediaPlayer.h"
#include "adjunct/desktop_pi/DesktopOpSystemInfo.h"
#include "adjunct/desktop_pi/DesktopPopupMenuHandler.h"
#include "adjunct/desktop_pi/DesktopWindowManager.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/settings/DesktopSettings.h"
#include "adjunct/desktop_util/string/i18n.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"
#include "adjunct/desktop_util/skin/SkinUtils.h"
#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/autoupdate/autoupdater.h"
#endif // AUTO_UPDATE_SUPPORT
# ifdef DU_REMOTE_URL_HANDLER
#include "adjunct/desktop_util/handlers/RemoteURLHandler.h"
# endif //DU_REMOTE_URL_HANDLER
#include "adjunct/desktop_util/handlers/DownloadManager.h"
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/desktop_util/sessions/opsessionmanager.h"
#include "adjunct/desktop_util/sessions/SessionAutoSaveManager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/opera_auth/opera_auth_myopera.h"
#include "adjunct/quick/dialogs/ConfirmExitDialog.h"
#include "adjunct/quick/dialogs/CustomizeToolbarDialog.h"
#include "adjunct/quick/dialogs/PreferencesDialog.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/AnimationManager.h"
#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/quick/managers/AutoUpdateManager.h"
#endif // AUTO_UPDATE_SUPPORT
#include "adjunct/quick/managers/ManagerHolder.h"
#include "adjunct/quick/managers/CommandLineManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/DesktopTransferManager.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/FeatureManager.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick/managers/SpeedDialManager.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/managers/WebmailManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/menus/QuickMenu.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "adjunct/quick/widgets/DocumentViewGenerator.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/windows/DevToolsDesktopWindow.h"
#include "adjunct/quick/windows/SourceDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"

#include "modules/display/fontcache.h"
#include "modules/doc/doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/locale/src/opprefsfilelanguagemanager.h"
#include "modules/pi/OpView.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpFont.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_sync.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/prefs/prefsmanager/prefsnotifier.h"
#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsfile.h"
#ifdef SCOPE_SUPPORT
#include "modules/scope/src/scope_manager.h"
#endif // SCOPE_SUPPORT
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
#include "modules/scope/src/scope_window_manager.h"
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
#include "modules/security_manager/include/security_manager.h"
#ifdef SELFTEST
#include "modules/selftest/optestsuite.h"
#endif
#include "modules/skin/OpSkinManager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/OpLineParser.h"
#include "modules/util/timecache.h"
#include "modules/util/winutils.h"
#include "modules/util/filefun.h"
#include "modules/util/gen_str.h"
#include "modules/util/simset.h"
#include "modules/util/handy.h" // For UpdateOfflineModeL()
#include "modules/util/str.h"
#include "modules/widgets/WidgetContainer.h"

#include "adjunct/quick/data_types/OpenURLSetting.h"

#include "adjunct/quick/windows/PanelDesktopWindow.h"

#include "modules/windowcommander/src/WindowCommander.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"

#ifdef MSWIN
# include "platforms/windows/windows_ui/winshell.h"
#endif // MSWIN

#ifdef M2_SUPPORT
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/m2_ui/windows/ComposeDesktopWindow.h"
#include "adjunct/m2_ui/dialogs/AccountPropertiesDialog.h"
#include "adjunct/m2_ui/dialogs/AccountSubscriptionDialog.h"
#include "adjunct/m2_ui/dialogs/MailStoreUpdateDialog.h"
#include "adjunct/m2_ui/dialogs/NewAccountWizard.h"
#include "adjunct/m2_ui/dialogs/NewsfeedSubscribeDialog.h"
#include "adjunct/m2_ui/windows/ChatDesktopWindow.h"
#endif // M2_SUPPORT

#ifdef WIDGET_RUNTIME_SUPPORT
#include "adjunct/widgetruntime/GadgetConfirmWandDialog.h"
#endif //WIDGET_RUNTIME_SUPPORT

#ifdef SUPPORT_DATA_SYNC
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_factory.h"
#include "modules/sync/sync_transport.h"
#include "modules/sync/sync_parser.h"
#endif // SUPPORT_DATA_SYNC

#ifdef FEATURE_UI_TEST
# include "adjunct/ui_test_framework/OpUiCrawler.h"
#endif // FEATURE_UI_TEST

#if defined(MSWIN)
# include "platforms/windows/windows_ui/res/#buildnr.rci"
#elif defined(UNIX)
# include "platforms/unix/product/config.h"
#elif defined(_MACINTOSH_)
# include "platforms/mac/Resources/buildnum.h"
#elif defined(WIN_CE)
# include "platforms/win_ce/res/buildnum.h"
#elif !defined (VER_BUILD_NUMBER)
#  error "include the file where VER_BUILD_NUMBER is defined"
#endif

// Application* g_application = NULL;
// changed into macro, look @  Application.h file

#ifdef WAND_SUPPORT
extern BOOL InputCorrectPassword(OpWindow *parent_window);
#endif // WAND_SUPPORT

#if defined(_UNIX_DESKTOP_)
# include "platforms/viewix/FileHandlerManager.h"
# include "platforms/unix/base/common/applicationlauncher.h"
#endif

#if defined(_MACINTOSH_)
#include "platforms/mac/util/macutils.h"
#include "platforms/mac/File/FileUtils_Mac.h"
#endif

#if defined (GENERIC_PRINTING)
# include "modules/hardcore/mh/messages.h"
#endif

#include "adjunct/quick/windows/DesktopGadget.h"
#include "adjunct/quick/dialogs/WandMatchDialog.h"

#ifdef ENABLE_USAGE_REPORT
# include "adjunct/quick/usagereport/UsageReport.h"
#endif

// Function to make sure everything compiles with libgogi present and vegaoppainter disabled
OpPainter* get_op_painter()
{
	return NULL;
}

/***********************************************************************************
**
**  MailRecoveryWarningController
**
***********************************************************************************/
#ifdef M2_SUPPORT
class MailRecoveryWarningController : public SimpleDialogController
{
public:

	MailRecoveryWarningController() : SimpleDialogController(TYPE_YES_NO, IMAGE_QUESTION,WINDOW_NAME_MAIL_RECOVERY_WARNING, 
			Str::D_MAIL_MAINTENANCE_ON_EXIT_MESSAGE,Str::D_MAIL_MAINTENANCE_ON_EXIT_TITLE){}

    virtual void OnOk()
    {
        // we do not want to force exit, because there might be other dialogs that need to pop up
        // this dialog will not show again
        g_m2_engine->SetUserWantsToDoRecovery(TRUE);
        g_m2_engine->SetAskedAboutDoingMailRecovery(TRUE);
        g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT,1);

		SimpleDialogController::OnOk();
    }

    virtual void OnCancel()
    {
        g_m2_engine->SetUserWantsToDoRecovery(FALSE);
        g_m2_engine->SetAskedAboutDoingMailRecovery(TRUE);
        g_input_manager->InvokeAction(OpInputAction::ACTION_EXIT,1);

		SimpleDialogController::OnCancel();
    }
};
#endif //M2_SUPPORT

#ifdef WAND_ECOMMERCE_SUPPORT
/***********************************************************************************
**
**	ConfirmWandECommerceDialog
**
***********************************************************************************/

class ConfirmWandECommerceDialog : public Dialog
{
public:
	ConfirmWandECommerceDialog(WAND_ACTION store_action) : m_store_action(store_action) {}

	OP_STATUS Init(DesktopWindow* parent_window, WandInfo* info)
	{
		m_info = info;
		OpWindowCommander* win_comm = info->GetOpWindowCommander();
		OpWindow *window = win_comm->GetOpWindow();
		OpBrowserView* pw = static_cast<DesktopOpWindow*>(window)->GetBrowserView();
		if (pw)
			parent_window = pw->GetParentDesktopWindow();
		Dialog::Init(parent_window, 0, 0, pw);

		OpString title;
		g_languageManager->GetString(Str::S_EDIT_WAND_CAPTION_STR, title);
		SetTitle(title.CStr());
		return OpStatus::OK;
	}
#ifdef VEGA_OPPAINTER_SUPPORT
	virtual BOOL GetModality() {return FALSE;}
	virtual BOOL GetOverlayed() {return TRUE;}
#endif
	UINT32 OnOk()
	{
		m_info->ReportAction(m_store_action, WAND_STORE_ECOMMERCE_VALUES);
		return 1;
	}
	void OnCancel()
	{
		m_info->ReportAction(m_store_action);
	}
	void OnChange(OpWidget *widget, BOOL changed_by_mouse)
	{
		Dialog::OnChange(widget, changed_by_mouse);
	}

	const char*	GetWindowName()			{ return "Wand Ecom Dialog";}
	DialogType	GetDialogType()			{ return TYPE_OK_CANCEL; }
private:
	WandInfo* m_info;
	WAND_ACTION m_store_action;
};

#endif // WAND_ECOMMERCE_SUPPORT


class MailInitErrorController : public SimpleDialogController
{
public:
		
		MailInitErrorController(const OpStringC8 *status_msg):SimpleDialogController(TYPE_OK_CANCEL, IMAGE_ERROR,
			WINDOW_NAME_MAIL_INIT_ERROR,Str::NOT_A_STRING,Str::D_MAIL_PROBLEM_INITIALIZING_TITLE),m_status_msg(status_msg)
		{}

private:
		virtual void InitL()
		{
			SimpleDialogController::InitL();
			LEAVE_IF_ERROR(m_cancel_button->SetText(Str::DI_IDHELP));

			if (m_status_msg && m_status_msg->HasContent())
			{
				ANCHORD(OpString, status);
				ANCHORD(OpString, message);
				g_languageManager->GetStringL(Str::D_MAIL_PROBLEM_INITIALIZING_TEXT, message);
				message.AppendL(UNI_L("\n\n"));
				status.SetFromUTF8L(m_status_msg->CStr());
				message.AppendL(status.CStr());
				LEAVE_IF_ERROR(SetMessage(message));
			}
		}

		virtual void OnCancel()
		{	
			g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_HELP, false, UNI_L("http://www.opera.com/support/search/view/889/")); //placeholder link
			SimpleDialogController::OnCancel();
		}

		const OpStringC8*  m_status_msg;
};



/***********************************************************************************
**
**	DeleteAccountController
**
***********************************************************************************/

#ifdef M2_SUPPORT
class DeleteAccountController : public SimpleDialogController
{
	public:
		
		DeleteAccountController(UINT32 id, BOOL delete_local):
		  SimpleDialogController(TYPE_OK_CANCEL, IMAGE_WARNING,WINDOW_NAME_DELETE_ACCONT,Str::NOT_A_STRING,Str::S_REMOVE_ACCOUNT),
		  m_id(id),
		  m_delete_local(delete_local)
		  {}

		virtual void InitL()
		{
			SimpleDialogController::InitL();

			ANCHORD(OpString, tmp_message);
			ANCHORD(OpString, message);

			if(m_delete_local)
			{
				g_languageManager->GetStringL(Str::S_REMOVE_ACCOUNT_DELETE_LOCAL_WARNING, tmp_message);
				message.AppendL(tmp_message);
				message.AppendL(" ");
			}
			g_languageManager->GetStringL(Str::S_REMOVE_ACCOUNT_WARNING, tmp_message);
			message.AppendL(tmp_message);
			LEAVE_IF_ERROR(SetMessage(message));	
		}
		
		virtual void OnOk()
		{
			AccountManager* manager = g_m2_engine->GetAccountManager();

			if (!manager)
				return;

			OpStatus::Ignore(manager->RemoveAccount(m_id));

			g_application->SettingsChanged(SETTINGS_ACCOUNT_SELECTOR);
			g_application->SettingsChanged(SETTINGS_MENU_SETUP);
			g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);

			SimpleDialogController::OnOk();			
		}

		UINT32	m_id;
		BOOL	m_delete_local;
};
#endif

class AccountNeededDialogController : public SimpleDialogController
{
public:
	AccountNeededDialogController(AccountTypes::AccountType type, DesktopWindow* parent_window)
		: SimpleDialogController(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_QUESTION,
			WINDOW_NAME_YES_NO,Str::D_ACCOUNT_REQUIRED_DIALOG_TEXT, Str::D_ACCOUNT_REQUIRED_DIALOG_HEADER)
		, m_type(type)
		, m_parent_window(parent_window)
	{}

	virtual void OnOk()
	{
		g_application->CreateAccount(m_type, NULL, m_parent_window);
	}

private:
	AccountTypes::AccountType m_type;
	DesktopWindow* m_parent_window;
};


class EnterOnlineModeDialogController : public SimpleDialogController
{
public:
	EnterOnlineModeDialogController(Application::OnlineModeCallback* callback, Str::LocaleString message, Str::LocaleString title)
		: SimpleDialogController(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_QUESTION, WINDOW_NAME_ASK_ENTER_ONLINE, message, title)
		, m_callback(callback)
	{}

	virtual void OnOk()
	{
		OP_BOOLEAN is_offline = static_cast<ClassicApplication*>(g_application)->UpdateOfflineMode(FALSE);
		if (is_offline == OpBoolean::IS_TRUE)
		{
			// End offline mode if still on. We may have a number of
			// requests pending and we may already be in online mode.
			static_cast<ClassicApplication*>(g_application)->UpdateOfflineMode(TRUE);
		}
		if (m_callback.get())
			m_callback->OnOnlineMode();
	}

private:
	OpAutoPtr<Application::OnlineModeCallback> m_callback;
};


#ifdef WIC_USE_ANCHOR_SPECIAL
/***********************************************************************************
 *
 *  AnchorSpecialManager::AnchorSpecial
 *
 ************************************************************************************/

BOOL
AnchorSpecialManager::AnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info)
{
	if (KioskManager::GetInstance()->GetEnabled())
		return FALSE;

	for (UINT32 i = 0; i < m_handlers.GetCount(); i++)
	{
		AnchorSpecialHandler * handler = m_handlers.Get(i);
		if (handler->HandlesAnchorSpecial(commander, info))
		{
			return handler->AnchorSpecial(commander, info);
		}
	}

	// TODO: put that into a AnchorSpecialHandler class
#ifdef SUPPORT_MAIN_BAR
	BrowserDesktopWindow* browser = g_application->GetActiveBrowserDesktopWindow();
	if (browser && browser->GetMainBar()->AddButtonFromURL(info.GetURLName(), info.GetTitle()))
		return TRUE;
	else
#endif // SUPPORT_MAIN_BAR
		return FALSE;
}

#endif // WIC_USE_ANCHOR_SPECIAL


/***********************************************************************************
**
**	Application
**
***********************************************************************************/

ClassicApplication::ClassicApplication(OpStartupSequence& browser_startup_sequence) :
	m_active_toplevel_desktop_window(NULL),
	m_popup_desktop_window(NULL),
	m_is_showing_opera(TRUE),
	m_activated(FALSE),
	m_lock_active_window(FALSE),
    m_hack_open_next_tab_as_window(FALSE),
	m_hack_open_next_tab_as_private(FALSE),
	m_settings_changed(NULL),
	m_settings_changed_message_pending(FALSE),
	m_customize_toolbars_dialog(NULL),
	m_captionlength(0),
	m_confirm_online_dialog(FALSE),
	m_last_seen_document_context(NULL)
	, m_boot_manager(*this, browser_startup_sequence)
	, m_ui_context(NULL)
	, m_menu_handler(NULL)
	, m_finalized(FALSE)
#ifdef GADGET_UPDATE_SUPPORT
	, m_extension_updater(NULL)
#endif //GADGET_UPDATE_SUPPORT
{
}

OP_STATUS ClassicApplication::Init()
{
	// abort on OOM; if this fails for any other reason Opera will just use
	// country code reported by OS
	RETURN_IF_MEMORY_ERROR(m_boot_manager.StartCountryCheckIfNeeded());

	m_dispatcher = OP_NEW(PageLoadDispatcher, ());
	RETURN_OOM_IF_NULL(m_dispatcher);
	RETURN_IF_ERROR(m_dispatcher->Init());

	OpAutoPtr<OpToolTip> tooltip(OP_NEW(OpToolTip, ()));
	RETURN_OOM_IF_NULL(tooltip.get());
	RETURN_IF_ERROR(tooltip->Init());

	OpAutoPtr<BrowserApplicationCacheListener> cache_listener(OP_NEW(BrowserApplicationCacheListener, ()));
	RETURN_OOM_IF_NULL(cache_listener.get());

#ifdef GADGET_UPDATE_SUPPORT
	OpAutoPtr<ExtensionUpdater> extension_updater(OP_NEW(ExtensionUpdater, (m_update_controller)));
	RETURN_OOM_IF_NULL(extension_updater.get());
#endif //GADGET_UPDATE_SUPPORT

	RETURN_IF_LEAVE(g_pcui->RegisterListenerL(this));

	OpMessage msglist[] = {
		MSG_SETTINGS_CHANGED,
		MSG_GETPAGE,
		MSG_WINDOW_DEACTIVATED,
		MSG_QUICK_MAIL_PROBLEM_INITIALIZING,
		MSG_QUICK_DELAYED_ACTION,
		MSG_QUICK_UNLOCK_SESSION_WRITING,
		MSG_QUICK_RELOAD_SKIN
	};
	RETURN_IF_ERROR(g_main_message_handler->SetCallBackList(this, 0, msglist, ARRAY_SIZE(msglist)));

	OpAutoPtr<ClassicGlobalUiContext> ui_context(OP_NEW(ClassicGlobalUiContext, (*this)));
	RETURN_OOM_IF_NULL(ui_context.get());

	OpAutoPtr<BrowserMenuHandler> menu_handler(
			OP_NEW(BrowserMenuHandler, (*this, *ui_context, m_spell_check_context)));
	RETURN_OOM_IF_NULL(menu_handler.get());

	RETURN_IF_ERROR(AddSettingsListener(menu_handler.get()));

	OpAutoPtr<SessionLoadContext> auto_session_load_context(OP_NEW(SessionLoadContext, (*this)));
	RETURN_OOM_IF_NULL(auto_session_load_context.get());

	m_tooltip = tooltip.release();
	m_application_cache_listener = cache_listener.release();
	m_ui_context = ui_context.release();
	m_menu_handler = menu_handler.release();
	SessionLoadContext* session_load_context = auto_session_load_context.release();
#ifdef GADGET_UPDATE_SUPPORT
	m_extension_updater = extension_updater.release();
	g_windowCommanderManager->SetGadgetListener(&m_update_controller);
#endif //GADGET_UPDATE_SUPPORT

	g_input_manager->SetKeyboardInputContext(m_ui_context, FOCUS_REASON_OTHER);
	m_ui_context->SetParentInputContext(&m_extension_context);
	m_extension_context.SetParentInputContext(&m_spell_check_context);
	m_spell_check_context.SetParentInputContext(session_load_context);

	g_session_manager->SetSessionLoadContext(session_load_context);
	g_wand_manager->AddListener(this);

	return DocumentViewGenerator::InitDocumentViewGenerator();
}

ClassicApplication::~ClassicApplication()
{
	OP_ASSERT(m_finalized
			|| !"We're being destroyed yet we don't know we're exiting");

	OP_DELETE(m_settings_changed);
}

void ClassicApplication::Finalize()
{
    // Set this to true just to make sure that window sessions are not saved during the shutdown
	m_boot_manager.SetIsExiting();

	g_main_message_handler->UnsetCallBacks(this);

	// No wand listener in Kiosk mode
	if (!KioskManager::GetInstance()->GetEnabled())
		g_wand_manager->RemoveListener(this);

	// Stop listening to pref changes
	g_pcui->UnregisterListener(this);

#ifdef GADGET_SUPPORT
	//FIXME hide the gadgets (if shown)
#endif

	if (g_desktop_bookmark_manager)
		OpStatus::Ignore(g_desktop_bookmark_manager->SaveDirtyModelToDisk());

	// Close all DesktopWindows.
	m_window_collection.GentleClose();
	// Close all notifications (they are input contexts, too, and we're about
	// to delete the global input context).
	if (g_notification_manager)
		g_notification_manager->CancelAllNotifications();

	RemoveSettingsListener(m_menu_handler);
	OP_DELETE(m_menu_handler);
	m_menu_handler = NULL;

	m_ui_context->SetParentInputContext(NULL);
	OP_DELETE(m_ui_context);
	m_ui_context = NULL;

	m_extension_context.SetParentInputContext(NULL);

	m_spell_check_context.SetParentInputContext(NULL);

	SessionLoadContext* session_load_context = g_session_manager->GetSessionLoadContext();
	if (session_load_context)
	{
		g_session_manager->SetSessionLoadContext(NULL);
		session_load_context->SetParentInputContext(NULL);
		OP_DELETE(session_load_context);
	}

#ifdef ENABLE_USAGE_REPORT
	OP_DELETE(g_usage_report_manager);
	g_usage_report_manager = 0;
#endif

	OP_DELETE(m_tooltip);
	m_tooltip = NULL;

	OP_DELETE(m_application_cache_listener);
	m_application_cache_listener = NULL;

	// Say that we aren't running anymore
	OP_STATUS err = m_boot_manager.SetOperaRunning(FALSE);
	// We're shutting down anyway, so we don't really care if we get
	// an OOM here, there's not much we can do about it.
	OpStatus::Ignore(err);

	DownloadManager::DestroyManager();

#ifdef GADGET_UPDATE_SUPPORT
	g_windowCommanderManager->SetGadgetListener(NULL);

	OP_DELETE(m_extension_updater);
	m_extension_updater = NULL;
#endif //GADGET_UPDATE_SUPPORT

	OP_DELETE(m_dispatcher);
	m_dispatcher = NULL;

	m_finalized = TRUE;
}


UiContext* ClassicApplication::GetInputContext()
{
	return m_ui_context;
}


void ClassicApplication::StopListeningToWand()
{
	g_wand_manager->RemoveListener(this);
}

void ClassicApplication::PrepareKioskMode()
{
	// clear the clipboard in kiosk manager, requirement from some kiosk customers
	g_desktop_clipboard_manager->PlaceText(UNI_L(""));

	// No wand in kiosk mode! so just remove the wand listener
	StopListeningToWand();
}

/***********************************************************************************
**
**	ShowOpera
**
***********************************************************************************/

BOOL ClassicApplication::ShowOpera(BOOL show, BOOL force_activate)
{
	if (show && m_is_showing_opera && force_activate)
	{
		return g_input_manager->InvokeAction(OpInputAction::ACTION_ACTIVATE_WINDOW);
	}

	if (show == m_is_showing_opera)
		return FALSE;

	// If there is no system tray available, we can't hide, since
	// the user wouldn't be able to open it again. Minimize instead.
	if (!show && !g_desktop_op_system_info->HasSystemTray())
		m_window_collection.MinimizeAll();
	else
		m_window_collection.SetWindowsVisible(show != FALSE);

	m_is_showing_opera = show;

	if (show)
		g_desktop_global_application->OnShow();
	else
		g_desktop_global_application->OnHide();

	return FALSE;
}

#ifdef TEST_STARTUP
OP_STATUS ClassicApplication::GetGadgetsRunning(OpSession* session, OpINT32Vector& gadgets_to_start)
{
	return OpStatus::OK;
}
#endif


/***********************************************************************************
**
**	CreateSessionWindows
**
***********************************************************************************/

OP_STATUS ClassicApplication::CreateSessionWindows(OpSharedPtr<OpSession> session,
                                                   BOOL insert /* = FALSE */)
{
	RETURN_IF_ERROR(m_dispatcher->SetSession(session));
	m_dispatcher->LockInitHiddenUI();
	TRAP_AND_RETURN(err, CreateSessionWindowsL(session.get(), insert));
	return OpStatus::OK;
}

OP_STATUS ClassicApplication::CreateSessionWindowsL(OpSession* session, BOOL insert)
{
	// FIXME this method is old, tricky to use and impossible
	// to maintain. Functionality should be moved bit by bit
	// into PageLoadDispatcher, DesktopWindow implementations
	// and OpWorkspace. see DSK-355407

	if(!session)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	ANCHORD(OpINT32HashTable<DesktopWindow>, id_table);
	ANCHORD(OpVector<DesktopWindow>, desktop_windows_to_activate_list);
	ANCHORD(OpVector<BrowserDesktopWindow>,  browser_windows);

	UINT32 count = session->GetWindowCountL();
	UINT32 i;
	OpString8 msg; ANCHOR(OpString8, msg);

	if(OpStatus::IsSuccess(msg.AppendFormat("Session contains %d tabs and windows", count)))
	{
		OP_PROFILE_MSG(msg.CStr());
	}

	// We have to open in correct order. Setting restored at end of function
	BOOL saved_open_page_setting =
		g_pcui->GetIntegerPref(PrefsCollectionUI::OpenPageNextToCurrent);

	g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, FALSE);

	DesktopGroupModelItem* current_group = NULL;
	UINT32 current_group_no = 0;
	bool current_group_collapsed = false;
	for(i = 0; i < count; i++)
	{
		QuickDisableAnimationsHere dummy; ANCHOR(QuickDisableAnimationsHere, dummy);

		BOOL tab_is_skipped = FALSE; // set to TRUE if a tab is not relevant for this install, eg. mail tab when mail is disabled
		OpSessionWindow* session_window = session->GetWindowL(i);

		OpRect rect; ANCHOR(OpRect, rect);
		OP_STATUS s;
		OpWindow::State state;
		OpWindow::State restore_window_state;

		TRAP(s,	rect.x = session_window->GetValueL("x");
				rect.y = session_window->GetValueL("y");
				rect.width = session_window->GetValueL("w");
				rect.height = session_window->GetValueL("h");
				state = static_cast<OpWindow::State>(session_window->GetValueL("state"));
				restore_window_state = static_cast<OpWindow::State>(session_window->GetValueL("restore to state"));
			);
		if(OpStatus::IsError(s))
		{
			g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, saved_open_page_setting);
			LEAVE(s);
		}
		DesktopWindow* parent_window = NULL;
		OpWorkspace* workspace = NULL;

		if(!insert)
		{
			INT32 parent_id = session_window->GetValueL("parent");

			id_table.GetData(parent_id, &parent_window);

			if (parent_window && parent_window->GetType() == OpTypedObject::WINDOW_TYPE_BROWSER)
			{
				workspace = ((BrowserDesktopWindow*) parent_window)->GetWorkspace();
			}

			if(!parent_id && session->GetVersionL() < 7000)//old files don't have a parent window
			{
				workspace = GetBrowserDesktopWindow()->GetWorkspace();
			}
		}
		else //if inserting session into existing window
		{
			workspace = GetBrowserDesktopWindow()->GetWorkspace();
		}

		DesktopWindow* desktop_window = NULL;

		switch (session_window->GetTypeL())
		{
			case OpSessionWindow::BROWSER_WINDOW:
#ifdef DEVTOOLS_INTEGRATED_WINDOW
			case OpSessionWindow::DEVTOOLS_WINDOW:
#endif // DEVTOOLS_INTEGRATED_WINDOW
			{	if(!insert)
				{
					OP_PROFILE_METHOD("Created top level window");

					BrowserDesktopWindow *bw;
#ifdef DEVTOOLS_INTEGRATED_WINDOW
					if (OpStatus::IsSuccess(BrowserDesktopWindow::Construct(&bw, session_window->GetTypeL() == OpSessionWindow::DEVTOOLS_WINDOW, FALSE, TRUE)))
#else
					if (OpStatus::IsSuccess(BrowserDesktopWindow::Construct(&bw)))
#endif // DEVTOOLS_INTEGRATED_WINDOW
					{
						browser_windows.Add(bw);
						desktop_window = bw;
					}
				}
				else
					tab_is_skipped = TRUE;
				break;
			}
			case OpSessionWindow::DOCUMENT_WINDOW:
			{
				DocumentDesktopWindow::InitInfo info;
				info.m_staged_init = true;
				desktop_window = OP_NEW(DocumentDesktopWindow, (workspace, NULL, &info));
				break;
			}
#ifdef M2_SUPPORT
			case OpSessionWindow::MAIL_WINDOW:
				if(HasMail() || HasFeeds())
				{
					MailDesktopWindow *win;
					if (OpStatus::IsSuccess(MailDesktopWindow::Construct(&win, workspace)))
						desktop_window = win;
				}
				else
					tab_is_skipped = TRUE;
				break;
			case OpSessionWindow::MAIL_COMPOSE_WINDOW:
				if(HasMail())
				{
					ComposeDesktopWindow *win;
					if (OpStatus::IsSuccess(ComposeDesktopWindow::Construct(&win, workspace)))
						desktop_window = win;
				}
				else
					tab_is_skipped = TRUE;
				break;
			case OpSessionWindow::CHAT_WINDOW:
#ifdef IRC_SUPPORT
				if( ShowM2() )
				{
					desktop_window = OP_NEW(ChatDesktopWindow, (workspace));
				}
				else
#endif // IRC_SUPPORT
					tab_is_skipped = TRUE;
				break;
#endif //M2_SUPPORT
			case OpSessionWindow::PANEL_WINDOW:
				desktop_window = OP_NEW(PanelDesktopWindow, (workspace));
				break;
#ifdef GADGET_SUPPORT
			case OpSessionWindow::GADGET_WINDOW:
				//Let the hotlist manager create the gadget, and adjust the session specific window attributes (x, y etc) afterwards
				OpStringC gadget_id(session_window->GetStringL("id"));

				// Make sure that restart unite services after crash is set  if starting up after a crash
				if (gadget_id.Length() > 0
#ifdef WEBSERVER_SUPPORT
					&& (!g_pcui->GetIntegerPref(PrefsCollectionUI::Running) ||
					g_pcui->GetIntegerPref(PrefsCollectionUI::RestartUniteServicesAfterCrash))
#endif // WEBSERVER_SUPPORT
					)
				{
#ifdef WEBSERVER_SUPPORT
					// Adds all Unite gadgets that are actually running,
					// so they can be started at an appropriate time
					// Normal widgets return FALSE
					g_desktop_gadget_manager->AddAutoStartService(gadget_id);
#endif // WEBSERVER_SUPPORT
				}
				tab_is_skipped = TRUE;	// not a window
				break;
#endif // GADGET_SUPPORT
		}

		if (desktop_window)
		{
			if (OpStatus::IsError(desktop_window->init_status))
			{
				desktop_window->Close(TRUE);
				g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, saved_open_page_setting);
				LEAVE(desktop_window->init_status);
			}
			else
			{
				id_table.Add(session_window->GetValueL("id"), desktop_window);

				int locked = session_window->GetValueL("locked");
				desktop_window->SetSavePlacementOnClose(session_window->GetValueL("saveonclose"));

				OP_STATUS err = m_dispatcher->Enqueue(session_window, desktop_window);

				if(OpStatus::IsError(err))
				{
					g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, saved_open_page_setting);
					LEAVE(err);
				}
				if( m_boot_manager.HasStartupSetting() && desktop_window->GetType() == WINDOW_TYPE_BROWSER )
				{
					OP_PROFILE_METHOD("Set status and made top level window visible");

					BOOL show = TRUE;
					OpRect* r = &rect;

					// This is for std. command line support in X-windows
					m_boot_manager.GetStartupSetting( show, r, state );
					// DesktopWindow does not properly handle transformation to 
					// fullscreen in Show(). Fullscreen() does. OpBootManager
					// will trigger DesktopWindow::Fullscreen() from StartBrowser()
					// so we revert here if neccesary
					if (state == OpWindow::FULLSCREEN)
						state = OpWindow::RESTORED;
					else if (state == OpWindow::MAXIMIZED && m_boot_manager.HasGeometryStartupSetting())
						state = OpWindow::RESTORED; // Use command line geometry data instead of maximizing
					desktop_window->Show(show, r, state, FALSE, TRUE);

					desktop_window->SetLockedByUser(locked ? TRUE : FALSE);
				}
				else
				{
					BOOL show = TRUE;
#ifdef WEBSERVER_SUPPORT
					if(desktop_window->GetType() == WINDOW_TYPE_GADGET)
					{
						DesktopGadget *gadget = (DesktopGadget *)desktop_window;
						show = gadget->GetGadgetStyle() != DesktopGadget::GADGET_STYLE_HIDDEN;
					}
#endif
					if (desktop_window->GetType() != WINDOW_TYPE_DOCUMENT) // document's Show() is triggered by dispatcher at the appropriate time; see DSK-355407
						desktop_window->Show(show, &rect, state, FALSE, TRUE);
					desktop_window->GetOpWindow()->SetRestoreState(restore_window_state);
					if( session_window->GetValueL("active") )
					{
						desktop_windows_to_activate_list.Add(desktop_window);
					}
					desktop_window->SetLockedByUser(locked ? TRUE : FALSE);

					UINT32 group_no = session_window->GetValueL("group");
					if (group_no && current_group_no == group_no)
					{
						current_group_collapsed = current_group_collapsed || session_window->GetValueL("hidden");
						if (!current_group)
						{
							// Found two consecutive windows with the same group, so let's create the group
							OP_ASSERT(desktop_window->GetModelItem().GetPreviousItem());
							current_group = m_window_collection.CreateGroup(desktop_window->GetModelItem(),
									*desktop_window->GetModelItem().GetPreviousItem(), current_group_collapsed);
							if (!current_group)
								LEAVE(OpStatus::ERR_NO_MEMORY);
						}
						else
						{
							// Move to existing group
							m_window_collection.ReorderByItem(desktop_window->GetModelItem(), current_group, current_group->GetLastChildItem());
						}

						if (session_window->GetValueL("group-active"))
							current_group->SetActiveDesktopWindow(desktop_window);
					}
					else if (current_group_no != group_no)
					{
						current_group_no = group_no;
						current_group = NULL;
						current_group_collapsed = session_window->GetValueL("hidden") != 0;
					}
				}
			}
		}
		else if(!tab_is_skipped)	// check if not having a window should be considered an error
		{
			g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, saved_open_page_setting);
			LEAVE_IF_NULL(desktop_window);
		}
	}

	// Restore active doc window in each browser window.
	for(i = 0; i < desktop_windows_to_activate_list.GetCount(); i++ )
	{
		desktop_windows_to_activate_list.Get(i)->Activate();
	}

	m_dispatcher->CloseQueue();

	g_main_message_handler->PostDelayedMessage(MSG_QUICK_CONTINUE_SESSION_LOADING, 0, 0, 200);

	// Restore pref setting
	g_pcui->WriteIntegerL(PrefsCollectionUI::OpenPageNextToCurrent, saved_open_page_setting);

	return OpStatus::OK;
}

/***********************************************************************************
**
**	SettingsChanged
**
***********************************************************************************/

void ClassicApplication::SettingsChanged(SettingsType setting)
{
	if (!m_settings_changed)
		m_settings_changed = OP_NEW(DesktopSettings, ());

	if (m_settings_changed)
		m_settings_changed->Add(setting);

	if (m_settings_changed_message_pending)
		return;

	m_settings_changed_message_pending = TRUE;

	g_main_message_handler->PostMessage(MSG_SETTINGS_CHANGED, (MH_PARAM_1)this, 0);
}

/***********************************************************************************
**
**	SyncSettings
**
***********************************************************************************/

void ClassicApplication::SyncSettings()
{
	if (!m_settings_changed)
		return;

	DesktopSettings* settings = m_settings_changed;
	m_settings_changed = NULL;

	OnSettingsChanged(settings);

	for (OpListenersIterator iterator(m_settings_listeners); m_settings_listeners.HasNext(iterator);)
	{
		m_settings_listeners.GetNext(iterator)->OnSettingsChanged(settings);
	}

	OP_DELETE(settings);

	g_input_manager->UpdateAllInputStates();
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void ClassicApplication::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_SYSTEM_SKIN) || settings->IsChanged(SETTINGS_PERSONA))
	{
		g_skin_manager->Flush();
#ifdef PREFS_HAVE_REREAD_FONTS
		TRAPD(err, PrefsNotifier::OnFontChangedL());
#endif // PREFS_HAVE_REREAD_FONTS
	}
	if (settings->IsChanged(SETTINGS_KEYBOARD_SETUP) || settings->IsChanged(SETTINGS_MOUSE_SETUP))
	{
		g_input_manager->Flush();
	}
	if (settings->IsChanged(SETTINGS_MULTIMEDIA))
	{
		OpVector<DesktopWindow> windows;
		m_window_collection.GetDesktopWindows(OpTypedObject::WINDOW_TYPE_DOCUMENT, windows);
		for (UINT32 i = 0; i < windows.GetCount(); i++)
			static_cast<DocumentDesktopWindow*>(windows.Get(i))->UpdateWindowImage(TRUE, TRUE);
	}
}

/***********************************************************************************
**
**	HandleCallback
**
***********************************************************************************/

void ClassicApplication::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_SETTINGS_CHANGED)
	{
		m_settings_changed_message_pending = FALSE;
		SyncSettings();
	}
	else if(msg == MSG_GETPAGE)
	{
		Window* window = windowManager->GetWindow(par1);
		STARTUPTYPE start_type = (STARTUPTYPE)par2;
		if (window)
		{
			OP_PROFILE_CHECKPOINT_START(window->GetWindowCommander(), "Loaded session page");

			GetStartupPage(window, start_type);
		}
	}
	else if(msg == MSG_WINDOW_DEACTIVATED)
	{
		// If the keyboard input context is ourselves, then user really has activated a
		// non Opera window
		{
			if (GetInputContext() == g_input_manager->GetKeyboardInputContext())
			{
					OnActivate(FALSE);
			}
		}
	}
#ifdef M2_SUPPORT
	else if (msg == MSG_QUICK_MAIL_PROBLEM_INITIALIZING)
	{
		OpString8 * status_msg = (OpString8*)par2;

		CommandLineArgument *no_start_dialogs = CommandLineManager::GetInstance()->GetArgument(CommandLineManager::WatirTest);
		if (!no_start_dialogs)
		{
			MailInitErrorController* controller = OP_NEW(MailInitErrorController, (status_msg));		
			OpStatus::Ignore(ShowDialog(controller,g_global_ui_context,GetActiveDesktopWindow()));				
		}
		OP_DELETE(status_msg);
	}
#endif // M2_SUPPORT
	else if (msg == MSG_QUICK_DELAYED_ACTION)
	{
		OpInputAction *action = (OpInputAction *) par1;
		g_input_manager->InvokeAction(action);
		OP_DELETE(action);
	}
	else if (msg == MSG_QUICK_UNLOCK_SESSION_WRITING)
	{
		g_session_auto_save_manager->UnlockSessionWriting();
	}
	else if (msg == MSG_QUICK_RELOAD_SKIN)
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_RELOAD_SKIN);
	}
}

void ClassicApplication::OnActivate(BOOL activate)
{
    m_activated = activate;
}


/***********************************************************************************
 **
 **	AddDesktopWindow
 **
 ***********************************************************************************/

OP_STATUS ClassicApplication::AddDesktopWindow(DesktopWindow* desktop_window, DesktopWindow* parent_window)
{
	RETURN_IF_ERROR(desktop_window->AddListener(this));
	if (m_ui_context)
		RETURN_IF_ERROR(desktop_window->AddListener(m_ui_context));

	if (!desktop_window->GetParentWorkspace() && desktop_window->GetStyle() == OpWindow::STYLE_DESKTOP)
		RETURN_IF_ERROR(m_toplevel_desktop_windows.Add(desktop_window));

	return m_window_collection.AddDesktopWindow(desktop_window, parent_window);
}

OP_STATUS ClassicApplication::RemoveDesktopWindow(DesktopWindow* desktop_window)
{
	return m_dispatcher->Remove(desktop_window);
}

/***********************************************************************************
 **
 **	AddBrowserDesktopWindowListener
 **
 ***********************************************************************************/

OP_STATUS ClassicApplication::AddBrowserDesktopWindowListener(BrowserDesktopWindowListener* listener)
{
	// if we already have open browserdesktop windows, we call the listener to notify about them
	int count = m_toplevel_desktop_windows.GetCount();
	for (int i = 0; i < count; i++)
	{
		DesktopWindow* win = m_toplevel_desktop_windows.Get(i);
		if (win && win->GetType() == WINDOW_TYPE_BROWSER)
		{
			listener->OnBrowserDesktopWindowCreated(static_cast<BrowserDesktopWindow *>(win));
		}
	}
	return m_browserwindow_listeners.Add(listener);
}

/***********************************************************************************
**
**	GetBrowserDesktopWindow
**
***********************************************************************************/

BrowserDesktopWindow* ClassicApplication::GetBrowserDesktopWindow(BOOL force_new_window, BOOL force_background, BOOL create_new_page, DocumentDesktopWindow** new_page, OpWindowCommander* window_commander, INT32 width, INT32 height, BOOL show_toolbars, BOOL focus_document, BOOL ignore_modifier_keys, BOOL no_speeddial, OpenURLSetting *setting, BOOL private_browsing)
{
	if (IsEmBrowser())
	{
		return NULL;
	}

	BrowserDesktopWindow* browser = GetActiveBrowserDesktopWindow();
	BOOL single_tab = FALSE;
	BOOL behind = IsOpeningInBackgroundPreferred(ignore_modifier_keys) || force_background;

	if( IsBrowserStarted() )
	{
		// Fix for bug #178618. A manual resize will not have updated the saved size
		// which we use in DesktopWindow::Show()
		g_session_auto_save_manager->SaveDefaultStartupSize();
	}
	else if (!browser)
	{
		// Will happen if we start iconified
		browser = (BrowserDesktopWindow*) m_window_collection.GetDesktopWindowByType(WINDOW_TYPE_BROWSER);
	//	if (!browser)
	//		StartBrowser();
	}

	if (!browser || force_new_window)
	{
		RETURN_VALUE_IF_ERROR(BrowserDesktopWindow::Construct(&browser, FALSE, private_browsing), NULL);

		OpRect rect(0, 0, 0, 0);
		WinSizeState state = NORMAL;

		OpString name_str;
		name_str.Set(browser->GetWindowName());
		g_pcui->GetWindowInfo(name_str, rect, state);

		// if I'm going to create new page, I better size the invisible browser window first
		// to something sensible

		browser->SetOuterSize(rect.width, rect.height);
		BOOL show = TRUE;
		OpRect *rectptr = 0;
		OpWindow::State winstate = OpWindow::RESTORED;

		// This is for std. command line support in X-windows
		m_boot_manager.GetStartupSetting( show, rectptr, winstate );

		browser->Show(show, rectptr, winstate, behind, rectptr ? TRUE : FALSE);

		single_tab = TRUE;

#ifdef _MACINTOSH_
		create_new_page = TRUE;	// Do not allow empty workspace on mac
#endif
	}
	if (create_new_page)
	{
		if(setting)
		{
			setting->m_in_background = behind ? YES : NO;   // used when deciding tab closing behavior
		}

		QuickDisableAnimationsHere* dont_animate = NULL;
		if (single_tab)
			dont_animate = OP_NEW(QuickDisableAnimationsHere, ());

		DocumentDesktopWindow* document = CreateDocumentDesktopWindow(browser->GetWorkspace(), window_commander, width, height, show_toolbars, focus_document, behind, no_speeddial, setting, browser->PrivacyMode() || private_browsing);

		OP_DELETE(dont_animate);

		if (new_page)
		{
			*new_page = document;
		}
	}
	return browser;
}

/***********************************************************************************
**
**	GetActiveDesktopWindow
**
***********************************************************************************/

DesktopWindow* ClassicApplication::GetActiveDesktopWindow(BOOL toplevel_only)
{
	if (!toplevel_only)
	{
		if (m_active_toplevel_desktop_window && m_active_toplevel_desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
			return m_active_toplevel_desktop_window;
		BrowserDesktopWindow* browser = GetActiveBrowserDesktopWindow();
		if (browser && browser->GetActiveDesktopWindow())
			return browser->GetActiveDesktopWindow();
	}

	return m_active_toplevel_desktop_window;
}

/***********************************************************************************
**
**	GetActiveBrowserDesktopWindow
**
***********************************************************************************/
BrowserDesktopWindow* ClassicApplication::GetActiveBrowserDesktopWindow()
{
	if (m_active_toplevel_desktop_window)
	{
		OpTypedObject::Type win_type = m_active_toplevel_desktop_window->GetType();

		if (win_type == WINDOW_TYPE_BROWSER
#if defined(INTEGRATED_DEVTOOLS_SUPPORT) && defined(DEVTOOLS_INTEGRATED_WINDOW)
			&& !m_active_toplevel_desktop_window->IsDevToolsOnly()
#endif //  defined(INTEGRATED_DEVTOOLS_SUPPORT) && defined(DEVTOOLS_INTEGRATED_WINDOW)
			)
		{
			return (BrowserDesktopWindow*) m_active_toplevel_desktop_window;
		}
		else
		{
			// Special hack for other windows.

			// If another window is active and you click on a link in it,
			// this will not return a desktop window, so we need to go look for it.

			// If the devtools window has the main focus and you click a link in an
			// external program you want to open a tab in a browser window, not the devtools
			UINT32 cnt = m_toplevel_desktop_windows.GetCount();
			while (cnt)
			{
				cnt--;
				DesktopWindow *tmpwindow = m_toplevel_desktop_windows.Get(cnt);
				if(tmpwindow->GetType() == WINDOW_TYPE_BROWSER
#ifdef DEVTOOLS_INTEGRATED_WINDOW
							   && !tmpwindow->IsDevToolsOnly()
#endif // DEVTOOLS_INTEGRATED_WINDOW
				  )
				{
					return (BrowserDesktopWindow*) tmpwindow;
				}
			}
		}
	}
	return NULL;
}

/***********************************************************************************
**
**	GetActiveWindow - DO NOT USE THIS FUNCTION!!!!!
**
***********************************************************************************/

Window* ClassicApplication::GetActiveWindow()
{
	return WindowCommanderProxy::GetActiveWindowCommander() ?
		WindowCommanderProxy::GetActiveWindowCommander()->GetWindow() : NULL;
}

/***********************************************************************************
**
**	GetActiveDocumentDesktopWindow
**
***********************************************************************************/

DocumentDesktopWindow* ClassicApplication::GetActiveDocumentDesktopWindow()
{
	DocumentDesktopWindow* document_window = NULL;

	if (m_active_toplevel_desktop_window && m_active_toplevel_desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		document_window = static_cast<DocumentDesktopWindow*>(m_active_toplevel_desktop_window);
	}
	else
	{
		BrowserDesktopWindow* browser = GetActiveBrowserDesktopWindow();
		if (browser)
		{
			document_window = browser->GetActiveDocumentDesktopWindow();
		}
	}

	OP_ASSERT(document_window ? document_window->IsActive() : true);
	return document_window;
}

/***********************************************************************************
**
**	GetDesktopWindowFromAction
**
***********************************************************************************/

DesktopWindow* ClassicApplication::GetDesktopWindowFromAction(OpInputAction* action)
{
	if (!action)
		return NULL;

	OpInputContext* input_context = action->GetFirstInputContext();

	while (input_context)
	{
		if (input_context->GetType() >= WINDOW_TYPE_UNKNOWN && input_context->GetType() < DIALOG_TYPE_LAST)
		{
			return (DesktopWindow*) input_context;
		}

		input_context = input_context->GetParentInputContext();
	}

	return NULL;
}

/***********************************************************************************
**
**	GetActiveMailDesktopWindow
**
***********************************************************************************/

MailDesktopWindow* ClassicApplication::GetActiveMailDesktopWindow()
{
	if (m_active_toplevel_desktop_window && m_active_toplevel_desktop_window->GetType() == WINDOW_TYPE_MAIL_VIEW)
	{
		return (MailDesktopWindow*) m_active_toplevel_desktop_window;
	}

	BrowserDesktopWindow* browser = GetActiveBrowserDesktopWindow();

	if (browser)
	{
		return browser->GetActiveMailDesktopWindow();
	}

	return NULL;
}

BOOL ClassicApplication::KioskFilter(OpInputAction* action)
{
	return KioskManager::GetInstance() && KioskManager::GetInstance()->ActionFilter(action);
}


void ClassicApplication::RegisterActivity()
{
	if( KioskManager::GetInstance() )
		KioskManager::GetInstance()->RegisterActivity();
}

/***********************************************************************************
**
**	OpenIfSessionFile
**
***********************************************************************************/

BOOL ClassicApplication::OpenIfSessionFile( const OpString &address )
{
	// Only try to open automatically if it is a ".win" file
	int pos = address.FindLastOf('.');
	if( pos != KNotFound )
	{
		if( uni_stricmp( &address.CStr()[pos+1], UNI_L("win")) == 0 )
		{
			OpFile file;
			OP_STATUS err = file.Construct(address.CStr());
			RETURN_VALUE_IF_ERROR(err, FALSE);

			OpSharedPtr<OpSession> session(g_session_manager->CreateSessionFromFile(&file));
			err = CreateSessionWindows(session, TRUE);
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: do what if rc is bad ?
			return OpStatus::IsSuccess(err);
		}
	}
	return FALSE;
}

/***********************************************************************************
**
**	GetLocalAddress
**
***********************************************************************************/

BOOL ClassicApplication::GetLocalAddress(const OpStringC& address, OpString& local_address)
{
	// use URL to find out if the address is local
	OpString resolved_address;
	BOOL is_resolved;
	TRAPD(status, is_resolved = g_url_api->ResolveUrlNameL(address, resolved_address));
	if (OpStatus::IsSuccess(status) && is_resolved)
	{
		URL	url = g_url_api->GetURL(resolved_address.CStr());
		// points the address to a local file?
		if (url.GetAttribute(URL::KType) == URL_FILE)
		{
			// use original address, the OS does not handle URI formatted address
			local_address.Set(address);

			// Maybe this stripping is provided by an url function? I have not found it [espen]
#if defined(WIN32) || defined(_UNIX_DESKTOP_)
			if( local_address.Find(UNI_L("file://localhost/")) == 0 )
			{
				local_address.Delete( 0, 17 );
			}
#else
			if( local_address.Find(UNI_L("file://localhost")) == 0 )
			{
				local_address.Delete( 0, 16 );
			}
#endif
			else if( local_address.Find(UNI_L("file://")) == 0 )
			{
				local_address.Delete( 0, 7 );
			}

			// now if it is not empty, we consider it valid
			if (local_address.HasContent())
				return TRUE;
		}
	}
	// this is not a valid local file or path
	return FALSE;
}

/***********************************************************************************
**
**	GoToPage
**
***********************************************************************************/

void ClassicApplication::GoToPage(const OpStringC& url,
						   BOOL force_new_page,
						   BOOL force_background,
						   BOOL url_came_from_address_field,
						   DesktopWindow* target_window,
						   URL_CONTEXT_ID context_id,
						   BOOL ignore_modifier_keys,
						   BOOL privacy_mode)
{
 	OpenURLSetting setting;
	WindowCommanderProxy::InitOpenURLSetting(setting,
											 url,
											 force_new_page,
											 force_background,
											 url_came_from_address_field,
											 target_window,
											 context_id,
											 ignore_modifier_keys);
	setting.m_is_privacy_mode = privacy_mode;
	OpenURL( setting );
}


void ClassicApplication::AdjustForAction(OpenURLSetting& setting, OpInputAction* action, DocumentDesktopWindow* ddw)
{
	if (!action)
		return;

	if (action->GetReferrerAction() == OpInputAction::ACTION_PASTE_AND_GO ||
		action->GetReferrerAction() == OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND)
	{
		// Paste&Go shortcut contains by default CTRL and SHIFT which modifies open mode
		// It is ok to let a menu activated action to be modified by the shift keys though
		if (action->GetActionMethod() == OpInputAction::METHOD_KEYBOARD)
			setting.m_ignore_modifier_keys = TRUE;

		setting.m_in_background = action->GetAction() == OpInputAction::ACTION_PASTE_AND_GO_BACKGROUND ? YES : NO;
		setting.m_new_window = setting.m_new_page = NO;

		if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow))
		{
			// Test for empty existing tab. An empty tab shall be used no matter what
			if (!ddw || ddw->HasDocument())
			{
				if (g_application->IsSDI())
					setting.m_new_window = YES;
				else
					setting.m_new_page = YES;
			}
		}
	}
}


BOOL ClassicApplication::OpenURL( const OpStringC &address, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, URL_CONTEXT_ID context_id, BOOL ignore_modifier_keys, BOOL is_remote_command, BOOL is_privacy_mode)
{
	OpenURLSetting setting;
	setting.m_address.Set( address );
	setting.m_new_window    = new_window;
	setting.m_new_page      = new_page;
	setting.m_in_background = in_background;
	setting.m_context_id	= context_id;
	setting.m_ignore_modifier_keys = ignore_modifier_keys;
	setting.m_is_remote_command = is_remote_command;
	setting.m_is_privacy_mode = is_privacy_mode;

	return OpenURL( setting );
}


BOOL ClassicApplication::OpenURL( OpenURLSetting& setting )
{
	// Test that address is valid so that we do not open
	// an empty window when we should not do it.

	OpString address;
	address.Set( setting.m_address.CStr() );
	address.Strip();

	// Check if its empty, AND if its a gadget
	if(!setting.m_force_new_tab && address.IsEmpty())
	{
		if (setting.m_url.IsEmpty())
		{
			return FALSE;
		}
	}
	else
	{
		// check if the address points to local file/adress,
		// and check if it is a special type that Opera can use
		OpString local_address;

		if (GetLocalAddress(address, local_address))
		{
			if (OpenIfSessionFile(local_address))
			{
				return TRUE;
			}

			BOOL is_extension_path = FALSE;
			RETURN_VALUE_IF_ERROR(
					ExtensionUtils::IsExtensionPath(local_address, 
						is_extension_path), FALSE);
			if (is_extension_path)
			{
				return OpStatus::IsSuccess(
						g_desktop_extensions_manager->InstallFromLocalPath(
							local_address));
			}

			if(SkinUtils::OpenIfThemeFile(address))
			{
				return TRUE;
			}
		}
	}

	// Check if it's a nickname
	if (setting.m_address.HasContent() && !uni_strpbrk( setting.m_address.CStr(), UNI_L(".:/?\\")))
	{
		OpAutoVector<OpString> nick_urls;
		g_hotlist_manager->ResolveNickname(setting.m_address, nick_urls);

		if (nick_urls.GetCount() >= 1)
		{
			INT32 limit = g_pcui->GetIntegerPref(PrefsCollectionUI::ConfirmOpenBookmarkLimit);
			if( limit > 0 && nick_urls.GetCount() > (UINT32)limit )
			{
				OpString message, title;
				RETURN_VALUE_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_BOOKMARKS, title),FALSE);
				RETURN_VALUE_IF_ERROR(StringUtils::GetFormattedLanguageString(message,Str::S_ABOUT_TO_OPEN_X_BOOKMARKS,nick_urls.GetCount()),FALSE);

				SimpleDialogController* controller = OP_NEW(SimpleDialogController,
					(SimpleDialogController::TYPE_OK_CANCEL, SimpleDialogController::IMAGE_INFO,WINDOW_NAME_WARN_OPENING_BOOKMARKS,message, title));
				if (!controller)
					return FALSE;

				SimpleDialogController::DialogResultCode result;
				controller->SetBlocking(result);
				RETURN_VALUE_IF_ERROR(controller->ConnectDoNotShowAgain(*g_pcui,PrefsCollectionUI::ConfirmOpenBookmarkLimit,-limit,limit),FALSE);

				if (OpStatus::IsError(ShowDialog(controller, g_global_ui_context,setting.m_target_window)))
					return FALSE;

				if(result != SimpleDialogController::DIALOG_RESULT_OK)
					return FALSE;
			}

			for (UINT32 i = 0; i < nick_urls.GetCount(); i++)
			{
				setting.m_address.Set(*nick_urls.Get(i));
				if (i > 0)
				{
					if (setting.m_new_window != YES)
						setting.m_new_page = YES;
				}
				OpenResolvedURL(setting);
			}
			return TRUE;
		}
	}
	// Check if it's a speed dial
#ifdef SUPPORT_SPEED_DIAL
	if(setting.m_from_address_field || setting.m_digits_are_speeddials)
	{
		OpString speed_dial_address;
		OpString speed_dial_title;
		if (g_speeddial_manager->IsAddressSpeedDialNumber(address, speed_dial_address, speed_dial_title))
		{
			address.Set(speed_dial_address);
			setting.m_address.Set(address);
		}
	}
#endif // SUPPORT_SPEED_DIAL
	// Check if it's a shortcut for opera page (o:...)
	const uni_char* typed_address = setting.m_address.CStr();
	if (typed_address && uni_strlen(typed_address) >= 2 && typed_address[0] == 'o' && typed_address[1] == ':')
	{
		OpString new_address;
		RETURN_VALUE_IF_ERROR(new_address.Set("opera:"), FALSE);
		RETURN_VALUE_IF_ERROR(new_address.Append(typed_address + 2), FALSE);
		RETURN_VALUE_IF_ERROR(setting.m_address.Set(new_address), FALSE);
	}

	BrowserDesktopWindow* bw = GetActiveBrowserDesktopWindow();
	if( bw )
	{
		bw->Activate(TRUE);
	}
	return 	OpenResolvedURL(setting);
}

/**
 * Opens a URL in a browser page
 *
 * Handles settings for new_window / new_page / in_background
 *
 *
 **/
BOOL ClassicApplication::OpenResolvedURL( OpenURLSetting& setting )
{
	OpString address;
	address.Set( setting.m_address.CStr() );

	if (IsEmBrowser())
	{
		windowManager->OpenURLNewWindow(address.CStr(), TRUE, setting.m_new_window);
		setting.m_target_window = NULL;
		return TRUE;
	}

	if (setting.m_from_address_field)
	{
		// url from address field and speeddial should inherit privacy mode from active tab
		// fix for DSK-278231 shift-clicking items from Private tab speed dial open as non-private
		DocumentDesktopWindow* dw = GetActiveDocumentDesktopWindow();
		if (dw)
			setting.m_is_privacy_mode |= dw->PrivacyMode();
	}
	else
	{
		// otherwise inherit privacy mode from the src window
		if (setting.m_src_commander)
			setting.m_is_privacy_mode |= setting.m_src_commander->GetPrivacyMode();
	}

	BOOL new_window    = setting.m_new_window == YES;
	BOOL new_page      = setting.m_new_page == YES;
	BOOL in_background = setting.m_in_background == YES;

	// variables to store special condition states
	BOOL use_current_page = FALSE;
	BOOL javascript_address = FALSE;

	// First check some special conditions
	if (!setting.m_from_address_field)
	{
		// Never open in a new page or window if there is an active empty page or speeddial
		DocumentDesktopWindow* document_desktop_window = GetActiveDocumentDesktopWindow();
		if (document_desktop_window)
		{
			OP_ASSERT(document_desktop_window->GetWindowCommander());
			const uni_char* active_url = document_desktop_window->GetWindowCommander()->GetCurrentURL(FALSE);
			if ( (!active_url || !active_url[0] || document_desktop_window->IsSpeedDialActive()) && (!document_desktop_window->GetWindowCommander()->IsLoading())
				 && !(g_op_system_info->GetShiftKeyState() & SHIFTKEY_OPEN_IN_NEW_WINDOW) )
				use_current_page = TRUE;
		}
		javascript_address = address.Find("javascript:") == 0;
	}

	// If no new pages/windows have been force, get prefered opening from preferences
	if (setting.m_new_page == MAYBE)
	{
		new_page = IsOpeningInNewPagePreferred(setting.m_ignore_modifier_keys, setting.m_from_address_field) && !use_current_page && !javascript_address;
	}
	if (setting.m_new_window == MAYBE )
	{
		new_window = IsOpeningInNewWindowPreferred(setting.m_ignore_modifier_keys, setting.m_from_address_field) && !use_current_page && !javascript_address;
	}
	if (setting.m_in_background == MAYBE )
	{
		in_background = IsOpeningInBackgroundPreferred(setting.m_ignore_modifier_keys);
	}

	DesktopWindow* active_desktop_window = GetActiveDesktopWindow(FALSE);

	// Panels always open in a new page
	if (active_desktop_window && active_desktop_window->GetType() == WINDOW_TYPE_PANEL)
	{
		new_page = YES;
	}

	//Kiosk mode specials
	if( KioskManager::GetInstance()->GetEnabled() )
	{
		if( new_window == TRUE )
		{
			if( !KioskManager::GetInstance()->GetKioskNormalScreen() )
			{
				new_window = FALSE;
				new_page = TRUE;
			}
		}
		if( !KioskManager::GetInstance()->GetKioskWindows() )
		{
			new_page = FALSE;
			in_background = FALSE;
		}
	}

	// Select window and/or page
	DocumentDesktopWindow *dw = NULL;

	// Check for some other special conditions
	if (!new_window && !new_page && setting.m_src_commander)	//Should not open new page/window, but source is known
	{
		DocumentDesktopWindow* src_window = m_window_collection.GetDocumentWindowFromCommander(setting.m_src_commander);
		// Let's check the source window
		// urls from non-document window need to open in new page, mail window can't show webpages
		if (!src_window)
			new_page = TRUE;
		// reuse source window
		else if (!setting.m_target_window)
			setting.m_target_window = src_window;
	}
	// we don't want speed dial if we have an address
	BOOL no_speeddial = setting.m_address.HasContent();

	if (!new_window && !new_page && setting.m_target_window && setting.m_target_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		//Target window is specified, let's use it then
		dw = (DocumentDesktopWindow*)setting.m_target_window;
	}
	else
	{
		if(new_window)
		{
			GetBrowserDesktopWindow( TRUE, in_background, TRUE, &dw, NULL, 0, 0, TRUE, FALSE, setting.m_ignore_modifier_keys, TRUE, 0, setting.m_is_privacy_mode);
		}
		else
		{
			if (new_page && setting.m_target_window && setting.m_target_window->GetType() == WINDOW_TYPE_BROWSER)
			{
				BrowserDesktopWindow* bw = (BrowserDesktopWindow*)setting.m_target_window;
				dw = CreateDocumentDesktopWindow(bw->GetWorkspace(), NULL, 0, 0, TRUE, FALSE, in_background, no_speeddial, 0, bw->PrivacyMode() || setting.m_is_privacy_mode);
			}
			else
			{

				BrowserDesktopWindow* bw = GetActiveBrowserDesktopWindow();
				if( bw )
				{
					dw = bw->GetActiveDocumentDesktopWindow();
					if( new_page || !dw )
					{

						if (dw && !setting.m_is_remote_command)
							setting.m_is_privacy_mode |= dw->PrivacyMode();

						GetBrowserDesktopWindow( FALSE, in_background, TRUE, &dw, NULL, 0, 0, TRUE, FALSE, setting.m_ignore_modifier_keys, no_speeddial, &setting, setting.m_is_privacy_mode);
						if (!dw)
							return FALSE;

						if( setting.m_target_position >= 0 )
						{
							DesktopWindowCollectionItem* parent = setting.m_target_parent ? setting.m_target_parent : dw->GetParentWorkspace()->GetModelItem();
							g_application->GetDesktopWindowCollection().ReorderByPos(dw->GetModelItem(), parent, setting.m_target_position);
						}
					}
				}
				else
				{
					DesktopWindow *w = GetActiveDesktopWindow(TRUE);
					if( w && w->GetType() == WINDOW_TYPE_DOCUMENT )
					{
						// Will happen when we open a url from addressbar in a detached page/tab

						dw = (DocumentDesktopWindow*)w;
						if( new_page || new_window )
						{
							dw = CreateDocumentDesktopWindow(0, 0, dw->GetInnerWidth(), dw->GetInnerHeight(), TRUE, FALSE, in_background, no_speeddial, 0, setting.m_is_privacy_mode);
						}
						else
						{
							dw = (DocumentDesktopWindow*)w;
						}
					}
					else
					{
						// Will happen on startup (as when "start-with-homepage")
						GetBrowserDesktopWindow( FALSE, FALSE, TRUE, &dw, NULL, 0, 0, TRUE, FALSE, setting.m_ignore_modifier_keys, no_speeddial, 0, setting.m_is_privacy_mode);
					}
				}
			}
		}
	}

	// Open url

	if( dw )
	{
#ifdef _MACINTOSH_
		DesktopWindow* dw_parent = dw->GetParentDesktopWindow();

		if (dw_parent && dw_parent->IsMinimized())
			dw_parent->Restore();
		
		if (!in_background)
			dw->Activate(TRUE);
#endif // _MACINTOSH_

		// BrowserDesktopWindow may need to know the setting
		setting.m_new_window    = new_window ? YES : NO;
		setting.m_new_page      = new_page ? YES : NO;
		setting.m_in_background = in_background ? YES : NO;

# ifdef DU_REMOTE_URL_HANDLER
		if (setting.m_is_remote_command && RemoteURLHandler::CheckLoop(setting))
		{
			// This is the proverbial bit-bucket (/dev/null), to improve the handling of
			// urls's that are looping, opera should be forced to be viewer or handler (for mail)
			// the fact that one are looping the urls is a problem of misconfiguration either
			// on the system level or in opera mime or protocol handling. Second best would be
			// to indicate it in the error console as currently implemented:
#ifdef OPERA_CONSOLE
			if (g_console)
			{
				OpConsoleEngine::Message msg(OpConsoleEngine::Internal, OpConsoleEngine::Error);
				msg.context.Set("Commandline or remote URL handling");
				msg.message.Set("This URL has prior to this been sent to an external application for handling.\r\n Further handling will be aborted as that would constitute a loop, possibly endless.\r\n The cause is probably misconfiguration of mime or protocol type handling.");
				msg.url.Set(address.CStr());

				TRAPD(rc, g_console->PostMessageL(&msg));
			}
#endif // OPERA_CONSOLE
			return FALSE;
		}
# endif // DU_REMOTE_URL_HANDLER

		// only go to the url if it's not empty. This makes it possible to use -newtab etc. from the command line without specifying a url
		if(setting.m_address.HasContent() || !setting.m_url.IsEmpty())
		{
			if (setting.m_save_address_to_history && !dw->PrivacyMode())
			{
				if (setting.m_typed_address.HasContent())
				{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
					if(g_searchEngineManager->IsSearchString(setting.m_typed_address))
						setting.m_type = DirectHistory::SEARCH_TYPE;
#endif // DESKTOP_UTIL_SEARCH_ENGINES
					if (g_hotlist_manager->HasNickname(setting.m_typed_address, NULL))
						setting.m_type = DirectHistory::NICK_TYPE;

					directHistory->Add(setting.m_typed_address, setting.m_type, g_timecache->CurrentTime());
				}
				else
				{
					directHistory->Add(setting.m_address, DirectHistory::TEXT_TYPE, g_timecache->CurrentTime());
				}
			}

			dw->GoToPage(setting);
		}
		// if the caller didn't specify a target window (or another target window was used for some reason)
		// make sure the correct target window is returned
		if (setting.m_target_window != dw)
		{
			setting.m_target_window = dw;
		}
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/***********************************************************************************
**
**	IsOpeningInNewWindowPreferred
**
***********************************************************************************/
BOOL ClassicApplication::IsOpeningInNewWindowPreferred(BOOL ignore_modifier_keys, BOOL from_address_field)
{
	BOOL new_window = IsSDI() && g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow) && !from_address_field;
	if (!ignore_modifier_keys)
		new_window &= (g_op_system_info->GetShiftKeyState() & SHIFTKEY_OPEN_IN_NEW_WINDOW) != 0;
	return new_window;
}

/***********************************************************************************
**
**	IsOpeningInNewPagePreferred
**
***********************************************************************************/
BOOL ClassicApplication::IsOpeningInNewPagePreferred(BOOL ignore_modifier_keys, BOOL from_address_field)
{
	BOOL new_page = !IsSDI() && g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow) && !from_address_field;
	if (!ignore_modifier_keys)
		new_page |= (g_op_system_info->GetShiftKeyState() & SHIFTKEY_OPEN_IN_NEW_WINDOW) != 0;
	return new_page;
}

/***********************************************************************************
**
**	IsOpeningInBackgroundPreferred
**
***********************************************************************************/
BOOL ClassicApplication::IsOpeningInBackgroundPreferred(BOOL ignore_modifier_keys)
{
	if (!ignore_modifier_keys)
	{
#ifdef NEW_WINDOW_DEFAULT_OPEN_IN_BACKGROUND
		BOOL new_window_default_open_in_background = TRUE;
#else
		BOOL new_window_default_open_in_background = FALSE;
#endif // NEW_WINDOW_DEFAULT_OPEN_IN_BACKGROUND
		BOOL toggle_in_background = ((g_op_system_info->GetShiftKeyState() & SHIFTKEY_TOGGLE_OPEN_IN_BACKGROUND)
									 && !(g_op_system_info->GetShiftKeyState() & SHIFTKEY_PREVENT_OPEN_IN_BACKGROUND));
		return BOOL(new_window_default_open_in_background ^ toggle_in_background);
	}
	else
		return FALSE;
}

BOOL ClassicApplication::ShowM2()
{
#ifdef M2_SUPPORT
	return g_m2_engine != 0 
# ifdef M2_MERLIN_COMPATIBILITY
			&& !StoreUpdater::NeedsUpdate()
# endif
			;
#else
	return FALSE;
#endif
}

/***********************************************************************************
**
**	HasMail
**
***********************************************************************************/

BOOL ClassicApplication::HasMail()
{
#ifdef M2_SUPPORT
	if (!ShowM2())
		return FALSE;

	if (g_m2_engine->GetAccountManager()->GetFirstAccountWithType(AccountTypes::POP))
		return TRUE;

	if (g_m2_engine->GetAccountManager()->GetFirstAccountWithType(AccountTypes::IMAP))
		return TRUE;

	if (g_m2_engine->GetAccountManager()->GetFirstAccountWithType(AccountTypes::NEWS))
		return TRUE;
#endif
	return FALSE;
}

BOOL ClassicApplication::HasChat()
{
#ifdef IRC_SUPPORT
	if (!ShowM2())
		return FALSE;

	if (g_m2_engine->GetAccountManager()->GetFirstAccountWithType(AccountTypes::IRC))
		return TRUE;

	if (g_m2_engine->GetAccountManager()->GetFirstAccountWithType(AccountTypes::MSN))
		return TRUE;

#ifdef JABBER_SUPPORT
	if (g_m2_engine->GetAccountManager()->GetFirstAccountWithType(AccountTypes::JABBER))
		return TRUE;
#endif // JABBER_SUPPORT

#endif // IRC_SUPPORT
	return FALSE;
}

BOOL ClassicApplication::HasFeeds()
{
#ifdef M2_SUPPORT
	return ShowM2() && g_m2_engine->GetAccountManager()->GetRSSAccount(FALSE);
#else
	return FALSE;
#endif
}

OpUiWindowListener* ClassicApplication::CreateUiWindowListener()
{
	return OP_NEW(BrowserUiWindowListener, ());
}

DesktopOpAuthenticationListener* ClassicApplication::CreateAuthenticationListener()
{
	return OP_NEW(BrowserAuthenticationListener, ());
}

DesktopMenuHandler* ClassicApplication::GetMenuHandler()
{
	return m_menu_handler;
}


/***********************************************************************************
**
**	GoToMailView
**
***********************************************************************************/
#ifdef M2_SUPPORT
MailDesktopWindow* ClassicApplication::GoToMailView(INT32 index_id, const uni_char* address, const uni_char* title, BOOL force_show, BOOL force_new_window, BOOL force_download, BOOL ignore_modifier_keys)
{
	if (!ShowM2())
		return NULL;

	// Check if index exists
	if (index_id && !g_m2_engine->GetIndexById(index_id))
		return NULL;

	if (m_active_toplevel_desktop_window && !force_new_window)
	{
		force_new_window = (!ignore_modifier_keys && m_active_toplevel_desktop_window->GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_OPEN_IN_NEW_WINDOW);
	}

	MailDesktopWindow* mail_window = GetActiveMailDesktopWindow();

	if (!mail_window && !force_show)
		return NULL;

	if ((!mail_window || mail_window->IsMessageOnlyView()) && !force_new_window)
	{
		// the active window was not a mail window, but maybe some other mail window exist
		// for this index inside current browser window, and if so active that one.
		// This avoids getting a lot of similar mail views open.. (typically the unread window)
		// This function now follows the "Reuse existing" option.

		BrowserDesktopWindow* browser = GetActiveBrowserDesktopWindow();

		if (browser)
		{
			OpVector<DesktopWindow> mailwindows;
			OpStatus::Ignore(browser->GetWorkspace()->GetDesktopWindows(mailwindows, OpTypedObject::WINDOW_TYPE_MAIL_VIEW));
			for (UINT32 i = 0; i < mailwindows.GetCount(); i++)
			{
				mail_window = static_cast<MailDesktopWindow*>(mailwindows.Get(i));

				if (!mail_window->IsMessageOnlyView() && (mail_window->IsCorrectMailView(index_id, address) || !force_new_window))
				{
					mail_window->Activate();
					break;
				}

				mail_window = NULL;
			}
		}
	}

	if (!mail_window || force_new_window)
	{
		BrowserDesktopWindow* browser = GetBrowserDesktopWindow(IsSDI());

		if (browser)
		{
			if (OpStatus::IsSuccess(MailDesktopWindow::Construct(&mail_window, browser->GetWorkspace())))
			{
				mail_window->Init(index_id, address, title);
				mail_window->Show(TRUE);
			}
		}
	}
	else
	{
		mail_window->SetMailViewToIndex(index_id, address, title, TRUE, FALSE, force_show);
		mail_window->Activate();
	}

	if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
    {
		g_m2_engine->SelectFolder(index_id);
    }

	g_session_auto_save_manager->SaveCurrentSession();

	// make sure the browser window is maximized, since this action can be triggered from the systray
	if (mail_window->GetParentDesktopWindow())
		mail_window->GetParentDesktopWindow()->Activate();

	return mail_window;
}

/***********************************************************************************
**
**	GoToMailView
**
***********************************************************************************/

MailDesktopWindow* ClassicApplication::GoToMailView(OpINT32Vector& id_list, BOOL force_show, BOOL include_folder_content, BOOL ignore_modifier_keys)
{
	OpString address, title;

	if (!g_hotlist_manager->GetMailAndName(id_list, address, title, include_folder_content))
		return NULL;

	return GoToMailView(0, address.CStr(), title.CStr(), force_show, FALSE, TRUE, ignore_modifier_keys);
}

/***********************************************************************************
**
**	GoToChat
**
***********************************************************************************/
#ifdef IRC_SUPPORT
void ClassicApplication::GoToChat(UINT32 account_id, const ChatInfo& room, BOOL is_room,
	BOOL is_joining, BOOL open_in_background)
{
	if (room.HasName())
	{
		ChatDesktopWindow* window = ChatDesktopWindow::FindChatRoom(account_id, room.ChatName(), is_room);
		if (window)
		{
			if (!is_joining)
				window->GoToChat();
		}
		else
		{
			BrowserDesktopWindow* browser = GetBrowserDesktopWindow(IsSDI());
			if (browser)
			{
				window = OP_NEW(ChatDesktopWindow, (browser->GetWorkspace()));
				if (window)
				{
					if (OpStatus::IsSuccess(window->init_status) && OpStatus::IsSuccess(window->Init(account_id, room.ChatName(), is_room, is_joining)))
						window->Show(TRUE, 0, OpWindow::RESTORED, is_room ? FALSE : open_in_background);
					else
						window->Close(TRUE);
				}
			}
		}
	}
}

/***********************************************************************************
**
**	LeaveChatRoom
**
***********************************************************************************/

BOOL ClassicApplication::LeaveChatRoom(INT32 account_id, const OpStringC& name)
{
	ChatDesktopWindow* window =	ChatDesktopWindow::FindChatRoom(account_id, name, TRUE);

	if (!window)
		return FALSE;

	window->Close();
	return TRUE;
}

/***********************************************************************************
**
**	GetChatStatusFromString
**
***********************************************************************************/

AccountTypes::ChatStatus  ClassicApplication::GetChatStatusFromString(const uni_char* chat_status_string)
{
	if (!chat_status_string)
		return AccountTypes::OFFLINE;

	if (uni_stricmp(chat_status_string, UNI_L("online")) == 0)				return AccountTypes::ONLINE;
	else if (uni_stricmp(chat_status_string, UNI_L("busy")) == 0)			return AccountTypes::BUSY;
	else if (uni_stricmp(chat_status_string, UNI_L("be right back")) == 0)	return AccountTypes::BE_RIGHT_BACK;
	else if (uni_stricmp(chat_status_string, UNI_L("away")) == 0)			return AccountTypes::AWAY;
	else if (uni_stricmp(chat_status_string, UNI_L("on the phone")) == 0)	return AccountTypes::ON_THE_PHONE;
	else if (uni_stricmp(chat_status_string, UNI_L("out to lunch")) == 0)	return AccountTypes::OUT_TO_LUNCH;
	else if (uni_stricmp(chat_status_string, UNI_L("appear offline")) == 0)	return AccountTypes::APPEAR_OFFLINE;

	return AccountTypes::OFFLINE;
}

/***********************************************************************************
**
**	GetChatStatus
**
***********************************************************************************/

AccountTypes::ChatStatus ClassicApplication::GetChatStatus(INT32 account_id)
{
	BOOL is_connecting = FALSE;
	return GetChatStatus(account_id, is_connecting);
}

AccountTypes::ChatStatus ClassicApplication::GetChatStatus(INT32 account_id, BOOL& is_connecting)
{
	if (!ShowM2() || !account_id)
		return AccountTypes::OFFLINE;

	AccountManager* account_manager = g_m2_engine->GetAccountManager();
	Account* account = NULL;

	if (account_id == -1)
	{
		// if account id is -1, that means return the first non-offline account's status

		for (INT32 i = 0; i < account_manager->GetAccountCount(); i++)
		{
			account = account_manager->GetAccountByIndex(i);

			if (account)
			{
				AccountTypes::ChatStatus current_chat_status = account->GetChatStatus(is_connecting);

				if (current_chat_status != AccountTypes::OFFLINE)
					return current_chat_status;
			}
		}

		return AccountTypes::OFFLINE;
	}

	account_manager->GetAccountById(account_id, account);

	if (!account)
		return AccountTypes::OFFLINE;

	return account->GetChatStatus(is_connecting);
}

/***********************************************************************************
**
**	SetChatStatus
**
***********************************************************************************/

BOOL ClassicApplication::SetChatStatus(INT32 account_id, AccountTypes::ChatStatus chat_status, BOOL check_if_possible_only)
{
	if (!ShowM2() || !account_id)
		return FALSE;

	AccountManager* account_manager = g_m2_engine->GetAccountManager();
	Account* account = NULL;

	if (account_id == -1)
	{
		// if account id is 0, that means affect all chat accounts that are already online
		// ie, doing SetChatStatus(0, ONLINE) will not make all accounts online, but only set online status to ONLINE for those
		// already connected to

		BOOL handled = FALSE;

		for (INT32 i = 0; i < account_manager->GetAccountCount(); i++)
		{
			account = account_manager->GetAccountByIndex(i);

			if (account)
			{
				BOOL is_connecting = FALSE;
				AccountTypes::ChatStatus current_chat_status = account->GetChatStatus(is_connecting);

				if (current_chat_status == AccountTypes::OFFLINE)
					continue;

				if (check_if_possible_only)
					return TRUE;

				account->SetChatStatus(chat_status);

				handled = TRUE;
			}
		}

		return handled;
	}

	account_manager->GetAccountById(account_id, account);

	if (!account)
		return FALSE;

	AccountTypes::AccountType account_type = account->GetIncomingProtocol();

	if (account_type != AccountTypes::IRC
		&& account_type != AccountTypes::MSN
#ifdef JABBER_SUPPORT
		&& account_type != AccountTypes::JABBER
#endif
	)
	{
		return FALSE;
	}

	if (!account->IsChatStatusAvailable(chat_status))
		return FALSE;

	if (check_if_possible_only)
		return TRUE;

	BOOL is_connecting = FALSE;
	AccountTypes::ChatStatus current_chat_status = account->GetChatStatus(is_connecting);

	if (current_chat_status == chat_status)
		return FALSE;

	account->SetChatStatus(chat_status);

	return TRUE;
}
#endif // IRC_SUPPORT
/***********************************************************************************
**
**	SubscribeAccount
**
***********************************************************************************/

BOOL ClassicApplication::SubscribeAccount(AccountTypes::AccountType type, const char* type_name, INT32 account_id, DesktopWindow* parent_window)
{
	if (!ShowM2())
		return FALSE;

	if (!parent_window)
		parent_window = GetActiveDesktopWindow();

	if (account_id)
	{
		AccountSubscriptionDialog* dialog = OP_NEW(AccountSubscriptionDialog, ());
		if (dialog)
			dialog->Init(account_id, parent_window);
		return TRUE;
	}

	if (type_name)
	{
		type = Account::GetProtocolFromName(type_name);
	}

	if (g_m2_engine->GetAccountManager()->GetFirstAccountWithType(type))
	{
		AccountSubscriptionDialog* dialog = OP_NEW(AccountSubscriptionDialog, ());
		if (dialog)
			dialog->Init(type, parent_window);
		return TRUE;
	}

	return ShowAccountNeededDialog(type,parent_window);
}

/***********************************************************************************
**
**	CreateAccount
**
***********************************************************************************/

BOOL ClassicApplication::CreateAccount(AccountTypes::AccountType type, const char* type_name, DesktopWindow* parent_window)
{
	if (!ShowM2())
		return FALSE;

	if (!parent_window)
		parent_window = GetActiveDesktopWindow();

	if (type_name)
	{
		type = Account::GetProtocolFromName(type_name);
	}

	NewAccountWizard* wizard = OP_NEW(NewAccountWizard, ());
	if (wizard)
		wizard->Init(type, parent_window);
	return TRUE;
}

/***********************************************************************************
**
**	ShowAccountNeededDialog
**
***********************************************************************************/

BOOL ClassicApplication::ShowAccountNeededDialog(AccountTypes::AccountType type, DesktopWindow* parent_window)
{
	if (!ShowM2())
		return FALSE;

	SimpleDialogController* controller = OP_NEW(AccountNeededDialogController, (type, parent_window));
	RETURN_VALUE_IF_NULL(controller, FALSE);
	RETURN_VALUE_IF_ERROR(ShowDialog(controller, g_global_ui_context, GetActiveDesktopWindow()), FALSE);

	return TRUE;
}

/***********************************************************************************
**
**	DeleteAccount
**
***********************************************************************************/

BOOL ClassicApplication::DeleteAccount(INT32 account_id, DesktopWindow* parent_window)
{
	if (!ShowM2() || !account_id)
		return FALSE;

	if (!parent_window)
		parent_window = GetActiveDesktopWindow();

	Account* account = NULL;
	if(g_m2_engine->GetAccountManager()->GetAccountById((UINT16)account_id, account) == OpStatus::OK && account)
	{
		DeleteAccountController* controller = OP_NEW(DeleteAccountController, (account_id, TRUE));
		
		RETURN_VALUE_IF_ERROR(ShowDialog(controller,g_global_ui_context,parent_window),FALSE);
	}

	return TRUE;
}

/***********************************************************************************
**
**	EditAccount
**
***********************************************************************************/

BOOL ClassicApplication::EditAccount(INT32 account_id, DesktopWindow* parent_window, BOOL modality)
{
	if (!ShowM2() || !account_id)
		return FALSE;

	AccountPropertiesDialog *dialog = OP_NEW(AccountPropertiesDialog, (modality));
	if (dialog)
		dialog->Init(account_id, parent_window);

	return TRUE;
}


#ifdef JABBER_SUPPORT

/***********************************************************************************
**
**  ComposeMessage
**
***********************************************************************************/

ChatDesktopWindow* ClassicApplication::ComposeMessage(OpINT32Vector& id_list)
{
	HotlistModelItem *hmi = g_hotlist_manager->GetItemByID(id_list.Get(0));
	Account *account = MessageEngine::GetInstance()->GetAccountManager()->GetFirstAccountWithType(AccountTypes::JABBER);

	ChatDesktopWindow *window = NULL;
	BrowserDesktopWindow* browser = GetBrowserDesktopWindow(IsSDI());
	if (browser)
	{
		window = OP_NEW(ChatDesktopWindow, (browser->GetWorkspace()));
		if (window)
		{
			if (OpStatus::IsSuccess(window->init_status) && OpStatus::IsSuccess(window->Init(account->GetAccountId(), hmi->GetName(), FALSE, FALSE))
				window->Show(TRUE, 0, OpWindow::RESTORED, FALSE);
			else
			{
				window->Close(TRUE);
				return NULL;
			}
		}
	}

	return window;
}


/***********************************************************************************
**
**  SubscribeToPresence
**
***********************************************************************************/

BOOL ClassicApplication::SubscribeToPresence(OpINT32Vector& id_list, BOOL subscribe)
{
	HotlistModelItem *hmi = g_hotlist_manager->GetItemByID(id_list.Get(0));
	Account *account = MessageEngine::GetInstance()->GetAccountManager()->GetFirstAccountWithType(AccountTypes::JABBER);

	return account->SubscribeToPresence(hmi->GetShortName(), subscribe) == OpStatus::OK;
}


/***********************************************************************************
**
**  AllowPresenceSubscription
**
***********************************************************************************/

BOOL ClassicApplication::AllowPresenceSubscription(OpINT32Vector& id_list, BOOL allow)
{
	HotlistModelItem *hmi = g_hotlist_manager->GetItemByID(id_list.Get(0));
	Account *account = MessageEngine::GetInstance()->GetAccountManager()->GetFirstAccountWithType(AccountTypes::JABBER);

	return account->AllowPresenceSubscription(hmi->GetShortName(), allow) == OpStatus::OK;
}

#endif // JABBER_SUPPORT

#endif //M2_SUPPORT

/***********************************************************************************
**
**	GetPanelDesktopWindow
**
***********************************************************************************/
// startBrowser??
PanelDesktopWindow* ClassicApplication::GetPanelDesktopWindow(Type type,
															  BOOL create_new_if_needed,
															  BOOL force_new,
															  BOOL in_background,
															  const uni_char* action_string_parameter)
{
	if (type == UNKNOWN_TYPE)
		return NULL;

	if (type == PANEL_TYPE_MAIL && !HasMail() && !HasFeeds())
		return NULL;
	if (type == PANEL_TYPE_CHAT && !HasChat())
		return NULL;
	if (!force_new)
	{
		OpVector<DesktopWindow> panel_windows;
		m_window_collection.GetDesktopWindows(OpTypedObject::WINDOW_TYPE_PANEL, panel_windows);

		for (UINT32 i = 0; i < panel_windows.GetCount(); i++)
		{
			PanelDesktopWindow* panel_window = static_cast<PanelDesktopWindow*>(panel_windows.Get(i));
			if (panel_window->GetPanelType() == type && 
				(!panel_window->GetParentDesktopWindow() ||
				 panel_window->GetParentDesktopWindow() == GetActiveBrowserDesktopWindow()))
			{
				panel_window->SetSearch(action_string_parameter);

				if (!in_background)
					panel_window->Activate();
				
				return panel_window;
			}
		}

		if (!create_new_if_needed)
			return NULL;
	}

	BrowserDesktopWindow* browser = IsSDI() ? NULL : GetBrowserDesktopWindow();

	PanelDesktopWindow* window = OP_NEW(PanelDesktopWindow, (browser ? browser->GetWorkspace() : NULL));

	if (window)
	{
		if (OpStatus::IsError(window->init_status))
		{
			window->Close(TRUE);
			return NULL;
		}

		window->SetPanelByType(type);
		if (!window->GetPanel())
		{
			window->Close(TRUE);
			return NULL;
		}

		// Pass the parameter string along - sets the text in the quickfind field
		window->SetSearch(action_string_parameter);

		window->Show(TRUE, NULL, OpWindow::RESTORED, in_background);

		if( browser && browser->GetWorkspace() && !in_background )
		{
			// Fix for bug #184609 (Transfers windows takes focus) - third comment.
			// It happens because DocumentDesktopWindow::OnLoadingFinished() will call
			// OpInputManager::UpdateRecentKeyboardChildInputContext() after we have
			// created the window and thereby remove focus from visible window. OnLoadingFinished()
			// will be called from download dialog when we start to show progress in transfer
			// window [espen 2006-05-26]
			window->Activate();
		}
	}

	return window;
}

/***********************************************************************************
**
**	CreateDocumentDesktopWindow
**
***********************************************************************************/

DocumentDesktopWindow* ClassicApplication::CreateDocumentDesktopWindow(OpWorkspace* workspace, OpWindowCommander* window_commander, INT32 width, INT32 height, BOOL show_toolbars, BOOL focus_document, BOOL force_background, BOOL no_speeddial, OpenURLSetting *setting, BOOL private_browsing)
{
	DocumentDesktopWindow::InitInfo info;
	info.m_window_commander = window_commander;
	info.m_show_toolbars = !!show_toolbars;
	info.m_focus_document = !!focus_document;
	info.m_no_speeddial = !!no_speeddial;
	info.m_url_settings = setting;
	info.m_private_browsing = !!private_browsing;
	info.m_delayed_addressbar = width != 0 || height != 0;
	DocumentDesktopWindow* window = OP_NEW(DocumentDesktopWindow, (workspace, NULL, &info));
	if (!window)
		return NULL;

	if (OpStatus::IsError(window->init_status))
	{
		window->Close(TRUE);
		return NULL;
	}

	if (width && height)
	{
		window->GetSizeFromBrowserViewSize(width, height);
	}

	OpRect rect(DEFAULT_SIZEPOS, DEFAULT_SIZEPOS, width == 0 ? DEFAULT_SIZEPOS : width, height == 0 ? DEFAULT_SIZEPOS : height);

	BOOL use_rect = width || height;
	DesktopWindow *adw;

	window->Show(TRUE, use_rect ? &rect : NULL, OpWindow::RESTORED, force_background, use_rect, use_rect);

	// Make it fullscreen it there are other fullscreen windows
	if (KioskManager::GetInstance()->GetEnabled() && (adw = GetActiveDesktopWindow()) != NULL && adw->IsFullscreen())
		window->Fullscreen(TRUE);

	g_session_auto_save_manager->SaveCurrentSession();

	return window;
}

/***********************************************************************************
**
**	OnDesktopWindowActivated
**
***********************************************************************************/

void ClassicApplication::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL activate)
{
	// popup window shouldn't be activated, but if it is somehow we want to pretend it
	// doesn't happen so the window doesn't end up being returned by ActiveDesktopWindow()
	if (m_popup_desktop_window == desktop_window)
		return;

	// Close the popup
	if (m_popup_desktop_window
			&& !m_popup_desktop_window->GetModelItem().IsAncestorOf(desktop_window->GetModelItem()))
	{
		SetPopupDesktopWindow(NULL);
	}

	if (activate)
	{
		if (!IsActivated())
			OnActivate(TRUE); //The opera application was inactive, and is activated again
	}
	else
	{
		if( !(desktop_window->GetType() == WINDOW_TYPE_BROWSER && desktop_window->IsFullscreen()) )
		{
			//A non-Opera window might be activated, so the Opera application could be inactive now.
			//Because we can't know now if the new active window is an Opera window or not, we check if
			//Opera is reactivated when the message handled.
			g_main_message_handler->PostMessage(MSG_WINDOW_DEACTIVATED, (MH_PARAM_1)this, 0, 100);
		}
	}

	//remember that the desktop window now is the active desktop window
	DesktopWindow* parent_desktop_window = desktop_window->GetParentDesktopWindow();
	if (!parent_desktop_window)
	{
		if (activate)
		{
			if (m_active_toplevel_desktop_window != desktop_window)
			{
				g_input_manager->UpdateAllInputStates(TRUE);
				//set the desktop window as active if it's not a popup
				if (desktop_window->GetStyle() != OpWindow::STYLE_POPUP &&
					desktop_window->GetStyle() != OpWindow::STYLE_EXTENSION_POPUP &&
					!(desktop_window->GetStyle() == OpWindow::STYLE_TRANSPARENT && desktop_window->GetParentWindow()) &&
					!desktop_window->IsClosing())
				{
					m_active_toplevel_desktop_window = desktop_window;
				}
			}
		}
		else
		{
			g_input_manager->ResetInput();
		}
	}

	if (activate && desktop_window->GetWorkspace())
	{
		// Only iterate and modify list when activating browser window, not tab.
		if (desktop_window->GetType() == WINDOW_TYPE_BROWSER)
		{
			// Move activated window to the end of top level windows vector
			// so that we know the order in which windows were activated.
			// GetActiveBrowserDesktopWindow() then relies on order to pick up
			// last active window - fixes DSK-270794.
			UINT32 cnt = m_toplevel_desktop_windows.GetCount();
			if (cnt > 1)
				while(cnt)
				{
					cnt--;
					if (m_toplevel_desktop_windows.Get(cnt) == desktop_window)
					{
						m_toplevel_desktop_windows.Remove(cnt);
						m_toplevel_desktop_windows.Add(desktop_window);
						break;
					}
				}
		}

#if defined(SCOPE_SUPPORT) && defined(SCOPE_WINDOW_MANAGER_SUPPORT)
		// Inform scope about activation of desktop window (tab) when
		// switching between Opera windows or just tabs.
		OpWindowCommander* win_comm = desktop_window->GetWindowCommander();

		if (win_comm)
			g_scope_manager->window_manager->ActiveWindowChanged(win_comm->GetWindow());
#endif // SCOPE_SUPPORT && SCOPE_WINDOW_MANAGER_SUPPORT
	}

	// In some rare cases clicking on desktop window does not result in proper
	// toplevel window being activated. It may lead to crashes if
	// m_active_toplevel_desktop_window was just closed.
	// fixes DSK-337863
	if (activate && !m_active_toplevel_desktop_window && parent_desktop_window)
	{
		OnDesktopWindowActivated(parent_desktop_window, TRUE);
	}
}

/***********************************************************************************
**
**	OnDesktopWindowIsCloseAllowed
**
***********************************************************************************/

BOOL ClassicApplication::OnDesktopWindowIsCloseAllowed(DesktopWindow* desktop_window)
{
	if( (desktop_window->GetType() == WINDOW_TYPE_BROWSER) && KioskManager::GetInstance()->GetNoExit() )
	{
		return FALSE;
	}

#ifndef _MACINTOSH_

	if (m_toplevel_desktop_windows.GetCount() == 1 && m_toplevel_desktop_windows.Get(0) == desktop_window)
	{
		if (HandleExitStrategies() == TRUE)
		{
			// already handled, no close allowed (or handled elsewhere)
			return FALSE;
		}
#ifdef M2_SUPPORT
		if ((HasMail() || HasFeeds()) && g_m2_engine->NeedToAskAboutDoingMailRecovery())
		{
			MailRecoveryWarningController* controller = OP_NEW(MailRecoveryWarningController, ());
			RETURN_VALUE_IF_ERROR(ShowDialog(controller,g_global_ui_context,GetActiveDesktopWindow()),TRUE);
			return FALSE;
		}
#endif //M2_SUPPORT
	}
#endif
	return TRUE;
}

/***********************************************************************************
**
**	OnDesktopWindowClosing
**
***********************************************************************************/

void ClassicApplication::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (desktop_window == m_customize_toolbars_dialog)
	{
		m_customize_toolbars_dialog = NULL;
	}

	Type type = desktop_window->GetType();
	if(type == WINDOW_TYPE_BROWSER)
	{
		g_application->BroadcastOnBrowserWindowDeleting(static_cast<BrowserDesktopWindow *>(desktop_window));
	}

	BOOL auto_save = TRUE;

#ifndef _MACINTOSH_
	// save session and exit if this is last top level desktop window being closed by user
	BOOL last_window_closed = m_toplevel_desktop_windows.GetCount() > 0;
	for (UINT32 i = 0; i < m_toplevel_desktop_windows.GetCount(); i++)
	{
		DesktopWindow * window = m_toplevel_desktop_windows.Get(i);
		if (window != desktop_window
#ifdef DEVTOOLS_INTEGRATED_WINDOW
			&& !window->IsDevToolsOnly()
#else
			&& window->GetType() != WINDOW_TYPE_DEVTOOLS // a Developer Tools window doesn't count
#endif
			)
		{
			last_window_closed = FALSE;
			break;
		}
	}
	if (last_window_closed && user_initiated && !IsExiting())
	{
		g_desktop_global_application->ExitStarted();

#ifdef WEBSERVER_SUPPORT
		g_webserver_manager->Shutdown();
#endif // WEBSERVER_SUPPORT

#ifdef SUPPORT_DATA_SYNC
		// If logged in and there is something to sync, we MUST do that before attempting to exit
		if (g_sync_manager && g_sync_manager->IsLinkEnabled() && g_sync_manager->CanSync() && g_sync_coordinator->HasQueuedItems())
		{
			g_session_auto_save_manager->SaveCurrentSession(NULL, TRUE, FALSE);

			m_boot_manager.SetIsExiting();

			// Make sure everything is sync-ed before exiting!
			g_sync_manager->SyncNow(SyncManager::Exit);
		}
		else
#endif // SUPPORT_SYNC
		if (m_boot_manager.TryExit() == 0)
		{
			TRAPD(err,g_session_manager->BackupAutosaveSessionL());
			auto_save = FALSE;
			g_session_auto_save_manager->SaveCurrentSession(NULL, TRUE, FALSE);

			m_boot_manager.SetIsExiting();

			KioskManager::GetInstance()->OnExit();

			g_desktop_global_application->Exit();
		}
	}
#endif // _MACINTOSH_

	if(!IsExiting() && desktop_window && desktop_window->GetType() == WINDOW_TYPE_BROWSER && !desktop_window->PrivacyMode()
#ifdef DEVTOOLS_INTEGRATED_WINDOW
		&& !desktop_window->IsDevToolsOnly()
#endif // DEVTOOLS_INTEGRATED_WINDOW
		)
	{
		// Do not save closed session for empty workspace
		if (desktop_window->GetWorkspace()->GetItemCount() > 0)
			OpStatus::Ignore(static_cast<BrowserDesktopWindow*>(desktop_window)->AddClosedSession());
	}

	desktop_window->RemoveListener(this);

	//(julienp) Makes sure that address bars get notified when closing a tab that is not focused
	for (UINT32 i = 0; i < m_address_bars.GetCount(); i++)
		if (type == WINDOW_TYPE_DOCUMENT)
			m_address_bars.Get(i)->OnTabClosing((DocumentDesktopWindow*)desktop_window);

	if (desktop_window == m_active_toplevel_desktop_window)
	{
		m_active_toplevel_desktop_window = NULL;
	}

	m_toplevel_desktop_windows.RemoveByItem(desktop_window);
	m_window_collection.RemoveDesktopWindow(desktop_window);

	// Only save the session on normal runs otherwise blank session files
	// can be written out if this is invoked during an upgrade
	if (DetermineFirstRunType() == Application::RUNTYPE_NORMAL && auto_save &&
		(type == WINDOW_TYPE_DOCUMENT     ||
		 type == WINDOW_TYPE_BROWSER      ||
		 type == WINDOW_TYPE_MAIL_VIEW    ||
		 type == WINDOW_TYPE_MAIL_COMPOSE ||
#ifdef GADGET_SUPPORT
		 type == WINDOW_TYPE_GADGET       ||
#endif
		 type == WINDOW_TYPE_PANEL ))
	{
		g_session_auto_save_manager->SaveCurrentSession();
	}
}

/***********************************************************************************
**
**	OnDesktopWindowParentChanged
**
***********************************************************************************/

void ClassicApplication::OnDesktopWindowParentChanged(DesktopWindow* desktop_window)
{
	DesktopWindowCollectionItem& item = desktop_window->GetModelItem();

	DesktopWindowCollectionItem* old_parent = item.GetToplevelWindow();
	DesktopWindowCollectionItem* new_parent = NULL;

	if (desktop_window->GetParentDesktopWindow())
	{
		new_parent = &desktop_window->GetParentDesktopWindow()->GetModelItem();
	}

	if (new_parent == old_parent)
	{
		return;
	}

	DesktopWindowCollection::ModelLock lock(item.GetModel());

	// Simulate the effect of this page being closed to alert the right listeners
	OnDesktopWindowClosing(desktop_window, FALSE);
	// But make sure we are still listening ourselves
	OpStatus::Ignore(desktop_window->AddListener(this));

	if (old_parent)
		m_window_collection.ReorderByItem(item, new_parent, new_parent->GetLastChildItem());
	else
		AddDesktopWindow(desktop_window, desktop_window->GetParentDesktopWindow());

	g_session_auto_save_manager->SaveCurrentSession();
}

void ClassicApplication::OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height)
{
	if (m_popup_desktop_window != NULL && m_popup_desktop_window != desktop_window
			// Only old-style pop-ups (which use STYLE_NOTIFIER) require
			// closing  here.  QuickDialog-based pop-ups are able to adapt
			// their position to the resized parent window.
			&& m_popup_desktop_window->GetStyle() == OpWindow::STYLE_NOTIFIER)
	{
		SetPopupDesktopWindow(NULL);
	}
}


/***********************************************************************************
**
**	GetTypeString
**
***********************************************************************************/

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS ClassicApplication::GetTypeString(OpString& type_string)
{
	return g_languageManager->GetString(Str::S_WINDOW_HEADER, type_string);
}

OP_STATUS ClassicApplication::AccessibilityGetAbsolutePosition(OpRect &rect)
{
	rect.Empty();
	return OpStatus::OK;
}

OP_STATUS ClassicApplication::AccessibilityGetText(OpString& str)
{
	str.Empty();
	return OpStatus::OK;
}

Accessibility::State ClassicApplication::AccessibilityGetState()
{
	return Accessibility::kAccessibilityStateNone;
}

Accessibility::ElementKind ClassicApplication::AccessibilityGetRole() const
{
	return Accessibility::kElementKindApplication;
}

int ClassicApplication::GetAccessibleChildrenCount()
{
	return m_toplevel_desktop_windows.GetCount();
}

OpAccessibleItem* ClassicApplication::GetAccessibleChild(int n)
{
	DesktopWindow* win = m_toplevel_desktop_windows.Get(n);
	if (win)
	{
		OpWindow* opwin = win->GetOpWindow();
		if (opwin)
		{
			return win;
		}
	}
	return NULL;
}

int ClassicApplication::GetAccessibleChildIndex(OpAccessibleItem* child)
{
	int index = m_toplevel_desktop_windows.Find(static_cast<DesktopWindow*>(child));
	if (index == -1)
		return Accessibility::NoSuchChild;
	else
		return index;
}

OpAccessibleItem* ClassicApplication::GetAccessibleChildOrSelfAt(int x, int y)
{
	UINT32 i, count = m_toplevel_desktop_windows.GetCount();
	OpRect bounds;
	OpPoint pt(x, y);
	for (i=0; i<count; i++)
	{
		DesktopWindow* win = m_toplevel_desktop_windows.Get(i);
		if (win)
		{
			win->GetRect(bounds);
			if (bounds.Contains(pt))
			{
				OpWindow* opwin = win->GetOpWindow();
				if (opwin)
				{
					return win;
				}
			}
		}
	}
	return NULL;
}

OpAccessibleItem* ClassicApplication::GetLeftAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* ClassicApplication::GetRightAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* ClassicApplication::GetDownAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* ClassicApplication::GetUpAccessibleObject()
{
	return NULL;
}

OpAccessibleItem* ClassicApplication::GetAccessibleFocusedChildOrSelf()
{
	if (m_active_toplevel_desktop_window)
	{
		return m_active_toplevel_desktop_window;
	}
	return this;
}

#endif

BOOL ClassicApplication::GetShowGoOnlineDialog()
{
	return m_confirm_online_dialog == FALSE;
}

void ClassicApplication::SetConfirmOnlineShow()
{
	m_confirm_online_dialog = TRUE;
}

void ClassicApplication::AddOfflineQueryCallback(OpDocumentListener::DialogCallback* callback)
{
	m_offline_mode_callbacks.Add(callback);
}

void ClassicApplication::UpdateOfflineMode(OpDocumentListener::DialogCallback::Reply reply)
{
	for (UINT32 i = 0; i < m_offline_mode_callbacks.GetCount(); i++)
	{
		OpDocumentListener::DialogCallback * callback = m_offline_mode_callbacks.Get(i);
		if (callback)
		{
			callback->OnDialogReply(reply);
		}
	}
	m_offline_mode_callbacks.Clear();
	m_confirm_online_dialog = FALSE;
}

void ClassicApplication::AskEnterOnlineMode(BOOL test_offline_mode_first, OnlineModeCallback* callback, Str::LocaleString dialog_message, Str::LocaleString dialog_title)
{
	if( test_offline_mode_first &&
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode)
	  )
	{
		if(callback)
		{
			callback->OnOnlineMode();
		}
		OP_DELETE(callback);
		return;
	}

	SimpleDialogController* controller = OP_NEW(EnterOnlineModeDialogController, (callback, dialog_message, dialog_title));
	if (!controller)
	{
		OP_DELETE(callback);
		return;
	}

	OpStatus::Ignore(ShowDialog(controller, g_global_ui_context, GetActiveDesktopWindow()));
}

BOOL ClassicApplication::AskEnterOnlineMode(BOOL test_offline_mode_first)
{
	if( test_offline_mode_first &&
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode)
	  )
	{
		return TRUE;
	}

	SimpleDialogController* controller = OP_NEW(SimpleDialogController,
		(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_QUESTION,
		WINDOW_NAME_ASK_ENTER_ONLINE,Str::S_OFFLINE_TO_ONLINE, Str::D_OFFLINE_MODE_TITLE));
	if (!controller)
		return FALSE;

	SimpleDialogController::DialogResultCode result;
	controller->SetBlocking(result);				
	if (OpStatus::IsError(ShowDialog(controller, g_global_ui_context,GetActiveDesktopWindow())))
		return FALSE;
				
	if(result == SimpleDialogController::DIALOG_RESULT_CANCEL)
	{
		return FALSE;
	}

	OP_BOOLEAN is_offline = UpdateOfflineMode(FALSE);
	if (is_offline == OpBoolean::IS_TRUE)
	{
		// End offline mode if still on. We may have a number of
		// requests pending and we may already be in online mode.
		UpdateOfflineMode(TRUE);
	}

	return TRUE;
}

OP_BOOLEAN ClassicApplication::UpdateOfflineMode(BOOL toggle)
{
	OP_BOOLEAN ret = ::UpdateOfflineMode(toggle);

	RETURN_IF_ERROR(ret);

#ifdef M2_SUPPORT
	if (toggle && ShowM2())
	{
		RETURN_IF_ERROR(g_m2_engine->SetOfflineMode(ret == OpBoolean::IS_TRUE ? TRUE : FALSE));
	}
#endif // M2_SUPPORT

	return ret;
}

/***********************************************************************************
**
**	CustomizeToolbars
**
***********************************************************************************/

BOOL ClassicApplication::CustomizeToolbars(OpToolbar* target_toolbar, INT32 initial_page)
{
	if (m_customize_toolbars_dialog)
	{
		m_customize_toolbars_dialog->Activate();
		m_customize_toolbars_dialog->SetTargetToolbar(target_toolbar);
	}
	else
	{
		m_customize_toolbars_dialog = OP_NEW(CustomizeToolbarDialog, ());
		if(m_customize_toolbars_dialog)
		{
			if (OpStatus::IsError(m_customize_toolbars_dialog->Init(GetActiveDesktopWindow(), initial_page)))
				m_customize_toolbars_dialog = NULL;
			else
				m_customize_toolbars_dialog->SetTargetToolbar(target_toolbar);
		}
	}
	return TRUE;
}

CustomizeToolbarDialog*	ClassicApplication::GetCustomizeToolbarsDialog()
{
	// No customising toolbars in kiosk mode!
	if (KioskManager::GetInstance()->GetEnabled() && KioskManager::GetInstance()->GetNoChangeButtons())
		return NULL;

	return m_customize_toolbars_dialog;
}


BOOL ClassicApplication::IsDragCustomizingAllowed()
{
	INT32 shift_state = g_op_system_info->GetShiftKeyState();
# ifdef _MACINTOSH_
	return m_customize_toolbars_dialog || (shift_state & SHIFTKEY_SHIFT) || (shift_state & SHIFTKEY_CTRL);
# else
	return m_customize_toolbars_dialog || (shift_state & SHIFTKEY_SHIFT);
# endif

}

BOOL ClassicApplication::IsCustomizingHiddenToolbars()
{
	return m_customize_toolbars_dialog && m_customize_toolbars_dialog->GetShowHiddenToolbarsWhileCustomizing();
}

/***********************************************************************************
**
**	ExecuteProgram
**
***********************************************************************************/

void ClassicApplication::ExecuteProgram(const uni_char* application, const uni_char* parameters, BOOL run_in_terminal, const uni_char* protocol)
{
#if defined (DU_REMOTE_URL_HANDLER) && !defined (MSWIN)
	if (parameters)
		RemoteURLHandler::AddURLtoList(parameters);
	else if (application)
		RemoteURLHandler::AddURLtoList(application);
#endif // DU_REMOTE_URL_HANDLER
#if defined(_UNIX_DESKTOP_)
	FileHandlerManager::GetManager()->ExecuteApplication(protocol, application, parameters, run_in_terminal);
#elif !defined (MSWIN)
	OpString program;
	program.Set(application);
	UnQuote(program); //Remove quotes from string
#endif

#if defined(MSWIN)
	g_op_system_info->ExecuteApplication(application, parameters);
#elif defined(_MACINTOSH_)
    OpStatus::Ignore(MacExecuteProgram(program.CStr(), parameters));
//	MacOpenFileInApplication(parameters, program);
#endif
}

static BOOL validProgram(OpString & program, BOOL quote)
{
    program.Strip();

    if (program.IsEmpty())
		return FALSE;

    OpString tmp;

    if(!quote)
    {
		tmp.Append("\"");
		tmp.Append(program.CStr());
		tmp.Append("\"");
    }
    else
    {
		tmp.Append(program.CStr());
    }

    OpAutoVector<OpString> substrings;
    StringUtils::SplitString( substrings, tmp, ' ', TRUE );

	/* Note: the following is the only reason why GetIsExecutable needs to
	 * search a PATH evironment variable on Unix (and doubtless equivalents
	 * elsewhere): all other calls to GetIsExecutable reliably specify a full
	 * path for the file. */
    return substrings.Get(0) && substrings.Get(0)->HasContent() && g_op_system_info->GetIsExecutable(substrings.Get(0));
}

BOOL ClassicApplication::CanViewSourceOfURL(URL * url)
{
	// Only show the source view for certain types of documents
	switch (url->ContentType())
	{
		case URL_HTML_CONTENT:
		case URL_XML_CONTENT:
		case URL_TEXT_CONTENT:
		case URL_CSS_CONTENT:
		case URL_SVG_CONTENT:
		case URL_X_JAVASCRIPT:
		case URL_WML_CONTENT:

			return TRUE;
		break;

		default:
			break;
	}

	return FALSE;
}

void ClassicApplication::OpenSourceViewer(URL *url, UINT32 window_id, const uni_char* title, BOOL frame_source)
{
	if(!CanViewSourceOfURL(url))
		return;

#ifdef URL_CAP_PREPAREVIEW_FORCE_TO_FILE
	url->PrepareForViewing(URL::KFollowRedirect, TRUE, TRUE, TRUE);
#else
	url->PrepareForViewing(TRUE);
#endif
	OpString filename, suggested_filename;

#if 0
	// Only create the file in temp/cache for non local files
	if (url->GetAttribute(URL::KType) != URL_FILE)
	{
		// Fix for bug #321393, will cause the file to be deleted when the cache is deleted
		url->SetAttribute(URL::KUnique, TRUE);

		// Build a suggested filename
		TRAPD(err1, url->GetAttributeL(URL::KSuggestedFileName_L, suggested_filename, TRUE));
		if (err1!=OpStatus::OK || suggested_filename.IsEmpty())
			suggested_filename.Set(UNI_L("index.html"));

		// Save the cache file into a file locally. LoadToFile should update all the cache links
		// Build a nice filename
		OpString tmp_storage;
		const uni_char * temp_folder = g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_CACHE_FOLDER, tmp_storage);

		// Add the temp path
		if (temp_folder)
			filename.Set(temp_folder);

		// Add a path sep if needed
		int len = filename.Length();
		if(len > 0 && filename[len-1] != PATHSEPCHAR)
			filename.Append(PATHSEP);

		// Add the suggested filename
		filename.Append(suggested_filename);

		// Make sure that the filename is unique
		if (OpStatus::IsSuccess(CreateUniqueFilename(filename)))
		{
			// Copy to the temp folder
			url->LoadToFile(filename);
		}
	}
#endif // 0

	TRAPD(err, url->GetAttributeL(URL::KFilePathName_L, filename, TRUE));
	if (err!=OpStatus::OK || filename.IsEmpty())
	{
		return;
	}

	BOOL run_in_terminal = FALSE;

	INT32 viewer_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::SourceViewerMode);

	// Test if we are going to use our internal viewer by checking if the application is Opera or blank
	OpString program;
	BOOL quote = FALSE;

	if(viewer_mode == UseCustomApplication)
	{
		program.Set(g_pcapp->GetStringPref(PrefsCollectionApp::SourceViewer));
		program.Strip();
		quote = program.FindFirstOf('"') != -1;
	}
	else if(viewer_mode == UseDefaultApplication)
	{
		OpString dummy_filename;
		dummy_filename.Set("txt.txt");
		OpString dummy_content_type;
		dummy_content_type.Set("text/plain");

		g_op_system_info->GetFileHandler(&dummy_filename, dummy_content_type, program);
		quote = TRUE;
	}

	//If no valid program has been found and viewer mode is external - make user change prefs:
	if (!validProgram(program, quote)  && viewer_mode != UseInternalApplication)
	{	
		SimpleDialogController* controller = OP_NEW(SimpleDialogController,
			(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_INFO,
			 WINDOW_NAME_YES_NO,Str::D_EXTERNAL_CLIENT_MISSING, Str::DI_IDM_SRC_VIEWER_BOX));
		if (!controller)
			return;
				
		SimpleDialogController::DialogResultCode rc;
		controller->SetBlocking(rc);				
		if (OpStatus::IsError(ShowDialog(controller, g_global_ui_context,GetActiveDesktopWindow())))
			return;
				
		if(rc == SimpleDialogController::DIALOG_RESULT_OK)
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_PREFERENCES, NewPreferencesDialog::ProgramPage);
		}
		return;
	}
	else
	{
#ifdef _UNIX_DESKTOP_
	    run_in_terminal = g_pcapp->GetIntegerPref(PrefsCollectionApp::RunSourceViewerInTerminal);
#endif //_UNIX_DESKTOP_
	}

	BOOL is_detached_window = FALSE;
	OpRect preferred_rect;

	if (viewer_mode == UseInternalApplication)
	{
		OpWorkspace* parent_workspace = NULL;

		if (!IsSDI())
		{
			// If there is a window id we can find the correct browser desktop window to add the tab to
			if (window_id)
			{
				DesktopWindow *desktop_window = m_window_collection.GetDesktopWindowByID((INT32)window_id);
				if (desktop_window)
				{
					parent_workspace = desktop_window->GetWorkspace();
					is_detached_window = parent_workspace == 0;
					if( is_detached_window )
					{
						OpWindow::State state;
						desktop_window->GetOpWindow()->GetDesktopPlacement(preferred_rect, state);
						preferred_rect.x = preferred_rect.x + 20;
						preferred_rect.y = preferred_rect.y + 20;
					}
				}
			}
			else
			{
				// No window ID so just go for the active
				BrowserDesktopWindow *active_browser_view = g_application->GetActiveBrowserDesktopWindow();

				// If there is an active desktop then dump the new tab in it
				if (active_browser_view)
					parent_workspace = active_browser_view->GetWorkspace();
				else
				{
					// We couldn't find an active browser window but there still might
					// be one hiding behind the console window, if it is up.
					parent_workspace = m_window_collection.GetWorkspace();
				}
			}
		}

		if( !is_detached_window && !parent_workspace)
		{
			// Create new workspace if caller window is not a tached window
			parent_workspace = GetBrowserDesktopWindow(TRUE)->GetWorkspace();
		}

		// Create a new source desktop window
		SourceDesktopWindow* window = OP_NEW(SourceDesktopWindow, (parent_workspace, url, title, window_id, frame_source));
		if (window)
		{
			if (OpStatus::IsError(window->init_status))
				window->Close(TRUE);
			else
				window->Show(TRUE, is_detached_window ? &preferred_rect : 0);
		}
	}
	else
	{
		//Execute the program:
		OpString parameters;
		parameters.AppendFormat(UNI_L("\"%s\""), filename.CStr());
		ExecuteProgram(program.CStr(), parameters.CStr(), run_in_terminal);
	}
}

/***********************************************************************************
**
**	OnSubmit
**
***********************************************************************************/

OP_STATUS ClassicApplication::OnSubmit(WandInfo* info)
{
   
#ifdef WIDGET_RUNTIME_SUPPORT
	// this is copied from GadgetApplication. 
	// seems to be unecessary, however if widget running in debug mode (run by confi.xml d'n'd)
	// it ends up here with storing passowrds. 
	if ( info->GetOpWindowCommander() && info->GetOpWindowCommander()->GetOpWindow())
	{
		if (info->GetOpWindowCommander()->GetOpWindow()->GetStyle()== OpWindow::STYLE_GADGET && info->page->CountPasswords() > 0)
		{
			OpAutoPtr<GadgetConfirmWandDialog> dialog(
					OP_NEW(GadgetConfirmWandDialog, ()));
			RETURN_OOM_IF_NULL(dialog.get());
			RETURN_IF_ERROR(dialog->Init(GetActiveDesktopWindow(TRUE), info));
			dialog.release();
			return OpStatus::OK;
		}
	}
#endif //WIDGET_RUNTIME_SUPPORT   
   

	//
	// Extensions can also trigger wand to store password.
	// Problem is that we don't want to see the toolbar in a window below.
	// So - we need to check if window commander is the same as the current
	// active window one. If not - we shouldn't show anything here
	//
	if (GetActiveDocumentDesktopWindow() && info->GetOpWindowCommander() &&
		GetActiveDocumentDesktopWindow()->GetWindowCommander() != info->GetOpWindowCommander())
	{
		return OpStatus::OK;
	}

	if (!IsEmBrowser())
	{
#ifdef WAND_ECOMMERCE_SUPPORT
		if (info->page->CountPasswords() > 0)
#endif // WAND_ECOMMERCE_SUPPORT
		{
			if (GetActiveDocumentDesktopWindow())
				GetActiveDocumentDesktopWindow()->ShowWandStoreBar(info);
			else
				info->ReportAction(WAND_DONT_STORE);
			return OpStatus::OK;
		}

#ifdef WAND_ECOMMERCE_SUPPORT
		// Not password -> only eCommerce data!
		ConfirmWandECommerceDialog* dialog = OP_NEW(ConfirmWandECommerceDialog, (WAND_DONT_STORE));
		if (!dialog)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS status = dialog->Init(GetActiveDesktopWindow(), info);
		if (OpStatus::IsError(status))
			OP_DELETE(dialog);
		return status;
#endif // WAND_ECOMMERCE_SUPPORT
	}
	else
	{
		info->ReportAction(WAND_DONT_STORE);
	}
	return OpStatus::OK;
}

/***********************************************************************************
**
**	OnSelectMatch
**
***********************************************************************************/

OP_STATUS ClassicApplication::OnSelectMatch(WandMatchInfo* info)
{
	WandMatchDialog* dialog = OP_NEW(WandMatchDialog, ());
	if (!dialog)
		return OpStatus::ERR_NO_MEMORY;

	return dialog->Init(GetActiveDesktopWindow(), info);
}

/***********************************************************************************
**
**	OpenExternalURLProtocol
**
***********************************************************************************/

BOOL ClassicApplication::OpenExternalURLProtocol(URL& url)
{
	BOOL handled = FALSE;
	OpString url_string;
	url.GetAttribute(URL::KUniName, url_string);
#ifdef DU_REMOTE_URL_HANDLER
	RemoteURLHandler::AddURLtoList(url_string.CStr());
#endif // DU_REMOTE_URL_HANDLER

	OpString app_description,open_command;
	RETURN_VALUE_IF_ERROR(g_op_system_info->GetProtocolHandler(app_description,url_string,open_command),FALSE);

	BOOL open_anyway = FALSE;

#ifndef _UNIX_DESKTOP_
	if (!DocumentManager::IsTrustedExternalURLProtocol(url_string.CStr()))
	{
		if (!app_description.HasContent())
		{
			app_description.Set(open_command);
		}
		if (app_description.HasContent())
		{
			OpString caption, question, question_format;
			
			RETURN_VALUE_IF_ERROR(g_languageManager->GetString(Str::D_UNKNOWN_PROTOCOL_TITLE, caption), handled);
			RETURN_VALUE_IF_ERROR(g_languageManager->GetString(Str::D_UNKNOWN_PROTOCOL_TEXT, question_format), handled);
			RETURN_VALUE_IF_ERROR(I18n::Format(question, Str::D_UNKNOWN_PROTOCOL_TEXT, app_description, url_string), handled);

			SimpleDialogController* controller = OP_NEW(SimpleDialogController,
				(SimpleDialogController::TYPE_YES_NO, SimpleDialogController::IMAGE_QUESTION,WINDOW_NAME_YES_NO, question, caption));
			if (!controller)
				return handled;

			SimpleDialogController::DialogResultCode res;
			bool do_not_show_again = false;
			controller->SetBlocking(res);
			controller->SetDoNotShowAgain(do_not_show_again);
			if (OpStatus::IsError(ShowDialog(controller, g_global_ui_context,GetActiveDesktopWindow())))
				return handled;

			if (SimpleDialogController::DIALOG_RESULT_OK == res)
			{
				if (do_not_show_again)
				{
					OpString protocol_name;
					if (OpStatus::IsSuccess(protocol_name.Set(url.GetAttribute(URL::KProtocolName))))
					{
						int num_trusted_protocols = g_pcdoc->GetNumberOfTrustedProtocols();

						TrustedProtocolData data;
						data.protocol = protocol_name;
						data.viewer_mode = UseDefaultApplication;
						data.flags = TrustedProtocolData::TP_Protocol | TrustedProtocolData::TP_ViewerMode;

						BOOL success = FALSE;
						TRAPD(err,success = g_pcdoc->SetTrustedProtocolInfoL(num_trusted_protocols, data));

						if (OpStatus::IsSuccess(err) && success)
						{
							num_trusted_protocols++;
							// We accept that the functions can fail. Open url in any case.
							TRAP(err,g_pcdoc->WriteTrustedProtocolsL(num_trusted_protocols));
							OP_ASSERT(OpStatus::IsSuccess(err));
							TRAP(err, g_prefsManager->CommitL());
							OP_ASSERT(OpStatus::IsSuccess(err));
						}
					}
				}
				open_anyway = TRUE;
			}
			handled = TRUE;
		}
	}
#endif // _UNIX_DESKTOP_

	if (open_anyway || DocumentManager::IsTrustedExternalURLProtocol(url_string.CStr()))
	{
		if (OpStatus::IsSuccess(g_desktop_op_system_info->OpenURLInExternalApp(url)))
			handled = TRUE;
	}

	return handled;
}

void ClassicApplication::GetStartupPage(Window* window, STARTUPTYPE type)
{
	if (!window && type != STARTUP_HOME)
		return;

	BOOL new_window = FALSE;

	switch (type)
	{
		case STARTUP_HISTORY:
		{
#ifdef VEGA_OPPAINTER_SUPPORT
			QuickDisableAnimationsHere dummy;
#endif
			// Lock session writing during this call because we are still loading the session!
			// Writing it at this time (as a result of some callback) would write only parts of the session.
			g_session_auto_save_manager->LockSessionWriting();
			window->GotoHistoryPos();
			g_session_auto_save_manager->UnlockSessionWriting();
		}
		return;
    case STARTUP_WINHOMES:
		GetWindowHome(window, TRUE, TRUE);
        return;
#ifdef MSWIN
	case STARTUP_ONCE:
#endif // MSWIN
	case STARTUP_HOME:
		new_window = !window;
		break;

	default:
		new_window = TRUE;
		break;
	}

	const uni_char* url;

#ifdef MSWIN
	if (type == STARTUP_ONCE)
		url = g_pcui->GetStringPref(PrefsCollectionUI::OneTimePage).CStr();
	else
#endif // MSWIN
		url = g_pccore->GetStringPref(PrefsCollectionCore::HomeURL).CStr();

	if (IsStr(url))
	{
		if (new_window)
		{
			BOOL3 open_in_new_window = YES;
			BOOL3 open_in_background = MAYBE;
			window = windowManager->GetAWindow(TRUE, open_in_new_window, open_in_background);
		}
		if (window)
		{
			window->OpenURL(url, TRUE, TRUE);
		}
	}

#ifdef MSWIN
	if (type == STARTUP_ONCE)
	{
		window->Raise();

		// Clear the one-time page
		TRAPD(rc, g_pcui->WriteStringL(PrefsCollectionUI::OneTimePage, UNI_L(""))); // FIXME:OOM
		OP_ASSERT(OpStatus::IsSuccess(rc)); // FIXME: do what if rc is bad ?
	}
#endif // MSWIN
}

void ClassicApplication::PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue)
{
	if (PrefsCollectionUI::ProgramTitle == PrefsCollectionUI::stringpref(pref))
	{
		OpVector<DesktopWindow> browser_windows;
		RETURN_VOID_IF_ERROR(m_window_collection.GetDesktopWindows(OpTypedObject::WINDOW_TYPE_BROWSER, browser_windows));

		// update all BrowserDesktopWindows to show the new title
		for(UINT32 i = 0; i < browser_windows.GetCount(); i++)
			static_cast<BrowserDesktopWindow*>(browser_windows.Get(i))->UpdateWindowTitle();
	}
}

void ClassicApplication::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (PrefsCollectionUI::ExtendedKeyboardShortcuts == PrefsCollectionUI::integerpref(pref))
	{
		//Reload the keyboard.ini file to disable or enabled the extra keyboard shortcuts
		SettingsChanged(SETTINGS_KEYBOARD_SETUP);
	}
	else if(PrefsCollectionUI::ShowMenu == PrefsCollectionUI::integerpref(pref))
	{
		// Show menu setting has changed
		if(g_pcui->GetIntegerPref(PrefsCollectionUI::ShowMenu))
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_ENABLE_MENU_BAR, 1);
		}
		else
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_DISABLE_MENU_BAR, 1);
		}
	}
}

void ClassicApplication::HostOverrideChanged(OpPrefsCollection::Collections id, const uni_char *hostname)
{

}

BOOL ClassicApplication::AnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info)
{
	return m_anchor_special_manager.AnchorSpecial(commander, info);
}

OP_STATUS ClassicApplication::AddAddressBar(OpAddressDropDown *address_bar)
{
	return m_address_bars.Add(address_bar);
}

OP_STATUS ClassicApplication::RemoveAddressBar(OpAddressDropDown *address_bar)
{
	return m_address_bars.RemoveByItem(address_bar);
}

void ClassicApplication::BroadcastOnBrowserWindowCreated(BrowserDesktopWindow *window)
{
	for (OpListenersIterator iterator(m_browserwindow_listeners); m_browserwindow_listeners.HasNext(iterator);)
	{
		m_browserwindow_listeners.GetNext(iterator)->OnBrowserDesktopWindowCreated(window);
	}
}

void ClassicApplication::BroadcastOnBrowserWindowActivated(BrowserDesktopWindow *window)
{
	for (OpListenersIterator iterator(m_browserwindow_listeners); m_browserwindow_listeners.HasNext(iterator);)
	{
		m_browserwindow_listeners.GetNext(iterator)->OnBrowserDesktopWindowActivated(window);
	}
}

void ClassicApplication::BroadcastOnBrowserWindowDeactivated(BrowserDesktopWindow *window)
{
	for (OpListenersIterator iterator(m_browserwindow_listeners); m_browserwindow_listeners.HasNext(iterator);)
	{
		m_browserwindow_listeners.GetNext(iterator)->OnBrowserDesktopWindowDeactivated(window);
	}
}

void ClassicApplication::BroadcastOnBrowserWindowDeleting(BrowserDesktopWindow *window)
{
	for (OpListenersIterator iterator(m_browserwindow_listeners); m_browserwindow_listeners.HasNext(iterator);)
	{
		m_browserwindow_listeners.GetNext(iterator)->OnBrowserDesktopWindowDeleting(window);
	}
}


/***********************************************************************************
**
**  HandleExitStrategies
**
***********************************************************************************/

BOOL
ClassicApplication::HandleExitStrategies()
{
	BOOL hide = FALSE;
	INT32 exit_strategy;

	if (g_desktop_transfer_manager && g_desktop_transfer_manager->HasActiveTransfer())
	{
		exit_strategy = g_pcui->GetIntegerPref(PrefsCollectionUI::WarnAboutActiveTransfersOnExit);
		if (exit_strategy == ExitStrategyConfirm)
		{
			ConfirmExitDialog* dialog = OP_NEW(ConfirmExitDialog, (PrefsCollectionUI::WarnAboutActiveTransfersOnExit));
			if (dialog)
			{
				if (OpStatus::IsSuccess(dialog->Init(Str::D_ACTIVE_TRANSFERS_CAPTION, Str::D_ACTIVE_TRANSFERS_MESSAGE, GetActiveDesktopWindow())))
				{
					return TRUE;
				}
			}
		}
		hide = hide || (exit_strategy == ExitStrategyHide);
	}
	if (g_webserver_manager && g_webserver_manager->IsFeatureEnabled())
	{
		exit_strategy = g_pcui->GetIntegerPref(PrefsCollectionUI::OperaUniteExitPolicy);
		if (exit_strategy == ExitStrategyConfirm)
		{
			ConfirmExitDialog* dialog = OP_NEW(ConfirmExitDialog, (PrefsCollectionUI::OperaUniteExitPolicy));
			if (dialog)
			{
				if (OpStatus::IsSuccess(dialog->Init(Str::DI_EXITDIALOG, Str::D_UNITE_EXIT_WARNING, GetActiveDesktopWindow())))
				{
					return TRUE;
				}
			}
		}
		hide = hide || (exit_strategy == ExitStrategyHide);
	}

	exit_strategy = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowExitDialog);
	if (exit_strategy == ExitStrategyConfirm)
	{
		ConfirmExitDialog* dialog = OP_NEW(ConfirmExitDialog, (PrefsCollectionUI::ShowExitDialog));
		if (dialog)
		{
			if (OpStatus::IsSuccess(dialog->Init(Str::DI_EXITDIALOG, Str::D_EXIT_OPERA_MESSAGE, GetActiveDesktopWindow())))
			{
				return TRUE;
			}
		}
	}
	hide = hide || (exit_strategy == ExitStrategyHide);

#if !defined(_MACINTOSH_)
	if (hide)
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_HIDE_OPERA, 1);
		return TRUE;
	}
#endif // !defined(_MACINTOSH_)

	return FALSE;
}


OP_STATUS ClassicApplication::Start()
{
	const OP_STATUS result = m_boot_manager.StartBrowser();
	if (OpStatus::IsSuccess(result))
	{
		m_ui_context->SetEnabled(TRUE);
#ifdef SELFTEST
		SELFTEST_RUN( 1 );
#endif
	}

#ifdef GADGET_UPDATE_SUPPORT
	m_update_controller.StartScheduler();
#endif //GADGET_UPDATE_SUPPORT

	return result;
}


void ClassicApplication::Exit(BOOL force)
{
	if (!force)
	{
		if (KioskManager::GetInstance()->GetNoExit())
		{
			return;
		}
		
		if (HandleExitStrategies() == TRUE) // already handled
		{
			return;
		}
#ifdef M2_SUPPORT
		if ((HasMail() || HasFeeds()) && g_m2_engine->NeedToAskAboutDoingMailRecovery())
		{
			MailRecoveryWarningController* controller = OP_NEW(MailRecoveryWarningController, ());
			
			ShowDialog(controller,g_global_ui_context,GetActiveDesktopWindow());
			return;
		}
#endif //M2_SUPPORT

	}
	g_desktop_global_application->ExitStarted();

#ifdef WEBSERVER_SUPPORT
	if (g_webserver_manager) g_webserver_manager->Shutdown();
#endif // WEBSERVER_SUPPORT

#ifdef SUPPORT_DATA_SYNC
	// If logged in and there is something to sync, we MUST do that before attempting to exit
	if (g_sync_manager && g_sync_manager->IsLinkEnabled() && g_sync_manager->CanSync()
#ifdef SY_CAP_HASQUEUEDITEMS
		&& g_sync_coordinator->HasQueuedItems()
#endif // SY_CAP_HASQUEUEDITEMS
		)
	{
		g_session_auto_save_manager->SaveCurrentSession(NULL, TRUE, FALSE);

		// Make sure everything is sync-ed before exiting!
		g_sync_manager->SyncNow(SyncManager::Exit);
	}
	else
#endif // SUPPORT_SYNC
	if (m_boot_manager.TryExit() == 0){

		TRAPD(err,g_session_manager->BackupAutosaveSessionL());
		if (g_session_auto_save_manager) g_session_auto_save_manager->SaveCurrentSession(NULL, TRUE, FALSE);
		m_boot_manager.SetIsExiting();
		KioskManager::GetInstance()->OnExit();

		g_desktop_global_application->Exit();
	}
	
#ifdef _MACINTOSH_
	PostDummyEvent();
#endif // _MACINTOSH_	
}

UINT32 ClassicApplication::GetClipboardToken()
{
	BrowserDesktopWindow* win = GetActiveBrowserDesktopWindow();
	if (win && win->PrivacyMode())
		return windowManager->GetPrivacyModeContextId();

	return 0;
}

void ClassicApplication::SetPopupDesktopWindow(DesktopWindow* win)
{
	if (m_popup_desktop_window && m_popup_desktop_window != win)
		m_popup_desktop_window->Close();
	m_popup_desktop_window = win;
}
