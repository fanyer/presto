/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */
#include "core/pch.h"

#include "adjunct/quick/models/BookmarkFolder.h"

#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "modules/bookmarks/bookmark_manager.h"

void TrashFolder::OnBookmarkDeleted()
{
	OP_ASSERT(g_bookmark_manager->GetTrashFolder() && m_core_item != g_bookmark_manager->GetTrashFolder());
	
	Update();
}

void TrashFolder::Update()
{
	if (m_core_item != g_bookmark_manager->GetTrashFolder())
	{
		if (g_bookmark_manager->GetTrashFolder())
		{
			GetModel()->RemoveUniqueGUID(this);
			DetachCoreItem();
			m_core_item = g_bookmark_manager->GetTrashFolder();
			m_core_item->SetListener(this);
			GetModel()->AddUniqueGUID(this);
		}
		else
		{
			CoreBookmarkWrapper::OnBookmarkDeleted();
		}
	}
}
