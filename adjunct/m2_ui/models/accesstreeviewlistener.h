/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl) / Alexander Remen (alexr)
 */

#ifndef ACCESS_TREE_VIEW_LISTENER_H
#define ACCESS_TREE_VIEW_LISTENER_H

#include "adjunct/desktop_pi/DesktopDragObject.h"
#include "modules/widgets/OpWidget.h"
#include "adjunct/m2/src/include/defs.h"

class OpTreeView;

class AccessTreeViewListener : public OpWidgetListener
{
public:
	AccessTreeViewListener(OpTreeView& treeview) : m_treeview(treeview) {}

	virtual void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
	virtual void OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
	virtual void OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text);

private:
	BOOL CanDragMail(index_gid_t dest);
	void OnDropMail(DesktopDragObject* drag_object, index_gid_t source, index_gid_t dest);
	DropType GetMailDropType(index_gid_t source, index_gid_t dest);
	BOOL CanDragIndex(index_gid_t source);
	DesktopDragObject::InsertType GetIndexInsertType(index_gid_t dest, DesktopDragObject::InsertType suggested);
	void OnDropIndex(DesktopDragObject* drag_object, index_gid_t source, index_gid_t dest);
	BOOL CanDropIndex(index_gid_t source, index_gid_t dest);

	OpTreeView& m_treeview;
};

#endif // ACCESS_TREE_VIEW_LISTENER_H
