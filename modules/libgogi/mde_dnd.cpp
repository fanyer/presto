/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*- */
#include "core/pch.h"
#include "modules/libgogi/mde.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"

#ifdef MDE_SUPPORT_DND

void MDE_View::TrigDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y)
{
	m_screen->m_drag_x = start_x;
	m_screen->m_drag_y = start_y;

	MDE_View* view;
	if (m_screen->m_dnd_capture_view)
		view = m_screen->m_dnd_capture_view;
	else
		view = GetViewAt(start_x, start_y, true);
	if (view)
	{
		m_screen->m_last_dnd_view = view;
		view->ConvertFromScreen(start_x, start_y);
		view->ConvertFromScreen(current_x, current_y);
		view->OnDragStart(start_x, start_y, modifiers, current_x, current_y);
	}
}

void MDE_View::TrigDragEnter(int x, int y, ShiftKeyState modifiers)
{
	m_screen->m_drag_x = x;
	m_screen->m_drag_y = y;

	MDE_View* view;
	if (m_screen->m_dnd_capture_view)
		view = m_screen->m_dnd_capture_view;
	else
		view = GetViewAt(x, y, true);

	if (view)
	{
		m_screen->m_last_dnd_view = view;
		view->ConvertFromScreen(x, y);
		view->OnDragEnter(x, y, modifiers);
	}
}

void MDE_View::TrigDragDrop(int x, int y, ShiftKeyState modifiers)
{
	MDE_View* target;
	if (m_screen->m_dnd_capture_view)
		target = m_screen->m_dnd_capture_view;
	else
		target = GetViewAt(x, y, true);

	if (target)
	{
		target->ConvertFromScreen(x, y);
		target->OnDragDrop(x, y, modifiers);
		m_screen->m_last_dnd_view = NULL;
	}
	else
		g_drag_manager->StopDrag();
}

void MDE_View::TrigDragEnd(int x, int y, ShiftKeyState modifiers)
{
	if (m_screen->m_last_dnd_view)
	{
		m_screen->m_last_dnd_view->ConvertFromScreen(x, y);
		m_screen->m_last_dnd_view->OnDragEnd(x, y, modifiers);
		m_screen->m_last_dnd_view = NULL;
	}
	else
		g_drag_manager->StopDrag();
}

void MDE_View::TrigDragCancel(ShiftKeyState modifiers)
{
	MDE_View* target;
	if (m_screen->m_dnd_capture_view)
		target = m_screen->m_dnd_capture_view;
	else
		target = GetViewAt(m_screen->m_drag_x, m_screen->m_drag_y, true);

	if (!target)
		target = m_screen->m_last_dnd_view;
	if (target)
	{
		target->OnDragCancel(modifiers);
		m_screen->m_last_dnd_view = NULL;
	}
	else
		g_drag_manager->StopDrag(TRUE);
}

void MDE_View::TrigDragLeave(ShiftKeyState modifiers)
{
	if (m_screen->m_last_dnd_view)
	{
		m_screen->m_last_dnd_view->OnDragLeave(modifiers);
	}
}

void MDE_View::TrigDragMove(int x, int y, ShiftKeyState modifiers)
{
	MDE_View *old_target = GetViewAt(m_screen->m_drag_x, m_screen->m_drag_y, true);

	BOOL changed = FALSE;

	MDE_View *target = GetViewAt(x, y, true);
	if (old_target && old_target != target)
	{
		old_target->OnDragLeave(modifiers);
		changed = TRUE;
	}

	m_screen->m_drag_x = x;
	m_screen->m_drag_y = y;

	if (target)
	{
		int tx = m_screen->m_drag_x;
		int ty = m_screen->m_drag_y;
		target->ConvertFromScreen(tx, ty);
		if (!changed)
			target->OnDragMove(tx, ty, modifiers);
		else
		{
			target->OnDragEnter(tx, ty, modifiers);
			m_screen->m_last_dnd_view = target;
		}
	}
}

void MDE_View::TrigDragUpdate()
{
	MDE_View* target;
	if (m_screen->m_dnd_capture_view)
		target = m_screen->m_dnd_capture_view;
	else
		target = GetViewAt(m_screen->m_drag_x, m_screen->m_drag_y, true);

	if (target)
	{
		int tx = m_screen->m_drag_x;
		int ty = m_screen->m_drag_y;
		target->ConvertFromScreen(tx, ty);
		target->OnDragUpdate(tx, ty);
	}
}

void MDE_View::OnDragStart(int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y) { OP_ASSERT(!"Not supported by this view"); }
void MDE_View::OnDragDrop(int x, int y, ShiftKeyState modifiers) { OP_ASSERT(!"Not supported by this view"); }
void MDE_View::OnDragEnd(int x, int y, ShiftKeyState modifiers) { OP_ASSERT(!"Not supported by this view"); }
void MDE_View::OnDragMove(int x, int y, ShiftKeyState modifiers) { OP_ASSERT(!"Not supported by this view"); }
void MDE_View::OnDragLeave(ShiftKeyState modifiers) { OP_ASSERT(!"Not supported by this view"); }
void MDE_View::OnDragCancel(ShiftKeyState modifiers) { OP_ASSERT(!"Not supported by this view"); }
void MDE_View::OnDragEnter(int x, int y, ShiftKeyState modifiers) { OP_ASSERT(!"Not supported by this view"); }
void MDE_View::OnDragUpdate(int x, int y) { OP_ASSERT(!"Not supported by this view"); }

#endif // MDE_SUPPORT_DND
