/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Tomasz Kupczyk (tkupczyk)
 */

#include "core/pch.h"

#include "adjunct/quick/extensions/views/ExtensionsManagerListView.h"

#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"
#include "adjunct/quick_toolkit/widgets/NullWidget.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickCentered.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"
#include "adjunct/quick_toolkit/widgets/QuickSeparator.h"
#include "adjunct/quick_toolkit/widgets/QuickButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/QuickImage.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick/extensions/ExtensionUtils.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/util/opautoptr.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetClass.h"


namespace
{
	const unsigned ICON_FIXED_WIDTH = 64;
	const unsigned ICON_FIXED_HEIGHT = 64;
	const unsigned DESCRIPTION_LABEL_MAX_HEIGHT = 38;
	const unsigned DESCRIPTION_RELATIVE_SYSTEM_FONT_SIZE = 100;
	const unsigned DESCRIPTION_PREFERRED_WIDTH = 800;

	const unsigned ROW_ITEM_TOP_PADDING = 20;
	const unsigned ROW_ITEM_BOTTOM_PADDING = 5;
	const unsigned EXTENSION_NAME_MINIMUM_WIDTH = 30;
	const unsigned EXTENSION_ICON_PADDING = 12;
	const unsigned TEXT_SPACING = 4;
	const unsigned NAME_PREFERRED_WIDTH = 550;

	const unsigned ENABLE_BUTTON_PAGE = 0;
	const unsigned DISABLE_BUTTON_PAGE = 1;

	const unsigned UPDATE_BUTTON_PAGE = 0;
	const unsigned NO_UPDATE_BUTTON_PAGE = 1;
};


static void TrimAndClean(OpString& text)
{
	const uni_char *t = text.CStr();
	int len = text.Length();
	uni_char *newText = OP_NEWA(uni_char, (len+1));
    if (!newText)
        return;
    
	int new_pos = 0;
	int pos = 0;
	BOOL skippingSpaces = FALSE;
	
	while (pos < len)
	{
		if (skippingSpaces)
		{
			if (t[pos] == ' '  ||
				t[pos] == '\r' ||
				t[pos] == '\n' ||
				t[pos] == '\t')
				pos++;
			else
				skippingSpaces = FALSE;
		}
		else 
		{
			if (t[pos] == '\n')
				skippingSpaces = TRUE;
			newText[new_pos++] = t[pos++];
		}
	}	
	newText[new_pos++] = '\0';
	text.Set(UNI_L(""));
	text.Append(newText, new_pos);
	OP_DELETEA(newText);
}

ExtensionsManagerListViewItem::ExtensionsManagerListViewItem(
		const ExtensionsModelItem& item) :
		ExtensionsManagerViewItem(item),
		m_enable_disable_pages(NULL),
		m_update_layout(NULL),
		m_description(NULL),
		m_icon(NULL),
		m_extension_name(NULL),
		m_author_version(NULL)
{
}

ExtensionsManagerListViewItem::~ExtensionsManagerListViewItem()
{
	if (!m_extension_img.IsEmpty())
	{
		m_extension_img.DecVisible(null_image_listener);
	}
}

void ExtensionsManagerListViewItem::SetEnabled(BOOL enabled)
{
	m_enable_disable_pages->GoToPage(enabled ? 
			DISABLE_BUTTON_PAGE : ENABLE_BUTTON_PAGE);
	m_description->SetEnabled(enabled);
	m_icon->SetEnabled(enabled);
	m_extension_name->SetEnabled(enabled);
	m_author_version->SetEnabled(enabled);
}

void ExtensionsManagerListViewItem::UpdateAvailable()
{
	m_update_layout->GoToPage(GetModelItem().IsUpdateAvailable() ? 
			UPDATE_BUTTON_PAGE : NO_UPDATE_BUTTON_PAGE);
}

void ExtensionsManagerListViewItem::UpdateExtendedName(const OpStringC& name)
{
	OpString name_string;
	if (const uni_char* attr = GetModelItem().GetGadgetClass()->GetAttribute(
			WIDGET_NAME_TEXT))
	{
		name_string.Append(attr);
	}

	if (name.HasContent() && (name.CompareI(name_string) != 0))
	{
		RETURN_VOID_IF_ERROR(name_string.Append(" : "));
		RETURN_VOID_IF_ERROR(name_string.Append(name));
	}
	if (m_extension_name)
		m_extension_name->SetText(name_string);
}

OP_STATUS ExtensionsManagerListViewItem::ConstructItemWidget(QuickWidget** widget)
{
	*widget = NULL;

	// name_col

	OpAutoPtr<QuickStackLayout> name_col(
			OP_NEW(QuickStackLayout, (QuickStackLayout::VERTICAL)));
	RETURN_OOM_IF_NULL(name_col.get());

	name_col->SetPreferredWidth(WidgetSizes::Fill);
	name_col->SetMinimumWidth(EXTENSION_NAME_MINIMUM_WIDTH);
	m_extension_name = ConstructName();
	RETURN_OOM_IF_NULL(m_extension_name);
	RETURN_IF_ERROR(name_col->InsertWidget(m_extension_name));
	m_author_version = ConstructAuthorAndVersion();
	RETURN_OOM_IF_NULL(m_author_version);
	RETURN_IF_ERROR(name_col->InsertWidget(m_author_version));

	// name_and_buttons_row

	OpAutoPtr<QuickStackLayout> name_and_buttons_row(
			OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL)));
	RETURN_OOM_IF_NULL(name_and_buttons_row.get());

	RETURN_IF_ERROR(name_and_buttons_row->InsertWidget(name_col.release()));
	QuickStackLayout* control_buttons = ConstructControlButtons();
	RETURN_OOM_IF_NULL(control_buttons);
	RETURN_IF_ERROR(name_and_buttons_row->InsertWidget(control_buttons));

	// main_col

	OpAutoPtr<QuickStackLayout> main_col(
			OP_NEW(QuickStackLayout, (QuickStackLayout::VERTICAL)));
	RETURN_OOM_IF_NULL(main_col.get());

	RETURN_IF_ERROR(main_col->InsertWidget(name_and_buttons_row.release()));
	RETURN_IF_ERROR(main_col->InsertEmptyWidget(0, TEXT_SPACING, 0, TEXT_SPACING));
	m_description = ConstructDescription();
	RETURN_OOM_IF_NULL(m_description);
	RETURN_IF_ERROR(main_col->InsertWidget(m_description));
	QuickStackLayout* debug_buttons = ConstructDebugButtons();
	if (debug_buttons)
	{
		RETURN_IF_ERROR(main_col->InsertWidget(debug_buttons));
	}

	// row_content

	OpAutoPtr<QuickStackLayout> row_content(
			OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL)));
	RETURN_OOM_IF_NULL(row_content.get());

	RETURN_IF_ERROR(
			row_content->InsertEmptyWidget(EXTENSION_ICON_PADDING, 0, EXTENSION_ICON_PADDING, 0));
	m_icon = ConstructExtensionIcon();
	RETURN_OOM_IF_NULL(m_icon);
	RETURN_IF_ERROR(row_content->InsertWidget(m_icon));
	RETURN_IF_ERROR(
			row_content->InsertEmptyWidget(EXTENSION_ICON_PADDING, 0, EXTENSION_ICON_PADDING, 0));
	RETURN_IF_ERROR(row_content->InsertWidget(main_col.release()));
	if (debug_buttons)
	{
		RETURN_IF_ERROR(row_content->InsertEmptyWidget(2, 0, 2, 0));
	}

	// row

	OpAutoPtr<QuickStackLayout> row(
			OP_NEW(QuickStackLayout, (QuickStackLayout::VERTICAL)));
	RETURN_OOM_IF_NULL(row.get());

	RETURN_IF_ERROR(row->InsertEmptyWidget(0, ROW_ITEM_TOP_PADDING, 0, 
			ROW_ITEM_TOP_PADDING));
	RETURN_IF_ERROR(row->InsertWidget(row_content.release()));
	RETURN_IF_ERROR(row->InsertEmptyWidget(0, ROW_ITEM_BOTTOM_PADDING, 0, 
			ROW_ITEM_BOTTOM_PADDING));

	QuickSkinElement *quick_skin_element =
			QuickSkinWrap(row.release(), "Extensions Panel List Item Skin");
	RETURN_OOM_IF_NULL(quick_skin_element);

	quick_skin_element->GetOpWidget()->SetAlwaysHoverable(TRUE);

	*widget = quick_skin_element;

	UpdateControlButtonsState();
	SetEnabled(!GetModelItem().IsDisabled());

	return OpStatus::OK;
}

void ExtensionsManagerListViewItem::UpdateControlButtonsState()
{
	m_update_layout->GoToPage(GetModelItem().IsUpdateAvailable() ? 
			UPDATE_BUTTON_PAGE : NO_UPDATE_BUTTON_PAGE);
	m_update_layout->Show();

	m_enable_disable_pages->GoToPage(GetModelItem().IsDisabled() ?  
			ENABLE_BUTTON_PAGE : DISABLE_BUTTON_PAGE);
	m_enable_disable_pages->Show();
}

QuickButton* ExtensionsManagerListViewItem::ConstructButtonAux(
		OpInputAction::Action action, const uni_char* action_data_str, 
			Str::LocaleString str_id)
{
	OpInputAction* in_action = OP_NEW(OpInputAction, (action));
	RETURN_VALUE_IF_NULL(in_action, NULL);
	in_action->SetActionData(reinterpret_cast<INTPTR>(action_data_str));

	OpAutoPtr<QuickButton> button_aptr(
			QuickButton::ConstructButton(str_id, in_action));
	RETURN_VALUE_IF_NULL(button_aptr.get(), NULL);

	OP_DELETE(in_action);

	button_aptr->GetOpWidget()->SetButtonTypeAndStyle(
			OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT);	
	button_aptr->SetSkin("Extensions Panel List Item Button");

#ifndef _MACINTOSH_	
	button_aptr->GetOpWidget()->SetTabStop(g_op_ui_info->IsFullKeyboardAccessActive());
#endif //!_MACINTOSH_

	return button_aptr.release();
}

QuickButton* ExtensionsManagerListViewItem::ConstructButtonAux(
		OpInputAction::Action action, const uni_char* action_data_str)
{
	OpInputAction* in_action = OP_NEW(OpInputAction, (action));
	RETURN_VALUE_IF_NULL(in_action, NULL);
	in_action->SetActionData(reinterpret_cast<INTPTR>(action_data_str));

	OpAutoPtr<QuickButton> button_aptr(
			QuickButton::ConstructButton(in_action));
	RETURN_VALUE_IF_NULL(button_aptr.get(), NULL);

	OP_DELETE(in_action);

#ifndef _MACINTOSH_
	button_aptr->GetOpWidget()->SetTabStop(g_op_ui_info->IsFullKeyboardAccessActive());
#endif //!_MACINTOSH_

	return button_aptr.release();
}

QuickStackLayout* ExtensionsManagerListViewItem::ConstructControlButtons()
{
	OpAutoPtr<QuickStackLayout> strip_aptr(
			OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL)));
	RETURN_VALUE_IF_NULL(strip_aptr.get(), NULL);

	OpAutoPtr<QuickButton> update_button_aptr(
			ConstructButtonAux(OpInputAction::ACTION_UPDATE_EXTENSION,
				GetModelItem().GetExtensionId(),
				Str::D_EXTENSION_MANAGER_UPDATE));
	RETURN_VALUE_IF_NULL(update_button_aptr.get(), NULL);

	OpAutoPtr<QuickButton> enable_button_aptr(
			ConstructButtonAux(OpInputAction::ACTION_ENABLE_EXTENSION,
				GetModelItem().GetExtensionId(),
				Str::S_LITERAL_ENABLE));
	RETURN_VALUE_IF_NULL(enable_button_aptr.get(), NULL);

	OpAutoPtr<QuickButton> disable_button_aptr(
			ConstructButtonAux(OpInputAction::ACTION_DISABLE_EXTENSION,
				GetModelItem().GetExtensionId(),
				Str::S_LITERAL_DISABLE));
	RETURN_VALUE_IF_NULL(disable_button_aptr.get(), NULL);

	OpAutoPtr<QuickButton> uninstall_button_aptr(
			ConstructButtonAux(OpInputAction::ACTION_UNINSTALL_EXTENSION,
				GetModelItem().GetExtensionId(),
				Str::D_EXTENSION_MANAGER_UNINSTALL));
	RETURN_VALUE_IF_NULL(uninstall_button_aptr.get(), NULL);

	OpAutoPtr<QuickButton> cogwheel_button_aptr(
			ConstructButtonAux(OpInputAction::ACTION_SHOW_EXTENSION_COGWHEEL_MENU,
				GetModelItem().GetExtensionId()));
	RETURN_VALUE_IF_NULL(cogwheel_button_aptr.get(), NULL);

	cogwheel_button_aptr->SetMinimumHeight(uninstall_button_aptr->GetMinimumHeight());
	cogwheel_button_aptr->GetOpWidget()->SetButtonTypeAndStyle(
			OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	cogwheel_button_aptr->SetSkin("Extensions Panel List Item Button");
	cogwheel_button_aptr->GetOpWidget()->GetForegroundSkin()->SetImage(
			"Extensions Panel Cogwheel");
	cogwheel_button_aptr->GetOpWidget()->SetName("Extensions Panel Cogwheel Button");
#ifndef _MACINTOSH_	
	cogwheel_button_aptr->GetOpWidget()->SetTabStop(
			g_op_ui_info->IsFullKeyboardAccessActive());
#endif //_MACINTOSH_
	OpAutoPtr<QuickPagingLayout> enable_disable_pages_aptr(
			OP_NEW(QuickPagingLayout, ()));
	RETURN_VALUE_IF_NULL(enable_disable_pages_aptr.get(), NULL);

	QuickPagingLayout* page_layout_tmp = enable_disable_pages_aptr.get();

	OpAutoPtr<QuickPagingLayout> update_pages_aptr(
			OP_NEW(QuickPagingLayout, ()));
	RETURN_VALUE_IF_NULL(update_pages_aptr.get(), NULL);

	m_update_layout = update_pages_aptr.get();

	RETURN_VALUE_IF_ERROR(
		update_pages_aptr->InsertPage(
			update_button_aptr.release(),0), NULL);

	// there is no empty widget for paging layout :(
	QuickWidget* widget = OP_NEW(NullWidget, ());
	if (!widget)
		return NULL;

	widget->SetMinimumWidth(10);
	widget->SetMinimumHeight(21);
	widget->SetPreferredWidth(40);

	RETURN_VALUE_IF_ERROR(
		update_pages_aptr->InsertPage(widget,1), NULL);

	RETURN_VALUE_IF_ERROR(
			enable_disable_pages_aptr->InsertPage(
				enable_button_aptr.release(), 0), NULL);
	RETURN_VALUE_IF_ERROR(
			enable_disable_pages_aptr->InsertPage(
				disable_button_aptr.release(), 1), NULL);

	RETURN_VALUE_IF_ERROR(
			strip_aptr->InsertWidget(
				update_pages_aptr.release()), NULL);

	RETURN_VALUE_IF_ERROR(
			strip_aptr->InsertWidget(
				enable_disable_pages_aptr.release()), NULL);
	RETURN_VALUE_IF_ERROR(
			strip_aptr->InsertWidget(
				uninstall_button_aptr.release()), NULL);
	RETURN_VALUE_IF_ERROR(
			strip_aptr->InsertWidget(
				cogwheel_button_aptr.release()), NULL);

	m_enable_disable_pages = page_layout_tmp;

	return strip_aptr.release();
}

QuickImage* ExtensionsManagerListViewItem::ConstructExtensionIcon()
{
	OpAutoPtr<QuickImage> extension_icon_aptr(OP_NEW(QuickImage,()));
	if (!extension_icon_aptr.get() || 
			OpStatus::IsError(extension_icon_aptr->Init()))
	{
		return NULL;
	}

	if (OpStatus::IsSuccess(ExtensionUtils::GetExtensionIcon(m_extension_img, 
			GetModelItem().GetGadgetClass())))
	{
		extension_icon_aptr->SetFixedWidth(ICON_FIXED_WIDTH);
		extension_icon_aptr->SetFixedHeight(ICON_FIXED_HEIGHT);
		OpWidget* icon_widget = extension_icon_aptr->GetOpWidget();
		OP_ASSERT(icon_widget);
		if (icon_widget)
		{
			icon_widget->GetForegroundSkin()->SetBitmapImage(m_extension_img);
			icon_widget->GetBorderSkin()->SetImage("Extensions Panel List Item Icon");
			m_extension_img.IncVisible(null_image_listener);
			return extension_icon_aptr.release();
		}
	}
	return NULL;
}

QuickStackLayout* ExtensionsManagerListViewItem::ConstructAuthorAndVersion()
{
	OpString author_string_format, author_string;
	if (const uni_char* attr = GetModelItem().GetGadgetClass()->GetAttribute(
			WIDGET_AUTHOR_TEXT))
	{
		RETURN_VALUE_IF_ERROR(g_languageManager->GetString(
				Str::D_WIDGET_INSTALL_BY_AUTHOR, author_string_format), NULL);
		RETURN_VALUE_IF_ERROR(author_string.AppendFormat(
				author_string_format, attr), NULL);
	}
	else
	{
		RETURN_VALUE_IF_ERROR(g_languageManager->GetString(
				Str::D_WIDGET_INSTALL_NO_AUTHOR, author_string), NULL);
	}

	OpString author_href;
	if (const uni_char* attr = GetModelItem().GetGadgetClass()->GetAttribute(
			WIDGET_AUTHOR_ATTR_HREF))
	{
		RETURN_VALUE_IF_ERROR(author_href.Set(attr), NULL);
	}

	OpString version_string;
	if (const uni_char* attr = GetModelItem().GetGadgetClass()->GetAttribute(
			WIDGET_ATTR_VERSION))
	{
		OpString version_literally;
		OP_STATUS ret = g_languageManager->GetString(Str::S_LITERAL_VERSION, 
				version_literally);
		if (OpStatus::IsSuccess(ret))
		{
			RETURN_VALUE_IF_ERROR(
					version_string.Append(version_literally), NULL);
			RETURN_VALUE_IF_ERROR(
					version_string.Append(" "), NULL);
		}
		RETURN_VALUE_IF_ERROR(version_string.Append(attr), NULL);
		RETURN_VALUE_IF_ERROR(version_string.Append(","), NULL);
	}
	else
	{
		version_string.Empty();
	}

	OpAutoPtr<QuickStackLayout> list_aptr(
			OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL)));
	RETURN_VALUE_IF_NULL(list_aptr.get(), NULL);

	RETURN_VALUE_IF_ERROR(
			list_aptr->InsertEmptyWidget(1, 0, 1, 0), NULL);

	// Create version button, without action.

	if (version_string.HasContent())
	{
		OpAutoPtr<QuickButton> version_button_aptr(
				QuickButton::ConstructButton(NULL));
		RETURN_VALUE_IF_NULL(version_button_aptr.get(), NULL);

		RETURN_VALUE_IF_ERROR(
				version_button_aptr->SetText(version_string), NULL);
		version_button_aptr->GetOpWidget()->SetButtonTypeAndStyle(
				OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT);	

		version_button_aptr->GetOpWidget()->SetRelativeSystemFontSize(DESCRIPTION_RELATIVE_SYSTEM_FONT_SIZE);
		version_button_aptr->GetOpWidget()->SetName("Extensions version button");
		version_button_aptr->SetSkin(
			"Extensions Panel List Item Extension Author Not Clickable");

		RETURN_VALUE_IF_ERROR(
				list_aptr->InsertWidget(version_button_aptr.release()), NULL);
	}

	// Construct author button (OpInputAction is copied by QuickButton).

	if (author_string.HasContent())
	{
		OpInputAction in_action(OpInputAction::ACTION_OPEN);
		in_action.SetActionData(reinterpret_cast<INTPTR>(GetModelItem().GetExtensionId()));
		in_action.SetActionDataString(author_href.CStr());

		OpAutoPtr<QuickButton> author_button_aptr(QuickButton::ConstructButton(&in_action));
		RETURN_VALUE_IF_NULL(author_button_aptr.get(), NULL);

		RETURN_VALUE_IF_ERROR(
				author_button_aptr->SetText(author_string), NULL);
		author_button_aptr->GetOpWidget()->SetButtonTypeAndStyle(
				OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT);	
#ifndef _MACINTOSH_		
		author_button_aptr->GetOpWidget()->SetTabStop(
				g_op_ui_info->IsFullKeyboardAccessActive());
#endif // !_MACINTOSH_
		author_button_aptr->GetOpWidget()->SetRelativeSystemFontSize(
				DESCRIPTION_RELATIVE_SYSTEM_FONT_SIZE);
		author_button_aptr->SetSkin(author_href.HasContent() ?
				"Extensions Panel List Item Extension Author Clickable" :
				"Extensions Panel List Item Extension Author Not Clickable");

		RETURN_VALUE_IF_ERROR(
				list_aptr->InsertWidget(author_button_aptr.release()), NULL);
	}

	return list_aptr.release();
}

QuickButton* ExtensionsManagerListViewItem::ConstructName()
{
	// Get all needed data from extension's configuration file.

	OpString name_string;
	if (const uni_char* attr = GetModelItem().GetGadgetClass()->GetAttribute(
			WIDGET_NAME_TEXT))
	{
		RETURN_VALUE_IF_ERROR(name_string.Append(attr), NULL);

		OpStringC str(m_model_item.GetExtendedName());
		if (str.HasContent() && (str.CompareI(name_string) != 0))
		{
			RETURN_VALUE_IF_ERROR(name_string.Append(" : "),NULL);
			RETURN_VALUE_IF_ERROR(name_string.Append(str), NULL);
		}
	}

	OpInputAction in_action(OpInputAction::ACTION_OPEN);
	in_action.SetActionData(reinterpret_cast<INTPTR>(GetModelItem().GetExtensionId()));
	bool should_name_be_clickable = false;
	const uni_char* attr = GetModelItem().GetGadgetClass()->GetAttribute(WIDGET_HOME_PAGE);
	if (!attr)
		attr = GetModelItem().GetGadgetClass()->GetAttribute(WIDGET_ATTR_ID);
	// FIXME: Such a heuristic should be somewhere in core utils,
	// and probably more sophisticated than this one.
	if (attr)
	{
		should_name_be_clickable = uni_strncmp(attr, "http://", 7) == 0 ||
		                           uni_strncmp(attr, "https://", 8) == 0;

		in_action.SetActionDataString(should_name_be_clickable ? attr : NULL);
	}

	OpAutoPtr<QuickButton> name_button_aptr(
			QuickButton::ConstructButton(&in_action));
	RETURN_VALUE_IF_NULL(name_button_aptr.get(), NULL);

	// Configure name button.

	name_button_aptr->GetOpWidget()->SetButtonTypeAndStyle(
		OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE_AND_TEXT_ON_LEFT);	
#ifndef _MACINTOSH_	
	name_button_aptr->GetOpWidget()->SetTabStop(g_op_ui_info->IsFullKeyboardAccessActive());
#endif //!_MACINTOSH_	
	name_button_aptr->GetOpWidget()->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);	
	name_button_aptr->GetOpWidget()->SetEllipsis(ELLIPSIS_END);

#if defined(_MACINTOSH_)
	name_button_aptr->GetOpWidget()->SetRelativeSystemFontSize(130);
#elif defined(_UNIX_DESKTOP_)
	name_button_aptr->GetOpWidget()->SetRelativeSystemFontSize(115);
#elif defined(MSWIN)
	name_button_aptr->GetOpWidget()->SetRelativeSystemFontSize(115);
#endif	

	RETURN_VALUE_IF_ERROR(
			name_button_aptr->SetText(name_string), NULL);

	if (should_name_be_clickable)
	{
		name_button_aptr->SetSkin(
			"Extensions Panel List Item Extension Name Clickable");
	}
	else
	{
		name_button_aptr->SetSkin(
			"Extensions Panel List Item Extension Name Not Clickable");
	}
	name_button_aptr->SetFixedWidth(NAME_PREFERRED_WIDTH);
	return name_button_aptr.release();
}

QuickStackLayout* ExtensionsManagerListViewItem::ConstructDescription()
{
	OpString label_text;
	RETURN_VALUE_IF_ERROR(
			label_text.Set(GetModelItem().GetGadgetClass()->GetAttribute(
				WIDGET_DESCRIPTION_TEXT)), NULL);
	
	// Text label can be a bit too spacey. We need to get rid of those first
	TrimAndClean(label_text);

	OpAutoPtr<QuickStackLayout> list_aptr(
			OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL)));
	RETURN_VALUE_IF_NULL(list_aptr.get(), NULL);

	// FIXME: On MAC we're using buttons for logically labels,
	// and these have some fixed additional padding, that's why
	// we need to adjust description from left a bit.

#if defined(_MACINTOSH_)
	RETURN_VALUE_IF_ERROR(
			list_aptr->InsertEmptyWidget(3, 0, 3, 0), NULL);
#endif // _MACINTOSH_
	
	OpAutoPtr<QuickMultilineLabel> descr_label_aptr(
			OP_NEW(QuickMultilineLabel,()));
	RETURN_VALUE_IF_NULL(descr_label_aptr.get(), NULL);
	RETURN_VALUE_IF_ERROR(descr_label_aptr->Init(), NULL);
	RETURN_VALUE_IF_ERROR(descr_label_aptr->SetText(label_text.Strip()), NULL);

	descr_label_aptr->SetSkin("Extensions Panel List Item Details");
	descr_label_aptr->SetPreferredWidth(DESCRIPTION_PREFERRED_WIDTH);
	descr_label_aptr->GetOpWidget()->SetScrollbarStatus(SCROLLBAR_STATUS_OFF);
	VisualDeviceHandler vis_dev_handler(descr_label_aptr->GetOpWidget());
	descr_label_aptr->SetFixedCharacterHeight(3); // 3 lines at the most
	descr_label_aptr->GetOpWidget()->SetRelativeSystemFontSize(DESCRIPTION_RELATIVE_SYSTEM_FONT_SIZE);

	RETURN_VALUE_IF_ERROR(list_aptr->InsertWidget(descr_label_aptr.release()), NULL);

	return list_aptr.release();
}

ExtensionsManagerListView::ExtensionsManagerListView() : 
		QuickStackLayout(QuickStackLayout::VERTICAL)
{
}

void ExtensionsManagerListView::OnBeforeExtensionsModelRefresh(
		const ExtensionsModel::RefreshInfo& info)
{
	ExtensionsManagerView::OnBeforeExtensionsModelRefresh(info);
	RemoveAllWidgets();
	m_view_items.DeleteAll();
}

OP_STATUS ExtensionsManagerListView::AddToList(
		ExtensionsManagerListViewItem* view_item)
{
	OP_ASSERT(view_item);
	OpAutoPtr<ExtensionsManagerListViewItem> view_item_aptr(view_item);

	if (GetViewItemsCount() > 0)
	{
		RETURN_IF_ERROR(InsertWidget(
				QuickSeparator::ConstructSeparator("Extensions Panel List Item Separator")));
	}

	RETURN_IF_ERROR(AddItem(view_item));

	// Create GUI widget and put it on stack layout.

	QuickWidget* item_widget = NULL;
	RETURN_IF_ERROR(view_item->ConstructItemWidget(&item_widget));
	RETURN_IF_ERROR(InsertWidget(item_widget));

	view_item_aptr.release();

	return OpStatus::OK;
}

void ExtensionsManagerListView::OnNormalExtensionAdded(
		const ExtensionsModelItem& model_item)
{
	ExtensionsManagerListViewItem* item = 
			OP_NEW(ExtensionsManagerListViewItem, (model_item));
	OP_ASSERT(item);
	if (!item)
	{
		return;
	}

	RETURN_VOID_IF_ERROR(AddToList(item));

	ExtensionsManagerView::OnNormalExtensionAdded(model_item);
}

void ExtensionsManagerListView::OnExtensionUninstall(
		const ExtensionsModelItem& model_item)
{
	ExtensionsManagerView::OnExtensionUninstall(model_item);
	RemoveAllWidgets();
	m_view_items.DeleteAll();
}
