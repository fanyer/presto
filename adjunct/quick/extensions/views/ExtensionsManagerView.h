/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef EXTENSIONS_MANAGER_VIEW_H
#define EXTENSIONS_MANAGER_VIEW_H

#include "adjunct/quick/models/ExtensionsModel.h"

#include "modules/util/adt/opvector.h"

class QuickWidget;

class ExtensionsManagerViewItem
{
	public:

		ExtensionsManagerViewItem(const ExtensionsModelItem& item);
		virtual ~ExtensionsManagerViewItem() {}

		const ExtensionsModelItem& GetModelItem() const { return m_model_item; }
		const uni_char* GetExtensionId() const { return m_model_item.GetExtensionId(); }

		virtual OP_STATUS ConstructItemWidget(QuickWidget** widget) = 0;
		virtual void UpdateAvailable() = 0;
		virtual void SetEnabled(BOOL enabled) = 0;
		virtual void UpdateExtendedName(const OpStringC& name) = 0;

	protected:	

		const ExtensionsModelItem& m_model_item;
};

class ExtensionsManagerView : 
		public ExtensionsModel::Listener
{
	public:

		/**
		 * When no extensions are installed this widget should
		 * be shown in the view.
		 *
		 * @return Widget representing empty manager.
		 */
		static QuickWidget* ConstructEmptyPageWidget();

	protected:

		OpAutoVector<ExtensionsManagerViewItem> m_view_items;

		OP_STATUS AddItem(ExtensionsManagerViewItem* item);
		OP_STATUS RemoveItem(ExtensionsManagerViewItem* item);
		ExtensionsManagerViewItem* FindExtensionViewItemById(const OpStringC& id);
		UINT32 GetViewItemsCount() { return m_view_items.GetCount(); }

		//
		// ExtensionsModel::Listener
		//

		virtual void OnBeforeExtensionsModelRefresh(
				const ExtensionsModel::RefreshInfo& info) { /* Do nothing. */ }
		virtual void OnAfterExtensionsModelRefresh() { /* Do nothing. */ }
		virtual void OnNormalExtensionAdded(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
		virtual void OnDeveloperExtensionAdded(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
		virtual void OnExtensionUninstall(
				const ExtensionsModelItem& model_item) { /* Do nothing. */ }
		virtual void OnExtensionEnabled(
				const ExtensionsModelItem& model_item);
		virtual void OnExtensionDisabled(
				const ExtensionsModelItem& model_item);
		virtual void OnExtensionUpdateAvailable(
				const ExtensionsModelItem& model_item);
		virtual void OnExtensionExtendedNameUpdate(
				const ExtensionsModelItem& model_item);
};

#endif // EXTENSIONS_MANAGER_VIEW_H
