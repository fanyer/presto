/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_QUICK_FIND_H
#define OP_QUICK_FIND_H

#include "modules/widgets/OpDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

/***********************************************************************************
**
**	OpQuickFind
**
***********************************************************************************/

class OpQuickFind : public OpDropDown, public OpTreeViewListener
{
	protected:
								OpQuickFind();

	public:
		static OP_STATUS		Construct(OpQuickFind** obj);

		void					SetTarget(OpTreeView* target_treeview, bool auto_target = false);

		/** Start or stop spinning the spinner
		  * @param busy Whether the quick find is busy or not
		  */
		void					SetBusy(BOOL busy);

		virtual void			OnDeleted();
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);
		virtual void			OnUpdateActionState() {UpdateTarget();}

		// OpWidgetListener

		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) {((OpWidgetListener*)GetParent())->OnDragDrop(this, drag_object, pos, x, y);}
		virtual void			OnDragLeave(OpWidget* widget, OpDragObject* drag_object) {((OpWidgetListener*)GetParent())->OnDragLeave(this, drag_object);}
		virtual BOOL            OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
		{
			OpWidgetListener* listener = GetParent() ? GetParent() : this;
			return listener->OnContextMenu(this, child_index, menu_point, avoid_rect, keyboard_invoked);
		}
		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		virtual void			OnDropdownMenu(OpWidget *widget, BOOL show);

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return WIDGET_TYPE_QUICK_FIND;}

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);

		// OpTreeViewListener

		virtual void			OnTreeViewDeleted(OpTreeView* treeview) {m_target_treeview = NULL;}
		virtual void			OnTreeViewMatchChanged(OpTreeView* treeview) { MatchText(); }
		virtual void			OnTreeViewModelChanged(OpTreeView* treeview) { MatchText(); }
		virtual void			OnTreeViewMatchFinished(OpTreeView* treeview) { SetBusy(FALSE); }

	protected:
		void					UpdateTarget();
		virtual void			MatchText();

		OpTreeView*				m_target_treeview;
		bool					m_auto_target;
};

#endif // OP_QUICK_FIND_H
