/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef VEGA_OPPAINTER_SUPPORT

#include "adjunct/quick/widgets/QuickGhostInputView.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/widgets/OpWidget.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "modules/libgogi/pi_impl/mde_widget.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"


QuickGhostInputView::QuickGhostInputView(OpWidget* w, QuickGhostInputViewListener* l) : m_widget(w), m_listener(l), m_left_right_margin(0), m_top_bottom_margin(0)
{
	SetTransparent(true);
	SetFullyTransparent(true);
}

QuickGhostInputView::~QuickGhostInputView()
{
	if (m_listener)
		m_listener->OnGhostInputViewDestroyed(this);
}

void QuickGhostInputView::OnPaint(const MDE_RECT &rect, MDE_BUFFER *screen)
{
#if 0 // Enable this to debug where the ghost view is positioned.
	class VEGAOpPainter* painter = (class VEGAOpPainter*)screen->user_data;
	painter->SetVegaTranslation(0, 0);
	int xp = rect.x;
	int yp = rect.y;
	ConvertToScreen(xp, yp);
	painter->SetClipRect(OpRect(xp, yp, rect.w, rect.h));
	xp = 0;
	yp = 0;
	ConvertToScreen(xp, yp);

	painter->SetColor(255, 0, 0, 255);
	painter->DrawRect(OpRect(xp, yp, m_rect.w, m_rect.h), 1);

	painter->RemoveClipRect();
#endif
}

void QuickGhostInputView::OnMouseDown(int x, int y, int button, int clicks, ShiftKeyState keystate)
{
	m_screen->SetDragCapture(this); // DSK-359867

	if (g_widget_globals->captured_widget)
	{
		for (OpWidget* run = g_widget_globals->captured_widget; run->parent; run = run->parent)
		{
			x += run->GetRect().x;
			y += run->GetRect().y;
		}
	}
	MouseButton b = MOUSE_BUTTON_1;
	switch (button)
	{
	case 2:
		b = MOUSE_BUTTON_2;
		break;
	case 3:
		b = MOUSE_BUTTON_3;
		break;
	default:
		break;
	}
	m_widget->GenerateOnMouseDown(OpPoint(x, y), b, clicks);
}

void QuickGhostInputView::OnMouseUp(int x, int y, int button, ShiftKeyState keystate)
{
	m_screen->ReleaseDragCapture(); // DSK-359867

	if (g_widget_globals->captured_widget)
	{
		for (OpWidget* run = g_widget_globals->captured_widget; run->parent; run = run->parent)
		{
			x += run->GetRect().x;
			y += run->GetRect().y;
		}
	}
	MouseButton b = MOUSE_BUTTON_1;
	switch (button)
	{
	case 2:
		b = MOUSE_BUTTON_2;
		break;
	case 3:
		b = MOUSE_BUTTON_3;
		break;
	default:
		break;
	}
	m_widget->GenerateOnMouseUp(OpPoint(x, y), b);
}

void QuickGhostInputView::OnMouseMove(int x, int y, ShiftKeyState keystate)
{
	// If the widget is captured it will translate to local coordinates, so we need to send screen coordinates
	if (g_widget_globals->captured_widget)
	{
		for (OpWidget* run = g_widget_globals->captured_widget; run->parent; run = run->parent)
		{
			x += run->GetRect().x;
			y += run->GetRect().y;
		}
	}
	// Use generate instead of calling OnMove directly, this makes sure cursor updating works
	m_widget->GenerateOnMouseMove(OpPoint(x, y));
}

void QuickGhostInputView::OnDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y)
{
	g_drag_manager->StopDrag(TRUE); // DSK-359867
}

void QuickGhostInputView::OnDragDrop(int x, int y, ShiftKeyState modifiers)
{
	OpDragObject* drag_object = g_drag_manager->GetDragObject();
	drag_object->SetDropType(DROP_NONE);
	drag_object->SetVisualDropType(DROP_NONE);
	WidgetContainer* container = m_widget->GetWidgetContainer();
	container->OnDragDrop(container->GetView(), drag_object, OpPoint(x, y), modifiers);
}

void QuickGhostInputView::OnDragEnd(int x, int y, ShiftKeyState modifiers)
{
	OpDragObject* drag_object = g_drag_manager->GetDragObject();
	WidgetContainer* container = m_widget->GetWidgetContainer();
	container->OnDragEnd(container->GetView(), drag_object, OpPoint(x,y), modifiers);
}

void QuickGhostInputView::OnDragCancel(ShiftKeyState modifiers)
{
	WidgetContainer* container = m_widget->GetWidgetContainer();
	container->OnDragCancel(container->GetView(), g_drag_manager->GetDragObject(), modifiers);
}


bool QuickGhostInputView::IsType(const char* type)
{
	return (!op_strcmp(type, "QuickGhostInputView"))||MDE_View::IsType(type);
}

bool QuickGhostInputView::IsLeftButtonPressed()
{
	return m_screen->m_captured_input == this && m_screen->m_captured_button == 1;
}

void QuickGhostInputView::SetWidgetVisible(BOOL vis)
{
	if (vis)
	{
		if (!m_parent)
		{
			((MDE_OpView*)m_widget->GetWidgetContainer()->GetOpView())->GetMDEWidget()->AddChild(this);
			SetZ(MDE_Z_TOP);
			WidgetRectChanged();
		}
	}
	else
	{
		if (m_parent)
			m_parent->RemoveChild(this);
	}
}

void QuickGhostInputView::SetMargins(unsigned int xmarg, unsigned int ymarg)
{
	m_left_right_margin = xmarg;
	m_top_bottom_margin = ymarg;
	WidgetRectChanged();
}

void QuickGhostInputView::WidgetRectChanged()
{
	OpRect r = m_widget->GetRect(TRUE);
	m_widget->GetWidgetContainer()->GetView()->ConvertToContainer(r.x, r.y);
	MDE_RECT mr = MDE_MakeRect(r.x-m_left_right_margin, r.y-m_top_bottom_margin, r.width+m_left_right_margin*2, r.height+m_top_bottom_margin*2);
	SetRect(mr);
}

#endif // VEGA_OPPAINTER_SUPPORT

