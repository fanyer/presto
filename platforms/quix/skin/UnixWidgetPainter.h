/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef UNIX_WIDGET_PAINTER_H
#define UNIX_WIDGET_PAINTER_H

#include "modules/skin/IndpWidgetPainter.h"
#include "platforms/quix/toolkits/ToolkitWidgetPainter.h"

class UnixWidgetPainter : public IndpWidgetPainter
{
public:
	UnixWidgetPainter(ToolkitWidgetPainter* toolkit_painter);
	virtual ~UnixWidgetPainter();

	virtual BOOL DrawScrollbar(const OpRect &drawrect);

	virtual BOOL DrawSlider(const OpRect& rect, BOOL horizontal, double min, double max, double pos, BOOL highlighted, BOOL pressed_knob, OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track);

	virtual OpWidgetInfo* GetInfo(OpWidget* widget);

	/** Get a scrollbar (owned by this UnixWidgetPainter) for a widget
	  */
	ToolkitScrollbar* GetScrollbarForWidget(OpWidget* widget);

	/** Get a slider (owned by this UnixWidgetPainter) for a widget
	  */
	ToolkitSlider* GetSliderForWidget(OpWidget* widget);

	ToolkitWidgetPainter* GetToolkitPainter() { return m_toolkit_painter; }

private:
	ToolkitScrollbar::HitPart ConvertHitPartToToolkit(SCROLLBAR_PART_CODE part);
	OP_STATUS DrawWidget(ToolkitWidget* widget, const OpRect& cliprect);

	ToolkitWidgetPainter* m_toolkit_painter;
	OpWidgetInfo* m_widget_info;
	ToolkitScrollbar* m_scrollbar;
	ToolkitSlider* m_slider;
};

#endif // UNIX_WIDGET_PAINTER_H
