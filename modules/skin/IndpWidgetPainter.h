/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef INDEPENDENT_WIDGETPAINTER_H
#define INDEPENDENT_WIDGETPAINTER_H

#include "modules/widgets/OpWidgetPainterManager.h"
#include "modules/widgets/OpWidget.h"

class OpSlider;

class IndpWidgetPainter : public OpWidgetPainter
{
public:
	virtual void InitPaint(VisualDevice* vd, OpWidget* widget);

	virtual BOOL CanHandleCSS() { return FALSE; }

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
	virtual BOOL DrawDropdownButton(const OpRect &drawrect, BOOL is_down) { return FALSE; }

	virtual BOOL DrawListbox(const OpRect &drawrect);

	virtual BOOL DrawButton(const OpRect &drawrect, BOOL value, BOOL is_default);

	virtual BOOL DrawEdit(const OpRect &drawrect);
	virtual BOOL DrawDropDownHint(const OpRect &drawrect);
	virtual BOOL DrawMultiEdit(const OpRect &drawrect) {return FALSE;}

	virtual BOOL DrawRadiobutton(const OpRect &drawrect, BOOL value);
	virtual BOOL DrawCheckbox(const OpRect &drawrect, BOOL value);

	virtual BOOL DrawItem(const OpRect &drawrect, OpStringItem* item, BOOL is_listbox, BOOL is_focused_item, int indent);

	virtual BOOL DrawResizeCorner(const OpRect &drawrect, BOOL active);

	virtual BOOL DrawWidgetResizeCorner(const OpRect &drawrect, BOOL is_active, SCROLLBAR_TYPE scrollbar);

#ifdef QUICK
	virtual BOOL DrawTreeViewRow(const OpRect &drawrect, BOOL focused);

	virtual BOOL DrawProgressbar(const OpRect &drawrect, double percent, INT32 progress_when_total_unknown, OpWidgetString* string, const char *empty_skin, const char *full_skin);
#endif

	virtual BOOL DrawPopupableString(const OpRect &drawrect, BOOL is_hovering_button);
	virtual BOOL DrawSlider(const OpRect& rect, BOOL horizontal, double min, double max, double pos, BOOL highlighted, BOOL pressed_knob, OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track);
	virtual BOOL DrawSliderFocus(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& focus_rect);
	virtual BOOL DrawSliderTrack(const OpRect& slider_rect, const OpRect& track_rect);
	virtual BOOL DrawSliderKnob(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& knob_rect);
	INT32 GetSliderState(OpSlider *slider);

protected:

	VisualDevice* vd;
	OpWidget* widget;

private:

	UINT32 fdef;
	WIDGET_COLOR color;

	void DrawFocusRect(const OpRect &drawrect);
};

#endif
