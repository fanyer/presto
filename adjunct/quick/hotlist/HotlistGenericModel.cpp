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

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/hotlist/HotlistGenericModel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/DesktopGadgetManager.h"
#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/quick/panels/GadgetsPanel.h"


#include "modules/locale/oplanguagemanager.h"

#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"
#include "modules/img/image.h"
#include "modules/img/src/imagemanagerimp.h"
#include "modules/pi/OpLocale.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpPainter.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkin.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/util/timecache.h"


// TEMP
#include "adjunct/quick/managers/SyncManager.h"

#include <time.h>

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


/***********************************************************************************
 **
 **	HotlistGenericModel
 **
 ***********************************************************************************/


HotlistGenericModel::HotlistGenericModel( INT32 type, BOOL temp )
:	HotlistModel(type,temp),
	m_email_buffer(0),
	m_trash_separator_id(-1),
	m_trash_folder_id(-1)
#ifdef WEBSERVER_SUPPORT
	, m_root_service(0)
#endif
{
	if( m_type == BookmarkRoot 
#ifdef NOTES_USE_URLICON
		|| m_type == NoteRoot
#endif
		)
		{
			if (!m_temporary)
				g_favicon_manager->AddListener(this);
		}
	
	m_trash_separator_id = -1;
}


HotlistGenericModel::~HotlistGenericModel()
{
	if( g_favicon_manager && (m_type == BookmarkRoot || m_type == NoteRoot) && !m_temporary)
	{
		g_favicon_manager->RemoveListener(this);
	}

	InvalidateEmailLookupTable();
}

struct HotlistModelListEntry
{
	OpString *name;
	HotlistModelItem* item;
};


/******************************************************************************
 *
 * HotlistGenericModel::CreateEmailLookupTable
 *
 * Note: Only contacts
 *
 ******************************************************************************/

void HotlistGenericModel::CreateEmailLookupTable()
{
	if(m_email_buffer)
		return;

	OpString buf;
	OpAutoVector<HotlistModelListEntry> secondary_list;

	if (!(m_email_buffer = OP_NEW(HotlistGenericModelEmailLookupBuffer, ())))
		return;

	INT32 count = GetItemCount();
	INT32 i;

	for (i = 0; i < count; i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);
		if (item->GetMail().IsEmpty())
			continue;

		if (item->GetMail().FindFirstOf(',') == KNotFound)
		{
			// Only one primary address. We do not have to save a copy of the string
			/*OP_STATUS rc = */m_email_buffer->table()->Add( item->GetMail().CStr(), item );
			/*
			if( rc == OpStatus::ERR )
			{
				OpString8 tmp;
				tmp.Set(item->GetMail());
				printf("Primary 1 add failed: %s\n", tmp.CStr());
			}
			else
			{
				OpString8 tmp;
				tmp.Set(item->GetMail());
				printf("Primary 1 added: %s\n", tmp.CStr());
			}
			*/
		}
		else
		{
			static const uni_char* delimiter = UNI_L(" ,");
			buf.Set(item->GetMail().CStr());
			if( buf.CStr() )
			{
				BOOL first = TRUE;
				const uni_char* mail_token = uni_strtok(buf.CStr(), delimiter);
				while (mail_token)
				{
					// We have to save a copy of mail_token
					OpString* s = OP_NEW(OpString, ());
					if (s)
					{
						s->Set(mail_token);
						m_email_buffer->text()->Add(s);

						// The first is the primary address
						if( first )
						{
							first = FALSE;
							/*OP_STATUS rc = */m_email_buffer->table()->Add( s->CStr(), item );
							/*
							if( rc == OpStatus::ERR )
							{
								OpString8 tmp;
								tmp.Set(mail_token);
								printf("Primary 2 add failed: %s\n", tmp.CStr());
							}
							else
							{
								OpString8 tmp;
								tmp.Set(mail_token);
								printf("Primary 2 added: %s\n", tmp.CStr());
							}
							*/
						}
						else
						{
							// All secondary addresses must be added after all primary addresses
							// to avoid rejected primary addresses (duplicates).
							HotlistModelListEntry* entry = OP_NEW(HotlistModelListEntry, ());
							if (entry)
							{
								entry->name = s;
								entry->item = item;
								secondary_list.Add( entry );
							}
						}
					}
					mail_token = uni_strtok(NULL, delimiter);
				}
			}
		}
	}

	count = secondary_list.GetCount();
	for (i = 0; i < count; i++)
	{
		HotlistModelListEntry* entry = secondary_list.Get(i);

		/*OP_STATUS rc = */m_email_buffer->table()->Add( entry->name->CStr(), entry->item );
		/*
		if( rc == OpStatus::ERR )
		{
			OpString8 tmp;
			tmp.Set(entry->name.CStr());
			printf("Secondary add failed: %s\n", tmp.CStr());
		}
		*/
	}
}


/******************************************************************************
 *
 * HotlistGenericModel::InvalidateEmailLookupTable
 *
 * Contacts
 *
 ******************************************************************************/

void HotlistGenericModel::InvalidateEmailLookupTable()
{
	if( m_email_buffer )
	{
		OP_DELETE(m_email_buffer);
		m_email_buffer = 0;
	}
}


/******************************************************************************
 *
 * HotlistGenericModel::OnFavIconAdded
 *
 * Notify UI and Link about favicon add
 *
 ******************************************************************************/
// Must be disabled when we are processing incoming items
// but that should be ok since sync bm items won't listen to hotlist then
void HotlistGenericModel::OnFavIconAdded(const uni_char* document_url, 
								  const uni_char* image_path)
{
	// Dont do notifications about icons added during sync -> Huh?
	if (GetIsSyncing())
	{
		return;
	}
	if (!document_url)
	{
		return;
	}

#ifdef NOTES_USE_URLICON
	if (GetModelType() == NoteRoot)
	{
		for (INT32 i = 0; i < GetCount(); i++)
		{
			HotlistModelItem* item = GetItemByIndex(i);
			if (item->IsNote())
			{
				if (item->GetUrl().Compare(document_url) == 0 && !m_temporary)
				{
					item->Change(TRUE);
					BroadcastHotlistItemChanged(item, FALSE, HotlistGenericModel::FLAG_ICON);
				}
			}
		}
	}
#endif
}

/**************************************************************************
 *
 * HotlistGenericModel::AddGadgetToModel
 *
 * Add the local gadget before the trash separator
 *
 **************************************************************************/

void HotlistGenericModel::AddGadgetToModel(HotlistModelItem* item, 
								  INT32* got_index)
{
	if (GetModelType() == HotlistGenericModel::UniteServicesRoot)
	{
		// Add trash separator if its not already there
		if (m_trash_separator_id == -1)
		{
			AddSpecialSeparator(SeparatorModelItem::TRASH_SEPARATOR);
		}

		INT32 trash_separator_idx = GetItemByID(m_trash_separator_id)->GetIndex();
		m_current_index = InsertBefore(item, trash_separator_idx);
		BroadcastHotlistItemAdded(item, FALSE);
		if (got_index)
			*got_index = m_current_index;
	}
}

/***********************************************************************************
 *
 * HotlistGenericModel::HandleGadgetCallback
 *
 * @param OpDocumentListener::GadgetShowNotificationCallback* gadget -
 *
 * @param OpDocumentListener::GadgetShowNotificationCallback::Reply reply -
 *
 *
 *
 ***********************************************************************************/

void HotlistGenericModel::HandleGadgetCallback(OpDocumentListener::GadgetShowNotificationCallback* gadget, 
										OpDocumentListener::GadgetShowNotificationCallback::Reply reply)
{
	if (!gadget)
	{
		return;
	}

	for (INT32 i = 0; i < GetCount(); i++)
	{
		HotlistModelItem* item = (GadgetModelItem*)GetItemByIndex(i);
		if (item && item->IsUniteService())
		{
			DesktopGadget* gadget_win = ((GadgetModelItem*)item)->GetDesktopGadget();
			if (gadget_win && ((GadgetModelItem*)item)->GetOpGadget() == gadget)
			{
				if (reply != OpDocumentListener::GadgetShowNotificationCallback::REPLY_IGNORED)
				{
#ifdef WEBSERVER_SUPPORT
					static_cast<UniteServiceModelItem*>(item)->OpenOrFocusServicePage();
#endif
				}
				gadget_win->DoNotificationCallback(gadget, reply);
#ifdef WEBSERVER_SUPPORT
				static_cast<UniteServiceModelItem*>(item)->SetNeedsAttention(FALSE);
#endif
			}
		}
	}
}

/******************************************************************************
 *
 * HotlistGenericModel::AddItem
 *
 * Creates empty item and adds it to the model
 *
 ******************************************************************************/

HotlistModelItem* HotlistGenericModel::AddItem( OpTypedObject::Type type, OP_STATUS& status, BOOL parsed_item )
{
	HotlistModelItem* item = CreateItem(type, status, FALSE);
	if (item)
	{
		if( !AddItem(item, /* allow_dups = (for now) */TRUE, parsed_item ) )
		{
			OP_DELETE(item);
			item = 0;
		}
	}

	return item;
}

/**************************************************************************
 **
 ** HotlistGenericModel::AddItem
 **
 ** @param HotlistModelItem* item - item to add to model
 ** @param BOOL allow_dups - if FALSE, check for duplicate, and don't add
 **                          if same URL
 ** @param BOOL parsed_item - True if item is added from parsed/imported file
 **
 **
 ** Will not add the item if an item with the given GUID already exists
 **
 ** Callers: Parser, Add bookmark dialog
 **
 ***************************************************************************/
// currently allow_dups is FALSE for import only, not parsing

HotlistModelItem* HotlistGenericModel::AddItem(HotlistModelItem* item, BOOL allow_dups, BOOL parsed_item )
{
	if (!item)
		return 0;

	if (parsed_item
		&& item->GetUniqueGUID().HasContent() &&
		GetByUniqueGUID(item->GetUniqueGUID()) != NULL)
		{
			return 0;
		}
		
	HotlistModelItem* parent = GetItemByIndex(m_parent_index);

	if (parent && !parent->GetSubfoldersAllowed() && item->IsFolder())
		return 0;
	
	//	if (parent && parent->GetMaxItems() > 0 && parent->GetChildCount() >= parent->GetMaxItems())
	if (parent && !parent->GetSeparatorsAllowed() && item->IsSeparator())
		return 0;
	
	if (item->GetUniqueGUID().IsEmpty())
	{
		OpString guid;
		
		// generate a unique ID
		if(OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
		{
			item->SetUniqueGUID(guid.CStr(), GetModelType());
		}
	}

#ifdef WEBSERVER_SUPPORT
	if (m_type == HotlistGenericModel::UniteServicesRoot)
		{
#ifdef WEBSERVER_SUPPORT
			if (item->IsRootService())
			{
				OP_ASSERT(!GetRootService());
				m_current_index = AddFirst(item, -1);
				SetRootService(static_cast<UniteServiceModelItem*>(item));
				AddRootServiceSeparator(item);

			}
			else 
#endif
				if (!parent)
			{
				if (item->IsSeparator())
				{
					m_current_index = InsertAfter(item, m_current_index); // s.d.
				}
				else if (item->GetIsTrashFolder())
				{
					if (m_trash_separator_id != -1)
					{
						INT32 trash_separator_idx = GetItemByID(m_trash_separator_id)->GetIndex();
						m_current_index = InsertAfter(item, trash_separator_idx);
					}
					else
					{
						m_current_index = AddLast(item, -1);
					}
				}
				else
				{
					AddGadgetToModel(item, &m_current_index);
				}
			}
		}

	if (!item->GetModel())
	{
#endif // WEBSERVER_SUPPORT
		m_current_index = AddLast(item, m_parent_index);
#ifdef WEBSERVER_SUPPORT
	}
#endif // WEBSERVER_SUPPORT
	if (!m_temporary)
	{
		BroadcastHotlistItemAdded(item, FALSE);
	}
	
	if( item->IsFolder() )
	{
		m_parent_index = m_current_index;
	}

#ifdef WEBSERVER_SUPPORT
	if (item->IsUniteService() && !item->IsRootService() && static_cast<UniteServiceModelItem*>(item)->NeedsToAutoStart())
	{
		if(g_desktop_gadget_manager)
		{
			g_desktop_gadget_manager->AddAutoStartService(item->GetGadgetIdentifier());
		}
	}
#endif

	return item;
}


/******************************************************************************
 *
 * HotlistModelItem::CreateItem
 *
 * Creates item, but does not add it to the model. 
 * Call AddItem(item) to add it to the model.
 *
 ******************************************************************************/

HotlistModelItem* HotlistGenericModel::CreateItem(OpTypedObject::Type type, OP_STATUS& status, BOOL )
{
	UINT32 model_type = GetModelType();

	if( type == OpTypedObject::CONTACT_TYPE )
	{
		if (model_type == HotlistGenericModel::ContactRoot)
		{
			m_current_item = OP_NEW(ContactModelItem, ());
		}
		else
		{
			status = OpStatus::ERR_NOT_SUPPORTED;
			return NULL;
		}
	}
	else if( type == OpTypedObject::GROUP_TYPE )
	{
		m_current_item = OP_NEW(GroupModelItem, ());
	}
	else if( type == OpTypedObject::NOTE_TYPE )
	{
		if (model_type == HotlistGenericModel::NoteRoot)
		{
			m_current_item = OP_NEW(NoteModelItem, ());
		}
		else
		{
			status = OpStatus::ERR_NOT_SUPPORTED;
			return NULL;
		}
	}
#ifdef WEBSERVER_SUPPORT
	else if( type == OpTypedObject::UNITE_SERVICE_TYPE )
	{
		if (model_type == HotlistGenericModel::UniteServicesRoot)
		{
			m_current_item = OP_NEW(UniteServiceModelItem, ());
		}
		else
		{
			status = OpStatus::ERR_NOT_SUPPORTED;
			return NULL;
		}
	}
#endif // WEBSERVER_SUPPORT
	else if( type == OpTypedObject::FOLDER_TYPE )
	{
#ifdef WEBSERVER_SUPPORT
		if(model_type == HotlistGenericModel::UniteServicesRoot)
		{
			m_current_item = OP_NEW(UniteFolderModelItem, ());
		}
		else
#endif // WEBSERVER_SUPPORT
		{
			m_current_item = OP_NEW(FolderModelItem, ());
		}
	}
	else if( type == OpTypedObject::SEPARATOR_TYPE )
	{
		m_current_item = OP_NEW(SeparatorModelItem, ());
	}
	else
	{
		status = OpStatus::ERR_NOT_SUPPORTED; // no such type
		return NULL;
	}

	status = m_current_item ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;

	return m_current_item;
}


/******************************************************************************
 *
 * HotlistGenericModel::IsDroppingGadgetAllowed
 *
 * @param HotlistModelItem* item - 
 * @param HotlistModelItem* to   - 
 * @param InsertType insert_type - decides where relative to param to, item is 
 *                      to be added, values AFTER, BEFORE or INTO
 *
 * @return TRUE if dropping item is allowed, item cannot be dropped
 *          between trash separator and trash folder, or after trash folder.
 *
 ******************************************************************************/

BOOL HotlistGenericModel::IsDroppingGadgetAllowed(HotlistModelItem* item, 
										   HotlistModelItem* to, 
										   DesktopDragObject::InsertType insert_type)
{
#ifdef WEBSERVER_SUPPORT
	INT32 to_idx    = to->GetIndex();
	INT32 trash_sep_idx = -1;

	if(item->GetIsTrashFolder())
		return FALSE;

	if (m_trash_separator_id != -1)
		trash_sep_idx = GetItemByID(m_trash_separator_id)->GetIndex();

	if (insert_type == DesktopDragObject::INSERT_AFTER && to->GetIsTrashFolder())
		return FALSE;
	if (insert_type == DesktopDragObject::INSERT_BEFORE && to->GetIsTrashFolder())
		return FALSE;
	if (insert_type == DesktopDragObject::INSERT_AFTER && to_idx == trash_sep_idx)
		return FALSE;
	return TRUE;

#else
	return TRUE;
#endif // WEBSERVER_SUPPORT
}


/**********************************************************************************
 *
 * HotlistGenericModel::AddRootServiceSeparator
 *
 * @param previous - Item to add separator after (should be root)
 *
 **********************************************************************************/

void HotlistGenericModel::AddRootServiceSeparator(HotlistModelItem* previous)
{
  
#ifdef WEBSERVER_SUPPORT
	OP_ASSERT(previous && previous->IsRootService() && GetRootService() == previous);
#endif

	// Add Special Separator after root service
	HotlistModelItem* sep = OP_NEW(SeparatorModelItem, (SeparatorModelItem::ROOT_SERVICE_SEPARATOR));
	if (sep && previous)
	{
		OpString guid;
		if (OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
			sep->SetUniqueGUID(guid.CStr(), GetModelType());
					
		InsertAfter(sep, previous->GetIndex());
	}
}


/**********************************************************************************
 *
 * HotlistGenericModel::AddSpecialSeparator
 *
 * @param type - type of special separator to add to the model
 *
 * Adds special separator in correct position in model
 * UniteSeparator  - First in model (called when first unite gadget added)
 * LocalSeparator -  Before all local gadgets (called when first local gadget added)
 * TrashSeparator -  Before trash (called when first gadget added)
 *
 ***********************************************************************************/

SeparatorModelItem* HotlistGenericModel::AddSpecialSeparator(UINT32 type)
{
	SeparatorModelItem* sep = 0;

	if (type == SeparatorModelItem::TRASH_SEPARATOR && m_trash_separator_id == -1)
	{
		sep = OP_NEW(SeparatorModelItem, (type));
		if (sep)
		{
			OpString guid;
			if (OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
				sep->SetUniqueGUID(guid.CStr(), GetModelType());

			if (m_trash_folder_id != -1)
			{
				INT32 trash_folder_idx = GetItemByID(m_trash_folder_id)->GetIndex();
				INT32 trash_separator_idx = InsertBefore(sep, trash_folder_idx);
				m_trash_separator_id = GetItemByIndex(trash_separator_idx)->GetID();
			}
			else
			{
			    INT32 trash_separator_idx = AddLast(sep, -1);
				m_trash_separator_id = GetItemByIndex(trash_separator_idx)->GetID();

			}
		}
	}
	return sep;
}

/******************************************************************************
 *
 * HotlistGenericModel::NumUniteGadgets
 *
 * @param running_only - If TRUE, NumUniteGadgets returns number of running
 *                        UniteServices, else all UniteServices are counted.
 *
 ******************************************************************************/

UINT32 HotlistGenericModel::NumUniteGadgets(BOOL running_only)
{
#ifdef WEBSERVER_SUPPORT
	if (GetModelType() != UniteServicesRoot)
		return 0;

	UINT32 num = 0;
	for (INT32 i = 0; i < GetCount(); i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);
		if (item->IsUniteService()) // Not folder or separator
		{
			if (!running_only || ((GadgetModelItem*)item)->IsGadgetRunning())
				num++;
		}
		
	}
	return num;
#else
	return 0;
#endif
}


/*******************************************************************
 **
 ** HotlistGenericModel::EmptyTrash
 **
 ** Removes all items in trash folder.
 **
 *******************************************************************/

BOOL HotlistGenericModel::EmptyTrash()
{
	m_current_search_item.Reset();
	InvalidateEmailLookupTable();

	HotlistModelItem *hmi = GetItemByID( m_trash_folder_id );
	if( hmi && hmi->GetIsTrashFolder() )
	{
		HotlistModelItem* child;

		// FIXME: We should probably remove favicon if not used elsewhere

		// Notify all items in the subtree about deletion
		INT32 index = hmi->GetIndex();
		INT32 end   = hmi->GetIndex() + hmi->GetSubtreeSize();
		for (INT32 i = index + 1; i <= end; i++)
		{
			HotlistModelItem* removed = GetItemByIndex(i);
			HotlistModelItem* parent = 0;
			if (removed)
			{
				parent = removed->GetParentFolder();
			}
			BroadcastHotlistItemRemoving(removed, /* removed_as_child*/ parent != hmi);
		}

		while ((child = hmi->GetChildItem()) != 0)
		{
// Moved to HotlistManager::OnHotlistItemRemoved
//#ifdef GADGET_SUPPORT
//			if (child->IsGadget() || child->IsUniteService())
//				static_cast<GadgetModelItem*>(child)->UninstallGadget();
//#endif // GADGET_SUPPORT

			child->Delete();
		}


#if 0
		// Don't change status of trash folder when deleting items from it
		BroadcastHotlistItemChanged(hmi, HotlistGenericModel::FLAG_TRASH);
#endif
		hmi->Change();
		return TRUE;
	}

	return FALSE;
}

/******************************************************************************
 *
 * HotlistGenericModel::CopyItem
 *
 * Copies the items in model from to this model.
 *
 * @param from              - Model to copy from
 * @param index             - index of item to start copy at
 * @param test_sibling      - If true, continue copying next sibling
 * @param sibling_index     - If -1, insert relative to m_parent_index,
 *                             else insert relative to sibling_index item
 * @param insert_type
 * @param maintain_flag     - If TRUE, properties personalbar, panel and
 *                            Is trash folder on copy is preserverd
 *
 ******************************************************************************/

INT32 HotlistGenericModel::CopyItem(HotlistModel& from, 
							 INT32 index, 
							 BOOL test_sibling,
							 INT32 sibling_index, 
							 DesktopDragObject::InsertType insert_type,
							 INT32 maintain_flag )
{
	// not really sure what value would be best for doing locking

	int first_item_id = -1;

	while(1)
	{
		HotlistModelItem* hmi = from.GetItemByIndex(index);

		if( !hmi )
		{
			break;
		}

		/* Copying anything to clipboard has to be allowed,
		   but into any other model, don't allow copying items of wrong type */
		if (!g_hotlist_manager->IsClipboard(this)
			&& hmi->GetType() != OpTypedObject::FOLDER_TYPE
			&& hmi->GetType() != OpTypedObject::SEPARATOR_TYPE)
		{
			INT32 model_type = GetModelType();

			INT32 item_model_type = -1;
			switch(hmi->GetType())
			{
				case OpTypedObject::NOTE_TYPE: item_model_type = HotlistGenericModel::NoteRoot; break;
				case OpTypedObject::CONTACT_TYPE: item_model_type = HotlistGenericModel::ContactRoot; break;
				case OpTypedObject::UNITE_SERVICE_TYPE: item_model_type = HotlistGenericModel::UniteServicesRoot; break;
				default: break;
			}

			if (model_type != item_model_type)
			{
				index = from.GetSiblingIndex(index);
				continue;
			}
		}

		// Sanity checking for gadgets / unite services only
#ifdef WEBSERVER_SUPPORT
		// Note: sibling might be parent or after / before
		if (GetModelType() == HotlistGenericModel::UniteServicesRoot)
		{
			HotlistModelItem* sibling = GetItemByIndex(sibling_index);
			if (sibling && !IsDroppingGadgetAllowed(hmi, sibling, insert_type))
			{
				return -1;
			}
			else if (!sibling)
			{
				HotlistModelItem* parent = GetItemByIndex(m_parent_index);
				if (parent && !parent->GetIsTrashFolder() && !IsDroppingGadgetAllowed(hmi, parent, insert_type))
				{
					return -1;
				}

			}
		}
#endif // WEBSERVER_SUPPORT

		HotlistModelItem* parent_item = GetItemByIndex(m_parent_index);
		if (parent_item && parent_item->IsGroup())
		{
			GroupModelItem* parent_group = (GroupModelItem*) parent_item;

			if (hmi->IsContact() || hmi->IsBookmark())
			{
				parent_group->AddItem(hmi->GetHistoricalID());
			}
			if (hmi->IsGroup())
			{
				GroupModelItem* from_group = (GroupModelItem*) hmi;

				UINT32 count = from_group->GetItemCount();
				for (UINT i = 0; i < count; i++)
				{
					UINT32 value = from_group->GetItemByPosition(i);
					if (value)
					{
						parent_group->AddItem(value);
					}
				}
			}
			break;
		}

		// Add duplicate of current item to our model
		HotlistModelItem *copy = hmi->GetDuplicate( );

		if (!m_is_moving_items)
		{
			if (copy)
			{
				// duplicate should not have same nickname
				copy->ClearShortName();

				// Clear target if this is a _real_ copy
				if (copy->GetTarget().HasContent())
				{
					OP_ASSERT(copy->IsFolder());
					copy->ClearTarget();
				}

				if (copy->IsFolder())
				{
					// Clear the other special stuff if it's a real copy
					copy->SetSubfoldersAllowed(TRUE);
					copy->SetSeparatorsAllowed(TRUE);
					copy->SetIsDeletable(TRUE);
					copy->SetHasMoveIsCopy(FALSE);
					copy->SetMaxItems(-1);
				}
			}

			/** Generate new id for the copy  **/
     	   	OpString guid;
 		    /** Generate new id   **/
 		    if(OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
 			{
 				copy->SetUniqueGUID(guid.CStr(), GetModelType());
 			}
		}

		// Check SubfoldersAllowed and SeparatorsAllowed ------------------------------------------
		if (GetModelType() == HotlistGenericModel::BookmarkRoot)
		{
			if (hmi && hmi->IsFolder()) // If item is a folder, don't copy if target folder cannot have subfolders
			{
				if (sibling_index == -1)
				{
					HotlistModelItem* fld = GetItemByIndex(m_parent_index);
					if (fld && !fld->GetSubfoldersAllowed())
						break;
				}
				else
				{
					HotlistModelItem* sib = GetItemByIndex(sibling_index);
					if (sib && sib->GetParentFolder() && !sib->GetParentFolder()->GetSubfoldersAllowed())
						break;
				}
			}

			if (hmi && hmi->IsSeparator()) // If item is a separator, don't copy if target folder cannot have separators
			{
				if (sibling_index == -1)
				{
					HotlistModelItem* fld = GetItemByIndex(m_parent_index);
					if (fld && !fld->GetSeparatorsAllowed())
						break;
				}
				else
				{
					HotlistModelItem* sib = GetItemByIndex(sibling_index);
					if (sib && sib->GetParentFolder() && !sib->GetParentFolder()->GetSeparatorsAllowed())
						break;
				}
			}
		}

		if (!copy)
			return -1;

		if( !(maintain_flag & Personalbar ) )
		{
			copy->SetOnPersonalbar(FALSE);
			copy->SetPersonalbarPos(-1);
		}
		if( !(maintain_flag & Panel) )
		{
			//copy->SetInPanel(FALSE);
			copy->SetPanelPos(-1);
		}
		if( !(maintain_flag & Trash) )
		{
			copy->SetIsTrashFolder(FALSE);
		}


		if( first_item_id == -1 )
		{
			first_item_id = copy->GetID();
		}

		if( copy->GetIsTrashFolder() )
		{
			// There can only be one trashfolder
			HotlistModelItem *trash_folder = GetItemByID(m_trash_folder_id);
			if( trash_folder )
			{
				trash_folder->SetIsTrashFolder(FALSE);
			}

			m_trash_folder_id = copy->GetID();
		}

		// ----------- Adding the item ---------------------------------------
		// If gadgetmodel and folder or separator, might need to change type

		if( sibling_index == -1 )
		{
			m_current_index = AddLast(copy, m_parent_index);
		}
		else
		{
			if( insert_type == DesktopDragObject::INSERT_AFTER )
			{
				m_current_index = InsertAfter(copy, sibling_index);
			}
			else
			{
				m_current_index = InsertBefore(copy, sibling_index);
			}

			sibling_index = m_current_index;
		}

		//  -----------------------------------------------------------------

		if (!m_is_moving_items)
		{
#ifdef DEBUG_HOTLIST_HASH
			if (!m_temporary && !g_hotlist_manager->IsClipboard(this))
				fprintf( stderr, " Generating a new GUID for the copy as copy_to_trash FALSE and moving_items FALSE\n");
			else
				fprintf( stderr, " Generating a new guid for item copy added to clipboard or temp model\n");
#endif
		    /** Generate new id for the copy  **/
     	   	OpString guid;
 		    if(OpStatus::IsSuccess(StringUtils::GenerateClientID(guid)))
 			{
 				copy->SetUniqueGUID(guid.CStr(), GetModelType());
 			}
		}

		if (!m_temporary)
		{
			BroadcastHotlistItemAdded(copy, m_move_as_child);
		}


		// ----------- Gadgets ------------
		if (GetModelType() == HotlistGenericModel::UniteServicesRoot
			&& copy->GetIsInsideTrashFolder())
		{
			if (copy->IsGadget())
			{
				static_cast<GadgetModelItem*>(copy)->ShowGadget(FALSE);
			}
#ifdef WEBSERVER_SUPPORT
			else if (copy->IsUniteService())
			{
				static_cast<UniteServiceModelItem*>(copy)->ShowGadget(FALSE, TRUE);
			}
#endif 
		}


		if( &from == this )
		{
			if( m_current_index <= index )
			{
				index ++;
			}
		}

		int num_children = from.GetSubtreeSize(index);
		if( num_children > 0 )
		{
			INT32 parent_index = GetParentIndex(); // Save
			m_parent_index = m_current_index;
			BOOL move_as_child = m_move_as_child;
			m_move_as_child = GetIsMovingItems();
			CopyItem( from, index+1, TRUE, -1, DesktopDragObject::INSERT_AFTER, maintain_flag ); // Hvis kallet originerer herifra, så skal param 2 til Added være TRUE
			m_move_as_child = move_as_child;
			SetParentIndex( parent_index );        // Restore
		}
		if( !test_sibling )
		{
			break;
		}
		index = from.GetSiblingIndex( index );
		if( index == -1 )
		{
			break;
		}
	}

	return first_item_id;
}



/******************************************************************************
 *
 * HotlistGenericModel::GetColumnCount
 *
 *
 ******************************************************************************/

INT32 HotlistGenericModel::GetColumnCount()
{
	switch (m_type)
	{
		case NoteRoot:
	    case UniteServicesRoot:
			return 1;

		case ContactRoot:
			return 3;

		case BookmarkRoot:
		default:
			return 6;
	}
}


/******************************************************************************
 *
 * HotlistGenericModel::GetColumnData
 *
 *
 ******************************************************************************/

OP_STATUS HotlistGenericModel::GetColumnData(ColumnData* column_data)
{
	OpString name, email, phone, nickname, address;

	g_languageManager->GetString(Str::DI_IDSTR_M2_NEWACC_WIZ_EMAILADDRESS, email);
	g_languageManager->GetString(Str::DI_ID_EDITURL_SHORTNAMELABEL, nickname);
	g_languageManager->GetString(Str::DI_ID_CL_PROP_PHONE_LABEL, phone);
	g_languageManager->GetString(Str::SI_LOCATION_TEXT, address);
	g_languageManager->GetString(Str::DI_ID_CL_PROP_NAME_LABEL, name);

	if( column_data->column == 0 )
	{
		column_data->text.Set( name );
	}
	else
	{
		switch (m_type)
		{
			case ContactRoot:
			{
				if( column_data->column == 1 )
				{
					column_data->text.Set( email );
				}
				else if( column_data->column == 2 )
				{
					column_data->text.Set( phone );
				}

				break;
			}

			case BookmarkRoot:
			default:
			{
				if( column_data->column == 1 )
				{
					column_data->text.Set( nickname );
				}
				else if( column_data->column == 2 )
				{
					column_data->text.Set( address );
				}
				break;
			}
		}
	}

	column_data->custom_sort = TRUE;

	return OpStatus::OK;
}


/******************************************************************************
 *
 * HotlistGenericModel::GetTypeString
 *
 *
 ******************************************************************************/

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
OP_STATUS HotlistGenericModel::GetTypeString(OpString& type_string)
{
	switch (m_type)
	{
		case BookmarkRoot:
			return g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_BOOKM, type_string);
		case ContactRoot:
			return g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_CONTACTS, type_string);
		case NoteRoot:
			return g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_NOTES, type_string);
	    case UniteServicesRoot:// AJMTODO: ADD UNITE SERVICE
			type_string.Set(UNI_L(""));

	}
	return OpStatus::ERR;
}
#endif

/******************************************************************************
 *
 * HotlistGenericModel::GetByEmailAddress
 *
 * Note: Contacts only
 *
 ******************************************************************************/

HotlistModelItem *HotlistGenericModel::GetByEmailAddress( const OpStringC &address )
{
	if (address.IsEmpty())
		return NULL;

	// 1 Create lookup buffer for e-mail strings if it does not already exists

	if( !m_email_buffer)
	{
		CreateEmailLookupTable();
	}

	// 2 Try to reuse last search result

	if( m_current_search_item.item && m_current_search_item.item->ContainsMailAddress(address) )
	{
		// Same key as last time. Reuse
		return m_current_search_item.item;
	}
	else if( !m_current_search_item.item && m_current_search_item.key.Compare(address) == 0 )
	{
		// Same key as last time (which did not match either). Reuse
		return NULL;
	}
	m_current_search_item.key.Set(address);

	// 3 Look in lookup buffer for e-mail strings.

	void* data;
	OP_STATUS rc = m_email_buffer->table()->GetData( address.CStr(), &data );
	if( rc == OpStatus::OK )
	{
		m_current_search_item.item = (HotlistModelItem*)data;
		return (HotlistModelItem*)data;
	}
	else
	{
		m_current_search_item.item = 0;
		return NULL;
	}
}

/******************************************************************************
 *
 * HotlistGenericModel::GetByGadgetIdentifier
 *
 *
 ******************************************************************************/

#ifdef GADGET_SUPPORT
HotlistModelItem* HotlistGenericModel::GetByGadgetIdentifier(const OpStringC &gadget_id)
{
	if (gadget_id.IsEmpty())
		return NULL;

	INT32 count = GetItemCount();

	for (INT32 i = 0; i < count; i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);

		if (item->GetGadgetIdentifier().Compare(gadget_id.CStr()) == 0)
			return item;
	}

	return NULL;
}
#endif // GADGET_SUPPORT


/******************************************************************************
 *
 * HotlistGenericModel::GetGadgetCountByIdentifier
 *
 *
 ******************************************************************************/

#ifdef GADGET_SUPPORT
UINT32 HotlistGenericModel::GetGadgetCountByIdentifier(const OpStringC &gadget_id, BOOL include_items_in_trash)
{
	if (gadget_id.IsEmpty())
		return 0;

	//counter for gadgets
	UINT32 gadget_count = 0;
	//total number of items
	INT32 count = GetItemCount();

	for (INT32 i = 0; i < count; i++)
	{
		HotlistModelItem* item = GetItemByIndex(i);
		if ((item->GetGadgetIdentifier().Compare(gadget_id.CStr()) == 0)
			&& (include_items_in_trash || !item->GetIsInsideTrashFolder()))
			++gadget_count;
	}

	return gadget_count;
}
#endif // GADGET_SUPPORT


/******************************************************************************
 *
 * HotlistGenericModel::DeleteItem
 *
 * Delete item from model. 
 *
 * @param item
 * @param copy_to_trash - If True, item is added to trash
 * @param handle_trash  - If True this overrides normal behaviour for trash and 
 *                        other non-deletable items
 * 
 *
 *
 ******************************************************************************/

BOOL HotlistGenericModel::DeleteItem( OpTreeModelItem* item, BOOL copy_to_trash, BOOL handle_trash, BOOL keep_gadget )
{
#ifdef DEBUG_HOTLIST_HASH
	fprintf( stderr, " DeleteItem - copy_to_trash = %d, handle_trash = %d\n", copy_to_trash, handle_trash);
#endif

#ifndef HOTLIST_USE_MOVE_ITEM
	BOOL moving_state = GetIsMovingItems();
#endif

	m_current_search_item.Reset();
	InvalidateEmailLookupTable();

	HotlistModelItem* hmi = GetItemByType(item); // hmi is the item to move / delete 

#ifdef WEBSERVER_SUPPORT
	if (hmi->IsRootService())
		return FALSE;
#endif
	 
	if (!hmi || hmi->GetModel() != this)
	{
		return FALSE;
	}

	if (hmi->GetIsTrashFolder() && !handle_trash)
	{
		return FALSE;
	}

	// copy_to_trash not an option if item is already in trash folder
	if (hmi->GetIsInsideTrashFolder())
	{
		copy_to_trash = FALSE;
	}

	// If this item is a targeted folder we need to ask before deleteing and then
	// kill it directly (i.e. not to trash) if they say yes
	if (hmi->GetTarget().HasContent() && copy_to_trash)
	{
		// If they don't want to delete just stop.
		if (!g_hotlist_manager->ShowTargetDeleteDialog(hmi))
		{
			return FALSE;
		}
		
		// Continue as normal but turn off copy to trash
		copy_to_trash = FALSE;
	}

	INT32 index = GetIndexByItem(hmi);

#ifdef HOTLIST_USE_MOVE_ITEM
	/**
	 *
	 * When MoveItem is used (defined on), DeleteItem will only be used
	 * to do _real_ deletes, and so this part is not really needed
	 *
	 **/
	if (copy_to_trash)
	{
		HotlistModelItem* trash_folder = GetTrashFolder();
		MoveItem(hmi->GetModel(), hmi->GetIndex(), trash_folder, INSERT_INTO);

		//BroadcastHotlistItemChanged(hmi); // Called inside move??
		BroadcastHotlistItemTrashed(hmi);
		
		// Notify the whole subtree about removal
		INT32 index = hmi->GetIndex();
		INT32 end   = hmi->GetIndex() + hmi->GetSubtreeSize();
		for (INT32 i = index + 1; i <= end; i++)
		{
			HotlistModelItem* removed = GetItemByIndex(i);
			//BroadcastHotlistItemChanged(removed); // called inside move??
			BroadcastHotlistItemTrashed(removed);
		}
		Change(m_parent_index);

		return TRUE;
	}

	
	///////// IF NOT COPY TO TRASH ////////

	if( index != -1 )
	{
		HotlistModel tmp(hmi->GetModel()->GetModelType(), TRUE);

#ifndef HOTLIST_USE_MOVE_ITEM
		tmp.SetIsMovingItems(TRUE);
#endif // !HOTLIST_USE_MOVE_ITEM
		
		// If item is in Trash; Uninstall gadget and remove icon file
		if(hmi->GetIsInsideTrashFolder() && !keep_gadget) 
		{
#ifdef GADGET_SUPPORT
			//uninstall the gadget when deleted from trash, and there is no other reference left
			if ((hmi->IsGadget() || hmi->IsUniteService()) && (GetGadgetCountByIdentifier(hmi->GetGadgetIdentifier()) == 1))
			{
				if (!GetIsMovingItems())
					static_cast<GadgetModelItem*>(hmi)->UninstallGadget();
			}
#endif // GADGET_SUPPORT
		}

#if defined(GADGET_SUPPORT) && defined (WEBSERVER_SUPPORT)
		if (hmi->IsSeparator() && hmi->GetSeparatorType() == SeparatorModelItem::TRASH_SEPARATOR)
		{
			return FALSE;
		}
#endif // WEBSERVER_SUPPORT && GADGET_SUPPORT
		
#ifndef HOTLIST_USE_MOVE_ITEM
		m_bypass_trash_on_delete = !copy_to_trash; // Cut action. Same as delete without moving to trash
		if (!GetIsMovingItems())
 			SetIsMovingItems(!m_bypass_trash_on_delete); 
#endif // !HOTLIST_USE_MOVE_ITEM
		
		if (!m_temporary)
		{
			BroadcastHotlistItemRemoving(hmi);
			
			// Notify the whole subtree about removal
			INT32 index = hmi->GetIndex();
			INT32 end   = hmi->GetIndex() + hmi->GetSubtreeSize();
			for (INT32 i = index + 1; i <= end; i++)
			{
				HotlistModelItem* removed = GetItemByIndex(i);
				BroadcastHotlistItemRemoving(removed);
			}
		}
		SetIsMovingItems(moving_state);

		hmi->Delete();
		
#ifndef HOTLIST_USE_MOVE_ITEM
		m_bypass_trash_on_delete = FALSE;
#endif // !HOTLIST_USE_MOVE_ITEM
	}

#else // !HOTLIST_USE_MOVE_ITEM

	if( index != -1 )
	{
		HotlistGenericModel tmp(hmi->GetModel()->GetModelType(), TRUE);

#ifndef HOTLIST_USE_MOVE_ITEM
		tmp.SetIsMovingItems(TRUE);
#endif // !HOTLIST_USE_MOVE_ITEM
		
		// Add to trash if possible. If so -> keep GUID as is
		if( copy_to_trash )
		{
			
#ifndef HOTLIST_USE_MOVE_ITEM
			SetIsMovingItems(TRUE);
#endif // !HOTLIST_USE_MOVE_ITEM
			
			HotlistModelItem* trash_folder = GetTrashFolder();

			if( trash_folder )
			{
				int trash_index = GetIndexByItem(trash_folder);
				if( trash_index != -1 )
				{
					tmp.CopyItem( *this, // from this model
								  index, 
								  FALSE, 
								  -1, 
								  DesktopDragObject::INSERT_AFTER, 
								  0 );
					
					if (!m_temporary)
					{
						BroadcastHotlistItemTrashed(hmi);
					}
				}
			}

#ifndef HOTLIST_USE_MOVE_ITEM			
			SetIsMovingItems(moving_state); // reset back to state before this function called
#endif // HOTLIST_USE_MOVE_ITEM
		}
		
		// Moved to HotlistManager::OnHotlistItemRemoved
//		// If item is in Trash; Uninstall gadget and remove icon file
//		if(hmi->GetIsInsideTrashFolder())
//		{
//#ifdef GADGET_SUPPORT
//			//uninstall the gadget when deleted from trash, and there is no other reference left
//			if ((hmi->IsGadget() || hmi->IsUniteService()) && (GetGadgetCountByIdentifier(hmi->GetGadgetIdentifier()) == 1))
//			{
//				if (!GetIsMovingItems())
//				{
//					static_cast<GadgetModelItem*>(hmi)->UninstallGadget();
//				}
//			}
//#endif // GADGET_SUPPORT
//			
//			//RemoveIconFile(hmi);
//		}

#if defined(GADGET_SUPPORT) && defined (WEBSERVER_SUPPORT)
		if (hmi->IsSeparator() && 
			hmi->GetSeparatorType() == SeparatorModelItem::TRASH_SEPARATOR
			|| hmi->GetSeparatorType() == SeparatorModelItem::ROOT_SERVICE_SEPARATOR)
		{
			return FALSE;
		}
#endif // WEBSERVER_SUPPORT && GADGET_SUPPORT
		
#ifndef HOTLIST_USE_MOVE_ITEM
		m_bypass_trash_on_delete = !copy_to_trash; // Cut action. Same as delete without moving to trash
#endif // !HOTLIST_USE_MOVE_ITEM
		
		if (!GetIsMovingItems()) //???? missing when fix 
			SetIsMovingItems(!m_bypass_trash_on_delete); 

		if (!m_temporary)
		{
			BroadcastHotlistItemRemoving(hmi, FALSE);
			
			// Notify the whole subtree about removal
			INT32 index = hmi->GetIndex();
			INT32 end   = hmi->GetIndex() + hmi->GetSubtreeSize();
			for (INT32 i = index + 1; i <= end; i++)
			{
				HotlistModelItem* removed = GetItemByIndex(i);
				BroadcastHotlistItemRemoving(removed, TRUE);
			}
		}
		
		SetIsMovingItems(moving_state); // vs (FALSE)

		hmi->Delete();
		
#ifndef HOTLIST_USE_MOVE_ITEM
		m_bypass_trash_on_delete = FALSE;
#endif // !HOTLIST_USE_MOVE_ITEM

		// tmp is non-empty if copy_to_trash TRUE and items not in trash folder
		if( tmp.GetItemCount() > 0 )
		{
#ifndef HOTLIST_USE_MOVE_ITEM
			SetIsMovingItems(TRUE);
			tmp.SetIsMovingItems(TRUE);
#endif // !HOTLIST_USE_MOVE_ITEM
			
			HotlistModelItem* trash_folder = GetTrashFolder();
			if( trash_folder )
			{
				int trash_index = GetIndexByItem(trash_folder);
				if( trash_index != -1 )
				{
					// Copy items from tmp to trash folder
					m_parent_index = trash_index;
					CopyItem( tmp, 0, FALSE, -1, DesktopDragObject::INSERT_AFTER, 0 );
					Change(m_parent_index);
					// trash changed children // BroadcastHotlistItemChanged(parent_item, HotlistModel::FLAG_CHILDREN);
				}
			}
			
#ifndef HOTLIST_USE_MOVE_ITEM
			SetIsMovingItems(moving_state);
#endif // !HOTLIST_USE_MOVE_ITEM
			
		}
	}
#endif // HOTLIST_USE_MOVE_ITEM

	return TRUE;
}

/******************************************************************************
 *
 * HotlistGenericModel::AddTrashFolder
 *
 *
 ******************************************************************************/

HotlistModelItem* HotlistGenericModel::AddTrashFolder()
{
	UINT32 model_type = GetModelType();

	if( m_trash_folder_id != -1)
	{
		HotlistModelItem *OP_MEMORY_VAR hmi = GetItemByID( m_trash_folder_id );
		if( hmi && hmi->GetIsTrashFolder() )
		{
			OpString text;
			g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_TRASH, text);
			hmi->SetName( text.CStr() );
			return hmi;

		}
	}
	HotlistModelItem *OP_MEMORY_VAR hmi = NULL;

#ifdef WEBSERVER_SUPPORT
	if(model_type == HotlistModel::UniteServicesRoot)
	{
		hmi = OP_NEW(UniteFolderModelItem, ());
	}
	else
#endif // WEBSERVER_SUPPORT
	{
		hmi = OP_NEW(FolderModelItem, ());
	}
	if (hmi)
	{
		hmi->SetCreated(g_timecache->CurrentTime());
		hmi->SetIsTrashFolder( TRUE );

		OpString text;
		g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_TRASH, text);
		hmi->SetName( text.CStr() );

		m_trash_folder_id = hmi->GetID();

		m_current_index = AddFirst(hmi); // +++

		if (!m_temporary)
		{
			BroadcastHotlistItemAdded(hmi, FALSE);
		}
	}

	return hmi;
}

void HotlistGenericModel::Erase()
{
	m_trash_folder_id = -1;

	m_current_search_item.Reset();
	InvalidateEmailLookupTable();

	HotlistModel::Erase();
}


/******************************************************************************
 *
 * HotlistGenericModel::CopyItem
 *
 *
 ******************************************************************************/

INT32 HotlistGenericModel::CopyItem( OpTreeModelItem *item, INT32 maintain_flag )
{
	HotlistModelItem* hmi = GetItemByType( item );
	if( hmi )
	{
		INT32 index = hmi->GetModel()->GetIndexByItem( hmi );
		if( index != -1 )
		{
			return CopyItem( *hmi->GetModel(), index, FALSE, -1, DesktopDragObject::INSERT_AFTER, maintain_flag );
		}
	}

	return -1;
}

/******************************************************************************
 *
 * HotlistGenericModel::PasteItem
 *
 *
 ******************************************************************************/

INT32 HotlistGenericModel::PasteItem( HotlistModel &from, OpTreeModelItem* at, DesktopDragObject::InsertType insert_type, INT32 maintain_flag )
{

	INT32 first_item_id = -1;

	HotlistModelItem* hmi = GetItemByType( at );

	if( hmi && hmi->GetModel() == this )
	{
		INT32 parent_index = -1;
		INT32 sibling_index = GetIndexByItem(hmi);

		if( hmi->IsFolder() || hmi->IsGroup())
		{
			if( insert_type == DesktopDragObject::INSERT_BEFORE || insert_type == DesktopDragObject::INSERT_AFTER )
			{
				parent_index = GetItemParent(sibling_index);
			}
			else
			{
				parent_index = GetIndexByItem(hmi);
				sibling_index = -1;
			}
		}
		else
		{
			if( insert_type == DesktopDragObject::INSERT_INTO )
			{
				insert_type = DesktopDragObject::INSERT_AFTER;
			}
			parent_index = GetItemParent(sibling_index);
		}

		SetParentIndex( parent_index );
		first_item_id = CopyItem( from, 0, TRUE, sibling_index, insert_type, maintain_flag );
	}
	else
	{
		SetParentIndex( -1 );
		first_item_id = CopyItem( from, 0, TRUE, -1, DesktopDragObject::INSERT_AFTER, maintain_flag );
	}

	return first_item_id;
}


/********************************************************************
 *
 * HotlistGenericModel::RegisterTemporaryItem
 *
 * @param item - the item to add/register
 * @param parent_id - Item will be added 
 *
 ********************************************************************/

BOOL HotlistGenericModel::RegisterTemporaryItem(OpTreeModelItem *item, 
										 int parent_id, 
										 INT32* got_id)
{
	HotlistModelItem* hmi = GetItemByType( item );

	HotlistModelItem* folder = GetItemByID( parent_id );

	if (folder && !folder->GetSubfoldersAllowed() && hmi->IsFolder())
		return FALSE;

	if (folder && !folder->GetSeparatorsAllowed() && hmi->IsSeparator())
		return FALSE;

	if( hmi && hmi->GetTemporaryModel() == this && !GetItemByID(hmi->GetID()) )
	{
		int parent_index = GetIndexByItem( GetItemByID(parent_id) );

		OP_ASSERT(!GetByUniqueGUID(hmi->GetUniqueGUID()));

#ifdef DEBUG_HOTLIST_HASH
		if (GetByUniqueGUID(hmi->GetUniqueGUID()))
		{
			fprintf( stderr, " \n Doing RegisterTemporaryItem on item with GUID that already exists! \n");
			// BAIL OUT
		}
#endif

#ifdef WEBSERVER_SUPPORT
		if (hmi->IsRootService()) // Root service should always be on top
		{
			OP_ASSERT(!GetRootService());

			UniteServiceModelItem * root_item = static_cast<UniteServiceModelItem*>(hmi);
			if ((root_item->GetGadgetIdentifier().HasContent() && !root_item->GetOpGadget()) || GetRootService())
			{
				OP_ASSERT(FALSE);
				return FALSE;
			}
			else
			{
				AddFirst(hmi);
				SetRootService(root_item);
				AddRootServiceSeparator(hmi);
			}
		}
		else
		{
#endif		
			AddLastBeforeTrash( parent_index, hmi, &m_current_index );
#ifdef WEBSERVER_SUPPORT
		}
#endif
		//BroadcastHotlistItemAdded(hmi); -> Called from AddLastBeforeTrash

		hmi->SetTemporaryModel(NULL);
		SetDirty(TRUE);
		if (got_id)
			*got_id = hmi->GetID();

		return TRUE;
	}
	return FALSE;
}


/******************************************************************************
 *
 * HotlistModel::RegisterTemporaryItemOrdered
 *
 * @param OpTreeModelItem* item - item to add to model
 * @param HotlistModelItem* previous - item is added to model _after_ this item
 *
 * @return BOOL - TRUE if item was successfully added
 *
 ******************************************************************************/

// Insert item after item with previous_id
BOOL HotlistGenericModel::RegisterTemporaryItemOrdered(OpTreeModelItem *item, HotlistModelItem* previous )
{
	if (!previous)
		return FALSE;

	HotlistModelItem* hmi = GetItemByType( item );

	if( hmi && hmi->GetTemporaryModel() == this ) // && !GetItemByID(hmi->GetID()) )
	{
		int previous_index = GetIndexByItem( previous );
		OP_ASSERT(!GetByUniqueGUID(hmi->GetUniqueGUID()));

		InsertAfter(hmi, previous_index );

		BroadcastHotlistItemAdded(hmi, FALSE);

		hmi->SetTemporaryModel(NULL);
		SetDirty(TRUE);
		return TRUE;
	}
	return FALSE;
}


/******************************************************************************
 *
 * HotlistModel::RegisterTemporaryItemFirstInFolder
 *
 * @param OpTreeModelItem* item - item to add to model
 * @param HotlistModelItem* parent - item is added as first child of this folder
 *
 * @return TRUE if item was successfully added
 *
 ******************************************************************************/

BOOL HotlistGenericModel::RegisterTemporaryItemFirstInFolder(OpTreeModelItem *item, HotlistModelItem* parent )
{

	HotlistModelItem* hmi = GetItemByType( item );

	if( hmi && hmi->GetTemporaryModel() == this ) //&& !GetItemByID(hmi->GetID()) )
	{
		int parent_index = -1;

		if (parent)
			parent_index = GetIndexByItem( parent );

		OP_ASSERT(!GetByUniqueGUID(hmi->GetUniqueGUID()));
		AddFirst(hmi, parent_index);

		BroadcastHotlistItemAdded(hmi, FALSE);

		hmi->SetTemporaryModel(NULL);
		SetDirty(TRUE);
		return TRUE;
	}
	return FALSE;
}


/******************************************************************************
 *
 * HotlistModel::RemoveTemporaryItem
 *
 *
 ******************************************************************************/

BOOL HotlistGenericModel::RemoveTemporaryItem( OpTreeModelItem *item )
{
	HotlistModelItem* hmi = GetItemByType( item );
	if( hmi && hmi->GetTemporaryModel() == this && !GetItemByID(hmi->GetID()) )
	{
		OP_DELETE(item);
		return TRUE;
	}
	return FALSE;
}

/******************************************************************************
 *
 * HotlistModel::AddTemporaryItem
 *
 *
 ******************************************************************************/

HotlistModelItem* HotlistGenericModel::AddTemporaryItem( OpTypedObject::Type type )
{
	OP_STATUS status = OpStatus::OK;
	HotlistModelItem* item = CreateItem(type,status,FALSE);

	if( item )
	{
		item->SetTemporaryModel(this);
		item->SetCreated(g_timecache->CurrentTime());
	}

	return item;
}


/******************************************************************************
 *
 * HotlistGenericModel::Reparent
 *
 * Moves a single item to a new position in the model
 *
 ******************************************************************************/

BOOL HotlistGenericModel::Reparent( HotlistModelItem *src, 
							 HotlistModelItem *target, 
							 BOOL first_child, 
							 BOOL handle_trash )
{

	BOOL moving_state = GetIsMovingItems();

	BOOL root_folder = FALSE;

	if (!src)
		return FALSE;

	if (!target)
	{
		if (!first_child)
			return FALSE;

		root_folder = TRUE;
	}

	if (!root_folder) // Not moving to root folder
	{
		if( src->GetModel() != this || !target->IsFolder() )
		{
			return FALSE;
		}

		if( src->GetModel() == target->GetModel() )
		{
			if( src->GetParentFolder() == target  &&
				(target->GetChildItem() == src || !first_child))
			{
				return TRUE; // Nothing to do
			}

			if( src == target )
			{
				return TRUE; // Same node
			}

			if( target->IsChildOf(src) )
			{
				return FALSE; // Cannot move parent into child, grandchild etc
			}
			if (!target->GetSubfoldersAllowed() && src->IsFolder())
			{
				return FALSE;
			}
			if (!target->GetSeparatorsAllowed() && src->IsSeparator())
			{
				return FALSE;
			}
		}
	}


	SetIsMovingItems(TRUE);

	BOOL result = FALSE;

	// Save item (and any children) to move
	HotlistGenericModel tmp(src->GetModel()->GetModelType(), TRUE);
	tmp.SetIsMovingItems(TRUE);

	tmp.CopyItem(src, Personalbar|Panel|Trash);

	// Delete source
	if( DeleteItem( src, FALSE, handle_trash ) )
	{
		// Copy saved items to new location
		SetDirty( TRUE );

		HotlistModelItem* next = NULL;

		if ( first_child ) // Insert first in target folder
		{
			if (root_folder)
			{
				next = GetItemByIndex(0);
				if (next)
				{
					PasteItem(tmp, next, DesktopDragObject::INSERT_BEFORE, Personalbar|Panel|Trash);
				}
			}
			else
			{
				next = target->GetChildItem(); // Get first child of target
				if (next)
					{
						PasteItem( tmp, next, DesktopDragObject::INSERT_BEFORE, Personalbar|Panel|Trash);
					}
				else
					{
						// TODO: Fix case
					}
			}
		}

		if (!first_child || !next)
		{
			PasteItem( tmp, target, DesktopDragObject::INSERT_INTO, Personalbar|Panel|Trash);
		}

		result = TRUE;
	}

	SetIsMovingItems(moving_state);

	return result;

}

/******************************************************************************
 *
 * HotlistGenericModel::Reorder
 *
 * Moves a single item to a new position - after new_prev
 *
 ******************************************************************************/

BOOL HotlistGenericModel::Reorder( HotlistModelItem *src, 
							HotlistModelItem *new_prev, 
							BOOL handle_trash ) // needed when doing copy and then delete, because we need
	                                            // it to work for moving trash folder too
{

	BOOL moving_state = GetIsMovingItems();

	if (!src || !new_prev)
	{
		return FALSE;
	}

	if( src->GetModel() != this )
	{
		return FALSE;
	}

	if( src->GetModel() == new_prev->GetModel() )
	{
		if( src->GetPreviousItem() == new_prev )
		{
			return TRUE; // Nothing to do
		}

		if( src == new_prev )
		{
			return TRUE; // Same node
		}

	}

	SetIsMovingItems(TRUE);

	BOOL result = FALSE;

	// Save item (and any children) to move
	HotlistGenericModel tmp(src->GetModel()->GetModelType(), TRUE);

	tmp.SetIsMovingItems(TRUE);
	
	tmp.CopyItem(src, Personalbar|Panel|Trash);

	/**
	   TODO: Test whether deleting src implies deleting new_prev?
	   That is, if new_prev is a child at some level of src.

	**/

	// Delete source
	if( DeleteItem( src, FALSE, handle_trash ) )
	{
		// Copy saved items to new location
		SetDirty( TRUE );
		PasteItem( tmp, new_prev, DesktopDragObject::INSERT_AFTER, Personalbar|Panel|Trash);
		result = TRUE;
	}

	SetIsMovingItems(moving_state);
	
	return result;

}


/**********************************************************************************
 *
 * HotlistGenericModel::AddLastBeforeTrash
 *
 * Item is added last in folder specified by param parent_index.
 *
 *
 * @param parent_index - index of parent 
 * @param item - item to add
 * @param got_index - If not null, got_index holds the new items index at return
 *
 **********************************************************************************/

void HotlistGenericModel::AddLastBeforeTrash( INT32 parent_index, 
									   HotlistModelItem* item, 
									   INT32* got_index )
{

#ifdef WEBSERVER_SUPPORT
	// TODO: this should be moved
	// TODO: When/if syncing gadgets need to add broadcast added event too
	if (m_type == HotlistGenericModel::UniteServicesRoot)
	{
		return AddGadgetToModel(item, got_index);
	}
#endif // WEBSERVER_SUPPORT

	INT32 count = GetItemCount();

	// 
	if( item->GetIsTrashFolder() || count <= 0 || parent_index != -1 )
	{
		// Adds item as last child of parent
		*got_index = AddLast(item, parent_index);
		BroadcastHotlistItemAdded(item, FALSE);
	}
	else // item is trash folder or no parent or no items previously
	{
		// Last item is trash
		HotlistModelItem *hmi = GetItemByIndex(count - 1);
		if( hmi->GetIsTrashFolder() )
		{
			INT32 trash_parent_index = GetItemParent(count - 1);
			if( trash_parent_index == -1 )
			{
				*got_index = InsertBefore(item, count - 1);
				BroadcastHotlistItemAdded(item, FALSE);
				return;
			}
		}
		// Trash contains items, is not last itself
		else if( hmi->GetIsInsideTrashFolder() )
		{
			while( hmi && !hmi->GetIsTrashFolder() )
			{
				hmi = hmi->GetParentFolder();
			}
			if( hmi )
			{
				INT32 trash_parent_index = GetIndexByItem( hmi );

				*got_index = InsertBefore(item, trash_parent_index);

				BroadcastHotlistItemAdded(item, FALSE);
				return;
			}
		}

		*got_index = AddLast(item, parent_index);
		BroadcastHotlistItemAdded(item, FALSE);
	}
}

void HotlistGenericModel::SetDirty( BOOL state, INT32 timeout_ms )
{
	m_current_search_item.Reset();
	InvalidateEmailLookupTable();
	HotlistModel::SetDirty(state, timeout_ms);
}
