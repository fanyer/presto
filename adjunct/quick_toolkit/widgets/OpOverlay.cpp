/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    emil
 *
 */

#include "core/pch.h"
#include "adjunct/quick_toolkit/widgets/OpOverlay.h"

OP_STATUS OpOverlayLayoutWidget::Init(DesktopWindow *overlay_window, OpWidget *parent)
{
	RETURN_IF_ERROR(overlay_window->AddListener(this));
	m_overlay_window = overlay_window;
	parent->AddChild(this);
	SetOnMoveWanted(TRUE);
	return OpStatus::OK;
}

OpOverlayLayoutWidget::~OpOverlayLayoutWidget()
{
	RemoveListeners();
}

void OpOverlayLayoutWidget::RemoveListeners()
{
	if (m_overlay_window)
	{
		m_overlay_window->RemoveListener(this);
		m_overlay_window = NULL;
	}
}

void OpOverlayLayoutWidget::OnResize(INT32* new_w, INT32* new_h)
{
}

void OpOverlayLayoutWidget::OnLayout()
{
	OpWidget::OnRelayout();
	if (m_overlay_window)
	{
		m_overlay_window->SetInnerSize(rect.width, rect.height);
		m_overlay_window->SetInnerPos(rect.x, rect.y);
	}
}

void OpOverlayLayoutWidget::OnMove()
{
	OnLayout();
}

void OpOverlayLayoutWidget::OnDeleted()
{
	RemoveListeners();
}

void OpOverlayLayoutWidget::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
}

void OpOverlayLayoutWidget::OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height)
{
	SetRect(OpRect(rect.x, rect.y, width, height));
}
