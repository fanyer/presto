/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef BOOKMARKS_CAPABILITIES_H
#define BOOKMARKS_CAPABILITIES_H

// The function BookmarkManager::GetOperaBookmarksListener is available.
#define BOOKMARKS_CAP_OPERA_BOOKMARKS_LISTENER

// The OperaBookmarksListener::SetSavedFormValues is available.
#define BOOKMARKS_CAP_SAVE_FORM_VALUES

// SpeedDialManager::SetSpeedDialReloadInterval is available.
#define BOOKMARKS_CAP_SPEED_DIAL_RELOAD_INTERVAL

#endif // BOOKMARKS_CAPABILITIES_H
