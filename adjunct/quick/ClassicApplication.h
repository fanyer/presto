/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef CLASSIC_APPLICATION_H
#define CLASSIC_APPLICATION_H

#include "adjunct/quick/Application.h"
#include "adjunct/quick/application/OpBootManager.h"
#include "adjunct/quick/application/BrowserApplicationCacheListener.h"

#include "adjunct/quick/extensions/ExtensionContext.h"

#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "modules/util/OpTypedObject.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/opstring.h"
#include "modules/util/gen_str.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/prefsfile/prefssection.h"

#include "modules/wand/wandmanager.h"
#include "adjunct/quick_toolkit/widgets/OpToolTip.h"

#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/util/OpSharedPtr.h"
#include "adjunct/desktop_util/sessions/opsession.h"
#include "adjunct/quick/application/DesktopOpAuthenticationListener.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/spellcheck/SpellCheckContext.h"

#ifdef USE_COMMON_RESOURCES
#include "adjunct/desktop_util/resources/ResourceDefines.h"
#endif // USE_COMMON_RESOURCES

#if defined(LIBSSL_AUTO_UPDATE)
# include "adjunct/autoupdate/scheduler/optaskscheduler.h"
#endif

#ifdef M2_SUPPORT
# include "adjunct/m2/src/include/enums.h"
#endif // M2_SUPPORT

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# include "modules/accessibility/opaccessibleitem.h"
# ifdef FEATURE_UI_TEST
#  include "adjunct/ui_test_framework/OpUiController.h"
# endif // FEATURE_UI_TEST
#endif

#ifdef GADGET_UPDATE_SUPPORT
#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"
#include "adjunct/autoupdate_gadgets/ExtensionsUpdater.h"
#endif //GADGET_UPDATE_SUPPORT

class OpTaskbar;
class Message;
class OpPoint;
class PrefsEntry;
class OpToolbar;
class OpToolTipListener;
class BrowserDesktopWindow;
class DocumentDesktopWindow;
class MailDesktopWindow;
class ChatDesktopWindow;
class ComposeDesktopWindow;
class PanelDesktopWindow;
class OpSession;
class TransfersPanel;
class Dialog;
class SessionAutoSaveManager;
class OpMultimediaPlayer;
class CustomizeToolbarDialog;
class OpLanguageManager;
class DesktopGadget;
class OpLineParser;
class OpAddressDropDown;
class OpenURLSetting;
class ChatInfo;
class ManagerHolder;
class DesktopWindow;
class DesktopMenuContext;
class ClassicGlobalUiContext;
class OpInputContext;
class OpWindowCommander;
class OpInputAction;
class OpStartupSequence;
class BrowserMenuHandler;
class PageLoadDispatcher;

#ifdef EMBROWSER_SUPPORT
extern long gVendorDataID;
#endif // EMBROWSER_SUPPORT


/***********************************************************************************
**
**	ClassicApplication
**
***********************************************************************************/

class ClassicApplication :
					public Application,
					public OpTypedObject,
					public DesktopWindowListener,
					public MessageObject,
#ifdef WAND_SUPPORT
					public WandListener,
#endif
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
					public OpAccessibleItem,
# ifdef FEATURE_UI_TEST
					public OpUiController,
# endif // FEATURE_UI_TEST
#endif
					public OpPrefsListener
{
	public: 

		explicit ClassicApplication(OpStartupSequence& browser_startup_sequence);
		virtual					~ClassicApplication();

		/**
		 * Called right after construction.
		 */
		OP_STATUS Init();

		/**
		 * Allows to accomplish a two-stage ClassicApplication destruction.
		 *
		 * This must be called prior to destroying the Desktop managers, or we
		 * get into trouble for some exit paths (as is the case with selftests.)
		 *
		 * It cannot be in the destructor, because the managers are destroyed
		 * before Application as they are allowed to assume g_application is
		 * always available.
		 */
		void Finalize();

		/***************************** Application::Start Helper Functions *****************************/
		/* Wrapper functions for the StartupSetting container. It is used to stored commandline parameters,
		 * the settings are used during Opera::Start. Appearantly only used by UNIX.
		 */
		void SetStartupSetting( const StartupSetting &setting )
		{ m_boot_manager.SetStartupSetting(setting); }

		OP_STATUS				CreateSessionWindows(OpSharedPtr<OpSession> session, BOOL insert = FALSE);

		BOOL					ShowOpera(BOOL show, BOOL force_activate = FALSE);

		void					SettingsChanged(SettingsType setting);
		void					SyncSettings();

		/* Checks if an address is local, and if so it returns the address in a form that can be used locally
		 * @param address		An unresolved address that might point to a local file/directory
		 * @param local_address	If this function returns TRUE, local_address will contain a valid address
		 *						(non URI format) that can be used to access local files or directories.
		 * @return				TRUE if the address is local
		 */
		BOOL					GetLocalAddress(const OpStringC& address, OpString& local_address);

		BOOL 					OpenIfSessionFile( const OpString &address );

		/**
		 * Open a url in a browser page
		 * @param url				The internet address that should be opened
		 * @param force_new_page	When set to TRUE, the url will be opened in a new page (tab), otherwise Opera
		 *							will determine what to do based on preferences and the state of shiftkeys
		 * @param force_background	When set to TRUE, the url will be opened in the background, otherwise Opera
		 *							will determine what to do based on the state of shiftkeys.
		 * @param url_came_from_address_field Set this flag if the URL was entered by the user in the addressfield,
		 *							is used to know if the URL should be opened in a special way, or if it needs to
		 *							be added to history.
		 * @param target_window		If not NULL, the url will be opened in this window. Must be BrowserDesktopWindow
		 *							or DocumentDesktopWindow
		 * @param context_id		ID used by core for refering to internal URLS
		 * @param ignore_modifier_keys	If TRUE, the shift/ctrl keys are not checked to determine if the user
		 *							tries to force opening of a new window or in the background.
		 * @return					TRUE if the url was successfully opened
		 */
		void					GoToPage(const OpStringC& url, BOOL force_new_page = FALSE, BOOL force_background = FALSE, BOOL url_came_from_address_field = FALSE, DesktopWindow* target_window = 0, URL_CONTEXT_ID context_id = -1, BOOL ignore_modifier_keys = FALSE, BOOL privacy_mode = FALSE);

		/**
		 * Modify settings for opening a URL depending on action and desktop window
		 *
		 * @param setting Settings that can be changed
		 * @param action Action that triggered opening the URL
		 * @param ddw Document window where the action occurred. Can be 0
		 */
		void AdjustForAction(OpenURLSetting& setting, OpInputAction* action, DocumentDesktopWindow* ddw);

		/**
		 * Open one or more urls in a browser page
		 * Checks if this is a speed dial shortcut or a nick, and calls OpenResolvedURL to open the url
		 *
		 * @param setting		Opening parameters to be used
		 * @return				TRUE if the url was successfully opened
		 */
		BOOL 					OpenURL( OpenURLSetting& setting );
	    /**
		 * Open a url in a browser page
		 * @param setting		Opening parameters to be used
		 * @return				TRUE if the url was successfully opened
		 */

    	BOOL                    OpenResolvedURL( OpenURLSetting& setting );

		/**
		 * Open a url in a browser page
		 * @param address		The internet address that should be opened
		 * @param new_window, new_page,in_background, context_id and ignore_modifier_keys:
		 *						correspond with parameters in the class Application::OpenURLSetting
		 * @return				TRUE if the url was successfully opened
		 */
		BOOL 					OpenURL( const OpStringC &address, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, URL_CONTEXT_ID context_id = 0, BOOL ignore_modifier_keys = FALSE, BOOL is_remote_command = FALSE, BOOL is_privacy_mode = FALSE);

		virtual OpUiWindowListener* CreateUiWindowListener();
		virtual DesktopOpAuthenticationListener* CreateAuthenticationListener();

		virtual DesktopMenuHandler* GetMenuHandler();

#ifdef M2_SUPPORT
		MailDesktopWindow*		GoToMailView(INT32 index_id, const uni_char* address = NULL, const uni_char* title = NULL, BOOL force_show = TRUE, BOOL force_new_window = FALSE, BOOL force_download = TRUE, BOOL ignore_modifier_keys = FALSE);
		MailDesktopWindow*		GoToMailView(OpINT32Vector& id_list, BOOL force_show = TRUE, BOOL include_folder_content = FALSE, BOOL ignore_modifier_keys = FALSE);

#ifdef IRC_SUPPORT
		void					GoToChat(UINT32 account_id, const ChatInfo& room, BOOL is_room, BOOL is_joining = FALSE, BOOL open_in_background = FALSE);

		BOOL					LeaveChatRoom(INT32 account_id, const OpStringC& name);
		AccountTypes::ChatStatus GetChatStatusFromString(const uni_char* chat_status);
		AccountTypes::ChatStatus GetChatStatus(INT32 account_id);
		AccountTypes::ChatStatus GetChatStatus(INT32 account_id, BOOL& is_connecting);
		BOOL					SetChatStatus(INT32 account_id, AccountTypes::ChatStatus chat_status, BOOL check_if_possible_only = FALSE);
#endif // IRC_SUPPORT
		BOOL					SubscribeAccount(AccountTypes::AccountType type, const char* type_name = NULL, INT32 account_id = 0, DesktopWindow* parent_window = NULL);

		BOOL					CreateAccount(AccountTypes::AccountType type = AccountTypes::UNDEFINED, const char* type_name = NULL, DesktopWindow* parent_window = NULL);
		BOOL					ShowAccountNeededDialog(AccountTypes::AccountType type){return ShowAccountNeededDialog(type,NULL);}
		BOOL					ShowAccountNeededDialog(AccountTypes::AccountType type, DesktopWindow* parent_window);
		BOOL					DeleteAccount(INT32 account_id, DesktopWindow* parent_window = NULL);
		BOOL					EditAccount(INT32 account_id, DesktopWindow* parent_window = NULL, BOOL modality = FALSE);
#ifdef JABBER_SUPPORT
		ChatDesktopWindow*      ComposeMessage(OpINT32Vector& id_list);
		BOOL                    SubscribeToPresence(OpINT32Vector& id_list, BOOL subscribe);
		BOOL                    AllowPresenceSubscription(OpINT32Vector& id_list, BOOL allow);
#endif // JABBER_SUPPORT

#endif // M2_SUPPORT

		BOOL 					CanViewSourceOfURL(URL * url);
		void 					OpenSourceViewer(URL *url, UINT32 window_id = 0, const uni_char* title = NULL, BOOL frame_source = FALSE);

		/*
		 * Executes an application.
		 *
		 * @param program The application to start
		 * @param parameters Parameters to the program
		 * @param run_in_terminal Start application in terminal if true
		 * @param protocol The protocol that triggerd 'program' to be selected
		 *        This typically only happens with parsing url activation
		 */
		void					ExecuteProgram(const uni_char* program, const uni_char* parameters, BOOL run_in_terminal = FALSE, const uni_char* protocol = 0);

		OP_STATUS				AddSettingsListener(SettingsListener* listener) {return m_settings_listeners.Add(listener);}
		void					RemoveSettingsListener(SettingsListener* listener) {m_settings_listeners.Remove(listener);}

	    void                    SetConfirmOnlineShow();
	    BOOL                    GetShowGoOnlineDialog();
	    void                    AddOfflineQueryCallback(OpDocumentListener::DialogCallback* callback);
	    void                    UpdateOfflineMode(OpDocumentListener::DialogCallback::Reply reply);
		void 					AskEnterOnlineMode(BOOL test_offline_mode_first, OnlineModeCallback* callback, Str::LocaleString dialog_message = Str::S_OFFLINE_TO_ONLINE, Str::LocaleString dialog_title = Str::D_OFFLINE_MODE_TITLE);
		BOOL					AskEnterOnlineMode(BOOL test_offline_mode_first);
		// FIXME: This is currently duplicated in ClassicGlobalUiContext.
		OP_BOOLEAN				UpdateOfflineMode(BOOL toggle);

		OP_STATUS				AddDesktopWindow(DesktopWindow* desktop_window, DesktopWindow* parent_window);
		OP_STATUS				RemoveDesktopWindow(DesktopWindow* desktop_window);

		OpToolTipListener*		GetToolTipListener() {return m_tooltip ? m_tooltip->GetListener() : NULL;}
		void					SetToolTipListener(OpToolTipListener* listener, uni_char key = 0) {if(m_tooltip) m_tooltip->SetListener(listener, key);}

		// called when we need to sync open tooltip information with the current url shown in the address bar
		void					UpdateToolTipInfo() { if(m_tooltip) m_tooltip->UrlChanged(); }

		virtual DesktopWindowCollection& GetDesktopWindowCollection() { return m_window_collection; }

		OP_STATUS				AddBrowserDesktopWindowListener(BrowserDesktopWindowListener* listener);
		void					RemoveBrowserDesktopWindowListener(BrowserDesktopWindowListener* listener) { m_browserwindow_listeners.Remove(listener);}

		BrowserDesktopWindow*	GetBrowserDesktopWindow(BOOL force_new_window = FALSE, BOOL force_background = FALSE, BOOL create_new_page = FALSE, DocumentDesktopWindow** new_page = NULL, OpWindowCommander* window_commander = NULL, INT32 width = 0, INT32 height = 0, BOOL show_toolbars = TRUE, BOOL focus_document = FALSE, BOOL ignore_modifier_keys = FALSE, BOOL no_speeddial = FALSE, OpenURLSetting *setting = NULL, BOOL private_browsing = FALSE);

		void					BroadcastOnBrowserWindowCreated(BrowserDesktopWindow *window);
		void					BroadcastOnBrowserWindowActivated(BrowserDesktopWindow *window);
		void					BroadcastOnBrowserWindowDeactivated(BrowserDesktopWindow *window);
		void					BroadcastOnBrowserWindowDeleting(BrowserDesktopWindow *window);
		/**
		 * Create and activate the specified panel/page (if already created - only activate
		 *
		 * @param type of panel
		 * @param create_new_if_needed if TRUE and the panel/page is not already open, it will create it
		 * @param force_new
		 * @param in_background
		 * @param action_string_parameter the parameter the action was invoked with
		 */
		PanelDesktopWindow*		GetPanelDesktopWindow(Type type,
													  BOOL create_new_if_needed = FALSE,
													  BOOL force_new = FALSE,
													  BOOL in_background = FALSE,
													  const uni_char* action_string_parameter = NULL);

		DocumentDesktopWindow*	CreateDocumentDesktopWindow(OpWorkspace* = NULL, OpWindowCommander* window_commander = NULL, INT32 width = 0, INT32 height = 0, BOOL show_toolbars = TRUE, BOOL focus_document = FALSE, BOOL force_background = FALSE, BOOL no_speeddial = FALSE, OpenURLSetting *setting = NULL, BOOL private_browsing = FALSE);

		DesktopWindow*			GetActiveDesktopWindow(BOOL toplevel_only = TRUE);
		/**
		  * @return Currently active window or the one that was active previously, before we lost focus
		  */
		BrowserDesktopWindow*	GetActiveBrowserDesktopWindow();
		DocumentDesktopWindow*	GetActiveDocumentDesktopWindow();
		MailDesktopWindow*		GetActiveMailDesktopWindow();

		virtual DesktopWindow*	GetDesktopWindowFromAction(OpInputAction* action);

		Image*					GetImageFromURL(const uni_char* url);

		// default language manager is used to get the English strings in a translated version if needed
		OpLanguageManager* GetDefaultLanguageManager()
		{ return m_boot_manager.GetDefaultLanguageManager(); }

		// This is ONLY for backward compatibilty and should be removed ASAP!
		KioskManager* 			GetKioskManager() { return KioskManager::GetInstance(); }
		BOOL					KioskFilter(OpInputAction* action);
		void 					RegisterActivity();

		/* this call is dying.. temp call just now since some old ugly window hooks wants a simple call to get active core window */

		DEPRECATED(Window* GetActiveWindow());

		BOOL					IsSDI() {return g_pcui->GetIntegerPref(PrefsCollectionUI::SDI) == 1;}

		/* Determines whether the user wants to open a url in a new window or not.
		 * Uses SDI/tab/window settings and the status of modifier keys.
		 * @param ignore_modifier_keys	If TRUE, the shift/ctrl keys are not checked to determine if the user
		 *								tries to force opening of a new window.
		 * @param from_address_field	Set to TRUE if the a url is opened from the addressfield. urls from addressfields
		 *								reuse the existing windows more often.
		 * @return						TRUE if a url should be opened in a new window
		 */
		BOOL					IsOpeningInNewWindowPreferred(BOOL ignore_modifier_keys = FALSE, BOOL from_address_field = FALSE);

		/* Determines whether the user wants to open a url in a new page (ie a tab) or not.
		 * Uses SDI/tab/window settings and the status of modifier keys.
		 * @param ignore_modifier_keys	If TRUE, the shift/ctrl keys are not checked to determine if the user
		 *								tries to force opening of a new page.
		 * @param from_address_field	Set to TRUE if the a url is opened from the addressfield. urls from addressfields
		 *								reuse the existing pages more often.
		 * @return						TRUE if a url should be opened in a new tab.
		 */
		BOOL					IsOpeningInNewPagePreferred(BOOL ignore_modifier_keys = FALSE, BOOL from_address_field = FALSE);

		/* Determines whether the users wants to open a url in the background or not, by checking the status
		 * of the shift and ctrl keys.
		 * @param ignore_modifier_keys	If TRUE, the shift/ctrl keys are not checked and the function
		 *								returns FALSE.
		 * @return						TRUE if a url should be opened in the background
		 */
		BOOL					IsOpeningInBackgroundPreferred(BOOL ignore_modifier_keys = FALSE);

		/** Check whether we have M2 support and we want to show it to the user
		  * @return Whether we can and want to show M2 to the user
		  */
		BOOL					ShowM2();

		BOOL					HasMail();
		BOOL					HasChat();
		BOOL					HasFeeds();

		BOOL					IsShowingOpera() const {return m_is_showing_opera;}
		virtual OP_STATUS Start();
		virtual void Exit(BOOL force);
		BOOL 					IsExiting() const { return m_boot_manager.IsExiting(); }
		BOOL					IsBrowserStarted() const { return m_boot_manager.IsBrowserStarted(); }
		BOOL					HasCrashed() const { return m_boot_manager.HasCrashed(); }
		BOOL					IsEmBrowser() const {
#ifdef EMBROWSER_SUPPORT
			return gVendorDataID != 'OPRA';
#else
			return FALSE;
#endif // EMBROWSER_SUPPORT
		}

		BOOL					CustomizeToolbars(OpToolbar* target_toolbar = NULL, INT32 initial_page = 0);
		BOOL					IsCustomizingToolbars() {return m_customize_toolbars_dialog != NULL;}
		CustomizeToolbarDialog*	GetCustomizeToolbarsDialog();
		BOOL					IsDragCustomizingAllowed();
		BOOL					IsCustomizingHiddenToolbars();

		BOOL					OpenExternalURLProtocol(URL& url);

		/**
		 * Check whether this is the first run of a clean install,
		 * the first run of an upgrade to a previously installed
		 * instance, or a normal run.
		 *
		 * This function will return the same value every time it's
		 * called in a run, even if the preference that it uses is changed.
		 *
		 * @return FIRST, FIRSTCLEAN or NORMAL based on run type.
		 */
		RunType DetermineFirstRunType() { return m_boot_manager.DetermineFirstRunType(); }

		/**
		 * Holds the MaxVersionSeen preference before it is overwritten on startup
		 * Can be used on first runs to add special upgrade behaviour
		 *
		 * @return version eg. 950, 1001 
		 */
		OperaVersion			GetPreviousVersion() { return m_boot_manager.GetPreviousVersion(); }

		void					GetStartupPage(Window* window, STARTUPTYPE type);

		virtual void			OnSettingsChanged(DesktopSettings* settings);

		void StopListeningToWand();

		void PrepareKioskMode();

		// Implementing OpTypedObject interface

		virtual Type			GetType() {return APPLICATION_TYPE;}
		virtual int				GetID() {return 0;}

		// Implementing the DesktopWindowListener interface

		virtual void			OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active);
		virtual BOOL			OnDesktopWindowIsCloseAllowed(DesktopWindow* desktop_window);
		virtual void			OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);
		virtual void			OnDesktopWindowParentChanged(DesktopWindow* desktop_window);
		virtual void			OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# ifdef FEATURE_UI_TEST
		// Implementing the OpUiController interface
		virtual OpAccessibilityExtension* GetRootNode() { return this; }
# endif // FEATURE_UI_TEST
		//This one should really never send events as the screen readers should never see it
		virtual void			AccessibilitySendEvent(Accessibility::Event evt) const {return;}
		virtual BOOL			AccessibilityIsReady() const {return TRUE;}
		virtual OP_STATUS		GetTypeString(OpString& type_string);
		virtual OP_STATUS AccessibilityGetAbsolutePosition(OpRect &rect);
		virtual OP_STATUS AccessibilityGetText(OpString& str);
		virtual Accessibility::State AccessibilityGetState();
		virtual Accessibility::ElementKind AccessibilityGetRole() const;
		virtual int GetAccessibleChildrenCount();
		virtual OpAccessibleItem* GetAccessibleParent() {return NULL;}
		virtual OpAccessibleItem* GetAccessibleChild(int n);
		virtual int GetAccessibleChildIndex(OpAccessibleItem* child);
		virtual OpAccessibleItem* GetAccessibleChildOrSelfAt(int x, int y);
		virtual OpAccessibleItem* GetNextAccessibleSibling() {return NULL;}
		virtual OpAccessibleItem* GetPreviousAccessibleSibling() {return NULL;}
		virtual OpAccessibleItem* GetLeftAccessibleObject();
		virtual OpAccessibleItem* GetRightAccessibleObject();
		virtual OpAccessibleItem* GetDownAccessibleObject();
		virtual OpAccessibleItem* GetUpAccessibleObject();
		virtual OpAccessibleItem* GetAccessibleFocusedChildOrSelf();
#endif

		// MessageObject::HandleCallback

		virtual void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

		// Event that happens when the opera application is (in)activated by the OS
		// For instance, if the user selects a non opera window
		virtual void			OnActivate(BOOL activate);
		virtual BOOL			IsActivated() { return m_activated; }

#ifdef WAND_SUPPORT
		// Implementing the WandListener interface

		virtual OP_STATUS		OnSubmit(WandInfo* info);
		virtual OP_STATUS		OnSelectMatch(WandMatchInfo* info);
#endif // WAND_SUPPORT

		// Implementing the OpPrefsListener interface

		virtual void			PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue);
		virtual void			PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);
		virtual void			HostOverrideChanged(OpPrefsCollection::Collections id,	const uni_char *hostname);

		OP_STATUS 				GetClientID(OpString& client_id) { return client_id.Set(m_client_id); }

		virtual AnchorSpecialManager & GetAnchorSpecialManager() { return m_anchor_special_manager; }
		BOOL					AnchorSpecial(	OpWindowCommander * commander,
												const OpDocumentListener::AnchorSpecialInfo & info);

	OP_STATUS				AddAddressBar(OpAddressDropDown* address_bar);
	OP_STATUS				RemoveAddressBar(OpAddressDropDown* address_bar);

	OpTreeModel*			GetWindowList() { return &m_window_collection; }

	UiContext*				GetInputContext();

	/**
	 * We have some trouble passing around an OpDocumentContext, especially in the
	 * action system (has to be fixed somehow) so we keep such an object here for when
	 * it is needed and all code can look it up. Will hopefully be NULL most of the
	 * time since they're not designed to be used for a long time (pages are
	 * dynamic and this stops being useful after a while)
	 */
	OpDocumentContext*		GetLastSeenDocumentContext()
	{
		OP_ASSERT(m_last_seen_document_context); // Just to see if I've missed assigning it somewhere. It's legal for it to be NULL in the general case.
		return m_last_seen_document_context;
	}
	void					SetLastSeenDocumentContext(OpDocumentContext * context)
	{
		m_last_seen_document_context = context;
	}

	BOOL HasActiveTransfer();

	BOOL LockActiveWindow(){return m_lock_active_window;}
	void SetLockActiveWindow(BOOL val){m_lock_active_window = val;}

	virtual DesktopOpAuthenticationListener* GetAuthListener() { return m_boot_manager.GetAuthListener(); }
	virtual OpApplicationCacheListener* GetApplicationCacheListener() { return m_application_cache_listener; }
	virtual UINT32 GetClipboardToken();

	virtual void SetPopupDesktopWindow(DesktopWindow* win);
	virtual void ResetPopupDesktopWindow() {m_popup_desktop_window = NULL;}
	virtual DesktopWindow* GetPopupDesktopWindow() {return m_popup_desktop_window;}

#ifdef GADGET_UPDATE_SUPPORT
	GadgetUpdateController* GetGadgetUpdateController(){return &m_update_controller;}
#endif //GADGET_UPDATE_SUPPORT

	private:

		OP_STATUS CreateSessionWindowsL(OpSession* session, BOOL insert);

		OpString m_client_id;

		/* Choose recovery strategy based on settings
		 * @param force_no_windows		If TRUE, this function will return Restore_NoWindows;
		 * @return						WindowRecoveryStrategy that matches the current preferences
		 */
		WindowRecoveryStrategy	GetRecoverStrategy( const BOOL force_no_windows )
		{ return m_boot_manager.GetRecoverStrategy(force_no_windows); }

		/* 
		 * Handles the confirm-on-exit dialogs and minimizing Opera if necessary.
		 * @return TRUE if exit was handled already, FALSE if not.
		 */
		BOOL								HandleExitStrategies();

		OpListeners<SettingsListener>		m_settings_listeners;
		OpVector<DesktopWindow>				m_toplevel_desktop_windows;
		DesktopWindowCollection				m_window_collection;
		DesktopWindow*						m_active_toplevel_desktop_window;
		DesktopWindow*						m_popup_desktop_window;
		OpToolTip*							m_tooltip;
		BOOL								m_is_showing_opera;
		BOOL								m_activated;
		BOOL								m_lock_active_window;
		BOOL								m_hack_open_next_tab_as_window;			// forward port hack until core's WindowManager::GetAWindow supports opening top-level windows and not just tabs
		BOOL								m_hack_open_next_tab_as_private;		// see above

		DesktopSettings*					m_settings_changed;
		BOOL								m_settings_changed_message_pending;

		CustomizeToolbarDialog*				m_customize_toolbars_dialog;
		OpString							m_mainwindowcaption;
		int									m_captionlength;

		OpVector<OpAddressDropDown>			m_address_bars;
		OpVector<OpDocumentListener::DialogCallback>	m_offline_mode_callbacks;
		BOOL								m_confirm_online_dialog;
		OpDocumentContext*					m_last_seen_document_context;
		void OnCancelled() {  }

	private:

		OpBootManager m_boot_manager;
		AnchorSpecialManager m_anchor_special_manager;
		ClassicGlobalUiContext* m_ui_context;
		ExtensionContext m_extension_context;
		SpellCheckContext m_spell_check_context;

		BrowserMenuHandler* m_menu_handler;
		BOOL m_finalized;
		BrowserApplicationCacheListener* m_application_cache_listener;

		OpListeners<BrowserDesktopWindowListener>	m_browserwindow_listeners;
#ifdef GADGET_UPDATE_SUPPORT
		GadgetUpdateController	m_update_controller;
		ExtensionUpdater*		m_extension_updater;
#endif //GADGET_UPDATE_SUPPORT
		PageLoadDispatcher*		m_dispatcher;
};

#endif // CLASSIC_APPLICATION_H
