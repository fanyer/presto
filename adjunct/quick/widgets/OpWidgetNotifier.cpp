/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author mzdun
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpWidgetNotifier.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/animation/QuickAnimation.h"
#include "adjunct/quick/managers/NotificationManager.h"
#include "adjunct/quick_toolkit/widgets/OpToolTip.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpButton.h"

namespace
{
	enum
	{
		HOVER_TIME = 5000, // in ms, time of the popup on-screen
		TEXT_WIDTH = 300,
	};
}

OpWidgetNotifier::OpWidgetNotifier(OpWidget* widget)
	: m_anchor(widget)
	, m_edit(NULL)
	, m_close_button(NULL)
	, m_longtext(NULL)
	, m_image_button(NULL)
	, m_timer(NULL)
	, m_action(NULL)
	, m_cancel_action(NULL)
	, m_listener(0)
	, m_wrapping(FALSE)
	, m_contents_width(1)
	, m_contents_height(1)
	, m_animate(true)	
{
	g_notification_manager->AddNotifier(this);
}

OpWidgetNotifier::~OpWidgetNotifier()
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

void OpWidgetNotifier::OnAnimationComplete()
{
	OpOverlayWindow::OnAnimationComplete();
	m_timer->Start(HOVER_TIME);
}

void OpWidgetNotifier::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
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

	OpInputAction* action = m_action;
	m_action = NULL;

	g_input_manager->InvokeAction(action); 

	OP_DELETE(action);
	Close();
}

void OpWidgetNotifier::OnMouseMove(OpWidget *widget, const OpPoint &point)
{
	if (m_timer->IsStarted())
	{
		m_timer->Stop();
		m_timer->Start(HOVER_TIME);
	}

	QuickAnimationWindow::OnMouseMove(widget, point);
}

void OpWidgetNotifier::OnTimeOut(OpTimer* timer)
{
	if (timer == m_timer)
	{
		m_timer->Stop();
		Close();
	}
}

OP_STATUS OpWidgetNotifier::InitContent(const OpStringC& title, const OpStringC& text)
{
	OpWindow* parent_op_window = NULL;
	DesktopWindow* parent_desktop_window = m_anchor->GetParentDesktopWindow();
	OP_ASSERT(parent_desktop_window);

	if (!parent_desktop_window->GetToplevelDesktopWindow()->SupportsExternalCompositing())
		parent_op_window = parent_desktop_window->GetToplevelDesktopWindow()->GetOpWindow();

	RETURN_IF_ERROR(QuickAnimationWindow::Init(parent_desktop_window, parent_op_window, OpWindow::STYLE_NOTIFIER, OpWindow::EFFECT_TRANSPARENT));

	SetSkin("Widget Notifier Skin");

	RETURN_IF_ERROR(OpMultilineEdit::Construct(&m_edit));

	RETURN_IF_ERROR(OpButton::Construct(&m_image_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	if (!m_image.IsEmpty())
	{
		m_image.IncVisible(null_image_listener);
	}
	m_image_button->GetForegroundSkin()->SetBitmapImage(m_image);
	m_image_button->SetDead(TRUE);

	RETURN_IF_ERROR(OpButton::Construct(&m_close_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE));
	m_close_button->GetForegroundSkin()->SetImage("Notifier Close Button Skin");
	m_close_button->SetVisibility(TRUE);

	m_timer = OP_NEW(OpTimer, ());
	RETURN_OOM_IF_NULL(m_timer);

	m_timer->SetTimerListener(this);

	m_edit->SetWrapping(FALSE);
	m_edit->SetLabelMode();
	m_edit->font_info.weight = 5;

	OpWidget* root = GetWidgetContainer()->GetRoot();

	root->AddChild(m_image_button);
	root->AddChild(m_close_button);
	root->AddChild(m_edit);
	root->SetListener(this);

	RETURN_IF_ERROR(OpMultilineEdit::Construct(&m_longtext));
	root->AddChild(m_longtext);
	m_longtext->SetWrapping(TRUE);
	m_longtext->SetLabelMode();
	RETURN_IF_ERROR(m_longtext->SetText(text.CStr()));

	m_edit->SetWrapping(m_wrapping);
	RETURN_IF_ERROR(m_edit->SetText(title.CStr()));

	return OpStatus::OK;
}

void OpWidgetNotifier::Layout()
{
	// layout content (image and edit)

	OpRect close_button_rect;
	OpRect image_rect;
	OpRect edit_rect;

	m_close_button->GetRequiredSize(close_button_rect.width, close_button_rect.height);

	if (m_image_button->GetForegroundSkin()->HasContent())
	{
		m_image_button->GetRequiredSize(image_rect.width, image_rect.height);
	}

	edit_rect.width = TEXT_WIDTH;
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

	m_contents_width = edit_rect.x + edit_rect.width + close_button_rect.width + padding_right;
	m_contents_height = max(image_rect.height, edit_rect.height + button_height) + padding_top + padding_bottom;
}

OP_STATUS OpWidgetNotifier::Init(DesktopNotifier::DesktopNotifierType notification_group, const OpStringC& title, Image& image, const OpStringC& text, OpInputAction* action, OpInputAction* cancel_action, BOOL managed, BOOL wrapping)
{
	m_wrapping = wrapping;
	m_image = image;
	m_action = action;
	m_cancel_action = cancel_action;
	
	RETURN_IF_ERROR(InitContent(title, text));

	SetParentInputContext(g_application->GetInputContext());

	m_animate = g_pccore->GetIntegerPref(PrefsCollectionCore::SpecialEffects) != 0;

	Layout();

	return OpStatus::OK;
}

void OpWidgetNotifier::StartShow()
{
	if (!m_anchor) return;

	OpRect anchor = m_anchor->GetScreenRect();

	OpToolTipListener::PREFERRED_PLACEMENT placement = OpToolTipListener::PREFERRED_PLACEMENT_BOTTOM;
	OpRect rect = GetPlacement(m_contents_width, m_contents_height, GetOpWindow(), GetWidgetContainer() , TRUE, anchor, placement);

	// set the arrow to point to the center of the button
	OpWidgetImage* skin = GetSkinImage();

	// By default the rect is center aligned with the button, make it right aligned
	INT32 padding_right, padding_left, dummy;
	skin->GetPadding(&padding_left, &dummy, &padding_right, &dummy);
	if (GetWidgetContainer()->GetRoot()->GetRTL())
		rect.x = anchor.Left() - padding_left;
	else
		rect.x = anchor.Right() - rect.width + padding_right;
	FitRectToScreen(rect);

	OpRect start_rect = rect;
	start_rect.OffsetBy(0, GetAnimationDistance());

	skin->PointArrow(rect, anchor);

	animateOpacity(0, 1);
	animateIgnoreMouseInput(TRUE, FALSE);
	animateRect(start_rect, rect);
	if (m_animate)
		g_animation_manager->startAnimation(this, ANIM_CURVE_SLOW_DOWN);
	else
		g_animation_manager->startAnimation(this, ANIM_CURVE_SLOW_DOWN, 0);

	// We don't want to be active, otherwise we can end up with being 
	// returned by g_application->GetActiveDesktopWindow and cause all
	// kinds of troubles. This is only for linux on which this window
	// is mde window when composite manager is missing. On other platforms
	// this window is a native one so no problem.
	((MDE_OpWindow*)GetWindow())->SetAllowAsActive(FALSE);

	Show(TRUE);
	g_application->SetPopupDesktopWindow(this);

	// Receive keyboard events so you can press ESC to exit
	// Yes, we can have focus even if the window is not active
	SetFocus(FOCUS_REASON_ACTIVATE);
}
