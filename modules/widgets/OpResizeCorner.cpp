/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpResizeCorner.h"

#include "modules/pi/OpWindow.h"

#include "modules/display/coreview/coreview.h"
#include "modules/display/vis_dev.h"
#include "modules/dochand/docman.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/doc/frm_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/skin/OpSkinManager.h"

void OpResizeCorner::OnBeforePaint()
{
#ifdef CSS_SCROLLBARS_SUPPORT
	const uni_char* host = NULL;
	if (vis_dev && vis_dev->GetDocumentManager()
		&& vis_dev->GetDocumentManager()->GetCurrentDoc()
		)
	{
		host = vis_dev->GetDocumentManager()->GetCurrentDoc()->GetHostName();
	}

	colors.SetEnabled(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::EnableScrollbarColors, host));
#endif
}

// == OpWindowResizeCorner ===========================================================

DEFINE_CONSTRUCT(OpWindowResizeCorner)

OpWindowResizeCorner::OpWindowResizeCorner()
	: window(NULL)
	, resizing(FALSE)
	, active(FALSE)
{
}

void OpWindowResizeCorner::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	widget_painter->DrawResizeCorner(GetBounds(), active);
}

#ifndef MOUSELESS
void OpWindowResizeCorner::OnSetCursor(const OpPoint &point)
{
	if (!active)
		OpWidget::OnSetCursor(point);

#if defined(_UNIX_DESKTOP_)	
	SetCursor(active ? CURSOR_SE_RESIZE : CURSOR_DEFAULT_ARROW);
#else
	CursorType cursor = active ? CURSOR_NW_RESIZE : CURSOR_DEFAULT_ARROW;
	SetCursor(CURSOR_DEFAULT_ARROW);	// Workaround. if(cursor != current_cursor) sometimes fails when it should not
	SetCursor(cursor);					// in Window::SetCursor, so this forces a cursorswitch.
#endif
}

void OpWindowResizeCorner::OnMouseMove(const OpPoint &point)
{
	if (resizing && window)
	{
		OpRect r = GetRect();

		AffinePos frame_pos;
		vis_dev->GetView()->GetPos(&frame_pos);

		int adjustment_x = point.x - down_point.x;
		int adjustment_y = point.y - down_point.y;

		OpPoint frame_pt = frame_pos.GetTranslation();

		window->SetClientSize(r.Right() + adjustment_x + frame_pt.x,
							  r.Bottom() + adjustment_y + frame_pt.y);
		window->Sync();
	}
}

void OpWindowResizeCorner::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (active)
	{
		down_point = point;
		resizing = TRUE;
	}
}

void OpWindowResizeCorner::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	resizing = FALSE;
}
#endif // !MOUSELESS

OpWidgetResizeCorner::OpWidgetResizeCorner(OpWidget* widget)
	: widget(widget)
	, resizing(FALSE)
	, down_width(0)
	, down_height(0)
	, original_width(-1)
	, original_height(-1)
	, has_horizontal_scrollbar(FALSE)
	, has_vertical_scrollbar(FALSE)
{
	OP_ASSERT(widget);
}

void OpWidgetResizeCorner::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
#ifdef SKIN_SUPPORT
	if (!HasScrollbars())
	{
		OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Element Resize Corner Skin");
		if (skin_elm)
		{
			skin_elm->GetSize(w, h, 0);
			if (*w > 0 && *h > 0)
				return;
		}
	}
#endif // SKIN_SUPPORT
	*w = INT32(GetInfo()->GetVerticalScrollbarWidth());
	*h = INT32(GetInfo()->GetHorizontalScrollbarHeight());
}

void OpWidgetResizeCorner::OnPaint(OpWidgetPainter* widget_painter, const OpRect& paint_rect)
{
	SCROLLBAR_TYPE scrollbar = SCROLLBAR_NONE;

	if (has_horizontal_scrollbar && has_vertical_scrollbar)
		scrollbar = SCROLLBAR_BOTH;
	else if (has_horizontal_scrollbar)
		scrollbar = SCROLLBAR_HORIZONTAL;
	else if (has_vertical_scrollbar)
		scrollbar = SCROLLBAR_VERTICAL;

#ifdef CSS_SCROLLBARS_SUPPORT
	if (colors.IsEnabled() && colors.IsModified() && scrollbar != SCROLLBAR_NONE)
		scrollbar = SCROLLBAR_CSS;
#endif // CSS_SCROLLBARS_SUPPORT

	widget_painter->DrawWidgetResizeCorner(paint_rect, IsActive(), scrollbar);
}

#ifndef MOUSELESS
void OpWidgetResizeCorner::OnSetCursor(const OpPoint& point)
{
	CursorType cursor = IsActive() ? CURSOR_NW_RESIZE : CURSOR_DEFAULT_ARROW;
	SetCursor(cursor);
}

void OpWidgetResizeCorner::OnMouseMove(const OpPoint& point)
{
	if (resizing)
	{
		OpPoint p = point;
		GetPosInDocument().Apply(p);

		int w = OpWidgetListener::NO_RESIZE;
		int h = OpWidgetListener::NO_RESIZE;

		if (widget->IsHorizontallyResizable())
			w = MAX(down_width + (p.x - down_point.x), original_width);
		if (widget->IsVerticallyResizable())
			h = MAX(down_height + (p.y - down_point.y), original_height);

		widget->OnResizeRequest(w, h);
	}
}

void OpWidgetResizeCorner::OnMouseDown(const OpPoint& point, MouseButton button, UINT8 nclicks)
{
	if (!IsActive())
		return;

	down_point = point;
	GetPosInDocument().Apply(down_point);

	widget->GetBorderBox(down_width, down_height);

	// Store original values, so we can use these as the minimum size.
	if (original_width == -1 && original_height == -1)
	{
		original_width = down_width;
		original_height = down_height;
	}

	resizing = TRUE;
}

void OpWidgetResizeCorner::OnMouseUp(const OpPoint& point, MouseButton button, UINT8 nclicks)
{
	resizing = FALSE;
}
#endif // !MOUSELESS

BOOL OpWidgetResizeCorner::IsActive() const
{
	return widget->IsResizable();
}
