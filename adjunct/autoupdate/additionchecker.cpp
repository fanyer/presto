/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/autoupdate/additionchecker.h"
#include "adjunct/autoupdate/additioncheckerlistener.h"
#include "adjunct/autoupdate/autoupdateserverurl.h"
#include "adjunct/quick/managers/AutoUpdateManager.h"

 /**
  * The list of updatable resource types that can be resolved by the addition checker.
  * No other type of resource will be possible to resolve this way.
  */
UpdatableResource::UpdatableResourceType AdditionChecker::KNOWN_TYPES[] = 
	{
		UpdatableResource::RTPlugin,
		UpdatableResource::RTDictionary
	};

/**
 * The number of seconds that will pass between resolving a new addition has been requested and the actual HTTP request.
 * The lower this is, the faster the resolve will happen, the higher it is, the less requests will be made since more 
 * items will have the chance to get scheduled for a single request.
 * The plugin install manager might request a couple of mimetypes for a single Web page during the page load, and we'd 
 * rather send them out with a single HTTP request to the autoupdate server.
 */
const unsigned int AdditionChecker::RESOLVE_TIMER_KICK_NOW_PERIOD_SEC = 2;

/**
 * The number of seconds that will pass between a failed request to the autoupdate server and a retry attempt. Note that
 * requesting any new addition check during the retry time will cause the retry to happen sooner, with the new addition
 * resolve attempt.
 */
const unsigned int AdditionChecker::RESOLVE_TIMER_KICK_DELAYED_PERDIOD_SEC = 10;

/**
 * The maximum number of resolve attempts per item. If the item could not be resolved with this many requests to the autoupdate
 * server, it is broadcasted as failing to resolve.
 */
const unsigned int AdditionChecker::RESOLVE_MAX_RETRY_COUNT_PER_ITEM = 5;

AdditionChecker::AdditionChecker():
	m_resolve_timer(NULL),
	m_autoupdate_server_url(NULL),
	m_autoupdate_xml(NULL)
{
}

AdditionChecker::~AdditionChecker()
{
	OP_DELETE(m_autoupdate_server_url);
	OP_DELETE(m_autoupdate_xml);

	if (m_resolve_timer)
		m_resolve_timer->SetTimerListener(NULL);

	OP_DELETE(m_resolve_timer);

	// Delete all items that are still not resolved
	m_items.DeleteAll();

	// Stop and *delete* all downloaders that might happen to be still alive
	for (UINT32 i=0; i<m_xml_downloaders.GetCount(); i++)
	{
		StatusXMLDownloader* downloader = m_xml_downloaders.Get(i);
		OP_ASSERT(downloader);
		OpStatus::Ignore(downloader->StopRequest());
	}
	m_xml_downloaders.DeleteAll();
}

OP_STATUS AdditionChecker::Init()
{
	OpAutoPtr<OpTimer> resolve_timer_guard;
	OpAutoPtr<AutoUpdateXML> autoupdate_xml_guard;
	OpAutoPtr<AutoUpdateServerURL> autoupdate_server_url_guard;

	m_resolve_timer = OP_NEW(OpTimer, ());
	RETURN_OOM_IF_NULL(m_resolve_timer);
	resolve_timer_guard = m_resolve_timer;
	m_resolve_timer->SetTimerListener(this);

	m_autoupdate_xml = OP_NEW(AutoUpdateXML, ());
	RETURN_OOM_IF_NULL(m_autoupdate_xml);
	autoupdate_xml_guard = m_autoupdate_xml;
	RETURN_IF_ERROR(m_autoupdate_xml->Init());

	m_autoupdate_server_url = OP_NEW(AutoUpdateServerURL, ());
	RETURN_OOM_IF_NULL(m_autoupdate_server_url);
	autoupdate_server_url_guard = m_autoupdate_server_url;
	RETURN_IF_ERROR(m_autoupdate_server_url->Init());

	resolve_timer_guard.release();
	autoupdate_xml_guard.release();
	autoupdate_server_url_guard.release();

	return OpStatus::OK;
}

OP_STATUS AdditionChecker::AddListener(AdditionCheckerListener* listener)
{
	return m_listeners.Add(listener);
}

OP_STATUS AdditionChecker::RemoveListener(AdditionCheckerListener* listener)
{
	return m_listeners.Remove(listener);
}

OP_STATUS AdditionChecker::CheckForAddition(UpdatableResource::UpdatableResourceType type, const OpStringC& key)
{
	// Did you call Init()?
	OP_ASSERT(m_resolve_timer);

	// Only allow the known types. See AdditionChecker::KNOWN_TYPES[]
	if (!IsKnownType(type))
	{
		OP_ASSERT(!"Uknown resource type passed to addition checker!");
		return OpStatus::ERR;
	}

	// Only allow new items, don't cache same items twice.
	if (NULL == GetItem(type, key))
	{
		// Haven't seen this item before, save it among m_items as pending a resolve attempt.
		OpAutoPtr<AdditionCheckerItem> item(OP_NEW(AdditionCheckerItem, ()));
		RETURN_OOM_IF_NULL(item.get());
		RETURN_IF_ERROR(item->SetKey(key));
		item->SetType(type);
		RETURN_IF_ERROR(m_items.Add(item.get()));
		item.release();
	}

	/* No matter if the item was known before or not, reset the resolve timer in order to
	 * shorten the retry time in case we're in the retry period at the moment and still
	 * allow to cache more items before a request is made if this is a new item.
	 */ 
	KickResolveTimer(KickNow);

	return OpStatus::OK;
}

AdditionCheckerItem* AdditionChecker::GetItem(UpdatableResource::UpdatableResourceType type, const OpStringC& key)
{
	OP_ASSERT(key.HasContent());
	// Find an item with the given type and key, there should be only one, but we don't verify that here
	AdditionCheckerItem* item = NULL;
	for (UINT32 i=0; i<m_items.GetCount(); i++)
	{
		item = m_items.Get(i);
		OP_ASSERT(item);
		if (item->IsType(type) && item->IsKey(key))
			return item;
	}
	return NULL;
}

OP_STATUS AdditionChecker::GetItemsForDownloader(OpVector<AdditionCheckerItem>& items, StatusXMLDownloader* downloader)
{
	// Gather all items that are supposed to be resolved by the given StatusXMLDownloader.
	for (UINT32 i=0; i<m_items.GetCount(); i++)
	{
		AdditionCheckerItem* item = m_items.Get(i);
		OP_ASSERT(item);
		if (item->IsXMLDownloader(downloader))
			RETURN_IF_ERROR(items.Add(item));
	}
	return OpStatus::OK;
}

OP_STATUS AdditionChecker::GetItemsForDownloaderAndType(OpVector<AdditionCheckerItem>& items, StatusXMLDownloader* downloader, UpdatableResource::UpdatableResourceType type)
{
	// Gather all items that are supposed to be resolved by the given StatusXMLDownloader.
	for (UINT32 i=0; i<m_items.GetCount(); i++)
	{
		AdditionCheckerItem* item = m_items.Get(i);
		OP_ASSERT(item);
		if (item->IsXMLDownloader(downloader) && item->IsType(type))
			RETURN_IF_ERROR(items.Add(item));
	}
	return OpStatus::OK;

}

void AdditionChecker::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(m_resolve_timer);
	if (timer == m_resolve_timer)
	{
		if (OpStatus::IsError(OnResolveTimeOut()))
		{
			// Something went terribly wrong. Perhaps things will get better next time an addition check is requested,
			// however this should not happen.
			OP_ASSERT(!"Shouldn't happen");
		}
	}
	else
		OP_ASSERT(!"Unknown timer");
}

OP_STATUS AdditionChecker::OnResolveTimeOut()
{
	OP_ASSERT(m_resolve_timer);
	OP_ASSERT(m_autoupdate_xml);
	OP_ASSERT(m_autoupdate_server_url);
	// The resolve timer fired. We want to send a request to the autoupdate server asking about all additions that are still not resolved
	// at this moment.
	// Since the autoupdate server requires a different autoupdate check level for every different addition type and since we really don't
	// want to change that, we cannot mix different addition types with one request to the server.

	// Go through all the known types
	for (UINT32 i=0; i<GetKnownTypesCount(); i++)
	{
		// The type that we're currently processing
		UpdatableResource::UpdatableResourceType current_type = KNOWN_TYPES[i];
		OP_ASSERT(current_type);

		// Gather a list of all addition items of the current type that are still not resolved in this vector
		OpVector<AdditionCheckerItem> items;
		// Get a list of all items of the current type that have a NULL downloader, meaning that they are not 
		// waiting for a callback from a downloader.
		RETURN_IF_ERROR(GetItemsForDownloaderAndType(items, NULL, current_type));

		if (items.GetCount() > 0)
		{
			// There is only one AutoUpdateXML, so we need to prepare it for a new request
			m_autoupdate_xml->ClearRequestItems();

			for (UINT32 i=0; i<items.GetCount(); i++)
			{
				AdditionCheckerItem* item = items.Get(i);
				OP_ASSERT(item);
				// Add the item to the AutoUpdateXML, this is needed to generate the proper request XML with all the items
				// we want to resolve.
				RETURN_IF_ERROR(m_autoupdate_xml->AddRequestItem(item));
			}

			// Get the current autoupdate server address. It is possible to have multiple addresses in the Autoupdate Server
			// preference, these will be cycled through in case of error while getting the response XML.
			OpString autoupdate_server_url_string;
			RETURN_IF_ERROR(m_autoupdate_server_url->GetCurrentURL(autoupdate_server_url_string));

			// Create a new StatusXMLDownloader that will be responsible for resolving this particular list of items
			OpAutoPtr<StatusXMLDownloader> current_downloader(OP_NEW(StatusXMLDownloader, ()));
			RETURN_OOM_IF_NULL(current_downloader.get());
			// The StatusXMLDownloader class saves the response XML to the filesystem in order to parse it. This is needed to
			// be able to reparse the XML in case the browser has crashed during autoupdate check, since we want to try hard
			// to limit the request count made to the autoupdate server.
			// The check type for the downloader determines if the response XML is saved in the autoupdate_response.xml file
			// or a temporary file with a temporary name that will be lost right after the check is done.
			RETURN_IF_ERROR(current_downloader->Init(StatusXMLDownloader::CheckTypeOther, this));

			// Get the autoupdate XML request string basing on the items that we want to resolve
			OpString8 xml_string;
			RETURN_IF_ERROR(m_autoupdate_xml->GetRequestXML(xml_string));

			// Save the StatusXMLDownloader to be able to recoginze it later
			RETURN_IF_ERROR(m_xml_downloaders.Add(current_downloader.get()));

			// Start the XML request, we'll get a callback via the StatusXMLDownloaderListener interface once the request
			// is complete
			RETURN_IF_ERROR(current_downloader->StartXMLRequest(autoupdate_server_url_string, xml_string));

			// Since everything went fine, we want to set the StatusXMLDownloader pointer for all items that were requested
			// with the above call.
			for (UINT32 i=0; i<items.GetCount(); i++)
			{
				AdditionCheckerItem* item = items.Get(i);
				OP_ASSERT(item);
				item->SetXMLDownloader(current_downloader.get());
			}

			// Forget about the downloader now, we hold it in the m_xml_downloaders vector
			current_downloader.release();
		}
		else
		{
			// No items of this type found currently, we'll pick up the remaining items with the next pass.
		}
	}
	return OpStatus::OK;
}

void AdditionChecker::StatusXMLDownloaded(StatusXMLDownloader* downloader)
{
	OP_ASSERT(downloader);
	// Check if we have spawned the downloader that is calling us back
	OP_ASSERT(m_xml_downloaders.Find(downloader) != -1);

	// Get all the items that are currently waiting to be resolved by the downloader that is calling us back
	OpVector<AdditionCheckerItem> items;
	// We can't return an error from this method. The only reason for GetItemsForDownloader() to fail
	// seems to be an OOM. Let's try to resume operation with whatever items there are, we need to get
	// to deleting the downloader at the end of this method anyway.
	OpStatus::Ignore(GetItemsForDownloader(items, downloader));

	// The StatusXMLDownloader gives us a list of resources that it got in response to the HTTP request that was sent out.
	// First, we check all the items that match both among the items waiting to be resolved and those sent by the downloader.
	for (UINT32 i=0; i<items.GetCount(); i++)
	{
		AdditionCheckerItem* item = items.Get(i);
		OP_ASSERT(item);

		UpdatableResource* resource = downloader->GetFirstResource(); 

		// Go through all the resources that are currently left in the downloader
		while (resource)
		{
			UpdatableResource::UpdatableResourceType resource_type = resource->GetType();
			URAttr resource_key = resource->GetResourceKey();

			// You haven't implemented UpdatableResource::GetResourceKey() for your resource type if this assert triggers.
			OP_ASSERT(URA_LAST != resource_key);

			OpString key_value;

			// If this resource is our addition...
			if (OpStatus::IsSuccess(resource->GetAttrValue(resource_key, key_value)) && item->IsType(resource_type) && item->IsKey(key_value))
			{
				// Notify that it is resolved and forget about it
				NotifyAdditionResolved(resource_type, key_value, resource);
				RETURN_VOID_IF_ERROR(downloader->RemoveResource(resource));
				// Note that we have invalid items in the items vector now, but we don't go back over it anyway
				RETURN_VOID_IF_ERROR(m_items.Delete(item));
				break;
			}
			resource = downloader->GetNextResource();
		}
	}

	// Go through all the items that we should have gotten with this StatusXMLDownloader but we didn't
	items.Clear();
	// We can't return an error from this method. The only reason for GetItemsForDownloader() to fail
	// seems to be an OOM. Let's try to resume operation with whatever items there are, we need to get
	// to deleting the downloader at the end of this method anyway.
	OpStatus::Ignore(GetItemsForDownloader(items, downloader));

	// This shouldn't really happen since we should always get a response about an item if we ask about it
	// but the autoupdate server has its ways.
	for (UINT32 i=0; i<items.GetCount(); i++)
	{
		AdditionCheckerItem* item = items.Get(i);
		OP_ASSERT(item);
		OpString key;
		if (OpStatus::IsSuccess(item->GetKey(key)))
		{
			// We can only notify if we can know the key.
			NotifyAdditionResolveFailed(item->GetType(), key);
		}

		OpStatus::Ignore(m_items.Delete(item));
	}

	// Iterate over all the resources that the downloader sent us but that we have never asked for.
	UpdatableResource* resource = downloader->GetFirstResource(); 
	while (resource)
	{
		// If this is a setting then we'll handle it
		if (resource->GetType() == UpdatableResource::RTSetting)
		{
			UpdatableSetting* setting = static_cast<UpdatableSetting*>(resource);
			if (!(setting->CheckResource() && OpStatus::IsSuccess(setting->UpdateResource())))
				OP_ASSERT(!"Could not update setting resource!");
		}
		else
		{
			// But if it's anything else then we're really confused
			OP_ASSERT(!"Unexpected resource got from the autoupdate server!");
		}
		OpStatus::Ignore(downloader->RemoveResource(resource));
		resource = downloader->GetNextResource();
	}

	// Delete the downloader finally.
	OpStatus::Ignore(m_xml_downloaders.Delete(downloader));
}

void AdditionChecker::StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus status)
{
	OP_ASSERT(downloader);
	// Check if we know the downloader
	OP_ASSERT(m_xml_downloaders.Find(downloader) != -1);

	// Try to increment the server URL, i.e. use a different one next time. This will most likely fail since we use one server anyway.
	OpStatus::Ignore(m_autoupdate_server_url->IncrementURLNo(AutoUpdateServerURL::Wrap));

	// Get the list of all the items that should be resolved by this downloader
	OpVector<AdditionCheckerItem> items;
	// We can't return an error from this method. The only reason for GetItemsForDownloader() to fail
	// seems to be an OOM. Let's try to resume operation with whatever items there are, we need to get
	// to deleting the downloader at the end of this method anyway.
	OpStatus::Ignore(GetItemsForDownloader(items, downloader));

	for (UINT32 i=0; i<items.GetCount(); i++)
	{
		AdditionCheckerItem* item = items.Get(i);
		OP_ASSERT(item);
		// Increment the retry timer
		item->IncRetryCounter();
		// Set the downloader to NULL in order to pick this item up with the next resolve attempt.
		item->SetXMLDownloader(NULL);

		// However in case the item has reached the maximum retry count, notify it has failed to be resolved and drop it without a further retry
		if (item->GetRetryCounterValue() >= RESOLVE_MAX_RETRY_COUNT_PER_ITEM)
		{
			OpString item_key;
			UpdatableResource::UpdatableResourceType item_type = item->GetType();
			if (OpStatus::IsSuccess(item->GetKey(item_key)))
				NotifyAdditionResolveFailed(item_type, item_key);

			OpStatus::Ignore(m_items.Delete(item));
		}
	}

	// If there still are some items that should be retried, schedule a retry attempt
	if (items.GetCount() > 0)
		KickResolveTimer(KickDelayed);

	OpStatus::Ignore(m_xml_downloaders.Delete(downloader));
}

void AdditionChecker::NotifyAdditionResolved(UpdatableResource::UpdatableResourceType type, const OpStringC& key, UpdatableResource* resource)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnAdditionResolved(type, key, resource);
}

void AdditionChecker::NotifyAdditionResolveFailed(UpdatableResource::UpdatableResourceType type, const OpStringC& key)
{
	for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
		m_listeners.GetNext(iterator)->OnAdditionResolveFailed(type, key);
}

UINT32 AdditionChecker::GetKnownTypesCount()
{
	size_t item_size = sizeof(UpdatableResource::UpdatableResourceType);
	size_t full_size = sizeof(KNOWN_TYPES);
	return full_size / item_size;
}

bool AdditionChecker::IsKnownType(UpdatableResource::UpdatableResourceType type)
{
	UINT32 count = GetKnownTypesCount();
	for (UINT32 i=0; i<count; i++)
	{
		if (KNOWN_TYPES[i] == type)
			return true;
	}
	return false;
}

void AdditionChecker::KickResolveTimer(KickType type)
{
	OP_ASSERT(m_resolve_timer);
	if (KickNow == type)
		m_resolve_timer->Start(RESOLVE_TIMER_KICK_NOW_PERIOD_SEC * 1000);
	else
		m_resolve_timer->Start(RESOLVE_TIMER_KICK_DELAYED_PERDIOD_SEC * 1000);
}

/***********************
 * AdditionCheckerItem *
 ***********************/

AdditionCheckerItem::AdditionCheckerItem():
	m_status_xml_downloader(NULL),
	m_type(UpdatableResource::RTEmpty),
	m_retry_counter(0)
{
}

AdditionCheckerItem::~AdditionCheckerItem()
{
}

OP_STATUS AdditionCheckerItem::SetKey(const OpStringC& key)
{
	return m_key.Set(key);
}

void AdditionCheckerItem::SetType(UpdatableResource::UpdatableResourceType type)
{
	m_type = type;
}

OP_STATUS AdditionCheckerItem::GetKey(OpString& key)
{
	return key.Set(m_key);
}

UpdatableResource::UpdatableResourceType AdditionCheckerItem::GetType()
{
	return m_type;
}

bool AdditionCheckerItem::IsType(UpdatableResource::UpdatableResourceType type)
{
	return m_type == type;
}

bool AdditionCheckerItem::IsKey(const OpStringC& key)
{
	return m_key.CompareI(key) == 0;
}

bool AdditionCheckerItem::IsXMLDownloader(const StatusXMLDownloader* downloader)
{
	return m_status_xml_downloader == downloader;
}

void AdditionCheckerItem::SetXMLDownloader(const StatusXMLDownloader* downloader)
{
	m_status_xml_downloader = downloader;
}

const StatusXMLDownloader* AdditionCheckerItem::GetXMLDownloader()
{
	return m_status_xml_downloader;
}

#endif // AUTO_UPDATE_SUPPORT
