/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "platforms/windows/pi/util/WindowFrame.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/win_handy.h"
#include "modules/skin/opskinmanager.h"
#include "modules/skin/OpSkin.h"
#include "adjunct/quick_toolkit/widgets/OpBar.h"
#include "adjunct/quick/widgets/OpPagebar.h"

void WindowFrame::CalculateSizes()
{
	if(UseCustomNativeFrame())
	{
		// the native border
		m_sizes.m_native_rect.left = m_sizes.m_native_rect.right = 0;
		m_sizes.m_native_rect.top = GetSystemMetrics(SM_CYSIZEFRAME);
		m_sizes.m_native_rect.bottom = m_window->m_fullscreen ? 1 : 0;

		// the ui border
		m_sizes.m_ui_rect.left = m_sizes.m_ui_rect.right = m_sizes.m_ui_rect.bottom = WINDOW_DEFAULT_BORDER_SIZE;
		m_sizes.m_ui_rect.top = 0;
	}
	else if(m_window->m_state != OpWindow::MAXIMIZED && IsSystemWinVista())
	{
		// we only reduce the regular non-persona aero border on Vista/Win7, XP already has a slim border and won't work properly
		// if we reduce if further.
		m_sizes.m_native_rect.left = m_sizes.m_native_rect.right = WINDOW_DEFAULT_BORDER_SIZE;
		m_sizes.m_native_rect.bottom = m_sizes.m_native_rect.top = WINDOW_DEFAULT_BORDER_SIZE;

		// the ui border
		if(UseCustomUIFrame())
		{
			m_sizes.m_ui_rect.left = m_sizes.m_ui_rect.right = m_sizes.m_ui_rect.bottom = WINDOW_DEFAULT_UI_BORDER_SIZE;
		}
		else
		{
			m_sizes.m_ui_rect.left = m_sizes.m_ui_rect.right = m_sizes.m_ui_rect.bottom = 0;
		}
		m_sizes.m_ui_rect.top = 0;
	}
	else
	{
		m_sizes.m_native_rect.left = m_sizes.m_native_rect.right = GetSystemMetrics(SM_CXSIZEFRAME);
		m_sizes.m_native_rect.bottom = m_sizes.m_native_rect.top = GetSystemMetrics(SM_CYSIZEFRAME);

		// the ui border
		m_sizes.m_ui_rect.left = m_sizes.m_ui_rect.right = m_sizes.m_ui_rect.bottom = 0;
		m_sizes.m_ui_rect.top = 0;
	}
}

RECT WindowFrame::GetNativeFrameSizes()
{
	return m_sizes.GetNativeFrameSize();
}

RECT WindowFrame::GetUIFrameSizes()
{
	return m_sizes.GetUIFrameSize();
}

BOOL WindowFrame::UseCustomNativeFrame()
{
	BOOL has_persona_skin = g_skin_manager->HasPersonaSkin();
	OpPagebar* pb = m_window->GetPagebar();
	BOOL pagebar_on_top = pb && (pb->GetAlignment() == OpBar::ALIGNMENT_TOP && m_window->m_requested_titlebar_height > 0);

	return (pagebar_on_top && has_persona_skin && m_window->m_state != OpWindow::MAXIMIZED && m_window->IsThemed() && !m_window->m_menubar_visibility);
}

BOOL WindowFrame::UseCustomUIFrame()
{
	return UseCustomNativeFrame();
}