/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef EXTENSIONS_MANAGER_DEV_LIST_VIEW_H
#define EXTENSIONS_MANAGER_DEV_LIST_VIEW_H

#include "adjunct/quick/extensions/views/ExtensionsManagerListView.h"

class ExtensionsManagerDevListViewItem :
		public ExtensionsManagerListViewItem
{
	public:

		ExtensionsManagerDevListViewItem(const ExtensionsModelItem& item);

	protected:

		virtual QuickStackLayout* ConstructDebugButtons();
};

class ExtensionsManagerDevListView :
		public ExtensionsManagerListView
{
	protected:

		//
		// ExtensionsModel::Listener
		//
		virtual void OnDeveloperExtensionAdded(const ExtensionsModelItem& model_item);
		virtual void OnNormalExtensionAdded(const ExtensionsModelItem& model_item) { /* Do nothing. */ }
};

#endif // EXTENSIONS_MANAGER_DEV_LIST_VIEW_H
