/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_WINDOWLIST_H
#define OP_WINDOWLIST_H

#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

class DesktopWindow;
class DesktopWindowCollectionItem;

/***********************************************************************************
**
**	OpWindowList
**
***********************************************************************************/

class OpWindowList : public OpTreeView
{
	public:

								OpWindowList();

		// OpWidgetListener

		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		BOOL					OnContextMenu(const OpPoint& point, const OpRect* avoid_rect, BOOL keyboard_invoked);
		void 					OpenNewPage(const OpString &url);


		INT32					ListIndexToWorkspaceIndex( INT32 list_index );
		void					SetDetailed(BOOL detailed, BOOL force = FALSE);

	private:
		void					OnDragURL(DesktopDragObject* drag_object, INT32 pos);
		void					OnDropURL(DesktopDragObject* drag_object, INT32 pos);
		DesktopWindow* 			GetDesktopWindowByPosition(INT32 pos);
		BOOL					IsSingleClick() {return !m_detailed && g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSingleClick);}

	private:

		BOOL					m_detailed;
};

#endif // OP_WINDOWLIST_H
