/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "adjunct/quick/widgets/QuickChromeHandler.h"
#include "adjunct/quick/widgets/OpPagebar.h"
#include "adjunct/quick/widgets/PagebarButton.h"
#include "adjunct/desktop_pi/DesktopOpWindow.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

#ifdef MDE_OPWINDOW_CHROME_SUPPORT

#define RESIZE_AREA_THICKNESS 8
#define RESIZE_AREA_TOP_THICKNESS 8
#define CHROME_TOP_SKIN "Restored Child Window Chrome Skin"
#define CHROME_WIN_SKIN "Restored Child Window Skin"

/** == OpChromeResizer ============================================================================
	Handle mouse input and cursor on the borders of the chrome so the target window can be resized.
*/
class OpChromeResizer : public OpWidget
{
public:
	OpChromeResizer();
	virtual	~OpChromeResizer();
	static OP_STATUS Construct(OpChromeResizer** obj);

	void SetIsTop(BOOL top) { m_is_top = top; }
	void SetChromeHandler(QuickChromeHandler *ch) { m_ch = ch; }

	void OnMouseMove(const OpPoint &point);
	void OnMouseLeave();
	void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks);
	void OnSetCursor(const OpPoint &point);
private:
	QuickChromeHandler *m_ch;
	CursorType m_cursor;
	BOOL m_resizing;
	OpPoint m_down_point;
	BOOL m_is_top;
};

DEFINE_CONSTRUCT(OpChromeResizer)

OpChromeResizer::OpChromeResizer() : m_ch(NULL), m_cursor(CURSOR_DEFAULT_ARROW), m_resizing(FALSE), m_is_top(FALSE) { }

OpChromeResizer::~OpChromeResizer() { }

void OpChromeResizer::OnMouseMove(const OpPoint &point)
{
	if (m_resizing)
	{
		int adjustment_x = point.x - m_down_point.x;
		int adjustment_y = point.y - m_down_point.y;

		INT32 x, y;
		UINT32 _w, _h;
		m_ch->m_target_window->GetInnerSize(&_w, &_h);
		m_ch->m_target_window->GetInnerPos(&x, &y);
		INT32 w = (INT32)_w, h = (INT32)_h;

		if (m_cursor == CURSOR_SE_RESIZE)
		{
			m_ch->m_target_window->SetInnerSize(MAX(w + adjustment_x, 1), MAX(h + adjustment_y, 1));
			m_down_point = point;
		}
		else if (m_cursor == CURSOR_E_RESIZE)
		{
			m_ch->m_target_window->SetInnerSize(MAX(w + adjustment_x, 1), h);
			m_down_point = point;
		}
		else if (m_cursor == CURSOR_S_RESIZE)
		{
			m_ch->m_target_window->SetInnerSize(w, MAX(h + adjustment_y, 1));
			m_down_point = point;
		}
		else if (m_cursor == CURSOR_W_RESIZE)
		{
			if (w - adjustment_x < 1)
				adjustment_x = w - 1;
			m_ch->m_target_window->SetInnerPos(x + adjustment_x, y);
			m_ch->m_target_window->SetInnerSize(MAX(w - adjustment_x, 1), h);
		}
		else if (m_cursor == CURSOR_SW_RESIZE)
		{
			if (w - adjustment_x < 1)
				adjustment_x = w - 1;
			m_ch->m_target_window->SetInnerPos(x + adjustment_x, y);
			m_ch->m_target_window->SetInnerSize(MAX(w - adjustment_x, 1), MAX(h + adjustment_y, 1));
			m_down_point.Set(m_down_point.x, point.y);
		}
		else if (m_cursor == CURSOR_N_RESIZE)
		{
			if (h - adjustment_y < 1)
				adjustment_y = h - 1;
			if (y + adjustment_y < m_ch->m_sz_tab)
				adjustment_y = m_ch->m_sz_tab - y;
			m_ch->m_target_window->SetInnerPos(x, y + adjustment_y);
			m_ch->m_target_window->SetInnerSize(w, MAX(h - adjustment_y, 1));
		}
		else if (m_cursor == CURSOR_NE_RESIZE)
		{
			if (h - adjustment_y < 1)
				adjustment_y = h - 1;
			if (y + adjustment_y < m_ch->m_sz_tab)
				adjustment_y = m_ch->m_sz_tab - y;
			m_ch->m_target_window->SetInnerPos(x, y + adjustment_y);
			m_ch->m_target_window->SetInnerSize(MAX(w + adjustment_x, 1), MAX(h - adjustment_y, 1));
			m_down_point.Set(point.x, m_down_point.y);
		}
		else if (m_cursor == CURSOR_NW_RESIZE)
		{
			if (w - adjustment_x < 1)
				adjustment_x = w - 1;
			if (h - adjustment_y < 1)
				adjustment_y = h - 1;
			if (y + adjustment_y < m_ch->m_sz_tab)
				adjustment_y = m_ch->m_sz_tab - y;
			m_ch->m_target_window->SetInnerPos(x + adjustment_x, y + adjustment_y);
			m_ch->m_target_window->SetInnerSize(w - adjustment_x, h - adjustment_y);
		}
		return;
	}
	OpRect rect = GetBounds();

	int corner_size = 14;

	m_cursor = CURSOR_DEFAULT_ARROW;

	if (m_is_top)
	{
		if (point.x >= rect.width - corner_size)
			m_cursor = CURSOR_NE_RESIZE;
		else if (point.x <= corner_size)
			m_cursor = CURSOR_NW_RESIZE;
		else
			m_cursor = CURSOR_N_RESIZE;
		return;
	}

	rect.x += m_ch->m_sz_left;
	rect.y += m_ch->m_sz_top;
	rect.width -= m_ch->m_sz_left + m_ch->m_sz_right;
	rect.height -= m_ch->m_sz_top + m_ch->m_sz_bottom;

	if (!rect.InsetBy(-RESIZE_AREA_THICKNESS).Contains(point))
		return;

	if (point.x >= rect.x + rect.width - corner_size && point.y >= rect.y + rect.height)
		m_cursor = CURSOR_SE_RESIZE;
	else if (point.x <= rect.x + corner_size && point.y >= rect.y + rect.height)
		m_cursor = CURSOR_SW_RESIZE;
	else if (point.x <= rect.x)
		m_cursor = CURSOR_W_RESIZE;
	else if (point.x >= rect.x + rect.width)
		m_cursor = CURSOR_E_RESIZE;
	else if (point.y >= rect.y + rect.height)
		m_cursor = CURSOR_S_RESIZE;

}

void OpChromeResizer::OnMouseLeave()
{
	m_resizing = FALSE;
}

void OpChromeResizer::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1 && m_cursor != CURSOR_DEFAULT_ARROW)
	{
		m_resizing = TRUE;
		m_down_point = point;
	}
}

void OpChromeResizer::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	m_resizing = FALSE;
}

void OpChromeResizer::OnSetCursor(const OpPoint &point)
{
	SetCursor(m_cursor);
}

// == QuickChromeHandler ==================================================================

QuickChromeHandler::~QuickChromeHandler()
{
	if (m_target_desktop_window)
		m_target_desktop_window->RemoveListener(this);
	OP_DELETE(m_widget_container);
}

OP_STATUS QuickChromeHandler::InitChrome(OpWindow *chrome_window, OpWindow *target_window, int &chrome_sz_left, int &chrome_sz_top, int &chrome_sz_right, int &chrome_sz_bottom)
{
	m_target_window = target_window;

	// Init WidgetContainer
	m_widget_container = OP_NEW(WidgetContainer, ());
	if (m_widget_container == NULL)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_widget_container->Init(OpRect(0,0,0,0), chrome_window));

	m_target_desktop_window = ((DesktopOpWindow*)target_window)->GetDesktopWindow();

	RETURN_IF_ERROR(m_target_desktop_window->AddListener(this));

	// Set parent input context to the window so actions in the chrome propagates correctly instead of die.
	m_widget_container->SetParentInputContext(m_target_desktop_window);

	// Init widgets
	PagebarButton::Construct(&m_tab_button, m_target_desktop_window);
	m_tab_button->SetButtonTypeAndStyle(OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
	m_tab_button->GetBorderSkin()->SetImage(CHROME_TOP_SKIN);
	m_tab_button->GetForegroundSkin()->SetImage("Window Document Icon");
	m_tab_button->SetIsMoveWindowButton();
	m_tab_button->SetListener(this);

	OpChromeResizer::Construct(&m_top_resizer);
	m_top_resizer->SetHasCssBorder(FALSE);
	m_top_resizer->SetChromeHandler(this);
	m_top_resizer->SetIsTop(TRUE);
	m_widget_container->GetRoot()->AddChild(m_top_resizer);

	m_widget_container->GetRoot()->AddChild(m_tab_button);

	OpChromeResizer::Construct(&m_background);
	m_background->GetBorderSkin()->SetImage(CHROME_WIN_SKIN);
	m_background->GetForegroundSkin()->SetImage(CHROME_WIN_SKIN);
	m_background->SetHasCssBorder(FALSE);
	m_background->SetSkinned(TRUE);
	m_background->SetChromeHandler(this);
	m_widget_container->GetRoot()->AddChild(m_background);

	// FIX: Handle skin changes

	m_sz_tab = 24;
	m_sz_left = m_sz_top = m_sz_right = m_sz_bottom = 0;

	OpSkinElement* win_skin = g_skin_manager->GetSkinElement(CHROME_WIN_SKIN);
	OpSkinElement* tab_skin = g_skin_manager->GetSkinElement(CHROME_TOP_SKIN);
	if (win_skin && tab_skin)
	{
		INT32 left = 0, top = 0, right = 0, bottom = 0;
		win_skin->GetPadding(&left, &top, &right, &bottom, 0);
		m_sz_left = left;
		m_sz_top = top;
		m_sz_right = right;
		m_sz_bottom = bottom;

		INT32 button_w, button_h;
		m_tab_button->GetRequiredSize(button_w, button_h);
		m_sz_tab = button_h;
	}

	chrome_sz_left = m_sz_left;
	chrome_sz_top = m_sz_top + m_sz_tab;
	chrome_sz_right = m_sz_right;
	chrome_sz_bottom = m_sz_bottom;

	m_tab_button->UpdateTextAndIcon(true, true);

	return OpStatus::OK;
}

void QuickChromeHandler::OnResize(int new_w, int new_h)
{
	m_widget_container->SetSize(new_w, new_h);
	m_background->SetRect(OpRect(0, m_sz_tab, new_w, new_h - m_sz_tab));
	m_top_resizer->SetRect(OpRect(0, 0, new_w, RESIZE_AREA_TOP_THICKNESS));
	m_tab_button->SetRect(OpRect(0, 0, new_w, m_sz_tab));
}

BOOL QuickChromeHandler::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	// FIX: New context menu?
	// g_application->ShowPopupMenu("Pagebar Item Popup Menu");
	return FALSE;
}

void QuickChromeHandler::OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active)
{
	if (desktop_window == m_target_desktop_window)
		m_tab_button->SetValue(active ? 1 : 0);
}

// == MDEChromeHandler ==================================================================

/*static */ MDEChromeHandler* MDEChromeHandler::Create()
{
	return OP_NEW(QuickChromeHandler, ());
}

#endif // MDE_OPWINDOW_CHROME_SUPPORT
