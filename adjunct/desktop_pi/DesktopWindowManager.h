/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DESKTOPOPWINDOWMANAGER_H
#define DESKTOPOPWINDOWMANAGER_H

/**
 * DesktopOpWindowManager defines an interface to platform window managers
 */
class DesktopWindowManager
{
public:

	/**
	 * Shall set focus to window if any under cursor
	 */
	static void FocusWindowUnderCursor();

};

#endif // DESKTOPOPWINDOWMANAGER_H
