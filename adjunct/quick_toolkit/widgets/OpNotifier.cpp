/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpNotifier.h"

#include "adjunct/desktop_pi/desktop_notifier.h"
#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/managers/FindTextManager.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/DesktopWidgetWindow.h"
#include "adjunct/quick_toolkit/widgets/OpHoverButton.h"
#include "modules/display/FontAtt.h"
#include "modules/display/styl_man.h"
#include "modules/pi/OpDragManager.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpWindow.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/style/css.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/WidgetContainer.h"

// == OpNotifier =============================================

OpNotifier::OpNotifier()
	: m_counter(0)
	, m_edit(NULL)
	, m_longtext(NULL)
	, m_image_button(NULL)
	, m_timer(NULL)
	, m_action(0)
	, m_cancel_action(0)
	, m_auto_close(FALSE)
	, m_listener(0)
	, m_wrapping(FALSE)
{
	g_notification_manager->AddNotifier(this);
}

OpNotifier::~OpNotifier()
{
	g_notification_manager->RemoveNotifier(this);

	if (!m_image.IsEmpty())
	{
		m_image.DecVisible(null_image_listener);
	}

	if (m_listener)
	{
		m_listener->OnNotifierDeleted(this);
	}

	OP_DELETE(m_timer);
	OP_DELETE(m_action);
	OP_DELETE(m_cancel_action);
}

OP_STATUS OpNotifier::Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& text, 
							const OpStringC8& image, OpInputAction* action,
							BOOL managed, BOOL wrapping)
{
	m_auto_close = TRUE;
	m_managed = managed;
	m_wrapping = wrapping;
	
	RETURN_IF_ERROR(DesktopWidgetWindow::Init(OpWindow::STYLE_NOTIFIER, "Notifier", NULL));
	SetParentInputContext(g_application->GetInputContext());

	RETURN_IF_ERROR(OpMultilineEdit::Construct(&m_edit));

	RETURN_IF_ERROR(OpButton::Construct(&m_image_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	m_image_button->GetForegroundSkin()->SetImage(image.CStr());
	m_image_button->SetDead(TRUE);

	RETURN_IF_ERROR(OpButton::Construct(&m_close_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	m_close_button->GetForegroundSkin()->SetImage("Notifier Close Button Skin");
	m_close_button->SetVisibility(TRUE);

	m_timer = OP_NEW(OpTimer, ());
	RETURN_OOM_IF_NULL(m_timer);

	m_action = action;
	m_timer->SetTimerListener(this);

	OpWidget* root = m_widget_container->GetRoot();
	root->SetRTL(UiDirection::Get() == UiDirection::RTL);
	root->GetBorderSkin()->SetImage("Notifier Skin", "Edit Skin");
	root->SetHasCssBorder(FALSE);

	m_edit->SetWrapping(FALSE);
	m_edit->SetFlatMode();
	m_edit->UpdateSkinPadding(); // So that Layout gets correct padding values the first time
//	m_edit->SetPosInDocument(1, 1);
//	m_edit->font_info.justify = JUSTIFY_CENTER;
	m_edit->font_info.weight = 5;

	root->AddChild(m_image_button);
	root->AddChild(m_close_button);
	root->AddChild(m_edit);
	root->SetListener(this);

/*	FontAtt font;

	g_op_ui_info->GetUICSSFont(CSS_VALUE_status_bar, font);

	const OpFontInfo *font_info = styleManager->GetFontInfo(font.GetFontNumber());

	if (font_info)
	{
		m_edit->SetFontInfo(font_info, font.GetSize(), font.GetItalic(), font.GetWeight(), JUSTIFY_LEFT);
	}

	m_edit->SetBackgroundColor(g_op_ui_info->GetUICSSColor(CSS_VALUE_InfoBackground));
	m_edit->SetForegroundColor(g_op_ui_info->GetUICSSColor(CSS_VALUE_InfoText));
*/

	if (!IsAnimated())
		m_counter = 51;

	SetText(text);

	if (!managed)
		StartShow();

	return OpStatus::OK;
}

OP_STATUS OpNotifier::Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& title, Image& image, 
							const OpStringC& text, OpInputAction* action, OpInputAction* cancel_action,
							BOOL managed, BOOL wrapping)
{
	m_auto_close = TRUE;
	m_managed = managed;
	m_wrapping = wrapping;

	RETURN_IF_ERROR(DesktopWidgetWindow::Init(OpWindow::STYLE_NOTIFIER, "Notifier", NULL));
	SetParentInputContext(g_application->GetInputContext());

	RETURN_IF_ERROR(OpMultilineEdit::Construct(&m_edit));

	RETURN_IF_ERROR(OpButton::Construct(&m_image_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	if (!image.IsEmpty())
	{
		m_image = image;
		m_image.IncVisible(null_image_listener);
	}
	m_image_button->GetForegroundSkin()->SetBitmapImage(image);
	m_image_button->SetDead(TRUE);

	RETURN_IF_ERROR(OpButton::Construct(&m_close_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	m_close_button->GetForegroundSkin()->SetImage("Notifier Close Button Skin");
	m_close_button->SetVisibility(TRUE);

	m_timer = OP_NEW(OpTimer, ());
	RETURN_OOM_IF_NULL(m_timer);

	m_action = action;
	m_cancel_action = cancel_action;
	m_timer->SetTimerListener(this);

	OpWidget* root = m_widget_container->GetRoot();
	root->SetRTL(UiDirection::Get() == UiDirection::RTL);
	root->GetBorderSkin()->SetImage("Notifier Skin", "Edit Skin");
	root->SetHasCssBorder(FALSE);

	m_edit->SetWrapping(FALSE);
	m_edit->SetLabelMode();
	//m_edit->SetFlatMode();
	m_edit->UpdateSkinPadding(); // So that Layout gets correct padding values the first time
	m_edit->font_info.weight = 5;

	root->AddChild(m_image_button);
	root->AddChild(m_close_button);
	root->AddChild(m_edit);
	root->SetListener(this);

	switch (notification_group)
	{
	case EXTENSION_NOTIFICATION:
	case AUTOUPDATE_NOTIFICATION:
	case AUTOUPDATE_NOTIFICATION_EXTENSIONS:
	case PLUGIN_DOWNLOADED_NOTIFICATION:
		{
			RETURN_IF_ERROR(OpMultilineEdit::Construct(&m_longtext));
			root->AddChild(m_longtext);
			m_longtext->SetWrapping(TRUE);
			m_longtext->SetLabelMode();
			m_longtext->UpdateSkinPadding(); // So that Layout gets correct padding values the first time
			m_longtext->SetText(text.CStr());
		}
		break;
	default:
		{
			OpHoverButton* button;
			// Construct the button
			RETURN_IF_ERROR(OpHoverButton::Construct(&button, OP_RGB(0x00, 0x00, 0xff)));
			root->AddChild(button);
			RETURN_IF_ERROR(m_buttons.Add(button));
			// Setup the button
			RETURN_IF_ERROR(button->SetText(text.CStr()));
			button->SetEllipsis(ELLIPSIS_CENTER);
			button->SetJustify(root->GetRTL() ? JUSTIFY_RIGHT : JUSTIFY_LEFT, FALSE);
		}
		break;
	}

	if (!IsAnimated())
		m_counter = 51;

	SetText(title);

	if (!managed)
		StartShow();

	return OpStatus::OK;
}

void OpNotifier::StartShow()
{
	Layout();
	m_timer->Start(20);
}

void OpNotifier::Layout()
{
	OpRect close_button_rect;
	OpRect image_rect;
	OpRect edit_rect;

	m_close_button->GetRequiredSize(close_button_rect.width, close_button_rect.height);

	if (m_image_button->GetForegroundSkin()->HasContent())
	{
		m_image_button->GetRequiredSize(image_rect.width, image_rect.height);
	}

	edit_rect.width = WIDTH;
	edit_rect.height = m_edit->GetContentHeight() + m_edit->GetPaddingTop() + m_edit->GetPaddingBottom();

	m_edit->SetSize(edit_rect.width, edit_rect.height);
	edit_rect.height = m_edit->GetContentHeight() + m_edit->GetPaddingTop() + m_edit->GetPaddingBottom();

	INT32 padding_left, padding_top, padding_right, padding_bottom, spacing;

	OpWidget* root = GetWidgetContainer()->GetRoot();
	root->GetPadding(&padding_left, &padding_top, &padding_right, &padding_bottom);
	root->GetSpacing(&spacing);

	image_rect.x = padding_left;
	image_rect.y = padding_top;

	edit_rect.x = padding_left;
	edit_rect.y = padding_top;

	if (image_rect.width)
	{
		edit_rect.x += image_rect.width + spacing;
	}

	close_button_rect.x = edit_rect.x + edit_rect.width;
	close_button_rect.y = edit_rect.y;

	m_image_button->SetVisibility(image_rect.width != 0);
	root->SetChildRect(m_image_button, image_rect);
	root->SetChildRect(m_edit, edit_rect);
	root->SetChildRect(m_close_button, close_button_rect);

	INT32 button_height = 0;

	if (m_longtext)
	{
		button_height += 10;
		OpRect text_rect;
		text_rect.width  = edit_rect.width;
		text_rect.height = m_longtext->GetContentHeight() + m_longtext->GetPaddingTop() + m_longtext->GetPaddingBottom();
		m_longtext->SetSize(text_rect.width, text_rect.height);

		text_rect.x      = edit_rect.x;
		text_rect.y      = edit_rect.y + edit_rect.height + button_height;
		text_rect.height = m_longtext->GetContentHeight() + m_longtext->GetPaddingTop() + m_longtext->GetPaddingBottom();

		root->SetChildRect(m_longtext, text_rect);
		button_height += text_rect.height;
	}

	// position extra buttons
	button_height += m_buttons.GetCount() ? 10 : 0;

	for (unsigned i = 0; i < m_buttons.GetCount(); i++)
	{
		OpHoverButton* button		= m_buttons.Get(i);
		OpRect		   button_rect;

		button->GetPreferedSize(&button_rect.width, &button_rect.height, 1, 1);
		button_rect.x		= edit_rect.x;
		button_rect.y		= edit_rect.y + edit_rect.height + button_height,
		button_rect.width	= edit_rect.width + close_button_rect.width;

		root->SetChildRect(button, button_rect);
		button_height += button_rect.height;
	}

	// Compute notifier position on the screen.

	m_full_rect.width = edit_rect.x + edit_rect.width + close_button_rect.width + padding_right;
	m_full_rect.height = MAX(image_rect.height, edit_rect.height + button_height) + padding_top + padding_bottom;

	// Obtain the screen properties, prefer the primary screen.
	OpPoint screen_pos;
	OpScreenProperties screen_props;
	BrowserDesktopWindow* browser_window = g_application->GetActiveBrowserDesktopWindow();
	if (browser_window == NULL || browser_window->IsMinimized() || !g_application->IsShowingOpera())
	{
		GetWidgetContainer()->GetView()->GetMousePos(&screen_pos.x, &screen_pos.y);
		screen_pos = GetWidgetContainer()->GetView()->ConvertToScreen(screen_pos);
		g_op_screen_info->GetProperties(&screen_props, GetWindow(), &screen_pos);
	}
	else
	{
		OpRect browser_rect;
		browser_window->GetRect(browser_rect);
		screen_pos = browser_rect.Center();
		g_op_screen_info->GetProperties(&screen_props, browser_window->GetOpWindow(), &screen_pos);
	}

	const OpRect& workspace = screen_props.workspace_rect;
	const OpRect& full_screen = screen_props.screen_rect;

	// If we think there is a task bar on the left or right side of the
	// screen, show the notifier there.  Otherwise, choose the side
	// based on the UI direction.
	bool left_aligned = UiDirection::Get() == UiDirection::RTL;
	if (workspace.Left() > full_screen.Left())
		left_aligned = true;
	else if (workspace.Right() < full_screen.Right())
		left_aligned = false;

	m_full_rect.x = left_aligned ? workspace.Left() : workspace.Right() - m_full_rect.width;
	m_full_rect.y = workspace.Bottom() - m_full_rect.height;
}

void OpNotifier::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == m_close_button)
	{
		if(!down)
		{
			if (m_cancel_action)
				g_input_manager->InvokeAction(m_cancel_action);
			Close();
		}
		return;
	}

	if (!g_application)
		return;

	if (widget->GetAction()) 
	{
		g_input_manager->InvokeAction(widget->GetAction());
		Close();
		return;
	}

	if (!m_action) 
		return;

	// invoke action could mean showing a menu that blocks the call, so we
	// need to do this..

	BOOL close_when_done = m_auto_close;
	OpInputAction* action = m_action;

	m_auto_close = FALSE;

	if (close_when_done) 
		m_action = NULL;

	g_input_manager->InvokeAction(action); 

	if (close_when_done)
	{
		OP_DELETE(action);
		Close();
	}
}

void OpNotifier::OnMouseMove(OpWidget *widget, const OpPoint &point)
{
	// When the mouse move, we make this notifier visible for a longer time
	if(m_counter > 50)
	{
		m_counter = 51;
	}
}

void OpNotifier::SetText(const OpStringC& text)
{
	m_edit->SetWrapping(m_wrapping);
	m_edit->SetText(text.CStr());

	Layout();
}


/***********************************************************************************
 ** Adds a button to the notifier below the title
 **
 ** OpNotifier::AddButton
 ** @param text_left Left-justified text, will be cut off if too wide
 ** @param text_right Right-justified text, will not be cut off
 ** @param action Action to execute when this button is clicked
 ***********************************************************************************/
OP_STATUS OpNotifier::AddButton(const OpStringC& text, OpInputAction* action)
{
	OpWidget* root = m_widget_container->GetRoot();
	OpHoverButton* button;

	// Construct the button
	RETURN_IF_ERROR(OpHoverButton::Construct(&button, OP_RGB(0x00, 0x00, 0xff)));
	root->AddChild(button);
	RETURN_IF_ERROR(m_buttons.Add(button));

	// Setup the button
	RETURN_IF_ERROR(button->SetText(text.CStr()));
	button->SetEllipsis(ELLIPSIS_CENTER);
	button->SetJustify(root->GetRTL() ? JUSTIFY_RIGHT : JUSTIFY_LEFT, FALSE);
	button->SetAction(action);

	Layout();

	return OpStatus::OK;
}

bool OpNotifier::IsAnimated()
{
	return g_pccore->GetIntegerPref(PrefsCollectionCore::SpecialEffects) != 0;
}

void OpNotifier::Animate()
{
	INT32 counter = -1;

	if (m_counter <= 50)
	{
		counter = m_counter;
	}
	else if (m_counter >= DISAPPEAR_DELAY)
	{
		counter = DISAPPEAR_DELAY + 50 - m_counter;
		if (!counter || !IsAnimated())
		{
			Close();
			return;
		}
	}

	if (!IsAnimated())
	{
		Show(TRUE, &m_full_rect);
	}
	else if (counter >= 0)
	{
		OpRect notifier_rect = m_full_rect;
		notifier_rect.height = m_full_rect.height * counter / 50;
		notifier_rect.y = m_full_rect.Bottom() - notifier_rect.height;

		Show(TRUE, &notifier_rect);
	}

	if (m_counter < 199 || m_auto_close)
	{
		m_counter++;
		m_timer->Start(20);
	}
}

/***********************************************************************************
**
**  IME support
**
***********************************************************************************/

#ifdef WIDGETS_IME_SUPPORT
/*
IM_WIDGETINFO OpNotifier::OnStartComposing(OpInputMethodString* imstring, IM_COMPOSE compose)
{
	m_edit->SetReadOnly(FALSE);
	m_edit->SetCaretOffset(m_input_offset);
	return m_edit->OnStartComposing(imstring, compose);
}

IM_WIDGETINFO OpNotifier::OnCompose()
{
	IM_WIDGETINFO result = m_edit->OnCompose();
	UpdateNotifier(FALSE, TRUE);
	return result;
}

IM_WIDGETINFO OpNotifier::GetWidgetInfo()
{
	return m_edit->GetWidgetInfo();
}

void OpNotifier::OnCommitResult()
{
	m_edit->OnCommitResult();
	UpdateNotifier(FALSE, TRUE);
}

void OpNotifier::OnStopComposing(BOOL cancel)
{
	m_edit->SetReadOnly(TRUE);
	m_edit->OnStopComposing(cancel);
	UpdateNotifier(FALSE, TRUE);
}
*/
#endif // WIDGETS_IME_SUPPORT
