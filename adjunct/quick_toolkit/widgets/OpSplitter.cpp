/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "OpSplitter.h"
#include "OpWorkspace.h"

#include "modules/pi/OpWindow.h"

#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpButton.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/display/vis_dev.h"

#define SPLITTER_MIN_DRAG_SIZE 7

#ifdef VEGA_OPPAINTER_SUPPORT

DEFINE_CONSTRUCT(OpSplitterDragArea)

OpSplitterDragArea::OpSplitterDragArea() : m_ghost(NULL), m_splitter(NULL)
{}
OpSplitterDragArea::~OpSplitterDragArea()
{
	if (m_ghost)
		m_ghost->SetWidgetVisible(FALSE);
	OP_DELETE(m_ghost);
}

void OpSplitterDragArea::OnResize(INT32* new_w, INT32* new_h)
{
	OpWidget::OnResize(new_w, new_h);
	if (m_ghost)
		m_ghost->WidgetRectChanged();
}
void OpSplitterDragArea::OnShow(BOOL show)
{
	OpWidget::OnShow(show);
	UpdateGhost();
}
void OpSplitterDragArea::UpdateGhost()
{
	// Add or remove m_ghost
	if (IsAllVisible() && m_splitter && !m_splitter->IsFixed())
	{
		if (!m_ghost)
			m_ghost = OP_NEW(QuickGhostInputView, (this, this));
		if (m_ghost)
		{
			m_ghost->SetWidgetVisible(TRUE);
			SetOnMoveWanted(TRUE);
		}
	}
	else
	{
		if (m_ghost)
		{
			m_ghost->SetWidgetVisible(FALSE);
			SetOnMoveWanted(FALSE);
		}
	}
}
void OpSplitterDragArea::OnMove()
{
	if (m_ghost)
		m_ghost->WidgetRectChanged();
}

void OpSplitterDragArea::OnMouseMove(const OpPoint &point)
{
	if (m_splitter)
	{
		OpPoint p = point;
		p.x += GetRect().x;
		p.y += GetRect().y;
		m_splitter->OnMouseMove(p);
	}
}
void OpSplitterDragArea::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (m_splitter)
	{
		OpPoint p = point;
		p.x += GetRect().x;
		p.y += GetRect().y;
		m_splitter->OnMouseDown(p, button, nclicks);
	}
}
void OpSplitterDragArea::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (m_splitter)
	{
		OpPoint p = point;
		p.x += GetRect().x;
		p.y += GetRect().y;
		m_splitter->OnMouseUp(p, button, nclicks);
	}
}
void OpSplitterDragArea::OnSetCursor(const OpPoint &point)
{
	if (m_splitter)
	{
		OpPoint p = point;
		p.x += GetRect().x;
		p.y += GetRect().y;
		m_splitter->OnSetCursor(p);
	}
}

#endif // VEGA_OPPAINTER_SUPPORT

// == OpSplitter ===========================================================

DEFINE_CONSTRUCT(OpSplitter)

OpSplitter::OpSplitter()
	: m_split_division(0x8000)
	, m_split_size(3)
	, m_split_position(0)
	, m_child_count(0)
	, m_is_fixed(false)
	, m_is_horizontal(false)
	, m_is_dragging(false)
	, m_pixel_division(false)
	, m_reversed_pixel_division(false)
	, m_reversed(false)
	, m_divider_button(NULL)
#ifdef VEGA_OPPAINTER_SUPPORT
	, m_divider_drag_area(NULL)
#endif
{
	OP_STATUS status = OpButton::Construct(&m_divider_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
	CHECK_STATUS(status);

	m_divider_button->SetDead(TRUE);

#ifdef VEGA_OPPAINTER_SUPPORT
	status = OpSplitterDragArea::Construct(&m_divider_drag_area);
	CHECK_STATUS(status);
	m_divider_drag_area->SetSplitter(this);
	m_divider_drag_area->SetVisibility(FALSE);
	AddChild(m_divider_drag_area);
	m_split_size = 1;
#endif // VEGA_OPPAINER_SUPPORT

	SetDividerSkin("Splitter Horizontal Divider Skin");
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilitySkipsMe();
#endif
}

void OpSplitter::SetDividerSkin(const char* skin)
{
	m_divider_button->GetBorderSkin()->SetImage(skin);
}

OpSplitter::~OpSplitter()
{
	OP_DELETE(m_divider_button);
}

void OpSplitter::SetPixelDivision(bool pixel_division, bool reversed)
{
	if (m_pixel_division == pixel_division && m_reversed_pixel_division == reversed)
		return;

	m_pixel_division = pixel_division;
	m_reversed_pixel_division = reversed;
	Relayout();
}

void OpSplitter::SetReversed(bool reversed)
{
	if (m_reversed == reversed)
		return;

	m_reversed = reversed;
	Relayout();
}

void OpSplitter::SetDivision(INT32 division)
{
	if (m_split_division == division)
		return;

	m_split_division = division;
	Relayout();
}

void OpSplitter::SetHorizontal(bool is_horizontal)
{
	if (m_is_horizontal == is_horizontal)
		return;

	m_is_horizontal = is_horizontal;
	Relayout();
}

void OpSplitter::ComputeSplitPosition(INT32 width, INT32 height)
{
	int limit = m_is_horizontal ? width : height;

	if (m_pixel_division)
	{
		m_split_position = IsPixelDivisionReversed() ? limit - m_split_division : m_split_division;
	}
	else
	{
		m_split_position = ((limit * (IsReversed() ? (0x10000 - m_split_division) : m_split_division)) >> 16) - (m_split_size / 2);
	}

	if (m_split_position > limit - OPSPLITTER_MIN_SIZE)
	{
		m_split_position = limit - OPSPLITTER_MIN_SIZE;
	}

	if(m_split_position < OPSPLITTER_MIN_SIZE)
	{
		m_split_position = OPSPLITTER_MIN_SIZE;
	}
}

void OpSplitter::ComputeRealSplitPosition(INT32 available, INT32 min0, INT32 max0, INT32 min1, INT32 max1)
{
	available -= m_split_size;

	INT32 new_max0 = available - min1;
	INT32 new_min0 = available - max1;

	if (new_max0 < max0)
		max0 = new_max0;

	if (new_min0 > min0)
		min0 = new_min0;

	if (min0 > m_split_position)
		m_split_position = min0;

	if (max0 < m_split_position)
		m_split_position = max0;

	if (m_split_position >= available)
		m_split_position = available + m_split_size;
	else if (m_split_position <= 0)
		m_split_position = -m_split_size;
}

void OpSplitter::OnLayout()
{
	if (!GetWidgetContainer())
		return;

	OpRect rect = GetBounds();

	ComputeSplitPosition(rect.width, rect.height);

	OpWidget* children[2] = {NULL, NULL};

	m_child_count = 0;

	children[m_child_count] = IsReversed() ? GetLastChild() : GetFirstChild();

#ifdef VEGA_OPPAINTER_SUPPORT
	if (children[m_child_count] == m_divider_drag_area)
		children[m_child_count] = IsReversed() ? children[m_child_count]->GetPreviousSibling() : children[m_child_count]->GetNextSibling();
#endif // VEGA_OPPAINTER_SUPPORT

	if (children[m_child_count])
	{
		if (children[m_child_count]->IsVisible())
		{
			m_child_count++;
		}

		children[m_child_count] = IsReversed() ? children[0]->GetPreviousSibling() : children[0]->GetNextSibling();
#ifdef VEGA_OPPAINTER_SUPPORT
		if (children[m_child_count] == m_divider_drag_area)
			children[m_child_count] = IsReversed() ? children[m_child_count]->GetPreviousSibling() : children[m_child_count]->GetNextSibling();
#endif // VEGA_OPPAINTER_SUPPORT

		if (children[m_child_count] && children[m_child_count]->IsVisible())
		{
			m_child_count++;
		}
	}

#ifdef VEGA_OPPAINTER_SUPPORT
	BOOL drag_area_visible = FALSE;
#endif // VEGA_OPPAINTER_SUPPORT
	if (m_child_count == 2)
	{
#ifdef VEGA_OPPAINTER_SUPPORT
		drag_area_visible = TRUE;
#endif
		OpRect rect0 = rect;
		OpRect rect1 = rect0;

		OpWidgetLayout layout0(rect.width, rect.height);
		OpWidgetLayout layout1(rect.width, rect.height);

		children[0]->GetLayout(layout0);
		children[1]->GetLayout(layout1);

		if (m_is_horizontal)
		{
			INT32 min0 = layout0.GetMinWidth();
			INT32 max0 = layout0.GetMaxWidth();
			INT32 min1 = layout1.GetMinWidth();
			INT32 max1 = layout1.GetMaxWidth();

			SetFixed(min0 == max0 || min1 == max1);
			ComputeRealSplitPosition(rect.width, min0, max0, min1, max1);

			if(children[0]->GetIsResizable() != children[1]->GetIsResizable())
			{
				rect0.width = m_split_position;
				rect1.width -= m_split_position;
				rect1.x = m_split_position;
			}
			else
			{
				rect0.width = m_split_position;
				rect1.width -= m_split_position + m_split_size;
				rect1.x = m_split_position + m_split_size;
			}
		}
		else
		{
			INT32 min0 = layout0.GetMinHeight();
			INT32 max0 = layout0.GetMaxHeight();
			INT32 min1 = layout1.GetMinHeight();
			INT32 max1 = layout1.GetMaxHeight();

			SetFixed(min0 == max0 || min1 == max1);
			ComputeRealSplitPosition(rect.height, min0, max0, min1, max1);

			if(children[0]->GetIsResizable() != children[1]->GetIsResizable())
			{
				rect0.height = m_split_position;
				rect1.y = m_split_position;
				rect1.height -= m_split_position;
			}
			else
			{
				rect0.height = m_split_position;
				rect1.y = m_split_position + m_split_size;
				rect1.height -= m_split_position + m_split_size;
			}
		}
		if (RectsNotEqual(rect0, children[0]->GetRect()) || RectsNotEqual(rect1, children[1]->GetRect()))
		{
			GetParentDesktopWindow()->LockUpdate(TRUE);

			children[0]->SetRect(rect0, TRUE);
			children[1]->SetRect(rect1, TRUE);

			if(m_divider_button)
			{
				OpRect rect;

				// horizontal is about how the groups are laid out next to each other and not how the line is,
				// so for skinners is more logical to talk about how the line is
				if(m_is_horizontal)
				{
					// line is really vertical
					rect.x = rect1.x - m_split_size;
					rect.height = rect0.height;
					rect.width = m_split_size;
					rect.y = rect0.y;
				}
				else
				{
					// line is really horizonal
					rect.x = rect0.x;
					rect.height = m_split_size;
					rect.width = rect0.width;
					rect.y = rect1.y - m_split_size;
				}
				m_divider_button->SetRect(rect);
#ifdef VEGA_OPPAINTER_SUPPORT
				OpRect divider_area = rect;
				int split_padding = (SPLITTER_MIN_DRAG_SIZE-m_split_size)/2;
				if (m_is_horizontal)
				{
					divider_area.x -= split_padding;
					divider_area.width += split_padding*2;
				}
				else
				{
					divider_area.y -= split_padding;
					divider_area.height += split_padding*2;
				}
				m_divider_drag_area->SetRect(divider_area);
#endif // VEGA_OPPAINTER_SUPPORT
			}
			GetParentDesktopWindow()->LockUpdate(FALSE);
		}
	}
	else if (m_child_count == 1)
	{
		SetFixed(true);

		if (RectsNotEqual(rect, children[0]->GetRect()))
		{
			GetParentDesktopWindow()->LockUpdate(TRUE);

			children[0]->SetRect(rect, TRUE);

			GetParentDesktopWindow()->LockUpdate(FALSE);
		}
	}
#ifdef VEGA_OPPAINTER_SUPPORT
	m_divider_drag_area->SetVisibility(drag_area_visible);
#endif // VEGA_OPPAINTER_SUPPORT
}

void OpSplitter::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (m_child_count == 1)
		return;

	OpRect rect = GetBounds();

	if (m_child_count == 2)
	{
		if (m_is_horizontal)
		{
			rect.x = m_split_position;
			rect.width = m_split_size;
		}
		else
		{
			rect.y = m_split_position;
			rect.height = m_split_size;
		}
	}

	EnsureSkin(rect);

	if(m_divider_button->GetBorderSkin()->HasContent())
	{
		m_divider_button->GetBorderSkin()->Draw(GetVisualDevice(), m_divider_button->GetRect());
	}
}

void OpSplitter::OnMouseMove(const OpPoint &point)
{
	if (m_is_dragging)
	{
		OpRect rect = GetBounds();

		INT32 pos;
		UINT32 limit;

		if(m_is_horizontal)
		{
			pos = point.x;
			limit = rect.width;
		}
		else
		{
			pos = point.y;
			limit = rect.height;
		}

		if (m_pixel_division ? IsPixelDivisionReversed() : IsReversed())
			pos = limit - pos;

		if (pos < 0)
			pos = 0;

		if (m_pixel_division)
		{
			m_split_division = pos;
		}
		else
		{
			m_split_division = (pos << 16) / limit;
		}

		Relayout();
		SyncLayout();
		Sync();
	}
}

void OpSplitter::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button != MOUSE_BUTTON_1 || m_is_fixed)
		return;

	m_is_dragging = true;
}

void OpSplitter::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button != MOUSE_BUTTON_1)
		return;

	m_is_dragging = false;
}

void OpSplitter::OnSetCursor(const OpPoint &point)
{
	if (!m_is_fixed)
		SetCursor(m_is_horizontal ? CURSOR_VERT_SPLITTER : CURSOR_HOR_SPLITTER);
}

void OpSplitter::SetFixed(bool fixed)
{
	m_is_fixed = fixed;
	m_divider_drag_area->UpdateGhost();
}
