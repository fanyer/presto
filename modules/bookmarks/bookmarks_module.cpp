/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#include "core/pch.h"

#ifdef CORE_BOOKMARKS_SUPPORT

#include "modules/bookmarks/bookmark_manager.h"
#include "modules/bookmarks/bookmarks_module.h"
#ifdef CORE_SPEED_DIAL_SUPPORT
#include "modules/bookmarks/speeddial_manager.h"
#endif // CORE_SPEED_DIAL_SUPPORT

BookmarksModule::BookmarksModule() :
  m_bookmark_manager(NULL)
#ifdef CORE_SPEED_DIAL_SUPPORT
  ,m_speed_dial_manager(NULL)
#endif // CORE_SPEED_DIAL_SUPPORT
{
}

void BookmarksModule::InitL(const OperaInitInfo& info)
{
    m_bookmark_manager = OP_NEW_L(BookmarkManager, ());
    LEAVE_IF_ERROR(m_bookmark_manager->Init());
#ifdef CORE_SPEED_DIAL_SUPPORT
	m_speed_dial_manager = OP_NEW_L(SpeedDialManager, ());
	LEAVE_IF_ERROR(m_speed_dial_manager->Init());
#endif // CORE_SPEED_DIAL_SUPPORT
}

void BookmarksModule::Destroy()
{
#ifdef CORE_SPEED_DIAL_SUPPORT
	OP_DELETE(m_speed_dial_manager);
	m_speed_dial_manager = NULL;
#endif // CORE_SPEED_DIAL_SUPPORT
	if (m_bookmark_manager)
		m_bookmark_manager->Destroy();
    OP_DELETE(m_bookmark_manager);
	m_bookmark_manager = NULL;
}

#endif // CORE_BOOKMARKS_SUPPORT
