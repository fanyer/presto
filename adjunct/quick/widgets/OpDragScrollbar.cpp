/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "OpDragScrollbar.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/display/vis_dev.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#define DRAGSCROLLBAR_DRAG_SIZE 7
/***********************************************************************************
**
**	OpDragScrollbar - Combined drag bar and scroll bar.  Scrollbar is pending implementation.
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpDragScrollbar)

OpDragScrollbar::OpDragScrollbar() : 
		m_target(0),
		m_is_dragging(FALSE), 
		m_max_height(0), 
		m_min_height(0), 
		m_max_width(0), 
		m_min_width(0),
		m_drag_checker(50)
#ifdef VEGA_OPPAINTER_SUPPORT
		, m_ghost(NULL)
#endif // VEGA_OPPAINER_SUPPORT
{
	GetBorderSkin()->SetImage("Drag Scrollbar Skin", "Toolbar Skin");
	GetForegroundSkin()->SetImage("Drag Scrollbar Knob");

	SetSkinned(TRUE);

	SetAlignment(OpBar::ALIGNMENT_OFF);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityPrunesMe();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

}

/***********************************************************************************
**
**	~OpDragScrollbar
**
***********************************************************************************/

OpDragScrollbar::~OpDragScrollbar()
{
#ifdef VEGA_OPPAINTER_SUPPORT
	if (m_ghost)
		m_ghost->SetWidgetVisible(FALSE);
	OP_DELETE(m_ghost);
#endif // VEGA_OPPAINTER_SUPPORT
}

void OpDragScrollbar::GetRequiredSize(INT32& width, INT32& height)
{
	OpBar::Alignment alignment = GetAlignment();
	SkinType skin_type = SKINTYPE_DEFAULT;

	switch (alignment)
	{
		case OpBar::ALIGNMENT_LEFT:		skin_type = SKINTYPE_LEFT; break;
		case OpBar::ALIGNMENT_TOP:		skin_type = SKINTYPE_TOP; break;
		case OpBar::ALIGNMENT_RIGHT:	skin_type = SKINTYPE_RIGHT; break;
		case OpBar::ALIGNMENT_BOTTOM:	skin_type = SKINTYPE_BOTTOM; break;
	}
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Drag Scrollbar Knob", skin_type);
	if (skin_elm)
	{
		skin_elm->GetSize(&width, &height, 0);
#ifndef VEGA_OPPAINTER_SUPPORT
		if(alignment == OpBar::ALIGNMENT_TOP || alignment == OpBar::ALIGNMENT_BOTTOM)
		{
			if(!height)
			{
				// missing height, use some hard coded value
				height = 6;
			}
		}
		else
		{
			if(!width)
			{
				// missing height, use some hard coded value
				width = 6;
			}
		}
#endif // VEGA_OPPAINTER_SUPPORT
	}
#ifndef VEGA_OPPAINTER_SUPPORT
	else
	{
		if(alignment == OpBar::ALIGNMENT_TOP || alignment == OpBar::ALIGNMENT_BOTTOM)
		{
			height = 6;
		}
		else
		{
			width = 6;
		}
	}
#endif // VEGA_OPPAINTER_SUPPORT
}

OpRect OpDragScrollbar::LayoutToAvailableRect(const OpRect& rect, BOOL compute_rect_only)
{
	OpRect real_rect = rect;
	OpRect set_rect;
	INT32 width = 0, height = 0;

	GetRequiredSize(width, height);

	if (m_target && m_target->IsDragScrollbarEnabled())
	{
		if (GetAlignment() == OpBar::ALIGNMENT_TOP)
		{
			real_rect.height -= height;
			real_rect.y += height;

			if (!compute_rect_only)
			{
				set_rect = rect;
				set_rect.height = height;

				SetRect(set_rect);
			}
		}
		else if (GetAlignment() == OpBar::ALIGNMENT_BOTTOM)
		{
			real_rect.height -= height;

			if (!compute_rect_only)
			{
				set_rect = rect;
				set_rect.height = height;
				set_rect.y = real_rect.height + rect.y;

				SetRect(set_rect);
			}
		}
		else if (GetAlignment() == OpBar::ALIGNMENT_LEFT)
		{
			real_rect.width -= width;
			real_rect.x += width;

			if (!compute_rect_only)
			{
				set_rect = rect;
				set_rect.width = width;

				SetRect(set_rect);
			}
		}
		else if (GetAlignment() == OpBar::ALIGNMENT_RIGHT)
		{
			real_rect.width -= width;

			if (!compute_rect_only)
			{
				set_rect = rect;
				set_rect.width = width;
				set_rect.x = real_rect.width + rect.x;

				SetRect(set_rect);
			}
		}
	}
	else
	{
		// set empty rect
		SetRect(set_rect);
	}
	return real_rect;
}

void OpDragScrollbar::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpRect rect = GetBounds();

	// we don't want to let the OpScrollbar paint here
	if(!rect.IsEmpty())
	{
		EnsureSkin(rect);
		GetForegroundSkin()->Draw(vis_dev, rect);
	}
}

void OpDragScrollbar::SetAlignment(OpBar::Alignment alignment)
{
	m_alignment = alignment;

	SkinType skin_type = SKINTYPE_DEFAULT;

	switch (m_alignment)
	{
		case OpBar::ALIGNMENT_LEFT:		skin_type = SKINTYPE_LEFT; break;
		case OpBar::ALIGNMENT_TOP:		skin_type = SKINTYPE_TOP; break;
		case OpBar::ALIGNMENT_RIGHT:	skin_type = SKINTYPE_RIGHT; break;
		case OpBar::ALIGNMENT_BOTTOM:	skin_type = SKINTYPE_BOTTOM; break;
	}

	GetBorderSkin()->SetType(skin_type);
	GetForegroundSkin()->SetType(skin_type);

	m_max_height = m_min_height = 0;
	m_max_width = m_min_width = 0;

#ifdef VEGA_OPPAINTER_SUPPORT
	// Trigger the resize code when alignment changes to make sure the margins are updated
	INT32 w, h;
	OpRect b = GetBounds();
	w = b.width;
	h = b.height;
	OnResize(&w, &h);
#endif // VEGA_OPAINTER_SUPPORT
}

void OpDragScrollbar::UpdateMinMaxValues()
{
	OpBar::Alignment alignment = GetAlignment();

	if(alignment == OpBar::ALIGNMENT_TOP || alignment == OpBar::ALIGNMENT_BOTTOM)
	{
		if(!m_min_height || !m_max_height)
		{
			// if we don't have the maximum and minimal height already, get it now
			if(m_target)
			{
				m_target->GetMinMaxHeight(m_min_height, m_max_height);
			}
		}
	}
	else
	{
		if(!m_min_width || !m_max_width)
		{
			// if we don't have the maximum and minimal height already, get it now
			if(m_target)
			{
				m_target->GetMinMaxWidth(m_min_width, m_max_width);
			}
		}
	}
}

void OpDragScrollbar::EndDragging()
{
	if(m_is_dragging && m_target)
	{
		// Snap to min height if it's too little to display any thumbnail anyway

		OpBar::Alignment alignment = GetAlignment();
		if(alignment == OpBar::ALIGNMENT_TOP || alignment == OpBar::ALIGNMENT_BOTTOM)
		{
			UpdateMinMaxValues();

			OpRect pagebar_rect = m_target->GetWidgetSize();
			if (pagebar_rect.height <= m_target->GetMinHeightSnap())
			{
				pagebar_rect.height = m_min_height;
				m_target->SizeChanged(pagebar_rect);
			}
		}

		m_target->EndDragging();
	}
	m_is_dragging = FALSE;
}

/*
void Win32DebugDragLogging(const uni_char *x, ...)
{
	uni_char *tmp = new uni_char[500];
	int num = 0;
	va_list ap;

	va_start( ap, x );

	if(tmp)
	{
		num = uni_vsnprintf( tmp, 499, x, ap );
		if(num != -1)
		{
			tmp[499] = '\0';
			OutputDebugString(tmp);
		}
		delete [] tmp;
	}
	va_end(ap);
}
*/
void OpDragScrollbar::OnMouseMove(const OpPoint &point)
{
	VisualDevice *vis_dev = GetVisualDevice();

	// check if we stopped the move by some other means (such as another window taking focus)
	if (vis_dev && !vis_dev->GetView()->GetMouseButton(MOUSE_BUTTON_1) 
#ifdef VEGA_OPPAINTER_SUPPORT
		&& (!m_ghost || !m_ghost->IsLeftButtonPressed())
#endif // VEGA_OPPAINTER_SUPPORT
		)
	{
		EndDragging();
	}
	if(m_is_dragging)
	{
		// try to reduce the number of updates of the size we pass on for performance reasons
		if(!m_drag_checker.CanNotifyAboutDrag(point))
		{
			return;
		}
		UpdateMinMaxValues();

		if(m_target)
		{
			OpRect target_rect = m_target->GetWidgetSize();
			OpBar::Alignment alignment = GetAlignment();

//			Win32DebugDragLogging(UNI_L("point.x: %d, width: %d\n"), point.x, target_rect.width);

			// modify the height based on the mouse position, but stay within the boundaries
			if (alignment == OpBar::ALIGNMENT_TOP)
			{
				target_rect.height += point.y - m_mouse_down_point.y;
			}
			else if (alignment == OpBar::ALIGNMENT_BOTTOM)
			{
				target_rect.height -= point.y - m_mouse_down_point.y;
			}
			else if (alignment == OpBar::ALIGNMENT_LEFT)
			{
				target_rect.width += point.x - m_mouse_down_point.x;
			}
			else if (alignment == OpBar::ALIGNMENT_RIGHT)
			{
				target_rect.width -= point.x - m_mouse_down_point.x;
			}
			if (alignment == OpBar::ALIGNMENT_TOP || alignment == OpBar::ALIGNMENT_BOTTOM)
			{
				if(target_rect.height < m_min_height)
				{
					target_rect.height = m_min_height;
				}
				else if(target_rect.height > m_max_height)
				{
					target_rect.height = m_max_height;
				}
			}
			else
			{
				if(target_rect.width < m_min_width)
				{
					target_rect.width = m_min_width;
				}
				else if(target_rect.width > m_max_width)
				{
					target_rect.width = m_max_width;
				}
			}
			m_target->SizeChanged(target_rect);
		}
	}
}

void OpDragScrollbar::OnMouseLeave()
{
	Invalidate(GetBounds());
}

void OpDragScrollbar::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	m_mouse_down_point = point;
	if ((button == MOUSE_BUTTON_1 && nclicks == 2) || button == MOUSE_BUTTON_3)
	{
		OpBar::Alignment alignment = GetAlignment();

		// only support expand/shrink when placement is top or bottom
		if(alignment == OpBar::ALIGNMENT_TOP || alignment == OpBar::ALIGNMENT_BOTTOM)
		{
			UpdateMinMaxValues();

			if(m_target)
			{
				// get the current size of the target
				OpRect pagebar_rect = m_target->GetWidgetSize();

				// if we're not at the minimum, make it so.  If we're at the minimum, switch to maximum
				if(pagebar_rect.height > m_min_height)
				{
					pagebar_rect.height = m_min_height;
				}
				else
				{
					pagebar_rect.height = m_max_height;
				}
				m_target->SizeChanged(pagebar_rect);
			}
		}
	}
	else if (button != MOUSE_BUTTON_1)
		return;

	if (nclicks == 1 )
		m_is_dragging = TRUE;

	OpWidget::OnMouseDown(point, button, nclicks);
}

void OpDragScrollbar::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button != MOUSE_BUTTON_1)
		return;

	EndDragging();

	OpWidget::OnMouseUp(point, button, nclicks);
}

void OpDragScrollbar::OnSetCursor(const OpPoint &point)
{
	if (GetAlignment() == OpBar::ALIGNMENT_TOP || GetAlignment() == OpBar::ALIGNMENT_BOTTOM)
	{
		SetCursor(CURSOR_HOR_SPLITTER);
	}
	else if (GetAlignment() == OpBar::ALIGNMENT_LEFT || GetAlignment() == OpBar::ALIGNMENT_RIGHT)
	{
		SetCursor(CURSOR_VERT_SPLITTER);
	}
}

void OpDragScrollbar::EnableTransparentSkin(BOOL enable)
{
	if (enable)
		GetBorderSkin()->SetImage("Drag Scrollbar Transparent Skin", "Toolbar Transparent Skin");
	else
		GetBorderSkin()->SetImage("Drag Scrollbar Skin", "Toolbar Skin");
}

// these are methods to be overridden in the future
/*
BOOL OpDragScrollbar::OnScroll(INT32 delta, BOOL vertical, BOOL smooth)
{
	return OpScrollbar::OnScroll(delta, vertical, smooth);
}

BOOL OpDragScrollbar::OnMouseWheel(INT32 delta,BOOL vertical)
{
	return OpScrollbar::OnMouseWheel(delta, vertical);
}
*/

#ifdef VEGA_OPPAINTER_SUPPORT
void OpDragScrollbar::OnResize(INT32* new_w, INT32* new_h)
{
	OpScrollbar::OnResize(new_w, new_h);
	if (m_ghost)
	{
		switch (m_alignment)
		{
		case OpBar::ALIGNMENT_LEFT:
		case OpBar::ALIGNMENT_RIGHT:
			m_ghost->SetMargins((DRAGSCROLLBAR_DRAG_SIZE-(*new_w))/2, 0);
			break;
		case OpBar::ALIGNMENT_TOP:
		case OpBar::ALIGNMENT_BOTTOM:
			m_ghost->SetMargins(0, (DRAGSCROLLBAR_DRAG_SIZE-(*new_h))/2);
			break;
		}
		m_ghost->WidgetRectChanged();
	}
}

void OpDragScrollbar::OnShow(BOOL show)
{
	OpScrollbar::OnShow(show);
	// Add or remove m_ghost
	if (show)
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

void OpDragScrollbar::OnMove()
{
	OpScrollbar::OnMove();
	if (m_ghost)
		m_ghost->WidgetRectChanged();
}
#endif // VEGA_OPPAINTER_SUPPORT


