/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SKIN_SUPPORT
#include "modules/skin/OpSkinElement.h"
#include "modules/skin/OpSkinManager.h"
#endif

#include "modules/widgets/CssWidgetPainter.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpResizeCorner.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpWidgetStringDrawer.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/widgets/OpSlider.h"
#include "modules/widgets/OpMonthView.h"
#include "modules/widgets/OpColorField.h"

#include "modules/display/vis_dev.h"
#include "modules/layout/layout.h"

void CssWidgetPainter::InitPaint(VisualDevice* vd, OpWidget* widget)
{
	this->vd = vd;
	this->widget = widget;
	gray = info.GetSystemColor(OP_SYSTEM_COLOR_BUTTON);
	lgray = info.GetSystemColor(OP_SYSTEM_COLOR_BUTTON_LIGHT);
	dgray = info.GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK);
	dgray2 = info.GetSystemColor(OP_SYSTEM_COLOR_BUTTON_VERYDARK);
	fdef = info.GetSystemColor(OP_SYSTEM_COLOR_TEXT);
	bdef = info.GetSystemColor(widget->IsEnabled() ? OP_SYSTEM_COLOR_BACKGROUND : OP_SYSTEM_COLOR_BACKGROUND_DISABLED);
	black = OP_RGBA(0, 0, 0, 255);
	color = widget->GetColor();
}

OpWidgetInfo* CssWidgetPainter::GetInfo(OpWidget* widget)
{
	return &info;
}

// static
void CssWidgetPainter::DrawArrow(const OpRect &drawrect, int direction)
{
	OpPainter* painter = vd->painter;
	OpRect scalerect = drawrect;
	scalerect.x += vd->GetTranslationX() - vd->GetRenderingViewX();
	scalerect.y += vd->GetTranslationY() - vd->GetRenderingViewY();
	scalerect = vd->OffsetToContainer(vd->ScaleToScreen(scalerect));
	INT32 i;
	INT32 minsize = MIN(scalerect.width, scalerect.height);
	INT32 xofs = scalerect.x + scalerect.width/2;
	INT32 yofs = scalerect.y + scalerect.height/2;

	INT32 levels = (minsize / 3 - 1);
	if (minsize < 9)
		levels = 2;
	else if (minsize <= 12)
		levels = 3;

	int length = 1;
	int ofs = levels - 1;

	INT32 aw = levels;
	INT32 ah = (levels >> 1) + 1;

	if (direction == ARROW_UP)
	{
		xofs -= aw;
		yofs -= ah;
		for(i = 0; i < levels; i++, length += 2, ofs--)
			painter->FillRect(OpRect(xofs + ofs, yofs + i, length, 1));
	}
	else if (direction == ARROW_DOWN)
	{
		xofs -= aw;
		yofs -= ah;
		for(i = 0; i < levels; i++, length += 2, ofs--)
			painter->FillRect(OpRect(xofs + ofs, yofs + levels - i, length, 1));
	}
	else if (direction == ARROW_LEFT)
	{
		xofs -= ah;
		yofs -= aw;
		for(i = 0; i < levels; i++, length += 2, ofs--)
			painter->FillRect(OpRect(xofs + i, yofs + ofs, 1, length));
	}
	else if (direction == ARROW_RIGHT)
	{
		xofs -= ah;
		yofs -= aw;
		for(i = 0; i < levels; i++, length += 2, ofs--)
			painter->FillRect(OpRect(xofs + levels - i, yofs + ofs, 1, length));
	}
}

// static
void CssWidgetPainter::Draw3DBorder(UINT32 lcolor, UINT32 dcolor, const OpRect &drawrect)
{
	vd->SetColor32(lcolor);
	vd->FillRect(OpRect(drawrect.x, drawrect.y, drawrect.width-1, 1));
	vd->FillRect(OpRect(drawrect.x, drawrect.y, 1, drawrect.height-1));
	vd->SetColor32(dcolor);
	vd->FillRect(OpRect(drawrect.x, drawrect.y + drawrect.height - 1, drawrect.width, 1));
	vd->FillRect(OpRect(drawrect.x + drawrect.width - 1, drawrect.y, 1, drawrect.height));
}

void CssWidgetPainter::DrawSunkenExternalBorder(const OpRect& rect)
{
	// Only draw borders if the widgets has no own css borders 
	// (which are drawn by the layout engine)
	if (!widget->HasCssBorder())
	{
		DrawSunkenBorder(rect);
	}
}

void CssWidgetPainter::DrawSunkenBorder(OpRect rect)
{
	Draw3DBorder(dgray, lgray, rect);
	Draw3DBorder(dgray2, gray, rect.InsetBy(1, 1));
}

void CssWidgetPainter::DrawFocusRect(const OpRect &drawrect)
{
#ifndef WIDGETS_DONT_DRAW_FOCUS_RECT_FOR_CSS_STYLED_WIDGETS
	vd->DrawFocusRect(drawrect);
#endif // WIDGETS_DONT_DRAW_FOCUS_RECT_FOR_CSS_STYLED_WIDGETS
}

void CssWidgetPainter::DrawDirectionButton(OpRect rect, int direction, BOOL is_scrollbar, BOOL is_down)
{
	BOOL enabled_look = widget->IsEnabled();
	UINT32 light = gray;
	UINT32 darkshadow = dgray2;
	UINT32 highlight = lgray;
	UINT32 shadow = dgray;
	UINT32 face = gray;
	UINT32 arrow = fdef;
	if (is_scrollbar)
	{
		OpScrollbar* scroll = (OpScrollbar*) widget;
		if (!scroll->CanScroll())
			enabled_look = FALSE;

		light = scroll->scrollbar_colors.Light(light);
		darkshadow = scroll->scrollbar_colors.DarkShadow(darkshadow);
		highlight = scroll->scrollbar_colors.Highlight(highlight);
		shadow = scroll->scrollbar_colors.Shadow(shadow);
		face = scroll->scrollbar_colors.Face(face);
		arrow = scroll->scrollbar_colors.Arrow(arrow);
	}
	vd->SetColor32(face);
	vd->FillRect(rect);
	if (is_down)
	{
		vd->SetColor32(shadow);
		vd->DrawRect(rect);
		rect.x++;
		rect.y++;
	}
	else
	{
		Draw3DBorder(light, darkshadow, rect);
		Draw3DBorder(highlight, shadow, rect.InsetBy(1, 1));
	}
	// Draw arrow
	if (enabled_look)
	{
		vd->SetColor32(arrow);
		DrawArrow(rect, direction);
	}
	else
	{
		rect.x ++; rect.y ++;
		vd->SetColor32(highlight);
		DrawArrow(rect, direction);
		rect.x --; rect.y --;
		vd->SetColor32(shadow);
		DrawArrow(rect, direction);
	}
}

BOOL CssWidgetPainter::DrawScrollbarDirection(const OpRect &drawrect, int direction, BOOL is_down, BOOL is_hovering)
{
	DrawDirectionButton(drawrect, direction, TRUE, is_down);
	return TRUE;
}

BOOL CssWidgetPainter::DrawScrollbarBackground(const OpRect &drawrect)
{
	OpScrollbar* scroll = (OpScrollbar*) widget;
	UINT32 bgcol = info.GetSystemColor(OP_SYSTEM_COLOR_SCROLLBAR_BACKGROUND);
	bgcol = scroll->scrollbar_colors.Track(bgcol);
	vd->SetColor32(bgcol);
	vd->FillRect(drawrect);
	return TRUE;
}

BOOL CssWidgetPainter::DrawScrollbarKnob(const OpRect &drawrect, BOOL is_down, BOOL is_hovering)
{
	OpScrollbar* scroll = (OpScrollbar*) widget;
	if (scroll->CanScroll())
	{
		UINT32 light = scroll->scrollbar_colors.Light(gray);
		UINT32 darkshadow = scroll->scrollbar_colors.DarkShadow(dgray2);
		UINT32 highlight = scroll->scrollbar_colors.Highlight(lgray);
		UINT32 shadow = scroll->scrollbar_colors.Shadow(dgray);
		UINT32 face = scroll->scrollbar_colors.Face(gray);
		Draw3DBorder(light, darkshadow, drawrect);
		Draw3DBorder(highlight, shadow, drawrect.InsetBy(1, 1));
		vd->SetColor32(face);
		vd->FillRect(drawrect.InsetBy(2, 2));
	}
	else
	{
		DrawScrollbarBackground(drawrect);
	}
	return TRUE;
}

#ifdef PAGED_MEDIA_SUPPORT
BOOL CssWidgetPainter::DrawPageControlBackground(const OpRect &drawrect)
{
	vd->SetColor(GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BUTTON));
	vd->FillRect(drawrect);

	vd->SetColor(GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK));
	vd->DrawRect(drawrect);

	return TRUE;
}

BOOL CssWidgetPainter::DrawPageControlButton(const OpRect& drawrect, ARROW_DIRECTION direction, BOOL is_down, BOOL is_hovering)
{
	DrawDirectionButton(drawrect, direction, FALSE, is_down);
	return TRUE;
}
#endif // PAGED_MEDIA_SUPPORT

BOOL CssWidgetPainter::DrawDropdown(const OpRect &drawrect)
{
	OpDropDown* dropdown = (OpDropDown*) widget;

	OpRect inner_rect = drawrect;

	DrawSunkenExternalBorder(inner_rect);

	dropdown->GetInfo()->AddBorder(dropdown, &inner_rect);

	if(widget->LeftHandedUI())
		inner_rect.x += GetInfo()->GetDropdownButtonWidth(widget);

	inner_rect.width -= GetInfo()->GetDropdownButtonWidth(widget);

	OpStringItem* item = dropdown->GetItemToPaint();
	if (!dropdown->edit && item)
	{
		dropdown->SetClipRect(inner_rect);
		DrawItem(inner_rect, item, FALSE, TRUE, 0);
		dropdown->RemoveClipRect();
	}

	if(widget->LeftHandedUI())
		inner_rect.x -= GetInfo()->GetDropdownButtonWidth(widget);
	else
		inner_rect.x += inner_rect.width;

	inner_rect.width = GetInfo()->GetDropdownButtonWidth(widget);
	if (dropdown->m_dropdown_packed.show_button)
		DrawDropdownButton(inner_rect, dropdown->m_dropdown_window != NULL);

	return TRUE;
}

BOOL CssWidgetPainter::DrawDropdownButton(const OpRect &drawrect, BOOL is_down)
{
	DrawDirectionButton(drawrect, ARROW_DOWN, FALSE, is_down);
	return TRUE;
}

BOOL CssWidgetPainter::DrawListbox(const OpRect &drawrect)
{
	UINT32 bgcol = color.use_default_background_color ? bdef : color.background_color;

	if (!widget->HasCssBackgroundImage())
	{
		vd->SetColor32(bgcol);
		UINT32 w = 0;
		if (((OpListBox*)widget)->HasScrollbar())
			w = info.GetVerticalScrollbarWidth();
		UINT32 left_scr_bar = widget->LeftHandedUI() ? info.GetVerticalScrollbarWidth() : 0;
		vd->FillRect(OpRect(drawrect.x + left_scr_bar, drawrect.y, drawrect.width - w, drawrect.height));
	}
	DrawSunkenExternalBorder(drawrect);
	return TRUE;
}

BOOL CssWidgetPainter::DrawButton(const OpRect &drawrect, BOOL pressed, BOOL is_default)
{
	OpButton* button = (OpButton*) widget;

	OpRect rect(drawrect), inner_rect(drawrect);

	if(!widget->HasCssBorder())
	{
		inner_rect = inner_rect.InsetBy(2, 2);
	}

	UINT32 bgcol = color.background_color;
	if (color.use_default_background_color)
		bgcol = gray;
	UINT32 fcol = color.use_default_foreground_color ? fdef : color.foreground_color;
	if (widget->IsEnabled() == FALSE)
		fcol = info.GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);

	// draw def button border

	OpRect text_rect = inner_rect;

	if (!widget->HasCssBorder() && is_default)
	{
		vd->SetColor32(black);
		vd->DrawRect(rect);
		rect = rect.InsetBy(1, 1);
		inner_rect = inner_rect.InsetBy(1, 1);
	}

	BOOL raised = !pressed;

	// draw background

	if (!widget->HasCssBackgroundImage())
	{
		vd->SetColor32(bgcol);
		vd->FillRect(rect);
	}

	// clipping.. just in case

	widget->SetClipRect(rect);

	// draw dropdown first

	OpRect border_rect = rect;

	if (pressed)
	{
		DrawSunkenExternalBorder(border_rect);
	}
	else if (raised && !widget->HasCssBorder())
	{
		Draw3DBorder(gray, dgray2, border_rect);
		Draw3DBorder(lgray, dgray, border_rect.InsetBy(1, 1));
	}

	// Draw the text

	if (pressed)
	{
		text_rect.x++;
		text_rect.y++;
	}

	button->PaintContent(text_rect, fcol);

	// draw focus rect

	if ((widget->IsFocused() || button->HasForcedFocusedLook()) && widget->HasFocusRect())
	{
		DrawFocusRect(inner_rect.InsetBy(1, 1));
	}

	widget->RemoveClipRect();

	return TRUE;
}

BOOL CssWidgetPainter::DrawEdit(const OpRect &drawrect)
{
	OpEdit* edit = ((OpEdit*)widget);

	BOOL draw_ghost = (!widget->IsFocused() || edit->GetShowGhostTextWhenFocused()) && edit->GetTextLength() == 0 && edit->ghost_string.Get() && *edit->ghost_string.Get();

	OpRect rect(drawrect), inner_rect(drawrect);
	if(!widget->HasCssBorder())
		inner_rect = inner_rect.InsetBy(2, 2);

	UINT32 bgcol = color.use_default_background_color ? bdef : color.background_color;
	UINT32 fcol = color.use_default_foreground_color ? fdef : color.foreground_color;

	if (widget->IsEnabled() == FALSE)
		fcol = info.GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);

	if (draw_ghost)
	{
		fcol = !widget->IsFocused() ? info.GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED) : 
			edit->GetGhostTextFGColorWhenFocused();
	}

	if (!widget->HasCssBackgroundImage())
	{
		vd->SetColor32(bgcol);
		vd->FillRect(rect);
	}
	DrawSunkenExternalBorder(drawrect);

	edit->PaintContent(inner_rect, fcol, draw_ghost);

	return TRUE;
}

BOOL CssWidgetPainter::DrawDropDownHint(const OpRect &drawrect)
{
	return FALSE;
}

BOOL CssWidgetPainter::DrawMultiEdit(const OpRect &drawrect)
{
	OpRect inner_rect(drawrect);

#ifdef SKIN_SUPPORT
	BOOL was_skinned = !g_widgetpaintermanager->NeedCssPainter(widget);
#endif

	if(!widget->HasCssBorder())
	{
		inner_rect = inner_rect.InsetBy(2, 2);

#ifdef SKIN_SUPPORT
		if (!was_skinned)
#endif
		{
			DrawSunkenExternalBorder(drawrect);
		}
	}

	OpMultilineEdit* multiedit = (OpMultilineEdit*)widget;

	UINT32 bgcol = color.use_default_background_color ? bdef : color.background_color;
	UINT32 fcol = color.use_default_foreground_color ? fdef : color.foreground_color;
	if (widget->IsEnabled() == FALSE || multiedit->GhostMode())
		fcol = info.GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);

#ifdef SKIN_SUPPORT
	if (multiedit->IsFlatMode())
	{
		INT32 state = !widget->IsEnabled() ? SKINSTATE_DISABLED : 0;
		fcol = multiedit->GetForegroundColor(info.GetSystemColor(state & SKINSTATE_DISABLED ? OP_SYSTEM_COLOR_UI_DISABLED_FONT : OP_SYSTEM_COLOR_UI_FONT), state);
	}
#endif

#ifdef SKIN_SUPPORT
	if (!widget->HasCssBackgroundImage() && !was_skinned && (!multiedit->IsFlatMode() || !color.use_default_background_color))
#else
	if (!widget->HasCssBackgroundImage())
#endif
	{
		OpRect bgrect = multiedit->GetVisibleRect();

		vd->SetColor32(bgcol);
		vd->FillRect(bgrect);
	}

	widget->SetClipRect(inner_rect);
	multiedit->OutputText(fcol);
	widget->RemoveClipRect();

	// draw focus rect for read-only multiline edits
	// no effect of SetHasFocusRect() in OpMultiEdit::SetReadOnly(), so I'm using IsReadOnly() here - manuelah
	if (multiedit->IsFocused() && multiedit->IsReadOnly() && !multiedit->IsFlatMode()) // && multiedit->HasFocusRect())
	{
		DrawFocusRect(drawrect);
	}
	return TRUE;
}

BOOL CssWidgetPainter::DrawRadiobutton(const OpRect &drawrect, BOOL value)
{
	OpRect rect(drawrect), inner_rect(drawrect), border_rect(drawrect);
	if(!widget->HasCssBorder())
		inner_rect = inner_rect.InsetBy(2, 2);

	const UINT8 col_tre = 48; // minimum max component difference
	UINT32 bgcol = color.use_default_background_color ? bdef : color.background_color;
	UINT32 fcol = color.use_default_foreground_color ? fdef : color.foreground_color;
	if (OP_GET_A_VALUE(fcol) == 255)
		fcol = vd->GetVisibleColorOnBgColor(fcol, bgcol, col_tre);
	if (widget->IsEnabled() == FALSE)
	{
		fcol = info.GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
		if (color.use_default_background_color)
			bgcol = info.GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_DISABLED);
	}

	int size = MIN(drawrect.width, drawrect.height);

	// Create a rect for the round button centered in the widget
	OpRect r1(border_rect.x,
			  border_rect.y + (border_rect.height - size) / 2,
			  size, size);

	OpRadioButton* radio = (OpRadioButton*) widget;

	OpRect focus_rect = rect;

	if (uni_strlen(radio->string.Get()) > 0)
	{
		int x = r1.x + r1.width + 6;

		focus_rect.Set(x, inner_rect.y, inner_rect.width - x, inner_rect.height);

		radio->string.Draw(focus_rect, vd, fcol);

		focus_rect.width = radio->string.GetWidth() + 4;
		focus_rect.x -= 2;
	}
	else
	{
		// Center horizontally
		r1.x += (border_rect.width - size) / 2;
	}

	if (size <= 0)
		return TRUE;
	// stop the insanity, we can't draw that small radio buttons with a beveled border
	if (size < 5)
	{
		vd->SetColor32(dgray);
		vd->FillEllipse(r1);
		if (value)
			vd->SetColor32(fcol);
		else
			vd->SetColor32(bgcol);
		vd->FillEllipse(r1.InsetBy(1,1));
	}
	else if (widget->HasCssBorder())
	{
		r1 = r1.InsetBy(1, 1);
		if (value)
			vd->SetColor32(fcol);
		else
		{
			r1 = r1.InsetBy(1,1);
			vd->SetColor32(dgray);
			vd->FillEllipse(r1);
			vd->SetColor32(bgcol);
		}
		vd->FillEllipse(r1.InsetBy(1,1));
	}
	else
	{
		// ~ r*(1 - 1/sqrt(2)), where r is the inner radius (i.e. inside border)
		// this corresponds to the horizontal (or vertical) distance from the edge
		// of the bounding square to the intersection of the inner edge of the border
		// and the line y=x
		INT32 clipside = ((r1.width-2)*19) >> 6;

		widget->SetClipRect(OpRect(r1.x - rect.x, r1.y - rect.y, r1.width - clipside, r1.height - clipside));
		vd->SetColor32(dgray);
		vd->FillEllipse(r1);

		widget->RemoveClipRect();

		widget->SetClipRect(OpRect(r1.x + clipside - rect.x, r1.y + clipside - rect.y, r1.width - clipside, r1.height - clipside));
		vd->SetColor32(lgray);
		vd->FillEllipse(r1);

		widget->RemoveClipRect();

		r1 = r1.InsetBy(1, 1);
		--clipside;

		widget->SetClipRect(OpRect(r1.x - rect.x, r1.y - rect.y, r1.width - clipside, r1.height - clipside));
		vd->SetColor32(dgray2);
		vd->FillEllipse(r1);

		widget->RemoveClipRect();

		widget->SetClipRect(OpRect(r1.x + clipside - rect.x, r1.y + clipside - rect.y, r1.width - clipside, r1.height - clipside));
		vd->SetColor32(gray);
		vd->FillEllipse(r1);

		widget->RemoveClipRect();

		vd->SetColor32(bgcol);
		vd->FillEllipse(r1.InsetBy(1, 1));

#ifndef DONT_DRAW_WIDGET_SELECTION_OR_FOCUS
		if (value)
		{
			vd->SetColor32(fcol);
			UINT8 inset = MAX(1, MIN(3, (r1.width - 3) >> 1));
			vd->FillEllipse(r1.InsetBy(inset, inset));
		}
#endif // !DONT_DRAW_WIDGET_SELECTION_OR_FOCUS
	}

#ifndef DONT_DRAW_WIDGET_SELECTION_OR_FOCUS
	if (widget->IsFocused() && widget->HasFocusRect())
	{
		DrawFocusRect(focus_rect);
	}
#endif // !DONT_DRAW_WIDGET_SELECTION_OR_FOCUS
	return TRUE;
}

BOOL CssWidgetPainter::DrawCheckbox(const OpRect &drawrect, BOOL value)
{
	OpRect inner_rect(drawrect);
	if(!widget->HasCssBorder())
		inner_rect = inner_rect.InsetBy(2, 2);

	UINT32 bgcol = color.use_default_background_color ? bdef : color.background_color;

	const UINT8 col_tre = 48; // minimum max component difference
#ifdef SKIN_SUPPORT
	UINT32 fcol = widget->GetForegroundColor(fdef, widget->GetBorderSkin()->GetState());
	if (OP_GET_A_VALUE(fcol) == 255)
		fcol = vd->GetVisibleColorOnBgColor(fcol, bgcol, col_tre);
#else
	UINT32 fcol = color.use_default_foreground_color ? fdef : color.foreground_color;
	if (OP_GET_A_VALUE(fcol) == 255)
		fcol = vd->GetVisibleColorOnBgColor(fcol, bgcol, col_tre);
	if (widget->IsEnabled() == FALSE)
		fcol = info.GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
#endif

	if (widget->IsEnabled() == FALSE)
	{
		if (color.use_default_background_color)
			bgcol = info.GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_DISABLED);
	}

	INT32 checkbox_side = MIN(drawrect.width, drawrect.height);
	// center box on drawrect
	OpRect border(
		drawrect.x + ((drawrect.width - checkbox_side) >> 1),
		drawrect.y + ((drawrect.height - checkbox_side) >> 1),
		checkbox_side,
		checkbox_side);
	OpRect inner = widget->HasCssBorder() ? border : border.InsetBy(2,2);

	OpCheckBox* checkbox = NULL;

	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_CHECKBOX)
	{
		checkbox = (OpCheckBox*) widget;
	}

	if (checkbox && uni_strlen(checkbox->string.Get()) > 0)
	{
		// Do we ever come here without skinning?
		OP_ASSERT(FALSE);
	}

	if (!widget->HasCssBackgroundImage())
	{
		vd->SetColor32(bgcol);
		vd->FillRect(inner);
	}
	vd->SetColor32(fcol);

	// Apply padding if we have enough space for it and draw checkmark.
	OpRect check = inner;
	check.x += widget->GetPaddingLeft();
	check.width -= widget->GetPaddingLeft() + widget->GetPaddingRight();
	check.y += widget->GetPaddingTop();
	check.height -= widget->GetPaddingTop() + widget->GetPaddingBottom();

	// don't allow tiny marks
	const short min_mark_size = 5;
	if (check.width < min_mark_size && inner.width >= min_mark_size)
	{
		check.x -= (min_mark_size - check.width) >> 1;
		check.width = min_mark_size;
	}
	if (check.height < min_mark_size && inner.height >= min_mark_size)
	{
		check.y -= (min_mark_size - check.height) >> 1;
		check.height = min_mark_size;
	}

	BOOL indeterminate = checkbox && checkbox->GetIndeterminate();
	if (indeterminate)
	{
		vd->FillRect(check);
	}
	else if (value)
	{
		DrawCheckmark(check);
	}
	if(!widget->HasCssBorder())
	{
		Draw3DBorder(dgray, lgray, border);
		Draw3DBorder(dgray2, gray, border.InsetBy(1, 1));
	}

	if (checkbox && widget->IsFocused() && widget->HasFocusRect())
		DrawFocusRect(inner_rect);
	return TRUE;
}

void CssWidgetPainter::DrawCheckmark(OpRect inner)
{
	if (inner.width <= 0 || inner.height <= 0)
		return;

	inner.x += vd->GetTranslationX() - vd->GetRenderingViewX();
	inner.y += vd->GetTranslationY() - vd->GetRenderingViewY();
	inner = vd->OffsetToContainer(vd->ScaleToScreen(inner));

	INT32 side = MIN(inner.width, inner.height);
	const INT32 SizeRatio = side >= 26 ? 3 : 2; // for bigger sizes, 2 overflows
	INT32 size = side <= SizeRatio ? 1 : side / SizeRatio;
	INT32 short_line = side / 3;
	INT32 long_line = MAX(0, side - short_line - 1);

	OpPainter* painter = vd->painter;

	// for really small checkmarks, just fill rect
	if (!long_line)
	{
		painter->FillRect(inner);
		return;
	}

	OpPoint p3(inner.x+side-1, inner.y + ((side-(long_line+size)) >> 1));
	OpPoint p2(p3.x - long_line, p3.y + long_line);
	OpPoint p1(p2.x - short_line, p2.y - short_line);

	for (int i = 0; i < size; ++i)
	{
		painter->DrawLine(OpPoint(p1.x,p1.y+i), OpPoint(p2.x,p2.y+i));
		painter->DrawLine(OpPoint(p2.x,p2.y+i), OpPoint(p3.x,p3.y+i));
	}
}

BOOL CssWidgetPainter::DrawItem(const OpRect &drawrect, OpStringItem* item, BOOL is_listbox, BOOL is_focused_item, int indent)
{
	OpRect rect = drawrect;
	WIDGET_COLOR color = widget->GetColor();

	OpListBox* listbox = (widget->GetType() == OpTypedObject::WIDGET_TYPE_LISTBOX ? (OpListBox*)widget : NULL);
	OpDropDown* dropdown = (widget->GetType() == OpTypedObject::WIDGET_TYPE_DROPDOWN ? (OpDropDown*)widget : NULL);

	UINT32 fcol = color.use_default_foreground_color ? fdef : color.foreground_color;
	UINT32 bcol = color.use_default_background_color ? bdef : color.background_color;

	if (is_listbox)
	{
		if (item->bg_col != USE_DEFAULT_COLOR)
			bcol = item->bg_col;
		if (item->fg_col != USE_DEFAULT_COLOR)
			fcol = item->fg_col;
	}

#ifdef SKIN_SUPPORT
	// Determine if the color used for the item is the default color or not.
	BOOL has_item_bg_col = FALSE;
	if (is_listbox)
	{
		has_item_bg_col = (item->bg_col != USE_DEFAULT_COLOR);
	}
	else if (!color.use_default_background_color)
	{
		has_item_bg_col = (color.background_color != USE_DEFAULT_COLOR);
	}
#endif // SKIN_SUPPORT

	BOOL selected = item->IsSelected();

#if defined(WIDGETS_COLOR_ITEM_TEXT) || defined(SKIN_SUPPORT)
	BOOL draw_selected = FALSE; ///< Might differ from selected if WIDGETS_CHECKBOX_MULTISELECT is on.
#endif

	// Don't paint selected item in dropdown that has menu down.
#ifndef DONT_DRAW_WIDGET_SELECTION_OR_FOCUS
	if (dropdown && dropdown->IsDroppedDown())
#endif // !DONT_DRAW_WIDGET_SELECTION_OR_FOCUS
		selected = FALSE;

	if (!item->IsGroupStart())
	{
#ifdef WIDGETS_CHECKBOX_MULTISELECT
		// Don't paint with highlighted colors
		if (! (listbox && listbox->ih.is_multiselectable))
#endif
		if (selected && (widget->IsFocused() || is_listbox))
		{
			bcol = info.GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
			fcol = info.GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED);
#ifndef WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN
			if (dropdown)
			{
				bcol = info.GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND);
				fcol = info.GetSystemColor(OP_SYSTEM_COLOR_TEXT);
			}
#endif // WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN

#if defined(WIDGETS_COLOR_ITEM_TEXT) || defined(SKIN_SUPPORT)
			draw_selected = TRUE;
#endif
		}
		if (!widget->IsEnabled() || !item->IsEnabled())
			fcol = info.GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
	}

#ifdef SKIN_SUPPORT
	// Prefer colors that comes from styling over the currnet skin
	BOOL allow_skin = (bcol == bdef && fcol == fdef) || draw_selected;
#endif // SKIN_SUPPORT
#ifdef WIDGETS_COLOR_ITEM_TEXT
	if(allow_skin && !g_widgetpaintermanager->GetUseFallbackPainter())
	{
		if(widget->IsEnabled() && item->IsEnabled())
		{
			if(!is_focused_item && !draw_selected)
				fcol = info.GetSystemColor(OP_SYSTEM_COLOR_ITEM_TEXT);
			else
				fcol = info.GetSystemColor(OP_SYSTEM_COLOR_ITEM_TEXT_SELECTED);
		}
	}
#endif // WIDGETS_COLOR_ITEM_TEXT

#ifdef SKIN_SUPPORT
	if (!is_listbox && dropdown && dropdown->m_dropdown_packed.is_hovering_button && color.use_default_foreground_color)
	{
		UINT32 foreground_color;
		if (OpStatus::IsSuccess(widget->GetSkinManager()->GetTextColor(widget->GetBorderSkin()->GetImage(),
		                                                               &foreground_color,
		                                                               SKINSTATE_HOVER,
		                                                               widget->GetBorderSkin()->GetType(),
		                                                               widget->GetBorderSkin()->GetSize(),
		                                                               widget->GetBorderSkin()->IsForeground())))
		{
			fcol = foreground_color;
		}
	}
#endif // SKIN_SUPPORT

	vd->SetColor(bcol);
#ifdef SKIN_SUPPORT
	if (is_listbox || has_item_bg_col ||
# ifdef WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN
		item->IsSelected() ||
# endif // WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN
		!widget->GetSkinned())
#endif // SKIN_SUPPORT
	{
		OpRect dst_rect(rect.x - indent, rect.y, rect.width + indent, rect.height);
		BOOL bg_painted = FALSE;
#ifdef SKIN_SUPPORT
		if (allow_skin && !g_widgetpaintermanager->GetUseFallbackPainter())
		{
			const char *name = "Listitem Skin";
#ifdef WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN
			BOOL draw_highlights = TRUE;
#else
			BOOL draw_highlights = !dropdown;
#endif // WIDGETS_HIGHLIGHT_ITEM_IN_DROPDOWN
			INT32 state = 0;
			INT32 hover_value = 0;
			if (draw_selected && draw_highlights)
				state |= SKINSTATE_SELECTED;
			if (!widget->IsEnabled() || !item->IsEnabled())
				state |= SKINSTATE_DISABLED;
			if (is_focused_item && draw_highlights)
				state |= SKINSTATE_FOCUSED;
			if (OpStatus::IsSuccess(g_skin_manager->DrawElement(vd, name, dst_rect, state, hover_value)))
				bg_painted = TRUE;
		}
#endif // SKIN_SUPPORT

		if (!bg_painted)
			vd->FillRect(dst_rect);
	}

#ifdef WIDGETS_CHECKBOX_MULTISELECT
	if (!item->IsGroupStart())
	{
		if (listbox && listbox->ih.is_multiselectable)
		{
			INT32 image_width = 7;
			INT32 image_height = 7;
#ifdef SKIN_SUPPORT
			OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Checkbox Skin");
			if (skin_elm)
			{
				skin_elm->GetSize(&image_width, &image_height, 0);

				OpRect img_rect(rect.x, rect.y + (rect.height - image_width) / 2, image_width, image_width);
				INT32 state = !item->IsEnabled() ? SKINSTATE_DISABLED : 0;
				if (selected)
					state |= SKINSTATE_SELECTED;
				widget->GetSkinManager()->DrawElement(vd, "Checkbox Skin", img_rect, state);
			}
			else
#endif // SKIN_SUPPORT
			{
				if (selected)
				{
					vd->SetColor(fcol);
					DrawCheckmark(OpRect(rect.x + 1, rect.y + (rect.height - image_height) / 2 + 1, image_width, image_height));
				}
				image_width += 2; // 1px left and right padding
			}

			rect.x += image_width;
			rect.width -= image_width;
		}
	}
#endif

#ifdef SKIN_SUPPORT
	if (item->m_indent && is_listbox)
	{
		OpWidgetImage dummy;

		dummy.SetRestrictImageSize(TRUE);
		dummy.SetSkinManager(widget->GetSkinManager());

		INT32 image_width;
		INT32 image_height;

		dummy.GetRestrictedSize(&image_width, &image_height);

		image_width *= item->m_indent;

		rect.x += image_width;
		rect.width -= image_width;
	}
	if (item->m_image.HasContent())
	{
#ifdef WIDGETS_CHECKBOX_MULTISELECT
		item->m_image.SetState(selected ? SKINSTATE_SELECTED : 0);
#endif
		OpRect image_rect = item->m_image.CalculateScaledRect(rect, FALSE, TRUE);
		image_rect.x += 2;
		if (widget->GetRTL())
			image_rect.x = drawrect.Right() - image_rect.Right() + drawrect.x;
		item->m_image.Draw(vd, image_rect);

		UINT32 image_width = image_rect.width + 4;

		rect.x += image_width;
		rect.width -= image_width;
	}
#endif // SKIN_SUPPORT

	// Apply margin and draw the string
	OpRect string_rect = rect;
	OpStringItem::AddItemMargin(&info, widget, string_rect);

	INT32 string2_width = item->string2 ? item->string2->GetWidth() : 0;
	INT32 x_scroll = 0;
	if (listbox && is_focused_item)
	{
		x_scroll = listbox->x_scroll;
		if (x_scroll > item->string.GetWidth() - string_rect.width)
			x_scroll = item->string.GetWidth() - string_rect.width;
		if (item->string.GetWidth() < string_rect.width)
			x_scroll = 0;
	}

	if (item->string2 && item->string.GetWidth() + item->string2->GetWidth() > string_rect.width)
	{
		string2_width = MIN(string2_width, string_rect.width / 2);
	}

	int string1_width = string_rect.width - string2_width;
	OpRect string1_rect(string_rect.x, string_rect.y, string1_width, string_rect.height);
	if (widget->GetRTL())
		string1_rect.x = drawrect.Right() - string1_rect.Right() + drawrect.x;

	OpWidgetStringDrawer string_drawer;
	string_drawer.Draw(&item->string, string1_rect, vd, fcol, string2_width ? ELLIPSIS_END : ELLIPSIS_NONE, x_scroll);

	// Temp fix for 2 columns in autocompletionpopup until it uses TreeView
	if (item->string2)
	{
		JUSTIFY old_justify = widget->font_info.justify;
		widget->font_info.justify = JUSTIFY_RIGHT;

		item->string2->Draw(OpRect(string_rect.x + string1_width, string_rect.y, string2_width, string_rect.height), vd, fcol);

		widget->font_info.justify = old_justify;
	}

#ifndef WIDGETS_DONT_DRAW_ITEM_FOCUS_RECT
	if (widget->IsFocused() && is_focused_item)
	{
		OpRect focus_rect = rect;
		if (widget->GetRTL())
			focus_rect.x = drawrect.Right() - focus_rect.Right() + drawrect.x;
		DrawFocusRect(focus_rect);
	}
#endif // WIDGETS_DONT_DRAW_ITEM_FOCUS_RECT

	return TRUE;
}

BOOL CssWidgetPainter::DrawResizeCorner(const OpRect &drawrect, BOOL active)
{
	OpResizeCorner* corner = (OpResizeCorner*) widget;
	ScrollbarColors& colors = corner->GetScrollbarColors();
	UINT32 highlight = colors.Highlight(lgray);
	UINT32 shadow = colors.Shadow(dgray);
	UINT32 face = colors.Face(gray);
	vd->SetColor32(face);
	vd->FillRect(drawrect);

	if (active)
	{
		vd->BeginClipping(drawrect);
		for(int i = drawrect.width - 15; i < drawrect.width; i += 4)
		{
			vd->SetColor32(shadow);
			vd->DrawLine(OpPoint(drawrect.x + i+1, drawrect.y + drawrect.height),
							OpPoint(drawrect.x + drawrect.width, drawrect.y + i+1), 2);
			vd->SetColor32(highlight);
			vd->DrawLine(OpPoint(drawrect.x + i, drawrect.y + drawrect.height),
							OpPoint(drawrect.x + drawrect.width, drawrect.y + i));
		}
		vd->EndClipping();
	}
	return TRUE;
}

BOOL CssWidgetPainter::DrawWidgetResizeCorner(const OpRect &drawrect, BOOL is_active, SCROLLBAR_TYPE scrollbar)
{
	return scrollbar != SCROLLBAR_NONE ? DrawResizeCorner(drawrect, is_active) : FALSE;
}

BOOL CssWidgetPainter::DrawPopupableString(const OpRect &drawrect, BOOL is_hovering_button)
{
	OpRect inner_rect = drawrect;

	DrawSunkenExternalBorder(inner_rect);

	widget->GetInfo()->AddBorder(widget, &inner_rect);

	inner_rect.width -= GetInfo()->GetDropdownButtonWidth(widget);

	OpString text;
	OP_STATUS status;
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_CALENDAR)
	{
		status = widget->GetText(text);
		if (OpStatus::IsSuccess(status))
		{
			// Hack: Reuse the list item class
			OpStringItem item;

			// Copy widget colors to item
			WIDGET_COLOR color = widget->GetColor();
			item.SetFgColor(color.use_default_foreground_color ? fdef : color.foreground_color);
			item.SetBgColor(color.use_default_background_color ? bdef : color.background_color);

			status = item.SetString(text.CStr(), widget); // XXX empty string?
			if (OpStatus::IsSuccess(status))
			{
				widget->SetClipRect(inner_rect);
				DrawItem(inner_rect, &item, FALSE, TRUE, 0);
				widget->RemoveClipRect();
			}
		}
	}
	else if (widget->GetType() == OpTypedObject::WIDGET_TYPE_COLOR_BOX)
	{
		OpColorBox* colorbox = (OpColorBox*) widget;
		COLORREF color = colorbox->GetColor();
		widget->GetPainterManager()->DrawColorBox(inner_rect, color);
	}

	inner_rect.x += inner_rect.width;
	inner_rect.width = GetInfo()->GetDropdownButtonWidth(widget);
	DrawDropdownButton(inner_rect, FALSE); // XXX Never depressed

	return TRUE;
}

/* virtual */
BOOL CssWidgetPainter::DrawSlider(const OpRect& rect, BOOL horizontal, double min_val, 
								  double max_val, double pos, BOOL highlighted, BOOL pressed_knob,
								  OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track)
{
	return FALSE;
}

/* virtual */
BOOL CssWidgetPainter::DrawSliderFocus(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& focus_rect)
{
	DrawFocusRect(focus_rect);
	return TRUE;
}

/* virtual */
BOOL CssWidgetPainter::DrawSliderTrack(const OpRect& slider_rect, const OpRect& track_rect)
{
	Draw3DBorder(dgray, lgray, track_rect);
	return TRUE;
}

/* virtual */
BOOL CssWidgetPainter::DrawSliderKnob(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& knob_rect)
{
	vd->SetColor32(gray);
	vd->FillRect(knob_rect);
	Draw3DBorder(lgray, dgray, knob_rect);
	return TRUE;
}

/* virtual */
BOOL CssWidgetPainter::DrawMonthView(const OpRect& drawrect, const OpRect& header_area)
{
	OP_ASSERT(header_area.y > 10); // or this will be clipped away
	return TRUE;
}

/* virtual */
BOOL CssWidgetPainter::DrawColorBox(const OpRect& drawrect, COLORREF boxcolor)
{
	OpRect box = drawrect;

	// Draw the background
	if (!color.use_default_background_color)
	{
		vd->SetColor(color.background_color);
		vd->FillRect(drawrect);
	}

	// Inset with padding
	box.x += widget->GetPaddingLeft();
	box.y += widget->GetPaddingTop();
	box.width -= widget->GetPaddingLeft() + widget->GetPaddingRight();
	box.height -= widget->GetPaddingTop() + widget->GetPaddingBottom();

	// Draw the selected color
	vd->SetBgColor(boxcolor);
#ifdef VEGA_OPPAINTER_SUPPORT
	Border border;
	border.Reset();
	border.SetRadius(3);
	vd->DrawBgColorWithRadius(box, &border);
#else
	vd->DrawBgColor(box);
#endif // VEGA_OPPAINTER_SUPPORT

#ifndef WIDGETS_DONT_DRAW_ITEM_FOCUS_RECT
	if (widget->IsFocused())
		DrawFocusRect(drawrect);
#endif // WIDGETS_DONT_DRAW_ITEM_FOCUS_RECT

	return TRUE;
}
