/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGETS_TREE_MODEL_ITEM_IMPL_H
#define GADGETS_TREE_MODEL_ITEM_IMPL_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "modules/img/image.h"

class OpGadgetClass;

/**
 * A helper class providing a common interface for different kinds of items in
 * a gadgets model.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetsTreeModelItemImpl
{
public:
	virtual ~GadgetsTreeModelItemImpl()  {}

	/**
	 * @return the display name of the item
	 */
	virtual OpString GetName() const = 0;

	/**
	 * @return the status string of the item
	 */
	virtual OpString GetStatus() const = 0;

	/**
	 * @return the item's icon
	 */
	virtual Image GetIcon(INT32 size) const = 0;

	/**
	 * @return GadgetClass object that this item represents
	 */
	virtual const OpGadgetClass& GetGadgetClass() const = 0;

	/**
	 * @return whether the item should be emphasized (e.g., with a bold font)
	 */
	virtual BOOL IsEmphasized() const = 0;

	/**
	 * @return Whether the item can be uninstalled by Opera.
	 */
	virtual BOOL CanBeUninstalled() const = 0;

	/**
	 * Implements the action taken when the item is "opened".
	 */
	virtual void OnOpen() = 0;

	/**
	 * Implements the action taken when the item is "deleted".
	 *
	 * @return @c TRUE iff the item is really deleted on return
	 */
	virtual BOOL OnDelete() = 0;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGETS_TREE_MODEL_ITEM_IMPL_H
