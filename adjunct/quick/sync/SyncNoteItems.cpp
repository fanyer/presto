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

#include "adjunct/quick/sync/SyncNoteItems.h"
#include "adjunct/quick/managers/SyncManager.h"
#include "modules/sync/sync_coordinator.h"
#include "adjunct/quick/Application.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/formats/base64_decode.h"
#include "modules/pi/OpLocale.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/img/imagedump.h"

#ifdef MSWIN
#include "platforms/windows/user_fun.h"
#endif

#define SYNC_NOTES_DELAY		(30*1000)		// 30 seconds

/************************************************************************************
 **
 ** SyncNoteItems - responsible for syncing of notes
 **
 ** Sends each item to the sync module _when_ it is changed/added/removed
 **
 **
 ************************************************************************************/

/**
 *  - On all Inconsistencies -> Should set dirty flag (DoCompleteSync preference)
 *  - Use cases:
 *    - Non-existant previous -> Set dirty flag.
 *    - Parent is not a folder -> Use parents parent as parent. If this is not a folder either. -> Use case 1?
 *    - Previous and parent don't match -> Insert after previous
 *
 **/

// TODO: How to handle type = trash. Currently only sending it when an item has IsTrashFolder
// but not doing anything on receiving items, really relying still on the unique GUID
// In particular: Trash folder for notes won't work correctly.


//#define SYNC_CLIENT_DEBUG

////////////////////////////////////////////////////////////////////////////////////////

SyncNoteItems::SyncNoteItems() :
	m_is_receiving_items(FALSE)
	,m_has_delayed_timer(FALSE)
{
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

SyncNoteItems::~SyncNoteItems()
{
	if (g_hotlist_manager)
	{
		HotlistModel* notes_model = g_hotlist_manager->GetNotesModel();
		if (notes_model)
			notes_model->RemoveModelListener(this);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS SyncNoteItems::SyncDataInitialize(OpSyncDataItem::DataItemType type)
{
	if (type == OpSyncDataItem::DATAITEM_NOTE)
	{
		OpFile notes_file;
		TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::NoteListFile, notes_file));
		g_sync_manager->BackupFile(notes_file);
		return OpStatus::OK;
	}

	OP_ASSERT(FALSE); // Shouldn't get any other data type
	return OpStatus::ERR;
}

// Process incoming items
OP_STATUS SyncNoteItems::SyncDataAvailable(OpSyncCollection *new_items, OpSyncDataError& data_error)
{
	OP_STATUS status = OpStatus::OK;
	BOOL do_dirty_sync = FALSE;

	if(new_items && new_items->First())
	{
		OpSyncItemIterator iter(new_items->First());

		do
		{
			OpSyncItem *current = iter.GetDataItem();

			OP_ASSERT(current);
			if(current)
			{
				status = ProcessSyncItem(current, do_dirty_sync);
				if (OpStatus::IsError(status) || do_dirty_sync)
   				{
					if (do_dirty_sync)
					{
						data_error =  SYNC_DATAERROR_INCONSISTENCY;
					}
 					break; // Don't process more items
 				}

			}
 		} while (iter.Next());
	}

	return status;
}

OP_STATUS SyncNoteItems::SyncDataFlush(OpSyncDataItem::DataItemType type, BOOL first_sync, BOOL is_dirty)
{
	switch(type)
	{
	case OpSyncDataItem::DATAITEM_NOTE:
		{
			OP_ASSERT(g_sync_coordinator->GetSupports(	SYNC_SUPPORTS_NOTE ));
			if (!g_sync_coordinator->GetSupports(	SYNC_SUPPORTS_NOTE))
				return OpStatus::ERR;

			SendAllItems(is_dirty, HotlistModel::NoteRoot);
			break;
		}
	default:
		{
			OP_ASSERT(FALSE);
			return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

void SyncNoteItems::EnableNoteListening(BOOL enable)
{
	OP_ASSERT(g_hotlist_manager);
	HotlistModel* model = g_hotlist_manager->GetNotesModel();

	if (!model)
	{
		return;
	}

	if (enable)
		model->AddModelListener(this);
	else
		model->RemoveModelListener(this);
}


/***********************************************************************
 *
 * OnSyncDataSupportsChanged
 *
 *
 *
 ***********************************************************************/

OP_STATUS SyncNoteItems::SyncDataSupportsChanged(OpSyncDataItem::DataItemType type, 
													 BOOL has_support)
{
	OP_ASSERT(g_hotlist_manager);

	if (type == OpSyncDataItem::DATAITEM_NOTE)
	{
		EnableNoteListening(has_support);

		OpStatus::Ignore(SendModifiedNotes());
	}
	else
	{
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}


//////////////////////////////////////////////////////////////////
OpSyncDataItem::DataItemType
SyncNoteItems::GetSyncTypeFromItemType( HotlistModelItem* model_item )
{
	if (!model_item || !model_item->GetModel())
		return OpSyncDataItem::DATAITEM_GENERIC;

	INT32 model_type = model_item->GetModel()->GetModelType();

	if (model_type == HotlistModel::NoteRoot)
	{
		if (model_item->IsFolder())
		{
			return OpSyncDataItem::DATAITEM_NOTE_FOLDER;
		}
		else if (model_item->IsSeparator())
		{
			return OpSyncDataItem::DATAITEM_NOTE_SEPARATOR;
		}
		else if (model_item->IsNote())
		{
			return OpSyncDataItem::DATAITEM_NOTE;
		}
		else
		{
			OP_ASSERT(FALSE);
		}
	}

	return OpSyncDataItem::DATAITEM_GENERIC;
}


// HotlistModelListener interface methods

/*****************************************************************************
 **
 ** HotlistItemChanged
 **
 ** @param HotlistModelItem* model_item
 ** @param OpSyncDataItem::DataItemStatus status
 ** @param BOOL dirty
 ** @param UINT32 changed_flags
 ** @param BOOL delayed_triggered - TRUE if this is called from a delayed triggered update
 **
 *****************************************************************************/

/*
  NOTE: Elements that aren't sent from the client to the server
  are interpreted as 'not changed'. That is, Empty fields needs to be sent.
  Missing fields will be interpreted as not-to-be-changed.

  Exception:
  Previous/Parent: If they are not present, the server will
  interpret it as being first in its folder / being in root folder.
  That is, missing parent/previous's should _not_ be sent.

  NOTE:
  For changed items we send only the fields that actually changed, not all fields.
  For removed items we send only the id and action
  For added items we send all fields

  (TODO: Could change to send only set fields on added items)

*/

OP_STATUS SyncNoteItems::HotlistItemChanged(HotlistModelItem* model_item,
												OpSyncDataItem::DataItemStatus status,
												BOOL dirty,
												UINT32 changed_flag,
												BOOL delayed_triggered)
{
	OpSyncItem *sync_item = NULL;
	OP_STATUS sync_status = OpStatus::OK;
	OpString data;

	if ( !model_item  || !model_item->GetModel())
	{
		return OpStatus::OK;
	}

	// Check if syncing has been turned off for this model
	if ( !model_item->GetModel()->GetModelIsSynced())
	{
		return OpStatus::OK;
	}

	BOOL is_add = changed_flag & HotlistModel::FLAG_ADDED;

#ifdef SYNC_CLIENT_DEBUG
	if (model_item->GetName().HasContent())
	{
		OpString8 name;
		name.Set(model_item->GetName().CStr());
		OpString8 guid;
		guid.Set(model_item->GetUniqueGUID().CStr());
		fprintf( stderr,"\n [Link] HotlistItemChanged(%s[%x], status=%d, dirty=%d, flag=%x)\n", name.CStr(), guid.CStr(), status, dirty, changed_flag);
	}

#endif

	OpSyncDataItem::DataItemType type = GetSyncTypeFromItemType(model_item);
	INT32 model_type = model_item->GetModel()->GetModelType();

	// Check if supports notes
	// Must check the model_type not the item type since folders are in notes

	if (model_type == HotlistModel::NoteRoot
		&& !g_sync_manager->SupportsType(SyncManager::SYNC_NOTES))
		return OpStatus::OK;

	if (model_type == HotlistModel::NoteRoot)
	{
		// Generate the unique id if it's not there
		if (!model_item->GetUniqueGUID().HasContent())
		{
			OP_ASSERT(!"Item added to sync does not have a unique GUID ! \n");
			OpString guid;

			// generate a default unique ID
			if(OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
			{
				model_item->SetUniqueGUID(guid.CStr(), model_type);
				model_item->GetModel()->SetDirty(TRUE);
			}
		}


		RETURN_IF_ERROR(data.Set(model_item->GetUniqueGUID()));

		OP_ASSERT(model_item->GetUniqueGUID().HasContent());

		// Must have a GUID
		if (!data.HasContent())
			return OpStatus::ERR;

		if(delayed_triggered)
		{
			// reset some data
			model_item->SetChangedFields(0);
			model_item->SetLastSyncStatus(OpSyncDataItem::DATAITEM_ACTION_NONE);
		}
		else
		{
			// Send deleted items right away
			if(model_type == HotlistModel::NoteRoot && status != OpSyncDataItem::DATAITEM_ACTION_DELETED)
			{
				// start the timer and collect all changes for an item every now and then
				model_item->SetChangedFields(model_item->GetChangedFields() | changed_flag);
				// Added state should not be changed to modified
				if (model_item->GetLastSyncStatus() != OpSyncDataItem::DATAITEM_ACTION_ADDED)
					model_item->SetLastSyncStatus(status);
				return StartTimeout();
			}
		}

		sync_status = g_sync_coordinator->GetSyncItem(&sync_item, type, OpSyncItem::SYNC_KEY_ID, data.CStr());

		if(OpStatus::IsSuccess(sync_status))
		{
			sync_item->SetStatus(status);

			DEBUG_LINK(UNI_L("%s: id '%s', status: %d, name: '%s' outgoing\n"), model_item->IsNote() ? UNI_L("NT") : UNI_L("BM"),
					   model_item->GetUniqueGUID().CStr(), status, model_item->GetName().CStr());

			// Don't set fields if item is deleted
			if (status != OpSyncDataItem::DATAITEM_ACTION_DELETED)
			{
				// NOTE: Parent and previous MUST always be sent!!
				SYNC_RETURN_IF_ERROR(SetParent(model_item, sync_item), sync_item);
				SYNC_RETURN_IF_ERROR(SetPrevious(model_item, sync_item), sync_item);

 				// If Trash folder, set type = trash
 				if (model_item->GetIsTrashFolder())
 				{
 					SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_TYPE,
 															UNI_L("trash")), sync_item);
 				}

				if (model_item->GetTarget().HasContent())
				{
					SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_TARGET,
															model_item->GetTarget().CStr()),
															sync_item);
				}

				// Notes, note folders
				if(model_item->IsNote())
				{
					if ( changed_flag & HotlistModel::FLAG_URL
						 || is_add )
					{
						SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_URI,
																model_item->GetUrl().CStr()), sync_item);
					}
				}
				if (model_item->IsFolder())
				{
					if ( changed_flag & HotlistModel::FLAG_NAME
						 || is_add )
					{
							SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_TITLE,
																	model_item->GetName().CStr()), sync_item);
					}
				}

				// Notes only
				if (model_item->IsNote())
				{
					if ( changed_flag & HotlistModel::FLAG_NAME
						 || is_add )
					{
						SYNC_RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_CONTENT,
																model_item->GetName().CStr()), sync_item);
					}
				}

				////////////////////////////////////////////////////////////////////////////////////////////////////

				// Notes, Folders
				if ( changed_flag & HotlistModel::FLAG_CREATED
					 || is_add )
				{
					SYNC_RETURN_IF_ERROR(SetCreated(model_item, sync_item), sync_item);
				}

			} // end !deleted

			/*
			  When commiting the item, if the item has been removed or added,
			  the item that was before it previously needs to have its
			  previous updated
			  1) hotlist_item was added: next_items previous is hotlist_item
			  2) hotlist_item was moved from here (or removed): next items previous is hotlist_items old previous
			  3) Hotlist_item was moved to :
			     FLAG_MOVED_TO: must update the items next_items previous
				 The moved from is handled separately in OnHotlistItemMovedFrom
			*/

			// moved, removed, added
			// if ( flag_changed & HotlistModel::FLAG_MOVED_TO, MOVED_FROM, ADDED, REMOVED )

			BOOL added = FALSE;
			HotlistModelItem* next_item = model_item->GetSiblingItem();

			OpSyncDataItem::DataItemType next_type = GetSyncTypeFromItemType(next_item);

			if (sync_item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_ADDED)
			{
				// We know there's a previous when adding
				if (next_item && model_item)
				{
					SYNC_RETURN_IF_ERROR(sync_item->CommitItem(dirty, !dirty), sync_item);

#ifdef SYNC_CLIENT_DEBUG
					fprintf( stderr, "Item added. Commit item and Update its next(%s)s previous to be me\n", uni_down_strdup(next_item->GetName().CStr()));
#endif

					SYNC_RETURN_IF_ERROR(g_sync_coordinator->UpdateItem(next_type, next_item->GetUniqueGUID().CStr(),
																		OpSyncItem::SYNC_KEY_PREV,
																		model_item->GetUniqueGUID().CStr()), sync_item);

					added = TRUE;
				}
			}
			else if (sync_item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_DELETED)
			{
				HotlistModelItem* my_previous = model_item->GetPreviousItem();

				if (next_item && my_previous)
				{
#ifdef SYNC_CLIENT_DEBUG
					fprintf( stderr, "Item deleted. Commit item and Update its next(%s)s previous to my_previous\n", uni_down_strdup(next_item->GetName().CStr()));
#endif
					SYNC_RETURN_IF_ERROR(sync_item->CommitItem(dirty, !dirty), sync_item);

					SYNC_RETURN_IF_ERROR(g_sync_coordinator->UpdateItem(next_type, next_item->GetUniqueGUID().CStr(),
																		OpSyncItem::SYNC_KEY_PREV,
																		my_previous->GetUniqueGUID().CStr()), sync_item);

					added = TRUE;
				}
				else if (next_item)
				{

#ifdef SYNC_CLIENT_DEBUG
					fprintf( stderr, "Item deleted. Commit item and Update its nexts (%s) previous to nothing\n", uni_down_strdup(next_item->GetName().CStr()));
#endif
					SYNC_RETURN_IF_ERROR(sync_item->CommitItem(dirty, !dirty), sync_item);

					SYNC_RETURN_IF_ERROR(g_sync_coordinator->UpdateItem(next_type, next_item->GetUniqueGUID().CStr(),
																		OpSyncItem::SYNC_KEY_PREV,
																		UNI_L("")), sync_item);

					added = TRUE;
				}
			}
			/* This is a possible move to -
			   TODO: Only do this when we KNOW it's a move-to  */
			else if (sync_item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_MODIFIED
					 && (changed_flag & HotlistModel::FLAG_MOVED_TO))
			{
				if (next_item && model_item)
				{
#ifdef SYNC_CLIENT_DEBUG
					fprintf( stderr, " Item moved_to. Commit item and update its next (%s) to having me as previous \n", uni_down_strdup(next_item->GetName().CStr()));
#endif
					SYNC_RETURN_IF_ERROR(sync_item->CommitItem(dirty, !dirty), sync_item);

					SYNC_RETURN_IF_ERROR(g_sync_coordinator->UpdateItem(next_type, next_item->GetUniqueGUID().CStr(),
																		OpSyncItem::SYNC_KEY_PREV,
																		model_item->GetUniqueGUID().CStr()), sync_item);

					added = TRUE;
				}
			}
			// Any change but a move, add, delete should go here.
			// And: Fallback for add, remove, modified
			if (!added)
			{
#ifdef SYNC_CLIENT_DEBUG
				fprintf( stderr, " Default CommitItem \n");
#endif
				SYNC_RETURN_IF_ERROR(sync_item->CommitItem(dirty, !dirty), sync_item);
			}
		}

	}
	if(sync_item)
	{
		g_sync_coordinator->ReleaseSyncItem(sync_item);
	}
	return sync_status;
}


// HotlistModelListener functions

/****************************************************************************
 **
 ** OnHotlistItemMovedFrom
 **
 ** @param HotlistModelItem* model_item - the item that moved from its old
 **                                       position
 **
 **  When moving an item from a position,
 **  sync module needs to know that its next got a new previous.
 **  The previous can be NULL, if the item is first in its folder
 **
 **  No need to commit item, since if it is moved somewhere it will be
 **  commited when moved _to_.
 **
 ****************************************************************************/

void SyncNoteItems::OnHotlistItemMovedFrom(HotlistModelItem* model_item)
{
	if (!model_item)
	{
		return;
	}

//#if 0
	if (IsProcessingIncomingItems())
	{
		return;
	}
//#endif

	INT32 model_type = model_item->GetModel()->GetModelType();

	if (model_type == HotlistModel::NoteRoot)
	{
		HotlistModelItem* next_item   = model_item->GetSiblingItem();
		HotlistModelItem* my_previous = model_item->GetPreviousItem();
		OpSyncDataItem::DataItemType next_type = GetSyncTypeFromItemType(next_item); // model_item?

#ifdef SYNC_CLIENT_DEBUG
		fprintf( stderr, " OnHotlistItemMovedFrom(%s[%s]. Update my next to have my previous as its previous\n", uni_down_strdup(model_item->GetName().CStr()), uni_down_strdup(model_item->GetUniqueGUID().CStr()));
#endif // SYNC_CLIENT_DEBUG

		if (next_item)
		{
			if (my_previous)
			{
				// Next items new previous is my_previous
				OpStatus::Ignore(g_sync_coordinator->UpdateItem(next_type, next_item->GetUniqueGUID().CStr(),
																OpSyncItem::SYNC_KEY_PREV,
																my_previous->GetUniqueGUID().CStr()));
			}
			else
			{
				// Next item now has no previous
				OpStatus::Ignore(g_sync_coordinator->UpdateItem(next_type, next_item->GetUniqueGUID().CStr(),
																OpSyncItem::SYNC_KEY_PREV,
																UNI_L("")));
			}
		}
	}
}


////// Helper methods to set fields on the sync item from the hotlist item //////


/**********************************************************************
 **
 ** SetParent
 ** Adds attribute parent to sync_item, based on item's parent
 **
 **********************************************************************/

OP_STATUS SyncNoteItems::SetParent(HotlistModelItem* item,
									   OpSyncItem* sync_item)
{

	if (!item || !sync_item) return OpStatus::ERR_NULL_POINTER;

	OpString parentGuid;
	HotlistModelItem* parent   = item->GetParentFolder();

	if (parent && item && /*parent == item*/ (parent->GetIndex() == item->GetIndex()))
	{
#ifdef SYNC_CLIENT_DEBUG
		OpString8 parent_guid;
		OpString8 guid;
		parent_guid.Set(parent->GetUniqueGUID().CStr());
		guid.Set(item->GetUniqueGUID().CStr());
		if (parent->GetUniqueGUID().Compare(item->GetUniqueGUID()) == 0)
			fprintf( stderr, " Item %s cannot be parent of item %s\n", parent_guid.CStr(), guid.CStr());
#endif
		OP_ASSERT(!"Parent cannot be item itself \n");
		return OpStatus::ERR;
	}

	if (parent)
	{

		parentGuid.Set( parent->GetUniqueGUID() );
#ifdef SYNC_CLIENT_DEBUG
		fprintf( stderr, " item %s has parent %s\n", uni_down_strdup(item->GetUniqueGUID()), uni_down_strdup(parentGuid));
#endif

		RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_PARENT, parentGuid.CStr()));
	}
	else
	{
		RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_PARENT, UNI_L("")));

	}

	return OpStatus::OK;
}


/************************************************************************
 **
 ** SetPrevious
 **
 ** Adds attribute previous to sync_item, based on item's previous
 **
 ************************************************************************/

OP_STATUS SyncNoteItems::SetPrevious(HotlistModelItem* item,
										 OpSyncItem* sync_item)
{
	if (!item || !sync_item) return OpStatus::ERR_NULL_POINTER;

	OpString previousGuid;
	HotlistModelItem* previous = item->GetPreviousItem();

	if (previous && item && (previous->GetIndex() == item->GetIndex()))
	{
		OP_ASSERT(!"Previous cannot be item itself");
		return OpStatus::ERR;
	}

	if (previous)
	{
		previousGuid.Set(previous->GetUniqueGUID());

		RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_PREV, previousGuid.CStr()));
	}
	else
	{
		RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_PREV, UNI_L("")));

	}

	return OpStatus::OK;
}


/************************************************************************
 **
 ** SetCreated
 **
 ** Set created attribute on sync_item from item
 **
 ************************************************************************/
OP_STATUS SyncNoteItems::SetCreated(HotlistModelItem* item,
										OpSyncItem* sync_item)
{
	if (!item || !sync_item) return OpStatus::ERR_NULL_POINTER;

	OpString createdDate;
	time_t cdate = item->GetCreated();
	if (cdate > 0)
	{
		OP_STATUS status = SyncUtil::CreateRFC3339Date( cdate, createdDate);
		if (OpStatus::IsSuccess(status))
		{
			RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_CREATED, createdDate.CStr()));
		}
	}
	return OpStatus::OK;
}

/*************************************************************************
 **
 ** ProcessSyncItem
 **
 ** Process OpSyncItem* item; build the corresponding hotlistmodelItem
 ** and change or add or remove it from the hotlistmodel as appropriate.
 **
 ** Called when synchronization has completed.
 ** Updates "local" synced items.
 **
 ** Note; The SyncManager will ask for a set the dirty flag to ask
 ** for a complete sync if ProcessSyncItem returns OpStatus::ERR
 **
 **************************************************************************/

// TODO: Only set the fields that actually changed
OP_STATUS SyncNoteItems::ProcessSyncItem(OpSyncItem *item, BOOL& dirty)
{
	if (!item)
		return OpStatus::ERR_NULL_POINTER;

	QuickSyncLock lock(m_is_receiving_items, TRUE, FALSE);

	OpSyncDataItem::DataItemType type = item->GetType();

	HotlistGenericModel* model = NULL;

	if (type == OpSyncDataItem::DATAITEM_NOTE ||
			 type == OpSyncDataItem::DATAITEM_NOTE_FOLDER ||
			 type == OpSyncDataItem::DATAITEM_NOTE_SEPARATOR)
	{
		if(!g_sync_manager->SupportsType(SyncManager::SYNC_NOTES))
		{
			return OpStatus::OK;
		}

		model = g_hotlist_manager->GetNotesModel();
	}
	else
	{
		return OpStatus::ERR;
	}

	// No reason to build item and do all the processing if it's a delete.
	if(item->GetStatus() == OpSyncDataItem::DATAITEM_ACTION_DELETED)
	{
		OpString itemGuid;
		RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_ID, itemGuid));
		HotlistModelItem* deleted_item = model->GetByUniqueGUID(itemGuid);

		if (!deleted_item)
		{
			// it doesn't exist already, so we do nothing if it's deleted
			return OpStatus::OK;
		}
		else
		{
			// Should we handle non deletable items being deleted in items from server
			// OP_ASSERT(deleted_item->GetIsDeletable());
			BOOL deleted = model->DeleteItem(deleted_item, FALSE);
			if ( !deleted )
			{
				OP_ASSERT(!"Failed deleting item \n");
			}

			model->SetDirty(TRUE);
			return OpStatus::OK;
		}
	}

	// Don't want notifications about favicons added during processing incoming items
	model->SetIsSyncing(TRUE);

	HotlistManager::ItemData item_data;

	double created_date = 0;

	OpString previousGuid;
	OpString parentGuid;

	OP_STATUS build_status = OpStatus::ERR;

	if (type == OpSyncDataItem::DATAITEM_NOTE ||
		type == OpSyncDataItem::DATAITEM_NOTE_FOLDER ||
		type == OpSyncDataItem::DATAITEM_NOTE_SEPARATOR)
	{
		build_status = BuildNotesItem(item, item_data, created_date, previousGuid, parentGuid);
	}

	if (OpStatus::IsError(build_status))
	{
		model->SetIsSyncing(FALSE);
		return build_status;
	}
	// TODO: Handle Trash folder.

	HotlistModelItem  *synced_item = NULL;
	HotlistModelItem  *parent	   = 0;
	HotlistModelItem  *previous	   = 0;
	BOOL no_parent				   = FALSE;
	BOOL no_previous			   = FALSE;

	if (parentGuid.HasContent())
	{
		parent = model->GetByUniqueGUID(parentGuid);

		if (parent && !parent->IsFolder()) // If parent is not folder, use parents parent as parent
		{
			DEBUG_LINK(UNI_L("Error: Parent '%s' not a folder \n"), parent->GetName().CStr());
			parent = parent->GetParentFolder();
		}
		// If no parentGuid -> Insert in root
	}
	else
	{
		no_parent = TRUE;   // Item is in root folder
	}

	if (previousGuid.HasContent())
	{
		previous = model->GetByUniqueGUID(previousGuid);
	}
	else
	{
		no_previous = TRUE; // Item is first in its parent
	}

	if (!no_parent && !parent) // Has parent, but parent does not exist -> error
	{
		dirty = TRUE;

#if defined(_DEBUG) && (defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_))
		OpString8 guid;
		guid.Set(parentGuid.CStr());
		fprintf( stderr, " ** USE CASE 1 **: Non-existent parent %s\n", guid.CStr());
#endif
		DEBUG_LINK(UNI_L("Error: Non-existing parent id '%s'\n"), parentGuid.CStr());

		/* No parent: Drop processing this item, and set dirty flag */
		model->SetIsSyncing(FALSE);
		return OpStatus::OK;

	}
	else if (!no_previous && !previous) // has previous, but previous does not exist -> error
	{
		dirty = TRUE;

#if defined(_DEBUG) && (defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_))
		OpString8 guid;
		guid.Set(previousGuid.CStr());
		fprintf( stderr, " ** USE CASE 2 **: Non-existent previous %s\n", guid.CStr());
#endif
		DEBUG_LINK(UNI_L("Error: Non-existing previous id '%s'\n"), previousGuid.CStr());

#ifdef SYNC_CLIENT_DEBUG
		OpString8 name;
		name.Set(item_data.name.CStr());
		if (item_data.name.HasContent())
			fprintf( stderr, " Item %s \n",  name.CStr());
#endif
		model->SetIsSyncing(FALSE);
		return OpStatus::OK;
	}

	synced_item = model->GetByUniqueGUID(item_data.unique_guid);


	/************ synced_item does not exist on client **********************************************************/
	if(!synced_item)
	{
#ifdef SYNC_CLIENT_DEBUG
		OpString8 guid;
		guid.Set(item_data.unique_guid.CStr());
		fprintf( stderr, " Item %s does not exist on client \n", guid.CStr());
#endif
		DEBUG_LINK(UNI_L("BM: NEW id '%s', name: '%s', folder: '%s', NEW from server\n"), item_data.unique_guid.CStr(), item_data.name.CStr(), item_data.folder.CStr());

		// Neither parent nor previous -> First in root
		if (no_previous && no_parent)
		{
			OP_ASSERT(model);
			if (OpStatus::IsError(CreateItemFirst(model, type, item_data, NULL)))
			{
#if defined(_DEBUG) && (defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_))
				fprintf( stderr, " ERROR: ProcessSyncItem: Couldn't create item \n");
#endif
				model->SetIsSyncing(FALSE);
				return OpStatus::ERR;
			}
		}
		/* If there is a previous set, make a new item that is this previous' next */
		/* If not, insert the item first in the parent folder */
		/* If neither  previous or parent, insert item first in root  ( case above) */
		else if (previous)
		{
			OP_ASSERT(!no_previous);
			/* Check if parent matches. If not, set dirty flag */
			if (parent && previous->GetParentFolder() != parent)
			{
				dirty = TRUE;

#if defined(_DEBUG) && (defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_))
				fprintf( stderr, " ** USE CASE 7 **: Previous and parent does not match \n");
#endif
				DEBUG_LINK(UNI_L("Error: Previous and parent does not match \n"));
			}

			OP_ASSERT(model);
			if (OpStatus::IsError(CreateItemOrdered(model, type, item_data, previous)))
			{
#if defined(_DEBUG) && (defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_))
				fprintf( stderr, " ERROR: ProcessSyncItem Couldn't create item \n");
#endif
				// Ask for complete sync. Dont proceed.
				model->SetIsSyncing(FALSE);
				return OpStatus::ERR;
			}
		}
		else if (parent && parent->IsFolder())
		{
			OP_ASSERT(!no_parent);
			OP_ASSERT(model);
			if (OpStatus::IsError(CreateItemFirst(model, type, item_data, parent)))
			{
				// Couldn't create item
#if defined(_DEBUG) && (defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_))
				fprintf( stderr, " ERROR: ProcessSyncItem: Couldn't create item \n");
#endif
				// Don't do further sync. But ask for complete sync instead.
				model->SetIsSyncing(FALSE);
				return OpStatus::ERR;
			}
		}
		else if (parent) // Parent not a folder: Current use case solution: Use parents parent as items parent
		{
#if defined(_DEBUG) && (defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_))
			fprintf( stderr, " ** USE CASE 4 **: Parent not a folder \n");
#endif
			DEBUG_LINK(UNI_L("Error: Parent '%s' not a folder \n"), parent->GetName().CStr());

			parent = parent->GetParentFolder();

			dirty = TRUE;

			// TODO: FIX FOR NOTES!!
			if (!parent->IsFolder() || (type == OpSyncDataItem::DATAITEM_NOTE_FOLDER
										 && !parent->GetSubfoldersAllowed()))
				// parents parent is not a folder either, handle like use case 1
				// Handle same way if parent has subfolders not allowed
			{
#if defined(_DEBUG) && (defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_))
				fprintf( stderr, " ** USE CASE 4 -> 1 **: Parents parent not a folder \n");
#endif
				// DEBUG_LINK(UNI_L("And parents parent not a folder \n"));
				model->SetIsSyncing(FALSE);
				return OpStatus::OK;
			}
		}
		else
		{
			OP_ASSERT(!"We should not be here \n");
#if defined(_DEBUG) && (defined(_MACINTOSH_) || defined(_UNIX_DESKTOP_))
			OpString8 guid;
			guid.Set(item_data.unique_guid.CStr());
			fprintf( stderr, " New Item with GUID %s\n", guid.CStr());
			fprintf( stderr, " previous=%p, parent=%p, no_previous=%d, no_parent=%d\n", previous,parent,no_previous, no_parent);
#endif
		}

		HotlistModelItem* bm_item = model->GetByUniqueGUID( item_data.unique_guid );

		if (bm_item)
		{
			/********* Set created field *********/

			if (item_data.created.HasContent() && created_date > 0)
			{
				bm_item->SetCreated( (INT32)created_date );
			}

			OpString data;
			OP_STATUS status = item->GetData(OpSyncItem::SYNC_KEY_TARGET, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				bm_item->SetTarget(data.CStr());
			}
			status = item->GetData(OpSyncItem::SYNC_KEY_MOVE_IS_COPY, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				INT32 copy = uni_atoi(data.CStr());
				bm_item->SetHasMoveIsCopy(copy != 0);
			}
			status = item->GetData(OpSyncItem::SYNC_KEY_DELETABLE, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				INT32 deletable = uni_atoi(data.CStr());
				bm_item->SetIsDeletable(deletable != 0);
			}
			status = item->GetData(OpSyncItem::SYNC_KEY_SUB_FOLDERS_ALLOWED, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				INT32 subfolder = uni_atoi(data.CStr());
				bm_item->SetSubfoldersAllowed(subfolder != 0);
			}
			status = item->GetData(OpSyncItem::SYNC_KEY_SEPARATORS_ALLOWED, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				INT32 separator = uni_atoi(data.CStr());
				bm_item->SetSeparatorsAllowed(separator != 0);
			}
			status = item->GetData(OpSyncItem::SYNC_KEY_MAX_ITEMS, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				INT32 max_items = uni_atoi(data.CStr());
				if (max_items >= 0)
				{
					bm_item->SetMaxItems(max_items);
				}
			}
		}

		model->SetDirty(TRUE);
	}
	else /*********** Item already exists on client **********************************/
	{
		if ( previous && previous->GetParentFolder() == synced_item
			|| parent && parent->GetParentFolder() == synced_item)
			{
				model->SetIsSyncing(FALSE);
				m_is_receiving_items = FALSE;
				return OpStatus::OK;
			}
		
#ifdef SYNC_CLIENT_DEBUG
		OpString8 guid;
		guid.Set(item_data.unique_guid.CStr());
		fprintf( stderr, " Bookmark %s does exist on client \n", guid.CStr());
#endif

		DEBUG_LINK(UNI_L("BM: OLD id '%s', name: '%s', folder: '%s', (existing) from server\n"), item_data.unique_guid.CStr(), item_data.name.CStr(), item_data.folder.CStr());

		switch(item->GetStatus())
		{
			case OpSyncDataItem::DATAITEM_ACTION_ADDED:
			case OpSyncDataItem::DATAITEM_ACTION_MODIFIED:
			{
				// If no previous - > first in parent
				// If no parent   - > in root folder
#ifdef SYNC_CLIENT_DEBUG
				OpString8 name;
				name.Set(item_data.name.CStr());
				fprintf( stderr, " Item %s moved to new position or changed\n", name.CStr());
#endif

				// 1. Moved to position first in folder or root
				if (!previous) // Was _moved_ to being first in its folder
				{
#ifdef SYNC_CLIENT_DEBUG
					fprintf( stderr, " No previous. reparenting \n");
#endif

					// Only move if it actually moved to a new position
					if ( synced_item->GetPreviousItem() != NULL
						 || synced_item->GetParentFolder() != parent )
					{
						model->Reparent(synced_item, parent, TRUE, synced_item->GetIsTrashFolder());
					}
				}
				// 2. New previous item (diff from old previous, or had no previous before)
				// Reorder, new previous (and possibly parent) item
				else if (previous && synced_item->GetPreviousItem() != previous)
				{
#ifdef SYNC_CLIENT_DEBUG
					if (synced_item->GetName().HasContent())
					{
						OpString8 name;
						OpString8 prev_name;
						name.Set(synced_item->GetName().CStr());
						prev_name.Set(previous->GetName().CStr());
						fprintf( stderr, " Item %s has new previous %s\n", name.CStr(), prev_name.CStr());
					}
#endif
					if (parent)
					{
						OP_ASSERT(parent == previous->GetParentFolder());
					}
#ifdef SYNC_CLIENT_DEBUG
						fprintf( stderr, " Reorder item -----------\n");
#endif
					model->Reorder(synced_item, previous, synced_item->GetIsTrashFolder());
				}
				// 3. New parent, previous unchanged
				else if (parent && synced_item->GetParentFolder() != parent) // Reparent, insert first in parent
				{
#ifdef SYNC_CLIENT_DEBUG
					fprintf( stderr, " item has new parent. same previous \n");
#endif

					// No previous data sent
					model->Reparent(synced_item, parent, TRUE, synced_item->GetIsTrashFolder() );

				}
				else if (!parent && synced_item->GetParentFolder())
				{
					//   has previous and previous is unchanged
					//   !parent OR (has parent and parent is unchanged)
					//   model->Reorder(bookmark, previous, bookmark->GetIsTrashFolder());
				}

				synced_item = model->GetByUniqueGUID(item_data.unique_guid);

				// We have to set all the data we get from the server
				INT32 flag = HotlistModel::FLAG_UNKNOWN;

				if( synced_item && !g_hotlist_manager->SetItemValue(synced_item, item_data, TRUE, flag))
				{
					OP_ASSERT(FALSE);
				}
			}
			break;

		default:
			// No action defined. What do we do?
			OP_ASSERT(FALSE);
			model->SetIsSyncing(FALSE);
			return OpStatus::ERR;
		}

		OP_STATUS status;
		OpString data;

		/********* Set visited field *********/
		if (synced_item)
		{
			status = item->GetData(OpSyncItem::SYNC_KEY_TARGET, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				synced_item->SetTarget(data.CStr());
			}
			status = item->GetData(OpSyncItem::SYNC_KEY_MOVE_IS_COPY, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				INT32 copy = uni_atoi(data.CStr()); // 0 eller 1
				synced_item->SetHasMoveIsCopy(copy != 0);
			}
			status = item->GetData(OpSyncItem::SYNC_KEY_DELETABLE, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				INT32 deletable = uni_atoi(data.CStr()); // 0 eller 1
				synced_item->SetIsDeletable(deletable != 0);
			}
			status = item->GetData(OpSyncItem::SYNC_KEY_SUB_FOLDERS_ALLOWED, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				INT32 subfolder = uni_atoi(data.CStr());
				synced_item->SetSubfoldersAllowed(subfolder != 0);
			}
			status = item->GetData(OpSyncItem::SYNC_KEY_SEPARATORS_ALLOWED, data);
			if (OpStatus::IsSuccess(status) && data.HasContent())
			{
				INT32 separator = uni_atoi(data.CStr());
				synced_item->SetSeparatorsAllowed(separator != 0);
			}
		}
		status = item->GetData(OpSyncItem::SYNC_KEY_MAX_ITEMS, data);
		if (OpStatus::IsSuccess(status) && data.HasContent())
		{
			INT32 max_items = uni_atoi(data.CStr());
			if (max_items >= 0)
			{
				synced_item->SetMaxItems(max_items);
			}
		}

		model->SetDirty(TRUE);
	}

	model->SetIsSyncing(FALSE);
	return OpStatus::OK;
}



/********************************************************************
 **
 ** SendAllItems
 **
 ** Sends all items to the sync module
 ** To be used on first login and when doing a dirty sync
 **
 ** @param dirty - should be TRUE if doing a dirty sync
 **
 ********************************************************************/

OP_STATUS SyncNoteItems::SendAllItems(BOOL dirty, INT32 model_type)
{
	HotlistModel *model = 0;
	if (model_type == HotlistModel::NoteRoot)
	{
		model = g_hotlist_manager->GetNotesModel();
	}
	else
	{
		return OpStatus::ERR;
	}

	HotlistModelItem *item;
	INT32 i;

	for (i = 0; i < model->GetItemCount(); i++)
	{
		item = model->GetItemByIndex(i);

		// Generate the unique id if it's not there
		if (!item->GetUniqueGUID().HasContent())
		{
			OP_ASSERT(!"Item added to sync does not have a unique GUID ! \n");
			OpString guid;

			// generate a default unique ID
			if(OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
			{
				item->SetUniqueGUID(guid.CStr(), model_type);
			}
		}

		if (!item->GetUniqueGUID().HasContent())
			return OpStatus::ERR;

		// Last param, delay_triggered, must be TRUE, to force us to give the items to the sync module at once.
		RETURN_IF_ERROR(HotlistItemChanged(item, OpSyncDataItem::DATAITEM_ACTION_ADDED, dirty, HotlistModel::FLAG_UNKNOWN, TRUE));
	}

	return OpStatus::OK;
}

/************************************************************************
 **
 ** BuildNotesItem - Processes an incoming notes model item
 **
 ** Build the ItemData from the OpSyncItem received from the sync module
 **
 ** @param OpSyncItem* item - item to set fields from
 ** @param HotlistManager::ItemData& item_data - item to set fields on
 ** @param double& created_date - holds created date on return
 ** @param OpString& previousGuid - holds Guid of items previous on return,
 **                                 empty if it doesnt have a previous
 ** @param OpString& parentGuid - holds Guid of items parent on return,
 **                               empty if it doesnt have a parent
 **
 ************************************************************************/

OP_STATUS SyncNoteItems::BuildNotesItem(OpSyncItem* item,
						 HotlistManager::ItemData& item_data,
						 double& created_date,
						 OpString& previousGuid,
						 OpString& parentGuid)
{
	if (!item)
		return OpStatus::OK;

	OpString data;
	created_date = 0;

	OpSyncDataItem::DataItemType type = item->GetType();

	OP_STATUS status = item->GetData(OpSyncItem::SYNC_KEY_ID, data);

	RETURN_IF_ERROR(item_data.unique_guid.Set(data));

	status = item->GetData(OpSyncItem::SYNC_KEY_CREATED, data);

	/**
	 * Created won't be set in item->SetItemValue
	 * Created will be set to current time in NewBookmark
	 * So we use SetCreated on the note to set it after ev. call to NewNote
	 **/
	if (OpStatus::IsSuccess(status) && data.HasContent())
	{
		created_date = OpDate::ParseRFC3339Date(data.CStr()); // returns op_nan if not valid date
		if(!created_date)
		{
			// no date, what now?
		}
	}

	status = item->GetData(OpSyncItem::SYNC_KEY_PARENT, data);
	if (OpStatus::IsSuccess(status) && data.HasContent())
	{
		status = parentGuid.Set(data);
		if (OpStatus::IsError(status))
		{
			return OpStatus::ERR;
		}
	}

	status = item->GetData(OpSyncItem::SYNC_KEY_PREV, data);
	if (OpStatus::IsSuccess(status) && data.HasContent())
	{
		status = previousGuid.Set(data);
		if (OpStatus::IsError(status))
			return OpStatus::ERR;
	}

	status = item->GetData(OpSyncItem::SYNC_KEY_CONTENT, data);
	if (OpStatus::IsSuccess(status))
	{
		OpStatus::Ignore(item_data.name.Set(data));
	}

	if (type == OpSyncDataItem::DATAITEM_NOTE)
	{
		status = item->GetData(OpSyncItem::SYNC_KEY_URI, data);
		if (OpStatus::IsSuccess(status))
		{
				OpStatus::Ignore(item_data.url.Set(data));
		}
	}
	else if (type == OpSyncDataItem::DATAITEM_NOTE_FOLDER)
	{
		status = item->GetData(OpSyncItem::SYNC_KEY_TITLE, data);
		if (OpStatus::IsSuccess(status))
		{
			OpStatus::Ignore(item_data.name.Set(data));
		}
	}


	// Not syncing note icon
	return OpStatus::OK;

}

// HotlistModelListener methods

/***************************************************************************
 **
 ** OnHotlistItemAdded
 **
 ** @param HotlistModelItem* item - the item that has been added
 **
 ** In old world, this will start they sync timer.
 **
 **************************************************************************/
void SyncNoteItems::OnHotlistItemAdded(HotlistModelItem* item)
{
	// OP_ASSERT(item->GetStatus() == HotlistModelItem::STATUS_NEW);

	if (!item)
		return;

	// Don't re-add items we're receiving
	if (IsProcessingIncomingItems())
	{
		return;
	}

	// New world: Send item to sync module
	HotlistItemChanged(item, OpSyncDataItem::DATAITEM_ACTION_ADDED, FALSE, HotlistModel::FLAG_ADDED);
}


/***************************************************************************
 **
 ** OnHotlistItemChanged
 **
 ** @param HotlistModelItem* item - the changed item
 ** @param UINT32 changed_flag    - hold which changed happened
 **
 **************************************************************************/
void SyncNoteItems::OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, 
											 UINT32 changed_flag)
{
	if (!item)
		return;

	if (IsProcessingIncomingItems() || moved_as_child)
	{
		return;
	}

	// New world: Give Item to the sync module
	HotlistItemChanged(item, OpSyncDataItem::DATAITEM_ACTION_MODIFIED, FALSE, changed_flag);
}

/***************************************************************************
 **
 ** OnHotlistItemRemoved
 **
 ** @param HotlistModelItem* item - the item that is to be removed
 **
 ** Even in old world: Add immediately to Sync when removed, or the
 ** removal will be lost.
 **
 **************************************************************************/
void SyncNoteItems::OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child)
{
	if (IsProcessingIncomingItems() || !item || removed_as_child)
		return;

	// if the item was removed because it's parent was, do not send the delete
	// if (flag & HotlistModel::FLAG_CHILD)
	// return;

	// Send item to sync module
	HotlistItemChanged(item, OpSyncDataItem::DATAITEM_ACTION_DELETED, FALSE, HotlistModel::FLAG_REMOVED);
}

//////////////////////////////////////////////////////////////
/// Methods to create new incomings item in hotlist model

/**********************************************************************
 ** CreateItemOrdered
 **
 ** @param HotlistModel* model - model to create new item in
 ** @param type - Type of item to Create
 ** @param HotlistManager::ItemData& item_data - fields of new item
 ** @param HotlistModel* previous -
 **
 ** Creates a new item with ItemData item_data after item previous in
 ** model
 **
 **
 ***********************************************************************/
OP_STATUS SyncNoteItems::CreateItemOrdered(HotlistGenericModel* model,
											   OpSyncDataItem::DataItemType type,
											   const HotlistManager::ItemData& item_data,
											   HotlistModelItem* previous )
{
	if (!model)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	OpString name;
	OpTreeModelItem::Type hotlist_item_type;

	// Translate sync item type to hotlist item type
	switch ( type )
	{
	case OpSyncDataItem::DATAITEM_NOTE:
		hotlist_item_type = OpTreeModelItem::NOTE_TYPE;
		break;
	case OpSyncDataItem::DATAITEM_NOTE_FOLDER:
		hotlist_item_type = OpTreeModelItem::FOLDER_TYPE;
		break;
	case OpSyncDataItem::DATAITEM_NOTE_SEPARATOR:
		hotlist_item_type = OpTreeModelItem::SEPARATOR_TYPE;
		break;
	default:
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	if (!g_hotlist_manager->NewItemOrdered(model, hotlist_item_type, &item_data, previous ))
	{
		OP_ASSERT(FALSE);
	}

	return OpStatus::OK;
}


/******************************************************************************
 ** CreateItemFirst
 **
 ** @param HotlistModel* model - model to create new item in
 ** @param OpSyncDataItem::DataItemType type - type of item to create
 ** @param HotlistManager::ItemData& item_data - fields of new item
 ** HotlistModelItem* parent - Item should be inserted first in parent.
 **                  If parent NULL, it should be inserted in root folder.
 **
 *****************************************************************************/
OP_STATUS SyncNoteItems::CreateItemFirst(HotlistGenericModel* model,
											 OpSyncDataItem::DataItemType type,
											 const HotlistManager::ItemData& item_data,
											 HotlistModelItem* parent)
{
	if (!model)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	OpTreeModelItem::Type hotlist_item_type;

	// Translate sync item type to hotlist item type
	switch ( type )
	{
	case OpSyncDataItem::DATAITEM_NOTE:
		hotlist_item_type = OpTreeModelItem::NOTE_TYPE;
		break;
	case OpSyncDataItem::DATAITEM_NOTE_FOLDER:
		hotlist_item_type = OpTreeModelItem::FOLDER_TYPE;
		break;
	case OpSyncDataItem::DATAITEM_NOTE_SEPARATOR:
		hotlist_item_type = OpTreeModelItem::SEPARATOR_TYPE;
		break;
	default:
		OP_ASSERT(FALSE);
		return OpStatus::ERR;
	}

	if(!g_hotlist_manager->NewItemFirstInFolder(model, hotlist_item_type, &item_data, parent))
	{
		OP_ASSERT(FALSE);
	}

	return OpStatus::OK;
}

void SyncNoteItems::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_DELAYED_SYNCHRONIZATION);

	if(msg == MSG_DELAYED_SYNCHRONIZATION)
	{
		// unset the callback. If the user edits more, it'll be set again
		m_has_delayed_timer = FALSE;
		g_main_message_handler->UnsetCallBack(this, MSG_DELAYED_SYNCHRONIZATION);

		OpStatus::Ignore(SendModifiedNotes());
	}
}

// start a delayed trigger used for sending notes to the server after a certain time interval
OP_STATUS SyncNoteItems::StartTimeout()
{
	if(m_has_delayed_timer)
	{
		// we already have a timer going. This is more efficient than checking if the message is in the message queue
		return OpStatus::OK;
	}
	if (!g_main_message_handler->HasCallBack(this, MSG_DELAYED_SYNCHRONIZATION, 0))
	{
		RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_DELAYED_SYNCHRONIZATION, 0));
	}
	g_main_message_handler->RemoveDelayedMessage(MSG_DELAYED_SYNCHRONIZATION, 0, 0);

	m_has_delayed_timer = TRUE;

	g_main_message_handler->PostDelayedMessage(MSG_DELAYED_SYNCHRONIZATION, 0, 0, SYNC_NOTES_DELAY);

	return OpStatus::OK;
}


// This should actully only send changed items (not adds or deletes)
OP_STATUS SyncNoteItems::SendModifiedNotes()
{
	HotlistModel* notes_model = g_hotlist_manager->GetNotesModel();
	if (notes_model)
	{
		// send off modified but not yet synchronized items
		INT32 count = notes_model->GetItemCount();
		for(INT32 i = 0; i < count; i++)
		{
			HotlistModelItem* item = notes_model->GetItemByIndex(i);
			if(item)
			{
				if (item->GetLastSyncStatus() != OpSyncDataItem::DATAITEM_ACTION_NONE)
				{
					OP_STATUS status = HotlistItemChanged(item, item->GetLastSyncStatus(), FALSE, item->GetChangedFields(), TRUE);
					if(OpStatus::IsError(status))
					{
						return status;
					}
				}
			}
		}
	}

	return OpStatus::OK;
}
