/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef EXTENSION_INSTALLER_H
#define EXTENSION_INSTALLER_H

#include "adjunct/desktop_util/handlers/SimpleDownloadItem.h"
#include "adjunct/quick_toolkit/widgets/DialogListener.h"
#include "modules/util/adt/oplisteners.h"
#include "modules/hardcore/timer/optimer.h"

class OpGadget;
class OpGadgetClass;
struct GadgetUpdateInfo;

class ExtensionInstaller :
		public SimpleDownloadItem::Listener,
		public OpTimerListener
{
	public:
		enum VersionType
		{
			NoVersionFound,
			OtherVersion,
			SameVersion,
		};

		class Listener
		{
			public:
				virtual ~Listener() {}

				/**
				 * Called after extension download has been started.
				 * @param extension_installer ExtensionInstaller which has started
				 *        extension download.
				 */
				virtual void OnExtensionDownloadStarted(ExtensionInstaller& extension_installer) = 0;

				/**
				 * Called during extension download progress.
				 * @param loaded size of already loaded data of current extension being downloaded.
				 * @param total total size of current extension being downloaded.
				 */
				virtual void OnExtensionDownloadProgress(OpFileLength loaded, OpFileLength total) = 0;

				/**
				 * Called after extension download finished.
				 */
				virtual void OnExtensionDownloadFinished() = 0;

				/**
				 * Called after extension has been installed
				 * @param extension installed 
				 * @param source url, from which installation has been started
				 */
				virtual void OnExtensionInstalled(const OpGadget& extension, const OpStringC& source) = 0;

				/**
				 * Called after extnesion installation has been aborted.
				 */
				virtual void OnExtensionInstallAborted() = 0;

				/**
				 * Called after extnesion installation has failed.
				 */
				virtual void OnExtensionInstallFailed() = 0;
		};

		ExtensionInstaller();
		virtual ~ExtensionInstaller();

		OP_STATUS AddListener(Listener* listener);
		OP_STATUS RemoveListener(Listener* listener);

		/**
		 * Checks, if the button might be used for showing the installation notification.
		 * This function is called a lot. And I mean A-LOT. It's being called from
		 * OpExtensionButton::OnShow for every button being added everywhere. E*V*E*R*Y.
		 * In every toolbar, on every document page and in the future, possibly in other
		 * extension contexts as well.
		 *
		 * Be quick about it, exit as soon as staying here make no more sense.
		 */
		void CheckUIButtonForInstallNotification(class OpExtensionButton* button,BOOL show);

		/**
		 * Install extension from local disc pointed by extension_path.
		 *
		 * @param extension_path Local path to extension.
		 * @param silent_installation if set to TRUE installer will not ask if extension should be installed.
		 *        There will be also no indication that extension was installed or that there was any error.
		 * @return Status.
		 */
		OP_STATUS InstallLocal(const OpStringC& extension_path, BOOL silent_installation = FALSE);

		/**
		 * Install all extensions from given directory.
		 * Extensions will be installed in silent mode (installer will not ask if extension should
		 * be installed and there will be no indication either that it was installed successfully or that
		 * there was any installation error). Extension file (*.oex) will be deleted after
		 * successful installation.
		 *
		 * @param extension_path full path to a directory which extensions should be installed from.
		 * @return status
		 */
		OP_STATUS InstallExtensionsFromDirectory(const OpStringC& directory);

		/**
		 * Install extension from external location (URL).
		 *
		 * @param url_name the URL name
		 * @param silent_installation if set to TRUE installer will not ask if extension should be installed.
		 *        There will be also no indication that extension was installed or that there was any error.
		 * @param temp_mode flag, when installing only for temp_mode (temp mode)
		 * @return Status.
		 */
		OP_STATUS InstallRemote(const OpStringC& url_name,BOOL silent_installation,BOOL preview = FALSE);

		/**
		* For extensions which are running or have different access rights
		* update is not made immidiately
		* this function is used to do it on user request or after restart
		* @param gadget - (in) extention to be updated
		* @param updated - (out) if the update has been successful
		* @param force - TRUE, if update should be made even when assecc right are different
		*/
		OP_STATUS InstallDelayedUpdate(OpGadget* gadget, BOOL& updated, BOOL force = FALSE);
		
		/**
		* Called from the main installer dialog, when user allows installation 
		*/
		virtual void InstallAllowed(BOOL ssl_access, BOOL private_access);

		/**
		* Called from the mian installer dialog, when user deny installation 
		*/
		virtual void InstallCanceled();

		/**
		* Replaces one extension with another, might be used for update 
		* @param for_update - (in) gadget to be replaced
		* @param source - (in) path to update file 
		* @param updated - (out) gadget which has been updated
		* @delayed - (out) in case extensions is running, its replace is not made immidiately
		*/
		static OP_STATUS ReplaceExtension(	OpGadget** for_update,
											const OpStringC& update_source,
											BOOL& delayed);

		void AbortDownload(const OpStringC& download_url);

		/** Cancels current extension download. */
		void CancelDownload();

	protected:

		struct DelayedNotification
		{
			OpString title;
			OpString gadget_id;
			Image icon;
			OpWidget* follow;
			DelayedNotification(): follow(NULL) {}
		};

		/**
		*	Compares access level of two extesions
		*	
		*	@param gadget - 1st of the gadget
		*	@param update_file_path - 2st of the gadget, path to its file 
		*/
		static BOOL HasDifferentAccess(OpGadget* gadget,const OpStringC& file_path);

		/**
		*	Should be called when installation fails, notifies listeners 
		*	and optionally triggers fail dialog
		*/
		void InstallFailed(bool reason_network = false);

		OP_STATUS CheckIfTrusted(BOOL& trusted);

		/**
		 * Regardless of what was the initialization method install
		 * extension. If this extension is in external location it
		 * will be fetched first.
		 *
		 * @return Status.
		 */
		OP_STATUS InstallInternal();


		/**
		 * After Repository has been verified as trusted
		 *
		 * @return Status.
		 */
		OP_STATUS InstallInternalPart2();

		/** 
		 * Regular (non-dev) mode installation.
		 *
		 * @param ssl_access - if the extension has access to secure pages
		 * @param private_access - if the extension has access to private tabs
		 * @param gadget - extension gadget created during installation
		 * @return Status.
		 */
		virtual OP_STATUS Install(BOOL ssl_access, BOOL private_access,OpGadget** gadget);

		//
		// SimpleDownloadItem::Listener
		//

		virtual void DownloadSucceeded(const OpStringC& tmp_path);
		virtual void DownloadFailed();
		virtual void DownloadInProgress(const URL& url);

		/**
		 * If extension is installed from external URL we need
		 * to fetch it first. After successfull download from URL
		 * location appropriate listener methods are called.
		 *
		 * @return Status.
		 */
		OP_STATUS FetchExtensionFromURL();

		/**
		 * Developer mode installation.
		 *
		 * @return Status.
		 */
		OP_STATUS InstallDeveloperMode();

		/**
		 * Show installer dialog, this class is set as a listener
		 * for its OnOk and OnCancel methods.
		 *
		 * @return Status.
		 */
		virtual OP_STATUS ShowInstallerDialog();

		virtual void ShowInstallNotificationSuccess(OpGadget*);
		void InternalShowInstallNotificationSuccess(OpWidget*);

		/**
		 * Load gadget class from m_extension_path.
		 *
		 * @return Status.
		 */
		OP_STATUS LoadGadgetClass();

		/**
		 * Reset installer class, so it can be reused for installation
		 * of different extension. 
		 */
		void Reset();

		void CleanUp();

		OP_STATUS CheckIfTheSameExist(OpGadgetClass* gclass, VersionType& version, OpGadget** gadget);
		OP_STATUS ReplaceExtension(OpGadgetClass* gclass,OpGadget** gadget);
		void	  FinalizeInstallation();

		OP_STATUS InstallOrAskIfNeeded();

		static OP_STATUS ReplaceOexFile(const OpStringC& destination, const OpStringC& source,BOOL delete_source = FALSE);

		void NotifyExtensionInstalled(const OpGadget& gadget);
		void NotifyExtensionInstallAborted();
		void NotifyExtensionInstallFailed();

		void OnTimeOut(OpTimer* timer);

		OpString m_extension_path;
		OpGadgetClass* m_gclass;
		SimpleDownloadItem* m_simple_download_item;
		OpString m_url_name;
		BOOL m_dev_mode_install;
		BOOL m_busy;
		OpListeners<Listener> m_listeners;
		BOOL m_silent_installation;
		BOOL m_temp_mode;

		/*
		* After extension installation we might show popup pointing to extensions toolbar button.
		* Members below are used to handle that feature. 
		* 
		* Toolbar button might be created or not, depending on extensions. 
		* We will wait UI_TIMEOUT for the button creation. After button has been created, 
		* it might be updated stright away by the extension script, that's why we wait additional
		* SHOW_TICK time for final button state.
		*/
		DelayedNotification m_delayed_notification;
		OpTimer m_button_show_timer; // timer from installation to show of the toolbar button
		OpTimer m_button_firmness_timer; // timer from button show to button firmness.
		bool	m_waiting_for_button;	// valid until timers finish or popup shown.

	private:
		void NotifyDownloadStarted();
		void NotifyDownloadProgress(OpFileLength loaded, OpFileLength total);
		void NotifyDownloadFinished();

		/**
		 * (Re)Initialize new or updated extension.
		 *
		 * @param  id The unique ID of the extension.
		 * @param  ssl_access Whether extension is allowed to work on secure pages.
		 * @param  private_access Whether extension is allowed to work on private tabs.
		 *
		 * @return OpStatus::OK on success.
		 */
		OP_STATUS InitializeExtension(const uni_char* id, BOOL ssl_access, BOOL private_access);
};



#endif // EXTENSION_INSTALLER_H

