/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef EXTENSIONS_MODEL_H
#define EXTENSIONS_MODEL_H

#include "adjunct/quick/extensions/ExtensionButtonComposite.h"
#include "adjunct/quick/models/ExtensionsModelItem.h"
#include "adjunct/desktop_util/adt/hashiterator.h"

#include "modules/util/adt/oplisteners.h"
#include "modules/util/OpHashTable.h"
#include "modules/windowcommander/OpWindowCommander.h"

/**
 * @brief Model for Desktop Extensions.
 *
 * This class is a wrapper around core's OpGadgetManager.
 * It gathers all gadgets that are extensions, manages their UI buttons,
 * provides useful operations on extensions, notifies about changes.
 * Listeners of this model can be notified about (normal or developer)
 * extensions installation, uninstallation, disabling, enabling etc.
 */
class ExtensionsModel
{
	public:

		struct RefreshInfo
		{
			RefreshInfo() : m_dev_count(0), m_normal_count(0) { }

			/**
			 * Count of developer extensions in the model.
			 */
			unsigned m_dev_count;

			/**
			 * Count of non-developer extensions in the model.
			 */
			unsigned m_normal_count;
		};

		class Listener
		{
			public:
				virtual ~Listener() {}

				/**
				 * When Extensions Model is about to be "refreshed" this
				 * event occurs. For the listener it means all data in the
				 * model has become out-of-date. New data will arrive soon.
				 */
				virtual void OnBeforeExtensionsModelRefresh(
						const RefreshInfo& info) = 0;

				/**
				 * When Extensions Model is done with "refreshing" itself
				 * this event occurs. For the listener it means all data in the
				 * model is valid and it can be processed.
				 */
				virtual void OnAfterExtensionsModelRefresh() = 0;

				/**
				 * Notify listener about new normal extension that has been 
				 * added to the model.
				 *
				 * @param model_item Extensions model item.
				 */
				virtual void OnNormalExtensionAdded(
						const ExtensionsModelItem& model_item) = 0;

				/**
				 * Notify listener about new developer extension that has been 
				 * added to the model.
				 *
				 * @param model_item Extensions model item.
				 */
				virtual void OnDeveloperExtensionAdded(
						const ExtensionsModelItem& model_item) = 0;

				/**
				 * Event occurs when extension described by extension
				 * model item is about to be removed (so in this event
				 * it is perfectly safe to use it).
				 *
				 * @param model_item Extensions model item.
				 */
				virtual void OnExtensionUninstall(
						const ExtensionsModelItem& model_item) = 0;

				/**
				 * Notify listener that existing extension was started.
				 *
				 * @param model_item Extensions model item.
				 */
				virtual void OnExtensionEnabled(
						const ExtensionsModelItem& model_item) = 0;

				/**
				 * Notify listener that existing extension was stopped.
				 *
				 * @param model_item Extensions model item.
				 */
				virtual void OnExtensionDisabled(
						const ExtensionsModelItem& model_item) = 0;

				/**
				 * Notify listener that status of update availability has changed
				 *
				 * @param model_item Extensions model item.
				 */
				virtual void OnExtensionUpdateAvailable(
						const ExtensionsModelItem& model_item) = 0;

				/**
				* Notify listener that extended name for extension has been changed
				*/
				virtual void OnExtensionExtendedNameUpdate(
						const ExtensionsModelItem& model_item) = 0;
		};

		~ExtensionsModel();

		OP_STATUS Init();

		/**
		 * Starts extensions, which have been configured to
		 * start with browser startup
		 */
		void StartAutoStartServices();

		/**
		 * Notify listeners about incoming refresh. Delete all old data,
		 * create new model items, send notifications.
		 *
		 * @see ExtensionsModel::OnBeforeExtensionsModelRefresh
		 * @see ExtensionsModel::OnAfterExtensionsModelRefresh
		 *
		 * @return Status.
		 */
		OP_STATUS Refresh();

		/**
		 * Listeners handling functions.
		 */
		OP_STATUS AddListener(Listener* listener);
		OP_STATUS RemoveListener(Listener* listener);

		/**
		 * Uninstall functions.
		 */
		OP_STATUS UninstallExtension(const OpStringC& extension_id);
		OP_STATUS UninstallExtension(ExtensionsModelItem* item);

		/**
		 * Enable/Disable functions.
		 */
		OP_STATUS EnableExtension(const OpStringC& extension_id);
		OP_STATUS EnableExtension(ExtensionsModelItem* item);
		OP_STATUS DisableExtension(const OpStringC& extension_id, BOOL user_requested = FALSE, BOOL notify_listeners = TRUE);
		OP_STATUS DisableExtension(ExtensionsModelItem* item, BOOL user_requested = FALSE, BOOL notify_listeners = TRUE);

		/**
		 * Reload extension data (eg. js, html, window).
		 *
		 * @param extension_id Extension id.
		 * @return Status.
		 */
		OP_STATUS ReloadExtension(const OpStringC& extension_id);

		/**
		 * When extension is running and update is available
		 *
		 * @param extension_id Extension id.
		 * @return Status.
		 */
		OP_STATUS UpdateAvailable(const OpStringC& extension_id);

		/**
		* Extension can have additional part of the name, set for example from SD cell title
		* 
		* @param extension_id Extension id.
		* @param name new extension extended name.
		*/
		OP_STATUS UpdateExtendedName(const OpStringC& extension_id,const OpStringC& name);
		/**
		 * When extension update has been made
		 *
		 * @param extension_id Extension id.
		 * @return Status.
		 */
		OP_STATUS UpdateFinished(const OpStringC& extension_id);

		/**
		 * @param extension_id Extension id.
		 * @return TRUE iff extension is enabled on CORE's side.
		 */
		BOOL IsEnabled(const OpStringC& extension_id);

		/**
		 * @param extension_id Extension id.
		 * @return TRUE iff extension options page can be used/showed.
		 */
		BOOL CanShowOptionsPage(const OpStringC& extension_id);

		/**
		 * Send request to core that we want to open this extension's
		 * options page.
		 *
		 * @param extension_id Extension id.
		 * @return Status.
		 */
		OP_STATUS OpenExtensionOptionsPage(const OpStringC& extension_id);

		/** 
		 * @param extension_id Extension id (wuid).
		 * @return ExtensionsModelItem* for extension or NULL if there's
		 * no extension with this extension_id. 
		 */
		ExtensionsModelItem* FindExtensionById(const OpStringC& extension_id) const;

		/** 
		* By usage of this function you can open any url, including internal gadget url
		* which is not working via g_application->GoToPage(..)
		* 
		* @param extension_id Extension id.
		* @return Status.
		*/
		OP_STATUS OpenExtensionUrl(const OpStringC& url,const OpStringC& extension_id);

		typedef OpExtensionUIListener::ExtensionId ExtensionId;

		/**
		 * @return NULL iff ExtensionButton with such id doesn't exist.
		 */
		ExtensionButtonComposite* GetButton(ExtensionId id);
		OP_STATUS CreateButton(ExtensionId id);
		OP_STATUS DeleteButton(ExtensionId id);

		/**
		 * Called after OpExtensionButton has been removed from composite.
		 * @param id extension id
		 */
		void OnButtonRemovedFromComposite(INT32 id);

		/**
		 * Returns number of extension model items.
		 */
		unsigned GetCount() const
		{ OP_ASSERT(m_extensions.GetCount() >= 0); return m_extensions.GetCount(); }

		/**
		 * Retrieve all installed speed dial extensions.
		 *
		 * @param[out] extensions vector of all installed speed dial extensions
		 * @return status of operation. If any error is returned @a extensions should not be used
		 */
		OP_STATUS GetAllSpeedDialExtensions(OpVector<OpGadget>& extensions);

		/**
		 * Reloads all running developer mode extensions.
		 */
		void ReloadDevModeExtensions();

	protected:

		OpAutoStringHashTable<ExtensionsModelItem> m_extensions;
		OpAutoINT32HashTable<ExtensionButtonComposite> m_extension_buttons;
		// buttons to be deleted after all associated UI elements are destroyed
		OpAutoINT32HashTable<ExtensionButtonComposite> m_zombie_buttons;
		OpListeners<Listener> m_listeners;

		static int CompareItems(const ExtensionsModelItem** a, const ExtensionsModelItem** b);

		/**
		 * If extension is running close it's Window, and destroy
		 * platform OpWindow also. Make it not running :)
		 *
		 * @param extension_id Extension id.
		 * @return Status.
		 */
		OP_STATUS CloseExtensionWindows(OpGadget* extension);

		// Helper functions for propagating notifications.

		void NotifyAboutBeforeRefresh(const ExtensionsModel::RefreshInfo& info);
		void NotifyAboutAfterRefresh();
		void NotifyAboutNormalExtensionAdded(const ExtensionsModelItem& item);
		void NotifyAboutDeveloperExtensionAdded(const ExtensionsModelItem& item);
		void NotifyAboutUninstall(const ExtensionsModelItem& item);
		void NotifyAboutEnable(const ExtensionsModelItem& item);
		void NotifyAboutDisable(const ExtensionsModelItem& item);
		void NotifyAboutUpdateAvailable(const ExtensionsModelItem& item);
		void NotifyAboutExtendedNameUpdate(const ExtensionsModelItem& item);

	private:
		OP_STATUS DisableAllExtensions();
		OP_STATUS DestroyDeletedExtensions();
};

#endif // EXTENSIONS_MODEL_H
