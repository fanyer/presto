/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef DESKTOP_OP_DROP_DOWN_H
#define DESKTOP_OP_DROP_DOWN_H

#include "adjunct/desktop_util/adt/opdelegate.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/util/OpHashTable.h"

class OpInputAction;

/**
 * @brief A dropdown just as OpDropDown, with the difference that dropdown item actions are stored separately, instead of in one OR-action.
 */
class DesktopOpDropDown : public OpDropDown
{
public:
	static OP_STATUS	Construct(DesktopOpDropDown** obj, BOOL editable_text = FALSE);

	virtual				~DesktopOpDropDown();

	OP_STATUS			AddItem(const OpStringC & text, INT32 index = -1, INT32 * got_index = NULL, INTPTR user_data = 0, OpInputAction * action = NULL, const OpStringC8 & widget_image = NULL);

	/**
	 * Sets the delegate to be invoked upon adding an item.
	 *
	 * @param handler the delegate.  Ownership is transferred.
	 */
	void				SetItemAddedHandler(OpDelegateBase<INT32>* handler);

	void				SetEditableText();

	// Override OpDropDown
	virtual	OP_STATUS	AddItem(const uni_char* txt, INT32 index = -1, INT32 *got_index = NULL, INTPTR user_data = 0);

	virtual OpScopeWidgetInfo* CreateScopeWidgetInfo();

protected:
	DesktopOpDropDown(BOOL editable_text = FALSE);

	// Override OpDropDown
	virtual void		InvokeAction(INT32 index);
	virtual void		OnUpdateActionState();

private:
	class DropdownWidgetInfo;

	OpAutoINT32HashTable<OpInputAction>		m_item_actions; //< The actions stored by item id
	OpDelegateBase<INT32>*					m_item_added_handler;
};

#endif // DESKTOP_OP_DROP_DOWN_H
