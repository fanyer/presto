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

#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "adjunct/quick/models/Bookmark.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/desktop_util/string/stringutils.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/img/imagedump.h"

/***********************************************************************************
 *
 * Constructor
 * 
 ***********************************************************************************/
Bookmark::Bookmark(BookmarkItem* item)
	: DesktopBookmark<GenericModelItem>(item)
	, m_saving_favicon(FALSE)
	, m_history_item(NULL)
	, m_listener(NULL)
{
	if (item)
		OP_ASSERT(item->GetFolderType() == FOLDER_NO_FOLDER);
}

/***********************************************************************************
 * 
 *
 *
 ***********************************************************************************/

Bookmark::~Bookmark()
{
	if (m_listener)
		m_listener->OnBookmarkDestroyed();
	RemoveHistoryItem();
}


/***********************************************************************************
 * 
 * GetImage
 *
 * @return 
 *
 ************************************************************************************/

const char* Bookmark::GetImage() const
{
	//OP_STATUS GetAttribute(, BookmarkAttribute *attr_val);
	time_t now = g_timecache->CurrentTime();

	if(now-GetVisited() < g_pcnet->GetFollowedLinksExpireTime())
		return "Bookmark Visited";
		
	return "Bookmark Unvisited";
}


/***********************************************************************************
 * 
 * GetIcon
 *
 * @return 
 *
 ***********************************************************************************/

Image Bookmark::GetIcon()
{  
	OpStringC url = GetUrl();
	if (url.HasContent())
		return g_favicon_manager->Get(url.CStr());
	else
		return Image();

}

/******************************************************************************
 *
 * Bookmark::OnHistoryItemAccessed
 *
 * @param time_t acc - Updates time visited on item to acc.
 *
 ******************************************************************************/
void Bookmark::OnHistoryItemAccessed(time_t acc)
{
	if( acc > GetVisited() )
	{
		SetVisited(acc); // will notifiy about change to Sync
		if (GetModel())
			GetModel()->SetDirty(TRUE);
	}
}

/******************************************************************************
 *
 * Bookmark::RemoveHistoryItem
 *
 *
 ******************************************************************************/
void Bookmark::RemoveHistoryItem()
{
 	if (m_history_item)
	{
		m_history_item->RemoveListener(this);
    }

	m_history_item = NULL;
}

void Bookmark::SetHistoryItem( HistoryPage* item )
{
  // if setting history item to null or history item not already set
	if (!m_history_item || !item)
	{
		m_history_item = item;

		if (item)
		{
			item->AddListener(this);
		}
	}
}

void Bookmark::SyncIconAttribute(BOOL save)
{
	if (save)
	{
		OpString icon;
		OpString8 icon8;
		unsigned char* buffer = NULL;
		unsigned long len = 0;

		RETURN_VOID_IF_ERROR(GetStringAttribute(BOOKMARK_FAVICON_FILE, icon));
		icon8.Set(icon);
		RETURN_VOID_IF_ERROR(DecodeBase64(icon8, buffer, len));

        m_saving_favicon = TRUE;
		g_favicon_manager->Add(CastToHotlistModelItem()->GetUrl(), buffer, len);
		OP_DELETEA(buffer);
        m_saving_favicon = FALSE;
	}
	else if (!m_saving_favicon) // Ignore the notification when saving favicon (DSK-332808)
	{
		Image icon = GetIcon();
		OpString16 base64;
		OpBitmap *bitmap = icon.GetBitmap(NULL);
		if (bitmap)
		{
			TempBuffer buffer;
			if(OpStatus::IsSuccess(GetOpBitmapAsBase64PNG(bitmap, &buffer)))
			{
				if(OpStatus::IsSuccess(base64.Set(buffer.GetStorage(), buffer.Length())))
				{
					SetAttribute(BOOKMARK_FAVICON_FILE, base64);
				}
			}
			icon.ReleaseBitmap();
		}
	}
}

void Bookmark::SetListener(BookmarkListener* listener) 
{
	m_listener = listener;
}
