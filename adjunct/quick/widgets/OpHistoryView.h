/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_HISTORY_VIEW_H
#define OP_HISTORY_VIEW_H

#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"

/***********************************************************************************
 **
 **	OpHistoryView
 **
 ***********************************************************************************/

class OpHistoryView
	: public OpWidget,
	  public OpTreeModel::Listener,
	  public OpTreeViewListener
{
protected:
    OpHistoryView();

public:

	virtual ~OpHistoryView();
	
	static OP_STATUS Construct(OpHistoryView** obj);

    void			SetDetailed(BOOL detailed, BOOL force = FALSE);
    virtual void	OnResize(INT32* new_w, INT32* new_h) {m_history_view->SetRect(GetBounds());}
    BOOL			IsSingleClick();
	virtual Type	GetType() {return WIDGET_TYPE_HISTORY_VIEW;}
    BOOL			ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context);

	/**
	 * Set the search text in the quick find field of the associated treeview
	 * @param search text to be set
	 */
	void 			SetSearchText(const OpStringC& search_text);

    // subclassing OpWidget
    virtual void	SetAction(OpInputAction* action) {m_history_view->SetAction(action); }
	virtual void	OnDeleted();

    // OpWidgetListener
    virtual void	OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
    virtual void	OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);

    // == OpInputContext ======================

    virtual BOOL		OnInputAction(OpInputAction* action);
    virtual const char*	GetInputContextName() {return "History Widget";}
    OpTreeView*			GetTree() const { return m_history_view; }

	// OpTreeModel::Listener.
	virtual	void OnItemAdded(OpTreeModel* tree_model, INT32 item);
	virtual	void OnItemChanged(OpTreeModel* tree_model, INT32 item, BOOL sort) {}
	virtual	void OnItemRemoving(OpTreeModel* tree_model, INT32 item) { }
	virtual	void OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) { }
	virtual	void OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) { }
	virtual void OnTreeChanging(OpTreeModel* tree_model) { }
	virtual void OnTreeChanged(OpTreeModel* tree_model) { }
	virtual void OnTreeDeleted(OpTreeModel* tree_model) { }

	// OpTreeViewListener

	virtual void OnTreeViewDeleted(OpTreeView* treeview) {}
	virtual void OnTreeViewMatchChanged(OpTreeView* treeview) {SearchContent();}
	virtual void OnTreeViewModelChanged(OpTreeView* treeview) {}

	// MessageObject

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:

    void			DoAllSelected(OpInputAction::Action action);

    DesktopHistoryModel*	m_history_model;
    void            SetViewStyle(DesktopHistoryModel::History_Mode view_style, BOOL force);

	OP_STATUS       SearchContent();

    OpTreeView*		m_history_view;
    BOOL			m_detailed;
};

#endif // OP_HISTORY_VIEW_H
