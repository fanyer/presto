/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author everyone / spoon / Arjan van Leeuwen (arjanl)
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include "adjunct/desktop_util/boot/DesktopBootstrap.h"
#include "adjunct/desktop_util/prefs/DesktopPrefsTypes.h"
#include "adjunct/desktop_util/settings/SettingsType.h"
#include "adjunct/desktop_util/version/operaversion.h"

#include "adjunct/m2/src/include/enums.h"
#include "adjunct/quick_toolkit/contexts/UiContext.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/windowcommander/WritingSystem.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/locale/locale-enum.h"

class AnchorSpecialManager;
class BrowserDesktopWindow;
class ComposeDesktopWindow;
class CustomizeToolbarDialog;
class DesktopMenuHandler;
class DesktopWindow;
class DesktopWindowCollection;
class DocumentDesktopWindow;
class MailDesktopWindow;
class OpAddressDropDown;
class OpBrowserView;
class OpenURLSetting;
class OpInputAction;
class OpLanguageManager;
class OpToolTipListener;
class OpTreeModel;
class OpWidget;
class OpWorkspace;
class PanelDesktopWindow;
class PrefsEntry;
class SettingsListener;
class OpUiWindowListener;
class OpAuthenticationListener;
class DesktopMenuContext;
class MailTo;
class DesktopOpAuthenticationListener;
class OpSession;
class OpToolbar;

#ifdef WIC_USE_ANCHOR_SPECIAL
/***********************************************************************************
**
**	AnchorSpecialHandler
**
***********************************************************************************/
class AnchorSpecialHandler
{
public:
	/**
	 **  Returns if this handler is interested in this kind of special anchor
	 **/
	virtual BOOL HandlesAnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info) = 0;
	/**
	 ** Performs the action (as indicated in AnchorSpecialInfo). Returns TRUE if the action was successful,
	 ** FALSE if not (= invalid URL).
	 **/
	virtual BOOL AnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info) = 0;
};

/***********************************************************************************
**
**	AnchorSpecialManager
**
***********************************************************************************/
class AnchorSpecialManager
{
public:
	// === helper functions ===
	OP_STATUS	AddHandler(AnchorSpecialHandler * handler) { return m_handlers.Add(handler); }
	OP_STATUS	RemoveHandler(AnchorSpecialHandler * handler) { return m_handlers.RemoveByItem(handler); }

	// == see OpDocumentListener::OnAnchorSpecial
	BOOL	AnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info);


private:
	OpVector<AnchorSpecialHandler>	m_handlers;
};

#endif // WIC_USE_ANCHOR_SPECIAL


#define g_application (g_desktop_bootstrap->GetApplication())
#define g_global_ui_context (g_desktop_bootstrap->GetApplication()->GetInputContext())

/** @brief Main class for desktop Opera
  * This singleton class represents the main global object used for starting
  * and exiting Desktop Opera. Other objects that should be available during
  * the full lifetime of a desktop Opera session should be registered as
  * managers (see managers/DesktopManager.h).
  *
  * NB: Please do not add extra functionality to this class; use a local object
  * or, if it's really necessary to keep the object alive during the full
  * lifetime of the desktop session, use a manager.
  */
class Application
{
public:
	virtual ~Application() {}

	// ------ Utility functions used by core ------ //

	/** this call is dying.. temp call just now since some old ugly window hooks wants a simple call to get active core window */
	DEPRECATED(virtual Window* GetActiveWindow()) = 0;

	virtual DesktopWindow* GetActiveDesktopWindow(BOOL toplevel_only = TRUE) = 0;
	virtual BrowserDesktopWindow* GetActiveBrowserDesktopWindow() = 0;
	virtual DocumentDesktopWindow* GetActiveDocumentDesktopWindow() = 0;

	virtual BrowserDesktopWindow* GetBrowserDesktopWindow(BOOL force_new_window = FALSE, BOOL force_background = FALSE, BOOL create_new_page = FALSE, DocumentDesktopWindow** new_page = NULL, OpWindowCommander* window_commander = NULL, INT32 width = 0, INT32 height = 0, BOOL show_toolbars = TRUE, BOOL focus_document = FALSE, BOOL ignore_modifier_keys = FALSE, BOOL no_speeddial = FALSE, OpenURLSetting *setting = NULL, BOOL private_browsing = FALSE) = 0;

	/**
	 * Modify settings for opening a URL depending on action and desktop window
	 *
	 * @param setting Settings that can be changed
	 * @param action Action that triggered opening the URL
	 * @param ddw Document window where the action occurred. Can be 0
	 */
	virtual void AdjustForAction(OpenURLSetting& setting, OpInputAction* action, DocumentDesktopWindow* ddw) = 0;

	/**
	 * Open one or more urls in a browser page
	 * Checks if this is a speed dial shortcut or a nick, and calls OpenResolvedURL to open the url
	 *
	 * @param setting		Opening parameters to be used
	 * @return				TRUE if the url was successfully opened
	 */
	virtual BOOL OpenURL(OpenURLSetting& setting) = 0;

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
	virtual void GoToPage(const OpStringC& url, BOOL force_new_page = FALSE, BOOL force_background = FALSE, BOOL url_came_from_address_field = FALSE, DesktopWindow* target_window = 0, URL_CONTEXT_ID context_id = -1, BOOL ignore_modifier_keys = FALSE, BOOL privacy_mode = FALSE) = 0;

	virtual BOOL OpenResolvedURL( OpenURLSetting& setting ) = 0;

	virtual void OpenSourceViewer(URL *url, UINT32 window_id = 0, const uni_char* title = NULL, BOOL frame_source = FALSE) = 0;

	virtual BOOL KioskFilter(OpInputAction* action) = 0;

	virtual void PrepareKioskMode() = 0;

	virtual void RegisterActivity() = 0;

	/**
	 * Sets the listener that will provide content for the tooltip. Setting the 
	 * listener to 0 will hide any visible tooltip
	 *
	 * @param listener The listener to use
	 * @param key The OP_KEY_NN value that was used to trigger the function. Use 0
	 *        if this did not happened as a direct result of a keyboard event
	 */
	virtual void SetToolTipListener(OpToolTipListener* listener, uni_char key = 0) = 0;

	/** TODO Remove this static function */
	static BOOL HandleExternalURLProtocol(URL& url) { return g_application->OpenExternalURLProtocol(url); }
	virtual BOOL OpenExternalURLProtocol(URL& url) = 0;

	/**
	 * Executes an application.
	 *
	 * @param program The application to start
	 * @param parameters Parameters to the program
	 * @param run_in_terminal Start application in terminal if true
	 * @param protocol The protocol that triggerd 'program' to be selected
	 *        This typically only happens with parsing url activation
	 */
	virtual void ExecuteProgram(const uni_char* program, const uni_char* parameters, BOOL run_in_terminal = FALSE, const uni_char* protocol = 0) = 0;

	virtual void SettingsChanged(SettingsType setting) = 0;

	/**
	 * @return @c TRUE iff the application has started and it is a browser
	 */
	virtual BOOL IsBrowserStarted() const = 0;

	virtual BOOL HasCrashed() const = 0;

	virtual BOOL IsDragCustomizingAllowed() = 0;

	// ------ Below this line only used by Quick / adjunct ------ //

	// Remove this once Common resources are in use USE_COMMON_RESOURCES
	enum RunType
	{
		RUNTYPE_NORMAL = 0,
		RUNTYPE_FIRST,
		RUNTYPE_FIRSTCLEAN,
		RUNTYPE_FIRST_NEW_BUILD_NUMBER,
		RUNTYPE_NOT_SET
	};

	struct MailComponents
	{
		OpString to;
		OpString cc;
		OpString bcc;
		OpString subject;
		OpString message;
		OpString raw_mailto_address;
	};

	/** @return an OpUiWindowListener object that should be used in this application
	  * Caller is responsible for deletion
	  */
	virtual OpUiWindowListener* CreateUiWindowListener() = 0;

	/** @return An OpAuthenticationListener object that should be used in this application
	  * Caller is responsible for deletion
	  */
	virtual DesktopOpAuthenticationListener* CreateAuthenticationListener() = 0;

	/**
	 * @return A DesktopMenuHandler that should be used in this application.
	 */
	virtual DesktopMenuHandler* GetMenuHandler() = 0;

	virtual OP_STATUS AddSettingsListener(SettingsListener* listener) = 0;
	virtual void RemoveSettingsListener(SettingsListener* listener) = 0;

	virtual void SyncSettings() = 0;

	virtual DesktopWindowCollection& GetDesktopWindowCollection() = 0;

	virtual MailDesktopWindow* GoToMailView(INT32 index_id, const uni_char* address = NULL, const uni_char* title = NULL, BOOL force_show = TRUE, BOOL force_new_window = FALSE, BOOL force_download = TRUE, BOOL ignore_modifier_keys = FALSE) = 0;
#ifdef IRC_SUPPORT
	virtual void GoToChat(UINT32 account_id, const class ChatInfo& room, BOOL is_room, BOOL is_joining = FALSE, BOOL open_in_background = FALSE) = 0;

	virtual BOOL LeaveChatRoom(INT32 account_id, const OpStringC& name) = 0;
	virtual AccountTypes::ChatStatus GetChatStatusFromString(const uni_char* chat_status) = 0;
	virtual AccountTypes::ChatStatus GetChatStatus(INT32 account_id) = 0;
	virtual AccountTypes::ChatStatus GetChatStatus(INT32 account_id, BOOL& is_connecting) = 0;
	virtual BOOL SetChatStatus(INT32 account_id, AccountTypes::ChatStatus chat_status, BOOL check_if_possible_only = FALSE) = 0;
#endif // IRC_SUPPORT
	virtual MailDesktopWindow* GetActiveMailDesktopWindow() = 0;

	virtual BOOL SubscribeAccount(AccountTypes::AccountType type, const char* type_name = NULL, INT32 account_id = 0, DesktopWindow* parent_window = NULL) = 0;
	virtual BOOL CreateAccount(AccountTypes::AccountType type = AccountTypes::UNDEFINED, const char* type_name = NULL, DesktopWindow* parent_window = NULL) = 0;
	virtual BOOL ShowAccountNeededDialog(AccountTypes::AccountType type) = 0;
	virtual BOOL DeleteAccount(INT32 account_id, DesktopWindow* parent_window = NULL) = 0;
	virtual BOOL EditAccount(INT32 account_id, DesktopWindow* parent_window = NULL, BOOL modality = FALSE) = 0;

	virtual BOOL IsSDI() = 0;

	 /** @return Whether we can and want to show M2 to the user
	  */
	virtual BOOL ShowM2() = 0;

	virtual BOOL HasMail() = 0;
	virtual BOOL HasChat() = 0;

	/**
	 * Callback object passed to AskEnterOnlineMode function.
	 */
	class OnlineModeCallback
	{
	public:
		virtual ~OnlineModeCallback() {}

		/**
		 * Called if Opera is in online mode.
		 */
		virtual void OnOnlineMode() = 0;
	};

	/**
	 * Ask user if Opera can enter online mode.
	 *
	 * This function uses modal non-blocking dialog to ask user about
	 * switching to online mode. Result is returned via callback object,
	 * which may be called immediately (if Opera already is in online
	 * mode) or asynchronously.
	 *
	 * Class that implements this function should take ownership
	 * of callback object when function is called and delete it
	 * when it is no longer needed.
	 *
	 * @param test_offline_mode_first whether mode should be checked before displaying the dialog
	 * @param callback object notified about the result; ownership is transferred to the Application object
	 * @param dialog_message message displayed in the dialog
	 * @parma dialog_title title of the dialog
	 */
	virtual void AskEnterOnlineMode(BOOL test_offline_mode_first, OnlineModeCallback* callback,
		Str::LocaleString dialog_message = Str::S_OFFLINE_TO_ONLINE,
		Str::LocaleString dialog_title = Str::D_OFFLINE_MODE_TITLE) = 0;

	/**
	 * Ask user if Opera can enter online mode.
	 *
	 * This funcion displays blocking dialog, result is always returned synchronously.
	 *
	 * @param test_offline_mode_first whether mode should be checked before displaying the dialog
	 *
	 * @retval TRUE if Opera is in online mode
	 * @retval FALSE if user didn't allow Opera to enter online mode or if function failed to display the dialog
	 */
	virtual BOOL AskEnterOnlineMode(BOOL test_offline_mode_first) = 0;

	virtual BOOL CanViewSourceOfURL(URL * url) = 0;

	/**
	 * Open a url in a browser page
	 * @param address The internet address that should be opened
	 * @param new_window, new_page,in_background, context_id and ignore_modifier_keys:
	 *						correspond with parameters in the class OpenURLSetting
	 * @return TRUE if the url was successfully opened
	 */
	virtual BOOL OpenURL( const OpStringC &address, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, URL_CONTEXT_ID context_id = 0, BOOL ignore_modifier_keys = FALSE, BOOL is_remote_command = FALSE, BOOL is_privacy_mode = FALSE) = 0;

	virtual BOOL IsEmBrowser() const = 0;

	/**
	 * Create and activate the specified panel/page (if already created - only activate
	 *
	 * @param type of panel
	 * @param create_new_if_needed if TRUE and the panel/page is not already open, it will create it
	 * @param force_new
	 * @param in_background
	 * @param action_string_parameter the parameter the action was invoked with
	 */
	virtual PanelDesktopWindow* GetPanelDesktopWindow(OpTypedObject::Type type,
													BOOL create_new_if_needed = FALSE,
													BOOL force_new = FALSE,
													BOOL in_background = FALSE,
													const uni_char* action_string_parameter = NULL) = 0;

	virtual DocumentDesktopWindow* CreateDocumentDesktopWindow(OpWorkspace* = NULL, OpWindowCommander* window_commander = NULL, INT32 width = 0, INT32 height = 0, BOOL show_toolbars = TRUE, BOOL focus_document = FALSE, BOOL force_background = FALSE, BOOL no_speeddial = FALSE, OpenURLSetting *setting = NULL, BOOL private_browsing = FALSE) = 0;

	virtual BOOL CustomizeToolbars(OpToolbar* target_toolbar = NULL, INT32 initial_page = 0) = 0;
	virtual BOOL IsCustomizingToolbars() = 0;
	virtual BOOL IsCustomizingHiddenToolbars() = 0;
	virtual CustomizeToolbarDialog* GetCustomizeToolbarsDialog() = 0;

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
	virtual RunType DetermineFirstRunType() = 0;

	/**
	 * Indicates to the application that it's environment is now completely
	 * set up and it can start operation.
	 */
	virtual OP_STATUS Start() = 0;

	/**
	 * Tells the application to terminate.
	 *
	 * @param force @c TRUE indicates the application must terminate
	 * 		unconditionally.  Otherwise, the implementation may choose not to
	 * 		terminate at this point.
	 */
	virtual void Exit(BOOL force) = 0;

	virtual BOOL IsExiting() const = 0;

	/** Determines whether the user wants to open a url in a new window or not.
	 * Uses SDI/tab/window settings and the status of modifier keys.
	 * @param ignore_modifier_keys	If TRUE, the shift/ctrl keys are not checked to determine if the user
	 *								tries to force opening of a new window.
	 * @param from_address_field	Set to TRUE if the a url is opened from the addressfield. urls from addressfields
	 *								reuse the existing windows more often.
	 * @return						TRUE if a url should be opened in a new window
	 */
	virtual BOOL IsOpeningInNewWindowPreferred(BOOL ignore_modifier_keys = FALSE, BOOL from_address_field = FALSE) = 0;

	/** Determines whether the user wants to open a url in a new page (ie a tab) or not.
	 * Uses SDI/tab/window settings and the status of modifier keys.
	 * @param ignore_modifier_keys	If TRUE, the shift/ctrl keys are not checked to determine if the user
	 *								tries to force opening of a new page.
	 * @param from_address_field	Set to TRUE if the a url is opened from the addressfield. urls from addressfields
	 *								reuse the existing pages more often.
	 * @return						TRUE if a url should be opened in a new tab.
	 */
	virtual BOOL IsOpeningInNewPagePreferred(BOOL ignore_modifier_keys = FALSE, BOOL from_address_field = FALSE) = 0;

	/** Determines whether the users wants to open a url in the background or not, by checking the status
	 * of the shift and ctrl keys.
	 * @param ignore_modifier_keys	If TRUE, the shift/ctrl keys are not checked and the function
	 *								returns FALSE.
	 * @return						TRUE if a url should be opened in the background
	 */
	virtual BOOL IsOpeningInBackgroundPreferred(BOOL ignore_modifier_keys = FALSE) = 0;

	/** @return Tree model that represents all open windows
	  */
	virtual OpTreeModel* GetWindowList() = 0;

	/**
	 * Lets Application know a new DesktopWindow has been created.
	 *
	 * @param desktop_window the new DesktopWindow
	 * @param parent_window the parent of @a desktop_window.  Should be @c NULL
	 * 		if there's no parent.
	 * @return status
	 */
	virtual OP_STATUS AddDesktopWindow(DesktopWindow* desktop_window, DesktopWindow* parent_window) = 0;

	virtual OP_STATUS RemoveDesktopWindow(DesktopWindow* desktop_window) = 0;

	virtual OpToolTipListener* GetToolTipListener() = 0;

	virtual OP_STATUS AddAddressBar(OpAddressDropDown* address_bar) = 0;
	virtual OP_STATUS RemoveAddressBar(OpAddressDropDown* address_bar) = 0;

	virtual AnchorSpecialManager & GetAnchorSpecialManager() = 0;
	virtual BOOL AnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info) = 0;

	virtual void SetConfirmOnlineShow() = 0;
	virtual BOOL GetShowGoOnlineDialog() = 0;
	virtual void AddOfflineQueryCallback(OpDocumentListener::DialogCallback* callback) = 0;
	virtual void UpdateOfflineMode(OpDocumentListener::DialogCallback::Reply reply) = 0;

	virtual void UpdateToolTipInfo() = 0;

	/**
	 * Holds the MaxVersionSeen preference before it is overwritten on startup
	 * Can be used on first runs to add special upgrade behaviour
	 *
	 * @return version eg. 950, 1001
	 */
	virtual OperaVersion GetPreviousVersion() = 0;

	virtual BOOL ShowOpera(BOOL show, BOOL force_activate = FALSE) = 0;

	/** @return global input context */
	virtual UiContext* GetInputContext() = 0;

	// ------ Below this line only used by platforms ------ //

	// Command line support [espen 2003-01-29]
	class StartupSetting
	{
	public:
		StartupSetting()
		{
			open_in_background = FALSE;
			open_in_new_page   = FALSE;
			open_in_new_window = FALSE;
		}
		BOOL HasGeometry() const { return geometry.width > 0 && geometry.height > 0; }

	public:
		BOOL open_in_background;
		BOOL open_in_new_page;
		BOOL open_in_new_window;
		BOOL fullscreen;
		BOOL iconic;
		OpRect geometry;
	};
	class BrowserDesktopWindowListener
	{
	public:
		virtual ~BrowserDesktopWindowListener() {}

		virtual void	OnBrowserDesktopWindowCreated(BrowserDesktopWindow *window) = 0;
		virtual void	OnBrowserDesktopWindowActivated(BrowserDesktopWindow *window) = 0;
		virtual void	OnBrowserDesktopWindowDeactivated(BrowserDesktopWindow *window) = 0;
		virtual void	OnBrowserDesktopWindowDeleting(BrowserDesktopWindow *window) = 0;
	};
	virtual OP_STATUS	AddBrowserDesktopWindowListener(BrowserDesktopWindowListener* listener) = 0;
	virtual void		RemoveBrowserDesktopWindowListener(BrowserDesktopWindowListener* listener) = 0;
	virtual void		BroadcastOnBrowserWindowCreated(BrowserDesktopWindow *window) = 0;
	virtual void		BroadcastOnBrowserWindowActivated(BrowserDesktopWindow *window) = 0;
	virtual void		BroadcastOnBrowserWindowDeactivated(BrowserDesktopWindow *window) = 0;
	virtual void		BroadcastOnBrowserWindowDeleting(BrowserDesktopWindow *window) = 0;

	virtual BOOL IsShowingOpera() const = 0;

	virtual void SetStartupSetting(const StartupSetting &setting) = 0;

	virtual BOOL HasFeeds() = 0;

	virtual OpLanguageManager* GetDefaultLanguageManager() = 0;

    virtual DesktopWindow* GetDesktopWindowFromAction(OpInputAction* action) = 0;

	/**
	 * We have some trouble passing around an OpDocumentContext, especially in the
	 * action system (has to be fixed somehow) so we keep such an object here for when
	 * it is needed and all code can look it up. Will hopefully be NULL most of the
	 * time since they're not designed to be used for a long time (pages are
	 * dynamic and this stops being useful after a while)
	 */
	virtual OpDocumentContext* GetLastSeenDocumentContext() = 0;
	virtual void SetLastSeenDocumentContext(OpDocumentContext * context) = 0;

	// don't allow window to be activated
	virtual BOOL LockActiveWindow() = 0;
	virtual void SetLockActiveWindow(BOOL val) = 0;

	virtual DesktopOpAuthenticationListener* GetAuthListener() = 0;

	// Listener that handles requests for offline application cache
	virtual OpApplicationCacheListener* GetApplicationCacheListener() = 0;

	// return the token that should be used for current copy operation
	// in case the active window is private, return the private context id
	// otherwise return 0
	virtual UINT32 GetClipboardToken() = 0;

	// if you want a window to be closed automatically like STYLE_POPUP windows
	// but want to perform the close action yourself(for example you may want to
	// animate the window and then close), add it here. DesktopWindow::Close will
	// be called when needed, do whatever you want there.

	// Close the current window and set the new one
	virtual void SetPopupDesktopWindow(DesktopWindow* win) = 0;

	// Reset the current without closing it
	virtual void ResetPopupDesktopWindow() = 0;
	virtual DesktopWindow* GetPopupDesktopWindow() = 0;
};

#endif // APPLICATION_H
