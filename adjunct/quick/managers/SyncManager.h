/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Adam Minchinton (adamm)
 * @author Co-owner: Karianne Ekern  (karie)
 *
 */

#ifndef __SYNC_MANAGER_H__
#define __SYNC_MANAGER_H__

#include "modules/sync/sync_coordinator.h"
#include "adjunct/quick/sync/SyncBookmarkItems.h"
#include "adjunct/quick/sync/SyncNoteItems.h"
#include "adjunct/quick/sync/SyncSearchEntries.h"

#include "adjunct/quick/sync/controller/SyncSettingsContext.h"

#include "adjunct/quick/controller/FeatureController.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick/sync/controller/SyncWidgetStateModifier.h"

#define QUICK_HANDLE_SUPPORTS

// Make DEBUG_LINK call the SyncLog function only when the DEBUG_LIVE_SYNC is defined and _DEBUG
#if defined(DEBUG_LIVE_SYNC) && defined(_DEBUG)
  #define DEBUG_LINK SyncManager::GetInstance()->SyncLog
#else
  inline void DEBUG_LINK(const void *, ...) {}
#endif // defined(DEBUG_LIVE_SYNC) && defined(_DEBUG)

class OpLabel;
class Dialog;
class OpInputAction;
class SyncSpeedDialEntries;

// Page shown after first use
#define SYNC_INFO_PAGE UNI_L("http://www.opera.com/products/link/desktop/")



#define g_sync_manager (SyncManager::GetInstance())

//////////////////////////////////////////////////////////////////////////////////////////////////////////////


class QuickSyncLock
{
public:
	QuickSyncLock(BOOL& lock, BOOL init_value, BOOL reset_to)
		: m_lock(lock), m_reset_to(reset_to)
	{
		m_lock = init_value;
	}

	~QuickSyncLock()
	{
		m_lock = m_reset_to;
	}
	
	BOOL IsLocked(){return m_lock == TRUE;}

 private:
	BOOL& m_lock;
	BOOL m_reset_to;
};

/*************************************************************************
 **
 ** SyncController
 **		Feature controller for link. Facade for calls needed by link wizard.
 *************************************************************************/

class SyncController : public FeatureController
{
public:
	virtual ~SyncController() {}

	virtual void		SetConfiguring(BOOL configuring) = 0;
	virtual BOOL		IsConfiguring() = 0;
};


/*************************************************************************
 **
 ** SyncManager
 **      is a Singleton
 **
 ** SyncManager::Listener
 **
 **
 **************************************************************************/

class SyncManager : public DesktopManager<SyncManager>,
					public OpTimerListener,
					public OpSyncUIListener,
					public MessageObject,
					public SyncController
{
public:

	enum SyncType
	{
		Now,
		Exit,
		Logout
	};

	enum StatusTextType
	{
		Normal,
		Permanent,
		Temporary
	};

	enum SyncDataType
	{
		SYNC_BOOKMARKS	= SYNC_SUPPORTS_BOOKMARK,
		SYNC_NOTES		= SYNC_SUPPORTS_NOTE,
		SYNC_SEARCHES = SYNC_SUPPORTS_SEARCHES,
		SYNC_TYPEDHISTORY = SYNC_SUPPORTS_TYPED_HISTORY,
		SYNC_URLFILTERS = SYNC_SUPPORTS_URLFILTER,
		SYNC_SPEEDDIAL	= SYNC_SUPPORTS_SPEEDDIAL_2,
		SYNC_PASSWORD_MANAGER = SYNC_SUPPORTS_PASSWORD_MANAGER,
		SYNC_PERSONALBAR = SYNC_SUPPORTS_MAX // Desktop only
	};

public:
	SyncManager();
	~SyncManager();

	OP_STATUS	Init();
	BOOL		IsInited() { return m_inited; }

	OP_STATUS	EnableIfRequired(BOOL& has_started);
	OP_STATUS	ShowLoginDialog(BOOL* logged_in, DesktopWindow* parent = NULL);
	
	/**
	 * Show Link login dialog, if the result from the dialog is that we are
	 * logged in then enable Link.
	 *
	 * @param parent Parent window for the login dialog.
	 * @return Status.
	 */
	OP_STATUS 	ShowLoginDialogAndEnableLink(DesktopWindow* parent = NULL);	

	OP_STATUS	ShowSetupWizard() { return ShowSetupWizard(NULL); } // not blocking
	OP_STATUS	ShowSetupWizard(BOOL * sync_enabled) { return ShowSetupWizard(NULL, GetSavedSettings(), g_desktop_account_manager->GetAccountContext()); } // blocking if webserver_enabled != NULL
	OP_STATUS	ShowSetupWizard(BOOL * sync_enabled, SyncSettingsContext * settings_context, OperaAccountContext* account_context);
	OP_STATUS	ShowSettingsDialog();
	OP_STATUS	ShowStatus();

	//=== MessageObject implementation ===========
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	virtual BOOL	IsFeatureEnabled() const;
	virtual void	EnableFeature(const FeatureSettingsContext* settings);
	virtual void	DisableFeature();
	virtual void	SetFeatureSettings(const FeatureSettingsContext* settings);
	virtual void	InvokeMessage(OpMessage msg, MH_PARAM_1 param_1, MH_PARAM_2 param_2);

	void SetSyncActive(BOOL active);

	BOOL SupportsType(SyncDataType data_type);
	OP_STATUS SetSupportsType(SyncDataType data_type, BOOL enable);

	void SetLinkEnabled(BOOL enabled, BOOL auto_login = FALSE, BOOL save_pref = TRUE);
	BOOL IsLinkEnabled() const { return m_settings_context.IsFeatureEnabled(); }

	void UpdateOffline(BOOL offline);

	// m_comm_not_working - TRUE if we got a communication error from the server
	BOOL IsCommWorking() { return !m_comm_not_working; }

	// m_sync_module_enabled - FALSE if the sync module couldn't start up
	BOOL SyncModuleEnabled()           { return m_sync_module_enabled; }
	// Function to check if we can sync, TRUE if not in kiosk or offline mode.
	BOOL CanSync();

	// OpTimerListener implementation
	void OnTimeOut(OpTimer* timer);

	/**
	 * @see OpSyncUIListener
	 */
	virtual void OnSyncStarted(BOOL items_sent);
	virtual void OnSyncError(OpSyncError error, const OpStringC& error_msg);
	virtual void OnSyncFinished(OpSyncState& sync_state);
	virtual void OnSyncReencryptEncryptionKeyFailed(ReencryptEncryptionKeyContext* context);
	virtual void OnSyncReencryptEncryptionKeyCancel(ReencryptEncryptionKeyContext* context);

	OP_STATUS SyncNow(SyncManager::SyncType sync_type) { return SyncNow(sync_type, FALSE); }
	OP_STATUS SyncNow(SyncManager::SyncType sync_type, BOOL force_complete_sync);

	// Methods for integration with quick sync classes
	void StartSyncTimer(UINT32 interval);

	// returns true if sync state != 0
	BOOL HasUsedSync();

	OperaAccountManager::OAM_Status GetStatus() { return m_status; }

#ifdef DEBUG_LIVE_SYNC
	// This is to write info out to a debug file
	void SyncLog(const uni_char *format, ...);
#endif // DEBUG_LIVE_SYNC

	OP_STATUS BackupFile(OpFile& file, const uni_char* suffix = NULL);

	virtual void	SetConfiguring(BOOL configuring);
	virtual BOOL	IsConfiguring();

	WidgetStateModifier*	GetWidgetStateModifier()	{ return &m_state_modifier; }
	SyncSettingsContext*	GetSavedSettings()			{ return &m_settings_context; }

	void RetryPasswordRecovery(const OpStringC& old_pass);
	void AbandonPasswordRecovery();
	
private:
	void	SetFeatureSettingsWithoutEnabling(const FeatureSettingsContext* context);
	void	SetDefaultSettings();
	void	LogError(OpSyncError error);

	BOOL	m_inited;				// Set to TRUE after a successful Init() call
	BOOL	m_comm_not_working;		// Set to TRUE if we receive an error saying we are having trouble commuicating with the link server
	BOOL    m_is_syncing;           // Set whenever SyncNow started a sync and waiting for the reply
	BOOL	m_is_final_syncing;		// Set after SyncNow is called.
	BOOL	m_final_sync_and_close;	// Set if the SyncNow is to close, otherwise it just logs out.

	BOOL                    m_sync_module_enabled; // TRUE if Sync module was initialized

	OpAutoPtr<OpTimer>      m_status_timer;

	OpString                m_current_permanent_status;

	OpAutoPtr<OpTimer>      m_shutdown_timer;				// Timer to show the shutdown progress dialog

	SyncBookmarkItems      *m_bookmarks_syncer;
	SyncNoteItems          *m_notes_syncer;
	SyncSpeedDialEntries   *m_speed_dial_syncer;
	SyncSearchEntries*      m_searches_syncer;

	BOOL                    m_pref_initialized;

#ifdef DEBUG_LIVE_SYNC
	OpFile					m_log_file;
#endif // DEBUG_LIVE_SYNC

	OpString				m_current_error_message;

	OperaAccountManager::OAM_Status		m_status; // Status of the Opera Link service

	SyncWidgetStateModifier	m_state_modifier;
	SyncSettingsContext		m_settings_context;
	MessageHandler*			m_msg_handler;
	INT32					m_suggested_error_action;

private:

	void		UpdateStatus(OperaAccountManager::OAM_Status status, BOOL force_now = FALSE);

	enum SyncConfigureStyle {
		NotConfiguring = 0,
		SetupWizard,
		SettingsDialog,
		LoginDialog
	};
	SyncConfigureStyle         m_configure_style;
	class SyncDialog*          m_configure_dialog;
	ReencryptEncryptionKeyContext* m_reencryption_current_context;
	OperaAccountManager::OAM_Status m_reencryption_recovery_status;
};

#endif // __SYNC_MANAGER_H__
