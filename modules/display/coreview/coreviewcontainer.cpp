/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/coreview/coreview.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/pi/OpPainter.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/widgets/WidgetContainer.h"

#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT

//Will print out CoreView hierarchy to the outputwindow in visual studio
//#define DEBUG_COREVIEW

#ifdef DEBUG_COREVIEW

void DebugCoreView(CoreView* view, int level)
{
	uni_char buf[500]; // ARRAY OK 2009-01-27 emil
	for(int i = 0; i < level; i++)
		buf[i] = ' ';
	const uni_char* type = view->GetContainer() == view ? L"CoreViewContainer" : L"CoreView";
	uni_sprintf(&buf[level], L"%s %d %d %d %d  wantpaint: %d  wantmouse: %d  visible: %d %p\n", type, view->GetExtents().x, view->GetExtents().y,
					view->GetExtents().width, view->GetExtents().height, view->GetWantPaintEvents(), view->GetWantMouseEvents(), view->GetVisibility(), view);
	OutputDebugString(buf);

	INT32 rx, ry;
	UINT32 rw, rh;
	if (view->GetContainer() == view)
	{
		OpRect rect = view->GetExtents();
		((CoreViewContainer*)view)->ConvertToParentContainer(rect.x, rect.y);

		view->GetOpView()->GetPos(&rx, &ry);
		view->GetOpView()->GetSize(&rw, &rh);
		if (rect.x != rx || rect.y != ry ||
			rect.width != rw || rect.height != rh)
			int stop = 0;
	}

	CoreView* child = (CoreView*) view->m_children.First();
	while(child)
	{
		DebugCoreView(child, level + 2);
		child = (CoreView*) child->Suc();
	}
}

#endif



CoreViewContainer::CoreViewContainer()
	: m_opview(NULL)
	, m_painter(NULL)
	, m_getpainter_count(0)
	, m_before_painting(FALSE)
	, m_paint_lock(0)
#ifndef MOUSELESS
	, m_drag_view(NULL)
	, m_hover_view(NULL)
	, m_captured_view(NULL)
	, m_captured_button(MOUSE_BUTTON_1)
	, m_last_click_time(0)
	, m_last_click_button(MOUSE_BUTTON_1)
	, m_emulated_n_click(0)
#endif
{
	packed5.is_container = TRUE;
	// We will get paint events from the OpView and call Paint, so we don't want CoreView to call paint too.
	SetWantPaintEvents(FALSE);

#ifdef TOUCH_EVENTS_SUPPORT
	op_memset(&m_touch_captured_view, 0, sizeof(m_touch_captured_view));
#endif // TOUCH_EVENTS_SUPPORT
}

CoreViewContainer::~CoreViewContainer()
{
#ifdef _PLUGIN_SUPPORT_
	RemoveAllPluginAreas();
#endif
	OP_DELETE(m_opview);
}

/*static */OP_STATUS CoreViewContainer::Create(CoreView** view, OpWindow* parentwindow, OpView* parentopview, CoreView* parentview)
{
	*view = OP_NEW(CoreViewContainer, ());
	if (!*view)
		return OpStatus::ERR;

	OP_STATUS status = ((CoreViewContainer*)*view)->Construct(parentwindow, parentopview, parentview);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(*view);
		*view = NULL;
	}
	return status;
}

OP_STATUS CoreViewContainer::Construct(OpWindow* parentwindow, OpView* parentopview, CoreView* parentview)
{
	if (parentview)
	{
		RETURN_IF_ERROR(OpView::Create(&m_opview, parentview->GetOpView()));
		RETURN_IF_ERROR(CoreView::Construct(parentview));
	}
	else if (parentopview)
		RETURN_IF_ERROR(OpView::Create(&m_opview, parentopview));
	else
		RETURN_IF_ERROR(OpView::Create(&m_opview, parentwindow));

	m_opview->SetPaintListener(this);
#ifndef MOUSELESS
	m_opview->SetMouseListener(this);
#endif // !MOUSELESS
#ifdef TOUCH_EVENTS_SUPPORT
	m_opview->SetTouchListener(this);
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	m_opview->SetDragListener(this);
#endif // DRAG_SUPPORT
#ifdef WIDGETS_IME_SUPPORT
	m_opview->SetInputMethodListener(g_im_listener);
#endif

	return OpStatus::OK;
}

void CoreViewContainer::BeginPaintLock()
{
	m_paint_lock++;
}

void CoreViewContainer::EndPaintLock()
{
	m_paint_lock--;
	OP_ASSERT(m_paint_lock >= 0);
}

OpPainter* CoreViewContainer::GetPainter(const OpRect &rect, BOOL nobackbuffer)
{
	if (m_getpainter_count == 0)
		m_painter = m_opview->GetPainter(rect, nobackbuffer);
	if (m_painter)
		m_getpainter_count++;
	return m_painter;
}

void CoreViewContainer::ReleasePainter(const OpRect &rect)
{
	m_getpainter_count--;
	if (m_getpainter_count == 0)
	{
		m_opview->ReleasePainter(rect);
		m_painter = NULL;
	}
}

// == OpPaintListener ======================================

BOOL CoreViewContainer::BeforePaint()
{
	BOOL painted = TRUE;
	if (!m_before_painting)
	{
		m_before_painting = TRUE;
		painted = CallBeforePaint();
		m_before_painting = FALSE;
	}
	return painted;
}

void CoreViewContainer::OnPaint(const OpRect &rect, OpView* view)
{
	if (m_paint_listener)
	{
		// Be sure we start with no plugin areas
		OP_ASSERT(!plugin_intersections.First());
#ifdef _PLUGIN_SUPPORT_
		RemoveAllPluginAreas();
#endif

#ifdef _SUPPORT_SMOOTH_DISPLAY_
		view->UseDoublebuffer(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SmoothDisplay));
#endif

		GetPainter(rect);
		if (!m_painter)
		{
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return;
		}

		Paint(rect, m_painter);

		ReleasePainter(rect);

#ifdef _PLUGIN_SUPPORT_
		UpdateAndDeleteAllPluginAreas(rect);
#endif
	}
}

// == OpMoustListener ======================================

#ifndef MOUSELESS

void CoreViewContainer::OnMouseMove(const OpPoint &point, ShiftKeyState keystate, OpView* view)
{
	m_mouse_move_point = point;
	MouseMove(point, keystate);
}

void CoreViewContainer::OnMouseDown(MouseButton button, UINT8 nclicks, ShiftKeyState keystate, OpView* view)
{
#ifdef DEBUG_COREVIEW
	CoreView* tmp = this;
	while(tmp->m_parent)
		tmp = tmp->m_parent;
	DebugCoreView(tmp, 0);
#endif

#ifdef EMULATE_TRIPPLE_CLICK

	if (m_mouse_down_point.x == m_mouse_move_point.x &&
		m_mouse_down_point.y == m_mouse_move_point.y &&
		button == m_last_click_button &&
		m_last_click_time && m_last_click_time + 600 > g_op_time_info->GetWallClockMS() &&
		(m_emulated_n_click || nclicks))
	{
		if (m_emulated_n_click)
			m_emulated_n_click++;
		else
			m_emulated_n_click = 2;

		nclicks = m_emulated_n_click;
	}
	else
	{
		m_emulated_n_click = 0;
		m_mouse_down_point = m_mouse_move_point;
	}
	m_last_click_time = g_op_time_info->GetWallClockMS();
	m_last_click_button = button;
#endif // EMULATE_TRIPPLE_CLICK

	MouseDown(m_mouse_move_point, button, nclicks, keystate);
}

void CoreViewContainer::OnMouseUp(MouseButton button, ShiftKeyState keystate, OpView* view)
{
	MouseUp(m_mouse_move_point, button, keystate);
}

void CoreViewContainer::OnMouseLeave()
{
	MouseLeave();
	m_hover_view = NULL;
}

BOOL CoreViewContainer::OnMouseWheel(INT32 delta, BOOL vertical, ShiftKeyState keystate)
{
	return MouseWheel(delta, vertical, keystate);
}

void CoreViewContainer::OnSetCursor()
{
	// Called automatically by CoreView::MouseMove since we will get a new m_hover_view
}

void CoreViewContainer::OnMouseCaptureRelease()
{
	if (m_captured_view)
		m_captured_view->ReleaseMouseCapture();
}

#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT

void CoreViewContainer::OnTouchDown(int id, const OpPoint& point, int radius, ShiftKeyState modifiers, OpView* view, void* user_data)
{
	TouchDown(id, point, radius, modifiers, user_data);
}

void CoreViewContainer::OnTouchUp(int id, const OpPoint& point, int radius, ShiftKeyState modifiers, OpView* view, void* user_data)
{
	TouchUp(id, point, radius, modifiers, user_data);
}

void CoreViewContainer::OnTouchMove(int id, const OpPoint& point, int radius, ShiftKeyState modifiers, OpView* view, void* user_data)
{
	TouchMove(id, point, radius, modifiers, user_data);
}

#endif // TOUCH_EVENTS_SUPPORT

#ifdef DRAG_SUPPORT
void CoreViewContainer::OnDragStart(OpView* view, const OpPoint& start_point, ShiftKeyState modifiers, const OpPoint& current_point)
{
	if (g_drag_manager->IsDragging())
		return;

	m_drag_view = GetMouseHitView(start_point.x, start_point.y);

	if (m_drag_view && m_drag_view->m_drag_listener)
	{
		OpPoint local_start_point = start_point;
		OpPoint local_current_point = current_point;
		m_drag_view->ConvertFromContainer(local_start_point.x, local_start_point.y);
		m_drag_view->ConvertFromContainer(local_current_point.x, local_current_point.y);
		m_drag_view->m_drag_listener->OnDragStart(m_drag_view, local_start_point, modifiers, local_current_point);
	}
}

void CoreViewContainer::OnDragEnter(OpView* view, const OpPoint& point, ShiftKeyState modifiers)
{
	if (!g_drag_manager->IsDragging() || g_drag_manager->IsBlocked())
		return;

	m_drag_view = GetMouseHitView(point.x, point.y);

	if (m_drag_view && m_drag_view->m_drag_listener)
	{
		OpPoint tpoint = point;
		m_drag_view->ConvertFromContainer(tpoint.x, tpoint.y);
		m_drag_view->m_drag_listener->OnDragEnter(m_drag_view, g_drag_manager->GetDragObject(), tpoint, modifiers);
	}
}

void CoreViewContainer::OnDragCancel(OpView* view, ShiftKeyState modifiers)
{
	if (!g_drag_manager->IsDragging())
		return;

	if (m_drag_view && m_drag_view->m_drag_listener)
	{
		m_drag_view->m_drag_listener->OnDragCancel(m_drag_view, g_drag_manager->GetDragObject(), modifiers);
		m_drag_view = NULL;
	}
	else // No view to notify. Cancel d'n'd at this point.
		g_drag_manager->StopDrag(TRUE);
}

void CoreViewContainer::OnDragLeave(OpView* view, ShiftKeyState modifiers)
{
	if (m_drag_view && m_drag_view->m_drag_listener && g_drag_manager->IsDragging())
	{
		m_drag_view->m_drag_listener->OnDragLeave(m_drag_view, g_drag_manager->GetDragObject(), modifiers);
	}
}

void CoreViewContainer::OnDragMove(OpView* view, const OpPoint& point, ShiftKeyState modifiers)
{
	if (!g_drag_manager->IsDragging())
		return;

	if (m_captured_view)
		// Need to release capture since we won't get any mouseup (on windows).
		m_captured_view->ReleaseMouseCapture();

	CoreView* new_view = GetMouseHitView(point.x, point.y);
	if (new_view != m_drag_view)
	{
		if (m_drag_view && m_drag_view->m_drag_listener)
			m_drag_view->m_drag_listener->OnDragLeave(m_drag_view, g_drag_manager->GetDragObject(), modifiers);

		m_drag_view = new_view;

		if (m_drag_view && m_drag_view->m_drag_listener)
		{
			OpPoint tpoint = point;
			m_drag_view->ConvertFromContainer(tpoint.x, tpoint.y);
			m_drag_view->m_drag_listener->OnDragEnter(new_view, g_drag_manager->GetDragObject(), tpoint, modifiers);
			return;
		}
	}
	if (m_drag_view && m_drag_view->m_drag_listener)
	{
		OpPoint tpoint = point;
		m_drag_view->ConvertFromContainer(tpoint.x, tpoint.y);
		m_drag_view->m_drag_listener->OnDragMove(m_drag_view, g_drag_manager->GetDragObject(), tpoint, modifiers);
	}
}

void CoreViewContainer::OnDragDrop(OpView* view, const OpPoint& point, ShiftKeyState modifiers)
{
	if (!g_drag_manager->IsDragging())
		return;

	if (m_drag_view)
	{
		OpPoint tpoint = point;
		m_drag_view->ConvertFromContainer(tpoint.x, tpoint.y);

		if (m_drag_view->m_drag_listener)
			m_drag_view->m_drag_listener->OnDragDrop(m_drag_view, g_drag_manager->GetDragObject(), tpoint, modifiers);

		m_drag_view = NULL;
	}
	else // No view to notify. End d'n'd at this point.
		g_drag_manager->StopDrag();
}

void CoreViewContainer::OnDragEnd(OpView* view, const OpPoint& point, ShiftKeyState modifiers)
{
	if (!g_drag_manager->IsDragging())
		return;

	if (m_drag_view)
	{
		OpPoint tpoint = point;
		m_drag_view->ConvertFromContainer(tpoint.x, tpoint.y);

		if (m_drag_view->m_drag_listener)
			m_drag_view->m_drag_listener->OnDragEnd(m_drag_view, g_drag_manager->GetDragObject(), tpoint, modifiers);

		m_drag_view = NULL;
	}
	else // No view to notify. End d'n'd at this point.
		g_drag_manager->StopDrag();
}

#endif // DRAG_SUPPORT

void CoreViewContainer::OnVisibilityChanged(BOOL visible)
{
	BOOL true_visibility = (BOOL) packed5.visible;

	// Find out if there are a parent CoreView which is hidden.
	if (m_parent)
	{
		CoreViewContainer* container = m_parent->GetContainer();
		CoreView *tmp = this;
		while(tmp->m_parent && tmp != container)
		{
			if (!tmp->packed5.visible)
			{
				true_visibility = FALSE;
				break;
			}
			tmp = tmp->m_parent;
		}
	}
	m_opview->SetVisibility(true_visibility);
}

void CoreViewContainer::ConvertToParentContainer(int &x, int &y)
{
	if (!m_parent)
		return;
	CoreViewContainer* container = m_parent->GetContainer();
	CoreView *tmp = this;
	while(tmp->m_parent && tmp != container)
	{
		tmp->ConvertToParent(x, y);
		tmp = tmp->m_parent;
	}
}

/*virtual*/OpRect
CoreViewContainer::GetScreenRect()
{
	OpPoint top_left(0, 0);
	top_left = ConvertToScreen(top_left);

	return OpRect(top_left.x, top_left.y, m_width, m_height);
}

void CoreViewContainer::OnMoved()
{
	int x = 0, y = 0;
	ConvertToParentContainer(x, y);
	m_opview->SetPos(x, y);

	CoreView::OnMoved();
}

void CoreViewContainer::SetPos(const AffinePos& pos, BOOL onmove, BOOL invalidate)
{
	CoreView::SetPos(pos, onmove, invalidate);
}

void CoreViewContainer::SetSize(INT32 w, INT32 h)
{
	m_opview->SetSize(w, h);
	CoreView::SetSize(w, h);
}

void CoreViewContainer::ScrollRect(const OpRect &rect, INT32 dx, INT32 dy)
{
	m_opview->ScrollRect(rect, dx, dy);
}

void CoreViewContainer::Sync()
{
	if (m_getpainter_count == 0 && !m_before_painting && !m_paint_lock) // Never sync while already painting.
		GetOpView()->Sync();
}

void CoreViewContainer::LockUpdate(BOOL lock)
{
	m_opview->LockUpdate(lock);
}

void CoreViewContainer::Scroll(INT32 dx, INT32 dy)
{
	// Just update the position of the children. (No onmove called)
	MoveChildren(dx, dy, FALSE);

	// Scroll. (Might trig paint that applies childrens new position)
	m_opview->Scroll(dx, dy, FALSE);

	// Call OnMoved so we are sure new position is applied.
	CoreView* child = (CoreView*) m_children.First();
	while(child)
	{
		if (!child->GetFixedPosition())
			child->OnMoved();
		child = (CoreView*) child->Suc();
	}
}
