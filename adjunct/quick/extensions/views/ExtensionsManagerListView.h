/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Kupczyk (tkupczyk)
 */

#ifndef EXTENSIONS_MANAGER_LIST_VIEW_H
#define EXTENSIONS_MANAGER_LIST_VIEW_H

#include "adjunct/quick/extensions/views/ExtensionsManagerView.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"

#include "modules/widgets/OpButton.h"

class QuickPagingLayout;
class QuickButton;
class QuickImage;

class ExtensionsManagerListViewItem :
		public ExtensionsManagerViewItem
{
	public:

		ExtensionsManagerListViewItem(const ExtensionsModelItem& item);
		virtual ~ExtensionsManagerListViewItem();

		//
		// ExtensionsManagerViewItem
		//

		virtual OP_STATUS ConstructItemWidget(QuickWidget** widget);
		virtual void SetEnabled(BOOL enabled);
		virtual void UpdateAvailable();
		virtual void UpdateExtendedName(const OpStringC& name);

	protected:

		virtual QuickStackLayout* ConstructDebugButtons() { return NULL; }
		virtual QuickStackLayout* ConstructControlButtons();
		virtual QuickStackLayout* ConstructAuthorAndVersion();
		virtual QuickStackLayout* ConstructDescription();
		virtual QuickButton* ConstructName();
		virtual QuickImage* ConstructExtensionIcon();

		QuickButton* ConstructButtonAux(OpInputAction::Action action, 
				const uni_char* action_data_str, Str::LocaleString str_id);
		QuickButton* ConstructButtonAux(OpInputAction::Action action, 
				const uni_char* action_data_str);
		void UpdateControlButtonsState();

	private:

		QuickPagingLayout* m_enable_disable_pages;
		QuickPagingLayout* m_update_layout;
		QuickStackLayout* m_description;
		QuickImage* m_icon;
		QuickButton* m_extension_name;
		QuickStackLayout* m_author_version;
		Image m_extension_img;
};

class ExtensionsManagerListView : 
		public ExtensionsManagerView, 
		public QuickStackLayout
{
	public:

		ExtensionsManagerListView();
		OP_STATUS AddToList(ExtensionsManagerListViewItem* view_item);

	protected:

		//
		// ExtensionsModel::Listener
		//
		virtual void OnBeforeExtensionsModelRefresh(const ExtensionsModel::RefreshInfo& info);
		virtual void OnNormalExtensionAdded(const ExtensionsModelItem& model_item);
		virtual void OnExtensionUninstall(const ExtensionsModelItem& model_item);
};

#endif // EXTENSIONS_MANAGER_LIST_VIEW_WIDGET_H
