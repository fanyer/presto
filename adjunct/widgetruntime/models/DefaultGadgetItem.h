/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DEFAULT_GADGET_ITEM_H
#define DEFAULT_GADGET_ITEM_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/models/GadgetsTreeModelItemImpl.h"

struct GadgetInstallerContext;


/**
 * The GadgetsTreeModelItemImpl used for gadgets installed within the Widget
 * Runtime.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class DefaultGadgetItem : public GadgetsTreeModelItemImpl
{
public:
	/**
	 * Constructs a DefaultGadgetItem and establishes the association with a
	 * GadgetInstallerContext.
	 *
	 * @param install_info the associated GadgetInstallerContext
	 */
	explicit DefaultGadgetItem(const GadgetInstallerContext& install_info);

	virtual ~DefaultGadgetItem();

	virtual OpString GetName() const;

	virtual OpString GetStatus() const;

	virtual Image GetIcon(INT32 size) const;

	virtual const OpGadgetClass& GetGadgetClass() const;

	virtual BOOL IsEmphasized() const;

	virtual BOOL CanBeUninstalled() const;

	virtual void OnOpen();
	virtual BOOL OnDelete();

private:
	const GadgetInstallerContext* m_install_info;
	mutable Image m_icon;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // DEFAULT_GADGET_ITEM_H
