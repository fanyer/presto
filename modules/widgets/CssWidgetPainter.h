/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "modules/widgets/OpWidgetPainterManager.h"
#include "modules/widgets/OpWidget.h"

#ifndef CSS_WIDGETPAINTER_H
#define CSS_WIDGETPAINTER_H

/**	class CssWidgetPainter.
	The OpWidgetPainter that draw widgets in a simple nonskinned way, using the colors and properties that was set on the widgets from CSS.
	It is used as the fallbackpainter so it will paint things that other painters doesn't.
*/

class CssWidgetPainter : public OpWidgetPainter
{
public:
	virtual void InitPaint(VisualDevice* vd, OpWidget* widget);

	virtual BOOL CanHandleCSS() { return TRUE; }

	virtual OpWidgetInfo* GetInfo(OpWidget* widget = NULL);

	virtual BOOL DrawScrollbarDirection(const OpRect &drawrect, int direction, BOOL is_down, BOOL is_hovering);
	virtual BOOL DrawScrollbarBackground(const OpRect &drawrect);
	virtual BOOL DrawScrollbarKnob(const OpRect &drawrect, BOOL is_down, BOOL is_hovering);
	virtual BOOL DrawScrollbar(const OpRect &drawrect) {return FALSE;}

#ifdef PAGED_MEDIA_SUPPORT
	virtual BOOL DrawPageControlBackground(const OpRect &drawrect);
	virtual BOOL DrawPageControlButton(const OpRect& drawrect, ARROW_DIRECTION direction, BOOL is_down, BOOL is_hovering);
#endif // PAGED_MEDIA_SUPPORT

	virtual BOOL DrawDropdown(const OpRect &drawrect);
	virtual BOOL DrawDropdownButton(const OpRect &drawrect, BOOL is_down);

	virtual BOOL DrawListbox(const OpRect &drawrect);

	virtual BOOL DrawButton(const OpRect &drawrect, BOOL value, BOOL is_default);

	virtual BOOL DrawEdit(const OpRect &drawrect);
	
	virtual BOOL DrawDropDownHint(const OpRect &drawrect);

	virtual BOOL DrawMultiEdit(const OpRect &drawrect);

	virtual BOOL DrawRadiobutton(const OpRect &drawrect, BOOL value);

	virtual BOOL DrawCheckbox(const OpRect &drawrect, BOOL value);

	virtual BOOL DrawItem(const OpRect &drawrect, OpStringItem* item, BOOL is_listbox, BOOL is_focused_item, int indent);

	virtual BOOL DrawTreeViewRow(const OpRect &drawrect, BOOL focused) { return FALSE; }

	virtual BOOL DrawResizeCorner(const OpRect &drawrect, BOOL active);

	virtual BOOL DrawWidgetResizeCorner(const OpRect &drawrect, BOOL is_active, SCROLLBAR_TYPE scrollbar);

	virtual BOOL DrawProgressbar(const OpRect &drawrect, double fillpercent, INT32 progress_when_total_unknown, OpWidgetString* string, const char *empty_skin, const char *full_skin) { return FALSE; }

	virtual BOOL DrawPopupableString(const OpRect &drawrect, BOOL is_hovering_button);
	virtual BOOL DrawSlider(const OpRect& rect, BOOL horizontal, double min, double max, double pos, BOOL highlighted, BOOL pressed_knob, OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track);
	virtual BOOL DrawSliderFocus(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& focus_rect);
	virtual BOOL DrawSliderTrack(const OpRect& slider_rect, const OpRect& track_rect);
	virtual BOOL DrawSliderKnob(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& knob_rect);
	virtual BOOL DrawMonthView(const OpRect& drawrect, const OpRect& header_area);
	virtual BOOL DrawColorBox(const OpRect& drawrect, COLORREF color);

private:

	VisualDevice* vd;
	OpWidget* widget;

	UINT32 gray, lgray, dgray, dgray2, fdef, bdef, black;
	WIDGET_COLOR color;
	OpWidgetInfo info;

	void DrawArrow(const OpRect &drawrect, int direction);
	void DrawCheckmark(OpRect rect);
	void Draw3DBorder(UINT32 lcolor, UINT32 dcolor, const OpRect &drawrect);
	void DrawSunkenExternalBorder(const OpRect& rect);
	void DrawSunkenBorder(OpRect rect);
	void DrawFocusRect(const OpRect &drawrect);
	void DrawDirectionButton(OpRect rect, int direction, BOOL is_scrollbar, BOOL is_down);
};

#endif // CSS_WIDGETPAINTER_H
