/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DESKTOP_WINDOW_MODEL_ITEM_H
#define DESKTOP_WINDOW_MODEL_ITEM_H

#include "adjunct/quick/models/DesktopWindowCollectionItem.h"

/**
 * @brief Represents a desktop window
 */
class DesktopWindowModelItem : public DesktopWindowCollectionItem
{
public:
	explicit DesktopWindowModelItem(DesktopWindow* desktop_window) : m_desktop_window(desktop_window) {}

	// From DesktopWindowCollectionItem
	virtual DesktopWindow* GetDesktopWindow() { return m_desktop_window; }
	virtual bool AcceptsInsertInto();
	virtual bool IsContainer() { return GetType() == WINDOW_TYPE_BROWSER; }
	virtual const char* GetContextMenu() { return "Windows Item Popup Menu"; }

	// From TreeModelItem
	virtual INT32 GetID();
	virtual Type GetType();
	virtual OP_STATUS GetItemData(ItemData* item_data);
	virtual void OnAdded();

	DesktopWindow* m_desktop_window;
};

#endif // DESKTOP_WINDOW_MODEL_ITEM_H
