/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGETS_HOTLIST_VIEW_H
#define GADGETS_HOTLIST_VIEW_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick/widgets/OpHotlistTreeView.h"
#include "adjunct/widgetruntime/hotlist/GadgetsHotlistController.h"
#include "adjunct/widgetruntime/models/GadgetsTreeModel.h"


/**
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetsHotlistView : public OpHotlistTreeView
{
public:
	OP_STATUS Init();

	//
	// OpTypedObject
	//
	virtual Type GetType()
		{ return WIDGET_TYPE_GADGETS_VIEW; }

	//
	// OpHotlistTreeView
	//
	virtual BOOL ShowContextMenu(const OpPoint& point, BOOL center,
			OpTreeView* view, BOOL use_keyboard_context);
	virtual	INT32 GetRootID();
	virtual	void OnSetDetailed(BOOL detailed);
	virtual INT32 OnDropItem(HotlistModelItem* hmi_target,
			DesktopDragObject* drag_object, INT32 i, DropType drop_type,
			DesktopDragObject::InsertType insert_type, INT32 *first_id = 0,
			BOOL force_delete = FALSE);

	//
	// OpInputContext
	//
	virtual const char* GetInputContextName()
		{ return "Widgets Widget"; }
	virtual BOOL OnInputAction(OpInputAction* action);

	//
	// OpWidget
	//
	virtual void OnMouseEvent(OpWidget* widget, INT32 pos, INT32 x, INT32 y,
			MouseButton button, BOOL down, UINT8 nclicks);
	virtual void OnShow(BOOL show);
	virtual void OnDeleted();

private:
	OP_STATUS InitModel();
	OP_STATUS DeleteModel();

	OpAutoPtr<GadgetsTreeModel> m_gadgets_model;
	OpAutoPtr<GadgetsHotlistController> m_controller;

	static const char* const GADGET_MENU_NAME;
	static const UINT32 GADGET_HOTLIST_ICON_SIZE = 32;
	static const UINT32 SEPARATOR_COLUMN_WIDTH = 5;

	enum
	{
		ICON_COLUMN = 0,
		SEPARATOR_COLUMN,
		NAME_COLUMN
	};
};


#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGETS_HOTLIST_VIEW_H
