/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGETS_TREE_MODEL_ITEM_H
#define GADGETS_TREE_MODEL_ITEM_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/desktop_util/treemodel/optreemodel.h"

class OpGadgetClass;
class GadgetsTreeModel;
class GadgetsTreeModelItemImpl;


/**
 * An item in GadgetsTreeModel.
 *
 * @see GadgetsTreeModel
 * @see GadgetsTreeModelItemImpl
 *
 * @author Tomasz Kupczyk (tkupczyk)
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetsTreeModelItem
	: public TreeModelItem<GadgetsTreeModelItem, GadgetsTreeModel>
{
public:
	/**
	 * @param impl an implementation of GadgetsTreeModelItemImpl.
	 * 		GadgetsTreeModelItem gets ownership.
	 */
	explicit GadgetsTreeModelItem(GadgetsTreeModelItemImpl* impl);

	~GadgetsTreeModelItem();

	BOOL IsLocked() const { return m_locked; }

	BOOL CanBeUninstalled() const;

	//
	// OpTypedObject
	//
	virtual INT32 GetID()
		{ return 0; }

	virtual Type GetType()
		{ return GADGET_TYPE; }

	//
	// OpTreeModelItem
	//
	virtual OP_STATUS GetItemData(OpTreeModelItem::ItemData* item_data);
	virtual int GetNumLines() { return 2; }

	void OnOpen();

	void OnDelete();

	const OpGadgetClass& GetGadgetClass() const;

private:
	GadgetsTreeModelItemImpl* m_impl;
	BOOL m_locked;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGETS_TREE_MODEL_ITEM_H
