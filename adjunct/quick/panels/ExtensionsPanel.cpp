/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "ExtensionsPanel.h"

#include "adjunct/quick/extensions/views/ExtensionsManagerView.h"
#include "adjunct/quick/extensions/views/ExtensionsManagerListView.h"
#include "adjunct/quick/extensions/views/ExtensionsManagerDevListView.h"
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickCentered.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpWidget.h"

namespace
{
	const int EMPTY_PANEL_PAGE = 0;
	const int EXTENSIONS_LISTS_PAGE = 1;
};

ExtensionsPanel::ExtensionsPanel() :
		m_content(NULL),
		m_page_layout(NULL),
		m_extensions_view(NULL),
		m_counter_label(NULL),
		m_extensions_dev_view(NULL),
		m_dev_counter_label(NULL),
		m_dev_count(0),
		m_normal_count(0),
		m_contents_changed(FALSE)
{
}

OP_STATUS ExtensionsPanel::Init()
{
	m_content = OP_NEW(QuickScrollContainer, (QuickScrollContainer::VERTICAL,
			QuickScrollContainer::SCROLLBAR_AUTO));
	RETURN_OOM_IF_NULL(m_content);
	RETURN_IF_ERROR(m_content->Init());
	m_content->SetRespectScrollbarMargin(false);
	m_content->SetParentOpWidget(this);
	m_content->SetContainer(this);

	OpAutoPtr<QuickPagingLayout> page_layout_aptr(
			OP_NEW(QuickPagingLayout, ()));
	RETURN_OOM_IF_NULL(page_layout_aptr.get());

	OpAutoPtr<QuickWidget> empty_panel_widget_aptr(
			ExtensionsManagerView::ConstructEmptyPageWidget());
	RETURN_OOM_IF_NULL(empty_panel_widget_aptr.get());

	RETURN_IF_ERROR(page_layout_aptr->InsertPage(empty_panel_widget_aptr.release(), 0));

	m_page_layout = page_layout_aptr.get();

	OpAutoPtr<QuickSkinElement> padding_list_aptr(
			QuickSkinWrap(page_layout_aptr.release(), "Extensions Panel List Padding Skin"));
	RETURN_OOM_IF_NULL(padding_list_aptr.get());

	OpAutoPtr<QuickCenteredHorizontal> centered_list_aptr(
			OP_NEW(QuickCenteredHorizontal, ()));
	RETURN_OOM_IF_NULL(centered_list_aptr.get());

	centered_list_aptr->SetContent(padding_list_aptr.release());
	m_content->SetContent(centered_list_aptr.release());

	// Do this after m_content has been initialized.  SetImage() causes a
	// Relayout() call, which leads to OnDirectionChanged() when in RTL (all
	// widgets start out in LTR).
	SetSkinned(TRUE);
	GetBorderSkin()->SetImage("Extensions Panel Widget Skin");

	SetToolbarName("Extension Manager Toolbar", "Extension Manager Toolbar");

	RETURN_IF_ERROR(
			g_desktop_extensions_manager->AddExtensionsModelListener(this));
	if (OpStatus::IsError(g_desktop_extensions_manager->RefreshModel()))
	{
		RETURN_IF_ERROR(
				g_desktop_extensions_manager->RemoveExtensionsModelListener(this));

		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

void ExtensionsPanel::RemoveViews()
{
	if (m_page_layout->GetPageCount() > 1)
	{
		m_page_layout->RemovePage(1);
	}
	m_extensions_view = NULL;
	m_extensions_dev_view = NULL;
}

OP_STATUS ExtensionsPanel::ResetViews()
{
	if (m_extensions_view)
	{
		RETURN_IF_ERROR(g_desktop_extensions_manager->RemoveExtensionsModelListener(
					m_extensions_view));
	}
	if (m_extensions_dev_view)
	{
		RETURN_IF_ERROR(g_desktop_extensions_manager->RemoveExtensionsModelListener(
					m_extensions_dev_view));
	}

	RemoveViews();

	if (m_dev_count > 0 && m_normal_count > 0)
	{
		RETURN_IF_ERROR(
				m_page_layout->InsertPage(ConstructNormalAndDeveloper(), 1));
	}
	else if (m_dev_count == 0 && m_normal_count > 0)
	{
		RETURN_IF_ERROR(
				m_page_layout->InsertPage(ConstructNormalExtensionsList(), 1));
	}
	else if (m_dev_count > 0 && m_normal_count == 0)
	{
		RETURN_IF_ERROR(
				m_page_layout->InsertPage(ConstructDeveloperAndEmptyPageWidget(), 1));
	}

	if (m_extensions_view)
	{
		RETURN_IF_ERROR(g_desktop_extensions_manager->AddExtensionsModelListener(
				m_extensions_view));
	}
	if (m_extensions_dev_view)
	{
		RETURN_IF_ERROR(g_desktop_extensions_manager->AddExtensionsModelListener(
				m_extensions_dev_view));
	}

	return OpStatus::OK;
}

QuickStackLayout* ExtensionsPanel::ConstructNormalAndDeveloper()
{
	OpAutoPtr<QuickWidget> extensions_list_aptr(
			ConstructNormalExtensionsList());
	RETURN_VALUE_IF_NULL(extensions_list_aptr.get(), NULL);

	OpAutoPtr<QuickWidget> extensions_dev_list_aptr(
			ConstructDeveloperExtensionsList());
	RETURN_VALUE_IF_NULL(extensions_dev_list_aptr.get(), NULL);

	OpAutoPtr<QuickStackLayout> two_lists_aptr(
			OP_NEW(QuickStackLayout, (QuickStackLayout::VERTICAL)));
	RETURN_VALUE_IF_NULL(two_lists_aptr.get(), NULL);

	RETURN_VALUE_IF_ERROR(
			two_lists_aptr->InsertWidget(extensions_dev_list_aptr.release()), NULL);
	RETURN_VALUE_IF_ERROR(
			two_lists_aptr->InsertEmptyWidget(1, 13, 1, 13), NULL);
	RETURN_VALUE_IF_ERROR(
			two_lists_aptr->InsertWidget(extensions_list_aptr.release()), NULL);
	
	return two_lists_aptr.release();
}

QuickStackLayout* ExtensionsPanel::ConstructDeveloperAndEmptyPageWidget()
{
	OpAutoPtr<QuickWidget> extensions_dev_list_aptr(
			ConstructDeveloperExtensionsList());
	RETURN_VALUE_IF_NULL(extensions_dev_list_aptr.get(), NULL);
	
	OpAutoPtr<QuickWidget> empty_panel_widget_aptr(
			ExtensionsManagerView::ConstructEmptyPageWidget());
	RETURN_VALUE_IF_NULL(empty_panel_widget_aptr.get(), NULL);

	OpAutoPtr<QuickStackLayout> two_lists_aptr(OP_NEW(QuickStackLayout,
			(QuickStackLayout::VERTICAL)));
	RETURN_VALUE_IF_NULL(two_lists_aptr.get(), NULL);

	RETURN_VALUE_IF_ERROR(
			two_lists_aptr->InsertWidget(extensions_dev_list_aptr.release()), NULL);
	RETURN_VALUE_IF_ERROR(
			two_lists_aptr->InsertEmptyWidget(1, 33, 1, 33), NULL);
	RETURN_VALUE_IF_ERROR(
			two_lists_aptr->InsertWidget(empty_panel_widget_aptr.release()), NULL);
	
	return two_lists_aptr.release();
}

OP_STATUS ExtensionsPanel::ConstructHeader(QuickStackLayout** list, 
		QuickLabel** count_label, QuickButton** button,
			Str::LocaleString label_str, Str::LocaleString button_str, 
			OpInputAction* button_action)
{
	*list = NULL;
	*count_label = NULL;
	*button = NULL;

	OpAutoPtr<QuickStackLayout> list_aptr(
			OP_NEW(QuickStackLayout, (QuickStackLayout::VERTICAL)));
	RETURN_OOM_IF_NULL(list_aptr.get());

	OpAutoPtr<QuickStackLayout> header_aptr(
			OP_NEW(QuickStackLayout, (QuickStackLayout::HORIZONTAL)));
	RETURN_OOM_IF_NULL(header_aptr.get());

	OpAutoPtr<QuickLabel> count_label_aptr(
			QuickLabel::ConstructLabel(UNI_L("")));
	RETURN_OOM_IF_NULL(count_label_aptr.get());

	count_label_aptr->GetOpWidget()->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);

	OpAutoPtr<QuickButton> button_aptr(
			QuickButton::ConstructButton(button_str, button_action));
	RETURN_OOM_IF_NULL(button_aptr.get());
	
	button_aptr->GetOpWidget()->SetButtonTypeAndStyle(
			OpButton::TYPE_TOOLBAR, OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	button_aptr->GetOpWidget()->SetTabStop(g_op_ui_info->IsFullKeyboardAccessActive());

	QuickLabel* tmp_count_label = count_label_aptr.get();
	QuickButton* tmp_button = button_aptr.get();

	OpAutoPtr<QuickCenteredVertical> label_centered_aptr(
			OP_NEW(QuickCenteredVertical, ()));
	RETURN_OOM_IF_NULL(label_centered_aptr.get());

	label_centered_aptr->SetContent(count_label_aptr.release());

	RETURN_IF_ERROR(header_aptr->InsertWidget(label_centered_aptr.release()));
	RETURN_IF_ERROR(header_aptr->InsertEmptyWidget(0, 0, WidgetSizes::Fill, 
			WidgetSizes::Fill));
	RETURN_IF_ERROR(header_aptr->InsertWidget(button_aptr.release()));
	RETURN_IF_ERROR(list_aptr->InsertWidget(header_aptr.release()));
	RETURN_IF_ERROR(list_aptr->InsertEmptyWidget(1, 13, 1, 13));

	*count_label = tmp_count_label;
	*button = tmp_button;
	*list = list_aptr.release();

	return OpStatus::OK;
}

template <typename T>
ExtensionsManagerView* ExtensionsPanel::ConstructExtensionsList(
		QuickWidget** widget, QuickLabel** counter_label, 
			Str::LocaleString button_str, OpInputAction::Action action, 
			const char* button_skin)
{
	*widget = NULL;
	*counter_label = NULL;

	QuickLabel* tmp_label = NULL;
	QuickStackLayout* tmp_stack = NULL;
	QuickButton* button = NULL;
	OpInputAction input_action(action);
	RETURN_VALUE_IF_ERROR(
			ConstructHeader(&tmp_stack, &tmp_label, &button,
				Str::D_EXTENSION_PANEL_COUNTER,
				button_str, 
				&input_action), NULL);
	OpAutoPtr<QuickStackLayout> widget_aptr(tmp_stack);	

	OpAutoPtr<T> extensions_list_aptr(OP_NEW(T, ()));
	RETURN_VALUE_IF_NULL(extensions_list_aptr.get(), NULL);

	ExtensionsManagerView* view_tmp = extensions_list_aptr.get();

	OpAutoPtr<QuickSkinElement> skinned_list_aptr(
			QuickSkinWrap(extensions_list_aptr.release(), 
				"Extensions Panel List Skin"));
	RETURN_VALUE_IF_NULL(skinned_list_aptr.get(), NULL);

	RETURN_VALUE_IF_ERROR(
			widget_aptr->InsertWidget(skinned_list_aptr.release()), NULL);
	
	*widget = widget_aptr.release();
	*counter_label = tmp_label;

	button->SetImage(button_skin);
	button->GetOpWidget()->SetFixedImage(TRUE);
#ifndef _MACINTOSH_	
	button->GetOpWidget()->SetTabStop(g_op_ui_info->IsFullKeyboardAccessActive());
#endif // !_MACINTOSH_

	return view_tmp;
}

QuickWidget* ExtensionsPanel::ConstructNormalExtensionsList()
{
	QuickWidget* widget = NULL;
	m_extensions_view = 
		ConstructExtensionsList<ExtensionsManagerListView>(&widget, 
			&m_counter_label, Str::D_EXTENSION_PANEL_GET_MORE,
				OpInputAction::ACTION_GET_MORE_EXTENSIONS,
				"Extensions Panel Button Get More");
	RETURN_VALUE_IF_NULL(m_extensions_view, NULL);

	return widget;
}

QuickWidget* ExtensionsPanel::ConstructDeveloperExtensionsList()
{
	QuickWidget* widget = NULL;
	m_extensions_dev_view = 
		ConstructExtensionsList<ExtensionsManagerDevListView>(&widget, 
			&m_dev_counter_label, Str::D_EXTENSION_PANEL_OPEN_ERROR_CONSOLE,
				OpInputAction::ACTION_SHOW_MESSAGE_CONSOLE,
				"Extensions Panel Button Open Error Console");
	RETURN_VALUE_IF_NULL(m_extensions_dev_view, NULL);

	return widget;
}

void ExtensionsPanel::OnLayoutPanel(OpRect& rect)
{
	if (m_contents_changed || !rect.Equals(m_prev_rect))
	{
		m_content->Layout(rect);
		m_prev_rect = rect;
		m_contents_changed = FALSE;
	}
}

void ExtensionsPanel::OnContentsChanged()
{
	Relayout();
	m_contents_changed = TRUE;
}

void ExtensionsPanel::OnBeforeExtensionsModelRefresh(
		const ExtensionsModel::RefreshInfo& info)
{
	m_dev_count = info.m_dev_count;
	m_normal_count = info.m_normal_count;

	ResetViews();

	if (m_dev_count > 0 || m_normal_count > 0)
	{
		m_page_layout->GoToPage(EXTENSIONS_LISTS_PAGE);
	}
	else
	{
		m_page_layout->GoToPage(EMPTY_PANEL_PAGE);
	}

	RefreshCounterLabels();
}

OP_STATUS ExtensionsPanel::RefreshCounterLabels()
{
	if (m_extensions_view)
	{
		OpString str, counter_label_fmt;
		RETURN_IF_ERROR(
				g_languageManager->GetString(Str::D_EXTENSION_PANEL_COUNTER, 
					counter_label_fmt));
		RETURN_IF_ERROR(
				str.AppendFormat(counter_label_fmt.CStr(), m_normal_count));
		RETURN_IF_ERROR(
				m_counter_label->SetText(str.CStr()));
	}

	if (m_extensions_dev_view)
	{
		OpString str, counter_label_fmt;
		RETURN_IF_ERROR(
				g_languageManager->GetString(Str::D_EXTENSION_PANEL_DEV_COUNTER, 
					counter_label_fmt));
		RETURN_IF_ERROR(
				str.AppendFormat(counter_label_fmt.CStr(), m_dev_count));
		RETURN_IF_ERROR(
				m_dev_counter_label->SetText(str.CStr()));
	}
	
	return OpStatus::OK;
}

BOOL ExtensionsPanel::OnInputAction(OpInputAction* action)
{
	return g_desktop_extensions_manager->HandleAction(action); 
}

void ExtensionsPanel::GetPanelText(OpString& text,
								  Hotlist::PanelTextType text_type)
{
	switch (text_type)
	{
		case Hotlist::PANEL_TEXT_LABEL:
		case Hotlist::PANEL_TEXT_TITLE:
		{
			OpStatus::Ignore(g_languageManager->GetString(Str::D_EXTENSIONS, text));
			break;
		}
	}
}

void ExtensionsPanel::OnFullModeChanged(BOOL full_mode) 
{
}

BOOL ExtensionsPanel::OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
{
	return m_content->SetScroll(delta, smooth);
}

void ExtensionsPanel::OnMouseEvent(OpWidget *widget,
								   INT32 pos,
								   INT32 x, INT32 y,
								   MouseButton button,
								   BOOL down,
								   UINT8 nclicks)
{
	if (nclicks == 1 && button == MOUSE_BUTTON_2)
	{
		OP_ASSERT(!"not implemented");
	}
}

void ExtensionsPanel::OnDeleted()
{
	if (m_extensions_view)
	{
		OpStatus::Ignore(
				g_desktop_extensions_manager->RemoveExtensionsModelListener(
					m_extensions_view));
	}
	if (m_extensions_dev_view)
	{
		OpStatus::Ignore(
				g_desktop_extensions_manager->RemoveExtensionsModelListener(
					m_extensions_dev_view));
	}
	OpStatus::Ignore(
			g_desktop_extensions_manager->RemoveExtensionsModelListener(
				this));

	OP_DELETE(m_content);
	m_content = NULL;

	HotlistPanel::OnDeleted();
}
