/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef QUICK_DROP_DOWN_H
#define QUICK_DROP_DOWN_H

#include "adjunct/quick_toolkit/widgets/DesktopOpDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"

/**
 * QuickWidget wrapper for drop-downs.
 */
class QuickDropDown : public QuickEditableTextWidgetWrapper<DesktopOpDropDown>
{
	IMPLEMENT_TYPEDOBJECT(QuickEditableTextWidgetWrapper<DesktopOpDropDown>);
public:
	virtual ~QuickDropDown();

	virtual OP_STATUS Init();

	OP_STATUS	AddItem(const OpStringC & text, INT32 index = -1, INT32 * got_index = NULL, INTPTR user_data = 0, OpInputAction * action = NULL, const OpStringC8 & widget_image = NULL)
		{ return GetOpWidget()->AddItem(text, index, got_index, user_data, action, widget_image); }

private:
	void OnItemAdded(INT32 index)
		{ GetOpWidget()->ResetRequiredSize(); BroadcastContentsChanged(); }
};

#endif // QUICK_DROP_DOWN_H
