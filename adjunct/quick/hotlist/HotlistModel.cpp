/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */
#include "core/pch.h"

#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/panels/GadgetsPanel.h"

#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkin.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/img/image.h"
#include "modules/img/src/imagemanagerimp.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/pi/OpLocale.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/util/timecache.h"

#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"

#include "modules/pi/OpPainter.h"

#include "modules/prefs/prefsmanager/collections/pc_webserver.h"

#include "adjunct/quick_toolkit/widgets/OpTreeView/TreeViewModelItem.h"

#include <time.h>

OpString HotlistModelItem::m_dummy;
INT32 HotlistModelItem::m_id_counter = HotlistModel::MaxRoot+1;

//#define DEBUG_HOTLIST_HASH

/***************************************************************************
 *
 *
 * Each HotlistItem has a unique id, and is in the hash table m_unique_guids
 * which maps guids to items.
 *
 * GUIDS are set in the listener when an item is added to the model.
 * Except from this, items should in principle not get new unique GUIDs
 * while they are in a model.
 *
 * Among other things, this would make (new world) sync not work,
 * since the items are added to sync on the listener notifications.
 *
 ***************************************************************************/

/***************************************************************************
 *
 * GetHostNameLength
 *
 * @param uni_char* candidate - 
 *
 ***************************************************************************/

static INT32 GetHostnameLength( const uni_char* candidate )
{
	INT32 length = 0;

	if( candidate )
	{
		int count = 0;
		while( candidate[length] )
		{
			if( candidate[length] == '/' )
			{
				count ++;
				if( count == 3 )
				{
					break;
				}
			}
			length ++;
		}
	}

	return length;
}


/***************************************************************************
 *
 * IsSameHostname
 *
 *
 ***************************************************************************/

static BOOL IsSameHostname(const uni_char* candidate1,
						   const uni_char* candidate2)
{
	int length1 = GetHostnameLength( candidate1 );
	int length2 = GetHostnameLength( candidate2 );

	if( length1 > 0 && length1 == length2 )
	{
		return uni_strnicmp( candidate1, candidate2, length1 ) == 0;
	}
	else
	{
		return FALSE;
	}
}


/***********************************************************************************
 **
 **	HotlistModel
 **
 ***********************************************************************************/

INT32 HotlistModel::m_sort_column = 0;
BOOL  HotlistModel::m_sort_ascending = FALSE;


HotlistModel::HotlistModel(INT32 type, BOOL temp )
:	m_current_item(0),
	m_is_dirty(FALSE),
	m_is_moving_items(FALSE),
	m_bypass_trash_on_delete(FALSE), // TODO: Reconsider use
	m_is_syncing(FALSE),
	m_model_is_synced(TRUE),
	m_parent_index(-1),
	m_current_index(-1),
	m_active_folder_id(-1),
	m_type(type),
	m_timer(0),
	m_unique_guids(FALSE), // In case there are lower case uuids floating around, we have to be case insensitive
	m_temporary(temp),
	m_move_as_child(FALSE)
#ifdef WEBSERVER_SUPPORT
	, m_root_service(0)
#endif
{
}


HotlistModel::~HotlistModel()
{
	OP_DELETE(m_timer);
}


/******************************************************************************
 *
 * HotlistModel:Erase
 *
 *
 ******************************************************************************/
// Doesn't trigger OnItemRemoving,
// special handling of hash table for now

void HotlistModel::Erase()
{
	m_unique_guids.RemoveAll();

	if (!m_temporary)
	{
		m_bypass_trash_on_delete = TRUE;
		for (INT32 i = 0; i < GetCount(); i++)
		{
			BroadcastHotlistItemRemoving(GetItemByIndex(i), FALSE); // ???
		}
	}

	DeleteAll();

	m_bypass_trash_on_delete = FALSE;
	m_parent_index = -1;
	m_current_index = -1;
	m_active_folder_id = -1;
	m_is_dirty = FALSE;
	OP_DELETE(m_timer);
	m_timer = 0;
}

/******************************************************************************
 *
 * HotlistModel::BraodcastHotlistItemMovedFrom
 *
 * @param HotlistModelItem* hotlist_item
 *
 *
 ******************************************************************************/

// TODO: Drop ItemUnTrashed function, and call OnHotlistItemUnTrashed from here

void HotlistModel::BroadcastHotlistItemMovedFrom(HotlistModelItem* hotlist_item)
{
	// BroadcastHotlistItemChanged(hotlist_item, HotlistModel::FLAG_MOVED_FROM);
	for(UINT32 i = 0; i < m_model_listeners.GetCount(); i++)
	{
		HotlistModelListener *ml = m_model_listeners.Get(i);

		ml->OnHotlistItemMovedFrom(hotlist_item);
	}
}


/******************************************************************************
 *
 * HotlistModel::BraodcastHotlistItemRemoving
 *
 * @param HotlistModelItem* hotlist_item
 *
 *
 ******************************************************************************/

void HotlistModel::BroadcastHotlistItemRemoving(HotlistModelItem* hotlist_item, BOOL removed_as_child)
{
#ifdef DEBUG_HOTLIST_HASH
	if (!g_hotlist_manager->IsClipboard(this)
		&& !m_temporary)
	{
		OpString8 name;
		OpString8 guid;
		name.Set(hotlist_item->GetName().CStr());
		guid.Set(hotlist_item->GetUniqueGUID().CStr());
		fprintf( stderr, "\n HotlistModel[%p]::BroadcastHotlistItemRemoving(%s)[%s]\n", this, name.CStr(), guid.CStr());

	}
#endif

	if( m_is_moving_items)
	{
		// Do not inform external listeners about internal deletions when we
		// move from one location to another (we move with: copy item -
		// insert copied item elsewhere - delete original item)

#ifdef DEBUG_HOTLIST_HASH
		fprintf( stderr, " Item is being moved _from_ a position. Sending OnHotlistItemMovedFrom \n");
#endif

		// BroadcastHotlistItemChanged(hotlist_item, HotlistModel::FLAG_MOVED_FROM);
		for(UINT32 i = 0; i < m_model_listeners.GetCount(); i++)
		{
			HotlistModelListener *ml = m_model_listeners.Get(i);

			ml->OnHotlistItemMovedFrom(hotlist_item);
		}
	}
	else
	{
		if (hotlist_item != 0)
		{
			// Only mark as deleted when deleted from trash folder or as
			// a result of a CUT action.

			BOOL permanent = hotlist_item->GetIsInsideTrashFolder();
			if( permanent || m_bypass_trash_on_delete )
			{
				for(UINT32 i = 0; i < m_model_listeners.GetCount(); i++)
				{
					HotlistModelListener *ml = m_model_listeners.Get(i);
					ml->OnHotlistItemRemoved(hotlist_item, removed_as_child);
				}
			}
		}
	}

	// Update hash table m_unique_guids
	RemoveUniqueGUID(hotlist_item);
}


/******************************************************************************
 *
 * HotlistModel::BroadcastHotlistItemTrashed
 *
 *
 ******************************************************************************/
void HotlistModel::BroadcastHotlistItemTrashed(HotlistModelItem* item)
{
	if (item)
	{
		for(UINT32 i = 0; i < m_model_listeners.GetCount(); i++)
		{
			HotlistModelListener *ml = m_model_listeners.Get(i);
			ml->OnHotlistItemTrashed(item);
		}
	}
}

/******************************************************************************
 *
 * HotlistModel::BroadcastHotlistItemChanged
 *
 *
 ******************************************************************************/

void HotlistModel::BroadcastHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 flag_changed)
{
	if (item)
	{
		for(UINT32 i = 0; i < m_model_listeners.GetCount(); i++)
		{
			HotlistModelListener *ml = m_model_listeners.Get(i);
			ml->OnHotlistItemChanged(item, moved_as_child, flag_changed);
		}
	}
}

/******************************************************************************
 *
 * HotlistModel::ItemUnTrashed
 *
 * This will happen if an item is drag-dropped out of trash.
 * Note that in  this case, listeners will also receive an
 * OnHotlistItemChanged notification
 *
 ******************************************************************************/

void HotlistModel::ItemUnTrashed(HotlistModelItem* item)
{
	if (item)
	{
		for(UINT32 i = 0; i < m_model_listeners.GetCount(); i++)
		{
			HotlistModelListener *ml = m_model_listeners.Get(i);
			ml->OnHotlistItemUnTrashed(item);
		}
	}

}

/******************************************************************************
 *
 * HotlistModel::AddActiveFolder
 *
 *
 ******************************************************************************/

void HotlistModel::AddActiveFolder()
{
	// Actually: We can select a normal node as active "folder"
	if( GetActiveItemId() == -1 )
	{
		INT32 count = GetItemCount();
		for (INT32 i = 0; i < count; i++)
		{
			HotlistModelItem* item = GetItemByIndex(i);
			if( item && !item->GetIsInsideTrashFolder() )
			{
				SetActiveItemId( item->GetID(), FALSE);
				break;
			}
		}
	}
}


/*******************************************************************************
 *
 * HotlistModel::GetIsDescendantOf
 *
 * @return TRUE if descendant is descendant of param item
 *
 * Note: Depends on internal representation of TreeModel
 *
 *******************************************************************************/

BOOL HotlistModel::GetIsDescendantOf(HotlistModelItem* descendant,
									 HotlistModelItem* item)
{
	if (item && descendant)
	{
		if (   descendant->GetIndex() >  item->GetIndex()
			&& descendant->GetIndex() <= item->GetIndex() + item->GetSubtreeSize())
			{
				return TRUE;
			}

	}
	return FALSE;
}





/******************************************************************************
 *
 * HotlistModel::HasItems
 *
 * @return TRUE if there are items (not folders, separators) in model
 *
 ******************************************************************************/

BOOL HotlistModel::HasItems()
{
	for (INT32 i = 0; i < GetCount(); i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);
		if (item && !item->IsFolder() && !item->IsSeparator())
			return TRUE;
	}
	return FALSE;
}

/******************************************************************************
 *
 * HotlistModel::HasItems
 *
 * @return TRUE if there are items of param item_type in model
 *              (not folders, separators)
 *
 ******************************************************************************/

BOOL HotlistModel::HasItems(INT32 item_type)
{
	for (INT32 i = 0; i < GetCount(); i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);
		if (item && !item->IsFolder() && !item->IsSeparator() && item->GetType() == item_type)
			return TRUE;
	}
	return FALSE;
}

/******************************************************************************
 *
 * HotlistModel::HasItemsOrFolders
 *
 *
 ******************************************************************************/

BOOL HotlistModel::HasItemsOrFolders(INT32 item_type)
{
	for (INT32 i = 0; i < GetCount(); i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);
		if (item && (item->GetType() == item_type || item->IsFolder()))
			return TRUE;
	}
	return FALSE;
}

/******************************************************************************
 *
 * HotlistModel::SetActiveItemId
 *
 *
 ******************************************************************************/

void HotlistModel::SetActiveItemId( INT32 id,
									  BOOL save_change )
{
	if( m_active_folder_id != id )
	{
		HotlistModelItem* old_item = GetItemByID(m_active_folder_id);
		if(old_item)
			old_item->SetIsActive(FALSE);

		HotlistModelItem* item = GetItemByID(id);
		if(item)
			item->SetIsActive(TRUE);
		m_active_folder_id = id;

		if( save_change )
		{
			SetDirty( TRUE, 10000 );
		}
	}
}

/******************************************************************************
 *
 * HotlistModel::SetDirty
 *
 *
 ******************************************************************************/

void HotlistModel::SetDirty( BOOL state, INT32 timeout_ms )
{
	//m_current_search_item.Reset();

	if( m_is_dirty != state )
	{
		m_is_dirty = state;
		if( m_is_dirty )
		{
			// Save data when timeout expires
			if( !m_timer )
			{
				if (!(m_timer = OP_NEW(OpTimer, ())))
					return;
			}
			m_timer->SetTimerListener( this );
			m_timer->Start(timeout_ms);
		}
		else
		{
			// Stop timer (if any)
			if( m_timer )
			{
				m_timer->Stop();
			}
		}
	}
}


/******************************************************************************
 *
 * HotlistModel::OnTimeOut
 *
 *
 ******************************************************************************/

void HotlistModel::OnTimeOut(OpTimer* timer)
{
	OnChange();
}

void HotlistModel::OnChange()
{
	if( m_model_listeners.GetCount() )
	{
		if( m_is_dirty )
		{
			for(UINT32 i = 0; i < m_model_listeners.GetCount(); i++)
			{
				HotlistModelListener *ml = m_model_listeners.Get(i);
				if(ml->OnHotlistSaveRequest( this ))
					m_is_dirty = FALSE;
			}

			//Just in case:
			if(m_is_dirty)
			{
				OP_ASSERT(FALSE);
				// Noone has saved the model - resetting dirty anyway
				// must clear this, otherwise SetDirty will not trigger
				// another timer next time if the save failed somehow
				m_is_dirty = FALSE;
			}
		}
	}
}

/******************************************************************************
 *
 * HotlistModel::GetIndexList
 *
 *
 ******************************************************************************/

void HotlistModel::GetIndexList( INT32 index,
								 OpINT32Vector& index_list,
								 BOOL test_sibling,
								 INT32 probe_depth,
								 OpTypedObject::Type type )
{
	while( 1 )
	{
		HotlistModelItem* item = GetItemByIndex(index);
		if( !item )
		{
			break;
		}

		if( item->GetType() == type )
		{
			index_list.Add( index );
		}

		if( probe_depth > 0 )
		{
			int num_children = GetSubtreeSize(index);
			if( num_children > 0 )
			{
				GetIndexList( index+1, index_list, TRUE, probe_depth-1, type );
			}
		}

		if( !test_sibling )
		{
			break;
		}

		index = GetSiblingIndex( index );
		if( index == -1 )
		{
			break;
		}
	}
}


/******************************************************************************
 *
 * HotlistModel::IsFolderAtIndex
 *
 *
 ******************************************************************************/

BOOL HotlistModel::IsFolderAtIndex( INT32 index )
{
	if( index >= 0 && index < GetItemCount() )
	{
		return GetItemByPosition(index)->GetType() == OpTypedObject::FOLDER_TYPE;
	}

	return FALSE;
}

/******************************************************************************
 *
 * HotlistModel::IsChildOf
 *
 *
 ******************************************************************************/

BOOL HotlistModel::IsChildOf( INT32 parent_index,
							  INT32 candidate_child_index )
{
	if(IsFolderAtIndex(parent_index) && parent_index != candidate_child_index)
	{
		if( candidate_child_index>=0 && candidate_child_index<GetItemCount() )
		{
			candidate_child_index = GetItemParent(candidate_child_index);
			while( candidate_child_index != -1 )
			{
				if( candidate_child_index == parent_index )
				{
					return TRUE;
				}
				candidate_child_index = GetItemParent(candidate_child_index);
			}
		}
	}

	return FALSE;
}



/******************************************************************************
 *
 * HotlistModelItem::GetItemByType
 *
 *
 ******************************************************************************/

// static
HotlistModelItem *HotlistModel::GetItemByType( OpTreeModelItem *item )
{
	if( item )
	{
		switch(item->GetType())
		{
			case OpTypedObject::BOOKMARK_TYPE:
			case OpTypedObject::FOLDER_TYPE:
			case OpTypedObject::NOTE_TYPE:
			case OpTypedObject::GADGET_TYPE:
			case OpTypedObject::GROUP_TYPE:
			case OpTypedObject::SEPARATOR_TYPE:
			case OpTypedObject::CONTACT_TYPE:
			case OpTypedObject::UNITE_SERVICE_TYPE:
				return (HotlistModelItem*)item;
		}
	}
	return 0;
}


/******************************************************************************
 *
 * HotlistModel::GetIsRootFolder
 *
 *
 ******************************************************************************/
// MOVE!!
// static
BOOL HotlistModel::GetIsRootFolder( INT32 id )
{
	if( id >= BookmarkRoot && id <= UniteServicesRoot )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}



/******************************************************************************
 *
 * HotlistModel::GetByURL
 *
 *
 ******************************************************************************/
// TODO: Use history interface
HotlistModelItem *HotlistModel::GetByURL( const URL &document_url )
{
	// Content from UniName must always be copied immediately, as a temporary buffer is used.
	OpString document_name;
	document_url.GetAttribute(URL::KUniName_Username_Password_Hidden, document_name);

	INT32 count = GetItemCount();

	for(INT32 i = 0; i < count; i++ )
	{
		HotlistModelItem *item = GetItemByIndex(i);
		if( item->IsBookmark() || item->IsNote())
		{
			const OpStringC &resolved_url = item->GetResolvedUrl();
			if( resolved_url.HasContent() )
			{
				if( IsSameHostname( document_name.CStr(), resolved_url.CStr() ) )
				{
					URL url = g_url_api->GetURL(resolved_url.CStr());

					if( url == document_url )
					{
						return item;
					}
				}
			}
		}
	}
	return NULL;
}

HotlistModelItem* HotlistModel::GetByName( const OpStringC& name )
{
	for (INT32 i = 0; i < GetItemCount(); i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);
		if (!item->IsSeparator() && item->GetName().HasContent())
		{
			if (item->GetName().Compare(name) == 0)
				return item;
		}
	}
	return 0;
}

HotlistModelItem *HotlistModel::GetByUniqueGUID(const OpStringC8 &unique_guid)
{
	OpString tmp;
	if (!OpStatus::IsSuccess(tmp.Set(unique_guid)))
		return NULL;
	return GetByUniqueGUID(tmp);
}

/******************************************************************************
 *
 * HotlistModel::GetByUniqueGUID
 *
 *
 ******************************************************************************/

HotlistModelItem *HotlistModel::GetByUniqueGUID(const OpStringC &unique_id)
{
	if (unique_id.IsEmpty())
		return NULL;

	HotlistModelItem *item;

	if (OpStatus::IsSuccess(m_unique_guids.GetData(unique_id.CStr(), &item)) && item)
	{
		return item;
	}

#ifdef DEBUG_HOTLIST_HASH
	UINT32 count = GetItemCount();

	for (UINT32 i = 0; i < count; i++)
	{
		item = GetItemByIndex(i);

		if (item->GetUniqueGUID().Compare(unique_id.CStr()) == 0)
		{
			OpString8 guid;
			guid.Set(unique_id.CStr());
			fprintf(stderr, " GUID %s not in hash but in model \n", guid.CStr());
			return item;
		}
	}
#endif // defined(DEBUG_HOTLIST_HASH)

	return NULL;
}


/******************************************************************************
 *
 * HotlistModel::GetByNickname
 *
 * Note: contacts only
 *
 ******************************************************************************/

HotlistModelItem* HotlistModel::GetByNickname(const OpStringC &nick)
{
	if (nick.IsEmpty())
		return NULL;

	INT32 count = GetItemCount();

	for (INT32 i = 0; i < count; i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);
		if (item->HasNickname(nick))
			return item;
	}

	return NULL;
}



/******************************************************************************
 *
 * HotlistModel::GetSortedList
 *
 *
 ******************************************************************************/

void HotlistModel::GetSortedList( INT32 index, INT32 start_at, INT32 column, BOOL ascending, OpVector<HotlistModelItem>& items)
{

	// Determine number of items
	int num_item = 0;

	int i = index;
	while( i != -1 )
	{
		HotlistModelItem *hli = GetItemByType(GetItemByIndex(i));
		if( hli && !hli->GetIsTrashFolder() )
		{
			num_item ++;
		}
		i = GetSiblingIndex(i);
	}

	if( num_item == 0 )
	{
		return;
	}

	// Allocate list and fill it.
	HotlistModelItem **list = OP_NEWA(HotlistModelItem *, num_item+1);
	if( !list )
	{
		return;
	}

	i = index;
	int j=0;
	while( i != -1 )
	{
		HotlistModelItem *hli = GetItemByType(GetItemByIndex(i));
		if( hli && !hli->GetIsTrashFolder() )
		{
			list[j] = hli;
			j++;
		}
		i = GetSiblingIndex(i);
	}
	list[num_item] = 0;

	if (column != -1)
	{
		// Sort list. This should not be done here but rather reuse the
		// sort code of the bookmark tree lists.
		m_sort_column    = column;
		m_sort_ascending = ascending;
		qsort( list, num_item, sizeof(HotlistModelItem *), CompareHotlistModelItem );
	}
	else if( !ascending )
	{
		for( i=num_item-1; i>=start_at; i--)
		{
			items.Add(list[i]);
		}
		OP_DELETEA(list);
		return;
	}

	for( i=start_at; i<num_item; i++)
	{
		items.Add(list[i]);
	}

	OP_DELETEA(list);
}

/******************************************************************************
 *
 * HotlistModel::compareHotlistModelItem
 *
 *
 ******************************************************************************/

// static
int HotlistModel::CompareHotlistModelItem( const void *item1,
										   const void *item2,
										   INT32 model_type )
{
	INT32 result = CompareHotlistModelItem( m_sort_column, *(HotlistModelItem**)item1, *(HotlistModelItem**)item2, model_type );
	return m_sort_ascending ? result : -result;

}



/******************************************************************************
 *
 * HotlistModel::CompareHotlistModelItem
 *
 *
 ******************************************************************************/

// static
int HotlistModel::CompareHotlistModelItem( const void *item1,
										   const void *item2)
{

	INT32 result = CompareHotlistModelItem( m_sort_column, *(HotlistModelItem**)item1, *(HotlistModelItem**)item2);
	return m_sort_ascending ? result : -result;
}

/******************************************************************************
 *
 * HotlistModel::CompareHotlistModelItem
 *
 *
 ******************************************************************************/

// static
INT32 HotlistModel::CompareHotlistModelItem( int column,
											 HotlistModelItem* hli1,
											 HotlistModelItem* hli2 )
{
	if( !hli1 || !hli2 )
	{
		return 0;
	}

	OpString s1, s2;

	const uni_char *p1 = 0;
	const uni_char *p2 = 0;

	if( column == 0 ) // Name
		{
			p1 = hli1->GetName().CStr();
			p2 = hli2->GetName().CStr();
		}
	else if( column == 1 ) // Nickname
		{
			p1 = hli1->GetShortName().CStr();
			p2 = hli2->GetShortName().CStr();
		}
	else if( column == 2 ) // Address
		{
			p1 = hli1->GetUrl().CStr();
			p2 = hli2->GetUrl().CStr();
		}
	else if( column == 3 ) // description
		{
			p1 = hli1->GetDescription().CStr();
			p2 = hli2->GetDescription().CStr();
		}
	else if( column == 4 ) // created
		{
			s1.AppendFormat(UNI_L("%08x"),hli1->GetCreated());
			s2.AppendFormat(UNI_L("%08x"),hli2->GetCreated());
			p1 = s1.CStr();
			p2 = s2.CStr();
			//p1 = hli1->GetCreatedString().CStr();
			//p2 = hli2->GetCreatedString().CStr();
		}
	else if( column == 5 ) // visited
		{
			s1.AppendFormat(UNI_L("%08x"),hli1->GetVisited());
			s2.AppendFormat(UNI_L("%08x"),hli2->GetVisited());
			p1 = s1.CStr();
			p2 = s2.CStr();
			//p1 = hli1->GetVisitedString().CStr();
			//p2 = hli2->GetVisitedString().CStr();
		}



	if( hli1->GetIsTrashFolder() || hli2->GetIsTrashFolder() )
	{
		if( hli1->GetIsTrashFolder() )
		{
			return 1;
		}
		else if( hli2->GetIsTrashFolder() )
		{
			return -1;
		}
	}

#ifdef WEBSERVER_SUPPORT
	// Root service should always be on top - even when sorted
	if (hli1->IsRootService())
		return -1;
	else if (hli2->IsRootService())
		return 1;
#endif

	if( hli1->IsFolder() && !hli2->IsFolder() )
	{
		return -1;
	}
	else if( !hli1->IsFolder() && hli2->IsFolder() )
	{
		return 1;
	}
	else if( hli1->IsGroup() && !hli2->IsGroup() )
	{
		return -1;
	}
	else if( !hli1->IsGroup() && hli2->IsGroup() )
	{
		return 1;
	}
	else
	{
		if( !p1 && !p2 )
		{
			// fallback
			p1 = hli1->GetName().CStr();
			p2 = hli2->GetName().CStr();
			if( !p1 && !p2 )
			{
				return 0;
			}
		}

		if( !p1 )
		{
			return 1;
		}
		else if( !p2 )
		{
			return -1;
		}
		else
		{
			int ans;
			TRAPD(err, ans = g_oplocale->CompareStringsL(p1, p2, -1, FALSE/*TRUE*/));
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: handle error
			if (OpStatus::IsError(err))
			{
				ans = uni_stricmp(p1,p2);
			}
			return ans; //uni_strcmp( p1, p2 );
		}
	}
}

/******************************************************************************
 *
 * HotlistModel::GetCompareHotlistModelItem
 *
 *
 ******************************************************************************/

// static
INT32 HotlistModel::CompareHotlistModelItem( int column,
											 HotlistModelItem* hli1,
											 HotlistModelItem* hli2,
											 INT32 model_type )
{
	if( !hli1 || !hli2 )
	{
		return 0;
	}

	OpString s1, s2;

	const uni_char *p1 = 0;
	const uni_char *p2 = 0;

	if (model_type == HotlistModel::ContactRoot)
	{
		if( column == 0 ) // Name
		{
			p1 = hli1->GetName().CStr();
			p2 = hli2->GetName().CStr();
		}
		else if( column == 1 ) // Mail
		{
			p1 = hli1->GetMail().CStr();
			p2 = hli2->GetMail().CStr();
		}
		else if (column == 2) // Phone
		{
			p1 = hli1->GetPhone().CStr();
			p2 = hli2->GetPhone().CStr();
		}
	}
	else
	{
		if( column == 0 ) // Name
		{
			p1 = hli1->GetName().CStr();
			p2 = hli2->GetName().CStr();
		}
		else if( column == 1 ) // Nickname
		{
			p1 = hli1->GetShortName().CStr();
			p2 = hli2->GetShortName().CStr();
		}
		else if( column == 2 ) // Address
		{
			p1 = hli1->GetUrl().CStr();
			p2 = hli2->GetUrl().CStr();
		}
		else if( column == 3 ) // description
		{
			p1 = hli1->GetDescription().CStr();
			p2 = hli2->GetDescription().CStr();
		}
		else if( column == 4 ) // created
		{
			s1.AppendFormat(UNI_L("%08x"),hli1->GetCreated());
			s2.AppendFormat(UNI_L("%08x"),hli2->GetCreated());
			p1 = s1.CStr();
			p2 = s2.CStr();
			//p1 = hli1->GetCreatedString().CStr();
			//p2 = hli2->GetCreatedString().CStr();
		}
		else if( column == 5 ) // visited
		{
			s1.AppendFormat(UNI_L("%08x"),hli1->GetVisited());
			s2.AppendFormat(UNI_L("%08x"),hli2->GetVisited());
			p1 = s1.CStr();
			p2 = s2.CStr();
			//p1 = hli1->GetVisitedString().CStr();
			//p2 = hli2->GetVisitedString().CStr();
		}

	}

	if( hli1->GetIsTrashFolder() || hli2->GetIsTrashFolder() )
	{
		if( hli1->GetIsTrashFolder() )
		{
			return 1;
		}
		else if( hli2->GetIsTrashFolder() )
		{
			return -1;
		}
	}

#ifdef WEBSERVER_SUPPORT
	// Root service should always be on top - even when sorted
	if (hli1->IsRootService())
	{
		return -1;
	}
	else if (hli2->IsRootService())
	{
		return 1;
	}
#endif

	if (model_type == HotlistModel::UniteServicesRoot)
	{
		if (hli1->IsSeparator() && hli1->GetSeparatorType() == SeparatorModelItem::TRASH_SEPARATOR )
		{
			if (!hli2->GetIsTrashFolder())
			{
					return 1;
			}
		}
		else if (hli2->IsSeparator() && hli2->GetSeparatorType() == SeparatorModelItem::TRASH_SEPARATOR )
		{
			if (!hli1->GetIsTrashFolder())
			{
				return -1;
			}
		}
	}


	if( hli1->IsFolder() && !hli2->IsFolder() )
	{
		return -1;
	}
	else if( !hli1->IsFolder() && hli2->IsFolder() )
	{
		return 1;
	}
	else if( hli1->IsGroup() && !hli2->IsGroup() )
	{
		return -1;
	}
	else if( !hli1->IsGroup() && hli2->IsGroup() )
	{
		return 1;
	}
	else
	{
		if( !p1 && !p2 )
		{
			// fallback
			p1 = hli1->GetName().CStr();
			p2 = hli2->GetName().CStr();
			if( !p1 && !p2 )
			{
				return 0;
			}
		}

		if( !p1 )
		{
			return 1;
		}
		else if( !p2 )
		{
			return -1;
		}
		else
		{
			int ans;
			TRAPD(err, ans = g_oplocale->CompareStringsL(p1, p2, -1, FALSE/*TRUE*/));
			OP_ASSERT(OpStatus::IsSuccess(err)); // FIXME: handle error
			if (OpStatus::IsError(err))
			{
				ans = uni_stricmp(p1,p2);
			}
			return ans; //uni_strcmp( p1, p2 );
		}
	}
}

/******************************************************************************
 *
 * HotlistModel::AddUniqueGUID
 *
 *
 ******************************************************************************/
OP_STATUS HotlistModel::AddUniqueGUID(HotlistModelItem* item)
{
	if (!item)
		return OpStatus::ERR;

	// Make sure it not already in the list since we don't want to add it twice
	if (m_unique_guids.Contains(item->GetUniqueGUID().CStr()))
	{
		HotlistModelItem* dummy;
		m_unique_guids.Remove(item->GetUniqueGUID().CStr(), &dummy);
		OP_ASSERT(dummy);
	}

#ifdef DEBUG_HOTLIST_HASH
	if (GetModelType() == HotlistModel::BookmarkRoot
		&& !m_temporary
		&& !g_hotlist_manager->IsClipboard(this))
		{
			OpString8 guid;
			guid.Set(item->GetUniqueGUID().CStr());
			fprintf(stderr, " -------> [model=%p]Adding Unique GUID %s to HASH \n", this, guid.CStr());
		}
#endif
	return m_unique_guids.Add(item->GetUniqueGUID().CStr(), item);
}


/******************************************************************************
 *
 * HotlistModel::RemoveUniqueGUID
 *
 *
 ******************************************************************************/

OP_STATUS HotlistModel::RemoveUniqueGUID(HotlistModelItem* item)
{
	if (!item)
		return OpStatus::ERR;

	if (!item->GetUniqueGUID().HasContent())
		return OpStatus::ERR;

	HotlistModelItem *removed_item;
#ifdef DEBUG_HOTLIST_HASH
	OpString8 guid;
	guid.Set(item->GetUniqueGUID().CStr());
	if (GetModelType() == HotlistModel::BookmarkRoot
		&& !m_temporary
		&& !g_hotlist_manager->IsClipboard(this))
	{
		OpString8 guid;
		guid.Set(item->GetUniqueGUID().CStr());
		fprintf( stderr, " ------> [model=%p]Removing Unique GUID %s from HASH \n", this, guid.CStr());
	}
#endif

	return m_unique_guids.Remove(item->GetUniqueGUID().CStr(), &removed_item);
}


/******************************************************************************
 *
 * HotlistModel::BroadcastHotlistItemAdded
 *
 * @param HotlistModelItem* hotlist_item
 *
 * Broadcast add to listeners.
 * Note: When an item is moved, this is actually implemented as a remove and then
 * an add. So if m_is_moving_items is set here, we do a BroadcastHotlistItemChanged
 * instead.
 * Added item is also added to the hash table mapping GUIDs to items
 *
 ******************************************************************************/

void HotlistModel::BroadcastHotlistItemAdded(HotlistModelItem* hotlist_item, BOOL moved_as_child)
{
#ifdef DEBUG_HOTLIST_HASH
	if (!g_hotlist_manager->IsClipboard(this) && !m_temporary)
	{
		OpString8 name;
		OpString8 guid;
		name.Set(hotlist_item->GetName().CStr());
		guid.Set(hotlist_item->GetUniqueGUID().CStr());
		fprintf( stderr, "\n HotlistModel[%p]::BroadcastHotlistItemAdded(%s)[%s]\n", this, name.CStr(), guid.CStr());
	}
#endif

#if 0
	if (hotlist_item
		&& hotlist_item->GetParentFolder()
		&& hotlist_item->GetParentFolder()->GetIsTrashFolder())
		{
			// Bookmark is removed from history when in trash
			BroadcastHotlistItemTrashed(hotlist_item);
		}
#endif

	BOOL guid_added = FALSE;

	if( m_is_moving_items )
	{

		// Do not tell external listeners that an item has been added
		// when we move from one location to another (we move with: copy item -
		// insert copied item elsewhere - delete original item)

		// TODO:
		BroadcastHotlistItemChanged(hotlist_item, moved_as_child, HotlistModel::FLAG_MOVED_TO);
	}
	else
	{
		OP_ASSERT(!moved_as_child);

		if (hotlist_item != 0)
		{
			// If there is no unique Guid then generate it
			if (hotlist_item->GetUniqueGUID().IsEmpty())
			{
 				OpString guid;

 				// generate a default unique ID
 				if(OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
 				{
 					hotlist_item->SetUniqueGUID(guid.CStr(), GetModelType());
					guid_added = TRUE;
 				}
			}

			for(UINT32 i = 0; i < m_model_listeners.GetCount(); i++)
			{
				HotlistModelListener *ml = m_model_listeners.Get(i);
				ml->OnHotlistItemAdded(hotlist_item);
			}
		}
	}

	if (!guid_added)
	{
		// Update hash table m_unique_guids
		AddUniqueGUID(hotlist_item);
	}
}

/******************************************************************************
 *
 * HotlistModel::CompareItems
 *
 *
 ******************************************************************************/

INT32 HotlistModel::CompareItems( int column,
								  OpTreeModelItem* item1,
								  OpTreeModelItem* item2 )
{
	return CompareHotlistModelItem( column, (HotlistModelItem*)item1, (HotlistModelItem*)item2, GetModelType() );
}

void HotlistModelIterator::Reset()
{
	m_recent_num = 0;
	m_sorted_index = 0;
}

CollectionModelItem* HotlistModelIterator::Next()
{
	OP_ASSERT(GetSortType() != SORT_NONE);
	if (m_recent && m_recent_num > m_recent_count)
		return NULL;

	HotlistModelItem* item = m_model->GetItemByIndex(m_sorted_model.GetModelIndexByIndex(m_sorted_index));
	m_sorted_index++;

	if (m_recent)
	{
		while (item && item->GetPartnerID().HasContent())
		{
			item = m_model->GetItemByIndex(m_sorted_model.GetModelIndexByIndex(m_sorted_index));
			m_sorted_index++;
		}
	}

	m_recent_num++;
	return item;
}

int HotlistModelIterator::GetCount(bool include_folders)
{
	int count = 0;
	int start_idx = 0;
	int total_count = 0;

	if (m_folder)
	{
		if (m_folder->GetChildItem())
		{
			start_idx = m_folder->GetChildItem()->GetIndex();
			total_count = m_folder->GetSubtreeSize();
		}
		else
		{
			return 0;
		}
	}
	else
	{
		total_count = static_cast<HotlistModel*>(this->GetModel())->GetCount();
	}

	if (include_folders)
	{
		return total_count;
	}

	for (INT32 i = start_idx; i < total_count + start_idx; i++)
	{
		HotlistModelItem* item = m_model->GetItemByIndex(i);
		if (item->GetIsTrashFolder())
		{
			i += item->GetSubtreeSize();
			continue;
		}

		if (item && !item->IsSeparator() && !item->IsFolder()  && (!IsRecent() || !item->GetPartnerID().HasContent()))
		{
			count++;
		}
	}

	if (IsRecent())
		return count > m_recent_count ? m_recent_count : count;
	else
		return count;
}

CollectionModelIterator* HotlistModel::CreateModelIterator(CollectionModelItem* folder, CollectionModelIterator::SortType type, bool recent)
{
	if (recent)
		type = CollectionModelIterator::SORT_BY_CREATED;

	CollectionModelIterator* it = OP_NEW(HotlistModelIterator, (this, static_cast<HotlistModelItem*>(folder), type, recent));
	if (it)
	{
		it->Init();
	}
	return it;
}

void HotlistModelIterator::Init()
{
	if (m_folder && (m_folder->GetIsTrashFolder() || m_folder->GetIsInsideTrashFolder()))
	{
		m_sorted_model.SetIsTrashFolderOrFolderInTrashFolder();
	}

	INT32 idx = 0;
	if (m_folder && m_folder->GetChildItem())
		idx = m_folder->GetChildItem()->GetIndex();

	m_sorted_model.SetSortListener(this);
	m_sorted_model.SetModelParameters(idx, GetCount(TRUE));
	m_sorted_model.SetModel(m_model); // Init() is called from SetModel()
}

int HotlistModelIterator::GetPosition(CollectionModelItem* item)
{
	return m_sorted_model.GetIndexByModelIndex(static_cast<HotlistModelItem*>(item)->GetIndex());
}

INT32 HotlistModelIterator::OnCompareItems(OpTreeModel* tree_model, OpTreeModelItem* item0, OpTreeModelItem* item1)
{
	INT32 sort_by_column = 0;
	if (GetSortType() == CollectionModelIterator::SORT_BY_NAME)
		sort_by_column = 0;
	else if (GetSortType() == CollectionModelIterator::SORT_BY_CREATED)
		sort_by_column = 4;
	INT32 result = m_model->CompareItems(sort_by_column, ((TreeViewModelItem*)item0)->GetModelItem(), ((TreeViewModelItem*)item1)->GetModelItem());

	return (GetSortType() == CollectionModelIterator::SORT_BY_NAME) ? result : -result;
}

// Subclassed because there's an assertion that will trigger every time this is called for a folder
// since we're using it and not including folders
TreeViewModelItem *ViewModel::GetItemByModelItem(OpTreeModelItem* model_item)
{
	if (!model_item)
		return NULL;

	void *item = NULL;

	m_hash_table.GetData(model_item, &item);

	return (TreeViewModelItem*)item;
}

void ViewModel::Init()
{
	RemoveAll();

	// m_count includes folders and separators
	for (INT32 i = 0; i < m_count; i++, m_start_idx++)
	{
		HotlistModelItem* hitem = static_cast<HotlistModelItem*>(m_model->GetItemByPosition(m_start_idx));

		// Don't include items in trash unless we're representing a folder that is inside trash
		if (hitem && !hitem->IsSeparator() && !hitem->IsFolder() && (m_is_trash_folder_or_folder_in_trash || !hitem->GetIsInsideTrashFolder()))
		{
			TreeViewModelItem* item = OP_NEW(TreeViewModelItem, (hitem));
			AddLast(item);
		}
	}

	if (GetSortListener())
		Sort();
}

void ViewModel::SetModelParameters(INT32 start_idx, INT32 count)
{
	m_start_idx = start_idx;
	m_count = count;
}


