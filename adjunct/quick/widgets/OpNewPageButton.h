/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef OP_NEW_PAGE_BUTTON_H
#define OP_NEW_PAGE_BUTTON_H

#include "modules/widgets/OpButton.h"

/***********************************************************************************
 *
 *  @class OpNewPageButton
 *
 *	@brief button in tabbar that adds a new tab
 *
 ***********************************************************************************/
class OpNewPageButton : public OpButton
{
public:
	static OP_STATUS Construct(OpNewPageButton** obj);
	OpNewPageButton();


	virtual void			OnDragStart(const OpPoint& point);

	virtual void			GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	virtual Type		GetType() {return WIDGET_TYPE_NEW_PAGE_BUTTON;}
	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_NEW_PAGE_BUTTON || OpButton::IsOfType(type); }

protected:

	virtual BOOL OnContextMenu(const OpPoint& point, const OpRect* avoid_rect, BOOL keyboard_invoked);
};

#endif // OP_NEW_PAGE_BUTTON_H
