/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpSpinner.h"

// == OpSpinner ===========================================================

DEFINE_CONSTRUCT(OpSpinner)

OpSpinner::OpSpinner() :
	m_up_button(NULL),
	m_down_button(NULL)
{
	OP_STATUS status;

	status = OpButton::Construct(&m_down_button);
	CHECK_STATUS(status);
	AddChild(m_down_button, TRUE);

	status = OpButton::Construct(&m_up_button);
	CHECK_STATUS(status);
	AddChild(m_up_button, TRUE);

#ifdef SKIN_SUPPORT
	m_up_button->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
	m_down_button->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
	m_up_button->GetBorderSkin()->SetImage("Spinner Button Up", "Push Button Skin");
	m_down_button->GetBorderSkin()->SetImage("Spinner Button Down", "Push Button Skin");
	m_up_button->GetForegroundSkin()->SetImage("Up Arrow");
	m_down_button->GetForegroundSkin()->SetImage("Down Arrow");
#endif
	m_up_button->SetText(UNI_L("+"));
	m_down_button->SetText(UNI_L("-"));

	m_up_button->SetTabStop(FALSE);
	m_down_button->SetTabStop(FALSE);

	m_up_button->SetListener(this);
	m_down_button->SetListener(this);
	m_up_button->SetJustify(JUSTIFY_CENTER, TRUE);
	m_down_button->SetJustify(JUSTIFY_CENTER, TRUE);
}

void OpSpinner::OnResize(INT32* new_w, INT32* new_h)
{
	INT32 button_width = *new_w;
	INT32 button_height = (*new_h + 1)/2;
	m_up_button->SetRect(OpRect(0, 0, button_width, button_height));
	m_down_button->SetRect(OpRect(0, *new_h - button_height, button_width, button_height));
}

void OpSpinner::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	// The buttons should be centered regardless of the alignment of
	// the control as a whole.  We don't want any style to affect the
	// buttons
	m_up_button->UnsetForegroundColor();
	m_up_button->UnsetBackgroundColor();
	m_down_button->UnsetForegroundColor();
	m_down_button->UnsetBackgroundColor();
}

void OpSpinner::OnClick(OpWidget *object, UINT32 id)
{
	OP_ASSERT(listener); // This is quite meaningless without a listener
// We are now invoking listener->OnClick when the button is pressed DOWN and then on a timer.
// So we don't want to care about the OnClick sent by the OnButton since that is done on mouse UP.
/*	if (listener)
	{
		int val;
		if (object == m_up_button)
		{
			val = 0;
		}
		else
		{
			val = 1;
		}

		listener->OnClick(this, val);
	}*/
}

void OpSpinner::OnTimer()
{
	if (GetTimerDelay() == GetInfo()->GetScrollDelay(FALSE, TRUE))
	{
		// Increase repeat speed (first time is slower)
		StopTimer();
		StartTimer(GetInfo()->GetScrollDelay(FALSE, FALSE));
	}
	InvokeCurrentlyPressedButton();
}

void OpSpinner::InvokeCurrentlyPressedButton()
{
	if (!listener)
		return;
	if (m_up_button->IsDown())
		listener->OnClick(this, 0);
	else if (m_down_button->IsDown())
		listener->OnClick(this, 1);
}

void OpSpinner::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1 && (widget == m_up_button || widget == m_down_button))
	{
		if (down)
		{
			if (listener)
				listener->OnClick(this, widget == m_up_button ? 0 : 1);
			StartTimer(GetInfo()->GetScrollDelay(FALSE, TRUE));
		}
		else
			StopTimer();
	}
}

void OpSpinner::OnMouseLeave(OpWidget *widget)
{
	StopTimer();
}

void OpSpinner::GetPreferedSize(INT32* w, INT32* h)
{
	m_up_button->GetPreferedSize(w, h, 0, 0);
	*w = MAX(*w, *h); // In case of no "real" content
	*h *= 2; // two buttons
}
