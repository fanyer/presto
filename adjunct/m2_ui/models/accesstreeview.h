// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
/** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Owner:    Alexander Remen (alexr)
 */

#ifndef ACCESSTREEVIEW_H
#define ACCESSTREEVIEW_H

#include "adjunct/m2_ui/models/accessmodel.h"
#include "adjunct/m2_ui/models/accesstreeviewlistener.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

class MailPanel;

class AccessTreeView : public OpTreeView
{
public:

	~AccessTreeView()			{ OP_DELETE(GetTreeModel()); }

	static OP_STATUS		Construct(AccessTreeView** obj);
	OP_STATUS				Init(MailPanel* panel, UINT32 category_id);

	void					OnFullModeChanged(BOOL full_mode);

	virtual void			OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	virtual void			ChangeItemOpenState(TreeViewModelItem* item, BOOL open);
	virtual BOOL			OnInputAction(OpInputAction* action);

	virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y)
								{ m_listener.OnDragDrop(widget, drag_object, pos, x, y); }
	virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
	virtual void			OnDragMove(OpDragObject* op_drag_object, const OpPoint& point);
	virtual void			OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text) { m_listener.OnItemEdited(widget, pos, column, text); }
	virtual void			OnItemRemoving(OpTreeModel* tree_model, INT32 index) { if (GetListener()) GetListener()->OnRelayout(this); };
	virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual BOOL 			ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context);
	virtual void			ScrollToLine(INT32 line, BOOL force_to_center);
	virtual void			SetSelectedItem(INT32 pos, BOOL changed_by_mouse = FALSE, BOOL send_onchange = TRUE, BOOL allow_parent_fallback = FALSE);
	virtual BOOL			IsScrollable(BOOL vertical);
	virtual BOOL			OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth);

	BOOL					IsImapInbox(UINT32 index_id);
	void					SetSelectedIndex(UINT32 index_id);

	OP_STATUS				CreateNewFolder(index_gid_t parent_id);
	AccessModel*			GetAccessModel() { return static_cast<AccessModel*>(GetTreeModel());}

private:
	AccessTreeView() : m_mailpanel(0), m_listener(*this) {}

	MailPanel*				m_mailpanel;
	AccessTreeViewListener	m_listener;
};

#endif // ACCESSTREEVIEW_H
