/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"
#include "modules/display/coreview/coreview.h"
#include "modules/pi/OpPainter.h"
#include "modules/doc/frm_doc.h"

#include "modules/display/vis_dev.h" // For AffineTransform

#ifdef SVG_SUPPORT_FOREIGNOBJECT
#include "modules/svg/SVGManager.h"
#endif

#ifdef DRAG_SUPPORT
#include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT

#ifdef SELFTEST
/*static*/
void ST_CoreViewHacks::FakeCoreViewSize(CoreView *view, int w, int h)
{
	view->m_width = w;
	view->m_height = h;
}
#endif // SELFTEST

//Will display debug rectangle around all CoreViews
//#define DEBUG_COREVIEW

CoreView::CoreView()
	: m_parent(NULL)
	, m_reference_doc(NULL)
	, m_reference_elm(NULL)
	, m_paint_listener(NULL)
	, m_mouse_listener(NULL)
#ifdef TOUCH_EVENTS_SUPPORT
	, m_touch_listener(NULL)
#endif // TOUCH_EVENTS_SUPPORT
#ifdef DRAG_SUPPORT
	, m_drag_listener(NULL)
#endif
	, m_owning_element(NULL)
	, m_fixed_position_subtree(NULL)
{
	m_width = m_height = 0;

	packed5_init = 0;
	packed5.want_paint_events = TRUE;
	packed5.want_mouse_events = TRUE;
}

CoreView::~CoreView()
{
#ifndef MOUSELESS
	CoreViewContainer* container = GetContainer();
	while (container)
	{
		if (this == container->m_captured_view)
			container->m_captured_view = NULL;
		if (this == container->m_hover_view)
			container->m_hover_view = NULL;
		if (this == container->m_drag_view)
			container->m_drag_view = NULL;

		container = container->m_parent ? container->m_parent->GetContainer() : NULL;
	}
#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
	ReleaseTouchCapture(TRUE);
#endif // TOUCH_EVENTS_SUPPORT

#ifdef DRAG_SUPPORT
	if (m_drag_listener && g_drag_manager->GetOriginDragListener())
	{
		if (m_drag_listener == g_drag_manager->GetOriginDragListener())
			g_drag_manager->SetOriginDragListener(NULL);
	}
#endif // DRAG_SUPPORT

	Out();
	OP_ASSERT(m_children.First() == NULL);
	// The view can be destroyed long before the scroll listeners
	m_scroll_listeners.RemoveAll();
	if(this == pending_release_capture)
		pending_release_capture = NULL;
}

/*static */OP_STATUS CoreView::Create(CoreView** view, CoreView* parent)
{
	*view = OP_NEW(CoreView, ());
	if (!*view)
		return OpStatus::ERR;

	OP_STATUS status = (*view)->Construct(parent);

	if (OpStatus::IsError(status))
	{
		OP_DELETE(*view);
		*view = NULL;
	}
	return status;
}

OP_STATUS CoreView::Construct(CoreView* parent)
{
	m_parent = parent;
	Into(&parent->m_children);
	return OpStatus::OK;
}

void CoreView::Destruct()
{
	m_parent = NULL;
	Out();
}

void CoreView::IntoHierarchy(CoreView *parent)
{
	OpStatus::Ignore(Construct(parent));
}

void CoreView::OutFromHierarchy()
{
	Destruct();
}

void CoreView::SetVisibility(BOOL visible)
{
	if ((BOOL)packed5.visible != visible)
	{
		packed5.visible = visible;
#ifndef MOUSELESS
		if (!visible)
			ReleaseMouseCapture();
#endif // !MOUSELESS

		Invalidate(OpRect(0, 0, m_width, m_height));

		OnVisibilityChanged(visible);
	}
}

void CoreView::OnVisibilityChanged(BOOL visible)
{
	CoreView* child = (CoreView*) m_children.First();
	while(child)
	{
		child->OnVisibilityChanged(visible);
		child = (CoreView*) child->Suc();
	}
}

BOOL CoreView::GetVisibility()
{
	return packed5.visible;
}

void CoreView::InvalidateInternal(OpRect& rect)
{
	if (m_parent)
	{
		ConvertToParent(rect);
		m_parent->InvalidateInternal(rect);
	}
}

void CoreView::ConvertToParent(OpRect& r)
{
	m_pos.Apply(r);
}

void CoreView::ConvertToParent(int& x, int& y)
{
	OpPoint p(x, y);
	m_pos.Apply(p);
	x = p.x, y = p.y;
}

void CoreView::ConvertFromParent(OpRect& r)
{
	m_pos.ApplyInverse(r);
}

void CoreView::ConvertFromParent(int& x, int& y)
{
	OpPoint p(x, y);
	m_pos.ApplyInverse(p);
	x = p.x, y = p.y;
}

void CoreView::GetTransformToContainer(AffinePos& tm)
{
	CoreViewContainer* container = GetContainer();
	CoreView *tmp = this;
	while (tmp->m_parent && tmp != container)
	{
		tm.Prepend(tmp->m_pos);

		tmp = tmp->m_parent;
	}
}

void CoreView::GetTransformFromContainer(AffinePos& tm)
{
	GetTransformToContainer(tm);
	tm.Invert();
}

#ifdef CSS_TRANSFORMS
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#endif // CSS_TRANSFORMS

void CoreView::Paint(const OpRect& crect, OpPainter* painter, int translation_x, int translation_y, BOOL paint_containers, BOOL clip_to_view)
{
	if (!packed5.visible)
		return;

	// Don't paint if rect does not intersect this view
	OpRect lview_rect(0, 0, m_width, m_height);
	if (!crect.Intersecting(lview_rect))
		return;

	BOOL has_cliprect = FALSE;

	// Intersect rect so it is as small as possible (always inside the visible area of this view)
	OpRect rect = crect;
	if (clip_to_view)
	{
		rect.IntersectWith(lview_rect);
		if (rect.IsEmpty())
			return;

		// Set cliprect to this views area. We should never paint outside it.
		OpRect clip_rect = lview_rect;

		// Get _transform_ from container instead, then apply that to
		// clip, but _only_ if it is a simple translation - else set
		// the transform on the painter and leave the rect
		// alone. :((
		AffinePos ctm;
		GetTransformToContainer(ctm);

#ifdef CSS_TRANSFORMS
		VEGAOpPainter* vpainter = static_cast<VEGAOpPainter*>(painter);
		if (ctm.IsTransform())
		{
			vpainter->SetTransform(ctm.GetTransform().ToVEGATransform());
		}
		else
#endif // CSS_TRANSFORMS
		{
			clip_rect.OffsetBy(ctm.GetTranslation());
		}

		clip_rect.OffsetBy(translation_x, translation_y);

		has_cliprect = OpStatus::IsSuccess(painter->SetClipRect(clip_rect));

#ifdef CSS_TRANSFORMS
		vpainter->ClearTransform();
#endif // CSS_TRANSFORMS
	}

	if (m_paint_listener)
	{
		// Store old color since OnPaint will change it.
		UINT32 old_color = painter->GetColor();

		m_paint_listener->OnPaint(rect, painter, this, translation_x, translation_y);

		// Restore
		painter->SetColor(old_color);
	}

	// Paint childviews
	CoreView* child = (CoreView*) m_children.First();
	while(child)
	{
		if (child->packed5.want_paint_events)
		{
			OpRect child_rect = rect;
			child->ConvertFromParent(child_rect);
			child->Paint(child_rect, painter, translation_x, translation_y, paint_containers);
		}
		else if (paint_containers && child->packed5.is_container)
		{
			// This will make ConvertToContainer relative to topmost container instead of
			// closest parentcontainer during paint, so that the paint offset will be correct.
			child->packed5.is_container = FALSE;

			OpRect child_rect = rect;
			child->ConvertFromParent(child_rect);
			child->Paint(child_rect, painter, translation_x, translation_y, paint_containers);

			child->packed5.is_container = TRUE;
		}
		child = (CoreView*) child->Suc();
	}

	// Restore cliprect
	if (has_cliprect)
		painter->RemoveClipRect();

#ifdef DEBUG_COREVIEW
	OpRect debug_rect(0, 0, m_width, m_height);
	ConvertToContainer(debug_rect.x, debug_rect.y);
	painter->SetColor(packed5.is_container ? OP_RGB(255, 0, 255) : OP_RGB(255, 128, 0));
	painter->DrawRect(debug_rect);
#endif
}

BOOL CoreView::CallBeforePaint()
{
	BOOL painted = TRUE;
	if (m_paint_listener && GetVisibility() && m_width > 0 && m_height > 0)
		painted = m_paint_listener->BeforePaint();

	// Call BeforePaint on child CoreView's that are not containers. (Containers get separate BeforePaint() from the system)

	CoreView* child = (CoreView*) m_children.First();
	while(child)
	{
		if (!child->packed5.is_container)
		{
			if (!child->CallBeforePaint())
				painted = FALSE;
		}
		child = (CoreView*) child->Suc();
	}
	return painted;
}

void CoreView::Invalidate(const OpRect& rect)
{
	OpRect trect = rect;
	trect.IntersectWith(OpRect(0, 0, m_width, m_height));
	if (trect.IsEmpty())
		return;

#ifdef SVG_SUPPORT_FOREIGNOBJECT
	if (m_reference_doc && GetVisibility())
	{
		g_svg_manager->HandleInlineChanged(m_reference_doc, m_reference_elm, TRUE);
		return;
	}
#endif

	CoreViewContainer* container = GetContainer();
	CoreView *tmp = this;
	while (tmp->m_parent && tmp != container && tmp->m_parent->GetVisibility())
		tmp = tmp->m_parent;

	/* Invalidate makes sense only if everything up to the container is already
	   visible. */
	if (tmp == container)
		InvalidateInternal(trect);
}

OpPainter* CoreView::GetPainter(const OpRect &rect, BOOL nobackbuffer)
{
	OpRect trect = rect;
	ConvertToParent(trect);
	return m_parent->GetPainter(trect, nobackbuffer);
}

void CoreView::ReleasePainter(const OpRect &rect)
{
	OpRect trect = rect;
	ConvertToParent(trect);
	m_parent->ReleasePainter(trect);
}

void CoreView::ConvertToContainer(int &x, int &y)
{
	CoreViewContainer* container = GetContainer();
	CoreView *tmp = this;
	while(tmp->m_parent && tmp != container)
	{
		tmp->ConvertToParent(x, y);
		tmp = tmp->m_parent;
	}
}

void CoreView::ConvertFromContainer(int &x, int &y)
{
	CoreViewContainer* container = GetContainer();
	CoreView *tmp = this;
	while(tmp->m_parent && tmp != container)
	{
		tmp->ConvertFromParent(x, y);
		tmp = tmp->m_parent;
	}
}

OpView* CoreView::GetOpView()
{
	return m_parent ? m_parent->GetOpView() : NULL;
}

CoreViewContainer* CoreView::GetContainer()
{
	CoreView *tmp = this;
	while(tmp)
	{
		if (tmp->packed5.is_container)
			return (CoreViewContainer*)tmp;
		tmp = tmp->m_parent;
	}
	return NULL;
}

void CoreView::OnMoved()
{
	CoreView* child = (CoreView*) m_children.First();
	while(child)
	{
		child->OnMoved();
		child = (CoreView*) child->Suc();
	}

	if (m_paint_listener)
		m_paint_listener->OnMoved();
}

void CoreView::OnResized()
{
	CoreView* child = (CoreView*) m_children.First();
	while(child)
	{
		child->OnResized();
		child = (CoreView*) child->Suc();
	}
}

void CoreView::SetPos(const AffinePos& pos, BOOL onmove, BOOL invalidate)
{
	if (m_pos != pos)
	{
		if (invalidate)
			Invalidate(OpRect(0, 0, m_width, m_height)); // FIX

		m_pos = pos;

		if (invalidate)
			Invalidate(OpRect(0, 0, m_width, m_height)); // FIX
	}
	// Must be done even when position doesn't change because when windows adds the menu to the mainwindow
	// it will offset the coordinatesystem so the containerviews must be moved down without changing their x and y.
	if (onmove)
		OnMoved();
}

void CoreView::GetPos(AffinePos* pos)
{
	*pos = m_pos;
}

void CoreView::SetSize(int w, int h)
{
	if (m_width != w || m_height != h)
	{
		Invalidate(OpRect(0, 0, m_width, m_height)); // FIX
		m_width = w;
		m_height = h;
		Invalidate(OpRect(0, 0, m_width, m_height)); // FIX

		OnResized();
	}
}

void CoreView::GetSize(int* w, int* h)
{
	*w = m_width;
	*h = m_height;
}

/*virtual*/
OpRect CoreView::GetScreenRect()
{
	OpRect rect = GetVisibleRect();
	if (rect.IsEmpty())
		return rect;

	OpPoint top_left(rect.x, rect.y);
	top_left = GetContainer()->ConvertToScreen(top_left);
	rect.x = top_left.x;
	rect.y = top_left.y;

	return rect;
}

OpRect CoreView::GetExtents() const
{
	OpRect extents(0, 0, m_width, m_height);
	m_pos.Apply(extents);
	return extents;
}

void CoreView::MoveChildren(INT32 dx, INT32 dy, BOOL onmove)
{
	// Move children
	CoreView* child = (CoreView*) m_children.First();
	while(child)
	{
		if (!child->GetFixedPosition())
		{
			AffinePos new_pos = child->m_pos;
			new_pos.PrependTranslation(dx, dy);
			child->SetPos(new_pos, onmove, FALSE);
		}
		child = (CoreView*) child->Suc();
	}
}

OpRect CoreView::GetVisibleRect(BOOL skip_intersection_with_container /* = FALSE */)
{
	OpRect trect(0, 0, m_width, m_height);
	CoreView* tmp = this;

	if (tmp->m_parent)
	{
		CoreView* container = GetContainer();

		while (tmp != container)
		{
			tmp->m_pos.Apply(trect);

			if ( !(skip_intersection_with_container && tmp->m_parent == container) )
				trect.IntersectWith(OpRect(0, 0, tmp->m_parent->m_width, tmp->m_parent->m_height));

			if (trect.IsEmpty())
				break;

			tmp = tmp->m_parent;
		}
	}

	return trect;
}

void CoreView::ScrollRect(const OpRect &rect, INT32 dx, INT32 dy)
{
	MoveChildren(dx, dy, TRUE);

	if (GetIsOverlapped())
		Invalidate(rect);
	else
	{
		// Intersect rect with the visible rect of this CoreView in the container.
		// The scroll that area on the container.

		OpRect scroll_rect = rect;
		ConvertToContainer(scroll_rect.x, scroll_rect.y); // FIXME: position vs. size

		OpRect visible_rect = GetVisibleRect();

		scroll_rect.IntersectWith(visible_rect);

		if (!scroll_rect.IsEmpty())
			GetContainer()->ScrollRect(scroll_rect, dx, dy);
	}
}

void CoreView::Scroll(INT32 dx, INT32 dy)
{
	ScrollRect(OpRect(0, 0, m_width, m_height), dx, dy);
}

void CoreView::Sync()
{
	GetContainer()->Sync();
}

void CoreView::LockUpdate(BOOL lock)
{
	// We don't lock the OpView from here. That would make childviews lock the parentviews which might be bad.
	// Only CoreViewContainer::LockUpdate lock the OpView.
}

OpPoint CoreView::ConvertToScreen(const OpPoint &point)
{
	OpPoint p = point;
	ConvertToContainer(p.x, p.y);
	return GetOpView()->ConvertToScreen(p);
}

ShiftKeyState CoreView::GetShiftKeys()
{
	return GetOpView()->GetShiftKeys();
}

OpRect CoreView::ConvertFromScreen(const OpRect &rect)
{
	OpPoint top_left_cntr = GetContainer()->ConvertFromScreen(OpPoint(rect.x, rect.y));
	OpRect trect(top_left_cntr.x, top_left_cntr.y, rect.width, rect.height);

	AffinePos transform_from_cntr;
	GetTransformFromContainer(transform_from_cntr);
	transform_from_cntr.Apply(trect);

	return trect;
}

#ifndef MOUSELESS

#ifdef SVG_SUPPORT_FOREIGNOBJECT
OpPoint CoreView::Redirect(const OpPoint& p)
{
	OP_ASSERT(m_reference_doc);
	OpPoint tp = p;
	g_svg_manager->TransformToElementCoordinates(m_reference_elm, m_reference_doc, tp.x, tp.y);
	return tp;
}

BOOL CoreView::IsInRedirectChain() const
{
	const CoreView* cv = this;
	while (cv)
	{
		if (cv->IsRedirected())
			return TRUE;

		cv = cv->m_parent;
	}
	return FALSE;
}

OpPoint CoreView::ApplyRedirection(const OpPoint& p)
{
	CoreView* cv = this;
	while (cv)
	{
		if (cv->IsRedirected())
			return cv->Redirect(p);

		cv = cv->m_parent;
	}
	return p;
}
#endif // SVG_SUPPORT_FOREIGNOBJECT

OpPoint CoreView::ToLocal(const OpPoint& p)
{
#ifdef SVG_SUPPORT_FOREIGNOBJECT
	if (IsInRedirectChain())
		return ApplyRedirection(p);
#endif // SVG_SUPPORT_FOREIGNOBJECT

	OpPoint lpoint = p;
	ConvertFromContainer(lpoint.x, lpoint.y);
	return lpoint;
}

CoreView *CoreView::GetIntersectingChild(int x, int y)
{
	CoreView* tmp = (CoreView *) m_children.Last();
	while(tmp)
	{
		OpPoint p(x, y);
		if (tmp->packed5.visible && tmp->packed5.want_mouse_events)
		{
#ifdef SVG_SUPPORT_FOREIGNOBJECT
			if (tmp->IsRedirected())
				p = tmp->Redirect(p);
#endif // SVG_SUPPORT_FOREIGNOBJECT

			tmp->m_pos.ApplyInverse(p);

			if (tmp->GetHitStatus(p.x, p.y))
				return tmp;
		}

		tmp = (CoreView *) tmp->Pred();
	}
	return NULL;
}

void CoreView::UpdateOverlappedStatusRecursive()
{
#ifndef MOUSELESS
	if (m_mouse_listener)
		m_mouse_listener->UpdateOverlappedStatus(this);
#endif // !MOUSELESS

#ifdef TOUCH_EVENTS_SUPPORT
	if (m_touch_listener)
		m_touch_listener->UpdateOverlappedStatus(this);
#endif // TOUCH_EVENTS_SUPPORT

	CoreView *child = (CoreView *) m_children.First();
	while (child)
	{
		child->UpdateOverlappedStatusRecursive();
		child = (CoreView *) child->Suc();
	}
}
#endif // !MOUSELESS

#ifndef MOUSELESS
void CoreView::ReleaseMouseCapture()
{
	CoreViewContainer* container = GetContainer();

	if (this == container->m_captured_view)
		container->m_captured_view = NULL;

#ifdef TOUCH_EVENTS_SUPPORT
	ReleaseTouchCapture();
#endif // TOUCH_EVENTS_SUPPORT
}

CoreView *CoreView::GetMouseHitView(int x, int y)
{
	OpPoint p(x, y);
#ifdef SVG_SUPPORT_FOREIGNOBJECT
	if (IsRedirected())
		p = Redirect(p);
#endif // SVG_SUPPORT_FOREIGNOBJECT

	CoreView* candidate = GetIntersectingChild(p.x, p.y);

	if (candidate && m_mouse_listener)
	{
		// We have a intersecting child, but does it really intersect?
		// Check with the listener, so eventual z-indexed or overlapped iframes could be handled.
		candidate = m_mouse_listener->GetMouseHitView(p, this);
	}

	if (candidate)
	{
		candidate->ConvertFromParent(p.x, p.y);

		// Recurse
		CoreView* candidate_child = candidate->GetMouseHitView(p.x, p.y);
		if (candidate_child)
			return candidate_child;

		// this is the inner candidate
		return candidate;
	}
	return this;
}

BOOL CoreView::GetHitStatus(INT32 x, INT32 y)
{
	return OpRect(0, 0, m_width, m_height).Contains(OpPoint(x, y));
}

BOOL CoreView::GetMouseButton(MouseButton button)
{
	return GetOpView()->GetMouseButton(button);
}

#ifdef TOUCH_EVENTS_SUPPORT
void CoreView::SetMouseButton(MouseButton button, bool set)
{
	GetOpView()->SetMouseButton(button, set);
}
#endif // TOUCH_EVENTS_SUPPORT

void CoreView::GetMousePos(INT32* xpos, INT32* ypos)
{
// #ifdef SVG_SUPPORT_FOREIGNOBJECT
// #ifdef _DEBUG
// 	OP_ASSERT(!IsInRedirectChain());
// #endif
// #endif
	GetOpView()->GetMousePos(xpos, ypos);
	ConvertFromContainer(*xpos, *ypos);
}


void CoreView::MouseMove(const OpPoint &point, ShiftKeyState keystate)
{
	CoreViewContainer* container = GetContainer();
	CoreView *target;

	if (container->m_captured_view)
	{
		target = container->m_captured_view;
	}
	else
	{
		target = GetMouseHitView(point.x, point.y);
	}

	if (target != container->m_hover_view)
	{
		if (container->m_hover_view)
			container->m_hover_view->MouseLeave();

		container->m_hover_view = target;

		if (container->m_hover_view)
			container->m_hover_view->MouseSetCursor();
	}

	if (target && target->m_mouse_listener)
	{
		target->m_mouse_listener->OnMouseMove(target->ToLocal(point), keystate, target);
	}
}

void CoreView::MouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, ShiftKeyState keystate)
{
	CoreViewContainer* container = GetContainer();

	if (!container->m_captured_view)
	{
		container->m_captured_view = GetMouseHitView(point.x, point.y);
		container->m_captured_button = button;
	}
	CoreView *target = container->m_captured_view;

	if (target && target->m_mouse_listener)
	{
		target->m_mouse_listener->OnMouseDown(target->ToLocal(point), button, nclicks, keystate, target);
	}
}

void CoreView::MouseUp(const OpPoint &point, MouseButton button, ShiftKeyState keystate)
{
	CoreViewContainer* container = GetContainer();
	CoreView *target;

	pending_release_capture = container;

	if (container->m_captured_view)
	{
		target = container->m_captured_view;
	}
	else
	{
		target = GetMouseHitView(point.x, point.y);
	}

	BOOL release_capture = (button == container->m_captured_button);

	if (target && target->m_mouse_listener)
	{
		target->m_mouse_listener->OnMouseUp(target->ToLocal(point), button, keystate, target);
	}

	if (pending_release_capture && pending_release_capture == this && release_capture)
	{
		pending_release_capture->m_captured_view = NULL;
	}

	pending_release_capture = NULL ;
}

void CoreView::MouseLeave()
{
	if (m_mouse_listener)
		m_mouse_listener->OnMouseLeave();
}

void CoreView::MouseSetCursor()
{
	if (m_mouse_listener)
		m_mouse_listener->OnSetCursor();
}

BOOL CoreView::MouseWheel(INT32 delta, BOOL vertical, ShiftKeyState keystate)
{
	CoreViewContainer* container = GetContainer();
	INT32 x = container->m_mouse_move_point.x;
	INT32 y = container->m_mouse_move_point.y;
	ConvertFromContainer(x, y);

	CoreView *target = container->m_captured_view ? container->m_captured_view : GetMouseHitView(x, y);
	if (target)
	{
		BOOL handled = FALSE;
		while(target)
		{
			if (target->m_mouse_listener)
				handled = target->m_mouse_listener->OnMouseWheel(delta, vertical, keystate, target);
			if (handled || target == container)
				break;
			target = target->m_parent;
		}
		return handled;
	}
	return FALSE;
}
#endif // !MOUSELESS

void CoreView::NotifyScrollListeners(INT32 dx, INT32 dy, CoreViewScrollListener::SCROLL_REASON reason, BOOL parent)
{
	CoreViewScrollListener* listener = (CoreViewScrollListener*) m_scroll_listeners.First();
	while (listener)
	{
		if (parent)
			listener->OnParentScroll(this, dx, dy, reason);
		else
			listener->OnScroll(this, dx, dy, reason);
		listener = (CoreViewScrollListener*) listener->Suc();
	}

	CoreView* child = (CoreView*) m_children.First();
	while (child)
	{
		child->NotifyScrollListeners(dx, dy, reason, TRUE);
		child = (CoreView*) child->Suc();
	}
}

#ifdef TOUCH_EVENTS_SUPPORT
void CoreView::ReleaseTouchCapture(BOOL include_ancestor_containers /* = FALSE */)
{
	CoreViewContainer* container = GetContainer();

	while (container)
	{
		for (int i = 0; i < LIBGOGI_MULTI_TOUCH_LIMIT; i++)
			if (this == container->m_touch_captured_view[i])
				container->m_touch_captured_view[i] = NULL;

		if (include_ancestor_containers && container->m_parent)
			container = container->m_parent->GetContainer();
		else
			container = NULL;
	}
}

void CoreView::TouchDown(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, void *user_data)
{
	OP_ASSERT(id >= 0 && id <= LIBGOGI_MULTI_TOUCH_LIMIT);
	CoreViewContainer* container = GetContainer();

	if (!container->m_touch_captured_view[id])
		container->m_touch_captured_view[id] = GetMouseHitView(point.x, point.y);

	if (CoreView* target = container->m_touch_captured_view[id])
	{
		if (target->m_touch_listener)
			target->m_touch_listener->OnTouchDown(id, target->ToLocal(point), radius, modifiers, target, user_data);
		else if (id == 0) /* Let primary touch fall back to mouse handler. */
			MouseDown(point, MOUSE_BUTTON_1, 1, modifiers);
	}
}

void CoreView::TouchUp(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, void *user_data)
{
	OP_ASSERT(id >= 0 && id <= LIBGOGI_MULTI_TOUCH_LIMIT);
	CoreViewContainer* container = GetContainer();

	CoreView* target = container->m_touch_captured_view[id];
	if (!target)
		target = GetMouseHitView(point.x, point.y);

	if (target)
	{
		if (target->m_touch_listener)
			target->m_touch_listener->OnTouchUp(id, target->ToLocal(point), radius, modifiers, target, user_data);
		else if (id == 0) /* Let primary touch fall back to mouse handler. */
			MouseUp(point, MOUSE_BUTTON_1, modifiers);
	}

	container->m_touch_captured_view[id] = NULL;
}

void CoreView::TouchMove(int id, const OpPoint &point, int radius, ShiftKeyState modifiers, void *user_data)
{
	OP_ASSERT(id >= 0 && id <= LIBGOGI_MULTI_TOUCH_LIMIT);
	CoreViewContainer* container = GetContainer();

	CoreView* target = container->m_touch_captured_view[id];
	if (!target)
		container->m_touch_captured_view[id] = target = GetMouseHitView(point.x, point.y);

	if (target)
	{
		if (target->m_touch_listener)
			target->m_touch_listener->OnTouchMove(id, target->ToLocal(point), radius, modifiers, target, user_data);
		else if (id == 0) /* Let primary touch fall back to mouse handler. */
			MouseMove(point, modifiers);
	}
}
#endif // TOUCH_EVENTS_SUPPORT
