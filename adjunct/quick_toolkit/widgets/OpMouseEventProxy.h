/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cezary Kulakowski (ckulakowski)
 */

#ifndef OP_MOUSE_EVENT_PROXY_H
#define OP_MOUSE_EVENT_PROXY_H

#include "modules/widgets/OpWidget.h"


/** @brief This class should be a base class (instead of OpWidget) to all OpWidgets
 * which don't care for the event OnMouseDown and whether it is scrollable, 
 * and just want to pass it to the parent.  The scroll methods are needed for
 * calls from the touch listener in WidgetContainer which is used when touch
 * input is enabled on some platforms.
 */
class OpMouseEventProxy : public OpWidget
{
public:
	Type GetType() { return WIDGET_TYPE_MOUSE_EVENT_PROXY; }
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
	{
		if (GetParent())
		{
			GetParent()->OnMouseDown(point, button, nclicks);
		}
	}
	virtual BOOL IsScrollable(BOOL vertical) 
	{ 
		if (GetParent())
		{
			return GetParent()->IsScrollable(vertical);
		}
		return FALSE;
	}
	virtual BOOL OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth = TRUE)
	{
		if (GetParent())
		{
			return GetParent()->OnScrollAction(delta, vertical, smooth);
		}
		return FALSE;
	}
};

#endif // OP_MOUSE_EVENT_PROXY_H
