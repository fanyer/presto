/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DESKTOP_GROUP_MODEL_ITEM_H
#define DESKTOP_GROUP_MODEL_ITEM_H

#include "adjunct/quick/models/DesktopWindowCollectionItem.h"
#include "modules/util/adt/oplisteners.h"

class DesktopGroupModelItem;

class DesktopGroupListener
{
public:
	virtual ~DesktopGroupListener() {}

	/** Called when the collapsed state of the group has changed
	  * @param group Changed group
	  * @param collapsed New collapsed state for group
	  */
	virtual void OnCollapsedChanged(DesktopGroupModelItem* group, bool collapsed) = 0;

	/** Called when the active desktop window for the group has changed
	  * @param group Changed group
	  * @param active New active desktop window for group
	  */
	virtual void OnActiveDesktopWindowChanged(DesktopGroupModelItem* group, DesktopWindow* active) = 0;

	/** Called when the active desktop window for the group has changed
	  * @param group Changed group
	  * @param active New active desktop window for group
	  */
	virtual void OnGroupDestroyed(DesktopGroupModelItem* group) = 0;
};


class DesktopGroupModelItem : public DesktopWindowCollectionItem
{
public:
	DesktopGroupModelItem() : m_group_id(GetUniqueID()), m_active_desktop_window(NULL), m_collapsed(true) {}

	virtual bool IsContainer() { return true; }
	virtual bool AcceptsInsertInto() { return true; }
	virtual DesktopWindow* GetDesktopWindow() { return NULL; }
	virtual const char* GetContextMenu() { return "Tab Group Popup Menu"; }

	/** @return Active desktop window for this group
	  * (the window that remains visible if this group is collapsed)
	  */
	DesktopWindow* GetActiveDesktopWindow() const { return m_active_desktop_window; }
	void SetActiveDesktopWindow(DesktopWindow* window);

	/** Unset the active desktop window for this group
	  * Will choose a new active desktop window automatically
	  * @param window Window to unset, if it is the active desktop window
	  */
	void UnsetActiveDesktopWindow(DesktopWindow* window);

	void SetCollapsed(bool collapsed);
	bool IsCollapsed() const { return m_collapsed; }

	OP_STATUS AddListener(DesktopGroupListener* listener) { return m_listeners.Add(listener); }
	OP_STATUS RemoveListener(DesktopGroupListener* listener) { return m_listeners.Remove(listener); }

	// From TreeModelItem
	virtual INT32 GetID() { return m_group_id; }
	virtual Type GetType() { return WINDOW_TYPE_GROUP; }
	virtual OP_STATUS GetItemData(ItemData* item_data);
	virtual void OnAdded();
	virtual void OnRemoving();

private:
	INT32 m_group_id;
	DesktopWindow* m_active_desktop_window;
	bool m_collapsed;
	OpListeners<DesktopGroupListener> m_listeners;
};

#endif // DESKTOP_GROUP_MODEL_ITEM_H
