/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef DESKTOP_EXTENSIONS_MANAGER_H
#define DESKTOP_EXTENSIONS_MANAGER_H

#include "adjunct/quick/extensions/ExtensionInstaller.h"
#include "adjunct/quick/extensions/ExtensionUIListener.h"
#include "adjunct/quick/extensions/ExtensionSpeedDialHandler.h"
#include "adjunct/quick/extensions/TabAPIListener.h"
#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick/models/ExtensionsModel.h"
#include "adjunct/quick/windows/OpRichMenuWindow.h"
#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"


#define g_desktop_extensions_manager (DesktopExtensionsManager::GetInstance())

class ExtensionPrefsController;
class ExtensionPopupController;
class OpExtensionButton;
class OpGadgetClass;
class OpWindowCommander;
class OpWorkspace;


/**
 * @brief Desktop Manager for Desktop Extensions.
 *
 * Provides functions for installation/deinstallation, updates
 * of desktop extensions. Handles actions from Extensions Manager
 * panel (@see ExtensionsPanel).
 */
class DesktopExtensionsManager :
		public DesktopManager<DesktopExtensionsManager>,
		public OpRichMenuWindow::Listener,
		public OpPageListener,
		public DialogContextListener,
		public OpTimerListener
{
	public:

		DesktopExtensionsManager();
		virtual ~DesktopExtensionsManager();
		virtual OP_STATUS Init();

		void StartAutoStartServices();
		BOOL HandleAction(OpInputAction* action);

		/**
		 * @see ExtensionsModel
		 */
		OP_STATUS AddExtensionsModelListener(ExtensionsModel::Listener* listener);
		OP_STATUS RemoveExtensionsModelListener(ExtensionsModel::Listener* listener);

		OP_STATUS SetExtensionDataListener(const OpStringC& extension_id, ExtensionDataListener* listener);

		OP_STATUS RefreshModel();

		BOOL CanShowOptionsPage(const OpStringC& extension_id);

		/**
		 * Open an extension's options page in a DocumentDesktopWindow.
		 *
		 * @param extension_id extension WUID
		 * @return status
		 */
		OP_STATUS OpenExtensionOptionsPage(const OpStringC& extension_id);

		/**
		 * Open an extension's options page in a call-out dialog.
		 *
		 * @param extension_id extension WUID
		 * @param controller the dialog's controller
		 * @return status
		 */
		OP_STATUS OpenExtensionOptionsPage(const OpStringC& extension_id, ExtensionPrefsController& controller);

		OP_STATUS SetExtensionSecurity(const OpStringC& extension_id, BOOL ssl_access, 
				BOOL private_access);
		OP_STATUS GetExtensionSecurity(const OpStringC& extension_id, BOOL& ssl_access, 
				BOOL& private_access);

		OP_STATUS UninstallExtension(const OpStringC& extension_id);

		OP_STATUS EnableExtension(const OpStringC& extension_id);
		OP_STATUS DisableExtension(const OpStringC& extension_id, BOOL user_requested = TRUE, BOOL notify_listeners = TRUE);

		ExtensionButtonComposite* GetButtonFromModel(INT32 id);
		OP_STATUS CreateButtonInModel(INT32 id);
		OP_STATUS DeleteButtonInModel(INT32 id);

		/**
		 * @param extension_id Extension id.
		 * @return Status.
		 */
		OP_STATUS ShowExtensionUninstallDialog(const uni_char* extension_id);

		/**
		 * Called after OpExtensionButton has been removed from composite.
		 *
		 * @param id Extension id.
		 */
		void OnButtonRemovedFromComposite(INT32 id);

		/**
		 * @see ExtensionInstaller::Install*
		 */
		OP_STATUS InstallFromExternalURL(const OpStringC& url_name, BOOL silent_installation = FALSE, BOOL temp_mode = FALSE);
		OP_STATUS InstallFromLocalPath(const OpStringC& extension_path);
		
		/**
		 * Create a UI window for an extension.
		 *
		 * Called when handling OpUiWindowListener::CreateUiWindow() coming
		 * from Core.
		 *
		 * @param commander the extension's OpWindowCommander
		 * @param args describes the kind of window to be created
		 * @return status
		 */
		OP_STATUS CreateExtensionWindow(OpWindowCommander* commander, const OpUiWindowListener::CreateUiWindowArgs& args);

		/**
		 * Update functionality.
		 */
		OP_STATUS AskForExtensionUpdate(const OpStringC& extension_id);
		void UpdateExtension(const OpStringC& extension_id);
		OP_STATUS ShowUpdateAvailable(const OpStringC& extension_id);
		OP_STATUS CheckForDelayedUpdate(OpGadget* gadget,BOOL &updated);
		OP_STATUS CheckForDelayedUpdates();
		void CheckForUpdate();		
		void ShowUpdateNotification();
		void ShowUpdateNotification(OpGadget* last_updated, OpGadget* next_to_last_updated, 
				int number_of_updates_made);

		/**
		 * Checks if update of extension given by id was downloaded from a trusted host.
		 * When update is downloaded we create file update_url.dat in extension's profile
		 * directory where we put update's download url. This method checks if the host
		 * located in this file is located on a list of trusted hosts.
		 *
		 * @param extension_id id of the extension which we want to check
		 * @return true if host from which update was downloaded is a trusted host, false otherwise
		 */
		bool IsUpdateFromTrustedHost(const OpStringC& extension_id);

		/**
		  * Checks if update of extension represented by instance of class OpGadget was downloaded
		  * from a trusted host. For more details @see IsUpdateFromTrustedHost(const OpStringC&).
		  */
		bool IsUpdateFromTrustedHost(OpGadget& gadget);

		/**
		  * Shows dialog which informs user that extension's update was blocked
		  * as it was downloaded from untrusted repository.
		  *
		  * @param repository URL of the untrusted repository.
		  */
		void ShowUntrustedRepositoryWarningDialog(OpStringC repository) const;


		void StartListeningTo(OpWorkspace* workspace);
		void StopListeningTo(OpWorkspace* workspace);

		OP_STATUS RemoveDuplicatedSDExtensions(OpGadget* gadget);
		OP_STATUS UpdateExtension(GadgetUpdateInfo* data,OpStringC& update_source);

		void CloseDevExtensionPopup();

		/**
		* Regular extension popup, platform popup, 
		* is closed autmaticly when user changes focus to something else. 
		* Only time, when there is a need to manually close popup, 
		* is when new tab from extension popup is created. 
		*/
		void CloseRegExtensionPopup();

		//
		// OpPageListener
		//

		virtual void OnPageClose(OpWindowCommander* commander);
		virtual BOOL OnPagePopupMenu(OpWindowCommander* commander,
				OpDocumentContext& context) { return TRUE; }
		//
		// OpRichMenuWindow::Listener
		//

		virtual void OnMenuClosed();
		/** 
		 * By usage of this function you can open any url, including internal gadget url
		 * which is not working via g_application->GoToPage(..)
		 */
		OP_STATUS OpenExtensionUrl(const OpStringC& url,const OpStringC& wuid);

		/**
		 * Retrive from extension pointed by extension_id fallback url from config.xml
		 * 
		 * @param extension_id Extension wuid.
		 * @param fallback_url - here fallback url will be returned, 
		 *						if extension with extension_id exist
		 * @return Status.
		 */
		OP_STATUS GetExtensionFallbackUrl(const OpStringC& extension_id, OpString& fallback_url);

		/**
		 *  SD feature
 		 */
		void ExtensionDataUpdated(const OpStringC& extension_id);


		/** Install all extensions from custom folder. It should be called after
		 * SpeedDialManager is initialized and after it loaded speed dials from ini file.
		 */
		OP_STATUS InstallCustomExtensions();

		/**
		 * Returns number of installed extensions (including extensions in dev mode).
		 */
		unsigned GetExtensionsCount() const
				{ return m_extensions_model.GetCount(); }

		/**
		 *  Cancel download started by InstallRemote
		 *  @param download_url address, must be the same as passed to InstallRemote
		 */
		void CancelInstallationDownload(const OpStringC& download_url)
		{
			m_extension_installer.AbortDownload(download_url);
		};

		/** 
		 * Marks extensions as temporary
		 * Extension installed via SD add dialog might be considered as a temporary (until it is confirmed)
		 * which results in blocking some regular actions handling (see ExtensionSpeedDialHandler)
		 */
		OP_STATUS SetExtensionTemporary(const OpStringC& extension_id, BOOL temporary);
		BOOL IsExtensionTemporary(const OpStringC& extension_id);

		OP_STATUS AddInstallListener(ExtensionInstaller::Listener* listener) 
				{return m_extension_installer.AddListener(listener);}

		OP_STATUS RemoveInstallListener(ExtensionInstaller::Listener* listener)
				{return m_extension_installer.RemoveListener(listener);}

		/**
		 * Retrieve all installed speed dial extensions.
		 *
		 * @param[out] extensions vector of all installed speed dial extensions
		 * @return status of operation. If any error is returned @a extensions should not be used
		 */
		OP_STATUS GetAllSpeedDialExtensions(OpVector<OpGadget>& extensions)
			{ return m_extensions_model.GetAllSpeedDialExtensions(extensions); }

		/**
		 * Adds extension's speed dial represented by instance of class gadget
		 * to the end of all speed dials.
		 *
		 * @parameter gadget gadget which represents extension's speed dial
		 * @return status
		 */
		OP_STATUS AddSpeedDial(OpGadget& gadget) { return m_speeddial_handler.AddSpeedDial(gadget); }

		/**
		 * Reloads the specified extension. Shorthand for both reload the extension and refresh the model.
		 * It's used in extension view, as well as context menu of LSDs and extention button.
		 *
		 * @parameter id the gadget id of the extension to reload
		 * @return BOOL return value that can be used in HandleAction
		 */
		BOOL ReloadExtension(const OpStringC& id)
		{
			RETURN_VALUE_IF_ERROR(m_extensions_model.ReloadExtension(id), FALSE);
			RETURN_VALUE_IF_ERROR(m_extensions_model.Refresh(), FALSE);
			
			return TRUE;
		}

		//pass-through, for doc see ExtensionInstaller::CheckUIButtonForInstallNotification
		void CheckUIButtonForInstallNotification(class OpExtensionButton* button,BOOL show)
		{
			m_extension_installer.CheckUIButtonForInstallNotification(button,show);
		}

		void ShowPopup(OpWidget* opener, INT32 composite_id);
		void SetPopupURL(INT32 composite_id, const OpStringC& url);
		void SetPopupSize(INT32 composite_id,UINT32 width, UINT32 height);
		BOOL HandlePopupAction(OpInputAction* action);

		virtual void OnDialogClosing(DialogContext* context);

		ExtensionUIListener* GetExtensionUIListener() { return &m_extension_ui_listener; }
		TabAPIListener* GetTabAPIListener() { return &m_tab_api_listener; }

		/**
		 * Returns true if new extensions should be installed in silent mode (i.e. should
		 * not be focused after installation).
		 */
		bool IsSilentInstall() const { return m_silent_install; }

		// OpTimerListener interface
		void OnTimeOut(OpTimer* time) { CheckForUpdate(); }

	private:
		/** 
		 * Create a window for an extension's main script. 
		 */
		OP_STATUS CreateDummyExtensionWindow(OpWindowCommander* commander);

		/** 
		 * Create a window for a Speed Dial extension. 
		 */
		OP_STATUS CreateSpeedDialExtensionWindow(OpWindowCommander* commander);

		/** 
		 * Create a window for an extension's options page. 
		 */
		OP_STATUS CreateExtensionOptionsWindow(OpWindowCommander* commander, const OpStringC& url);

		/** 
		 * Create a window for an extension's options page. 
		 */
		OP_STATUS CreateExtensionPopupWindow(OpWindowCommander* commander, const OpStringC& url);		

		/**
		 * Gets extension URL.
		 *
		 * @param extension_id Id of extension from which host will be taken.
		 * @param url Will be filed with extension URL.
		 *
		 * @param return OK if host has been filled, ERR when extension was not found,
		 *               ERR_FILE_NOT_FOUND when extension file does not exist,
		 *               OOM in case of out of memomry.
		 */
		OP_STATUS GetURL(const OpStringC& extension_id, OpString& url) const;

		/**
		 * Gets gadget URL.
		 *
		 * @param gadget Gadget from which host will be taken.
		 * @param url Will be filed with gadget URL.
		 *
		 * @param return OK if host has been filled, ERR_FILE_NOT_FOUND when gadget
		 *               file does not exist, OOM in case of out of memomry.
		 */
		OP_STATUS GetURL(OpGadget& gadget, OpString& url) const;

		ExtensionInstaller        m_extension_installer;
		ExtensionUIListener       m_extension_ui_listener;
		TabAPIListener            m_tab_api_listener;
		OpStringHashTable<ExtensionDataListener> m_extension_data_listeners;

		OpGadget*                 m_last_updated;
		OpGadget*                 m_next_to_last_updated;
		int                       m_number_of_updates_made;

		ExtensionsModel           m_extensions_model;
		OpRichMenuWindow*         m_popup_window;
		BOOL                      m_popup_active;
		ExtensionSpeedDialHandler m_speeddial_handler;
		ExtensionPrefsController* m_extension_prefs_controller;
		ExtensionPopupController* m_popup_controller; // extension popup dialog for developer mode
		INT32                     m_popup_composite_id;
		OpExtensionButton*        m_extension_popup_btn; // pointer to opener of the popup dialog, in case opened via button
		OpString                  m_last_popup_url;
		bool                      m_silent_install; // when true new extensions should not be focused after installation
		OpTimer                   m_uid_retrieval_retry_timer;
		enum
		{
			MAX_UUID_RETRIEVAL_RETRIES = 5
		};
		unsigned                  m_uid_retrieval_retry_count;
};

#endif // DESKTOP_EXTENSIONS_MANAGER_H
