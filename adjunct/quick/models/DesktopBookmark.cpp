/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * 
 */

#include "core/pch.h"

#include "adjunct/quick/models/DesktopBookmark.h"

#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/models/BookmarkFolder.h"
#include "adjunct/quick/models/BookmarkSeparator.h"

#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/pi/OpLocale.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/img/imagedump.h"

CoreBookmarkWrapper::CoreBookmarkWrapper(BookmarkItem* core_item)
{
	m_core_item = core_item;
	m_was_in_trash = FALSE;
	m_ignore_next_notify = FALSE;
	
	// these 2 attributes must exist, as they have a different default value with others. (-1)
	if (!GetAttribute(m_core_item, BOOKMARK_SHOW_IN_PERSONAL_BAR) || !m_core_item->GetAttribute(BOOKMARK_PERSONALBAR_POS))
		SetIntAttribute(BOOKMARK_PERSONALBAR_POS, -1);
	
	if (!GetAttribute(m_core_item, BOOKMARK_SHOW_IN_PANEL) || !m_core_item->GetAttribute(BOOKMARK_PANEL_POS))
		SetIntAttribute(BOOKMARK_PANEL_POS, -1);
	
	m_core_item->SetListener(this);
}

CoreBookmarkWrapper::CoreBookmarkWrapper(const CoreBookmarkWrapper& right)
{
	m_core_item = right.m_core_item;
	m_core_item->SetListener(this);
}

CoreBookmarkWrapper::~CoreBookmarkWrapper()
{
	DetachCoreItem();
}

// CoreBookmarkWrapper
void CoreBookmarkWrapper::DetachCoreItem()
{
	if(m_core_item)
		m_core_item->RemoveListener(this);

	m_core_item = 0;
}


OP_STATUS CoreBookmarkWrapper::SetAttribute(BookmarkAttributeType type, const OpStringC& str)
{
	return ::SetAttribute(m_core_item,type,str);
}

OP_STATUS CoreBookmarkWrapper::SetIntAttribute(BookmarkAttributeType type, INT32 value	)
{
	return ::SetAttribute(m_core_item,type,value);
}

OP_STATUS CoreBookmarkWrapper::RemoveBookmark(BOOL real_delete, BOOL do_sync) 
{ 
	// DeleteBookmark moves item to trash if real_delete FALSE and there is a trash folder.
	if (m_core_item) 
		return g_bookmark_manager->DeleteBookmark(m_core_item, real_delete, do_sync); 
	return OpStatus::OK; 
}

void CoreBookmarkWrapper::OnBookmarkRemoved()
{
	OnBookmarkDeleted();
}

void CoreBookmarkWrapper::OnBookmarkDeleted()
{
	g_hotlist_manager->GetBookmarksModel()->OnBookmarkRemoved(CastToHotlistModelItem());
	DetachCoreItem();
	CastToHotlistModelItem()->Remove();
	OP_DELETE(this);
}

void CoreBookmarkWrapper::OnBookmarkChanged(BookmarkAttributeType attr_type, BOOL moved)
{ 
	if (m_ignore_next_notify)
	{
		return;
	}

	// update UI if necessary. unnecessary message tend to cause problems(recursive function call for example)

	if(attr_type == BOOKMARK_URL 
		|| attr_type == BOOKMARK_TITLE 
		|| attr_type == BOOKMARK_DESCRIPTION)
		CastToHotlistModelItem()->Change(TRUE);

	if (attr_type == BOOKMARK_PERSONALBAR_POS
		|| attr_type == BOOKMARK_PANEL_POS)
		CastToHotlistModelItem()->Change();

	if (attr_type == BOOKMARK_FAVICON_FILE)
	{
		Bookmark* bookmark = CastToBookmark();
		OP_ASSERT(bookmark);
		if (bookmark)
		{
			bookmark->SyncIconAttribute(TRUE);
			bookmark->Change();
		}
	}

	if (attr_type == BOOKMARK_URL || attr_type == BOOKMARK_PARTNER_ID)
	{
		CastToHotlistModelItem()->SetDisplayUrl(UNI_L(""));
	}

	g_hotlist_manager->GetBookmarksModel()->SetDirty(TRUE);

	if (moved) 
		g_hotlist_manager->GetBookmarksModel()->OnMoveItem(this);

	UINT flag = GetFlagFromAttribute(attr_type);

	if (flag)
		g_hotlist_manager->GetBookmarksModel()->BroadcastHotlistItemChanged(this->CastToBookmark(), FALSE, flag);
}


const uni_char* CoreBookmarkWrapper::GetParentGUID()  const // Obs: Don't try to use m_core_item->GetParentUniqueId -> it doesn't work
{ 
	if (!m_core_item)
	{
		return 0; 
	} 
	else 
	{
		if(m_core_item->GetParentFolder())
			return m_core_item->GetParentFolder()->GetUniqueId();
	}
	return 0;
}

CoreBookmarkWrapper* CoreBookmarkWrapper::CastToWrapper(HotlistModelItem* item)
{
	if (item)
	{
		if(item->IsBookmark())
			return static_cast<Bookmark*>(item);
		else if(item->IsFolder()) // and must be a BookmarkFolder object
			return static_cast<BookmarkFolder*>(item);
		else if(item->IsSeparator()) // and must be a BookmarkSeparator object
			return static_cast<BookmarkSeparator*>(item);
	}
			
	OP_ASSERT(FALSE);
	return NULL;
}

OP_STATUS SetAttribute(BookmarkItem* item, BookmarkAttributeType type, const OpStringC& str)
{
	BookmarkAttribute attribute;
	item->GetAttribute(type, &attribute);

	if (str.Compare(attribute.GetTextValue()) != 0)
	{
		RETURN_IF_ERROR(attribute.SetTextValue(str.CStr()));
 		RETURN_IF_ERROR(item->SetAttribute(type, &attribute));
	}

	return OpStatus::OK;
}
OP_STATUS SetAttribute(BookmarkItem* item, BookmarkAttributeType type, INT32 value)
{
	BookmarkAttribute attribute;
	item->GetAttribute(type, &attribute);

	if(attribute.GetIntValue() != value)
	{
 		attribute.SetIntValue(value);
 		RETURN_IF_ERROR(item->SetAttribute(type, &attribute));
	}

	return OpStatus::OK;
}

INT32 GetAttribute(BookmarkItem* item, BookmarkAttributeType type)
{
	switch(type)
	{
		case BOOKMARK_PERSONALBAR_POS:
		case BOOKMARK_SHOW_IN_PERSONAL_BAR:
		case BOOKMARK_PANEL_POS:
		case BOOKMARK_SHOW_IN_PANEL:
		case BOOKMARK_CREATED:
		case BOOKMARK_VISITED:
		case BOOKMARK_ACTIVE:
		case BOOKMARK_EXPANDED:
		case BOOKMARK_SMALLSCREEN:
			break;
		default:
			OP_ASSERT("Calling integer version of GetAttribute for string or unknown attribute" && FALSE);
	}
	
	if (item)
	{
		BookmarkAttribute* attr = item->GetAttribute(type);
		if (attr)
		{
			return attr->GetIntValue();
		}
	}

	// these 2 attributes must exist, as they have a different default value with others. (-1)
	if(type == BOOKMARK_PERSONALBAR_POS || type == BOOKMARK_PANEL_POS)
	{
		OP_ASSERT(FALSE);
		return -1;
	}

	return 0;
}

OP_STATUS GetAttribute(BookmarkItem* item, BookmarkAttributeType type, OpString& value)
{
	switch(type)
	{
		case BOOKMARK_URL:
		case BOOKMARK_TITLE:
		case BOOKMARK_DESCRIPTION:
		case BOOKMARK_SHORTNAME:
		case BOOKMARK_CREATED:
		case BOOKMARK_VISITED:
		case BOOKMARK_TARGET:
		case BOOKMARK_FAVICON_FILE:
		case BOOKMARK_PARTNER_ID:
		case BOOKMARK_DISPLAY_URL:
			break;
		default:
			OP_ASSERT("Calling string version of GetAttribute for integer or unknown attribute" && FALSE);
	}

	if (item)
	{
		BookmarkAttribute attr;
		if (OpStatus::IsSuccess(item->GetAttribute(type, &attr)))
		{
			return attr.GetTextValue(value);
		}
	}

	return OpStatus::ERR;
}

/**********************************************************************************
 *
 * SetdataFromItemData
 *
 * @param const BookmarkItemData* item_data -
 * @param BookmarkItem* bookmark - 
 *
 * @pre bookmark != 0
 *
 * Sets the fields from item_data on BookmarkItem* bookmark
 *
 *********************************************************************************/

void SetDataFromItemData(const BookmarkItemData* item_data, BookmarkItem* bookmark)
{
	if (!bookmark || !item_data) return;

	// these 2 attributes must exist, as they have a different default value with others. (-1)
	::SetAttribute(bookmark, BOOKMARK_PERSONALBAR_POS, -1);
	::SetAttribute(bookmark, BOOKMARK_PANEL_POS, -1);	

	::SetAttribute(bookmark, BOOKMARK_URL,			item_data->url);
	::SetAttribute(bookmark, BOOKMARK_TITLE,		item_data->name);
	::SetAttribute(bookmark, BOOKMARK_DESCRIPTION,	item_data->description);
	::SetAttribute(bookmark, BOOKMARK_SHORTNAME,	item_data->shortname);
	::SetAttribute(bookmark, BOOKMARK_CREATED,		item_data->created);
	::SetAttribute(bookmark, BOOKMARK_VISITED,		item_data->visited);
	::SetAttribute(bookmark, BOOKMARK_TARGET,		item_data->target);
	::SetAttribute(bookmark, BOOKMARK_PERSONALBAR_POS,item_data->personalbar_position);
	::SetAttribute(bookmark, BOOKMARK_PANEL_POS,	item_data->panel_position);
	::SetAttribute(bookmark, BOOKMARK_ACTIVE,		item_data->active);
	::SetAttribute(bookmark, BOOKMARK_EXPANDED,		item_data->expanded);
	::SetAttribute(bookmark, BOOKMARK_SMALLSCREEN,	item_data->small_screen);
	::SetAttribute(bookmark, BOOKMARK_SHOW_IN_PERSONAL_BAR,item_data->personalbar_position >= 0);
	::SetAttribute(bookmark, BOOKMARK_SHOW_IN_PANEL,item_data->panel_position >= 0);
	::SetAttribute(bookmark, BOOKMARK_PARTNER_ID,	item_data->partner_id);
	::SetAttribute(bookmark, BOOKMARK_DISPLAY_URL,	item_data->display_url);

	// icon
	Image icon = g_favicon_manager->Get(item_data->url);
	OpString16 base64;
	OpBitmap *bitmap = icon.GetBitmap(NULL);
	if (bitmap)
	{
		TempBuffer buffer;
		if(OpStatus::IsSuccess(GetOpBitmapAsBase64PNG(bitmap, &buffer)))
		{
			if(OpStatus::IsSuccess(base64.Set(buffer.GetStorage(), buffer.Length())))
			{
				::SetAttribute(bookmark, BOOKMARK_FAVICON_FILE, base64);
			}
		}
		icon.ReleaseBitmap();
	}
	if (item_data->unique_id.HasContent())
	{
		OpString tmp;
		tmp.Set(item_data->unique_id);
		uni_char* uni_unique_id = OP_NEWA(uni_char, (tmp.Length()+1));
		if (uni_unique_id)
		{
			uni_strcpy(uni_unique_id, tmp.CStr());
			bookmark->SetUniqueId(uni_unique_id);
		}
	}

	bookmark->SetMaxCount(item_data->max_count);
	bookmark->SetMoveIsCopy(item_data->move_is_copy);
	bookmark->SetSubFoldersAllowed(item_data->subfolders_allowed);
	bookmark->SetSeparatorsAllowed(item_data->separators_allowed);
	bookmark->SetDeletable(item_data->deletable);

	bookmark->SetFolderType(item_data->type);
}

UINT GetFlagFromAttribute(BookmarkAttributeType attribute)
{
	switch (attribute)
	{
		case BOOKMARK_URL:
			return HotlistModel::FLAG_URL;
		case BOOKMARK_TITLE:
			return HotlistModel::FLAG_NAME;
		case BOOKMARK_DESCRIPTION:
			return HotlistModel::FLAG_DESCRIPTION;
		case BOOKMARK_SHORTNAME:
			return HotlistModel::FLAG_NICK;
		case BOOKMARK_FAVICON_FILE:
			return HotlistModel::FLAG_ICON;
		case BOOKMARK_CREATED:
			return HotlistModel::FLAG_CREATED;
		case BOOKMARK_VISITED:
			return HotlistModel::FLAG_VISITED;
		case BOOKMARK_PANEL_POS:
		case BOOKMARK_SHOW_IN_PANEL:
			return HotlistModel::FLAG_PANEL;
		case BOOKMARK_PERSONALBAR_POS:
		case BOOKMARK_SHOW_IN_PERSONAL_BAR:
			return HotlistModel::FLAG_PERSONALBAR;
		case BOOKMARK_SMALLSCREEN:
			return HotlistModel::FLAG_SMALLSCREEN;
		case BOOKMARK_PARTNER_ID:
			return HotlistModel::FLAG_PARTNER_ID;
		default:
			break;
	}

	return HotlistModel::FLAG_NONE;
}
