/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpWindowMover.h"

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

DEFINE_CONSTRUCT(OpWindowMover);

OpWindowMover::OpWindowMover()
		: OpButton(TYPE_CUSTOM, STYLE_TEXT)
		, m_window(NULL)
		, m_moving(false)
{
	SetJustify(JUSTIFY_LEFT, FALSE);
}

void OpWindowMover::OnMouseMove(const OpPoint& point)
{
	if (m_moving && m_window)
	{
		INT32 x, y;
		m_window->GetOuterPos(x, y);
		x += point.x - m_down_point.x;
		y += point.y - m_down_point.y;
		OpWindow* parent_window = m_window->GetOpWindow()->GetParentWindow();
		if (parent_window)
		{
			// Limit position within the parent so the user can move it back again.
			UINT32 parent_w, parent_h;
			parent_window->GetInnerSize(&parent_w, &parent_h);
			int extra = 4;
			x = MAX(x, -m_down_point.x + extra);
			y = MAX(y, -m_down_point.y + extra);
			x = MIN(x, (INT32)parent_w - m_down_point.x - extra);
			y = MIN(y, (INT32)parent_h - m_down_point.y - extra);
		}
		m_window->SetOuterPos(x, y);
	}
}

void OpWindowMover::OnMouseDown(const OpPoint& point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1)
	{
		m_down_point = point;
		m_moving = true;
	}
}

void OpWindowMover::OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks)
{
	m_moving = false;
}
