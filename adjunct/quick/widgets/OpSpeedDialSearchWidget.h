/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#ifndef OP_SPEEDDIAL_SEARCH_WIDGET_H
#define OP_SPEEDDIAL_SEARCH_WIDGET_H

#include "adjunct/quick_toolkit/widgets/OpBar.h"

class OpWindowCommander;
class OpSearchDropDownSpecial;
class OpButton;
class OpLabel;

/***********************************************************************************
**
**	OpSpeedDialSearchWidget
**
***********************************************************************************/

class OpSpeedDialSearchWidget : public OpWidget
{
public:

	static OP_STATUS	Construct(OpSpeedDialSearchWidget** obj);

	OpSpeedDialSearchWidget();
	virtual ~OpSpeedDialSearchWidget();

public:

	// == OpInputContext ======================
	
	virtual const char*		GetInputContextName() {return "Speed Dial Search Widget";}

	// Implementing the OpTreeModelItem interface
	virtual Type	GetType() {return WIDGET_TYPE_SPEEDDIAL_SEARCH;}

	virtual void			OnLayout();
	void					GetRequiredSize(INT32& width, INT32& height);

	OP_STATUS Init();

	// Implementing the OpWidgetListener interface
	virtual void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y) {}
	virtual void OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) {}
	virtual void OnDragLeave(OpWidget* widget, OpDragObject* drag_object) { }

	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual BOOL OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

private:
	void DoLayout();


private:
	OpSearchDropDownSpecial*	m_search_field;
};

#endif // OP_SPEEDDIAL_SEARCH_WIDGET_H
