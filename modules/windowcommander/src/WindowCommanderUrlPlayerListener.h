/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifdef SCOPE_URL_PLAYER

#ifndef WINDOW_COMMANDER_URL_PLAYER_LISTENER_H
#define WINDOW_COMMANDER_URL_PLAYER_LISTENER_H

class OpWindowCommander;

/**
 *	Urlplayer needs to know if WindowCommander is deleted.
 *
 *  Remember to delete this when urlPlayer is deleted.
 *  We made a separate interface for this SO that it would
 *  be simple to remove later.
 *
 *  When a window is closed, the windows windowCommander is
 *	deleted. UrlPlayer (which will die) manages its windows
 *  by calling these windowCommanders directly, and when one
 *  is deleted by closed window, urlplayer really needs to know.
 *  Feks: BUG: CORE-14606
 */
class WindowCommanderUrlPlayerListener{
public:
	virtual void OnDeleteWindowCommander(OpWindowCommander* wc) = 0;
};

#endif

#endif
