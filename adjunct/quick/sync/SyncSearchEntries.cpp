/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Adam Minchinton (adamm)
 *
 */

#include "core/pch.h"

#ifdef SUPPORT_DATA_SYNC
#ifdef SUPPORT_SYNC_SEARCHES

#include "adjunct/quick/sync/SyncSearchEntries.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "adjunct/quick/Application.h"
#include "modules/formats/base64_decode.h"
#include "modules/img/imagedump.h"

//#define SYNC_CLIENT_DEBUG_SEARCH


/**
 * Implementation of syncing of search engine items.
 *
 * SyncSearchEntries listens to the g_searchEngineManager for adds, deletes
 * and changes to search engine items. g_searchEngineManager holds a
 * FilteredVector of all searches, both default and user added ones.
 *
 * We synchronize user added searches and changes to default searches.
 *
 *
 */

/*************************************************************************
 **
 ** SyncSearchEntries
 **
 **
 **************************************************************************/

SyncSearchEntries::SyncSearchEntries() :
	m_is_receiving_items(FALSE)
{
}

SyncSearchEntries::~SyncSearchEntries()
{
	if (g_searchEngineManager)
		g_searchEngineManager->RemoveSearchEngineListener(this);
}


#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
OP_STATUS SyncSearchEntries::SyncDataInitialize(OpSyncDataItem::DataItemType type)
#else
void SyncSearchEntries::OnSyncDataInitialize(OpSyncDataItem::DataItemType type)
#endif
{
	// AJMTODO: Backup file
	if (type == OpSyncDataItem::DATAITEM_SEARCH)
	{
		OP_ASSERT(g_sync_coordinator->GetSupports(	SYNC_SUPPORTS_SEARCHES ));
		if (!g_sync_coordinator->GetSupports(	SYNC_SUPPORTS_SEARCHES))
#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
			return OpStatus::ERR;
#else
			return;
#endif

		OpFile search_file;
		search_file.Construct(UNI_L("search.ini"), OPFILE_HOME_FOLDER);
		g_sync_manager->BackupFile(search_file);
	}
	else
	{
		OP_ASSERT(!"SyncSearchEntries: Got OnSyncDataInitialize notification on wrong type");
	}
#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
	return OpStatus::OK;
#endif
}

// Process incoming items
#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
OP_STATUS SyncSearchEntries::SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error)
#else
void SyncSearchEntries::OnSyncDataAvailable(OpSyncCollection *new_items)
#endif
{
	OP_STATUS status = OpStatus::OK;

	if(new_items && new_items->First())
	{
		OpSyncItemIterator iter(new_items->First());

		do
		{
			OpSyncItem *current = iter.GetDataItem();

			OP_ASSERT(current);
			if(current)
			{
				status = ProcessSyncItem(current);
			  	if (OpStatus::IsError(status))
   				{
 					break; // Don't process more items
 				}
				OP_ASSERT(!m_is_receiving_items);

			}
 		} while (iter.Next());
	}

	// if (do_dirty_sync) SendAllItems(TRUE);
#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
	return status;
#endif
}


#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
OP_STATUS SyncSearchEntries::SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty)
#else
void SyncSearchEntries::OnSyncDataFlush(OpSyncDataItem::DataItemType type)
#endif
{
	if (type == OpSyncDataItem::DATAITEM_SEARCH)
	{
		OP_ASSERT(g_sync_coordinator->GetSupports(	SYNC_SUPPORTS_SEARCHES ));
		if (!g_sync_coordinator->GetSupports(	SYNC_SUPPORTS_SEARCHES))
#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
			return OpStatus::ERR;
#else
			return;
#endif

#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
			SendAllItems(is_dirty);
#else
			SendAllItems(FALSE); 
#endif
	}
	else
	{
		OP_ASSERT(!"SyncSearchEntries: Got OnSyncDataFlush notification on wrong type");
	}
#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
	return OpStatus::OK;
#endif
}

#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
OP_STATUS SyncSearchEntries::SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support)
#else
void SyncSearchEntries::OnSyncDataSupportsChanged(OpSyncDataItem::DataItemType type, BOOL has_support)
#endif
{
	if (type == OpSyncDataItem::DATAITEM_SEARCH)
	{
		EnableSearchesListening(has_support);
	}
	else
	{
		OP_ASSERT(FALSE);
	}
#ifdef SY_CAP_SYNCDATALISTENER_CHANGED
	return OpStatus::OK;
#endif
}



/***************************************************************************
 *
 * SearchItemChanged
 *
 * @param SearchTemplate* item - item that changed, and should be synced
 * @param status - status of item: ADDED, MODIFIED or DELETED
 * @param dirty  - CommitItem on the OpSyncItem should specify
 *                 whether this is a dirty sync or not.
 * @param changed_flag - flag to specify which field changed on the item
 *
 ***************************************************************************/

OP_STATUS SyncSearchEntries::SearchItemChanged(SearchTemplate* item,
											   OpSyncDataItem::DataItemStatus status,
											   BOOL dirty,
											   UINT32 changed_flag)
{
	// Do not sync changes that happens on items received from the server,
	if (m_is_receiving_items)
	{
		return OpStatus::OK;
	}

	if ( !item )
		return OpStatus::ERR_NULL_POINTER;

	if (!g_sync_manager->SupportsType(SyncManager::SYNC_SEARCHES))
		return OpStatus::OK;


	OpSyncItem *sync_item = NULL;
	OP_STATUS sync_status = OpStatus::OK;
	OpString data;
	
#ifdef SYNC_CLIENT_DEBUG_SEARCH
	if (item->GetName().HasContent())
	{
		OpString8 name;
		name.Set(item->GetName().CStr());
		OpString8 guid;
		guid.Set(item->GetUniqueGUID().CStr());
		fprintf( stderr,"\n [Link] SearchEngineItemChanged(%s[%s], status=%d, dirty=%d, flag=%x)\n", name.CStr(), guid.CStr(), status, dirty, changed_flag);
	}
#endif
	
	// Generate the unique id if it's not there
	if (!item->GetUniqueGUID().HasContent())
	{
		//OP_ASSERT(!"Item added to sync does not have a unique GUID ! \n");
		OpString guid;
		
		// generate a default unique ID
		if(OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
		{
			item->SetUniqueGUID(guid.CStr());
			g_searchEngineManager->Write();
		}
	}

	SYNC_RETURN_IF_ERROR(SearchTemplateToOpSyncItem(item, sync_item, status, changed_flag), sync_item);
	SYNC_RETURN_IF_ERROR(sync_item->CommitItem(dirty, !dirty), sync_item);

	if(sync_item)
	{
		g_sync_coordinator->ReleaseSyncItem(sync_item);
	}
	
	return sync_status;
}

void SyncSearchEntries::OnSearchEngineItemAdded(SearchTemplate* item)
{
#ifdef SYNC_CLIENT_DEBUG_SEARCH
	fprintf( stderr, " SyncSearchEntries::OnSearchEngineItemAdded \n");
#endif

	SearchItemChanged(item,
					  OpSyncDataItem::DATAITEM_ACTION_ADDED,
					  FALSE, SearchEngineManager::FLAG_UNKNOWN);
}

void SyncSearchEntries::OnSearchEngineItemChanged(SearchTemplate* item, UINT32 flag)
{
#ifdef SYNC_CLIENT_DEBUG_SEARCH
	fprintf( stderr, " SyncSearchEntries::OnSearchEngineItemChanged\n");
#endif

	SearchItemChanged(item,
					  OpSyncDataItem::DATAITEM_ACTION_MODIFIED,
					  FALSE, flag);

}

void SyncSearchEntries::OnSearchEngineItemRemoved(SearchTemplate* item)
{
#ifdef SYNC_CLIENT_DEBUG_SEARCH
	fprintf( stderr, " SyncSearchEntries::OnSearchEngineItemRemoving\n");
#endif

	SearchItemChanged(item,
					  OpSyncDataItem::DATAITEM_ACTION_DELETED,
					  FALSE,
					  SearchEngineManager::FLAG_UNKNOWN);
}

/**********************************************************************
 *
 * EnableSearchesListening
 * 
 * @param enable - If TRUE, start listening to model, else stop listening
 * 
 **********************************************************************/
void SyncSearchEntries::EnableSearchesListening(BOOL enable)
{
	if (enable)
	{
		g_searchEngineManager->AddSearchEngineListener(this);
	}
	else
	{
		g_searchEngineManager->RemoveSearchEngineListener(this);
	}
}

/******************************************************************************
 *
 * SearchTemplateToOpSyncItem
 *
 * @param SearchTemplate* search
 * @param OpSyncItem sync_item -
 * @param DataItemStatus status - MODIFIED, DELETED or ADDED
 *
 *
 *
 ******************************************************************************/

OP_STATUS SyncSearchEntries::SearchTemplateToOpSyncItem(SearchTemplate* item, 
														OpSyncItem*& sync_item,
														OpSyncDataItem::DataItemStatus status,
														UINT32 changed_flag)
{
	if (!item)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS sync_status = OpStatus::OK;

	OpString data;
	RETURN_IF_ERROR(data.Set(item->GetUniqueGUID()));
	OP_ASSERT(item->GetUniqueGUID().HasContent());

	OpSyncDataItem::DataItemType type = OpSyncDataItem::DATAITEM_SEARCH;

	// Must have a GUID
	if (!data.HasContent())
		return OpStatus::ERR;
	
	sync_status = g_sync_coordinator->GetSyncItem(&sync_item, type, OpSyncItem::SYNC_KEY_ID, data.CStr());

	if(OpStatus::IsSuccess(sync_status))
	{
		sync_item->SetStatus(status);

		DEBUG_LINK(UNI_L("SearchItem: id '%s', status: %d, name: '%s' outgoing\n"), 
				   item->GetUniqueGUID().CStr(), status, item->GetName().CStr());

		if (status == OpSyncDataItem::DATAITEM_ACTION_DELETED)
		{
			OP_ASSERT(!item->IsFromPackageOrModified());
		}

		// Don't set fields if item is deleted (except for testing)
		if (status != OpSyncDataItem::DATAITEM_ACTION_DELETED)
		{
			if (item->IsFromPackageOrModified())
			{
				SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_GROUP, 
														UNI_L("desktop_default")), sync_item);
			}
			else
			{
				SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_GROUP, 
														UNI_L("custom")), sync_item);
			}
		

			if ( changed_flag & SearchEngineManager::FLAG_DELETED)
			{
				INT32 hidden = item->IsFiltered() ? 1 : 0;
				OpString hidden_value;
				if (OpStatus::IsSuccess(hidden_value.AppendFormat(UNI_L("%d"), hidden)))
				{
					SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_HIDDEN, hidden_value.CStr()), sync_item);
				}		
			}	

			// Type
			SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_TYPE, item->GetTypeString()), sync_item);

				
			if ( changed_flag & SearchEngineManager::FLAG_ISPOST)
			{
				OpString post;
				if (OpStatus::IsSuccess(post.AppendFormat(UNI_L("%d"), (item->GetIsPost() ? 1 : 0 ))))
				{   
					SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_IS_POST, post.CStr()), sync_item);
				}
			}

			// Pbar
			if (g_sync_manager->SupportsType(SyncManager::SYNC_PERSONALBAR))
			{
				if ( changed_flag & SearchEngineManager::FLAG_PERSONALBAR)
				{
					OpString pos;
					if (OpStatus::IsSuccess(pos.AppendFormat(UNI_L("%d"),item->GetPersonalbarPos())))
					{                                                              
						SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_PERSONAL_BAR_POS, pos.CStr()), sync_item);
					}
		
					OpString bar;
					if (OpStatus::IsSuccess(bar.AppendFormat(UNI_L("%d"), item->GetOnPersonalbar() ? 1 : 0)))
					{
						SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_SHOW_IN_PERSONAL_BAR,
																bar.CStr()), sync_item);
					}
				}
			}

			if ( changed_flag & SearchEngineManager::FLAG_NAME)
			{
				OpString engine_name;
				SYNC_RETURN_IF_ERROR(item->GetEngineName(engine_name), sync_item);
				SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_TITLE, engine_name), sync_item);
			}

			if ( changed_flag & SearchEngineManager::FLAG_URL)
				SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_URI, item->GetUrl().CStr()), sync_item);
			if ( changed_flag & SearchEngineManager::FLAG_KEY )
				SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_KEY, item->GetKey().CStr()), sync_item);
			if ( changed_flag & SearchEngineManager::FLAG_ENCODING)
				 SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_ENCODING, item->GetEncoding().CStr()), sync_item);
			if ( changed_flag & SearchEngineManager::FLAG_QUERY )
				SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_POST_QUERY, item->GetQuery().CStr()), sync_item);
				 
				 
			if ( changed_flag & SearchEngineManager::FLAG_ICON )
			{
				if (!item->GetIcon().IsEmpty())
				{
					OpBitmap *bitmap = item->GetIcon().GetBitmap(NULL);
					if(bitmap)
					{
						TempBuffer base64;
						SYNC_RETURN_IF_ERROR(GetOpBitmapAsBase64PNG(bitmap, &base64), sync_item);
						SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_ICON, base64.GetStorage()), sync_item);

						item->GetIcon().ReleaseBitmap();
					}
				}
				else
				{
					SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_ICON, UNI_L("")), sync_item);
				}
			}
			
		} 
	}
	
	return sync_status;
}


/************************************************************************
 *
 * OpSyncItemToSearchTemplate
 *
 *
 ***********************************************************************/

OP_STATUS SyncSearchEntries::OpSyncItemToSearchTemplate(OpSyncItem* item, 
														SearchTemplate*& search)
{
	if (!item || !search)
		return OpStatus::ERR_NULL_POINTER;

	OpString data;
	
	OP_STATUS status = item->GetData(OpSyncItem::SYNC_KEY_TITLE, data);
	RETURN_IF_ERROR(search->SetName(data));
	
	status = item->GetData(OpSyncItem::SYNC_KEY_URI, data);
	RETURN_IF_ERROR(search->SetURL(data));
	
	// TODO: How to handle duplicate keys
	status = item->GetData(OpSyncItem::SYNC_KEY_KEY, data);
	// Check for if there is already one or more items with this key
	// if (!g_searchEngineManager->GetSearchEngineByKey(data))
	{
		RETURN_IF_ERROR(search->SetKey(data));
	}
	
	status = item->GetData(OpSyncItem::SYNC_KEY_ENCODING, data);
	RETURN_IF_ERROR(search->SetEncoding(data));
	
	status = item->GetData(OpSyncItem::SYNC_KEY_POST_QUERY, data);
	RETURN_IF_ERROR(search->SetQuery(data));
	
	// type?
	// status = item->GetData(OpSyncItem::SYNC_KEY_TYPE, data);
	// search->SetSearchType(search_type);
	
	status = item->GetData(OpSyncItem::SYNC_KEY_IS_POST, data);
	if (data.HasContent())
	{
		BOOL is_post = uni_atoi(data.CStr());
		search->SetIsPost(is_post);
	}
	
	if (g_sync_manager->SupportsType(SyncManager::SYNC_PERSONALBAR))
	{
		// Ignore SHOW_IN_PERSONALBAR
		INT32 pos;
		
		status = item->GetData(OpSyncItem::SYNC_KEY_PERSONAL_BAR_POS, data);
		if(data.HasContent())
		{
			pos = uni_atoi(data.CStr());
			if (pos >= 0)
			{
				search->SetPersonalbarPos(pos);
			}
		}
	}
	
	// Just ignore the group attribute
	// status = item->GetData(OpSyncItem::SYNC_KEY_GROUP, data);
	
	// TODO: Same as bookmarks -> move
	OpString8 icon_data;
	status = item->GetData(OpSyncItem::SYNC_KEY_ICON, data);
	if (OpStatus::IsSuccess(status))
	{
		OpStatus::Ignore(icon_data.Set(data.CStr()));
	}
	if(icon_data.HasContent() && search->GetUrl().HasContent())
	{
		unsigned long pos = 0;
		BOOL warn = FALSE;
		const unsigned char* src = (const unsigned char*)icon_data.CStr();
		unsigned char* content;
		unsigned long calc_len = ((icon_data.Length() + 3) / 4) * 3;
		unsigned long len;
		
		content = OP_NEWA(unsigned char, calc_len);
		if(content == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		len = GeneralDecodeBase64(src, icon_data.Length(), pos, content, warn);
		
		OP_ASSERT(len <= calc_len);
		g_favicon_manager->Add(search->GetUrl().CStr(), content, len);
		OP_DELETEA(content);
	}

	return OpStatus::OK;
}



/************************************************************************
 *
 * ProcessSyncItem
 *
 * Process a Search Item from the server, 
 * by adding a new item, or deleting or modifying an existing item.
 *
 ***********************************************************************/

OP_STATUS SyncSearchEntries::ProcessSyncItem(OpSyncItem *item, SearchTemplate** received_search)
{
	 if ( !item )
	 {
		 return OpStatus::ERR_NULL_POINTER;
	 }

	 if (!g_sync_manager->SupportsType(SyncManager::SYNC_SEARCHES))
	 {
		 return OpStatus::OK;
	 }

	 OpSyncDataItem::DataItemType type = item->GetType();

	 if (type != OpSyncDataItem::DATAITEM_SEARCH)
	 {
		 return OpStatus::ERR;
	 }

	 QuickSyncLock lock(m_is_receiving_items, TRUE, FALSE);

	 // No reason to build item and do all the processing if it's a delete.
	 if(item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_DELETED)
	 {
		 OpString itemGuid;
		 RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_ID, itemGuid));
		 SearchTemplate* search = g_searchEngineManager->GetByUniqueGUID(itemGuid);

#ifdef SYNC_CLIENT_DEBUG_SEARCH
		 OpString8 guid;
		 guid.Set(itemGuid.CStr());
		 fprintf( stderr, " SyncSearchEntries::ProcessSyncItem [%s] Action delete \n", guid.CStr());
#endif

		 if (!search)
		 {
#ifdef SYNC_CLIENT_DEBUG_SEARCH
			 fprintf( stderr, " SyncSearchEntries::ProcessSyncItem - item not found. do nothing \n");
#endif			 
			 // it doesn't exist already, so we do nothing if it's deleted
			 return OpStatus::OK;
		 }
		 else
		 {
#ifdef SYNC_CLIENT_DEBUG_SEARCH
			 fprintf( stderr, " SyncSearchEntries::ProcessSyncItem - item found. Call search_man->RemoveItem() \n");
#endif			 

			 // Should we handle non deletable items being deleted in items from server
			 g_searchEngineManager->RemoveItem(search); 

			 OpStatus::Ignore(g_searchEngineManager->Write());

			 return OpStatus::OK;
		 }
	 }

	 OpString itemGuid;
	 RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_ID, itemGuid));
	 SearchTemplate  *synced_item = g_searchEngineManager->GetByUniqueGUID(itemGuid);

#ifdef SYNC_CLIENT_DEBUG_SEARCH
	 OpString8 guid;
	 guid.Set(itemGuid.CStr());
	 fprintf( stderr, " SyncSearchEntries::ProcessSyncItem [%s] Action add/modify \n", guid.CStr());
#endif
		 

	 /************ synced_item does not exist on client ****************************/
	 if(!synced_item)
	 {
		 SearchTemplate *new_item = OP_NEW(SearchTemplate, ());

		 RETURN_OOM_IF_NULL(new_item);
		 
		 new_item->SetUniqueGUID(itemGuid);

		 RETURN_IF_ERROR(OpSyncItemToSearchTemplate(item, new_item));


		 OpString data;
		 item->GetData(OpSyncItem::SYNC_KEY_HIDDEN, data);
		 if(data.HasContent())
		 {
			 BOOL hidden = uni_atoi(data.CStr());
			 
			 if (hidden) // Hidden but we do not have it ???
			 {
				 OP_DELETE(new_item);

				 // TODO: Check the type attribute here
				 // it doesn't exist already, so we do nothing if it's deleted
				 return OpStatus::OK;
			 }
			 // if not hidden, just continue
		 }
	
		 if (received_search)
			 *received_search = new_item;

#ifdef SYNC_CLIENT_DEBUG_SEARCH
		 OpString8 url;
		 url.Set(new_item->GetUrl().CStr());
		 fprintf( stderr, " \nSyncSearchEntries::ProcessSyncItem - Adding new Search EngineItem %s\n", url.CStr());
#endif

		 // the new item must be stored as Not from package or it will not be written
		 OP_ASSERT(new_item->IsCustom());
		 
		 g_searchEngineManager->AddItem(new_item);

		 OpStatus::Ignore(g_searchEngineManager->Write());
	 }
	 else /************ synced_item exists on client *********************************/
	 {
		 if (received_search)
			 *received_search = synced_item;

		 RETURN_IF_ERROR(OpSyncItemToSearchTemplate(item, synced_item));

		 OpString data;
		 RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_HIDDEN, data));
 		 if(data.HasContent())
 		 {
 			 BOOL hidden = uni_atoi(data.CStr());
			 if (hidden)
			 {
				 if (!synced_item->IsFromPackageOrModified())
				 {
					 OP_ASSERT(!"Received hidden = TRUE on custom search\n");
					 // But ok to just fall through as RemoveItem will do the correct thing
				 }
			 }

			 // If the hidden flag now changed from false to true /
			 // If this item is not already hidden on the client, hide it now
			 if (hidden != synced_item->IsFiltered())
			 {
				 if (hidden)
				 {
					 g_searchEngineManager->RemoveItem(synced_item); 
					 OpStatus::Ignore(g_searchEngineManager->Write());
					 return OpStatus::OK;
				 }
				 else // !hidden -> "Unhide" it
				 {
					 OP_ASSERT(!"UnDelete of search item should never happen");
				 }
			 }
		 }

		 INT32 pos = g_searchEngineManager->FindItem(synced_item);
		 
#ifdef SYNC_CLIENT_DEBUG_SEARCH
		 OpString8 url;
		 url.Set(synced_item->GetUrl().CStr());
		 fprintf( stderr, "  SyncSearchEntries::ProcessSyncItem - Changing Search EngineItem %s\n", url.CStr());
#endif

		 // Call this, but ensure we're not listening, first, we don't want an infinite loop
		 g_searchEngineManager->ChangeItem(synced_item, pos, SearchEngineManager::FLAG_UNKNOWN);

		 OpStatus::Ignore(g_searchEngineManager->Write());

	 }

	 return OpStatus::OK;
}

/*****************************************************************************
 *
 * SendAllItems
 *
 * 
 *
 *****************************************************************************/

OP_STATUS  SyncSearchEntries::SendAllItems(BOOL dirty)
{
	for (UINT32 i = 0; i < g_searchEngineManager->GetSearchEnginesCount(); i++)
	{
		SearchTemplate* search = (SearchTemplate*) g_searchEngineManager->GetSearchEngine(i);

		// Only send custom searches and modified package searches
		if (search && search->IsCustomOrModified())
		{
			RETURN_IF_ERROR(SearchItemChanged(search, OpSyncDataItem::DATAITEM_ACTION_ADDED, dirty, SearchEngineManager::FLAG_UNKNOWN));
		}
	}

	return OpStatus::OK;
}


////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////

#endif //SUPPORT_SYNC_SEARCHES
#endif // SUPPORT_DATA_SYNC

