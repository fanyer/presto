/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "platforms/quix/skin/UnixWidgetInfo.h"

#include "platforms/quix/skin/UnixWidgetPainter.h"
#include "platforms/quix/toolkits/ToolkitWidgetPainter.h"


BOOL UnixWidgetInfo::GetScrollbarPartHitBy(OpWidget *widget, const OpPoint &pt, SCROLLBAR_PART_CODE &hitPart)
{
	ToolkitScrollbar* scrollbar = m_widget_painter->GetScrollbarForWidget(widget);
	if (!scrollbar)
		return IndpWidgetInfo::GetScrollbarPartHitBy(widget, pt, hitPart);

	OpRect rect = widget->GetRect();

	switch (scrollbar->GetHitPart(pt.x, pt.y, rect.width, rect.height))
	{
		case ToolkitScrollbar::ARROW_SUBTRACT:
			hitPart = LEFT_OUTSIDE_ARROW; break;
		case ToolkitScrollbar::ARROW_ADD:
			hitPart = RIGHT_OUTSIDE_ARROW; break;
		case ToolkitScrollbar::TRACK_SUBTRACT:
			hitPart = LEFT_TRACK; break;
		case ToolkitScrollbar::TRACK_ADD:
			hitPart = RIGHT_TRACK; break;
		case ToolkitScrollbar::KNOB:
			hitPart = KNOB; break;
		case ToolkitScrollbar::NONE: /* fallthrough */
		default:
			hitPart = INVALID_PART; break;
	}

	return TRUE;
}


UINT32 UnixWidgetInfo::GetVerticalScrollbarWidth()
{
	if (m_widget_painter->GetToolkitPainter()->SupportScrollbar())
		return m_widget_painter->GetToolkitPainter()->GetVerticalScrollbarWidth();
	else
		return IndpWidgetInfo::GetVerticalScrollbarWidth();
}

UINT32 UnixWidgetInfo::GetHorizontalScrollbarHeight()
{
	if (m_widget_painter->GetToolkitPainter()->SupportScrollbar())
		return m_widget_painter->GetToolkitPainter()->GetHorizontalScrollbarHeight();
	else
		return IndpWidgetInfo::GetHorizontalScrollbarHeight();
}

INT32 UnixWidgetInfo::GetScrollbarFirstButtonSize()
{
	if (m_widget_painter->GetToolkitPainter()->SupportScrollbar())
		return m_widget_painter->GetToolkitPainter()->GetScrollbarFirstButtonSize();
	else
		return IndpWidgetInfo::GetScrollbarFirstButtonSize();
}

INT32 UnixWidgetInfo::GetScrollbarSecondButtonSize()
{
	if (m_widget_painter->GetToolkitPainter()->SupportScrollbar())
		return m_widget_painter->GetToolkitPainter()->GetScrollbarSecondButtonSize();
	else
		return IndpWidgetInfo::GetScrollbarSecondButtonSize();
}
