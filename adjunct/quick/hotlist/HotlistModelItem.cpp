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
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/panels/GadgetsPanel.h"
#include "adjunct/quick/windows/DesktopGadget.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"

#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkin.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/img/image.h"
#include "modules/img/src/imagemanagerimp.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpLocale.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/util/filefun.h"
#include "modules/util/timecache.h"

#include "modules/history/OpHistoryModel.h"
#include "modules/history/OpHistoryPage.h"

#include "modules/pi/OpPainter.h"
#include "adjunct/quick/hotlist/HotlistManager.h"

#include "modules/prefs/prefsmanager/collections/pc_webserver.h"

// TEMP
#include "adjunct/quick/managers/SyncManager.h"

#ifdef M2_SUPPORT
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index.h"
#endif // M2_SUPPORT

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

/***************************************************************************
 *
 * BuildTimeString
 *
 * @param OpString dest - holds string representation of time_t at at return
 * @param time_t at - time to create string representation of
 * @param BOOl use24Hour
 *
 ***************************************************************************/

void BuildTimeString( OpString &dest, time_t at, BOOL use24Hour )
{
	struct tm *tm = localtime(&at);

	if (!tm)
		return;

	OpString s;
	if (!s.Reserve(128))
		return;

	if( use24Hour )
	{
		g_oplocale->op_strftime( s.CStr(), s.Capacity(), UNI_L("%x %H:%M:%S"), tm );
	}
	else
	{
		g_oplocale->op_strftime( s.CStr(), s.Capacity(), UNI_L("%x %X"), tm );
	}

	dest.Set(s);
}

/**************************************************************************************
 *
 * IsMailSameAsServer
 *
 * Checks a mail address against an url
 *
 * @param candidate1 url
 * @param candidate2 mailto-url
 * @return TRUE if mail address in candidate2 has the same 
 * secondlevel-domain as candidate1
 *
 **************************************************************************************/
#if (0)
static BOOL IsMailSameAsServer(const OpStringC& candidate1, const OpStringC& candidate2)
{

	URL	candidate1_url = urlManager->GetURL(candidate1.CStr());
	URL	candidate2_url = urlManager->GetURL(candidate2.CStr());

	ServerName* servername;

	servername = candidate1_url.GetServerName();
	OpString str1;
	if(servername)
	{
		str1.Set(servername->UniName());
	}
	else
	{
		str1.Set(candidate1);
	}

	OpString str2;
	servername = candidate2_url.GetServerName();
	if(servername)
	{
		str2.Set(candidate2_url.GetServerName()->UniName());
	}
	else
	{
		str2.Set(candidate2);
	}

	//find the secondlevel domain in candidate1
	int pos1 = str1.FindLastOf('.');
	int pos2;
	if(pos1 != KNotFound)
	{
		pos2 = str1.FindLastOf('.');
		if(pos1 < pos2)		// we are in trouble, don't try to go further but go home to mom
		{
			return FALSE;
		}
		int startofsecondlevel = str1.SubString(0, pos2).FindLastOf('.');
		if(startofsecondlevel != KNotFound)
		{
			str1.Set(str1.SubString(startofsecondlevel+1));
		}
	}

	//find the secondlevel domain in candidate2
	pos1 = str2.FindFirstOf('@');
	if(pos1 != KNotFound)
	{
		int startofsecondlevel = str2.SubString(pos1).FindFirstOf('.');

		int i = 1;

		while(1)
		{
			pos2 = str2.SubString(pos1+startofsecondlevel+i).FindFirstOf('.');

			if(pos2 != KNotFound)
			{
				i++;
			}
			else
				break;
		}
		if(startofsecondlevel != KNotFound)
		{
			if(i == 1)
			{
				str2.Set(str2.SubString(pos1+1));
			}
			else
			{
				str2.Set(str2.SubString(pos1+startofsecondlevel+1));
			}
		}
	}

	return (str1.Compare(str2) == 0);

}
#endif // 0


/***********************************************************************************
 **
 **	HotlistModelItem
 **
 ***********************************************************************************/

HotlistModelItem::HotlistModelItem()
	: TreeModelItem<HotlistModelItem, HotlistModel>()
	, m_temporary_model(NULL)
	, m_global_id(m_id_counter++)
	, m_historical_id(m_global_id)
	, m_marked(FALSE)
	, m_moved_as_child(FALSE)
#ifdef SUPPORT_DATA_SYNC
	, m_changed_fields(0)
	, m_last_sync_status(OpSyncDataItem::DATAITEM_ACTION_NONE)
#endif // SUPPORT_DATA_SYNC
{

}

HotlistModelItem::HotlistModelItem( const HotlistModelItem& item )
	: TreeModelItem<HotlistModelItem, HotlistModel>()
	, m_temporary_model(NULL)
	, m_global_id(/*m_same_id_counter ? item.m_global_id : */m_id_counter++)
	, m_historical_id(item.m_historical_id)
	, m_marked(FALSE)
	, m_moved_as_child(FALSE)
#ifdef SUPPORT_DATA_SYNC
	, m_changed_fields(item.m_changed_fields)
	, m_last_sync_status(OpSyncDataItem::DATAITEM_ACTION_NONE)
#endif // SUPPORT_DATA_SYNC
{
	m_unique_guid.Set(item.m_unique_guid);
}

HotlistModelItem::~HotlistModelItem()
{
}

/*******************************************************************
 **
 ** HotlistModelItem::GetDuplicate
 **
 **
 *******************************************************************/

HotlistModelItem *HotlistModelItem::GetDuplicate()
{
	return OP_NEW(HotlistModelItem, (*this));
}

/*******************************************************************
 **
 ** HotlistModelItem::GetItemUrl
 **
 **
 *******************************************************************/

OP_STATUS HotlistModelItem::GetItemUrl(OpString& str,  bool display_url /*= true*/) const
{
	OP_STATUS status = OpStatus::ERR;
	if (display_url && GetHasDisplayUrl())
		status = str.Set(GetDisplayUrl()); 

	// Fallback if failed to fetch display URL or set display url!
	if (OpStatus::IsError(status))
		status = str.Set(const_cast<HotlistModelItem*>(this)->GetResolvedUrl());

	return status;
}

/*******************************************************************
 **
 ** HotlistModelItem::SetUniqueGUID
 **
 ** @param str - the UID
 ** @param model_type -
 **
 ** Sets items unique GUID and adds the item to the model hash table
 ** If Item is Trash folder -> uses the single trash GUID (per model)
 **
 ** If an item with this GUID is already in the model, it is removed
 ** from the hash table before the add.
 **
 *******************************************************************/

void HotlistModelItem::SetUniqueGUID( const uni_char* str, INT32 model_type )
{
	if (!str)
		return;

#ifdef HOTLIST_HASHTABLE
	// If there is already a GUID assigned remove it from the hash table
	if (m_unique_guid.HasContent() && GetModel())
	{
		GetModel()->RemoveUniqueGUID(this);
	}
#endif // HOTLIST_HASHTABLE

	// Set the new GUID making sure trash folder of bookmarks model always gets the same item
	if (model_type == HotlistModel::BookmarkRoot && GetIsTrashFolder())
	{
		m_unique_guid.Set(UNI_L("4E1601F6F30511DB9CA51FD19A7AAECA"));
	}
	else if (model_type == HotlistModel::NoteRoot && GetIsTrashFolder())
	{
		m_unique_guid.Set(UNI_L("A9AAFED0976111DC85AC8946BEF8D2DC"));
	}
	else
	{
		m_unique_guid.Set(str);
	}

#ifdef HOTLIST_HASHTABLE
	// Re-add the new GUID to the hash table
	if (GetModel())
	{
		GetModel()->AddUniqueGUID(this);
	}
#endif // HOTLIST_HASHTABLE
}


#ifndef DISPLAY_FALLBACK_PAINTING
void BlitHLineAlpha32(UINT32* src32, UINT32* dst32, UINT32 len)
{
	// Endianness-independent way of doing it:
	for (UINT32 i=0; i<len; i++)
	{
        UINT32 src = *src32++;
        int alpha = src >> 24;
        
        unsigned char d = *dst32;
        UINT32 blue  = d + (alpha * (((src      ) & 0xff) - d)>>8);
        d = *dst32 >>  8;
        UINT32 green = d + (alpha * (((src >>  8) & 0xff) - d)>>8);
        d = *dst32 >> 16;
        UINT32 red   = d + (alpha * (((src >> 16) & 0xff) - d)>>8);
        d = *dst32 >> 24;
        alpha        = d + (alpha * ((              0xff) - d)>>8);
        
        *dst32++ = blue | (green << 8) | (red << 16) | (alpha << 24);
	}
}
#else
extern void BlitHLineAlpha32(UINT32* src32, UINT32* dst32, UINT32 len);
#endif


/******************************************************************************
 *
 * HotlistModelItem::GetItemData
 *
 *
 ******************************************************************************/
// WARNING: The COLUMN_QUERY mode must be kept in sync with
// HotlistModel::CompareHotlistModelItem()
OP_STATUS HotlistModelItem::GetItemData( ItemData* item_data )
{
	if (item_data->query_type == INIT_QUERY)
	{
		if (IsSeparator())
		{
			item_data->flags |= FLAG_SEPARATOR;

		}
		else if(IsFolder() && GetIsExpandedFolder())
		{
			item_data->flags |= FLAG_INITIALLY_OPEN;
		}

		return OpStatus::OK;
	}

	if (item_data->query_type == MATCH_QUERY)
	{
		BOOL match = FALSE;

		if (item_data->match_query_data.match_type == MATCH_FOLDERS)
		{
			match = IsFolder();
		}
		else //if(!IsFolder())
		{
			int mode = IN_URLS; //CASE_SENSITIVE|START_OF_STRING;
			const uni_char* needle = item_data->match_query_data.match_text->CStr();
			match = CompareStrings( GetName().CStr(), needle, mode );
			match = match || CompareStrings( GetDescription().CStr(), needle, mode );
			match = match || CompareStrings( GetShortName().CStr(), needle, mode );
			if( mode & IN_URLS )
			{
				match = match || CompareStrings( GetDisplayUrl().CStr(), needle, mode );
			}

			if( !match && IsContact())
			{
				match = match || CompareStrings( GetMail().CStr(), needle, mode );
				match = match || CompareStrings( GetPhone().CStr(), needle, mode );
				match = match || CompareStrings( GetFax().CStr(), needle, mode );
				match = match || CompareStrings( GetPostalAddress().CStr(), needle, mode );
			}
		}

		if (match)
		{
			item_data->flags |= FLAG_MATCHED;
		}
		return OpStatus::OK;
	}

	if (item_data->query_type == INFO_QUERY)
	{
		GetInfoText(*item_data->info_query_data.info_text);
		return OpStatus::OK;
	}

	if (item_data->query_type != COLUMN_QUERY)
	{
		return OpStatus::OK;
	}

	if( item_data->flags & FLAG_FOCUSED )
	{
		GetModel()->SetActiveItemId( GetID(), TRUE );
	}

	if( IsFolder() ) // ...
	{
		if( GetIsTrashFolder() )
		{
			int index = GetModel()->GetIndexByItem(this);
			if( index != -1 && GetModel()->GetChildIndex(index) != -1 )
			{
				item_data->flags |= FLAG_BOLD;
			}
		}

		if( GetIsExpandedFolder() )
		{
			if( !(item_data->flags & FLAG_OPEN) )
			{
				SetIsExpandedFolder( FALSE );
				GetModel()->SetDirty( TRUE, 10000 );
			}
		}
		else
		{
			if( item_data->flags & FLAG_OPEN )
			{
				SetIsExpandedFolder( TRUE );
				GetModel()->SetDirty( TRUE, 10000 );
			}
		}
	}

	if (item_data->column_query_data.column == 0)
	{
		item_data->column_query_data.column_text->Set(GetName());

#ifdef GADGET_SUPPORT
		if (IsGadget() 
#ifdef WEBSERVER_SUPPORT
		|| IsUniteService()
#endif // WEBSERVER_SUPPORT
)
		{
			//show running gadget with bold text
			if (IsGadget() && static_cast<GadgetModelItem*>(this)->IsGadgetRunning()
#ifdef WEBSERVER_SUPPORT
			    || IsUniteService() && static_cast<UniteServiceModelItem*>(this)->IsGadgetRunning()
#endif // WEBSERVER_SUPPORT
				)
			{
				item_data->flags |= FLAG_BOLD;
				item_data->flags &= ~FLAG_DISABLED;
			}
			else 
			{
				//item_data->flags |= (FLAG_DISABLED | FLAG_ACCEPTS_CLICK);
				item_data->flags &= ~FLAG_BOLD;
			}
		}
#endif // GADGET_SUPPORT

		if(((IsBookmark() 
#ifdef NOTES_USE_URLICON
			 || IsNote()
#endif // NOTES_USE_URLICON
			 )
			&& g_pcdoc->GetIntegerPref(PrefsCollectionDoc::AlwaysLoadFavIcon) != 0 )
#ifdef GADGET_SUPPORT
			 ||	IsGadget()
#ifdef WEBSERVER_SUPPORT			
		     || IsUniteService()
#endif  // WEBSERVER_SUPPORT			
#endif // GADGET_SUPPORT
		)
		{
			item_data->column_bitmap = GetIcon();
#ifdef WEBSERVER_SUPPORT			
			if(IsUniteService())
			{
				Image img = item_data->column_bitmap;
				if(img.IsEmpty())
				{
					OpWidgetImage wi;
					wi.SetImage(GetImage());
					OpSkinElement* e = wi.GetSkinElement();
					if(e)
					{
						img = e->GetImage(0);
					}
				}
				UniteServiceModelItem* service = static_cast<UniteServiceModelItem*>(this);

				if(!service->IsGadgetRunning())
				{
					OpBitmap* bitmap = img.GetEffectBitmap(Image::EFFECT_DISABLED, 128, null_image_listener);
					if(bitmap)
					{
						OpBitmap* bm = NULL;
						if(OpStatus::IsSuccess(OpBitmap::Create(&bm, img.Width(), img.Height(), FALSE, TRUE, 0, 0, TRUE)))
						{
							UINT32 line_count = bitmap->Height();
							UINT32* data = OP_NEWA(UINT32, bitmap->Width());
							if(!data)
							{
								img.ReleaseEffectBitmap();
								return OpStatus::ERR_NO_MEMORY;
							}
							
							for(UINT32 i=0; i<line_count; i++)
							{
								if(bitmap->GetLineData(data, i) == TRUE)
								{
									bm->AddLine(data, i);
								}
							}
							OP_DELETEA(data);
							item_data->column_bitmap = imgManager->GetImage(bm);
							if (item_data->column_bitmap.IsEmpty())     // OOM
								OP_DELETE(bm);
						}
						img.ReleaseEffectBitmap();
					}
				}
				else if(service->GetNeedsAttention())
				{
					OpBitmap* bitmap = img.GetBitmap(null_image_listener);
					if(bitmap)
					{
						OpBitmap* bm = NULL;
		
						UINT32 line_count = bitmap->Height();
					
						OpWidgetImage wi;
						wi.SetImage("Unite Attention");
						OpSkinElement* e = wi.GetSkinElement();
						if(e)
						{
							Image i = e->GetImage(0);

							OpBitmap* b = i.GetBitmap(null_image_listener);
							if(b)
							{
								if(b->Width() == img.Width() && b->Height() == img.Height() && bitmap->GetBpp() == 32 && b->GetBpp() == 32)
								{
									UINT32* data = OP_NEWA(UINT32, b->Width());
									UINT32* data2 = OP_NEWA(UINT32, bitmap->Width());
									if(!data || !data2)
									{
										img.ReleaseBitmap();
										i.ReleaseBitmap();
										return OpStatus::ERR_NO_MEMORY;
									}

									if(OpStatus::IsSuccess(OpBitmap::Create(&bm, img.Width(), img.Height())))
									{
										for(UINT32 j=0; j<line_count; j++)
										{
											if(b->GetLineData(data, j) && bitmap->GetLineData(data2, j))
											{
												BlitHLineAlpha32(data, data2, img.Width());
												bm->AddLine(data2, j);
											}
										}
									}
									OP_DELETEA(data);
									OP_DELETEA(data2);
								}
								i.ReleaseBitmap();
							}
							
							if (bm)
							{
								item_data->column_bitmap = imgManager->GetImage(bm);
								if (item_data->column_bitmap.IsEmpty())     // OOM
									OP_DELETE(bm);
							}
						}
						img.ReleaseBitmap();
					}
				}
			}
#endif // WEBSERVER_SUPPORT
		}
		if (item_data->column_bitmap.IsEmpty())
		{
			item_data->column_query_data.column_image = GetImage(); // char
		}
	}

	else
	{
		switch (GetModel()->GetModelType())
		{
			case HotlistModel::BookmarkRoot:
			{
				switch (item_data->column_query_data.column)
				{
					case 1:	// nickname
					{
						item_data->column_query_data.column_text->Set(GetShortName());
						break;
					}
					case 2:	// url
					{
						if( GetDisplayUrl().Find(UNI_L("@")) == KNotFound )
						{
							item_data->column_query_data.column_text->Set(GetDisplayUrl());
						}
						else
						{
							URL	url = urlManager->GetURL(GetDisplayUrl().CStr());
							url.GetAttribute(URL::KUniName_Username_Password_Hidden, *item_data->column_query_data.column_text);
						}
						break;
					}
					case 3:	// description
					{
						item_data->column_query_data.column_text->Set(GetDescription());
						break;
					}
					case 4:	// created
					{
						item_data->column_query_data.column_text->Set(GetCreatedString());
						break;
					}
					case 5:	// visited
					{
						item_data->column_query_data.column_text->Set(GetVisitedString());
						break;
					}
				}
				break;
			}
			case HotlistModel::ContactRoot:
			{
				switch (item_data->column_query_data.column)
				{
					case 1:	// e-mail
					{
						item_data->column_query_data.column_text->Set(GetMail());
						break;
					}
					case 2:	// phone
					{
						item_data->column_query_data.column_text->Set(GetPhone());
						break;
					}
				}
				break;
			}

		}
	}

	if(IsFolder())
	{
		item_data->column_query_data.column_sort_order = -1;
	}

	if(GetIsTrashFolder()) // ...
	{
		item_data->column_query_data.column_sort_order = 1;
	}

	return OpStatus::OK;
}


/******************************************************************************
 *
 * HotlistModelItem::CompareStrings (static)
 *
 *
 ******************************************************************************/
// static
BOOL HotlistModelItem::CompareStrings( const uni_char* haystack, 
									   const uni_char* needle, 
									   int mode )
{
	if( !haystack || !needle )
	{
		return FALSE;
	}

	if( mode & ENTIRE_STRING )
	{
		if( mode & CASE_SENSITIVE )
		{
			return uni_strcmp( haystack, needle ) == 0;
		}
		else
		{
			return uni_stricmp( haystack, needle ) == 0;
		}
	}
	else if( mode & START_OF_STRING )
	{
		if( mode & CASE_SENSITIVE )
		{
			return uni_strncmp( haystack, needle, uni_strlen(needle) ) == 0;
		}
		else
		{
			return uni_strnicmp( haystack, needle, uni_strlen(needle) ) == 0;
		}
	}
	else
	{
		if( mode & CASE_SENSITIVE )
		{
			return uni_strstr( haystack, needle ) != 0;
		}
		else
		{
			return uni_stristr( haystack, needle ) != 0;
		}
	}
}


/******************************************************************************
 *
 * HotlistModelItem::GetIsInsideTrashFolder
 *
 * @return TRUE if this item is inside Trash folder
 *
 ******************************************************************************/
// TODO: OBS: Use IsDescendantOf instead
BOOL HotlistModelItem::GetIsInsideTrashFolder() const
{
	const HotlistModelItem *hmi = this;
	while( hmi )
	{
		if( hmi->GetIsTrashFolder() )
		{
			return TRUE;
		}
		hmi = hmi->GetParentFolder();
	}
	return FALSE;
}


/******************************************************************************
 *
 * HotlistModelItem::GetParentFolder
 *
 *
 ******************************************************************************/

HotlistModelItem *HotlistModelItem::GetParentFolder() const
{
	// return GetModel() ? GetModel()->GetParentFolder( GetID() ) : 0;
	// return GetModel() ? GetModel()->GetParentFolderByIndex( GetIndex() ) : 0;
	return GetParentItem();
}


/******************************************************************************
 *
 * HotlistModelItem::GetParentID
 *
 *
 ******************************************************************************/

INT32 HotlistModelItem::GetParentID()
{
	HotlistModelItem *folder = GetParentFolder();
	return folder ? folder->GetID() : -1;
}


/******************************************************************************
 *
 * HotlistModelItem::IsChildOf
 *
 * @param HotlistModelItem* candidate_parent - 
 *
 * @return TRUE if this item is child of param candidate_parent
 *
 ******************************************************************************/
BOOL HotlistModelItem::IsChildOf( HotlistModelItem* candidate_parent )
{
	if( !candidate_parent || candidate_parent->GetModel() != GetModel() )
	{
		return FALSE;
	}
	else
	{
		return GetModel()->IsChildOf( GetModel()->GetIndexByItem(candidate_parent), GetModel()->GetIndexByItem(this) );
	}
}


/******************************************************************************
 *
 * HotlistModelItem::SetUnknownTag
 *
 * @param uni_char* tag - 
 *
 * Appends param tag to m_unknown_tags. 
 *
 ******************************************************************************/

void HotlistModelItem::SetUnknownTag( const uni_char *tag )
{
	if( tag && tag[0] )
	{
		m_unknown_tags.Append( tag );
	}
}

/******************************************************************************
 *
 * HotlistModelItem::GetIsInMoveIsCopy
 *
 * @returns - If item is itself or is a child of a folder with 
 *          the MoveIsCopy attribute set, returns a pointer to the MoveIsCopy
 *          parent (or item itself), else it returns NULL.
 *
 *
 ******************************************************************************/

HotlistModelItem *HotlistModelItem::GetIsInMoveIsCopy()
{
	HotlistModelItem *parent_item, *child_item = this;

	// First check catches if this item is a folder and has the property set
	if (child_item->GetHasMoveIsCopy())
		return child_item;

	// We need to check this item, then all parent folders until we hit root
	// or we find a folder with the m_move_is_copy member set
	while ((parent_item = child_item->GetParentFolder()) != NULL)
	{
		// Property on a parent folder found so just hop out
		if (parent_item->GetHasMoveIsCopy())
			return parent_item;

		// Move up the tree
		child_item = parent_item;
	}

	return NULL;
}


/******************************************************************************
 *
 * HotlistModelItem::GetIsInMaxItemsFolder
 *
 * @returns - If item is itself or is a child of a folder with the maxItems
 *              property set, returns a pointer to the first folder
 *              met on traversing tree bottom-up, with the MaxItems property
 *              set. If none, returns NULL.
 *
 * NOTE/TODO: Does not handle MaxItemsFolder in MaxItemFolder properly.
 *            (Say if a folder with maxItems=5 contains a folder with maxItems=20)
 *
 ******************************************************************************/

FolderModelItem *HotlistModelItem::GetIsInMaxItemsFolder()
{
	HotlistModelItem *parent_item, *child_item = this;

	// First check catches if this item is a folder and has the property set
	if (child_item->IsFolder() && child_item->GetMaxItems() > 0)
		return static_cast<FolderModelItem*>(child_item);

	// We need to check this item, then all parent folders until we hit root
	// or we find a folder with the m_move_is_copy member set
	while ((parent_item = child_item->GetParentFolder()) != NULL)
	{
		// Property on a parent folder found so just hop out
		if (parent_item->IsFolder() && parent_item->GetMaxItems() > 0)
			return static_cast<FolderModelItem*>(parent_item);

		// Move up the tree
		child_item = parent_item;
	}

	return NULL;
}


/******************************************************************************
 *
 * HotlistModelItem::GetMenuName
 *
 *
 ******************************************************************************/
const OpStringC &HotlistModelItem::GetMenuName()
{
	static OpString menu_name;
	const uni_char* src = GetName().CStr();
	int new_size = 0;
	BOOL must_escape = FALSE;

	if (src)
	{
		const uni_char* p = src;
		while (*p)
		{
			if (*p == '&')
			{
				new_size++;
				must_escape = TRUE;
			}
			new_size++;
			p++;
		}
	}

	if (!must_escape)
	{
		return GetName();
	}

	uni_char *p = menu_name.Reserve(new_size);
	if (!p)
		return GetName();

	do
	{
		if (*src == '&')
			*p++ = '&';
	} while ((*p++ = *src++) != '\0');

	return menu_name;
}


/******************************************************************************
 *
 * HotlistModelItem::ContainsMailAddress
 *
 *
 ******************************************************************************/
//NOTE: Contact only?

BOOL HotlistModelItem::ContainsMailAddress( const OpStringC& address )
{
	static const uni_char* delimiter = UNI_L(" ,");
	OpString item_mail;

	if (GetMail().CompareI(address.CStr()) == 0)
		return TRUE;
	if (GetMail().FindFirstOf(',') != KNotFound)
	{
		item_mail.Set(GetMail().CStr());
		if( item_mail.CStr() )
		{
			const uni_char* mail_token = uni_strtok(item_mail.CStr(), delimiter);
			while (mail_token)
			{
				if (address.CompareI(mail_token) == 0)
					return TRUE;
				mail_token = uni_strtok(NULL, delimiter);
			}
		}
	}

	return FALSE;
}


GenericModelItem::GenericModelItem()
	:HotlistModelItem()
{
	m_created = 0;
	m_visited = 0;
	m_personalbar_position = -1;
	m_panel_position = -1;
	m_smallscreen = FALSE;
	m_upgrade_icon = TRUE; // Set to FALSE after first time we activate bookmark
}

GenericModelItem::GenericModelItem( const GenericModelItem& item )
	: HotlistModelItem(item)
{
	m_created = item.m_created;
	m_visited = item.m_visited;
	m_personalbar_position = item.m_personalbar_position;
	m_panel_position = item.m_panel_position;
	m_smallscreen = item.m_smallscreen;
	m_name.Set( item.m_name );
	m_url.Set( item.m_url ); 
	m_description.Set( item.m_description ); 
	m_shortname.Set( item.m_shortname ); 

}


GenericModelItem::~GenericModelItem()
{
}

/******************************************************************************
 *
 * GenericModelItem::SetName
 *
 *
 ******************************************************************************/

void GenericModelItem::SetName( const uni_char* str )
{
	if (GetIsTrashFolder())
	{
		OpString text;
		g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_TRASH, text);
		m_name.Set( text.CStr() );
	}
	else
	{
		m_name.Set(str);
	}
}


/******************************************************************************
 *
 * GenericModelItem::GetResolvedUrl
 *
 *

 ******************************************************************************/
const OpStringC& GenericModelItem::GetResolvedUrl()
{
	if( m_resolved_url.IsEmpty() )
	{
		OpString tmp;
		if (g_url_api->ResolveUrlNameL(GetUrl(), tmp))
		{
			m_resolved_url.Set(tmp);
		}
	}

	return m_resolved_url;
}


/******************************************************************************
 *
 * GenericModelItem::GetCreatedString
 *
 * @return OpStringC& representation of created value on bookmark item.
 *         Calls BuildTimeString if created string isn't set yet
 *
 ******************************************************************************/

const OpStringC& GenericModelItem::GetCreatedString()
{
	if( !m_created_string.HasContent() )
	{
		if( GetCreated() > 0 )
		{
			BuildTimeString( m_created_string, GetCreated(), TRUE );
		}
	}
	return m_created_string;
}


/******************************************************************************
 *
 * GenericModelItem::GetVisitedString
 *
 * @return OpStringC& representation of visited value on bookmark item
 *         Calls BuildTimeString to create the string if visited string isnt set
 *
 ******************************************************************************/

const OpStringC& GenericModelItem::GetVisitedString()
{
	if( !m_visited_string.HasContent() )
	{
		if( GetVisited() > 0 )
		{
			BuildTimeString( m_visited_string, GetVisited(), TRUE );
		}
	}
	return m_visited_string;
}

/******************************************************************************
 *
 * GenericModelItem::GetInfoText
 *
 *
 ******************************************************************************/

void GenericModelItem::GetInfoText( OpInfoText &text )
{
	OpString str;
	g_languageManager->GetString(Str::SI_LOCATION_TEXT, str);

	if( GetDisplayUrl().Find(UNI_L("@")) == KNotFound )
	{
		text.SetStatusText(GetDisplayUrl().CStr());
		text.AddTooltipText(str.CStr(), GetDisplayUrl().CStr());
	}
	else
	{
		URL	url = g_url_api->GetURL(GetDisplayUrl().CStr());
		OpString status;
		url.GetAttribute(URL::KUniName_Username_Password_Hidden, status);
		text.SetStatusText(status.CStr());
		text.AddTooltipText(str.CStr(), status.CStr());
	}

	g_languageManager->GetString(Str::DI_ID_EDITURL_SHORTNAMELABEL, str);
	text.AddTooltipText(str.CStr(), GetShortName().CStr());
	g_languageManager->GetString(Str::DI_ID_EDITURL_DESCRIPTIONLABEL, str);
	text.AddTooltipText(str.CStr(), GetDescription().CStr());
	g_languageManager->GetString(Str::DI_IDLABEL_HL_CREATED, str);
	text.AddTooltipText(str.CStr(), GetCreatedString().CStr());
	g_languageManager->GetString(Str::DI_IDLABEL_HL_LASTVISITED, str);
	text.AddTooltipText(str.CStr(), GetVisitedString().CStr());
}

/******************************************************************************
 *
 * GenericModelItem::SetCreated
 *
 *
 ******************************************************************************/

void GenericModelItem::SetCreated( time_t value )
{
	m_created = value;
	m_created_string.Empty();

	if (GetModel())
		GetModel()->BroadcastHotlistItemChanged(this, FALSE, HotlistModel::FLAG_CREATED);
}


/******************************************************************************
 *
 * GenericModelItem::SetVisited
 *
 *
 ******************************************************************************/

void GenericModelItem::SetVisited( time_t value )
{
	m_visited = value;
	m_visited_string.Empty();

	if (GetModel())
		GetModel()->BroadcastHotlistItemChanged( this, FALSE, HotlistModel::FLAG_VISITED);
}


/******************************************************************************
 *
 * GenericModelItem::SetShortName
 *
 *
 ******************************************************************************/
void GenericModelItem::SetShortName( const uni_char* str )
{
	m_shortname.Set( str && uni_strlen(str) > 0 ? str : 0 ) ;
	// Don't allow whitespace around it
	m_shortname.Strip();

}


BOOL GenericModelItem::GetSeparatorsAllowed() 
{
	HotlistModelItem* parent = GetParentFolder();
	if (!parent)
		return TRUE;
	else
		return parent->GetSeparatorsAllowed();
}

BOOL GenericModelItem::ClearVisited()
{
	if (GetVisited() != 0)
	{
		SetVisited(0);
		return TRUE;
	}
	return FALSE;
}

BOOL GenericModelItem::ClearCreated()
{
	if (GetCreated() != 0)
	{
		SetCreated(0);
		return TRUE;
	}
	return FALSE;
}


/***********************************************************************************
**
**	UniteFolderModelItem
**
***********************************************************************************/
#ifdef WEBSERVER_SUPPORT

UniteFolderModelItem::UniteFolderModelItem() : FolderModelItem()
{

}

UniteFolderModelItem::UniteFolderModelItem(INT32 dtype) : FolderModelItem(dtype)
{

}

UniteFolderModelItem::~UniteFolderModelItem()
{

}
UniteFolderModelItem::UniteFolderModelItem( const UniteFolderModelItem& item ) : FolderModelItem(item)
{

}

HotlistModelItem *UniteFolderModelItem::GetDuplicate()
{
	return OP_NEW(UniteFolderModelItem, (*this));
}

OP_STATUS UniteFolderModelItem::GetItemData( ItemData* item_data )
{
	if (item_data->query_type == ASSOCIATED_TEXT_QUERY)
	{
		return OpStatus::OK;
	}
	return FolderModelItem::GetItemData(item_data);
}

const char* UniteFolderModelItem::GetImage() const
{
	if(GetIsTrashFolder())
	{
		return "Unite Trash";
	}
	else
	{
		return "Unite Folder";
	}
}

#endif // WEBSERVER_SUPPORT

/***********************************************************************************
 **
 **	FolderModelItem
 **
 ***********************************************************************************/

FolderModelItem::FolderModelItem()
	:GenericModelItem()
{
	m_is_trashfolder     = FALSE;
	m_is_activefolder    = FALSE;
	m_is_expandedfolder  = FALSE;
	m_is_linkbarfolder   = FALSE;
	m_move_is_copy       = FALSE;
	m_deletable          = TRUE;
	m_subfolders_allowed = TRUE;
	m_separators_allowed = TRUE;
	m_max_items          = -1;

}

FolderModelItem::FolderModelItem(INT32 dtype)
	:GenericModelItem()
{
	m_is_trashfolder     = FALSE;
	m_is_activefolder    = FALSE;
	m_is_expandedfolder  = FALSE;
	m_is_linkbarfolder   = FALSE;
	m_move_is_copy       = FALSE;
	m_deletable          = TRUE;
	m_subfolders_allowed = TRUE;
	m_separators_allowed = TRUE;
	m_max_items          = -1;
}


FolderModelItem::FolderModelItem( const FolderModelItem& item )
	:GenericModelItem(item)
{
	m_is_trashfolder     = item.m_is_trashfolder;
	m_is_activefolder    = item.m_is_activefolder;
    m_is_expandedfolder  = item.m_is_expandedfolder;
	m_is_linkbarfolder   = item.m_is_linkbarfolder;
	m_move_is_copy       = item.m_move_is_copy;
	m_deletable          = item.m_deletable;
	m_subfolders_allowed = item.m_subfolders_allowed;
	m_separators_allowed = item.m_separators_allowed;
	m_max_items          = item.m_max_items;
	m_target.Set(item.m_target);
}

BOOL FolderModelItem::GetMaximumItemReached(INT32 count)
{
	HotlistModelItem* max_item_parent = GetIsInMaxItemsFolder();
	if (max_item_parent
		&& max_item_parent->IsFolder() 
		&& max_item_parent->GetMaxItems()  > 0
		&& max_item_parent->GetChildCount() + count > max_item_parent->GetMaxItems())
		return TRUE;
	
	return FALSE;
}

FolderModelItem::~FolderModelItem()
{
}


/******************************************************************************
 *
 * FolderModelItem::GetDuplicate
 *
 *
 ******************************************************************************/

HotlistModelItem *FolderModelItem::GetDuplicate( )
{
	return OP_NEW(FolderModelItem, (*this));
}


/******************************************************************************
 *
 * FolderModelItem::GetImage
 *
 *
 ******************************************************************************/

const char* FolderModelItem::GetImage() const
{
	if( GetIsTrashFolder() )
	{
		return "Trash";
	}
	else if(GetIsSpecialFolder())
	{
		return "Target Folder";
	}
	else
	{
		return "Folder";
	}
}

/******************************************************************************
 *
 * HotlistModelItem::GetInfoText
 *
 *
 ******************************************************************************/
void FolderModelItem::GetInfoText( OpInfoText &text )
{
	text.SetStatusText(GetName().CStr());

	OpString str;

	g_languageManager->GetString(Str::DI_ID_EDITURL_SHORTNAMELABEL, str);
	text.AddTooltipText(str.CStr(), GetShortName().CStr());
	g_languageManager->GetString(Str::DI_IDLABEL_HL_CREATED, str);
	text.AddTooltipText(str.CStr(), GetCreatedString().CStr());
}


/***********************************************************************************
 **
 **	NoteModelItem
 **
 ***********************************************************************************/

NoteModelItem::NoteModelItem()
	:GenericModelItem()
{
}

NoteModelItem::NoteModelItem( const NoteModelItem& item )
	:GenericModelItem(item)
{
}


NoteModelItem::~NoteModelItem()
{
}

#ifdef NOTES_USE_URLICON
Image NoteModelItem::GetIcon()
{
	//if (GetUrl())
	return g_favicon_manager->Get(GetUrl().CStr());
}
#endif // NOTES_USE_URLICON

/******************************************************************************
 *
 * NoteModelItem::GetDuplicate
 *
 ******************************************************************************/

HotlistModelItem *NoteModelItem::GetDuplicate( )
{
	return OP_NEW(NoteModelItem, (*this));
}

/******************************************************************************
 *
 * NoteModelItem::GetImage
 *
 ******************************************************************************/

const char* NoteModelItem::GetImage() const
{
	return GetUrl().HasContent() ? "Note Web" : "Note";
}


/******************************************************************************
 *
 * NoteModelItem::GetInfoText
 *
 *
 ******************************************************************************/
void NoteModelItem::GetInfoText( OpInfoText &text )
{
	text.SetStatusText(GetName().CStr());

	OpString str;
	g_languageManager->GetString(Str::DI_IDL_SOURCE, str);

	if( GetUrl().Find(UNI_L("@")) == KNotFound )
	{
		text.AddTooltipText(str.CStr(), GetUrl().CStr());
	}
	else
	{
		URL	url = g_url_api->GetURL(GetUrl().CStr());
		OpString url_string;
		url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_string);
		text.AddTooltipText(str.CStr(), url_string.CStr());
	}
	g_languageManager->GetString(Str::DI_IDLABEL_HL_CREATED, str);
	text.AddTooltipText(str.CStr(), GetCreatedString().CStr());
	text.AddTooltipText(NULL, UNI_L(" "));
	text.AddTooltipText(NULL, GetName().CStr());
}



#ifdef GADGET_SUPPORT

/***********************************************************************************
 **
 **	GadgetModelItem
 **
 ***********************************************************************************/

// TODO: AcitivityListener only for UniteServices
GadgetModelItem::GadgetModelItem()
	: GenericModelItem(), DesktopWindowListener(),
	  m_gadget_window(NULL),
	  m_perform_clean_uninstall(FALSE),
	  m_image(NULL),
#ifdef WEBSERVER_SUPPORT
	  m_upgraded(FALSE),
#endif // WEBSERVER_SUPPORT
	  m_ignore_notifications(FALSE)
{
#ifdef WEBSERVER_SUPPORT
	m_button_image.Set("Edit Unite Service");	 // Only Unite Services
#endif // WEBSERVER_SUPPORT
}


GadgetModelItem::GadgetModelItem(INT32 dtype)
	: GenericModelItem(), DesktopWindowListener(),
	  m_gadget_window(NULL),
	  m_perform_clean_uninstall(FALSE),
	  m_image(NULL),
#ifdef WEBSERVER_SUPPORT
	  m_upgraded(FALSE),
#endif // WEBSERVER_SUPPORT
	  m_ignore_notifications(FALSE)
{
#ifdef WEBSERVER_SUPPORT
	m_button_image.Set("Edit Unite Service");	// Only Unite Services
#endif // WEBSERVER_SUPPORT
}

GadgetModelItem::GadgetModelItem( const GadgetModelItem& item )
	: GenericModelItem(item), DesktopWindowListener(),
	  m_gadget_window(item.GetDesktopGadget()),
	  m_perform_clean_uninstall(item.GetGadgetCleanUninstall()),
	  m_image(NULL),
	  m_ignore_notifications(item.m_ignore_notifications)
{
	m_gadget_id.Set(item.GetGadgetIdentifier());

#ifdef WEBSERVER_SUPPORT
	m_upgraded = item.m_upgraded;
	m_button_image.Set(item.m_button_image.CStr());	 // m_button_image only needed for UniteServices
#endif // WEBSERVER_SUPPORT

	//We take over a running gadget, so we should listen to it
	if (m_gadget_window)
	{
		m_gadget_window->AddListener(this);
// #ifdef WEBSERVER_SUPPORT
// 		m_gadget_window->AddActivityListener(this);
// #endif
	}
}


GadgetModelItem::~GadgetModelItem()
{
	//remove ourselves as listener if a gadget is still running
	if (m_gadget_window)
	{
		m_gadget_window->RemoveListener(this);
	}

	if(m_image)
	{
		OP_DELETE(m_image);
	}
#ifdef WEBSERVER_SUPPORT
	if(!m_scaled_image.IsEmpty())
	{
		m_scaled_image.DecVisible(null_image_listener);
	}
#endif // WEBSERVER_SUPPORT
}


/******************************************************************************
 *
 * GadgetModelItem::GetImage
 *
 *
 ******************************************************************************/
const char* GadgetModelItem::GetImage() const
{
	//standard icon for widgets
	return "Widget";
}

/******************************************************************************
 *
 * GadgetModelItem::GetInfoText
 *
 *
 ******************************************************************************/
void GadgetModelItem::GetInfoText( OpInfoText &text )
{
	text.SetStatusText(GetName().CStr());

	text.AddTooltipText(NULL, GetName().CStr());
}

/******************************************************************************
 *
 * GadgetModelItem::GetOpGadget
 *
 * Does a lookup of the OpGadget for this GadgetModelItem based on its
 * GadgetIdentifier.
 *
 ******************************************************************************/
OpGadget* GadgetModelItem::GetOpGadget()
{
	OP_ASSERT(m_gadget_id.HasContent());
	if (m_gadget_id.IsEmpty())
		return NULL;
	//try to find a running instance

	OpGadget* op_gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_ID, m_gadget_id);
	return op_gadget;
}


/******************************************************************************
 *
 * GadgetModelItem::EqualsID
 *
 *
 ******************************************************************************/
BOOL
GadgetModelItem::EqualsID(const OpStringC & gadget_id, const OpStringC & alternate_gadget_id)
{
	OpString id;
	id.Set(gadget_id);
	if (id.IsEmpty())
	{
		id.Set(alternate_gadget_id);
	}
	OP_ASSERT(id.HasContent());
	if (id.HasContent())
	{
		OpGadget* gadget = GetOpGadget();
		OP_ASSERT(gadget);
		if (gadget)
		{
			OpString local_id;
			RETURN_VALUE_IF_ERROR(local_id.Set(gadget->GetGadgetId()), FALSE);
			if (local_id.IsEmpty())
			{
				RETURN_VALUE_IF_ERROR(gadget->GetGadgetDownloadUrl(local_id), FALSE);
			}
			//OP_ASSERT(local_id.HasContent());

			if (local_id.HasContent() && local_id.Compare(gadget_id) == 0)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/******************************************************************************
 *
 * GadgetModelItem::CloseGadgetWindow
 *
 *
 ******************************************************************************/
void GadgetModelItem::CloseGadgetWindow(BOOL immediately, 
										BOOL user_initiated, 
										BOOL force)
{
	if (m_gadget_window)
	{
		// This listener MUST BE removed before setting m_gadget_window to NULL, as the destructor
		// usually removes it
		m_gadget_window->RemoveListener(this);
		
		m_gadget_window->Close(immediately, user_initiated, force);
		m_gadget_window = NULL;
		Change();
	}
}

/******************************************************************************
 *
 * GadgetModelItem::RestartGadget
 *
 *
 ******************************************************************************/
OP_STATUS GadgetModelItem::RestartGadget(BOOL launch)
{

	// Shutdown the gadget if it's running
	if (launch)
	{
		ShowGadget(FALSE, FALSE);
	}

#ifdef WEBSERVER_SUPPORT
	m_scaled_image.Empty(); // make sure the icon isn't cached

	if (IsUniteService())
	{
		UniteServiceModelItem * unite_item = static_cast<UniteServiceModelItem *>(this);
		if (unite_item && unite_item->NeedsConfiguration())
		{
			unite_item->SetNeedsConfiguration(FALSE);
		}
	}
#endif // WEBSERVER_SUPPORT

	// if the gadget has been running, restart it
	// (and write 'RUNNING=YES' for into unite.adr for unite apps)
	if (launch)
		ShowGadget(TRUE, TRUE);


	return OpStatus::OK;
}

BOOL
GadgetModelItem::IsInsideGadgetFolder(const OpStringC & gadget_path)
{
	OpString tmp_storage;
	return gadget_path.Find(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_GADGET_FOLDER, tmp_storage)) == 0;
}


/******************************************************************************
 *
 * HotlistModelItem::InstallGadget
 *
 * @param OpStringC& address - 
 * @param OpStringC& orig_url - 
 * @param BOOL clean_uninstall - 
 * @param launch_after_install - 
 * @param OpStringC& force_widget_name - 
 * @param OpStringC& force_service_name - 
 *
 *
 *
 ******************************************************************************/
OP_STATUS GadgetModelItem::InstallGadget(const OpStringC& address,
										 const OpStringC& orig_url,
										 BOOL clean_uninstall,
										 BOOL launch_after_install,
										 const OpStringC& force_widget_name,
										 const OpStringC& force_service_name,
										 const OpStringC& shared_path,
										 URLContentType install_type)
{
	// The valid gadget check may change the path, so we can't use a const OpString as an arg.
	OpString checked_address;
	RETURN_IF_ERROR(checked_address.Set(address));

	//first check if the address really points to a valid gadget
	if (!g_gadget_manager->IsThisAGadgetPath(checked_address))
	{
		return OpStatus::ERR;
	}

	SetGadgetCleanUninstall(clean_uninstall);

	OpGadget* opgadget = NULL;

	//Only install the gadget, not run
	//Ask the gadget manager to create a gadget installation based on the package file
	RETURN_IF_ERROR(g_gadget_manager->CreateGadget(&opgadget, checked_address, install_type));

	// Save the original download URL of the gadget
	OpString download_url;
	if (opgadget->GetGadgetId())
	{
		download_url.Set(opgadget->GetGadgetId());
	}
	else if (orig_url.HasContent())
	{
		download_url.Set(orig_url.CStr());
	}

	opgadget->SetGadgetDownloadUrl(download_url);

	//Set name defined in widget configuration or generic name if not available
	OpString widget_name;

	// Change the name of the gadget if passed in
	if (!force_widget_name.HasContent())
	{
		// Not forced so get the name from the gadget
		opgadget->GetGadgetName(widget_name);
		if (widget_name.IsEmpty())
		{
			//take the default widget/service name
#ifdef WEBSERVER_SUPPORT
			if (opgadget->GetClass() && opgadget->GetClass()->IsSubserver())
			{
				g_languageManager->GetString(Str::S_UNNAMED_SERVICE, widget_name);
			}
			else
#endif // WEBSERVER_SUPPORT
			{
				g_languageManager->GetString(Str::S_UNNAMED_WIDGET, widget_name);
			}
		}
	}
	else
		widget_name.Set(force_widget_name);
	
#ifdef GADGETS_CAP_HAS_SETGADGETNAME
	opgadget->SetGadgetName(widget_name.CStr());
#endif // GADGETS_CAP_HAS_SETGADGETNAME
	SetName(widget_name.CStr());

	//remember the ID, this is the key to the unique gadget instance
	SetGadgetIdentifier(opgadget->GetIdentifier());

#ifdef WEBSERVER_SUPPORT
	// Change the virtual path/service name of the gadget if passed in
	if (force_service_name.HasContent())
	{
		opgadget->SetUIData(UNI_L("serviceName"), force_service_name.CStr());
		opgadget->SetupWebSubServer();
	}
	if(shared_path.HasContent())
	{
#ifdef GADGETS_CAP_HAS_SHAREDFOLDER
		opgadget->SetSharedFolder(shared_path.CStr());
#endif // GADGETS_CAP_HAS_SHAREDFOLDER
	}
#endif // WEBSERVER_SUPPORT

	//Let the gadget manager save all gadgets that are currently installed
	g_gadget_manager->SaveGadgets();

	if (launch_after_install)
	{
		return ShowGadget(TRUE, TRUE);
	}
	
	return OpStatus::OK;
}


/******************************************************************************
 *
 * GadgetModelItem::UninstallGadget
 *
 *
 ******************************************************************************/
OP_STATUS GadgetModelItem::UninstallGadget()
{
	ShowGadget(FALSE);
	OpGadget* op_gadget = GetOpGadget();
	return g_desktop_gadget_manager->UninstallGadget(op_gadget); 
}


/******************************************************************************
 *
 * GadgetModelItem::OnDesktopWindowClosing
 *
 *
 ******************************************************************************/
void GadgetModelItem::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (m_gadget_window == desktop_window)
	{
		m_gadget_window->RemoveListener(this);
		//remember that there is no running gadget anymore
		m_gadget_window = NULL;

		//update treeview with status
		Change();
		GetModel()->SetDirty(TRUE);
	}
}


/******************************************************************************
 *
 * GadgetModelItem::OnDesktopWindowChanged
 *
 *
 ******************************************************************************/
void GadgetModelItem::OnDesktopWindowChanged(DesktopWindow* desktop_window)
{
	if (m_gadget_window == desktop_window)
	{
		Change();
	}
}


/******************************************************************************
 *
 * GadgetModelItem::OnDesktopWindowStatusChanged
 *
 *
 ******************************************************************************/
void GadgetModelItem::OnDesktopWindowStatusChanged(DesktopWindow* desktop_window, DesktopWindowStatusType type)
{
#ifdef WEBSERVER_SUPPORT
	if (m_gadget_window &&
		m_gadget_window == desktop_window &&
		type != STATUS_TYPE_TITLE)
	{
		OpStringC status(m_gadget_window->DesktopWindow::GetStatusText(type));
		m_item_status.Wipe();
		if (status.HasContent())
			m_item_status.Set(status.CStr());
		Change();
	}
#endif //WEBSERVER_SUPPORT
}


/******************************************************************************
 *
 * GadgetModelItem::GetIcon
 *
 *
 ******************************************************************************/
Image GadgetModelItem::GetIcon()
{
#ifdef WEBSERVER_SUPPORT
	if (!m_scaled_image.IsEmpty())
	{
		return m_scaled_image;
	}
	else if(IsUniteService() && GetOpGadget())
	{
		g_desktop_gadget_manager->GetGadgetIcon(GetOpGadget()->GetClass(), m_scaled_image);
		return m_scaled_image;
	}
#endif // WEBSERVER_SUPPORT
	// code path needed to fix DSK-250101
	OpGadget* op_gadget = GetOpGadget();
	if ( op_gadget && !m_image )
	{
		OpString icon_path;
		INT32 width, height;
		OP_STATUS err = op_gadget->GetGadgetIcon(0, icon_path, width, height);

		if( !OpStatus::IsError(err) )
		{
			//try to construct an image with the icon file name from OpGadget
			m_image = OP_NEW(SimpleFileImage, (icon_path.CStr()));
			if( m_image && m_image->GetImage().IsEmpty())
			{
				OP_DELETE(m_image);
				m_image = NULL;
			}
		}
	}
	//return the constructed image if it exists
	return m_image ? m_image->GetImage() : Image();
}
#endif // GADGET_SUPPORT


#ifdef WEBSERVER_SUPPORT

/***********************************************************************************
 **
 **	UniteServicesModelItem
 **
 ***********************************************************************************/

UniteServiceModelItem::~UniteServiceModelItem()
{
	if (m_gadget_window)
	{
		m_gadget_window->RemoveActivityListener(this);
	}

	if (m_service_window)
	{
		m_service_window->RemoveListener(this);
	}

	OP_DELETE(m_item_activity_timer);
}

UniteServiceModelItem::UniteServiceModelItem()
	: GadgetModelItem()
	, DesktopGadget::ActivityListener()
	, OpTimerListener()
	, m_item_activity_timer(NULL)
	, m_item_activity_timer_count(0)
	, m_is_root_service(FALSE)
	, m_needs_configuration(FALSE)
	, m_add_to_auto_start(FALSE)
	, m_open_page(FALSE)
	, m_can_open_page(FALSE)
	, m_needs_attention(FALSE)
	, m_attention_state_changed(FALSE)
	, m_service_window(NULL)
{
}


UniteServiceModelItem::UniteServiceModelItem(const UniteServiceModelItem& item)
	: GadgetModelItem(item)
	, DesktopGadget::ActivityListener()
	, OpTimerListener()
	, m_item_activity_status()
	, m_item_activity_timer(NULL)
	, m_item_activity_timer_count(0)
	, m_is_root_service(item.m_is_root_service)
	, m_needs_configuration(item.m_needs_configuration)
	, m_add_to_auto_start(FALSE)
	, m_open_page(item.m_open_page)
	, m_can_open_page(FALSE)
	, m_needs_attention(FALSE)
	, m_attention_state_changed(FALSE)
	, m_service_window(NULL)
{
	m_item_activity_status.Set(item.m_item_activity_status);

	m_is_root_service = item.IsRootService();
	if (m_gadget_window)
	{
		if (!item.IsRootService())
			m_gadget_window->AddActivityListener(this);
	}
	if (item.m_service_window)
	{
		OpStatus::Ignore(SetServiceWindow(item.m_service_window));
	}
}

/******************************************************************************
 *
 * UniteServiceModelItem::GetDuplicate
 *
 *
 ******************************************************************************/

UniteServiceModelItem* UniteServiceModelItem::GetDuplicate()
{
	return OP_NEW(UniteServiceModelItem, (*this));
}


#ifdef GADGET_UPDATE_SUPPORT
OP_STATUS UniteServiceModelItem::ContainsStubGadget(BOOL& stub)
{
	stub = FALSE;
	OpString version;
	OpGadget* op_gadget = GetOpGadget();
	if (!op_gadget)
		return OpStatus::ERR;
	RETURN_IF_ERROR(version.Append(op_gadget->GetClass()->GetGadgetVersion()));
	if (version.Compare(UNI_L("0")) == 0)
		stub = TRUE;

	return OpStatus::OK;
}
#endif //GADGET_UPDATE_SUPPORT

/******************************************************************************
 *
 * UniteServiceModelItem::ShowGadget
 * = start the Unite application 
 *
 ******************************************************************************/
OP_STATUS UniteServiceModelItem::ShowGadget(BOOL show, BOOL save_running_state)
{
	if (show)
	{
		if (GetIsInsideTrashFolder())
		{
			return OpStatus::OK;
		}

		if (IsRootService())
		{
			BOOL has_started;
			RETURN_IF_ERROR(g_webserver_manager->OpenAndEnableIfRequired(has_started, FALSE, FALSE));
			return OpStatus::OK;
		}

		// check if gadget is properly installed
		if (NeedsConfiguration())
		{
			return g_desktop_gadget_manager->ConfigurePreinstalledService(this);
		}

		OP_ASSERT(NULL == m_gadget_window);

		OpGadget* op_gadget = GetOpGadget();
		if (op_gadget == NULL)
		{
			return OpStatus::ERR;
		}

		if (!g_webserver_manager->IsFeatureEnabled())
		{
			// Add this gadget to the list to start once the webserver is running
			// Note: If this method is called from WebServerManager::StartAutoStartServices(), 
			// it will be added to the m_running_services that is already iterating. Should be
			// fixed!
			BOOL has_started;

			g_desktop_gadget_manager->AddAutoStartService(op_gadget->GetIdentifier());
			RETURN_IF_ERROR(g_webserver_manager->EnableIfRequired(has_started, TRUE, FALSE));

			if (!has_started)
			{
				// the user has decided not to start the webserver, that's not an error
				return OpStatus::OK;
			}
			if (save_running_state)
			{
				SetAddToAutoStart(TRUE);
			}
			return OpStatus::OK;
		}

		RETURN_IF_ERROR(CreateGadgetWindow());

		m_gadget_window->AddActivityListener(this);

		if (GetModel())
		{
			UniteServiceModelItem* item =  static_cast<HotlistGenericModel*>(GetModel())->GetRootService();
			if (item && !item->IsGadgetRunning())
				item->ShowGadget();
		}

		if (save_running_state)
		{
			SetAddToAutoStart(TRUE);
		}

		//update treeview because the running status has changed
		Change();
	}
	else // !show
	{
		if (IsRootService())
		{
			g_webserver_manager->DisableFeature();
		}
		else
		{
			SetCanOpenServicePage(FALSE);

			if (m_gadget_window)
			{
				if (save_running_state)
				{
					SetAddToAutoStart(FALSE);
				}
				CloseGadgetWindow(TRUE, TRUE, TRUE);
			}
		}
	}
	return OpStatus::OK;
}

/******************************************************************************
 *
 * UniteServiceModelItem::SetIsRootService
 *
 *
 ******************************************************************************/
void  UniteServiceModelItem::SetIsRootService() 
{ 
	m_is_root_service = TRUE; 

} 

/******************************************************************************
 *
 * UniteServiceModelItem::SetNeedsConfiguration
 *
 *
 ******************************************************************************/
void 
UniteServiceModelItem::SetNeedsConfiguration(BOOL needs_configuration)
{
	m_needs_configuration = needs_configuration;
	Change(); // update treeview
}

/******************************************************************************
 *
 * UniteServiceModelItem::SetAddToAutoStart
 *
 *
 ******************************************************************************/
void UniteServiceModelItem::SetAddToAutoStart(BOOL add)
{
	if(m_add_to_auto_start != add && GetModel())
		GetModel()->SetDirty(TRUE);
		
	m_add_to_auto_start = add;
}

/******************************************************************************
 *
 * UniteServiceModelItem::IsGadgetRunning
 *
 *
 ******************************************************************************/

BOOL  UniteServiceModelItem::IsGadgetRunning()
{
	if (IsRootService())
	{
		return g_webserver_manager->IsFeatureEnabled();
	}
	return (m_gadget_window != NULL);
}

/******************************************************************************
 *
 * UniteServiceModelItem::OpenOrFocusServicePage
 *
 *
 ******************************************************************************/
BOOL
UniteServiceModelItem::OpenOrFocusServicePage()
{
	OpString page_address;
	RETURN_VALUE_IF_ERROR(GetServicePageAddress(page_address), FALSE);

	BOOL handled = FALSE;

	if (ServiceWindowHasServiceOpened())
	{
		m_service_window->Activate();
		handled = TRUE;
	}
	// if there's an outdated reference to a window, remove it
	else if (m_service_window)
	{
		m_service_window->RemoveListener(this);
		m_service_window = NULL;
	}
	
	// if an existing tab hasn't been focused, open a new one
	if (!handled)
	{
		OpenURLSetting setting;
		setting.m_address.Set(page_address.CStr());
		setting.m_new_page = YES;

		handled = g_application->OpenResolvedURL(setting);
		
		// save the returned window reference, if present
		if (handled && setting.m_target_window)
		{
			OP_ASSERT(setting.m_target_window->GetType() == WINDOW_TYPE_DOCUMENT);
			if (setting.m_target_window->GetType() == WINDOW_TYPE_DOCUMENT)
			{
				SetServiceWindow(static_cast<DocumentDesktopWindow *>(setting.m_target_window));
			}
		}
	}

	if (handled)
	{
		SetNeedsAttention(FALSE);
	}
	return handled;
}

/******************************************************************************
 *
 * UniteServiceModelItem::OnDesktopWindowClosing
 *
 *
 ******************************************************************************/
void UniteServiceModelItem::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	if (m_service_window == desktop_window)
	{
		m_service_window->RemoveListener(this);
		m_service_window = NULL;
	}
	else
	{
		OP_ASSERT(m_gadget_window == desktop_window);	//we should only have one gadget instance per treemodelitem
		GadgetModelItem::OnDesktopWindowClosing(desktop_window, user_initiated);
	}
}

/******************************************************************************
 *
 * UniteServiceModelItem::OnDesktopWindowActivated
 *
 *
 ******************************************************************************/

/*virtual*/ void
UniteServiceModelItem::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active)
{
	if (m_service_window == desktop_window)
	{
		if (ServiceWindowHasServiceOpened())
		{
			SetNeedsAttention(FALSE);
		}
	}
	else
	{
		GadgetModelItem::OnDesktopWindowActivated(desktop_window, active);
	}
}

/******************************************************************************
 *
 * UniteServiceModelItem::AddActivityListener
 *
 *
 ******************************************************************************/

void UniteServiceModelItem::AddActivityListener()
{
	if (m_gadget_window)
		m_gadget_window->AddActivityListener(this);
}

/******************************************************************************
 *
 * UniteServiceModelItem::CreateGadgetWindow
 *
 *
 ******************************************************************************/

OP_STATUS UniteServiceModelItem::CreateGadgetWindow()
{
	OnDesktopGadgetActivityStateChanged(DesktopGadget::DESKTOP_GADGET_STARTED, 0);

	OpGadget* op_gadget = GetOpGadget();
	if (op_gadget == NULL)
	{
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(g_gadget_manager->OpenGadget(op_gadget));

	// OpGadgetManager should have ended up calling SetGadgetWindow() on us.
	OP_ASSERT(NULL != m_gadget_window);

	RETURN_IF_ERROR(m_gadget_window->Init());

	//gadget has completely loaded now. Start listening to changes now
	m_gadget_window->AddListener(this);

	return OpStatus::OK;
}


void UniteServiceModelItem::SetGadgetWindow(DesktopGadget& gadget_window)
{
	OP_ASSERT(NULL == m_gadget_window || !"This only makes sense once");
	m_gadget_window = &gadget_window;
	m_gadget_window->AddListener(this);
}


/******************************************************************************
 *
 * UniteServiceModelItem::CloseGadgetWindow
 *
 *
 ******************************************************************************/

void UniteServiceModelItem::CloseGadgetWindow(BOOL immediately, BOOL user_initiated, BOOL force)
{
	if (m_gadget_window)
	{
		m_gadget_window->RemoveActivityListener(this);
	}

	m_item_activity_status.Set(UNI_L(" "));
	SetNeedsAttention(FALSE);

	GadgetModelItem::CloseGadgetWindow(immediately, user_initiated, force);
}



/******************************************************************************
 *
 * UniteServiceModelItem::OnDesktopGadgetActivityStateChanged
 *
 *
 ******************************************************************************/

void UniteServiceModelItem::OnDesktopGadgetActivityStateChanged(DesktopGadget::DesktopGadgetActivityState act_state, time_t seconds_since_last_request)
{
	OpString act_state_text;

	switch (act_state)
	{
		case DesktopGadget::DESKTOP_GADGET_STARTED:
			{
				g_languageManager->GetString(Str::S_SERVICE_STATUS_STARTED, act_state_text);
			}
			break;
		case DesktopGadget::DESKTOP_GADGET_NO_ACTIVITY:
			if (seconds_since_last_request > 60*10)
			{
				if (seconds_since_last_request > 60*10) // 10 minutes
				{
					g_languageManager->GetString(Str::S_SERVICE_STATUS_NO_ACTIVITY, act_state_text);
				}
				else if (seconds_since_last_request > 60*2) // 2 minutes
				{
					g_languageManager->GetString(Str::S_SERVICE_STATUS_LOW_ACTIVITY, act_state_text);
				}
			}
			else
			{
				g_languageManager->GetString(Str::S_SERVICE_STATUS_NO_ACTIVITY, act_state_text);
			}
		break;

		case DesktopGadget::DESKTOP_GADGET_LOW_ACTIVITY:
			g_languageManager->GetString(Str::S_SERVICE_STATUS_LOW_ACTIVITY, act_state_text);
		break;

		case DesktopGadget::DESKTOP_GADGET_MEDIUM_ACTIVITY:
			g_languageManager->GetString(Str::S_SERVICE_STATUS_MEDIUM_ACTIVITY, act_state_text);
		break;

		case DesktopGadget::DESKTOP_GADGET_HIGH_ACTIVITY:
			g_languageManager->GetString(Str::S_SERVICE_STATUS_HIGH_ACTIVITY, act_state_text);
		break;
	}

	m_item_activity_status.Set(act_state_text.CStr());

	if(!m_item_activity_timer)
	{
		m_item_activity_timer_count = 1;
		m_item_activity_timer = OP_NEW(OpTimer, ());
		if(m_item_activity_timer)
		{
			m_item_activity_timer->SetTimerListener(this);
			m_item_activity_timer->Start(4000);
		}
	}

	Change();
}


/******************************************************************************
 *
 * UniteServiceModelItem::OnGetAttention
 *
 *
 ******************************************************************************/

void UniteServiceModelItem::OnGetAttention()
{
	// set the attention state, but not if the service page is currently focused
	if (IsGadgetRunning())
	{
		if (!m_service_window || 
			(ServiceWindowHasServiceOpened() &&
			!m_service_window->IsFocused())) 
		{
			SetNeedsAttention(TRUE);
		}
	}
}


/**************************************************************************
 *
 * InstallRootService
 *
 *
 *
 *
 **************************************************************************/

OP_STATUS UniteServiceModelItem::InstallRootService(OpGadget& root_service)
{
	SetRootServiceName();
	SetGadgetIdentifier(root_service.GetIdentifier());

	//Let the gadget manager save all gadgets that are currently installed
	g_gadget_manager->SaveGadgets();
	Change();
	return OpStatus::OK;
}


/******************************************************************************
 *
 * UniteServiceModelItem::SetRootServiceName
 *
 *
 ******************************************************************************/

void
UniteServiceModelItem::SetRootServiceName()
{
	if (IsRootService())
	{
		OpString root_service_name;
		g_languageManager->GetString(Str::S_ROOT_SERVICE_NAME, root_service_name);
		SetName(root_service_name.CStr());
	}
	else
	{
		OP_ASSERT(!"don't set root service name for normal services");
	}
}

/******************************************************************************
 *
 * UniteServiceModelItem::GoToPublicPage
 *
 *
 ******************************************************************************/

void
UniteServiceModelItem::GoToPublicPage()
{
#ifdef GADGET_UPDATE_SUPPORT
		//BOOL stub = FALSE;
		//LEAVE_IF_ERROR(ContainsStubGadget(stub));
		if (IsRootService() && !IsGadgetRunning() && 
			(!g_webserver_manager->HasUsedWebServer()))
		{
			g_webserver_manager->ShowSetupWizard();
			return;
		}
#endif // GADGET_UPDATE_SUPPORT

	if (IsRootService())
	{
		BOOL has_started;
		OpStatus::Ignore(g_webserver_manager->OpenAndEnableIfRequired(has_started, TRUE, FALSE));
		return;
	}

	BOOL gadget_just_started = FALSE;
	if(!IsGadgetRunning())
	{
		gadget_just_started = TRUE;
		ShowGadget(TRUE, TRUE);
	}

	if(gadget_just_started && !GetCanOpenServicePage())
	{
		// the gadget is not fully openned, set a flag and the page will be openned when it's ready
		SetOpenServicePage(TRUE);
	}
	else
	{
		OpenOrFocusServicePage();
	}
}

/******************************************************************************
 *
 * UniteServiceModelItem::SetNeedsAttention
 *
 *
 ******************************************************************************/

void UniteServiceModelItem::SetNeedsAttention(BOOL needs_attention)
{
	if(m_needs_attention != needs_attention)
	{
		m_needs_attention = needs_attention;
		m_attention_state_changed = TRUE;
		Change();
		m_attention_state_changed = FALSE;
	}
}


/******************************************************************************
 *
 * UniteServiceModelItem::GetNotRunningStatusText
 *
 *
 ******************************************************************************/

OpString UniteServiceModelItem::GetNotRunningStatusText()
{
	OpString text;

	if(m_is_root_service)
		// Set status text to S_SERVICE_STATUS_NOT_RUNNING
		g_languageManager->GetString(Str::S_SERVICE_STATUS_NOT_RUNNING, text);
	else
		text.Set(UNI_L(" "));

	return text;
}


/******************************************************************************
 *
 * UniteServiceModelItem::GetInfoText
 *
 *
 ******************************************************************************/

void UniteServiceModelItem::GetInfoText(OpInfoText &text)
{
	text.SetStatusText(GetName().CStr());

	OpString tooltip;
	tooltip.Set(GetName());
	if(m_item_status.HasContent())
	{
		tooltip.Append("\n");
		tooltip.Append(m_item_status);
	}

	text.AddTooltipText(NULL, tooltip.CStr());	
}


/******************************************************************************
 *
 * UniteServiceModelItem::OnTimeOut
 *
 *
 ******************************************************************************/

void UniteServiceModelItem::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(timer == m_item_activity_timer);
	m_item_activity_timer_count++;
	if(m_item_activity_timer_count > 3)
	{
		OP_DELETE(m_item_activity_timer);
		m_item_activity_timer = NULL;
		m_item_activity_timer_count = 0;
	}
	else
	{
		m_item_activity_timer->Start(4000);
	}
	Change();
}

/******************************************************************************
 *
 * UniteServiceModelItem::SetServiceWindow
 *
 *
 ******************************************************************************/

OP_STATUS
UniteServiceModelItem::SetServiceWindow(DocumentDesktopWindow * service_window)
{
	// check if there has been a service window set before
	if (m_service_window)
	{
		if (m_service_window == service_window)
		{
			// we already have the right one
			return OpStatus::OK;
		}
		else // delete the old connection
		{
			m_service_window->RemoveListener(this);
		}
	}
	if (service_window && 
		OpStatus::IsSuccess(service_window->AddListener(this)))
	{
		m_service_window = service_window;
		RETURN_IF_ERROR(m_service_window->SetGadgetIdentifier(GetGadgetIdentifier()));

		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

/******************************************************************************
 *
 * UniteServiceModelItem::GetItemData
 *
 * @param ItemData* item_data
 *
 * Gets the UniteService specific fields, AssociatedImage and AssociatedText
 *
 ******************************************************************************/

OP_STATUS UniteServiceModelItem::GetItemData(ItemData* item_data)
{
	if (item_data->query_type == ASSOCIATED_IMAGE_QUERY)
	{
		OpTreeView* view = item_data->treeview;
		if(view)
		{
			OpGadget* op_gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_ID, m_gadget_id);
			if(op_gadget && op_gadget->IsSubserver() && !IsRootService())
			{
				RETURN_IF_ERROR(item_data->associated_image_query_data.image->Set(m_button_image.CStr()));
				if (NeedsConfiguration() || GetIsInsideTrashFolder())
				{
					item_data->associated_image_query_data.skin_state = SKINSTATE_DISABLED;
				}
			}
		}
	}

	if (item_data->query_type == INFO_QUERY)
	{
		OpGadget* service = g_gadget_manager->FindGadget(GADGET_FIND_BY_ID, m_gadget_id);

		if(service)
		{
			OpString service_description, tooltip_text;
			RETURN_IF_ERROR(service->GetGadgetName(tooltip_text));
			if (tooltip_text.IsEmpty())
			{
				tooltip_text.Set(GetName());
			}
			RETURN_IF_ERROR(service->GetClass()->GetGadgetDescription(service_description));
			if (service_description.HasContent())
			{
				RETURN_IF_ERROR(tooltip_text.AppendFormat(UNI_L("\n%s"), service_description.CStr()));
			}
			OP_ASSERT(tooltip_text.HasContent());
			if (tooltip_text.HasContent())
			{
				RETURN_IF_ERROR(item_data->info_query_data.info_text->SetTooltipText(tooltip_text.CStr()));
			}
			else
			{
				return OpStatus::ERR;
			}
		}
		else
		{
			OP_ASSERT(FALSE);
			GetInfoText(*item_data->info_query_data.info_text);
		}
		return OpStatus::OK;
	}

	if (item_data->query_type == ASSOCIATED_TEXT_QUERY)
	{
		OpGadget* op_gadget = g_gadget_manager->FindGadget(GADGET_FIND_BY_ID, m_gadget_id);

		if(op_gadget)
		{
			item_data->associated_text_query_data.associated_text_indentation = 32;

			if (IsGadgetRunning())
			{
				if(m_is_root_service)
				{
					// set string "username @ device", if available
					OpStringC device_name = g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice);
					OpString user_name;
					
					if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UseOperaAccount))
					{
						g_desktop_account_manager->GetUsername(user_name);
					}

					OpString desc_str;
					// username might be emtpy if pref UseOperaAccount is FALSE
					OP_ASSERT(device_name.HasContent()/* && user_name.HasContent()*/); 
					if (device_name.HasContent() && user_name.HasContent())
					{
						RETURN_IF_ERROR(desc_str.AppendFormat(UNI_L("%s @ %s"), user_name.CStr(), device_name.CStr()));
						return item_data->associated_text_query_data.text->Set(desc_str.CStr());
					}
					else if (device_name.HasContent())
					{
						return item_data->associated_text_query_data.text->Set(device_name.CStr());
					}
					else
					{
						return item_data->associated_text_query_data.text->Set(UNI_L(""));
					}
				}
				else 
				{
					if (m_item_status.HasContent() && m_item_activity_timer_count % 2 == 0)
					{
						return item_data->associated_text_query_data.text->Set(m_item_status.CStr());
					}
					else if (m_item_activity_status.HasContent())
					{
						return item_data->associated_text_query_data.text->Set(m_item_activity_status.CStr());
					}
					else
					{
						OpString status_text;
						g_languageManager->GetString(Str::S_SERVICE_STATUS_NO_ACTIVITY, status_text);
						RETURN_IF_ERROR(item_data->associated_text_query_data.text->Set(status_text.CStr()));
					}
				}
			}
			else if (IsRootService() && g_webserver_manager->IsFeatureEnabling())
			{
				OpString not_running_str;
				g_languageManager->GetString(Str::S_WEBSERVER_STATE_ENABLING, not_running_str);
				return item_data->associated_text_query_data.text->Set(not_running_str);
			}
			else
			{
				return item_data->associated_text_query_data.text->Set(GetNotRunningStatusText().CStr());
			}
		}
		else
		{
			// To get the tree view displayed correctly, we need to have an associated
			// text even before the gadget is really installed
			return item_data->associated_text_query_data.text->Set(GetNotRunningStatusText().CStr());
		}
	}

	return GenericModelItem::GetItemData(item_data); // 
}


/******************************************************************************
 *
 * UniteServiceModelItem::GetServicePageAddress
 *
 *
 ******************************************************************************/
OP_STATUS
UniteServiceModelItem::GetServicePageAddress(OpString & address)
{
	address.Empty();
	if (g_webserver && g_webserver->IsRunning())
	{
		if (IsRootService())
		{
			return address.Set(g_webserver_manager->GetRootServiceAddress(TRUE));
		}
		else
		{
			OpGadget * gadget = GetOpGadget();
			if (!gadget)
			{
				return OpStatus::ERR;
			}
			const uni_char * service_name = gadget->GetUIData(UNI_L("serviceName"));
			if (service_name && uni_strlen(service_name))
			{
				if (uni_stri_eq(service_name, UNI_L("_root"))) // == root service
				{
					OP_ASSERT(!"Check why IsRootService() is FALSE");
					// open root page
					return address.Set(g_webserver_manager->GetRootServiceAddress(TRUE));
				}
				else
				{
					OpString url;
					// url/file_sharing - url/File%20Sharing
					if(OpStatus::IsSuccess(url.AppendFormat(UNI_L("%s%s/"), g_webserver_manager->GetRootServiceAddress(TRUE), service_name)))
					{
						return address.Set(url.CStr());
					}
				}
			}
		}
	}
	return OpStatus::ERR;
}


/******************************************************************************
 *
 * UniteServiceModelItem::ServiceWindowHasServiceOpened
 *
 *
 ******************************************************************************/
BOOL
UniteServiceModelItem::ServiceWindowHasServiceOpened()
{
	// if there is a target window that has the (admin) service page
	// opened already, just focus this tab, and don't open a new tab
	if (m_service_window)
	{
		OpWindowCommander* win_comm = m_service_window->GetWindowCommander();
		RETURN_VALUE_IF_NULL(win_comm, FALSE);

		OpString service_url;
		RETURN_VALUE_IF_ERROR(GetServicePageAddress(service_url), FALSE);

		// check that it starts with the correct path
		OpStringC url(win_comm->GetCurrentURL(FALSE));
		if (url.HasContent() && url.Find(service_url) == 0)
		{
			// special handling for root service:  
			// check that the part between first and second slash (if available) and see if it's "_root" or empty
			if (IsRootService())
			{
				OpString url_path;
				WindowCommanderProxy::GetCurrentURL(win_comm).GetAttribute(URL::KUniPath, url_path);
				if (url_path.Length() >= 1) // the path should at least be "/"
				{
					if (url_path[1] == '\0' || url_path[1] == '/' || uni_strncmp(url_path.CStr() + 1, UNI_L("_root"), 5) == 0)
					{
						return TRUE;
					}
				}
			}
			else
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

#endif // WEBSERVER_SUPPORT

/***********************************************************************************
 **
 **	SeparatorModelItem
 **
 ***********************************************************************************/
SeparatorModelItem::SeparatorModelItem()
	: m_separator_type(SeparatorModelItem::NORMAL_SEPARATOR)
{
}


SeparatorModelItem::SeparatorModelItem(UINT32 type)
	: HotlistModelItem(),
	  m_separator_type(type)
{
}

SeparatorModelItem::SeparatorModelItem( const SeparatorModelItem& item )
	:HotlistModelItem(item)
{
	m_separator_type = item.m_separator_type;
	m_name.Set(item.m_name.CStr());
}


SeparatorModelItem::~SeparatorModelItem()
{
}


/******************************************************************************
 *
 * SeparatorModelItem::GetDuplicate
 *
 *
 ******************************************************************************/
HotlistModelItem *SeparatorModelItem::GetDuplicate( )
{
	return OP_NEW(SeparatorModelItem, (*this));
}


/******************************************************************************
 *
 * MusicModelItem::GetItemData
 *
 *
 ******************************************************************************/

OP_STATUS SeparatorModelItem::GetItemData( ItemData* item_data )
{
	switch(m_separator_type)
	{
	case SeparatorModelItem::TRASH_SEPARATOR:
	case SeparatorModelItem::ROOT_SERVICE_SEPARATOR:
		item_data->flags |= FLAG_DISABLED | FLAG_INITIALLY_DISABLED;
	default:
		;
	}

	return HotlistModelItem::GetItemData(item_data);
}


BOOL  SeparatorModelItem::GetIsDeletable()   const
{
	return GetSeparatorType()  ==  NORMAL_SEPARATOR;
}

/***********************************************************************************
 **
 **	ContactModelItem
 **
 ***********************************************************************************/

ContactModelItem::ContactModelItem()
	:GenericModelItem()
{
	m_m2_index_id = 0;
}

ContactModelItem::ContactModelItem( const ContactModelItem& item )
	:GenericModelItem(item)
{
	SetMail( item.GetMail().CStr() );
	SetPhone( item.GetPhone().CStr() );
	SetFax( item.GetFax().CStr() );
	SetPostalAddress( item.GetPostalAddress().CStr() );
	SetPictureUrl( item.GetPictureUrl().CStr() );
	SetIconName( item.GetImage() );
	SetConaxNumber( item.GetConaxNumber().CStr() );
	SetM2IndexId(0);
}


ContactModelItem::~ContactModelItem()
{
}


/******************************************************************************
 *
 * ContactModelItem::GetDuplicate
 *
 *
 ******************************************************************************/

HotlistModelItem *ContactModelItem::GetDuplicate( )
{
	return OP_NEW(ContactModelItem, (*this));
}


/******************************************************************************
 *
 * ContactModelItem::GetImage
 *
 *
 ******************************************************************************/

const char* ContactModelItem::GetImage() const
{
	return m_icon_name.CStr();
}


/******************************************************************************
 *
 * ContactModelItem::GetInfoText
 *
 *
 ******************************************************************************/

void ContactModelItem::GetInfoText( OpInfoText &text )
{
	if (GetMail().HasContent())
	{
		text.SetStatusText(GetMail().CStr());
	}
	else
	{
		text.SetStatusText(GetName().CStr());
	}

	OpString str;

	g_languageManager->GetString(Str::DI_IDSTR_M2_NEWACC_WIZ_EMAILADDRESS, str);
	text.AddTooltipText(str.CStr(), GetMail().CStr());
	g_languageManager->GetString(Str::DI_ID_CL_PROP_HOMEPAGE_LABEL, str);
	text.AddTooltipText(str.CStr(), GetUrl().CStr());
	g_languageManager->GetString(Str::DI_ID_CL_PROP_PHONE_LABEL, str);
	text.AddTooltipText(str.CStr(), GetPhone().CStr());
	g_languageManager->GetString(Str::DI_ID_CL_PROP_FAX_LABEL, str);
	text.AddTooltipText(str.CStr(), GetFax().CStr());
	g_languageManager->GetString(Str::DI_ID_CL_PROP_POSTADDR_LABEL, str);
	text.AddTooltipText(str.CStr(), GetPostalAddress().CStr());
	g_languageManager->GetString(Str::DI_ID_CL_PROP_NOTES_LABEL, str);
	text.AddTooltipText(str.CStr(), GetDescription().CStr());
}


/******************************************************************************
 *
 * ContactModelItem::GetPrimaryMailAddress
 *
 *
 ******************************************************************************/

OP_STATUS ContactModelItem::GetPrimaryMailAddress(OpString& address)
{
	// The addresses are separated by a space. We assume that the one listed
	// first is the primary one.
	const int separator_pos = m_mail.Find(",");
	if (separator_pos == KNotFound)
		RETURN_IF_ERROR(address.Set(m_mail));
	else
	{
		RETURN_IF_ERROR(address.Set(m_mail.CStr(), separator_pos));
		address.Strip();
	}

	return OpStatus::OK;
}


/******************************************************************************
 *
 * ContactModelItem::GetMailAddresses
 *
 *
 ******************************************************************************/

OP_STATUS ContactModelItem::GetMailAddresses(OpAutoVector<OpString>& addresses)
{
	// Gather all addresses in a vector.
	OpString addresses_string;
	RETURN_IF_ERROR(addresses_string.Set(m_mail));

	addresses_string.Strip();
	BOOL done = addresses_string.IsEmpty();

	while (!done)
	{
		OpString *address = OP_NEW(OpString, ());
		if (!address)
			return OpStatus::ERR_NO_MEMORY;

		OpAutoPtr<OpString> ap_address(address);
		int separator_pos = addresses_string.Find(",");
		RETURN_IF_ERROR(address->Set(addresses_string.CStr(), separator_pos));

		address->Strip();

		RETURN_IF_ERROR(addresses.Add(address));
		ap_address.release();

		addresses_string.Delete(0, separator_pos == KNotFound ? KNotFound : separator_pos + 1);
		addresses_string.Strip();

		done = addresses_string.IsEmpty();
	}

	return OpStatus::OK;
}


/******************************************************************************
 *
 * ContactModelItem::SetIconName
 *
 *
 ******************************************************************************/

void ContactModelItem::SetIconName( const char* str )
{
	m_icon_name.Set(str);
	if( str && op_isdigit(*str) )
	{
		m_icon_name.Insert(0,"Contact");
	}
}

/******************************************************************************
 *
 * ContactModelItem::ChangeImageIfNeeded
 *
 *
 ******************************************************************************/
void ContactModelItem::ChangeImageIfNeeded()
{

	if (m_icon_name.Compare("Contact0") == 0 ||
		m_icon_name.Compare("Contact38") == 0 ||
		m_icon_name.Compare("Contact39") == 0 )
	{
#ifdef M2_SUPPORT
		if (m_m2_index_id && g_m2_engine->GetIndexById(m_m2_index_id))
		{
			if (g_m2_engine->GetIndexById(m_m2_index_id)->IsIgnored())
				SetIconName("39");
			else if (g_m2_engine->GetIndexById(m_m2_index_id)->IsWatched())
				SetIconName("38");
			else
				SetIconName("0");
		}
		else
#endif // M2_SUPPORT
			SetIconName("0");
		Change();
	}
}

/******************************************************************************
 *
 * ContactModelItem::GetM2IndexId
 *
 *
 ******************************************************************************/
index_gid_t ContactModelItem::GetM2IndexId( BOOL create_if_null )
{
	if (!m_m2_index_id && create_if_null)
	{
		return m_m2_index_id = g_m2_engine->GetIndexIDByAddress(m_mail);
	}
	else 
	{
		return m_m2_index_id;
	}
}

/******************************************************************************
 *
 * ContactModelItem::HasNickname
 *
 *
 ******************************************************************************/

BOOL ContactModelItem::HasNickname(const OpStringC& nick)
{
	if (GetShortName().CompareI(nick.CStr()) == 0)
		return TRUE;

	if (GetShortName().FindFirstOf(',') == KNotFound)
		return FALSE;

	OpString item_nick;
	item_nick.Set(GetShortName().CStr());

	const uni_char* const delim = UNI_L(" ,");
	const uni_char* nick_token = uni_strtok(item_nick.CStr(), delim);

	while (nick_token)
	{
		if (nick.CompareI(nick_token) == 0)
			return TRUE;

		nick_token = uni_strtok(NULL, delim);
	}

	return FALSE;
}


/***********************************************************************************
 **
 **	GroupModelItem
 **
 ***********************************************************************************/

GroupModelItem::GroupModelItem()
	:GenericModelItem()
{
}


GroupModelItem::GroupModelItem( const GroupModelItem& item )
	:GenericModelItem(item)
{
	m_group_list.Set( item.m_group_list );
}


GroupModelItem::~GroupModelItem()
{
}


/******************************************************************************
 *
 * GroupModelItem::GetDuplicate
 *
 *
 ******************************************************************************/

HotlistModelItem *GroupModelItem::GetDuplicate( )
{
	return OP_NEW(GroupModelItem, (*this));
}


/******************************************************************************
 *
 * GroupModelItem::AddItem
 *
 *
 ******************************************************************************/

void GroupModelItem::AddItem(UINT32 id)
{
	OpString number;
	if (!number.Reserve(32))
		return;

	uni_snprintf(number.CStr(), 10, UNI_L("%u"), id);

	UINT32 count = GetItemCount();

	for (UINT32 i = 0; i < count; i++)
	{
		if (id == GetItemByPosition(i))
		{
			return;
		}
	}

	if (m_group_list.HasContent())
	{
		m_group_list.Append(UNI_L(","));
	}
	m_group_list.Append(number);
}


/******************************************************************************
 *
 * GroupModelItem::RemoveItem
 *
 *
 ******************************************************************************/

void GroupModelItem::RemoveItem(UINT32 id)
{
	OpString new_group_list;
	OpString number;
	if (!number.Reserve(32))
		return;

	UINT count = GetItemCount();

	for (UINT i = 0; i < count; i++)
	{
		if (id != GetItemByPosition(i))
		{
			uni_snprintf(number.CStr(), 10, UNI_L("%u"), GetItemByPosition(i));

			if (new_group_list.HasContent())
			{
				new_group_list.Append(UNI_L(","));
			}
			new_group_list.Append(number);
		}
	}

	m_group_list.Set(new_group_list);
}


/******************************************************************************
 *
 * GroupModelItem::GetItemCount
 *
 *
 ******************************************************************************/

UINT32 GroupModelItem::GetItemCount()
{
	int pos = 0;
	UINT32 count = 0;

	do
	{
		pos = m_group_list.FindFirstOf(UNI_L(","), pos+1);
		count++;
	}
	while (pos != KNotFound);

	return m_group_list.HasContent() ? count : 0;
}


/******************************************************************************
 *
 * GroupModelItem::GetItemByPosition
 *
 *
 ******************************************************************************/

UINT32 GroupModelItem::GetItemByPosition(UINT32 position)
{
	int first_pos = 0;
	int last_pos = -1;

	UINT32 value = 0;
	UINT32 count = 0;

	for (count = 0; count <= position; count++)
	{
		first_pos = last_pos + 1;
		last_pos = m_group_list.FindFirstOf(UNI_L(","), first_pos);

		if (last_pos == KNotFound)
		{
			last_pos = m_group_list.Length();
		}
	}

	OpString number;

	if (last_pos > 0)
	{
		number.Set((uni_char*)(m_group_list.CStr()+first_pos));
		number.CStr()[last_pos-first_pos] = 0;

		value = uni_atoi(number.CStr());
	}

	return value;
}

/******************************************************************************
 *
 * GroupModelItem::GetImage
 *
 *
 ******************************************************************************/

const char* GroupModelItem::GetImage() const
{
	return "View";
}

/******************************************************************************
 *
 * GroupModelItem::GetInfoText
 *
 *
 ******************************************************************************/
void  GroupModelItem::GetInfoText( OpInfoText &text )
{
}

// CollectionModelItem

CollectionModel* HotlistModelItem::GetItemModel() const
{
	return GetModel();
}
