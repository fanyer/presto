/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2000-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_BOOKMARKS_BOOKMARKS_MODULE_H
#define MODULES_BOOKMARKS_BOOKMARKS_MODULE_H

#ifdef CORE_BOOKMARKS_SUPPORT

#include "modules/hardcore/opera/module.h"

class BookmarkManager;
#ifdef CORE_SPEED_DIAL_SUPPORT
class SpeedDialManager;
#endif // CORE_SPEED_DIAL_SUPPORT

class BookmarksModule : public OperaModule
{
public:
	BookmarksModule();
  
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
	BookmarkManager *m_bookmark_manager;
#ifdef CORE_SPEED_DIAL_SUPPORT
	SpeedDialManager *m_speed_dial_manager;
#endif // CORE_SPEED_DIAL_SUPPORT
};

#define BOOKMARKS_MODULE_REQUIRED

#endif // CORE_BOOKMARKS_SUPPORT

#endif // MODULES_BOOKMARKS_BOOKMARKS_MOULE_H
