/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickScrollContainer.h"

#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/quick_toolkit/widgets/OpMouseEventProxy.h"
#include "modules/display/vis_dev.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/hardcore/opera/opera.h"
#include "modules/widgets/widgets_module.h"

class QuickScrollContainer::ScrollView : public OpMouseEventProxy
{
public:
	ScrollView(OpScrollbar* scrollbar) : m_scrollbar(scrollbar) {}
	virtual Type GetType() { return WIDGET_TYPE_SCROLLABLE; }
	virtual const char*	GetInputContextName() { return "Quick Scroll Container View"; }
	virtual BOOL OnMouseWheel(INT32 delta,BOOL vertical)
	{
		ShiftKeyState keystate = m_scrollbar->GetVisualDevice()->GetView()->GetShiftKeys();
		if (keystate & DISPLAY_SCALE_MODIFIER) // Scale
		{ // scaling key is pressed so do not scroll
			return FALSE;
		}
		// Value copied from display module to match web pages
		return m_scrollbar->OnScroll(delta * DISPLAY_SCROLLWHEEL_STEPSIZE, vertical);
	}
	virtual BOOL OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
	{
		// Don't send vertical scroll events to a horizontal scroll bar
		if (vertical != m_scrollbar->IsHorizontal())
			return m_scrollbar->OnScroll(delta, vertical, smooth);
		else
			return FALSE;
	}
	virtual BOOL IsScrollable(BOOL vertical)
	{
		return TRUE;
	}
	virtual void OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
	{
		SetFocus(FOCUS_REASON_MOUSE);
		OpMouseEventProxy::OnMouseDown(point, button, nclicks);
	}

	virtual BOOL OnInputAction(OpInputAction* action);

private:
	OpScrollbar* m_scrollbar;
};

BOOL QuickScrollContainer::ScrollView::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_PAGE_UP:
		case OpInputAction::ACTION_PAGE_DOWN:
		{
			// Find offset
			INT32 offset = m_scrollbar->GetValue();
			if (action->GetAction() == OpInputAction::ACTION_PAGE_UP)
			{
				offset -= m_scrollbar->big_step;
			}
			else
			{
				offset += m_scrollbar->big_step;
			}
			m_scrollbar->SetValue(offset);
			break;
		}
	}
	
	return OpWidget::OnInputAction(action);
}


QuickScrollContainer::QuickScrollContainer(Orientation orientation, Scrollbar scrollbar_behavior)
  : m_orientation(orientation)
  , m_scrollbar_behavior(scrollbar_behavior)
  , m_scrollbar(0)
  , m_canvas(0)
  , m_scrollview(0)
  , m_respect_scrollbar_margin(true)
  , m_listener(NULL)
{
}

QuickScrollContainer::~QuickScrollContainer()
{
	g_opera->widgets_module.RemoveExternalListener(this);
	
	if (m_scrollview && !m_scrollview->IsDeleted())
		m_scrollview->Delete();
}

OP_STATUS QuickScrollContainer::Init()
{
	g_opera->widgets_module.AddExternalListener(this);

	RETURN_IF_ERROR(OpScrollbar::Construct(&m_scrollbar, m_orientation == HORIZONTAL));
	m_scrollbar->UpdateSkinMargin();
	m_scrollbar->SetListener(this);

	m_scrollview = OP_NEW(ScrollView, (m_scrollbar));
	if (!m_scrollview)
		return OpStatus::ERR_NO_MEMORY;

	m_canvas = OP_NEW(OpMouseEventProxy, ());
	if (!m_canvas)
		return OpStatus::ERR_NO_MEMORY;
	m_scrollview->AddChild(m_canvas);

	m_scrollview->AddChild(m_scrollbar);

	return OpStatus::OK;
}

BOOL QuickScrollContainer::SetScroll(int offset, BOOL smooth_scroll)
{
	return m_scrollbar->OnScroll(offset, FALSE, smooth_scroll);
}

void QuickScrollContainer::ScrollToEnd()
{
	INT32 min, max;
	m_scrollbar->GetLimits(min, max);
	m_scrollbar->SetValue(max); // SetValueSmoothScroll() does not work expected.
}

void QuickScrollContainer::SetContent(QuickWidget* contents)
{
	m_content = contents;
	m_content->SetContainer(this);
	m_content->SetParentOpWidget(m_canvas);
}

bool QuickScrollContainer::IsScrollbarVisible() const
{
	return m_scrollbar->IsVisible() != FALSE;
}

unsigned QuickScrollContainer::GetScrollbarSize() const
{
	return m_orientation == HORIZONTAL
			? m_scrollbar->GetInfo()->GetHorizontalScrollbarHeight()
			: m_scrollbar->GetInfo()->GetVerticalScrollbarWidth();
}

OP_STATUS QuickScrollContainer::Layout(const OpRect& rect)
{
	m_scrollview->SetRect(rect, TRUE, FALSE);

	OpRect view_rect = rect;
	view_rect.x = view_rect.y = 0;
	view_rect = LayoutScrollbar(view_rect);

	// The canvas should be big enough for the nominal size of the content
	if (m_orientation == HORIZONTAL)
		view_rect.width = max(m_content->GetNominalWidth(), (unsigned)view_rect.width);
	else
		view_rect.height = max(m_content->GetNominalHeight(view_rect.width), (unsigned)view_rect.height);

	RETURN_IF_ERROR(m_content->Layout(view_rect));

	// Set the canvas to the scrolled position
	if (m_orientation == HORIZONTAL)
		view_rect.x = -m_scrollbar->GetValue();
	else
		view_rect.y = -m_scrollbar->GetValue();

	m_scrollview->SetChildRect(m_canvas, view_rect, TRUE, FALSE);

	return OpStatus::OK;
}

OpRect QuickScrollContainer::LayoutScrollbar(const OpRect& rect)
{
	// Determine scrollbar limits
	int content_width = rect.width;
	if (m_orientation == VERTICAL)
	{
		content_width -= m_scrollbar->GetInfo()->GetVerticalScrollbarWidth();
		content_width = max(content_width, 0);
	}
	int content_size = m_orientation == HORIZONTAL ? m_content->GetNominalWidth() : m_content->GetNominalHeight(content_width);
	int scrollview_size = m_orientation == HORIZONTAL ? rect.width : rect.height;

	m_scrollbar->SetLimit(0, content_size - scrollview_size, scrollview_size);
	// Copied from display module to match web pages
	m_scrollbar->SetSteps(DISPLAY_SCROLL_STEPSIZE, scrollview_size - 20);

	// Return original rect if we don't need a scrollbar
	bool needs_scrollbar = m_scrollbar_behavior != SCROLLBAR_NONE;

	if (m_scrollbar_behavior == SCROLLBAR_AUTO)
	{
		if (m_orientation == HORIZONTAL && (int)m_content->GetNominalWidth() <= rect.width)
			needs_scrollbar = false;
		else if ((int)m_content->GetNominalHeight(content_width) <= rect.height)
			needs_scrollbar = false;
	}

	if (!needs_scrollbar)
	{
		m_scrollbar->SetVisibility(FALSE);
		return rect;
	}

	m_scrollbar->SetVisibility(TRUE);

	// Calculate scrollbar rect
	short margin_left = 0, margin_top = 0, margin_right = 0, margin_bottom = 0;
	if (m_respect_scrollbar_margin)
		m_scrollbar->GetMargins(margin_left, margin_top, margin_right, margin_bottom);

	OpRect scrollbar_rect = rect;
	OpRect scrollview_rect = rect;

	if (m_orientation == HORIZONTAL)
	{
		scrollbar_rect.height = m_scrollbar->GetInfo()->GetHorizontalScrollbarHeight();
		scrollbar_rect.y = rect.height - scrollbar_rect.height;
		scrollview_rect.height -= scrollbar_rect.height + margin_top;
		scrollview_rect.height = max(scrollview_rect.height, 0);
	}
	else
	{
		scrollbar_rect.width = m_scrollbar->GetInfo()->GetVerticalScrollbarWidth();
		scrollbar_rect.x = rect.width - scrollbar_rect.width;
		scrollview_rect.width -= scrollbar_rect.width + margin_left;
		scrollview_rect.width = max(scrollview_rect.width, 0);
	}

	m_scrollview->SetChildRect(m_scrollbar, scrollbar_rect);

	return scrollview_rect;
}

void QuickScrollContainer::SetParentOpWidget(OpWidget* parent)
{
	if (parent)
		parent->AddChild(m_scrollview);
	else
		m_scrollview->Remove();
}

void QuickScrollContainer::Show()
{
	m_scrollview->SetVisibility(TRUE);
}

void QuickScrollContainer::Hide()
{
	m_scrollview->SetVisibility(FALSE);
}

BOOL QuickScrollContainer::IsVisible()
{
	return m_scrollview->IsVisible();
}

void QuickScrollContainer::SetEnabled(BOOL enabled)
{
	m_scrollview->SetEnabled(enabled);
}

void QuickScrollContainer::OnScroll(OpWidget *widget, INT32 old_val, INT32 new_val, BOOL caused_by_input)
{
	OpRect canvas_rect = m_canvas->GetRect();
	if (m_orientation == HORIZONTAL)
		canvas_rect.x = -new_val;
	else
		canvas_rect.y = -new_val;

	// Optimization: we do not invalidate the rect but use VisualDevice::ScrollRect()
	m_canvas->SetRect(canvas_rect, FALSE, FALSE);

	VisualDevice* vd = m_scrollview->GetVisualDevice();
	if (!vd)
		return;

	int delta = old_val - new_val;
	OpRect scroll_rect = m_scrollview->GetRect(TRUE);

	if (m_orientation == HORIZONTAL)
	{
		scroll_rect.height -= m_scrollbar->GetRect().height;
		vd->ScrollRect(scroll_rect, delta, 0);
	}
	else
	{
		int scrollbar_width = m_scrollbar->GetRect().width;
		scroll_rect.width -= scrollbar_width;
		if (UiDirection::Get() == UiDirection::RTL)
			scroll_rect.x += scrollbar_width;
		vd->ScrollRect(scroll_rect, 0, delta);
	}
	if (m_listener)
	{
		m_listener->OnContentScrolled(new_val, m_scrollbar->limit_min, m_scrollbar->limit_max);
	}
}

void QuickScrollContainer::OnFocus(OpWidget *widget, BOOL focus, FOCUS_REASON reason)
{
	if (!focus || reason == FOCUS_REASON_ACTIVATE)
		return;

	bool widget_in_canvas = false;
	for (OpWidget* parent = widget; parent; parent = parent->GetParent())
		if (parent == m_canvas)
		{
			widget_in_canvas = true;
			break;
		}

	if (widget_in_canvas && widget->ScrollOnFocus())
	{
		OpRect page_rect = m_scrollbar->GetScreenRect();
		OpRect widget_rect = widget->GetScreenRect();
		if (widget_rect.Bottom() > page_rect.Bottom())
			SetScroll(widget->GetRect().Bottom() - page_rect.height, FALSE);
		else if (widget_rect.Top() < page_rect.Top())
			SetScroll(widget->GetRect().Top(), FALSE);
	}
}

OpRect QuickScrollContainer::GetScreenRect() 
{ 
	return m_scrollview->GetScreenRect(); 
}

unsigned QuickScrollContainer::GetDefaultMinimumWidth()
{
	// If we're horizontal, should be big enough to fit at least the scrollbar
	if (m_orientation == HORIZONTAL)
		return m_scrollbar->GetKnobMinLength();

	// Otherwise depends on content and scrollbar width
	return AddScrollbarWidthTo(m_content->GetMinimumWidth());
}

unsigned QuickScrollContainer::GetDefaultMinimumHeight(unsigned width)
{
	// If we're vertical, should be big enough to fit at least the scrollbar
	if (m_orientation == VERTICAL)
		return m_scrollbar->GetKnobMinLength();

	// Otherwise depends on content and scrollbar height
	return AddScrollbarHeightTo(m_content->GetMinimumHeight(width));
}

unsigned QuickScrollContainer::GetDefaultNominalHeight(unsigned width)
{
	// If we're vertical, try to fit content
	if (m_orientation == VERTICAL)
		return m_content->GetNominalHeight(width);

	// Try to see if content will fit even with scrollbar
	unsigned min_height = AddScrollbarHeightTo(m_content->GetMinimumHeight(width));
	unsigned nom_height = m_content->GetNominalHeight(width);

	if (min_height <= nom_height)
		return nom_height;

	// Otherwise make sure that scrollbar will fit in
	return AddScrollbarHeightTo(m_content->GetNominalHeight(width));
}

unsigned QuickScrollContainer::GetDefaultNominalWidth()
{
	// If we're horizontal, try to fit content
	if (m_orientation == HORIZONTAL)
		return m_content->GetNominalWidth();

	// Try to see if content will fit even with scrollbar
	unsigned min_width = AddScrollbarWidthTo(m_content->GetMinimumWidth());
	unsigned nom_width = m_content->GetNominalWidth();

	if (min_width <= nom_width)
		return nom_width;

	// Otherwise make sure that scrollbar will fit in
	return AddScrollbarWidthTo(m_content->GetNominalWidth());
}

unsigned QuickScrollContainer::GetDefaultPreferredWidth()
{
	// Depends on content and scrollbar width
	return AddScrollbarWidthTo(m_content->GetPreferredWidth());
}

unsigned QuickScrollContainer::GetDefaultPreferredHeight(unsigned width)
{
	// Depends on content and scrollbar height
	return AddScrollbarHeightTo(m_content->GetPreferredHeight(width));
}

unsigned QuickScrollContainer::AddScrollbarWidthTo(unsigned width)
{
	if (width == WidgetSizes::Infinity || m_orientation == HORIZONTAL || m_scrollbar_behavior == SCROLLBAR_NONE)
		return width;

	if (m_respect_scrollbar_margin)
	{
		short margin_left, margin_top, margin_right, margin_bottom;
		m_scrollbar->GetMargins(margin_left, margin_top, margin_right, margin_bottom);
		width += margin_left;
	}

	return width + m_scrollbar->GetInfo()->GetVerticalScrollbarWidth();
}

unsigned QuickScrollContainer::AddScrollbarHeightTo(unsigned height)
{
	if (height == WidgetSizes::Infinity || m_orientation == VERTICAL || m_scrollbar_behavior == SCROLLBAR_NONE)
		return height;

	if (m_respect_scrollbar_margin)
	{
		short margin_left, margin_top, margin_right, margin_bottom;
		m_scrollbar->GetMargins(margin_left, margin_top, margin_right, margin_bottom);
		height += margin_top;
	}

	return height + m_scrollbar->GetInfo()->GetHorizontalScrollbarHeight();
}
