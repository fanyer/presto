/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Michal Zajaczkowski
 */

#ifdef AUTO_UPDATE_SUPPORT

#ifndef AUTOUPDATE_ADDITION_CHECKER_H
#define AUTOUPDATE_ADDITION_CHECKER_H

#include "modules/util/adt/oplisteners.h"
#include "modules/hardcore/timer/optimer.h"
#include "adjunct/autoupdate/autoupdatexml.h"
#include "adjunct/autoupdate/autoupdateserverurl.h"
#include "adjunct/autoupdate/statusxmldownloader.h"

class AutoUpdateXML;
class AutoUpdateManager;
class AutoUpdateServerURL;
class AdditionCheckerListener;

/**
 * AdditionCheckerItem represents a single addition item that is being resolved by the addition checker.
 * Each item has a type (i.e. UpdatableResource::ResourceTypePlugin), a key (i.e. the mimetype of the plugin) and a 
 * StatusXMLDownloader* value. 
 * If the item is not waiting to be resolved by any StatusXMLDownloader, then it has the downloader value set to NULL.
 * Otherwise the downloader value points to the StatusXMLDownloader object that should send back information about this
 * particular item.
 */
class AdditionCheckerItem
{
public:
	AdditionCheckerItem();
	~AdditionCheckerItem();

	/**
	 * Sets the value of the key of the addition item (i.e. the plugin mime-type)
	 *
	 * @param key - the new value of the key
	 *
	 * @returns - OpStatus:OK if setting the key value succeeds, ERR otherwise
	 */
	OP_STATUS SetKey(const OpStringC& key);

	/**
	 * Sets the type of the addition item (i.e. UpdatableResource::ResourceTypePlugin)
	 *
	 * @param type - the new type to set
	 *
	 * @returns nothing
	 */
	void SetType(UpdatableResource::UpdatableResourceType type);

	/**
	 * Assings a StatusXMLDownloader to the addition item, designating that item is pending an autoupdate response from the
	 * particular downloader. If this value is NULL, then the item is pending to be resolved.
	 *
	 * @param downloader - the new StatusXMLDownloader* value
	 */
	void SetXMLDownloader(const StatusXMLDownloader* downloader);

	/**
	 * Get the key of the item (i.e. the mime-type of a plugin)
	 *
	 * @param key - a reference to an OpString that will get the key value in case of success
	 *
	 * @returns - OpStatus::OK in case the value was set OK, ERR otherwise
	 */
	OP_STATUS GetKey(OpString& key);

	/**
	 * Get the type of the item, i.e. UpdatableResource::ResourceTypePlugin
	 *
	 * @returns - the type of the item
	 */
	UpdatableResource::UpdatableResourceType GetType();

	/**
	 * Get the StatusXMLDownloader* value assinged to the item, see SetXMLDownloader()
	 *
	 * @returns - the StatusXMLDownloader* value
	 */
	const StatusXMLDownloader* GetXMLDownloader();

	/**
	 * Check if this item is of the given type
	 *
	 * @param type - the type that we want to verify against the type of this item
	 *
	 * @returns - TRUE in case the type of this item matches the given type, FALSE otherwise
	 */
	bool IsType(UpdatableResource::UpdatableResourceType type);

	/**
	 * Check if this item has the given key value
	 *
	 * @param key - the key value that we want to check against the key of this item
	 *
	 * @returns TRUE in case the keys match, FALSE otherwise
	 */
	bool IsKey(const OpStringC& key);

	/**
	 * Check if the value of the StatusXMLDownloader pointer assigned to this item matches the given value
	 *
	 * @param downloader - the StatusXMLDownloader pointer value that is to be checked against the value stored with this item
	 *
	 * @returns - TRUE in case the pointers match, FALSE otherwise
	 */
	bool IsXMLDownloader(const StatusXMLDownloader* downloader);

	/**
	 * Increment the resolve retry counter for this item. This method does not check if the pointer exceeded the maximum retry count.
	 */ 
	void	IncRetryCounter() { m_retry_counter++; }

	/**
	 * Get the retry counter value to compare against the maximum retry count in the calling code.
	 *
	 * @returns - the current retry count for the given item
	 */
	unsigned int GetRetryCounterValue() { return m_retry_counter; }

private:
	/**
	 * The value of the key for this item. The key is an unique identifier among all items of the same type, i.e. a mime-type for a plugin.
	 */
	OpString m_key;

	/**
	 * The value of the StatusXMLDownloader pointer assigned to this item. This is NULL in case this item is pending resolving and it points
	 * to a downloader that should resolve this item in case the item has been requested to resolve.
	 */
	const StatusXMLDownloader* m_status_xml_downloader;

	/**
	 * Type of the addition represented by this item, i.e. UpdatableResource::ResourceTypePlugin.
	 */
	UpdatableResource::UpdatableResourceType m_type;

	/**
	 * The resolve retry counter that counts how many times a request about the given item has been sent to the autoupdate server.
	 * Used to have a resolve retry mechanism that will give up after RESOLVE_MAX_RETRY_COUNT_PER_ITEM attempts.
	 */
	unsigned int m_retry_counter;
};

/**
 * The autoupdate server currently serves two quite different purposes:
 * 1. Keeping the browser up-to-date with periodic automatic checks and manual checks;
 * 2. Making it possible to discover and download "additions" via the autoupdate server.
 *
 * An addition is any additional installable resource that might be needed during the browser run, currently
 * the only supported addition types are plugins and dictionaries, both installable on user demand. If the user
 * visits a page that requires a given plugin, the plugin availablity is checked against the autoupdate server with
 * the additions mechanisms.
 *
 * If you want to add a new type of resource that you want to resolve via the autoupdate server, you need to:
 * 1. Add a new resource type (see updatableresource.h, updatablefile.h);
 * 2. Add the new resource to the KNOWN_TYPES[] array;
 * 3. Agree with the core services team that the autoupdate server will support the new resource, agree a new update level for the new resource type! See 
 *    AutoUpdateXML::AutoUpdateLevel.
 * 4. Make sure the UpdatableResourceFactory is able to recognize all the XML elements used to describe the new resource
 *    type;
 * 5. Choose a key for the new resource. This is the value that is supposed to be unique across all resources of the same type, 
 *    i.e. the mimetype for a plugin or the language for a dictionary, make sure you implement the key by implementing the 
 *    UpdatableResource::GetResourceKey() method.
 * 6. Make sure AutoUpdateXML can recognize the new resource type, choose a new update check level and generate a proper XML for the new item type.
 * 7. You should get asserts if you forget about any of the above;
 *
 * In order to resolve a resource:
 * 1. Make your class inherit from the AdditionCheckerListener class, implement its methods;
 * 2. Register yourself as a listener with something like g_autoupdate_manager->AddListener(this);
 * 3. Call g_autoupdate_manager->CheckForAddition() to check the availability of the addition and wait for a callback via the
 *    AdditionCheckerListener interface;
 * 4. Carry on.
 *
 * The addition checker will try to repeat failed requests to the autoupdate server on its own, so you can consider that when it
 * fails to resolve the addition, it fails for good.
 * The addition checker may make a couple of HTTP requests to the autoupdate server in parallel in order to speed up things.
 * The addition checker will send a cummulative requst about all additions of a given type that are not resolved at the moment
 * of seding the request. 
 *
 * The AdditionChecker class is meant to be used by the AutoUpdateManager only. The AutoUpdateManager has all the interfaces
 * required to use the addition resolving functionality.
 *
 */
class AdditionChecker:
	public OpTimerListener,
	public StatusXMLDownloaderListener
{
	friend class AutoUpdateManager;
public:
	AdditionChecker();
	virtual ~AdditionChecker();

protected:
	/**
	 * The list of types that the AdditionChecker is be able to resolve.
	 */
	static UpdatableResource::UpdatableResourceType KNOWN_TYPES[];
	/**
	 * The resolve timer timeout period for the KickNow kick type.
	 */
	static const unsigned int RESOLVE_TIMER_KICK_NOW_PERIOD_SEC;
	/**
	 * The resolve timer timeout period for the KickDelayed kick type.
	 */
	static const unsigned int RESOLVE_TIMER_KICK_DELAYED_PERDIOD_SEC;
	/**
	 * The maximum retry count per a queued addition item, if the addition is not resolved after this many tries, it's 
	 * broadcasted as failed.
	 */
	static const unsigned int RESOLVE_MAX_RETRY_COUNT_PER_ITEM;

	enum KickType
	{
		/**
		 * Start the resolve timer using the RESOLVE_TIMER_KICK_DELAYED_PERDIOD_SEC period
		 */
		KickDelayed,
		/**
		 * Start the resolve timer using the RESOLVE_TIMER_KICK_NOW_PERIOD_SEC period
		 */
		KickNow
	};

	/**
	 * Initializes the object, needs to be called once before any operations are made on the AdditionChecker.
	 *
	 * @returns - OpStatus::OK if initialization went OK, ERR otherwise. The object can't be used until Init() returns OpStatus::OK.
	 */
	OP_STATUS Init();

	/**
	 * OpTimerListener implementation.
	 */
	virtual void OnTimeOut(OpTimer* timer);

	/**
	 * Handler for resolve timer. Will send out autoupdate XML requests for items awaiting resolving.
	 *
	 * @returns - OpStatus::OK on success, ERR otherwise.
	 */
	OP_STATUS OnResolveTimeOut();

	/**
	 * Implementation of the StatusXMLDownloaderListener interface.
	 */
	virtual void StatusXMLDownloaded(StatusXMLDownloader* downloader);
	virtual void StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus status);

	/**
	 * Register a listener with the addition resolver. You don't want to do that with your classes, use g_autoupdate_manager->AddListener().
	 *
	 * @param listener - a pointer to a class inheriting from the AdditionCheckerListener interface.
	 *
	 * @returns - OpStatus::OK if adding the listener succeeded, OpStatus::ERR otherwise.
	 */
	OP_STATUS AddListener(AdditionCheckerListener* listener);

	/**
	 * Unregister a listener with the addition resolver. You don't want to do that with your classes, use g_autoupdate_manager->RemoveListener().
	 *
	 * @param listener - a pointer to a class inheriting from the AdditionCheckerListener interface.
	 *
	 * @returns - OpStatus::OK if removing the listener succeeded, OpStatus::ERR otherwise.
	 */
	OP_STATUS RemoveListener(AdditionCheckerListener* listener);

	/**
	 * Try to resolve an addition of the given type and the given key against the autoupdate server. You don't want to use this, use 
	 * g_autoupdate_manager->CheckForAddition() instead.
	 * Before you call this method, be sure to register as a listener to AdditionChecker.
	 * After a call to this method, wait for a callback via the AdditionCheckerListener interface.
	 * Note that the actual XML request will be made later on, after the resolve timer fires.
	 *
	 * @param type - the type of the addition to be resolved.
	 * @param key - the key of the addition to be resolved.
	 *
	 * @returns - OpStatus::OK if queing the addition check was OK, ERR otherwise.
	 */
	OP_STATUS CheckForAddition(UpdatableResource::UpdatableResourceType type, const OpStringC& key);

	/**
	 * Starts the resolve timer with the timeout period determined by the parameter given.
	 *
	 * @param type - determines the timeout period, see KickType definition.
	 */
	void KickResolveTimer(KickType type);

	/**
	 * Notifies all the registered listeners about an addition being resolved or failed to resolve.
	 */
	void NotifyAdditionResolved(UpdatableResource::UpdatableResourceType type, const OpStringC& key, UpdatableResource* resource);
	void NotifyAdditionResolveFailed(UpdatableResource::UpdatableResourceType type, const OpStringC& key);

	/**
	 * Checks if a resource with the given type can be resolved as an addition.
	 */
	bool IsKnownType(UpdatableResource::UpdatableResourceType type);

	/**
	 * Get the total number of the types recognized as additions. Returns the number of items in the KNOWN_TYPES[] array.
	 */
	UINT32 GetKnownTypesCount();

	/**
	 * Searches the m_items vector for an item with the given type and key.
	 * The m_items vector should never contain more than one item with the same type AND key, i.e. we should never have queued
	 * more than one plugin with the same mime-type.
	 * This method doesn't check that condition, it relies on CheckForAddition() making the proper checks.
	 *
	 * @param type - addition type
	 * @param key - addition key
	 *
	 * @returns - a pointer to the addition item found, if one, NULL otherwise.
	 */
	AdditionCheckerItem* GetItem(UpdatableResource::UpdatableResourceType type, const OpStringC& key);

	/**
	 * Searches the m_items vector for all items that have the given StatusXMLDownloader assigned to them.
	 *
	 * @param items - a reference to a vector that will contain the items found.
	 * @param downloader - a pointer to the StatusXMLDownloader that we're looking for, may be NULL.
	 *
	 * @returns - OpStatus::OK if everything went fine, ERR otherwise.
	 */
	OP_STATUS GetItemsForDownloader(OpVector<AdditionCheckerItem>& items, StatusXMLDownloader* downloader);

	/**
	 * Searches the m_items vector for all items that have the given StatusXMLDownloader assigned to them AND that are of the given type.
	 *
	 * @param items - a reference to a vector that will contain the items found.
	 * @param downloader - a pointer to the StatusXMLDownloader that we're looking for, may be NULL.
	 * @param type - the type of the items that we care about.
	 *
	 * @returns - OpStatus::OK if everything went fine, ERR otherwise.
	 */
	OP_STATUS GetItemsForDownloaderAndType(OpVector<AdditionCheckerItem>& items, StatusXMLDownloader* downloader, UpdatableResource::UpdatableResourceType type);

	/**
	 * Hold a list of StatusXMLDownloader objects that were created when making the XML requests.
	 * A downloader is destroyed and removed after a callback is returned from it via the StatusXMLDownloaderListener interface.
	 * Additionally, if any downloaders still exist while the AdditionChecker is destroyed, they are stopped and destroyed as well.
	 */
	OpVector<StatusXMLDownloader> m_xml_downloaders;

	/**
	 * The resolve timer.
	 */
	OpTimer* m_resolve_timer;

	/**
	 * The list of registered listeners.
	 */
	OpListeners<AdditionCheckerListener> m_listeners;

	/**
	 * The list of addition checker items that are awaiting resolving.
	 */
	OpVector<AdditionCheckerItem> m_items;

	AutoUpdateServerURL* m_autoupdate_server_url;
	AutoUpdateXML* m_autoupdate_xml;
};

#endif // AUTOUPDATE_ADDITION_CHECKER_H
#endif // AUTO_UPDATE_SUPPORT
