/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/extensions/views/ExtensionsManagerView.h"

#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickImage.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickSeparator.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickCentered.h"
#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"

#include "modules/locale/oplanguagemanager.h"

namespace
{
	const unsigned PUZZLE_ICON_PADDING = 12;
	const unsigned NO_EXTENSIONS_DESCRIPTION_PREFERRED_WIDTH = 800;
};

ExtensionsManagerViewItem::ExtensionsManagerViewItem(
		const ExtensionsModelItem& item) :
		m_model_item(item)
{
}

QuickWidget* ExtensionsManagerView::ConstructEmptyPageWidget()
{
	OpAutoPtr<QuickStackLayout> main(OP_NEW(QuickStackLayout,
		(QuickStackLayout::VERTICAL)));
	main->InsertEmptyWidget(1,20,1,20);

	OpAutoPtr<QuickImage> puzzle_image(OP_NEW(QuickImage,()));
	RETURN_VALUE_IF_NULL(puzzle_image.get(), NULL);
	RETURN_VALUE_IF_ERROR(puzzle_image->Init(), NULL);

	puzzle_image->GetOpWidget()->GetForegroundSkin()->SetImage("Extensions 64 Gray");
	unsigned puzzle_image_height = puzzle_image->GetPreferredHeight();

	QuickStackLayout* row = OP_NEW(QuickStackLayout,(QuickStackLayout::HORIZONTAL));
	RETURN_VALUE_IF_NULL(row, NULL);
	main->InsertWidget(row);
	
	row->SetFixedHeight(puzzle_image_height + 30);

	RETURN_VALUE_IF_ERROR(row->InsertEmptyWidget(
			2 * PUZZLE_ICON_PADDING, 0, 2 * PUZZLE_ICON_PADDING, 0), NULL);

	RETURN_VALUE_IF_ERROR(row->InsertWidget(puzzle_image.release()), NULL);
	RETURN_VALUE_IF_ERROR(row->InsertEmptyWidget(PUZZLE_ICON_PADDING, 0, 
			PUZZLE_ICON_PADDING, 0), NULL);

	OpAutoPtr<QuickStackLayout> row_main(OP_NEW(QuickStackLayout,
			(QuickStackLayout::VERTICAL)));
	RETURN_VALUE_IF_NULL(row_main.get(), NULL);

	row_main->SetFixedHeight(puzzle_image_height);
	RETURN_VALUE_IF_ERROR(row_main->InsertEmptyWidget(1,10,1,10), NULL);

	OpString text_string;
	
	RETURN_VALUE_IF_ERROR(g_languageManager->GetString(
			Str::D_EXTENSION_PANEL_NO_EXTENSIONS,text_string), NULL);

	OpAutoPtr<QuickCenteredHorizontal> centered_label(OP_NEW(QuickCenteredHorizontal, ()));
	RETURN_VALUE_IF_NULL(centered_label.get(), NULL);

	QuickLabel* text_label = QuickLabel::ConstructLabel(text_string);
	RETURN_VALUE_IF_NULL(text_label, NULL);
	centered_label->SetContent(text_label);
	text_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
	text_label->SetRelativeSystemFontSize(120);
	text_label->SetSkin("Extensions Panel List No Extensions");
	
	RETURN_VALUE_IF_ERROR(row_main->InsertWidget(centered_label.release()), NULL);
	
	RETURN_VALUE_IF_ERROR(
			row_main->InsertEmptyWidget(
				0, 10, NO_EXTENSIONS_DESCRIPTION_PREFERRED_WIDTH, 10), NULL);

	OpAutoPtr<QuickButtonStrip> strip(OP_NEW(QuickButtonStrip, ()));
	if (!strip.get() || OpStatus::IsError(strip->Init()))
	{
		return NULL;
	}

	OpInputAction getmore_action(OpInputAction::ACTION_GET_MORE_EXTENSIONS);

	QuickButton* start_button = QuickButton::ConstructButton(
			Str::D_EXTENSION_PANEL_GET_STARTED, &getmore_action);
	if (!start_button)
	{
		return NULL;
	}
	OpAutoPtr<QuickButton> start_button_aptr(start_button);
	start_button->GetOpWidget()->SetButtonTypeAndStyle(
			OpButton::TYPE_CUSTOM,OpButton::STYLE_TEXT);	
	start_button->SetSkin("Extensions Panel List Item Button");
#ifndef _MACINTOSH_	
	start_button->GetOpWidget()->SetTabStop(g_op_ui_info->IsFullKeyboardAccessActive());
#endif //_MACINTOSH_	
	QuickCenteredHorizontal* centered_button=
			OP_NEW(QuickCenteredHorizontal, ());
	RETURN_VALUE_IF_NULL(centered_button, NULL);
	
	centered_button->SetContent(start_button);
	start_button_aptr.release();

	RETURN_VALUE_IF_ERROR(row_main->InsertWidget(centered_button), NULL);

	RETURN_VALUE_IF_ERROR(row->InsertWidget(row_main.release()), NULL);

	QuickSkinElement* qs_elem = 
			QuickSkinWrap(main.release(), "Extensions Panel Empty List Skin");
	RETURN_VALUE_IF_NULL(qs_elem, NULL);

	return qs_elem;
}

OP_STATUS ExtensionsManagerView::AddItem(ExtensionsManagerViewItem* item)
{
	if (!item)
	{
		return OpStatus::ERR;
	}

	return m_view_items.Add(item);
}

OP_STATUS ExtensionsManagerView::RemoveItem(ExtensionsManagerViewItem* item)
{
	return m_view_items.RemoveByItem(item);
}

void ExtensionsManagerView::OnExtensionDisabled(
		const ExtensionsModelItem& model_item)
{
	ExtensionsManagerViewItem* item = 
			FindExtensionViewItemById(model_item.GetExtensionId());
	if (item)
	{
		item->SetEnabled(FALSE);
	}
}

void ExtensionsManagerView::OnExtensionEnabled(
		const ExtensionsModelItem& model_item)
{
	ExtensionsManagerViewItem* item = 
			FindExtensionViewItemById(model_item.GetExtensionId());
	if (item)
	{
		item->SetEnabled(TRUE);
	}
}

void ExtensionsManagerView::OnExtensionUpdateAvailable(
		const ExtensionsModelItem& model_item)
{
	ExtensionsManagerViewItem* item = 
			FindExtensionViewItemById(model_item.GetExtensionId());
	if (item)
	{
		item->UpdateAvailable();
	}
}

void ExtensionsManagerView::OnExtensionExtendedNameUpdate(
		const ExtensionsModelItem& model_item)
{
	ExtensionsManagerViewItem* item = 
		FindExtensionViewItemById(model_item.GetExtensionId());
	if (item)
	{
		item->UpdateExtendedName(model_item.GetExtendedName());
	}
}

ExtensionsManagerViewItem* ExtensionsManagerView::FindExtensionViewItemById(
		const OpStringC& id)
{
	for (unsigned i = 0; i < m_view_items.GetCount(); ++i)
	{
		if (m_view_items.Get(i)->GetExtensionId() == id)
		{
			return m_view_items.Get(i);
		}
	}

	return NULL;
}
