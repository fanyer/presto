/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2011 Opera Software AS.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 */

#ifndef SCOPE_WIDGET_INFO_H
#define SCOPE_WIDGET_INFO_H

#include "adjunct/desktop_scope/src/generated/g_scope_desktop_window_manager_interface.h"

/** @brief Used to get more information about widget contents
 */
class OpScopeWidgetInfo
{
public:
	virtual ~OpScopeWidgetInfo() {}

	/** Create a list of items this widget consists of
	  * @param list List of items
	  * @param include_invisible Whether items contained in the widget that are not visible should be included in the list
	  */
	virtual OP_STATUS AddQuickWidgetInfoItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, BOOL include_nonhoverable, BOOL include_invisible = TRUE) = 0;
};

#endif // SCOPE_WIDGET_INFO_H
