/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef BROWSER_UI_WINDOW_LISTENER
#define BROWSER_UI_WINDOW_LISTENER

#include "modules/windowcommander/OpWindowCommanderManager.h"

class OpenURLSetting;

class BrowserUiWindowListener : public OpUiWindowListener
{
public:
	virtual OP_STATUS CreateUiWindow(OpWindowCommander* windowCommander, OpWindowCommander* opener, UINT32 width, UINT32 height, UINT32 flags);

	virtual void CloseUiWindow(OpWindowCommander* windowCommander);

	virtual OP_STATUS CreateUiWindow(OpWindowCommander* windowCommander, const CreateUiWindowArgs &args);

private:
	void CreateToplevelWindow(OpWindowCommander* windowCommander, OpenURLSetting & settings, UINT32 width, UINT32 height, BOOL toolbars, BOOL focus_document, BOOL open_background);
};

#endif // BROWSER_UI_WINDOW_LISTENER
