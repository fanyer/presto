/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*- */
#include "core/pch.h"
#include "modules/libgogi/mde.h"

#ifdef MDE_SUPPORT_MOUSE

void MDE_Screen::ReleaseMouseCapture()
{
	if (m_captured_input)
	{
		// OnMouseLeave() below can trigger a re-enter (example: DSK-318237)
		MDE_View *captured_input = m_captured_input;
		m_captured_input = 0;

		MDE_View *target = GetViewAt(m_mouse_x, m_mouse_y, true);
		if (target != captured_input)
			captured_input->OnMouseLeave();
		captured_input->OnMouseCaptureRelease();
	}
}

MDE_View *MDE_View::GetViewAt(int x, int y, bool include_children)
{
	MDE_View *tmp = m_first_child;
	MDE_View *last_match = NULL;
	while(tmp)
	{
		if (tmp->m_is_visible && tmp->GetHitStatus(x - tmp->m_rect.x, y - tmp->m_rect.y))
		{
			if (include_children)
			{
				last_match = tmp->GetViewAt(x - tmp->m_rect.x, y - tmp->m_rect.y, include_children);
				if (!last_match)
					last_match = tmp;
			}
			else
				last_match = tmp;
		}
		tmp = tmp->m_next;
	}
	return last_match;
}

bool MDE_View::GetHitStatus(int x, int y)
{
	if (m_custom_overlap_region.num_rects)
	{
		if (!MDE_RectContains(m_rect, x + m_rect.x, y + m_rect.y))
			return false;
		for (int i = 0; i < m_custom_overlap_region.num_rects; i++)
		{
			if (MDE_RectContains(m_custom_overlap_region.rects[i], x, y))
				return false;
		}
		return true;
	}
	return MDE_RectContains(m_rect, x + m_rect.x, y + m_rect.y);
}

void MDE_View::TrigMouseDown(int x, int y, int button, int clicks, ShiftKeyState keystate)
{
	m_screen->m_mouse_x = x;
	m_screen->m_mouse_y = y;
	if (!m_screen->m_captured_input)
	{
		m_screen->m_captured_input = GetViewAt(x, y, true);
		m_screen->m_captured_button = button;

		// don't allow double clicks where clicks stem from
		// different views - see CORE-28158
		if (g_mde_last_captured_input != m_screen->m_captured_input && clicks > 1)
			clicks = 1;

		g_mde_last_captured_input = m_screen->m_captured_input;
	}
	if (m_screen->m_captured_input)
	{
		m_screen->m_captured_input->ConvertFromScreen(x, y);
		m_screen->m_captured_input->OnMouseDown(x, y, button, clicks, keystate);
	}
}

void MDE_View::TrigMouseUp(int x, int y, int button, ShiftKeyState keystate)
{
	m_screen->m_mouse_x = x;
	m_screen->m_mouse_y = y;
	MDE_View *target = m_screen->m_captured_input ? m_screen->m_captured_input : GetViewAt(x, y, true);
	if (target)
	{
		target->ConvertFromScreen(x, y);
		target->OnMouseUp(x, y, button, keystate);
		if (button == m_screen->m_captured_button)
			m_screen->ReleaseMouseCapture();
	}
}

void MDE_View::TrigMouseMove(int x, int y, ShiftKeyState keystate)
{
	MDE_View *old_target = GetViewAt(m_screen->m_mouse_x, m_screen->m_mouse_y, true);
	m_screen->m_mouse_x = x;
	m_screen->m_mouse_y = y;
	ConvertToScreen(m_screen->m_mouse_x, m_screen->m_mouse_y);

	MDE_View *target = m_screen->m_captured_input ? m_screen->m_captured_input : GetViewAt(x, y, true);
	if (old_target && old_target != target && !m_screen->m_captured_input)
	{
		old_target->OnMouseLeave();
		// Just in case the above call caused target to be deleted.
		target = m_screen->m_captured_input ? m_screen->m_captured_input : GetViewAt(x, y, true);
	}
	if (target)
	{
		int tx = m_screen->m_mouse_x;
		int ty = m_screen->m_mouse_y;
		target->ConvertFromScreen(tx, ty);
		target->OnMouseMove(tx, ty, keystate);
	}
}

void MDE_View::TrigMouseWheel(int x, int y, int delta, bool vertical, ShiftKeyState keystate)
{
	MDE_View *target = m_screen->m_captured_input ? m_screen->m_captured_input : GetViewAt(x, y, true);
	if (target)
	{
		while(target && !target->OnMouseWheel(delta, vertical, keystate))
			target = target->m_parent;
	}
}

void MDE_View::OnMouseDown(int x, int y, int button, int clicks, ShiftKeyState keystate) { }
void MDE_View::OnMouseUp(int x, int y, int button, ShiftKeyState keystate) { }
void MDE_View::OnMouseMove(int x, int y, ShiftKeyState keystate) { }
void MDE_View::OnMouseLeave() { }
bool MDE_View::OnMouseWheel(int delta, bool vertical, ShiftKeyState keystate) { return false; }
void MDE_View::OnMouseCaptureRelease() { }

#endif // MDE_SUPPORT_MOUSE
