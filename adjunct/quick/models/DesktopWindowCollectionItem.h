/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DESKTOP_WINDOW_COLLECTION_ITEM_H
#define DESKTOP_WINDOW_COLLECTION_ITEM_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"

class DesktopWindowCollection;
class DesktopWindow;

/**
 * @brief Represents a desktop window or group in the tree of windows
 */
class DesktopWindowCollectionItem
	: public TreeModelItem<DesktopWindowCollectionItem, DesktopWindowCollection>
{
public:
	/** @return A DesktopWindow object if this item corresponds to one, NULL otherwise */
	virtual DesktopWindow* GetDesktopWindow() = 0;

	/** @return Whether this item can contain other items */
	virtual bool IsContainer() = 0;

	/** @return Whether items can be dragged into this item */
	virtual bool AcceptsInsertInto() = 0;

	/** @return Context menu entry to use for this item */
	virtual const char* GetContextMenu() = 0;

	/** @return the toplevel window item for this item */
	DesktopWindowCollectionItem* GetToplevelWindow();

	/** Get all DesktopWindow* descandants of a certain type
	  * @param[out] descendants On success, lists all descendants of this item of that type
	  * @param[in] type Type to get, or WINDOW_TYPE_UNKNOWN for all
	  */
	OP_STATUS GetDescendants(OpVector<DesktopWindow>& descendants, Type type = WINDOW_TYPE_UNKNOWN);

	/** Count all descendants of a certain type
	  * @param type Type of descendants to count
	  * @return Number of descendants with specified type
	  */
	UINT32 CountDescendants(Type type);

	/** @param other another item
	  * @return Whether this item is an ancestor of @a other
	  */
	bool IsAncestorOf(const DesktopWindowCollectionItem& other);
};

#endif // DESKTOP_WINDOW_COLLECTION_ITEM_H
