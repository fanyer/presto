/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpWidgetPainterManager.h"
#include "modules/widgets/CssWidgetPainter.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/widgets/OpScrollbar.h"

#ifdef SKIN_SUPPORT
# include "modules/skin/OpSkinManager.h"
# include "modules/layout/layoutprops.h"
#endif // SKIN_SUPPORT

//static
UINT32 OpWidgetPainter::GetColorInvRGB(UINT32 color, float f)
{
	// Calculate a brighter color if the set color is dark, and darker if it is bright.
	int r = OP_GET_R_VALUE(color);
	int g = OP_GET_G_VALUE(color);
	int b = OP_GET_B_VALUE(color);
	int a = OP_GET_A_VALUE(color);
	r = (int)(r * (1.f - f) + ((255 - r)) * f);
	g = (int)(g * (1.f - f) + ((255 - g)) * f);
	b = (int)(b * (1.f - f) + ((255 - b)) * f);
	return OP_RGBA(r, g, b, a);
}

//static
UINT32 OpWidgetPainter::GetColorMulAlpha(UINT32 color, float f)
{
	int r = OP_GET_R_VALUE(color);
	int g = OP_GET_G_VALUE(color);
	int b = OP_GET_B_VALUE(color);
	int a = OP_GET_A_VALUE(color);
	return OP_RGBA(r, g, b, (int)(a * f));
}

OpWidgetPainterManager::OpWidgetPainterManager()
{
	painter[0] = OP_NEW(CssWidgetPainter, ());
	painter[1] = painter[0];
	use_painter[0] = painter[0];
	use_painter[1] = painter[1];
	use_fallback = FALSE;
}

OpWidgetPainterManager::~OpWidgetPainterManager()
{
	if (painter[0] != painter[1])
		OP_DELETE(painter[0]);

    OP_DELETE(painter[1]);
}

void OpWidgetPainterManager::SetUseFallbackPainter(BOOL status)
{
	use_fallback = status;
}

void OpWidgetPainterManager::SetPrimaryWidgetPainter(OpWidgetPainter* widget_painter)
{
	if (painter[0] != painter[1])
		OP_DELETE(painter[0]);
	if (widget_painter == NULL)
		painter[0] = painter[1];
	else
		painter[0] = widget_painter;
}

BOOL OpWidgetPainterManager::NeedCssPainter(OpWidget* widget)
{
	if (use_fallback)
		return TRUE;

	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_SLIDER)
		// The CssWidgetPainter implementation ignore CSS anyway so let the primary painter
		// do the job if it can.
		return FALSE;

	if (widget->HasCssBorder() || widget->HasCssBackgroundImage() || !widget->GetColor().use_default_background_color)
		return TRUE;

//	if (widget->IsForm() && GET_PREF_INT(EnableSkinningOnForms) == FALSE)
//		return TRUE;

/*	if (widget->IsForm() && (widget->HasCssBorder() || widget->HasCssBackgroundImage() || !widget->GetColor().use_default_background_color))
		return TRUE;*/

	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR)
		if (((OpScrollbar*)widget)->scrollbar_colors.IsEnabled() &&
			((OpScrollbar*)widget)->scrollbar_colors.IsModified())
			return TRUE;

	return FALSE;
}

#ifdef SKIN_SUPPORT
static inline BOOL CanUseSkin(int skin_w, int skin_h, int avail_w, int avail_h)
{
	return avail_w >= skin_w && avail_h >= skin_h;
}
static BOOL GetSkinSize(OpWidget* widget, int& w, int& h)
{
	const char* skin = 0;
	switch (widget->GetType())
	{
	case OpWidget::WIDGET_TYPE_CHECKBOX:
		skin = "Checkbox Skin";
		break;
	case OpWidget::WIDGET_TYPE_RADIOBUTTON:
		skin = "Radio Button Skin";
		break;

	default:
		return FALSE;
	}

	OP_ASSERT(skin);
	widget->GetSkinManager()->GetSize(skin, &w, &h);
	return TRUE;
}
BOOL OpWidgetPainterManager::UseMargins(OpWidget* widget,
										short margin_left, short margin_top, short margin_right, short margin_bottom,
										UINT8& left, UINT8& top, UINT8& right, UINT8& bottom)
{
	if (NeedCssPainter(widget))
		return FALSE;

	int skin_w, skin_h;
	if (!GetSkinSize(widget, skin_w, skin_h))
		return FALSE;

	// cannot use negative margins to draw in
	if (margin_left < 0)
		margin_left = 0;
	if (margin_top < 0)
		margin_top = 0;
	if (margin_right < 0)
		margin_right = 0;
	if (margin_bottom < 0)
		margin_bottom = 0;

	OpRect available = widget->GetRect();
	if (CanUseSkin(skin_w, skin_h, available.width, available.height))
		return FALSE;

	if (!CanUseSkin(skin_w, skin_h, available.width + margin_left + margin_right, available.height + margin_top + margin_bottom))
		return FALSE;

	if (skin_w > available.width)
	{
		// distribute extra needed width evenly, but don't violate margin
		// bounds
		const int dw = skin_w - available.width;
		OP_ASSERT(dw <= margin_left + margin_right);
		left = dw / 2;
		if (left > margin_left)
			left = (UINT8)margin_left;
		right = dw - left;
		if (right > margin_right)
		{
			int dl = right - margin_right;
			left += dl;
			right -= dl;
		}
		OP_ASSERT(left + right == dw);
	}
	else
	{
		left = 0;
		right = 0;
	}

	if (skin_h > available.height)
	{
		// distribute extra needed height evenly, but don't violate margin
		// bounds
		const int dh = skin_h - available.height;
		OP_ASSERT(dh <= margin_top + margin_bottom);
		bottom = dh / 2;
		if (bottom > margin_bottom)
			bottom = (UINT8)margin_bottom;
		top = dh - bottom;
		if (top > margin_top)
		{
			int db = top - margin_top;
			top -= db;
			bottom += db;
		}
		OP_ASSERT(top + bottom == dh);
	}
	else
	{
		top = 0;
		bottom = 0;
	}

	OP_ASSERT(left <= margin_left);
	OP_ASSERT(right <= margin_right);
	OP_ASSERT(top <= margin_top);
	OP_ASSERT(bottom <= margin_bottom);

	return TRUE;
}
#endif // SKIN_SUPPORT

void OpWidgetPainterManager::InitPaint(VisualDevice* vd, OpWidget* widget)
{
	painter[0]->InitPaint(vd, widget);
	painter[1]->InitPaint(vd, widget);

	use_painter[0] = painter[0];
	use_painter[1] = painter[1];

	if (NeedCssPainter(widget) && painter[0]->CanHandleCSS() == FALSE &&
							painter[1]->CanHandleCSS())
	{
		use_painter[0] = painter[1];
		use_painter[1] = painter[0];
	}
}

OpWidgetInfo* OpWidgetPainterManager::GetInfo(OpWidget* widget)
{
	if (NeedCssPainter(widget))
		return painter[1]->GetInfo(NULL); // Currently, we know that the second is always CssWidgetPainter
	return painter[0]->GetInfo(NULL);
}

BOOL OpWidgetPainterManager::DrawScrollbarDirection(const OpRect &drawrect, int direction, BOOL is_down, BOOL is_hovering)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawScrollbarDirection(drawrect, direction, is_down, is_hovering))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawScrollbarBackground(const OpRect &drawrect)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawScrollbarBackground(drawrect))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawScrollbarKnob(const OpRect &drawrect, BOOL is_down, BOOL is_hovering)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawScrollbarKnob(drawrect, is_down, is_hovering))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawScrollbar(const OpRect &drawrect)
{
	/**
	* If the first widgetpainter doesn't support DrawScrollbar then that's fine,
	* we'll fallback to using DrawScrollbarDirection/DrawScrollbarKnob etc.
	* We need to do this, because otherwise scrollbars with CSS properties set
	* would get drawn by the first painter that returns TRUE for DrawScrollbar.
	*/
	return use_painter[0]->DrawScrollbar(drawrect);
}

#ifdef PAGED_MEDIA_SUPPORT
BOOL OpWidgetPainterManager::DrawPageControlBackground(const OpRect& drawrect)
{
	for (int i = 0; i < 2; i++)
		if (use_painter[i]->DrawPageControlBackground(drawrect))
			break;

	return TRUE;
}

BOOL OpWidgetPainterManager::DrawPageControlButton(const OpRect& drawrect, ARROW_DIRECTION direction, BOOL is_down, BOOL is_hovering)
{
	for (int i = 0; i < 2; i++)
		if (use_painter[i]->DrawPageControlButton(drawrect, direction, is_down, is_hovering))
			break;

	return TRUE;
}
#endif // PAGED_MEDIA_SUPPORT

BOOL OpWidgetPainterManager::DrawDropdown(const OpRect &drawrect)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawDropdown(drawrect))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawDropdownButton(const OpRect &drawrect, BOOL is_down)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawDropdownButton(drawrect, is_down))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawListbox(const OpRect &drawrect)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawListbox(drawrect))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawButton(const OpRect &drawrect, BOOL value, BOOL is_default)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawButton(drawrect, value, is_default))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawEdit(const OpRect &drawrect)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawEdit(drawrect))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawDropDownHint(const OpRect &drawrect)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawDropDownHint(drawrect))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawMultiEdit(const OpRect &drawrect)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawMultiEdit(drawrect))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawRadiobutton(const OpRect &drawrect, BOOL value)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawRadiobutton(drawrect, value))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawCheckbox(const OpRect &drawrect, BOOL value)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawCheckbox(drawrect, value))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawItem(const OpRect &drawrect, OpStringItem* item, BOOL is_listbox, BOOL is_focused_item, int indent)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawItem(drawrect, item, is_listbox, is_focused_item, indent))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawResizeCorner(const OpRect &drawrect, BOOL active)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawResizeCorner(drawrect, active))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawWidgetResizeCorner(const OpRect &drawrect, BOOL is_active, SCROLLBAR_TYPE scrollbar)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawWidgetResizeCorner(drawrect, is_active, scrollbar))
			break;
	return TRUE;
}

#ifdef QUICK
BOOL OpWidgetPainterManager::DrawTreeViewRow(const OpRect &drawrect, BOOL focused)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawTreeViewRow(drawrect, focused))
			break;
	return TRUE;
}

BOOL OpWidgetPainterManager::DrawProgressbar(const OpRect &drawrect, double fillpercent, INT32 progress_when_total_unknown, OpWidgetString* string, const char *empty_skin, const char *full_skin)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawProgressbar(drawrect, fillpercent, progress_when_total_unknown, string, empty_skin, full_skin))
			break;
	return TRUE;
}
#endif

BOOL OpWidgetPainterManager::DrawPopupableString(const OpRect &drawrect, BOOL is_hovering_button)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawPopupableString(drawrect, is_hovering_button))
			break;
	return TRUE;
}

/* virtual */
BOOL OpWidgetPainterManager::DrawSlider(const OpRect& rect, BOOL horizontal, double min, double max, double pos, BOOL highlighted, BOOL pressed_knob, OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawSlider(rect, horizontal, min, max, pos, highlighted, pressed_knob, out_knob_position, out_start_track, out_end_track))
			return TRUE;
	return FALSE;
}

/* virtual */
BOOL OpWidgetPainterManager::DrawSliderFocus(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& focus_rect)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawSliderFocus(slider_rect, track_rect, focus_rect))
			break;
	return TRUE;
}

/* virtual */
BOOL OpWidgetPainterManager::DrawSliderTrack(const OpRect& slider_rect, const OpRect& track_rect)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawSliderTrack(slider_rect, track_rect))
			break;
	return TRUE;
}

/* virtual */
BOOL OpWidgetPainterManager::DrawSliderKnob(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& knob_rect)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawSliderKnob(slider_rect, track_rect, knob_rect))
			break;
	return TRUE;
}

/* virtual */
BOOL OpWidgetPainterManager::DrawMonthView(const OpRect& drawrect, const OpRect& header_area)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawMonthView(drawrect, header_area))
			break;
	return TRUE;
}

/* virtual */
BOOL OpWidgetPainterManager::DrawColorBox(const OpRect& drawrect, COLORREF color)
{
	for(int i = 0; i < 2; i++)
		if (use_painter[i]->DrawColorBox(drawrect, color))
			break;
	return TRUE;
}
