/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"

#include "modules/pi/OpScreenInfo.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsfile.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/MouseGestureUI.h"
#include "adjunct/quick_toolkit/widgets/WidgetCreator.h"
#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick_toolkit/widgets/OpImageWidget.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/widgets/OpTabs.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpOverlay.h"
#include "adjunct/quick_toolkit/widgets/OpButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/OpWindowMover.h"
#include "adjunct/quick/managers/opsetupmanager.h"
#include "modules/skin/OpSkinManager.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "modules/util/OpLineParser.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "modules/locale/oplanguagemanager.h"
#include "adjunct/quick/managers/AnimationManager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/desktop_util/widget_utils/WidgetUtils.h"


Dialog::Dialog() :
	m_do_not_show_again(NULL),
	m_progress_indicator(NULL),
	m_tabs(NULL),
	m_skin_widget(NULL),
	m_header_widget(NULL),
	m_title_widget(NULL),
	m_dialog_mover_widget(NULL),
	m_image_widget(NULL),
	m_content_group(NULL),
	m_pages_group(NULL),
	m_current_page(NULL),
	m_current_page_number(0),
	m_listener(NULL),
	// m_dialog_type,
	m_parent_desktop_window(NULL),
	m_is_ok(FALSE),
	m_is_ync_cancel(FALSE),
	m_call_cancel(TRUE),
	m_button_strip(NULL),
	m_pages(NULL),
	m_pages_model(NULL),
	m_pages_view(NULL),
	m_back_enabled(TRUE),
	m_forward_enabled(TRUE),
	m_ok_enabled(TRUE),
	m_has_been_shown(FALSE),
	m_content_valid(FALSE),
	m_timer(NULL),
	m_protected_against_double_click(FALSE),
	m_parent_browser_view(NULL),
	m_overlay_layout_widget(NULL)
{
}

Dialog::~Dialog()
{
	RemoveListener(this);

	if (m_parent_desktop_window)
	{
		m_parent_desktop_window->RemoveListener(this);
		m_parent_desktop_window->RemoveDialog(this);

		if (m_parent_desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
		{
			DocumentDesktopWindow* document = static_cast<DocumentDesktopWindow*>(m_parent_desktop_window);
			document->RemoveDocumentWindowListener(this);
		}
	}

	OP_DELETE(m_timer);
	OP_DELETE(m_pages);
}

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS Dialog::Init(DesktopWindow* parent_window, INT32 start_page, BOOL modal, OpBrowserView *parent_browser_view)
{
	// this must be the first line to close and free memory in case of Init() failure
	// DialogCloser calls Close(), which calls InternalDesktopWindowListener::OnClose() and deletes "this" object
	DialogCloser closer(this);

	m_pages = OP_NEW(OpWidgetVector<OpWidget>, ());
	if (!m_pages)
		return OpStatus::ERR_NO_MEMORY;

	m_dialog_type = GetDialogType();
	m_parent_desktop_window = parent_window;
	m_parent_browser_view = parent_browser_view;
	m_title_widget = NULL;
	m_dialog_mover_widget = NULL;

	OpWindow::Style windowstyle;
	UINT32 effects = 0;

	if (m_parent_desktop_window && m_parent_desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		DocumentDesktopWindow* document = static_cast<DocumentDesktopWindow*>(m_parent_desktop_window);
		document->AddDocumentWindowListener(this);
	}

	if(GetIsBlocking())
	{
		windowstyle = OpWindow::STYLE_BLOCKING_DIALOG;
	}
	else if (GetIsWindowTool())
	{ 
		windowstyle = OpWindow::STYLE_GADGET;
	}
	else
	{
		if(GetModality())
		{
			windowstyle = OpWindow::STYLE_MODAL_DIALOG;
		}
		else if( GetIsConsole() )
		{
			windowstyle = OpWindow::STYLE_CONSOLE_DIALOG;
		}
		else
		{
			windowstyle = OpWindow::STYLE_MODELESS_DIALOG;
		}
	}
	if (GetDropShadow())
	{
		effects |= OpWindow::EFFECT_DROPSHADOW;
	}

	if (GetTransparent())
	{
		effects |= OpWindow::EFFECT_TRANSPARENT;
	}

	if (GetIsWindowTool() || HideWhenDesktopWindowInActive() || GetOverlayed())
	{
		// listen to the parent for activation changes
		if (parent_window && parent_window->GetParentWorkspace())
			parent_window->AddListener(this);
	}

	if (GetOverlayed() && m_parent_desktop_window && !m_parent_browser_view)
	{
		m_parent_browser_view = m_parent_desktop_window->GetBrowserView();
	}
	if (GetOverlayed() && m_parent_desktop_window && m_parent_browser_view)
	{
		windowstyle = OpWindow::STYLE_TRANSPARENT;
		//Dropshadow should be done in the "Overlay Dialog Skin". If not, enable this line:
		//effects = OpWindow::EFFECT_DROPSHADOW;
	}
	RETURN_IF_ERROR(DesktopWindow::Init(windowstyle, m_parent_desktop_window, effects));

	if (GetDimPage() && m_parent_browser_view)
		m_parent_browser_view->RequestDimmed(true);

	if (parent_window)
	{
		parent_window->AddDialog(this);
		SetParentInputContext(parent_window);
	}

	SetResourceName( GetWindowName() ); // Needed on X-windows. Do not remove

	m_current_page_number = start_page;

	// initialize default button text
	g_languageManager->GetString(GetOkTextID(), m_ok_text);
	g_languageManager->GetString(GetCancelTextID(), m_cancel_text);
	g_languageManager->GetString(GetYNCCancelTextID(), m_ync_cancel_text);
	g_languageManager->GetString(GetHelpTextID(), m_help_text);
	g_languageManager->GetString(GetDoNotShowAgainTextID(), m_do_not_show_text);

	OpWidget* root_widget = GetWidgetContainer()->GetRoot();

	const bool rtl = root_widget->GetRTL() == TRUE;

	// m_content_group is the widget that is parent of any pages treeview (like in prefs) and the m_pages_group

	RETURN_IF_ERROR(OpGroup::Construct(&m_content_group));
	root_widget->AddChild(m_content_group);
	m_content_group->SetXResizeEffect(RESIZE_SIZE);
	m_content_group->SetYResizeEffect(RESIZE_SIZE);
	m_content_group->SetSkinned(TRUE);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	m_content_group->AccessibilitySkipsMe();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
	m_content_group->SetRTL(rtl);

	// m_pages_group is the widget that is parent of all the sub pages

	RETURN_IF_ERROR(OpGroup::Construct(&m_pages_group));
	m_content_group->AddChild(m_pages_group);
	m_pages_group->SetXResizeEffect(RESIZE_SIZE);
	m_pages_group->SetYResizeEffect(RESIZE_SIZE);
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	m_pages_group->AccessibilitySkipsMe();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
	m_pages_group->SetRTL(rtl);

	// read the contents

	ReadDialog();

	if (!m_pages_group->childs.First())
	{
		OP_ASSERT(FALSE); // missing dialog.ini? (Trond)
		return OpStatus::ERR;
	}

	if (!m_button_strip)
	{
		RETURN_IF_ERROR(OpButtonStrip::Construct(&m_button_strip, g_op_ui_info->DefaultButtonOnRight()));
		root_widget->AddChild(m_button_strip);
		m_button_strip->SetXResizeEffect(RESIZE_SIZE);
		m_button_strip->SetYResizeEffect(RESIZE_SIZE);
		m_button_strip->SetRTL(rtl);
	}

	if (GetShowPagesAsTabs())
	{
		RETURN_IF_ERROR(OpTabs::Construct(&m_tabs));
		root_widget->AddChild(m_tabs, FALSE, TRUE);
		m_tabs->SetXResizeEffect(RESIZE_SIZE);
	}
	else if (GetShowPagesAsList())
	{
		RETURN_IF_ERROR(OpTreeView::Construct(&m_pages_view));
		m_pages_model = OP_NEW(SimpleTreeModel, ());
		if (!m_pages_model)
			return OpStatus::ERR_NO_MEMORY;

		m_pages_view->SetTreeModel(m_pages_model);
		m_pages_view->SetDeselectable(FALSE);
		m_pages_view->SetShowColumnHeaders(FALSE);
		m_pages_view->SetYResizeEffect(RESIZE_SIZE);
		m_content_group->AddChild(m_pages_view, FALSE, TRUE);
	}

	// make the image that the dialog might have

	const char* image = GetDialogImage();

	if (image && g_skin_manager->GetSkinElement(image))
	{
		if (0 != (m_image_widget = OP_NEW(OpWidget, ())))
		{
			m_content_group->AddChild(m_image_widget);
			m_image_widget->SetSkinned(TRUE);
		}
	}

	// iterate through all pages for init

	INT32 page_number = 0;

	OpWidget* page = (OpWidget*) m_pages_group->childs.First();

	while (page)
	{
#ifdef VEGA_OPPAINTER_SUPPORT
		QuickDisableAnimationsHere dummy;
#endif

		page->SetUserData((void*)0);
		page->SetVisibility(FALSE);

		m_pages->Add(page);

		if (GetShowPage(page_number))
		{
			OpString page_title;

			page->GetText(page_title);

			if (m_tabs)
			{
				OpGroup* group = static_cast<OpGroup*>(page);
				OpButton* button = m_tabs->AddTab(page_title.CStr(), (void *)(INTPTR) page_number);
				button->SetName(group->GetName());
				button->SetPropertyPage(group);
				button->GetForegroundSkin()->SetImage(page->GetName().CStr());
				group->SetTab(button);

				if (m_current_page_number == page_number)
				{
					m_tabs->SetSelected(m_tabs->GetTabCount() - 1, FALSE);
				}
			}
			else if (m_pages_view)
			{
				m_pages_model->AddItem(page_title.CStr(), page->GetName().CStr(), 0, -1, NULL, page_number);

				if (m_current_page_number == page_number)
				{
					m_pages_view->SetSelectedItemByID(page_number);
				}
			}
		}

		page = (OpWidget*) page->Suc();
		page_number++;
	}

	if (m_current_page_number >= (INT32) m_pages->GetCount())
		m_current_page_number = 0;

#if defined(HAS_TAB_BUTTON_POSITION)
	if (m_tabs)
		m_tabs->UpdateTabStates(m_current_page_number);
#endif

	m_current_page = GetPageByNumber(m_current_page_number);
	m_current_page->SetVisibility(TRUE);

	if (GetDoNotShowAgain())
	{
		if (OpStatus::IsSuccess(OpCheckBox::Construct(&m_do_not_show_again)))
		{
			m_do_not_show_again->SetName(WIDGET_NAME_CHECKBOX_DEFAULT);

			m_button_strip->AddChild(m_do_not_show_again);

			m_do_not_show_again->SetText(GetDoNotShowAgainText());
			m_do_not_show_again->SetYResizeEffect(RESIZE_MOVE);
			if (rtl)
				m_do_not_show_again->SetXResizeEffect(RESIZE_MOVE);
			m_do_not_show_again->SetRTL(rtl);
		}
	}
	else if (GetShowProgressIndicator())
	{
		if (OpStatus::IsSuccess(OpProgressBar::Construct(&m_progress_indicator)))
		{
			m_button_strip->AddChild(m_progress_indicator);
			m_progress_indicator->SetYResizeEffect(RESIZE_MOVE);
			if (rtl)
				m_progress_indicator->SetXResizeEffect(RESIZE_MOVE);
			m_progress_indicator->SetRTL(rtl);
		}
	}

	if (g_skin_manager->GetSkinElement(GetHeaderSkin()))
	{
		if (0 != (m_header_widget = OP_NEW(OpWidget, ())))
		{
			root_widget->AddChild(m_header_widget);
			m_header_widget->SetSkinned(TRUE);
			m_header_widget->SetXResizeEffect(RESIZE_SIZE);
		}
	}

	if (IsMovable() && GetOverlayed() && m_parent_desktop_window && m_parent_browser_view)
	{
		// Create a mover widget so this dialog can be moved.
		OpWindowMover *tmp;
		if (OpStatus::IsSuccess(OpWindowMover::Construct(&tmp)))
		{
			tmp->SetTargetWindow(*this);
			m_dialog_mover_widget = tmp;
			root_widget->AddChild(m_dialog_mover_widget);
			m_dialog_mover_widget->SetXResizeEffect(RESIZE_SIZE);
			// So that this widget doesn't obscure the title widget:
			m_dialog_mover_widget->SetSkinIsBackground(TRUE);
		}

		// Create a label for the title
		if (OpStatus::IsSuccess(OpLabel::Construct(&m_title_widget)))
		{
			root_widget->AddChild(m_title_widget);
			m_title_widget->SetXResizeEffect(RESIZE_SIZE);
			m_title_widget->SetText(GetTitle());
			m_title_widget->GetBorderSkin()->SetImage("Overlay Dialog Title Skin");
		}
		// Need to listen to the title change (OnDesktopWindowChanged)
		AddListener(this);
	}

	INT32 num_buttons = GetButtonCount();
	RETURN_IF_ERROR(m_button_strip->SetButtonCount(num_buttons));

	for (INT32 i = 0; i < num_buttons; i++)
	{
		OpInputAction* action = NULL;
		OpString text;
		BOOL is_enabled = TRUE;
		BOOL is_default = FALSE;
		OpString8 name;

		GetButtonInfo(i, action, text, is_enabled, is_default, name);

		RETURN_IF_ERROR(m_button_strip->SetButtonInfo(i, action, text, is_enabled, TRUE, name));
		if (is_default)
			m_button_strip->SetDefaultButton(i);
	}

	if (m_dialog_type == TYPE_WIZARD)
	{
		OpString finish;
		g_languageManager->GetString(Str::S_WIZARD_FINISH, m_finish_button_text);
		m_button_strip->SetHasBackForwardButtons(TRUE);
	}

	if (0 != (m_skin_widget = OP_NEW(OpWidget, ())))
	{
		root_widget->AddChild(m_skin_widget);

		m_skin_widget->SetXResizeEffect(RESIZE_SIZE);
		m_skin_widget->SetYResizeEffect(RESIZE_SIZE);
		m_skin_widget->SetSkinned(TRUE);
	}

	closer.Release(); // No longer necessary from here on

	// let dialog hide certain elements before we compute page sizes

	OnInitVisibility();
	OnInit();
	InitPage(m_current_page_number);

	m_content_valid = TRUE; // Indicates that all content has been set up and that ResetDialog() can access it all

	if (parent_window && !parent_window->IsActive() && parent_window->GetParentWorkspace())
	{
		if (!GetIsBlocking())
		{
			// show it later, when parent is activated
			ResetDialog();
			parent_window->AddListener(this);
			parent_window->SetAttention(TRUE);
			return OpStatus::OK;
		}
		else
			// avoid showing a blocking dialog in front of the wrong page
			parent_window->Activate();
	}

	ShowDialogInternal();

	return OpStatus::OK; // This is only useful for modeless dialogs

	// No code after this
}

const char* Dialog::GetFallbackSkin()
{
	if (GetOverlayed() && m_parent_desktop_window && m_parent_browser_view)
		return "Overlay Dialog Skin";
	return "Dialog Skin";
}

void Dialog::CloseDialog(BOOL call_cancel, BOOL immediatly, BOOL user_initiated)
{
	m_call_cancel = call_cancel; 
	Close(immediatly, user_initiated);
}


/***********************************************************************************
**
**	ReadDialog
**
***********************************************************************************/

void Dialog::ReadDialog()
{
	const char* dialog_name = GetWindowName(); // "Simple Dialog"

	PrefsSection *section;
	TRAPD(err, section = g_setup_manager->GetSectionL(dialog_name, OPDIALOG_SETUP, NULL, FALSE));
	if (OpStatus::IsError(err) || !section)
	{
		return;
	}

	OpAutoPtr<PrefsSection> section_ap(section);

	ReadDialogWidgets(section->Entries(), GetPagesGroup());

	OpStringC title_value(section->Get(UNI_L("Title")));

	if (title_value.HasContent())
	{
		OpLineParser line(title_value.CStr());
		OpString title;

		line.GetNextLanguageString(title);
		SetTitle(title.CStr());
	}
}

const PrefsEntry* Dialog::ReadDialogWidgets(const PrefsEntry *entry, OpWidget* parent)
{
	while (entry)
	{
		WidgetCreator widget_creator;

		widget_creator.SetActionData(entry->Value());
		widget_creator.SetParent(parent);
	
		OpWidget* widget = 0;
		if (OpStatus::IsSuccess(widget_creator.SetWidgetData(entry->Key())))
			widget = widget_creator.CreateWidget();

		entry = (const PrefsEntry *) entry->Suc();

		if (widget)
		{
			if (widget->GetType() == OpTypedObject::WIDGET_TYPE_GROUP)
				entry = ReadDialogWidgets(entry, widget);
			else if (widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON_STRIP)
				m_button_strip = static_cast<OpButtonStrip*>(widget);
		}

		if (widget_creator.IsEndWidget())
			return entry;
	}

	return 0;
}


/***********************************************************************************
**
**	ResetPageWidgetsToOriginalSizes
**
***********************************************************************************/

void Dialog::ResetPageWidgetsToOriginalSizes(OpWidget* parent)
{
	for (OpWidget* child = parent->GetFirstChild(); child; child = child->GetNextSibling())
		if (!child->IsInternalChild())
		{
			parent->SetChildRect(child, child->GetOriginalRect(), FALSE, FALSE);
			ResetPageWidgetsToOriginalSizes(child);
		}
}

/***********************************************************************************
**
**	ResetDialog
**
**	Reseting the dialog will cause a relayout and support the following changes:
**
**	- Skin can have changed
**	- Page widgets visibility can have changed (resize/recropping will occur)
**
**	The following is currently NOT supported to change
**
**	- Dialog type
**	- Dialog buttons
**	- Dialog image
**	- Shown pages
**  - +++
**
***********************************************************************************/

void Dialog::ResetDialog()
{
	// Test that Init actually succeeded. If not, DialogCloser will bring us here in an unstable state
	if (!m_content_valid) 
		return;

	ResetPageWidgetsToOriginalSizes(m_pages_group);

	OnReset();

	// Find out what skin elements to use.. could probably be rewritten to take advantage of the new fallback
	// skin support in OpWidgetImage

	if (GetShowPagesAsTabs())
	{
		m_pages_skin.Set(GetWindowName());
		m_pages_skin.Append(" Tab Page Skin");

		if (!g_skin_manager->GetSkinElement(m_pages_skin.CStr()))
		{
			// If a skin contains "Dialog Page Skin" but not "Dialog Tab Page Skin"
			// use "Dialog Page Skin" as fallback instead of "Dialog Tab Page Skin"
			// from standard skin, this is the conventional behavior and some skins
			// depend on this to work properly. See DSK-289395
			OpSkin* current = g_skin_manager->GetCurrentSkin();
			if (current && !current->GetSkinElement("Dialog Tab Page Skin") && current->GetSkinElement("Dialog Page Skin"))
				m_pages_skin.Set("Dialog Page Skin");
			else
				m_pages_skin.Set("Dialog Tab Page Skin");
		}
	}

	if (!GetShowPagesAsTabs() || !g_skin_manager->GetSkinElement(m_pages_skin.CStr()))
	{
		m_pages_skin.Set(GetWindowName());
		m_pages_skin.Append(" Page Skin");

		if (!g_skin_manager->GetSkinElement(m_pages_skin.CStr()))
		{
			m_pages_skin.Set("Dialog Page Skin");
		}
	}

	m_header_skin.Set(GetWindowName());
	m_header_skin.Append(" Header Skin");

	if (!g_skin_manager->GetSkinElement(m_header_skin.CStr()))
	{
		m_header_skin.Set("Dialog Header Skin");
	}

	m_button_border_skin.Set(GetWindowName());
	m_button_border_skin.Append(" Button Border Skin");

	if (!g_skin_manager->GetSkinElement(m_button_border_skin.CStr()))
	{
		m_button_border_skin.Set("Dialog Button Border Skin");
	}

	m_button_strip->SetBorderSkin(GetButtonBorderSkin());

	// Use background skin so the root is not painted with default skin
	m_content_group->GetBorderSkin()->SetImage(GetPagesSkin());
	m_content_group->SetSkinIsBackground(TRUE);

	BOOL is_h_sizeable = FALSE;
	BOOL is_v_sizeable = FALSE;

	INT32 dialog_left_padding = 10;
	INT32 dialog_top_padding = 10;
	INT32 dialog_right_padding = 10;
	INT32 dialog_bottom_padding = 10;

	g_skin_manager->GetPadding(GetSkin(), &dialog_left_padding, &dialog_top_padding, &dialog_right_padding, &dialog_bottom_padding);

	INT32 page_left_padding = 10;
	INT32 page_top_padding = 10;
	INT32 page_right_padding = 10;
	INT32 page_bottom_padding = 10;

	g_skin_manager->GetPadding(GetPagesSkin(), &page_left_padding, &page_top_padding, &page_right_padding, &page_bottom_padding);

	INT32 page_spacing = 10;

	g_skin_manager->GetSpacing(GetPagesSkin(), &page_spacing);

	//padding and margin of extra button border (standard a shadow effect)
	INT32 button_border_left_padding = 0;
	INT32 button_border_top_padding = 0;
	INT32 button_border_right_padding = 0;
	INT32 button_border_bottom_padding = 0;

	g_skin_manager->GetPadding(GetButtonBorderSkin(), &button_border_left_padding, &button_border_top_padding, &button_border_right_padding, &button_border_bottom_padding);
	m_button_strip->SetButtonPadding(button_border_left_padding, button_border_top_padding, button_border_right_padding, button_border_bottom_padding);

	INT32 button_border_left_margin = 0;
	INT32 button_border_top_margin = 0;
	INT32 button_border_right_margin = 0;
	INT32 button_border_bottom_margin = 0;

	g_skin_manager->GetMargin(GetButtonBorderSkin(), &button_border_left_margin, &button_border_top_margin, &button_border_right_margin, &button_border_bottom_margin);
	m_button_strip->SetButtonMargins(button_border_left_margin, button_border_top_margin, button_border_right_margin, button_border_bottom_margin);

	// making the dummy scale string translatable makes it easy for translators
	// to either keep the same dummy string, or create a "longer" one to cause
	// all dialogs to scale a bit to compensate for a language where
	// every string is noticably wider

	OpWidget* root_widget = GetWidgetContainer()->GetRoot();
	OpWidgetString dummy_scale_string;

	dummy_scale_string.Set(UNI_L("Dummy scale string"), root_widget);

	INT32 dummy_width = dummy_scale_string.GetWidth(); // this string has length 91 for ms sans serif setup.. testing..

	m_x_scale = (float) g_skin_manager->GetOptionValue("Dialog Width Scale", 100) * dummy_width;
	m_y_scale = (float) g_skin_manager->GetOptionValue("Dialog Height Scale", 100) * root_widget->font_info.size;

	m_x_scale /= (float) (91 * 100);
	m_y_scale /= (float) (11 * 100);

#ifdef _MACINTOSH_
	// Only for Mac, because we changed the default dialog font from 11pt to 13pt
	m_x_scale *= 0.90;
	m_y_scale *= 0.84;
#endif

	INT32 button_width = 0;
	INT32 button_height = 0;

	if (m_button_strip->IsCustomSize())
	{
		m_button_strip->GetButtonSize(button_width, button_height);
	}
	else if (GetButtonCount() > 0)
	{
#ifndef _MACINTOSH_
		button_width = ScaleX(80);
#else
		button_width = ScaleX(57);
#endif
		button_height = ScaleY(23);

		m_button_strip->SetButtonSize(button_width, button_height);
	}

	// paddings & margins to be added to raw button width/height
	INT32 buttonheight_padding_margin = button_border_top_padding + button_border_bottom_padding + button_border_top_margin + button_border_bottom_margin;

	INT32 dialog_buttons_width = m_button_strip->CalculatePositions();

	INT32 page_max_width = 0;
	INT32 page_max_height = 0;

	if (m_dialog_type == TYPE_WIZARD)
	{
		// This used to be a min size of 300x300 but with the introduction of the link wizard,
		// we have reduced the size. If this causes major regressions with other wizards we
		// may need to re-think this.
		page_max_width = 250;
		page_max_height = 150;
	}
	else if (GetShowPagesAsTabs())
	{
		page_max_width = 250;
		page_max_height = 150;
	}

	// Make room for title if there is one

	INT32 title_top_padding = 0;
	INT32 title_height = 0;
	if (m_title_widget)
	{
		title_top_padding = dialog_top_padding; // hack. We don't have this value in the skin.
		title_height = dummy_scale_string.GetHeight();
		dialog_top_padding += title_height;
	}

	// make the image that the dialog might have
	// Position it later, when the parent rect is known.

	OpRect image_rect;
	if (m_image_widget)
	{
		const char* image = GetDialogImage();

		m_image_widget->GetBorderSkin()->SetImage(image);

		INT32 width, height;

		m_image_widget->GetBorderSkin()->GetSize(&width, &height);
		image_rect = OpRect(page_left_padding, page_top_padding, width, height);

		page_left_padding += 8 + width;

		if (height > page_max_height)
			page_max_height = height;
	}

	// iterate through all children of pages and compute minimum size

	OpWidget* page = (OpWidget*) m_pages_group->childs.First();

	while (page)
	{
		if (IsScalable())
		{
			ScaleWidget(page);
		}

		page->SetXResizeEffect(RESIZE_CENTER);
		page->SetYResizeEffect(RESIZE_CENTER);

		int min_x = 10000;
		int min_y = 10000;
		int max_x = 0;
		int max_y = 0;

		OpWidget* widget = (OpWidget*) page->childs.First();

		while (widget)
		{
			if (widget->IsVisible())
			{
				OpRect rect = widget->GetRect();

				if (rect.x + rect.width > max_x)
				{
					max_x = rect.x + rect.width;
				}

				if (rect.y + rect.height > max_y)
				{
					max_y = rect.y + rect.height;
				}

				if (rect.x < min_x)
				{
					min_x = rect.x;
				}

				if (rect.y < min_y)
				{
					min_y = rect.y;
				}

				if (widget->GetXResizeEffect() == RESIZE_SIZE)
				{
					page->SetXResizeEffect(RESIZE_SIZE);
					is_h_sizeable = TRUE;
				}

				if (widget->GetYResizeEffect() == RESIZE_SIZE)
				{
					page->SetYResizeEffect(RESIZE_SIZE);
					is_v_sizeable = TRUE;
				}
			}

			widget = (OpWidget*) widget->Suc();
		}

		widget = (OpWidget*) page->childs.First();

		while (widget)
		{
			// This has been removed since some hiddedn controls
			// actually turn on and off in use (i.e. OpProgressBar) and
			// they still need to be positioned correctly even if they are hidden
			// at the start. If this breaks other dialogs we should add a
			// special case for OpProgressbar.
			// if (widget->IsVisible())
			{
				OpRect rect = widget->GetRect();

				rect.x -= min_x;
				rect.y -= min_y;

				widget->SetRect(rect, FALSE, FALSE);
			}

			widget = (OpWidget*) widget->Suc();
		}

		// size page to what it needs so we can center it later.. don't resize children

		page->SetRect(OpRect(0, 0, max_x - min_x, max_y - min_y), FALSE, FALSE);

		// remember biggest page

		if (max_x - min_x > page_max_width)
		{
			page_max_width = max_x - min_x;
		}

		if (max_y - min_y > page_max_height)
		{
			page_max_height = max_y - min_y;
		}

		page = (OpWidget*) page->Suc();
	}

	// dialog buttons can affect page_max_width

	INT32 bottom_left_widget_width = 0;
	INT32 bottom_left_widget_spacing = 0;

	if (m_do_not_show_again)
	{
		INT32 dummy;
		m_do_not_show_again->GetPreferedSize(&bottom_left_widget_width, &dummy, 1, 1);
		g_skin_manager->GetMargin(GetButtonBorderSkin(), &bottom_left_widget_spacing, &dummy, &dummy, &dummy);
	}
	else if (m_progress_indicator)
	{
		const INT32 indicator_width = ScaleX(100);
		const INT32 indicator_height = ScaleY(23);

		m_progress_indicator->SetSize(indicator_width, indicator_height);

		bottom_left_widget_width = indicator_width + 25;

		INT32 dummy;
		g_skin_manager->GetMargin(GetButtonBorderSkin(), &bottom_left_widget_spacing, &dummy, &dummy, &dummy);
	}

	if (page_max_width + page_left_padding + page_right_padding < dialog_buttons_width + bottom_left_widget_width + bottom_left_widget_spacing)
	{
		page_max_width = dialog_buttons_width + bottom_left_widget_width + bottom_left_widget_spacing - page_left_padding - page_right_padding;
	}

	// not too small.. looks bad

	if (page_max_width < 200)
	{
		page_max_width = 200;
	}

	// Make the rect for the pages root

	OpRect pages_root_rect(dialog_left_padding, dialog_top_padding, page_max_width + page_left_padding + page_right_padding, page_max_height + page_top_padding + page_bottom_padding);

	// Make the rect for the pages group

	OpRect pages_group_rect(page_left_padding, page_top_padding, page_max_width, page_max_height);

	// Position pages tree view

	if (m_pages_view)
	{
		INT32 pages_view_width = ScaleX(140);
		m_pages_view->SetRect(OpRect(page_left_padding, page_top_padding, pages_view_width, page_max_height));
		pages_root_rect.width += pages_view_width + page_left_padding + page_spacing;
		pages_group_rect.x += pages_view_width + page_left_padding + page_spacing;
	}

	// Position Tabs

	if (m_tabs)
	{
#ifdef QUICK_NEW_OPERA_MENU
		if (m_tabs->GetAlignment() == OpBar::ALIGNMENT_LEFT)
		{
			INT32 width = m_tabs->GetWidthFromHeight(page_max_height);
			m_tabs->SetRect(OpRect(dialog_left_padding, dialog_top_padding, width, pages_root_rect.height));
			pages_root_rect.x += width;
		}
		else if (m_tabs->GetAlignment() == OpBar::ALIGNMENT_TOP)
#endif // QUICK_NEW_OPERA_MENU
		{
			INT32 height = m_tabs->GetHeightFromWidth(page_max_width);
			m_tabs->SetRect(OpRect(dialog_left_padding, dialog_top_padding, pages_root_rect.width, height));
			pages_root_rect.y += height;
		}
	}

	m_full_width = pages_root_rect.x + pages_root_rect.width + dialog_right_padding;
	m_full_height = pages_root_rect.y + pages_root_rect.height + dialog_bottom_padding + button_height + buttonheight_padding_margin;

	// force size of root without resizing children

	root_widget->SetRect(OpRect(0, 0, m_full_width, m_full_height), FALSE, FALSE);

	// force size of pages root without resizing children

	root_widget->SetChildRect(m_content_group, pages_root_rect, FALSE, FALSE);

	// force size of pages group without resizing children

	m_content_group->SetChildRect(m_pages_group, pages_group_rect, FALSE, FALSE);

	// force size of button strip without resizing children (if button strip was not defined by user)
	if (m_button_strip->GetParent() == GetWidgetContainer()->GetRoot())
	{
		m_button_strip->SetRect(OpRect(dialog_left_padding, pages_root_rect.y + pages_root_rect.height, m_full_width - dialog_left_padding - dialog_right_padding, button_height + buttonheight_padding_margin));
	}

	// attach header

	if (m_header_widget)
	{
		m_header_widget->GetBorderSkin()->SetImage(GetHeaderSkin());

		INT32 width = 0;
		INT32 height = 0;

		g_skin_manager->GetSize(GetHeaderSkin(), &width, &height);

		m_header_widget->SetRect(OpRect(0, 0, root_widget->GetWidth(), height), FALSE, FALSE);
	}

	// title

	if (m_title_widget)
	{
		m_title_widget->SetRect(OpRect(dialog_left_padding, title_top_padding,
					root_widget->GetWidth() - dialog_left_padding - dialog_right_padding,
					title_height));
		m_title_widget->SetText(GetTitle());
	}

	if (m_dialog_mover_widget)
		m_dialog_mover_widget->SetRect(OpRect(0, 0, root_widget->GetWidth(), dialog_top_padding));

	// image
	if (m_image_widget)
		m_content_group->SetChildRect(m_image_widget, image_rect, FALSE, FALSE);

	// center pages now that we have max sizes

	if (GetCenterPages())
	for (unsigned i = 0; i < m_pages->GetCount(); i++)
	{
		page = GetPageByNumber(i);

		OpRect rect = page->GetRect();

		rect.x = (page_max_width - rect.width) / 2;
		rect.y = (page_max_height - rect.height) / 2;

		page->SetRect(rect, FALSE, FALSE);
	}

	// place dialog buttons

	// place "do_not_show_again" or progress indicator
	if (m_do_not_show_again)
	{
		OpRect rect(0, button_border_top_margin + button_border_top_padding, bottom_left_widget_width, button_height);
		m_button_strip->SetChildRect(m_do_not_show_again, rect, FALSE, FALSE);
	}
	else if (m_progress_indicator)
	{
		OpRect rect(0, button_border_top_margin + button_border_top_padding, bottom_left_widget_width, button_height);
		m_button_strip->SetChildRect(m_progress_indicator, rect, FALSE, FALSE);
	}
	m_button_strip->SetCenteredButtons(HasCenteredButtons());
	m_button_strip->PlaceButtons();

	if (m_skin_widget)
		m_skin_widget->SetRect(root_widget->GetBounds(), FALSE, FALSE);

	SetMinimumInnerSize(m_full_width, m_full_height);
	SetMaximumInnerSize(is_h_sizeable ? 10000 : m_full_width, is_v_sizeable ? 10000 : m_full_height);

	if (IsShowed())
	{
		UINT32 width, height;

		if (m_overlay_layout_widget)
		{
			m_placement_rect.Set(DEFAULT_SIZEPOS, DEFAULT_SIZEPOS, m_full_width, m_full_height);
			GetOverlayPlacement(m_placement_rect, m_overlay_layout_widget);
			width = m_placement_rect.width;
			height = m_placement_rect.height;

			// Introduced by DSK-304211
			OpRect target_rect = m_placement_rect;
			switch (GetPosAnimationType())
			{
			case POSITIONAL_ANIMATION_STANDARD:
				m_placement_rect.y -= m_placement_rect.height / 4;
				break;
			case POSITIONAL_ANIMATION_SHEET:
				m_placement_rect.y -= 2 * m_placement_rect.height;
				break;
			}

			BroadcastDesktopWindowAnimationRectChanged(m_placement_rect, target_rect);
		}
		else
		{
			GetInnerSize(width, height);
		}

		if (width < UINT32(m_full_width) || (!is_h_sizeable && width > UINT32(m_full_width)))
			width = m_full_width;

		if (height < UINT32(m_full_height) || (!is_v_sizeable && height > UINT32(m_full_height)))
			height = m_full_height;

		SetInnerSize(width, height);
		root_widget->SetSize(width, height);
		root_widget->InvalidateAll();
	}
	else
	{
		m_placement_rect.Set(DEFAULT_SIZEPOS, DEFAULT_SIZEPOS, m_full_width, m_full_height);
	}
}

/***********************************************************************************
**
**	ShowDialogInternal
**
***********************************************************************************/

void Dialog::ShowDialogInternal()
{
	if (IsShowed())
		return;

	// we need to sync the pending relayout to compute proper m_placement

	SyncLayout();

	if (m_overlay_layout_widget)
		ResetDialog();
	if (GetOverlayed() && m_parent_desktop_window && m_parent_browser_view && !m_overlay_layout_widget)
	{
		// Create OpOverlayLayoutWidget if overlayed so the dialog is positioned and resized by it.
		m_overlay_layout_widget = OP_NEW(OpOverlayLayoutWidget, ());
		if (!m_overlay_layout_widget || OpStatus::IsError(m_overlay_layout_widget->Init(this, m_parent_browser_view)))
		{
			OP_DELETE(m_overlay_layout_widget);
			m_overlay_layout_widget = NULL;
		}
	}
	if (m_overlay_layout_widget)
	{
		GetOverlayPlacement(m_placement_rect, m_overlay_layout_widget);

#ifdef VEGA_OPPAINTER_SUPPORT
		// Add animation of the dialog appearing...
		OpRect target_rect = m_placement_rect;
		AnimationCurve anim_curve = ANIM_CURVE_SLOW_DOWN;
		
		switch (GetPosAnimationType())
		{
			case POSITIONAL_ANIMATION_STANDARD:
				m_placement_rect.y -= m_placement_rect.height / 4;
				break;
			case POSITIONAL_ANIMATION_SHEET:
				m_placement_rect.y -= 2 * m_placement_rect.height;
				anim_curve = ANIM_CURVE_LINEAR;
				break;
			default:
				break;
		}

		QuickAnimationWindowObject *anim_win = OP_NEW(QuickAnimationWindowObject, ());
		if (!anim_win || OpStatus::IsError(anim_win->Init(this, m_overlay_layout_widget)))
			OP_DELETE(anim_win);
		else
		{
			if (GetPosAnimationType() != POSITIONAL_ANIMATION_SHEET)
				anim_win->animateOpacity(0, 1);
			anim_win->animateIgnoreMouseInput(TRUE, FALSE);
			anim_win->animateRect(m_placement_rect, target_rect);
			g_animation_manager->startAnimation(anim_win, anim_curve);
		}
#endif // VEGA_OPPAINTER_SUPPORT
	}
	else
		GetPlacement(m_placement_rect);	//get placement modified by subclasses

	BOOL force_rect = (m_placement_rect.x != DEFAULT_SIZEPOS || m_placement_rect.y != DEFAULT_SIZEPOS);

	Show(TRUE, &m_placement_rect, OpWindow::RESTORED, FALSE, force_rect, TRUE);
}

/***********************************************************************************
**
**	GetOverlayPlacement
**
***********************************************************************************/

void Dialog::GetOverlayPlacement(OpRect& initial_placement, OpWidget* overlay_layout_widget)
{
	OpRect parent_rect = overlay_layout_widget->GetParent()->GetRect();

	// here we force initial position to be either in the middle of window
	// or in left top corner, if dialog will try to be placed with negative coordinates
	initial_placement = OpRect(MAX((parent_rect.width - initial_placement.width) / 2, 0),
#ifdef _MACINTOSH_
								parent_rect.y,
#else
								MAX((parent_rect.height - initial_placement.height) / 2, 0),
#endif
								initial_placement.width, initial_placement.height);
	overlay_layout_widget->SetRect(initial_placement);
	overlay_layout_widget->SetXResizeEffect(RESIZE_CENTER);
#ifdef _MACINTOSH_
	overlay_layout_widget->SetYResizeEffect(RESIZE_FIXED);
#else
	overlay_layout_widget->SetYResizeEffect(RESIZE_CENTER);
#endif
}

/***********************************************************************************
**
**	OnDesktopWindowActivated
**
***********************************************************************************/

void Dialog::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active)
{
	if (desktop_window == m_parent_desktop_window)	//don't listen to other desktopwindows being activated
	{
		if (active)
		{
			ShowDialogInternal();
		}
		else if (GetIsWindowTool() || HideWhenDesktopWindowInActive())
		{
			//hide tool windows, they could overlap other tabs
			Show(FALSE);
		}

		// When a desktop window with an overlay is activated/deactivated, its
		// overlay should be activated/deactivated since that will be the active
		// input receiver. Not doing that can be a security issue [DSK-308012].
		if (GetOverlayed())
			Activate(active);
	}
}

/***********************************************************************************
**
**	OnDesktopWindowChanged
**
***********************************************************************************/

void Dialog::OnDesktopWindowChanged(DesktopWindow* desktop_window)
{
	if (desktop_window == this)
	{
		if (m_title_widget)
			m_title_widget->SetText(GetTitle());
	}
}

/***********************************************************************************
**
**	OnActivate
**
***********************************************************************************/

void Dialog::OnActivate(BOOL activate, BOOL first_time)
{
	if (activate && first_time)
	{
		SetFocus();
	}

	if (GetProtectAgainstDoubleClick())
	{
		m_protected_against_double_click = TRUE;

		if (activate)
		{
			if (!m_timer)
			{
				if (!(m_timer = OP_NEW(OpTimer, ())))
					return;

				m_timer->SetTimerListener(this);
			}
			m_timer->Start(400);
		}

		// make sure we disable OK button if needed
		g_input_manager->UpdateAllInputStates();
	}
}

/***********************************************************************************
**
**	GetProtectAgainstDoubleClick
**
***********************************************************************************/

BOOL Dialog::GetProtectAgainstDoubleClick()
{
	switch (GetDialogType())
	{
		case TYPE_OK:
		case TYPE_PROPERTIES:
		case TYPE_WIZARD:
		case TYPE_CLOSE:
		case TYPE_CANCEL:
			return FALSE;
		case TYPE_OK_CANCEL:
		case TYPE_YES_NO:
		case TYPE_YES_NO_CANCEL:
		case TYPE_NO_YES:
		default:
			return GetDialogImageByEnum() == IMAGE_WARNING;
	}
}


/***********************************************************************************
**
**	GetDialogImage
**
***********************************************************************************/

const char* Dialog::GetDialogImage()
{
	switch (GetDialogImageByEnum())
	{
		case IMAGE_WARNING: return "Dialog Warning"; break;
		case IMAGE_QUESTION: return "Dialog Question"; break;
		case IMAGE_ERROR: return "Dialog Error"; break;
		case IMAGE_INFO: return "Dialog Info"; break;
	}

	return GetWindowName();
}

/***********************************************************************************
**
**	GetOkTextID
**
***********************************************************************************/

Str::LocaleString Dialog::GetOkTextID()
{
	if (m_dialog_type == TYPE_YES_NO || m_dialog_type == TYPE_NO_YES || m_dialog_type == TYPE_YES_NO_CANCEL )
	{
		return Str::DI_IDYES;
	}
	else if (m_dialog_type == TYPE_CLOSE)
	{
		return Str::DI_IDBTN_CLOSE;
	}
	else
	{
		return Str::DI_ID_OK;
	}

}

Str::LocaleString Dialog::GetCancelTextID()
{
	if (m_dialog_type == TYPE_YES_NO || m_dialog_type == TYPE_NO_YES || m_dialog_type == TYPE_YES_NO_CANCEL )
	{
		return Str::DI_IDNO;
	}
	else
	{
		return Str::DI_ID_CANCEL;
	}
}


Str::LocaleString Dialog::GetYNCCancelTextID()
{
	return Str::DI_ID_CANCEL;
}

Str::LocaleString Dialog::GetHelpTextID()
{
	return Str::DI_IDHELP;
}

Str::LocaleString Dialog::GetDoNotShowAgainTextID()
{
	return Str::S_DO_NOT_SHOW_DIALOG_AGAIN;
}

OpInputAction* Dialog::GetYNCCancelAction() 
{
	OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CANCEL));
	if (action)
		action->SetActionData(YNC_CANCEL);
	return action;
}

OpInputAction* Dialog::GetHelpAction()
{
	OpString anchor;
	RETURN_VALUE_IF_ERROR(anchor.Set(GetHelpAnchor()), NULL);
	OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_HELP));
	if (action)
		action->SetActionDataString(anchor.CStr());
	return action;
}


/***********************************************************************************
**
**	GetButtonCount
**
***********************************************************************************/

INT32 Dialog::GetButtonCount()
{
	INT32 button_count = 0;

	switch (m_dialog_type)
	{
		case TYPE_WIZARD:
		case TYPE_YES_NO_CANCEL:
			button_count = 3;
			break;

		case TYPE_OK:
		case TYPE_CLOSE:
		case TYPE_CANCEL:
			button_count = 1;
			break;

		case TYPE_OK_CANCEL:
		case TYPE_YES_NO:
		case TYPE_NO_YES:
		case TYPE_PROPERTIES:
		default:
			button_count = 2;
			break;
	}

	const char* help = GetHelpAnchor();

	if (help && *help)
	{
		button_count++;
	}

	return button_count;
}

/***********************************************************************************
**
**	GetButtonInfo
**
***********************************************************************************/

void Dialog::GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name)
{
	switch (m_dialog_type)
	{
		case TYPE_WIZARD:
		{
			OpString prev, next;
			g_languageManager->GetString(Str::S_WIZARD_PREV_PAGE, prev);
			g_languageManager->GetString(Str::S_WIZARD_NEXT_PAGE, next);

			switch (index)
			{
				case 0: action = OP_NEW(OpInputAction, (OpInputAction::ACTION_BACK)); text.Set(prev); is_enabled = FALSE; name.Set(WIDGET_NAME_BUTTON_PREVIOUS); break;
				case 1: action = OP_NEW(OpInputAction, (OpInputAction::ACTION_FORWARD)); text.Set(next); is_default = TRUE; name.Set(WIDGET_NAME_BUTTON_NEXT); break;
				case 2: action = GetCancelAction(); text.Set(GetCancelText()); name.Set(WIDGET_NAME_BUTTON_CANCEL); break;
				case 3: action = GetHelpAction(); text.Set(GetHelpText()); name.Set(WIDGET_NAME_BUTTON_HELP); break;
			}
			break;
		}

		case TYPE_OK:
		case TYPE_CLOSE:
		if (index == 1)
		{
			index = 2;
		}
		// fall through

		case TYPE_CANCEL:
		case TYPE_OK_CANCEL:
		case TYPE_YES_NO:
		case TYPE_NO_YES:
		case TYPE_PROPERTIES:
		case TYPE_YES_NO_CANCEL:
		default:
		{
			if (m_dialog_type == TYPE_CANCEL && index == 0)
			{
				index = 1;
			}
			switch (index)
			{
				case 0:
					action = GetOkAction();
					text.Set(GetOkText());
					name.Set(WIDGET_NAME_BUTTON_OK);
					if (m_dialog_type != TYPE_NO_YES)
						is_default = TRUE;
				break;

				case 1:
					action = GetCancelAction();
					text.Set(GetCancelText());
					name.Set(WIDGET_NAME_BUTTON_CANCEL);
					if (m_dialog_type == TYPE_NO_YES)
						is_default = TRUE;
				break;

				case 2:
					if (m_dialog_type == TYPE_YES_NO_CANCEL)
					{
						action = GetYNCCancelAction();
						text.Set(GetYNCCancelText());
						name.Set(WIDGET_NAME_BUTTON_CANCEL);
					}
					else
					{
						action = GetHelpAction();
						text.Set(GetHelpText());
						name.Set(WIDGET_NAME_BUTTON_HELP);
					}
					break;
				case 3:	// if TYPE_YES_NO_CANCEL and has help anchor
					action = GetHelpAction();
					text.Set(GetHelpText());
					name.Set(WIDGET_NAME_BUTTON_HELP);
				break;
			}
			break;
		}
	}
}

/***********************************************************************************
**
**	SetFocus
**
***********************************************************************************/

void Dialog::SetFocus()
{
	if (m_dialog_type == TYPE_WIZARD)
	{
		m_current_page->RestoreFocus(FOCUS_REASON_OTHER);
	}
	else
	{
		// Set the focus to nearest widget, but Note that widget can also
		// be a button, so you always have a default button(if that button
		// doesn't happen to be *protected* when dialog shows up)
		GetWidgetContainer()->GetRoot()->RestoreFocus(FOCUS_REASON_KEYBOARD);
	}

	OnSetFocus();
}

/***********************************************************************************
**
**	ScaleWidget
**
***********************************************************************************/

void Dialog::ScaleWidget(OpWidget* widget)
{
	OpRect rect = widget->GetRect();

	rect.x = ScaleX(rect.x);
	rect.y = ScaleY(rect.y);

	// Do not scale icons in a dialog
	if (widget->GetType() != WIDGET_TYPE_ICON)
	{
		rect.width = ScaleX(rect.width);
		rect.height = ScaleY(rect.height);
	}

	widget->SetRect(rect, FALSE, FALSE);

	OpWidget* child_widget = (OpWidget*) widget->childs.First();

	while (child_widget)
	{
		if (!child_widget->IsInternalChild())
		{
			ScaleWidget(child_widget);
		}
		child_widget = (OpWidget*) child_widget->Suc();
	}
}

/***********************************************************************************
**
**	IsDoNotShowDialogAgainChecked
**
***********************************************************************************/

BOOL Dialog::IsDoNotShowDialogAgainChecked()
{
	return m_do_not_show_again ? m_do_not_show_again->GetValue() : FALSE;
}

/***********************************************************************************
**
**	SetDoNotShowDialogAgain
**
***********************************************************************************/

void Dialog::SetDoNotShowDialogAgain(BOOL checked)
{
	if (m_do_not_show_again) m_do_not_show_again->SetValue(checked);
}

/***********************************************************************************
**
**	IsPageChanged
**
***********************************************************************************/

BOOL Dialog::IsPageChanged(INT32 index)
{
	OpWidget* widget = GetPageByNumber(index);

	if (!widget)
		return FALSE;

	return ((INTPTR)widget->GetUserData() & PAGE_CHANGED) != 0;
}

/***********************************************************************************
**
**	IsPageInited
**
***********************************************************************************/

BOOL Dialog::IsPageInited(INT32 index)
{
	OpWidget* widget = GetPageByNumber(index);

	if (!widget)
		return FALSE;

	return ((INTPTR)widget->GetUserData() & PAGE_INITED) != 0;
}

/***********************************************************************************
**
**	InitPage
**
***********************************************************************************/

void Dialog::InitPage(INT32 index)
{
	OpWidget* widget = GetPageByNumber(index);

	if (!widget)
		return;

	BOOL first_time = !IsPageInited(index);

	INTPTR flags = (INTPTR) widget->GetUserData();

	if (first_time)
		flags &= ~PAGE_CHANGED;

	OnInitPage(index, first_time);
	widget->SetUserData((void*) (flags | PAGE_INITED));

	if (m_dialog_type == TYPE_WIZARD && IsLastPage(m_current_page_number))
		SetButtonText(1, m_finish_button_text.CStr());
}

/***********************************************************************************
**
**	GetPageByNumber
**
***********************************************************************************/

OpWidget* Dialog::GetPageByNumber(INT32 page_number) const
{
	return m_pages->Get(page_number);
}

/***********************************************************************************
**
**	GetPageCount
**
***********************************************************************************/

UINT32 Dialog::GetPageCount() const
{
	return m_pages->GetCount();
}

/***********************************************************************************
**
**	OkCloseDialog
**
***********************************************************************************/

void Dialog::OkCloseDialog(BOOL immediatly/* = FALSE*/, BOOL user_initiated/* = FALSE*/)
{
	m_is_ok = TRUE;
	CloseDialog(FALSE, immediatly, user_initiated);
}

/***********************************************************************************
**
**	WidgetChanged
**
***********************************************************************************/

void Dialog::WidgetChanged(OpWidget* widget)
{
	while (widget)
	{
		INT32 count = m_pages->GetCount();

		for (INT32 i = 0; i < count; i++)
		{
			if (m_pages->Get(i) == widget)
			{
				widget->SetUserData((void*) ((INTPTR) widget->GetUserData() | PAGE_CHANGED));
				return;
			}
		}
		widget = widget->GetParent();
	}
}

/***********************************************************************************
**
**	SetPageChanged
**
***********************************************************************************/

void Dialog::SetPageChanged(INT32 index, BOOL changed)
{
	OpWidget* widget = GetPageByNumber(index);

	if (!widget)
		return;

	INTPTR flags = (INTPTR) widget->GetUserData();

	if (changed)
		flags |= PAGE_CHANGED;
	else
		flags &= ~PAGE_CHANGED;

	widget->SetUserData((void*)flags);
}

/***********************************************************************************
**
**	UpdateProgress
**
***********************************************************************************/

void Dialog::UpdateProgress(UINT position, UINT total, const uni_char *label, BOOL show_only_label)
{
	OP_ASSERT(m_progress_indicator != 0);
	if (!m_progress_indicator)
		return;

	// Update the progress bar.
	m_progress_indicator->SetProgress(position, total);
	m_progress_indicator->SetShowOnlyLabel(show_only_label);

	// Set the label text for the progress information.
	if (label == 0)
	{
		// Create a text based on the position digit.
		OpString label_string;
		label_string.AppendFormat(UNI_L("%u"), position);
		m_progress_indicator->SetLabel(label_string.CStr(), TRUE);
	}
	else
		m_progress_indicator->SetLabel(label, TRUE);
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void Dialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_tabs)
	{
		SetCurrentPage((INTPTR) m_tabs->GetTabUserData(m_tabs->GetActiveTab()));
#if defined(_UNIX_DESKTOP_)
		// So that active tab is raised in PrintDialog.cpp (UNIX specific)
		// Trond: Any clue why this is required? We use a OpTreeView in that
		// dialog. No other tabbed dialog does that. A resize of the dialog
		// fixes the problem as well. [espen 2004-03-26]
		m_tabs->GenerateOnLayout(TRUE);
#endif
	}
	else if (widget == m_pages_view)
	{
		SetCurrentPage(m_pages_view->GetSelectedItem()->GetID());
	}

	WidgetChanged(widget);
}

/***********************************************************************************
**
**	OnClick
**
***********************************************************************************/

void Dialog::OnClick(OpWidget *widget, UINT32 id)
{
	int i = m_button_strip->GetButtonId(widget);
	if (i >= 0)
	{
		OnButtonClicked(i);
		return;
	}

	WidgetChanged(widget);
}

/***********************************************************************************
**
**	OnContextMenu
**
***********************************************************************************/

/*virtual*/ BOOL
Dialog::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	return HandleWidgetContextMenu(widget, menu_point);
}

/***********************************************************************************
**
**	SetCurrentPage
**
***********************************************************************************/

void Dialog::SetCurrentPage(INT32 page_number)
{
	if (m_current_page_number != page_number)
	{
		m_current_page->SetVisibility(FALSE);
		OpWidget* requested_page = GetPageByNumber(page_number);
		if(!requested_page)
		{
			OP_ASSERT(!"Illegal page number");
			// don't crash on illegal pages
			return;
		}
		m_current_page_number = page_number;
		m_current_page = requested_page;
		m_current_page->SetVisibility(TRUE);

		InitPage(m_current_page_number);

		if (!((m_pages_view && m_pages_view->IsFocused()) || (m_tabs && m_tabs->IsFocused())))
		{
			WidgetUtils::SetFocusInDialogPage(m_current_page);
		}

		if (m_tabs)
			m_tabs->SetSelected(m_current_page_number);
	}
}

/***********************************************************************************
**
**	DeletePage
**
***********************************************************************************/

void Dialog::DeletePage(OpWidget* page)
{
	if (OpStatus::IsSuccess(m_pages->RemoveByItem(page)))
		page->Delete();
}

/***********************************************************************************
**
**	SetWidgetText
**
***********************************************************************************/

void Dialog::SetWidgetText(const char* name, const uni_char* text)
{
	OpWidget* widget = GetWidgetByName(name);

	if (widget)
	{
		widget->SetText(text);
	}
}

void Dialog::SetWidgetText(const char* name, const char* text8)
{
	OpString text;
	text.Set(text8);
	SetWidgetText(name, text.CStr());
}

void Dialog::SetWidgetText(const char* name, Str::LocaleString string_id)
{
	OpString text;

	g_languageManager->GetString(string_id, text);
	SetWidgetText(name, text.CStr());
}

/***********************************************************************************
**
**	GetWidgetText
**
***********************************************************************************/

void Dialog::GetWidgetText(const char* name, OpString& text, BOOL empty_if_not_found)
{
	if (empty_if_not_found)
		text.Empty();

	OpWidget* widget = GetWidgetByName(name);

	if (widget)
	{
		widget->GetText(text);
	}
}

void Dialog::GetWidgetText(const char* name, OpString8& text8, BOOL empty_if_not_found)
{
	OpString text;
	text.Set(text8.CStr());

	GetWidgetText(name, text, empty_if_not_found);
	text8.Set(text.CStr());
}


OP_STATUS Dialog::GetWidgetTextUTF8(const char* name, OpString8& text8, BOOL empty_if_not_found)
{
	if (empty_if_not_found)
		text8.Empty();

	OpString text;

	OpWidget* widget = GetWidgetByName(name);
	if (widget)
		RETURN_IF_ERROR(widget->GetText(text));

	return text8.SetUTF8FromUTF16(text);
}


OP_STATUS Dialog::FormatWidgetText(const char* name,
		Str::LocaleString format_string_id, const OpStringC& value)
{
	OpWidget* widget = GetWidgetByName(name);
	OP_ASSERT( NULL != widget && "widget missing" );
	if (!widget)
		return OpStatus::ERR;
	OpString text;
	RETURN_IF_ERROR(StringUtils::GetFormattedLanguageString(text, format_string_id, value.CStr()));
	widget->SetText(text);
	return OpStatus::OK;
}
/***********************************************************************************
**
**	SetWidgetValue
**
***********************************************************************************/

void Dialog::SetWidgetValue(const char* name, INT32 value)
{
	OpWidget* widget = GetWidgetByName(name);

	if (widget)
	{
		widget->SetValue(value);
	}
}

/***********************************************************************************
**
**	GetWidgetValue
**
***********************************************************************************/

INT32 Dialog::GetWidgetValue(const char* name, INT32 default_value)
{
	OpWidget* widget = GetWidgetByName(name);

	if (widget)
	{
		return widget->GetValue();
	}

	return default_value;
}

/***********************************************************************************
**
**	SetWidgetEnabled
**
***********************************************************************************/

void Dialog::SetWidgetEnabled(const char* name, BOOL enabled)
{
	OpWidget* widget = GetWidgetByName(name);

	if (widget)
	{
		widget->SetEnabled(enabled);
	}
}

/***********************************************************************************
**
**	SetWidgetReadOnly
**
***********************************************************************************/

void Dialog::SetWidgetReadOnly(const char* name, BOOL read_only)
{
	OpWidget* widget = GetWidgetByName(name);

	if (widget)
	{
		widget->SetReadOnly(read_only);
	}
}

/***********************************************************************************
**
**	SetWidgetFocus
**
***********************************************************************************/

void Dialog::SetWidgetFocus(const char* name, BOOL focus)
{
	OpWidget* widget = GetWidgetByName(name);

	if (widget)
	{
		widget->SetFocus(FOCUS_REASON_OTHER);
	}
}

/***********************************************************************************
**
**	IsWidgetEnabled
**
***********************************************************************************/

BOOL Dialog::IsWidgetEnabled(const char* name)
{
	OpWidget* widget = GetWidgetByName(name);

	if (widget)
	{
		return widget->IsEnabled();
	}

	return FALSE;
}

/***********************************************************************************
**
**	ShowWidget
**
***********************************************************************************/

void Dialog::ShowWidget(const char* name, BOOL show)
{
	OpWidget* widget = GetWidgetByName(name);

	if (widget)
	{
		widget->SetVisibility(show);
	}
}

/***********************************************************************************
**
**	EnableBackButton
**
***********************************************************************************/

void Dialog::EnableBackButton(BOOL enable)
{
	m_back_enabled = enable;
	g_input_manager->UpdateAllInputStates();
}

/***********************************************************************************
**
**	EnableForwardButton
**
***********************************************************************************/

void Dialog::EnableForwardButton(BOOL enable)
{
	m_forward_enabled = enable;
	g_input_manager->UpdateAllInputStates();
}

/***********************************************************************************
**
**	EnableOkButton
**
***********************************************************************************/

void Dialog::EnableOkButton(BOOL enable)
{
	m_ok_enabled = enable;
	g_input_manager->UpdateAllInputStates();
}

/***********************************************************************************
**
**	SetForwardButtonText
**
***********************************************************************************/

void Dialog::SetForwardButtonText(const OpStringC& text)
{
	SetButtonText(1, text.CStr());
}

/***********************************************************************************
**
**	SetFinishButtonText
**
***********************************************************************************/

void Dialog::SetFinishButtonText(const uni_char *text)
{
	m_finish_button_text.Set(text);
}

/***********************************************************************************
**
**	CompressGroups
**
***********************************************************************************/

void Dialog::CompressGroups()
{
	// Don't do anything for wizard dialogs
	if (m_dialog_type == TYPE_WIZARD)
		return;

	OpWidget* pages_widget = GetPageByNumber(GetCurrentPage()); // current page shown
	OpWidget* group_widget = (OpWidget*)pages_widget->childs.First(); // the first group within the page (?)

	OpRect	rect;
	BOOL	first_group = TRUE;
	int		bottom = 0;

	// Loop the controls at this level as they should all be groups
	while (group_widget)
	{
		// For each visible group grab it's size and then adjust everything
		// below it so that they all come one after the other with no
		// gaps.
		if (group_widget->GetType() == OpTypedObject::WIDGET_TYPE_GROUP)
		{
			rect = group_widget->GetOriginalRect();

			// Get the y and height of the first group to start us off
			if (first_group)
			{
				// If the first group is hidden then the bottom will be 0 as it was initalised
				if (group_widget->IsVisible())
					bottom = rect.y + rect.height;

				first_group = FALSE;
			}
			else
			{
				// Skip over invisible groups
				if (group_widget->IsVisible())
				{
					// Change the y of the rect to sit at the current bottom coordinate
					rect.y = bottom;

					// Set the size in the group's original rect
					group_widget->SetOriginalRect(rect);

					// Add the height of this group to the overall bottom coordinate
					bottom += rect.height;
				}
			}
		}
		group_widget = (OpWidget*)group_widget->Suc();
	}

	// Reset the size of the overall page based on what happened above in the sizing
	rect = pages_widget->GetOriginalRect();
	rect.height = bottom;
	pages_widget->SetOriginalRect(rect);

	// Force the dialog to recalculate itself
	ResetDialog();
}

/***********************************************************************************
**
**	OnClose
**
***********************************************************************************/

void Dialog::OnClose(BOOL user_initiated)
{
	if (m_overlay_layout_widget)
	{
		m_overlay_layout_widget->Delete();
		m_overlay_layout_widget = NULL;
	}

#ifdef VEGA_OPPAINTER_SUPPORT
	if (GetOverlayed() && m_parent_desktop_window && m_parent_browser_view && !m_parent_desktop_window->IsClosing())
	{
		// Add animation of the dialog disappearing...
		QuickAnimationBitmapWindow *anim_win = OP_NEW(QuickAnimationBitmapWindow, ());
		if (OpStatus::IsError(anim_win->Init(m_parent_desktop_window, GetOpWindow(), GetWidgetContainer()->GetRoot())))
			OP_DELETE(anim_win);
		else
		{
			INT32 x, y;
			UINT32 w, h;
			anim_win->GetInnerPos(x, y);
			anim_win->GetInnerSize(w, h);
			if (GetPosAnimationType() != POSITIONAL_ANIMATION_SHEET)
				anim_win->animateOpacity(((MDE_OpWindow*)GetOpWindow())->GetTransparency() / 255.0, 0);
			anim_win->animateIgnoreMouseInput(TRUE, TRUE);
			int y_offset = 0;
			AnimationCurve anim_curve = ANIM_CURVE_SPEED_UP;
			
			switch (GetPosAnimationType())
			{
				case POSITIONAL_ANIMATION_STANDARD:
					y_offset = h / 4;
					break;
				case POSITIONAL_ANIMATION_SHEET:
					y_offset = - 2 * h;
					anim_curve = ANIM_CURVE_LINEAR;
					break;
				default:
					break;
			}
			
			anim_win->animateRect(OpRect(x, y, w, h), OpRect(x, y + y_offset, w, h));
			g_animation_manager->startAnimation(anim_win, anim_curve);
		}
	}
#endif // VEGA_OPPAINTER_SUPPORT

	if (m_parent_browser_view)
	{
		if (GetDimPage())
			m_parent_browser_view->RequestDimmed(false);

		int overlay = 0;	// count of overlayed dialogs(except this)

		DesktopWindow* parent = m_parent_browser_view->GetParentDesktopWindow();
		if (parent)
		{
			for (INT32 i=0; i<parent->GetDialogCount(); i++)
			{
				Dialog* dialog = parent->GetDialog(i);
				if ( dialog != this)
				{
					if (dialog->GetOverlayed())
						overlay ++;
				}
			}
		}

		if (GetOverlayed() && overlay == 0)
		{
			// Set focus back to page since overlayed dialogs doesn't do that automatically.
			m_parent_browser_view->RestoreFocus(FOCUS_REASON_OTHER);
		}
	}

	if (m_is_ok)
	{
		UINT32 result = OnOk();

		if (m_listener)
		{
			m_listener->OnOk(this, result);
		}
	}
	else if (m_dialog_type == TYPE_YES_NO_CANCEL && m_is_ync_cancel)
	{
		OnYNCCancel();

		if (m_listener)
		{
			m_listener->OnYNCCancel(this);
		}
	}
	else if (m_call_cancel)
	{
		OnCancel();

		if (m_listener)
		{
			m_listener->OnCancel(this);
		}
	}

	if (m_listener)
	{
		m_listener->OnClose(this);
	}
}


/***********************************************************************************
**
**	OnRelayout
**
***********************************************************************************/

void Dialog::OnRelayout(OpWidget* widget)
{
	if (widget != GetWidgetContainer()->GetRoot())
		return;

	DesktopWindow::OnRelayout(widget);
}

/***********************************************************************************
**
**	OnRelayout
**
***********************************************************************************/

void Dialog::OnRelayout()
{
	ResetDialog();
}

/***********************************************************************************
**
**	OnShow
**
***********************************************************************************/

void Dialog::OnShow(BOOL show)
{
	if (show)
		m_has_been_shown = TRUE;

	DesktopWindow::OnShow(show);
}

void Dialog::UpdateLanguage()
{
	DesktopWindow::UpdateLanguage();
	ResetDialog();
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL Dialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_FORWARD:
					child_action->SetEnabled(m_forward_enabled);
					return TRUE;

				case OpInputAction::ACTION_BACK:
					child_action->SetEnabled(m_back_history.GetCount() > 0 && m_back_enabled);
					return TRUE;

				case OpInputAction::ACTION_SHOW_HELP:
					child_action->SetEnabled(TRUE);
					return TRUE;

				case OpInputAction::ACTION_SET_VISIBILITY:
				{
					OpString8 name;
					name.Set(child_action->GetActionDataString());

					OpWidget* widget = GetWidgetByName(name.CStr());

					if (!widget)
						return FALSE;

					child_action->SetEnabled(child_action->GetActionData() != widget->IsVisible());
					return TRUE;
				}

				case OpInputAction::ACTION_SWITCH_GROUPS:
				{
					OpString8 group_open, group_closed;

					// Get the two group names
					GetGroupsToSwitch(child_action->GetActionDataString(), group_open, group_closed);

					OpGroup *open_group = (OpGroup *)GetWidgetByName(group_open);
					if (open_group)
					{
						child_action->SetSelected(open_group->IsVisible());
						return TRUE;
					}
					break;
				}

				case OpInputAction::ACTION_OK: // Fall through to default
				default:
				{
					BOOL override = GetProtectAgainstDoubleClick();

					DesktopWindow* parent = GetParentDesktopWindow();
					if (parent)
					{
						override = override || parent->GetProtectAgainstDoubleClick();
					}

					if (override)
					{
						// protect against double-clicking on a page
						child_action->SetEnabled(!m_protected_against_double_click);
						return TRUE;
					}
					// ACTION_OK is has no default fallback handling anywhere, so do it here
					else if (child_action->GetAction() == OpInputAction::ACTION_OK)
					{
						child_action->SetEnabled(m_ok_enabled);
						return TRUE;
					}
					break; // otherwise fall through
				}
			}
			break;
		}

		case OpInputAction::ACTION_SET_VISIBILITY:
		{
			OpString8 name;
			name.Set(action->GetActionDataString());

			OpWidget* widget = GetWidgetByName(name.CStr());

			if (!widget || widget->IsVisible() == action->GetActionData())
				return FALSE;

			widget->SetVisibility(action->GetActionData());
			Relayout();
			return TRUE;
		}

		case OpInputAction::ACTION_FORWARD:
		{
			if (!m_forward_enabled)
				return FALSE;

			if (m_dialog_type == TYPE_WIZARD)
			{
				if (OnValidatePage(m_current_page_number))
				{
					if (!IsLastPage(m_current_page_number))
					{
						INT32* current_page_number = OP_NEW(INT32, ());

						if (current_page_number)
						{
							*current_page_number = m_current_page_number;

							m_back_history.Add(current_page_number);
						}


						INT32 next_page_number = !action->GetActionData() ? OnForward(m_current_page_number) : action->GetActionData();

						m_current_page->SetVisibility(FALSE);
						m_current_page_number = next_page_number;
						m_current_page = GetPageByNumber(m_current_page_number);
						m_current_page->SetVisibility(TRUE);

						InitPage(m_current_page_number);

						if (IsLastPage(m_current_page_number))
							SetButtonText(1, m_finish_button_text.CStr());

						SetFocus();
						
						BroadcastDesktopWindowPageChanged();
					}
					else
					{
						m_is_ok = TRUE;
						Close(/*GetType()==DIALOG_TYPE_SIMPLE*/);
					}
				}
			}

			return TRUE;
		}

		case OpInputAction::ACTION_BACK:
		{
			if (!m_back_enabled)
				return FALSE;

			if (m_dialog_type == TYPE_WIZARD)
			{
				UINT32 history_count = m_back_history.GetCount();

				if (history_count > 0)
				{
					INT32* next_page_number = m_back_history.Remove(history_count - 1);

					OpString next;
					g_languageManager->GetString(Str::S_WIZARD_NEXT_PAGE, next);

					SetButtonText(1, next.CStr());

					m_current_page->SetVisibility(FALSE);
					m_current_page_number = *next_page_number;
					m_current_page = GetPageByNumber(m_current_page_number);
					m_current_page->SetVisibility(TRUE);

					InitPage(m_current_page_number);
					SetFocus();

					OP_DELETE(next_page_number);

					BroadcastDesktopWindowPageChanged();
				}
			}

			return TRUE;
		}
		case OpInputAction::ACTION_OK:
		{
			m_is_ok = TRUE;
			Close(FALSE, TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_CANCEL:
		{
			if (action->GetActionData() == YNC_CANCEL)
			{
				m_is_ync_cancel = TRUE;
			}
			// This is the no button for TYPE_YES_NO_CANCEL and TYPE_YES_NO and TYPE_NO_YES types
			Close(FALSE, TRUE);
			return TRUE;
		}
		case OpInputAction::ACTION_SHOW_HELP:
		{
			if (!action->GetActionDataString())
			{
				OpString help_anchor;
				help_anchor.Set(GetHelpAnchor());
				action->SetActionDataString(help_anchor.CStr());
			}
			break;
		}
		case OpInputAction::ACTION_CYCLE_TO_NEXT_PAGE:
		{
			if (m_pages_view)
			{
				m_pages_view->SelectNext(TRUE);
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_CYCLE_TO_PREVIOUS_PAGE:
		{
			if (m_pages_view)
			{
				m_pages_view->SelectNext(FALSE);
				return TRUE;
			}
			return FALSE;
		}
		case OpInputAction::ACTION_SWITCH_GROUPS:
		{
			OpString8 group_open, group_closed;

			// Get the two group names
			GetGroupsToSwitch(action->GetActionDataString(), group_open, group_closed);

			// Toggle each group's visibility
			OpGroup *open_group = (OpGroup *)GetWidgetByName(group_open);
			if (open_group)
				open_group->SetVisibility(!open_group->IsVisible());

			OpGroup *closed_group = (OpGroup *)GetWidgetByName(group_closed);
			if (closed_group)
				closed_group->SetVisibility(!closed_group->IsVisible());

			// Now we need to go through all of the groups in the dialog and
			// adjust them so all the visible groups are adjcent to each other
			CompressGroups();

			return TRUE;
		}
	}

	return DesktopWindow::OnInputAction(action);
}

////////////////////////////////////////////////////////////////////////////////////////

void Dialog::GetGroupsToSwitch(const OpStringC &action_string, OpString8 &group_open, OpString8 &group_closed)
{
	// Get the two group names

	// Copy in the group and then try to find the :
	group_open.Set(action_string.CStr());
	int index = group_open.Find(":");

	// if there is no : we have only the open group
	if (index > 0)
	{
		// Split out the group names
		group_closed.Set(group_open.CStr());
		group_closed.Delete(0, index + 1);
		group_open.Delete(index);
	}
	else
	{
		// This has no closed group
		group_closed.Empty();
	}
}

/***********************************************************************************
**
**	OnTimeOut
**
***********************************************************************************/

void Dialog::OnTimeOut(OpTimer* timer)
{
	if (timer == m_timer &&	IsActive() && GetProtectAgainstDoubleClick()) // don't allow click-through from window above
	{
		m_protected_against_double_click = FALSE;
		g_input_manager->UpdateAllInputStates();
		Relayout(); // seems to be needed to update buttons
	}
}

void Dialog::SetLabelInBold(const char* label_name)
{
	OP_ASSERT(NULL != label_name);

	OpWidget* label = GetWidgetByName(label_name);
	OP_ASSERT(NULL != label);
	if (NULL != label)
	{
		switch (label->GetType())
		{
		case WIDGET_TYPE_LABEL:
			static_cast<OpLabel*>(label)->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
			break;

		case WIDGET_TYPE_MULTILINE_LABEL:
		case WIDGET_TYPE_MULTILINE_EDIT:
			label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
			break;

		default:
			OP_ASSERT(!"Bad widget type");
		}
	}
}

void Dialog::FocusButton(INT32 button)
{
	m_button_strip->FocusButton(button);
}

void Dialog::EnableButton(INT32 button, BOOL enable)
{
	m_button_strip->EnableButton(button, enable);
}

void Dialog::ShowButton(INT32 button, BOOL show)
{
	m_button_strip->ShowButton(button, show);
}

void Dialog::SetButtonText(INT32 button, const uni_char* text)
{
	m_button_strip->SetButtonText(button, text);
}

void Dialog::SetButtonAction(INT32 button, OpInputAction* action)
{
	m_button_strip->SetButtonAction(button, action);
}

void Dialog::SetDefaultPushButton(INT32 index)
{
	m_button_strip->FocusButton(index);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG

void ListControls(OpWidget *parent, const char *curr_gap)
{
	OpWidget* child_widget = (OpWidget*) parent->childs.First();

	OpString8 gap;

	gap.Set(curr_gap);

	while (child_widget)
	{
		if (!child_widget->IsInternalChild())
		{
			OpString8 name;
			name.Set(child_widget->GetName());
			printf("%sControl: %s Type: %d\n", gap.CStr(), name.CStr(), child_widget->GetType());

			gap.Append("   ");
			ListControls(child_widget, gap.CStr());
			gap.Delete(gap.Length() - 3);
		}
		child_widget = (OpWidget*) child_widget->Suc();
	}
}

#endif // _DEBUG


