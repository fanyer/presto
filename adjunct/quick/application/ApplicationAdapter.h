/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef APPLICATION_ADAPTER_H
#define APPLICATION_ADAPTER_H

#include "adjunct/quick/Application.h"
#include "adjunct/quick/application/DesktopOpAuthenticationListener.h"
#include "adjunct/quick/application/NonBrowserApplication.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/WindowCommanderProxy.h"

class OpSession;

/** @brief Transitional class to identify functions only needed by browser
  */
class ApplicationAdapter : public Application
{
public:
	/**
	 * @param adaptee the NonBrowserApplication instance being adapted to
	 * 		Application interface.  Ownership is transfered.
	 */
	explicit ApplicationAdapter(NonBrowserApplication* adaptee);
	~ApplicationAdapter();

	virtual OpBrowserView* GetBrowserViewFromWindow(Window* window) { return 0; }
	virtual DesktopWindow* GetDesktopWindowFromWindow(Window* window) { return 0; }

	virtual Window* GetActiveWindow() { return WindowCommanderProxy::GetActiveWindowCommander() ? WindowCommanderProxy::GetActiveWindowCommander()->GetWindow() : 0; }

	virtual DesktopWindow* GetActiveDesktopWindow(BOOL toplevel_only) { return m_application->GetActiveDesktopWindow(toplevel_only); }
	virtual BrowserDesktopWindow* GetActiveBrowserDesktopWindow() { return 0; }
	virtual DocumentDesktopWindow* GetActiveDocumentDesktopWindow() { return 0; }

	virtual BrowserDesktopWindow* GetBrowserDesktopWindow(BOOL force_new_window, BOOL force_background, BOOL create_new_page, DocumentDesktopWindow** new_page, OpWindowCommander* window_commander, INT32 width, INT32 height, BOOL show_toolbars, BOOL focus_document, BOOL ignore_modifier_keys, BOOL no_speeddial, OpenURLSetting *setting, BOOL private_browsing) { return 0; }
	
	virtual void AdjustForAction(OpenURLSetting& setting, OpInputAction* action, DocumentDesktopWindow* ddw) {}
	virtual BOOL OpenURL(OpenURLSetting& setting);

	virtual void GoToPage(const OpStringC& url, BOOL force_new_page, BOOL force_background, BOOL url_came_from_address_field, DesktopWindow* target_window, URL_CONTEXT_ID context_id, BOOL ignore_modifier_keys, BOOL privacy_mode) {}

	virtual BOOL OpenResolvedURL( OpenURLSetting& setting ) { return FALSE; }

	virtual void OpenSourceViewer(URL *url, UINT32 window_id, const uni_char* title, BOOL frame_source) {}

	virtual BOOL KioskFilter(OpInputAction* action) { return FALSE; }

	virtual void PrepareKioskMode() {}

	virtual void RegisterActivity() {}

	virtual void SetToolTipListener(OpToolTipListener* listener, uni_char key = 0) {}

	virtual ComposeDesktopWindow* ComposeMail(const URL& url, BOOL force_in_opera, BOOL force_background, BOOL new_window) { return 0; }

	virtual BOOL OpenExternalURLProtocol(URL& url) { return FALSE; }

	virtual void ExecuteProgram(const uni_char* program, const uni_char* parameters, BOOL run_in_terminal, const uni_char* protocol) {}

	virtual void SettingsChanged(SettingsType setting) {}

	virtual BOOL IsBrowserStarted() const { return FALSE; }

	virtual BOOL HasCrashed() const { return m_application->HasCrashed(); }

	virtual BOOL IsDragCustomizingAllowed() { return FALSE; }

	// ------ Below this line only used by Quick / adjunct ------ //

	virtual OpUiWindowListener* CreateUiWindowListener() { return m_application->CreateUiWindowListener(); }
	virtual DesktopOpAuthenticationListener* CreateAuthenticationListener() { return m_application->CreateAuthenticationListener(); }

	virtual DesktopMenuHandler* GetMenuHandler()
		{ return m_application->GetMenuHandler(); }

	virtual OP_STATUS AddSettingsListener(SettingsListener* listener) { return OpStatus::OK; }
	virtual void RemoveSettingsListener(SettingsListener* listener) {}

	virtual void SyncSettings() {}

	virtual DesktopWindowCollection& GetDesktopWindowCollection() { return m_window_collection; }

	virtual MailDesktopWindow* GoToMailView(INT32 index_id, const uni_char* address, const uni_char* title, BOOL force_show, BOOL force_new_window, BOOL force_download, BOOL ignore_modifier_keys) { return 0; }

	virtual void GoToChat(UINT32 account_id, const class ChatInfo& room, BOOL is_room, BOOL is_joining, BOOL open_in_background) {}

	virtual BOOL LeaveChatRoom(INT32 account_id, const OpStringC& name) { return FALSE; }
	virtual AccountTypes::ChatStatus GetChatStatusFromString(const uni_char* chat_status) { return AccountTypes::OFFLINE; }
	virtual AccountTypes::ChatStatus GetChatStatus(INT32 account_id) { return AccountTypes::OFFLINE; }
	virtual AccountTypes::ChatStatus GetChatStatus(INT32 account_id, BOOL& is_connecting) { return AccountTypes::OFFLINE; }
	virtual BOOL SetChatStatus(INT32 account_id, AccountTypes::ChatStatus chat_status, BOOL check_if_possible_only) { return FALSE; }

	virtual ComposeDesktopWindow* ComposeMail(const OpStringC& to, const OpStringC& message) { return 0; }
	virtual ComposeDesktopWindow* ComposeMail(OpINT32Vector& id_list) { return 0; }

	virtual MailDesktopWindow* GetActiveMailDesktopWindow() { return 0; }

	virtual BOOL SubscribeAccount(AccountTypes::AccountType type, const char* type_name, INT32 account_id, DesktopWindow* parent_window) { return FALSE; }
	virtual BOOL CreateAccount(AccountTypes::AccountType type, const char* type_name, DesktopWindow* parent_window) { return FALSE; }
	virtual BOOL ShowAccountNeededDialog(AccountTypes::AccountType type) { return FALSE; }
	virtual BOOL DeleteAccount(INT32 account_id, DesktopWindow* parent_window) { return FALSE; }
	virtual BOOL EditAccount(INT32 account_id, DesktopWindow* parent_window, BOOL modality) { return FALSE; }

	virtual void SaveCurrentSession(const uni_char* save_as_filename, BOOL shutdown, BOOL delay, BOOL only_save_active_window) {}

	virtual BOOL IsSDI() { return FALSE; }

	virtual BOOL ShowM2() { return FALSE; }

	virtual BOOL HasMail() { return FALSE; }
	virtual BOOL HasChat() { return FALSE; }

	virtual void AskEnterOnlineMode(BOOL test_offline_mode_first, OnlineModeCallback* callback, Str::LocaleString dialog_message = Str::S_OFFLINE_TO_ONLINE, Str::LocaleString dialog_title = Str::D_OFFLINE_MODE_TITLE) { OP_DELETE(callback); }
	virtual BOOL AskEnterOnlineMode(BOOL test_offline_mode_first) { return FALSE; }

	virtual BOOL CanViewSourceOfURL(URL * url) { return FALSE; }

	virtual BOOL OpenURL( const OpStringC &address, BOOL3 new_window, BOOL3 new_page, BOOL3 in_background, URL_CONTEXT_ID context_id, BOOL ignore_modifier_keys, BOOL is_remote_command, BOOL is_privacy_mode = FALSE);

	virtual void SetModalDialogOpen(BOOL is_open) {}

	virtual BOOL IsEmBrowser() const { return FALSE; }

	virtual PanelDesktopWindow* GetPanelDesktopWindow(OpTypedObject::Type type,
													BOOL create_new_if_needed,
													BOOL force_new,
													BOOL in_background,
													const uni_char* action_string_parameter) { return 0; }

	virtual DocumentDesktopWindow* CreateDocumentDesktopWindow(OpWorkspace*, OpWindowCommander* window_commander, INT32 width, INT32 height, BOOL show_toolbars, BOOL focus_document, BOOL force_background, BOOL no_speeddial, OpenURLSetting *setting, BOOL private_browsing) { return 0; }

	virtual BOOL CustomizeToolbars(OpToolbar* target_toolbar, INT32 initial_page) { return FALSE; }
	virtual BOOL IsCustomizingToolbars() { return FALSE; }
	virtual BOOL IsCustomizingHiddenToolbars() { return FALSE; }
	virtual CustomizeToolbarDialog* GetCustomizeToolbarsDialog() { return 0; }

	virtual RunType DetermineFirstRunType() { return m_application->DetermineFirstRunType(); }

	virtual OP_STATUS Start()  { return m_application->Start(); }
	virtual void Exit(BOOL force) {}
	virtual BOOL IsExiting() const { return m_application->IsExiting(); }

	virtual BOOL IsOpeningInNewWindowPreferred(BOOL ignore_modifier_keys, BOOL from_address_field) { return FALSE; }

	virtual BOOL IsOpeningInNewPagePreferred(BOOL ignore_modifier_keys, BOOL from_address_field) { return FALSE; }

	virtual BOOL IsOpeningInBackgroundPreferred(BOOL ignore_modifier_keys) { return FALSE; }

	virtual OpTreeModel* GetWindowList() { return 0; }

	virtual void ReorderDesktopWindow(DesktopWindow* desktop_window, OpWorkspace* target_workspace, INT32 to_index) {}
	virtual OP_STATUS AddDesktopWindow(DesktopWindow* desktop_window, DesktopWindow* parent_window) { return OpStatus::OK; }
	virtual OP_STATUS RemoveDesktopWindow(DesktopWindow* desktop_window) { return OpStatus::OK; }

	virtual OpToolTipListener* GetToolTipListener() { return 0; }

	virtual OP_STATUS AddAddressBar(OpAddressDropDown* address_bar) { return OpStatus::ERR; }
	virtual OP_STATUS RemoveAddressBar(OpAddressDropDown* address_bar) { return OpStatus::ERR; }

	virtual AnchorSpecialManager & GetAnchorSpecialManager() { return *(AnchorSpecialManager*)0; }
	virtual BOOL AnchorSpecial(OpWindowCommander * commander, const OpDocumentListener::AnchorSpecialInfo & info) { return FALSE; }

	virtual void SetConfirmOnlineShow() {}
	virtual BOOL GetShowGoOnlineDialog() { return FALSE; }
	virtual void AddOfflineQueryCallback(OpDocumentListener::DialogCallback* callback) {}
	virtual void UpdateOfflineMode(OpDocumentListener::DialogCallback::Reply reply) {}

	virtual void ParseMailComponents(const URL& url, MailComponents& components, BOOL m2_handles_mail = TRUE) {}

	virtual void UpdateToolTipInfo() {}

	virtual OperaVersion GetPreviousVersion() { return m_application->GetPreviousVersion(); }

	virtual BOOL ShowOpera(BOOL show, BOOL force_activate);

	virtual UiContext* GetInputContext() { return m_application->GetInputContext(); }

	// ------ Below this line only used by platforms ------ //

	virtual OP_STATUS	AddBrowserDesktopWindowListener(BrowserDesktopWindowListener* listener) { return OpStatus::ERR; }
	virtual void		RemoveBrowserDesktopWindowListener(BrowserDesktopWindowListener* listener) {}
	virtual void		BroadcastOnBrowserWindowCreated(BrowserDesktopWindow *window) {}
	virtual void		BroadcastOnBrowserWindowActivated(BrowserDesktopWindow *window) {}	
	virtual void		BroadcastOnBrowserWindowDeactivated(BrowserDesktopWindow *window) {}	
	virtual void		BroadcastOnBrowserWindowDeleting(BrowserDesktopWindow *window) {}

	virtual BOOL IsShowingOpera() const { return m_is_showing_opera; }

	virtual void SetStartupSetting(const StartupSetting &setting) {}

	virtual BOOL HasFeeds() { return FALSE; }

	virtual OpLanguageManager* GetDefaultLanguageManager() { return 0; }

	virtual ComposeDesktopWindow* ComposeMail(const OpStringC& to, const OpStringC& cc, const OpStringC& bcc, const OpStringC& subject, const OpStringC& message, const OpStringC& raw_mailto_address, MAILHANDLER handler, BOOL force_background , BOOL new_window) { return 0; }

    virtual DesktopWindow* GetDesktopWindowFromAction(OpInputAction* action) { return 0; }
	virtual OpDocumentContext* GetLastSeenDocumentContext() { return 0; }
	virtual void SetLastSeenDocumentContext(OpDocumentContext * context) {}

	virtual BOOL LockActiveWindow() { return FALSE; }
	virtual void SetLockActiveWindow(BOOL val) {}
	virtual DesktopOpAuthenticationListener* GetAuthListener() { return NULL; }
	virtual OpApplicationCacheListener* GetApplicationCacheListener() { return NULL; }

	virtual UINT32 GetClipboardToken() {return 0;}
	
	virtual void SetPopupDesktopWindow(DesktopWindow* win){}
	virtual void ResetPopupDesktopWindow(){}
	virtual DesktopWindow* GetPopupDesktopWindow() {return NULL;}
	
private:
	NonBrowserApplication* m_application;
	DesktopWindowCollection m_window_collection;
	BOOL m_is_showing_opera;
};

#endif // APPLICATION_ADAPTER_H
