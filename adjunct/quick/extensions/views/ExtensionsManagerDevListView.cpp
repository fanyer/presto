/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/extensions/views/ExtensionsManagerDevListView.h"

#include "adjunct/quick_toolkit/widgets/QuickSeparator.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "modules/locale/locale-enum.h"

namespace
{
	const unsigned DEV_BUTTONS_TOP_MARGIN = 15;
};

ExtensionsManagerDevListViewItem::ExtensionsManagerDevListViewItem(
		const ExtensionsModelItem& item) :
		ExtensionsManagerListViewItem(item)
{
}

QuickStackLayout* ExtensionsManagerDevListViewItem::ConstructDebugButtons()
{
	OpAutoPtr<QuickStackLayout> list_aptr(
			OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL)));
	RETURN_VALUE_IF_NULL(list_aptr.get(), NULL);

	OpAutoPtr<QuickButton> reload_button_aptr(
			ConstructButtonAux(OpInputAction::ACTION_DEVTOOLS_RELOAD_EXTENSION,
				GetModelItem().GetExtensionId(),
				Str::SI_RELOAD_BUTTON_TEXT));
	RETURN_VALUE_IF_NULL(reload_button_aptr.get(), NULL);

	reload_button_aptr->SetSkin("Extensions Panel List Item Button Debug");

	RETURN_VALUE_IF_ERROR(list_aptr->InsertWidget(reload_button_aptr.release()), NULL);
	RETURN_VALUE_IF_ERROR(list_aptr->InsertEmptyWidget(10, 0, 10, 0), NULL);

	OpAutoPtr<QuickButton> open_folder_button_aptr(
			ConstructButtonAux(OpInputAction::ACTION_OPEN_FOLDER,
				GetModelItem().GetExtensionPath(),
				Str::D_EXTENSION_PANEL_OPEN_FOLDER));
	RETURN_VALUE_IF_NULL(open_folder_button_aptr.get(), NULL);

	open_folder_button_aptr->SetSkin("Extensions Panel List Item Button Debug");

	RETURN_VALUE_IF_ERROR(
			list_aptr->InsertWidget(open_folder_button_aptr.release()), NULL);

	OpAutoPtr<QuickStackLayout> stack_aptr(
			OP_NEW(QuickStackLayout, (QuickStackLayout::VERTICAL)));
	RETURN_VALUE_IF_NULL(stack_aptr.get(), NULL);

	RETURN_VALUE_IF_ERROR(stack_aptr->InsertEmptyWidget(0, DEV_BUTTONS_TOP_MARGIN, 0, 
			DEV_BUTTONS_TOP_MARGIN), NULL);
	RETURN_VALUE_IF_ERROR(stack_aptr->InsertWidget(list_aptr.release()), NULL);
	
	return stack_aptr.release();
}

void ExtensionsManagerDevListView::OnDeveloperExtensionAdded(
		const ExtensionsModelItem& model_item)
{
	ExtensionsManagerDevListViewItem* item = 
			OP_NEW(ExtensionsManagerDevListViewItem, (model_item));
	OP_ASSERT(item);
	if (!item)
	{
		return;
	}

	RETURN_VOID_IF_ERROR(AddToList(item));
}
