/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_TREE_VIEW_LISTENER_H
#define OP_TREE_VIEW_LISTENER_H

class OpTreeView;

/**
 * @brief
 * @author Peter Karlsson
 *
 *
 */

/***********************************************************************************
 **
 **	OpTreeViewListener
 **
 ***********************************************************************************/

class OpTreeViewListener
{
public:

	/**
	 *
	 *
	 */
	virtual ~OpTreeViewListener() {}

	/**
	 *
	 * @param treeview
	 */
	virtual void OnTreeViewDeleted(OpTreeView* treeview) = 0;

	/**
	 *
	 * @param treeview
	 */
	virtual void OnTreeViewMatchChanged(OpTreeView* treeview) = 0;

	/**
	 *
	 * @param treeview
	 */
	virtual void OnTreeViewModelChanged(OpTreeView* treeview) = 0;

	/**
	 * Called when the treeview is finished matching text
	 * @param treeview Treeview that was finished matching text
	 */
	virtual void OnTreeViewMatchFinished(OpTreeView* treeview) {}

	/**
	 * Called when the expanded/collapsed state of an item changed
	 * @param treeview Treeview where state changed
	 * @param id ID of item that changed
	 * @param open New open state
	 */
	virtual void OnTreeViewOpenStateChanged(OpTreeView* treeview, INT32 id, BOOL open) {}
};

#endif //OP_TREE_VIEW_LISTENER_H
