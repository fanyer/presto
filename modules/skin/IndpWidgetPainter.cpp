/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef SKIN_SUPPORT

#include "modules/skin/IndpWidgetPainter.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpResizeCorner.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpCalendar.h"
#include "modules/widgets/OpSlider.h"
#include "modules/widgets/OpWidgetStringDrawer.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/IndpWidgetInfo.h"
#include "modules/style/css.h"
#include "modules/display/vis_dev.h"
#include "modules/widgets/OpColorField.h"

#ifdef PAGED_MEDIA_SUPPORT
# include "modules/widgets/OpPageControl.h"
#endif // PAGED_MEDIA_SUPPORT

#if defined(MSWIN)
extern BOOL IsSystemWin2000orXP();
BOOL HasTextZoom() {return IsSystemWin2000orXP();}
#elif defined(_X11_)
BOOL HasTextZoom() {return TRUE;}
#elif defined(_MACINTOSH_)
BOOL HasTextZoom() {return TRUE;}
#else
BOOL HasTextZoom() {return FALSE;}
#endif

#define OMENU_IMAGE_SPACING 24

void IndpWidgetPainter::InitPaint(VisualDevice* vd, OpWidget* widget)
{
	this->vd = vd;
	this->widget = widget;

	if (!g_IndpWidgetInfo)
		GetInfo(widget);

	fdef = g_IndpWidgetInfo->GetSystemColor(OP_SYSTEM_COLOR_TEXT);
	color = widget->GetColor();
}

OpWidgetInfo* IndpWidgetPainter::GetInfo(OpWidget* widget)
{
	if (!g_IndpWidgetInfo)
		g_IndpWidgetInfo = OP_NEW(IndpWidgetInfo, ());
	return g_IndpWidgetInfo;
}

BOOL IndpWidgetPainter::DrawScrollbarDirection(const OpRect &drawrect, int direction, BOOL is_down, BOOL is_hovering)
{
	OpScrollbar* scroll = (OpScrollbar*) widget;

	INT32 state = 0;
	INT32 hover_value = 0;

	if (!scroll->CanScroll())
	{
		state |= SKINSTATE_DISABLED;
	}
	else if (!scroll->CanScroll(direction))
	{
		if (g_skin_manager->GetOptionValue("DirectionalScrollbarDisable", 1))
			state |= SKINSTATE_DISABLED;
	}

	if (is_down)
	{
		state |= SKINSTATE_PRESSED;
	}

	if (is_hovering)
	{
		state |= SKINSTATE_HOVER;
		hover_value = 100;
	}
	else if (g_widget_globals->hover_widget == widget)
	{
		state |= SKINSTATE_FOCUSED;
	}

	const char* arrow = NULL;
	
	switch (direction)
	{
		case ARROW_LEFT:
			arrow = "Scrollbar Horizontal Left Skin";
			break;

		case ARROW_RIGHT:
			arrow = "Scrollbar Horizontal Right Skin";
			break;

		case ARROW_UP:
			arrow = "Scrollbar Vertical Up Skin";
			break;

		case ARROW_DOWN:
			arrow = "Scrollbar Vertical Down Skin";
			break;
	}

	widget->SetClipRect(drawrect);
	OP_STATUS status = widget->GetSkinManager()->DrawElement(vd, arrow, drawrect, state, hover_value);
	widget->RemoveClipRect();

	return OpStatus::IsSuccess(status);
}

BOOL IndpWidgetPainter::DrawScrollbarKnob(const OpRect &drawrect, BOOL is_down, BOOL is_hovering)
{
	OpScrollbar* scroll = (OpScrollbar*) widget;

	if (scroll->CanScroll())
	{
		INT32 state = 0;
		INT32 hover_value = 0;

		if (is_down)
		{
			state |= SKINSTATE_PRESSED;
		}

		if (is_hovering)
		{
			state |= SKINSTATE_HOVER;
			hover_value = 100;
		}
		else if (g_widget_globals->hover_widget == widget)
		{
			state |= SKINSTATE_FOCUSED;
		}
		if (scroll->is_moving)
		{
			state |= SKINSTATE_PRESSED;
		}
		const char* knob = scroll->IsHorizontal() ? "Scrollbar Horizontal Knob Skin" : "Scrollbar Vertical Knob Skin";
		widget->SetClipRect(drawrect);
		OP_STATUS status = widget->GetSkinManager()->DrawElement(vd, knob, drawrect, state, hover_value);
		widget->RemoveClipRect();
		return OpStatus::IsSuccess(status);
	}
	return TRUE;
}

BOOL IndpWidgetPainter::DrawScrollbarBackground(const OpRect &drawrect)
{
	OpScrollbar* scroll = (OpScrollbar*) widget;
		const char* sb = scroll->IsHorizontal() ? "Scrollbar Horizontal Skin" : "Scrollbar Vertical Skin";
	return widget->GetSkinManager()->GetSkinElement(sb)!=NULL;
}

#ifdef PAGED_MEDIA_SUPPORT
BOOL IndpWidgetPainter::DrawPageControlBackground(const OpRect &drawrect)
{
	/* If the skin element exists, the skin system has already taken care of
	   the painting. Return TRUE in that case. */

	return widget->GetSkinManager()->GetSkinElement("Page Control Horizontal Skin") != NULL;
}

BOOL IndpWidgetPainter::DrawPageControlButton(const OpRect& drawrect, ARROW_DIRECTION direction, BOOL is_down, BOOL is_hovering)
{
	OpPageControl* page_control = static_cast<OpPageControl*>(widget);
	unsigned int current_page = page_control->GetPage();
	unsigned int page_count = page_control->GetPageCount();
	BOOL prev = direction == ARROW_UP;

	if (!prev)
#ifdef SUPPORT_TEXT_DIRECTION
		if (page_control->IsRTL())
			prev = direction == ARROW_RIGHT;
		else
#endif // SUPPORT_TEXT_DIRECTION
			prev = direction == ARROW_LEFT;

	INT32 state = 0;
	INT32 hover_value = 0;

	if (prev && current_page == 0 ||
		!prev && current_page + 1 == page_count)
	{
		if (g_skin_manager->GetOptionValue("DirectionalPageControlDisable", 1))
			state |= SKINSTATE_DISABLED;
	}
	else
	{
		if (is_down)
			state |= SKINSTATE_PRESSED;

		if (is_hovering)
		{
			state |= SKINSTATE_HOVER;
			hover_value = 100;
		}
		else if (g_widget_globals->hover_widget == widget)
			state |= SKINSTATE_FOCUSED;
	}

	const char* arrow = NULL;

	switch (direction)
	{
		case ARROW_LEFT:
			arrow = "Page Control Horizontal Left Skin";
			break;

		case ARROW_RIGHT:
			arrow = "Page Control Horizontal Right Skin";
			break;

		case ARROW_UP:
			arrow = "Page Control Horizontal Up Skin";
			break;

		case ARROW_DOWN:
			arrow = "Page Control Horizontal Down Skin";
			break;
	}

	if (arrow && widget->GetSkinManager()->GetSkinElement(arrow))
	{
		widget->SetClipRect(drawrect);
		OP_STATUS status = widget->GetSkinManager()->DrawElement(vd, arrow, drawrect, state, hover_value);
		widget->RemoveClipRect();

		return OpStatus::IsSuccess(status);
	}

	return FALSE;
}
#endif // PAGED_MEDIA_SUPPORT

void IndpWidgetPainter::DrawFocusRect(const OpRect &drawrect)
{
	if (!g_skin_manager->GetDrawFocusRect())
		return;
	vd->DrawFocusRect(drawrect);
}

static
void ExpandFocusRect(OpWidget* widget, OpRect& focus_rect, OpRect skin_rect)
{
	// extend focus rect to margins if possible and necessary
	short left, top, right, bottom;
	widget->GetMargins(left, top, right, bottom);
	// ammount of padding from edge of skin to focus rect
	const short pad = 1;

	if (left > 0 && focus_rect.x >= skin_rect.x)
	{
		const INT32 diff = MIN(left, focus_rect.x - skin_rect.x + pad);
		focus_rect.x -= diff;
		focus_rect.width += diff;
	}
	if (right > 0 && focus_rect.x + focus_rect.width <= skin_rect.x + skin_rect.width)
	{
		const INT32 diff = MIN(right, (skin_rect.x + skin_rect.width) - (focus_rect.x + focus_rect.width) + pad);
		focus_rect.width += diff;
	}
	if (top > 0 && focus_rect.y >= skin_rect.y)
	{
		const INT32 diff = MIN(top, focus_rect.y - skin_rect.y + pad);
		focus_rect.y -= diff;
		focus_rect.height += diff;
	}
	if (bottom > 0 && focus_rect.y + focus_rect.height <= skin_rect.y + skin_rect.height)
	{
		const INT32 diff = MIN(bottom, (skin_rect.y + skin_rect.height) - (focus_rect.y + focus_rect.height) + pad);
		focus_rect.height += diff;
	}
}

BOOL IndpWidgetPainter::DrawCheckbox(const OpRect &drawrect, BOOL value)
{
	OpCheckBox* checkbox = NULL;

	INT32 state;
	INT32 hover_value = 0;
	
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_CHECKBOX)
	{
		checkbox = (OpCheckBox*) widget;
		state = checkbox->GetForegroundSkin()->GetState();
		hover_value = checkbox->GetForegroundSkin()->GetHoverValue();
		if (checkbox->GetIndeterminate())
			state |= SKINSTATE_INDETERMINATE;
	}
	else
	{
		state = value ? SKINSTATE_SELECTED : 0;

		if (!widget->IsEnabled())
		{
			state |= SKINSTATE_DISABLED;
		}
	}
	if (OpWidget::GetFocused() == widget && widget->GetType() == OpTypedObject::WIDGET_TYPE_CHECKBOX)
		state |= SKINSTATE_FOCUSED;

	UINT32 text_color = widget->GetForegroundColor(fdef, state);

	const char* skin = "Checkbox Skin";

	OpRect skin_rect;
	OpRect focus_rect;

	widget->GetSkinManager()->GetSize(skin, &skin_rect.width, &skin_rect.height);

	// don't draw using skin when checkbox is smaller than skin
	OpRect widget_rect = widget->GetRect();
	if (widget_rect.width < skin_rect.width || widget_rect.height < skin_rect.height)
		return FALSE;

	if (checkbox && uni_strlen(checkbox->string.Get()) > 0)
	{
		skin_rect.x = drawrect.x;
		skin_rect.y = drawrect.y + (drawrect.height - skin_rect.height) / 2;

		INT32 focus_width = checkbox->string.GetWidth() + 4;
		focus_rect.width = drawrect.width - skin_rect.width - 4;
		focus_rect.height = checkbox->string.GetHeight() + 4;

		focus_rect.x = skin_rect.x + skin_rect.width + 4;
		focus_rect.y = drawrect.y + (drawrect.height - focus_rect.height - 1) / 2;
		
		if (widget->GetRTL())
		{
			skin_rect.x = drawrect.x + drawrect.width - skin_rect.width;
			focus_rect.x = drawrect.x;
		}

		INT32 indent = focus_rect.width - focus_width;
		focus_rect.width = focus_width;
		if (indent > 0)
		{
			if (widget->font_info.justify == JUSTIFY_RIGHT)
				focus_rect.x += indent;
			if (widget->font_info.justify == JUSTIFY_CENTER)
				focus_rect.x += indent / 2;
		}

		OpRect text_rect = focus_rect.InsetBy(2, 2);

		checkbox->string.Draw(text_rect, vd, text_color);
	}
	else
	{
		focus_rect.width = skin_rect.width + 4;
		focus_rect.height = skin_rect.height + 4;

		focus_rect.x = drawrect.x + (drawrect.width - focus_rect.width) / 2;
		focus_rect.y = drawrect.y + (drawrect.height - focus_rect.height) / 2;

		skin_rect = focus_rect.InsetBy(2, 2);
		focus_rect = drawrect;

		ExpandFocusRect(widget, focus_rect, skin_rect);
	}

	OP_STATUS status = widget->GetSkinManager()->DrawElement(vd, skin, skin_rect, state, hover_value);

	if (checkbox && widget->IsFocused() && widget->HasFocusRect() && OpStatus::IsSuccess(status))
	{
		DrawFocusRect(focus_rect);
	}

	return OpStatus::IsSuccess(status);
}

BOOL IndpWidgetPainter::DrawRadiobutton(const OpRect &drawrect, BOOL value)
{
	OpRadioButton* radiobutton = NULL;
	
	INT32 state;
	INT32 hover_value = 0;

	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_RADIOBUTTON)
	{
		radiobutton = (OpRadioButton*) widget;

		state = radiobutton->GetForegroundSkin()->GetState();
		hover_value = radiobutton->GetForegroundSkin()->GetHoverValue();
	}
	else
	{
		state = value ? SKINSTATE_SELECTED : 0;

		if (!widget->IsEnabled())
		{
			state |= SKINSTATE_DISABLED;
		}
	}
	if (OpWidget::GetFocused() == widget)
		state |= SKINSTATE_FOCUSED;

	UINT32 text_color = widget->GetForegroundColor(fdef, state);

	const char* skin = "Radio Button Skin";

	OpRect skin_rect;
	OpRect focus_rect;

	widget->GetSkinManager()->GetSize(skin, &skin_rect.width, &skin_rect.height);

	// don't draw using skin when radiobutton is smaller than skin
	OpRect widget_rect = widget->GetRect();
	if (widget_rect.width < skin_rect.width || widget_rect.height < skin_rect.height)
		return FALSE;

	if (radiobutton && uni_strlen(radiobutton->string.Get()) > 0)
	{
		skin_rect.x = drawrect.x;
		skin_rect.y = drawrect.y + (drawrect.height - skin_rect.height) / 2;

		INT32 focus_width = radiobutton->string.GetWidth() + 4;
		focus_rect.width = drawrect.width - skin_rect.width - 4;
		focus_rect.height = radiobutton->string.GetHeight() + 4;

		focus_rect.x = skin_rect.x + skin_rect.width + 4;
		focus_rect.y = drawrect.y + (drawrect.height - focus_rect.height - 1) / 2;

		if (widget->GetRTL())
		{
			skin_rect.x = drawrect.x + drawrect.width - skin_rect.width;
			focus_rect.x = drawrect.x;
		}

		INT32 indent = focus_rect.width - focus_width;
		focus_rect.width = focus_width;
		if (indent > 0)
		{
			if (widget->font_info.justify == JUSTIFY_RIGHT)
				focus_rect.x += indent;
			if (widget->font_info.justify == JUSTIFY_CENTER)
				focus_rect.x += indent / 2;
		}

		OpRect text_rect = focus_rect.InsetBy(2, 2);

		radiobutton->string.Draw(text_rect, vd, text_color);
	}
	else
	{
		focus_rect.width = skin_rect.width + 4;
		focus_rect.height = skin_rect.height + 4;

		focus_rect.x = drawrect.x + (drawrect.width - focus_rect.width) / 2;
		focus_rect.y = drawrect.y + (drawrect.height - focus_rect.height) / 2;

		skin_rect = focus_rect.InsetBy(2, 2);
		focus_rect = drawrect;

		ExpandFocusRect(widget, focus_rect, skin_rect);
	}

	OP_STATUS status = widget->GetSkinManager()->DrawElement(vd, skin, skin_rect, state, hover_value);

	if (radiobutton && widget->IsFocused() && widget->HasFocusRect() && OpStatus::IsSuccess(status))
	{
		DrawFocusRect(focus_rect);
	}

	return OpStatus::IsSuccess(status);
}

BOOL IndpWidgetPainter::DrawListbox(const OpRect &drawrect)
{
		const char* lb = "Listbox Skin";

	return widget->GetSkinManager()->GetSkinElement(lb) != NULL;
}

BOOL IndpWidgetPainter::DrawButton(const OpRect &rect, BOOL pressed, BOOL is_default)
{
	OpSkinManager* skin_manager = widget->GetSkinManager();
	OpButton* button = (OpButton*) widget;

	widget->SetClipRect(rect);

	INT32 left = 0, top = 0, right = 0, bottom = 0;

	button->GetPadding(&left, &top, &right, &bottom);

	OpRect content_rect(rect.x + left, rect.y + top, rect.width - left - right, rect.height - top - bottom);

	OpButton::ButtonStyle style = button->GetForegroundSkin()->HasContent() ? button->m_button_style : OpButton::STYLE_TEXT;

	INT32 spacing = 0;

#ifdef QUICK
	button->GetSpacing(&spacing);
#endif

	if (button->m_dropdown_image.HasContent())
	{
		INT32 image_width;
		INT32 image_height;

		button->m_dropdown_image.GetSize(&image_width, &image_height);

		INT32 dropdown_left, dropdown_top, dropdown_right, dropdown_bottom;

		button->m_dropdown_image.GetMargin(&dropdown_left, &dropdown_top, &dropdown_right, &dropdown_bottom);

		INT32 auto_dropdown_padding = (left + right) / 2;

		content_rect.width -= image_width + auto_dropdown_padding + dropdown_left + dropdown_right;
		
		OpRect image_rect(content_rect.x + content_rect.width, content_rect.y, rect.width - content_rect.width - auto_dropdown_padding, content_rect.height);
		
		image_rect = button->m_dropdown_image.CalculateScaledRect(image_rect, TRUE, TRUE);

		if (style == OpButton::STYLE_TEXT)
		{
			int rest = content_rect.width - button->string.GetWidth() - 4;
	
			if (button->font_info.justify == JUSTIFY_CENTER && rest > 0)
			{
				image_rect.x -= rest / 2;
			}
		}

		if (button->GetRTL())
			image_rect.x = rect.x + (rect.width - image_rect.Right());

		button->m_dropdown_image.Draw(vd, image_rect);
	}

	button->GetForegroundSkin()->GetMargin(&left, &top, &right, &bottom);

	if (right < 0 && style != OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT)
	{
		right = 0;
	}

	if (bottom < 0 && style != OpButton::STYLE_IMAGE_AND_TEXT_BELOW)
	{
		bottom = 0;
	}

	OpRect image_rect(content_rect.x + left, content_rect.y + top, content_rect.width - left - right, content_rect.height - top - bottom);
	OpRect text_rect = content_rect;

	switch (style)
	{
		case OpButton::STYLE_TEXT:
		{
			if (button->GetButtonType() == OpButton::TYPE_OMENU)
			{
				text_rect.x += OMENU_IMAGE_SPACING;
				text_rect.width -= OMENU_IMAGE_SPACING;
			}
			break;
		}

		case OpButton::STYLE_IMAGE_AND_TEXT_CENTER:
		{
			image_rect = button->GetForegroundSkin()->CalculateScaledRect(image_rect, FALSE, TRUE);
			image_rect.x = content_rect.x + (content_rect.width - image_rect.width - button->string.GetWidth() - spacing)/2;
			text_rect.x  = image_rect.x + image_rect.width + spacing;
			text_rect.width = button->string.GetWidth();
			break;
		}

		case OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT:
		{
			image_rect = button->GetForegroundSkin()->CalculateScaledRect(image_rect, FALSE, TRUE);
			int image_space = image_rect.width;
			if (button->GetButtonType() == OpButton::TYPE_OMENU)
				image_space = MAX(OMENU_IMAGE_SPACING, image_space);
			text_rect.x = image_rect.x + image_space + right + spacing;
			text_rect.width = content_rect.x + content_rect.width - text_rect.x;
			break;
		}

		case OpButton::STYLE_IMAGE_AND_TEXT_ON_LEFT:
		{
			image_rect = button->GetForegroundSkin()->CalculateScaledRect(image_rect, FALSE, TRUE);
			text_rect.x = image_rect.x;
			text_rect.width = content_rect.x + content_rect.width - text_rect.x - image_rect.width;
			image_rect.x = content_rect.x + content_rect.width - image_rect.width;
			break;
		}

		case OpButton::STYLE_IMAGE:
		{
			image_rect = button->GetForegroundSkin()->CalculateScaledRect(image_rect, TRUE, TRUE);
			break;
		}

		case OpButton::STYLE_IMAGE_AND_TEXT_BELOW:
		{
			image_rect = button->GetForegroundSkin()->CalculateScaledRect(image_rect, TRUE, FALSE);
			text_rect.y = image_rect.y + image_rect.height + bottom + spacing;
			text_rect.height = content_rect.y + content_rect.height - text_rect.y;
			break;
		}
	}

	if (widget->GetRTL())
	{
		image_rect.x = rect.x + (rect.width - image_rect.x - image_rect.width);
		text_rect.x = rect.x + (rect.width - text_rect.x - text_rect.width);
	}

	// draw

	INT32 state = button->GetBorderSkin()->GetState();
	UINT32 color = button->GetForegroundColor(g_IndpWidgetInfo->GetSystemColor(state & SKINSTATE_DISABLED ? OP_SYSTEM_COLOR_UI_DISABLED_FONT : OP_SYSTEM_COLOR_UI_FONT), state);

	if (!button->IsForm() && button->GetAction() && button->GetShowShortcut())
	{
		OpString shortcut_string;
		button->GetShortcutString(shortcut_string);
		if (!shortcut_string.IsEmpty())
		{
			OpWidgetString shortcut;
			shortcut.Set(shortcut_string, button);
			INT32 sw = shortcut.GetWidth();

			if (button->string.GetWidth() + sw < text_rect.width)
			{
				OpRect sr(text_rect.x + text_rect.width - sw, text_rect.y, sw, text_rect.height);
				shortcut.Draw(sr, vd, color);
				text_rect.width -= sw;
			}
		}
	}
#ifdef ACTION_SHOW_MENU_SECTION_ENABLED
	if (!button->IsForm() && button->GetButtonExtra() == OpButton::EXTRA_SUB_MENU)
	{
		if (OpSkinElement* element = widget->GetSkinManager()->GetSkinElement("Menu Right Arrow", SKINTYPE_DEFAULT, SKINSIZE_DEFAULT, TRUE))
		{
			INT32 hover_value = widget->GetForegroundSkin()->GetHoverValue();
			INT32 sw = 0, sh = 0;
			element->GetSize(&sw, &sh, 0);
			element->Draw(vd, OpRect(text_rect.x + text_rect.width - sw, text_rect.y + (text_rect.height - sh) / 2, sw, sh), state, hover_value, NULL);
			text_rect.width -= sw;
		}
	}
#endif // ACTION_SHOW_MENU_SECTION_ENABLED

	if (style != OpButton::STYLE_TEXT)
	{
		button->GetForegroundSkin()->Draw(vd, image_rect);
	}

	if (style != OpButton::STYLE_IMAGE)
	{
		BOOL draw_focus_rect = widget->IsFocused() && widget->HasFocusRect();
		if (!draw_focus_rect)
			draw_focus_rect = button->HasForcedFocusedLook() && widget->HasFocusRect();
		if (draw_focus_rect)
		{
			if (text_rect.IsEmpty())
				DrawFocusRect(rect.InsetBy(3, 3));
			else
				DrawFocusRect(text_rect.InsetBy(1, 1));
		}

		if (button->IsForm())
		{
			OpRect inner_rect;

			//DSK-364302, DSK-367673
			short left_b, right_b, top_b, bottom_b;
			button->GetBorders(left_b, top_b, right_b, bottom_b);
			inner_rect.Set(rect.x + left_b, rect.y + top_b, rect.width - left_b - right_b, rect.height - top_b - bottom_b);

			button->PaintContent(inner_rect, color);
		}
		else
		{
			INT32 size = button->font_info.size;

			BOOL zoom = FALSE;

			if (HasTextZoom() && style == OpButton::STYLE_IMAGE_AND_TEXT_BELOW && g_pccore->GetIntegerPref(PrefsCollectionCore::SpecialEffects))
			{
				skin_manager->GetTextZoom(button->GetBorderSkin()->GetImage(), &zoom, 0, button->GetBorderSkin()->GetType());

				if (zoom)
				{
					button->font_info.size = size * button->m_hover_counter / 100;
					button->string.NeedUpdate();
				}
			}

			BOOL underline = FALSE;

			skin_manager->GetTextUnderline(button->GetBorderSkin()->GetImage(), &underline, state, button->GetBorderSkin()->GetType());

			INT32 button_text_padding = skin_manager->GetOptionValue("Button Text Padding", 2);
			INT32 local_button_text_padding = -1;

			button->GetBorderSkin()->GetButtonTextPadding(&local_button_text_padding);

			if(local_button_text_padding != -1)
			{
				button_text_padding = local_button_text_padding;
			}

			ELLIPSIS_POSITION ellipsis_position = button->IsForm() ? ELLIPSIS_NONE : button->GetEllipsis();
			text_rect = text_rect.InsetBy(button_text_padding, button_text_padding);
#ifdef WIDGETS_AUTOSCROLL_HIDDEN_CONTENT
			INT32 x_scroll = button->GetXScroll();
			if (x_scroll > button->string.GetWidth() - text_rect.width)
				x_scroll = button->string.GetWidth() - text_rect.width;
			if (button->string.GetWidth() < text_rect.width)
				x_scroll = 0;
#else
			INT32 x_scroll = 0;
#endif // WIDGETS_AUTOSCROLL_HIDDEN_CONTENT

			OpWidgetStringDrawer string_drawer;
			string_drawer.SetEllipsisPosition(ellipsis_position);
			string_drawer.SetUnderline(underline);
			string_drawer.SetXScroll(x_scroll);
			string_drawer.Draw(&button->string, text_rect, vd, color);

			button->font_info.size = size;

			if (zoom)
			{
				button->string.NeedUpdate();
			}
		}
	}

	widget->RemoveClipRect();

	return TRUE;
}

BOOL IndpWidgetPainter::DrawEdit(const OpRect &drawrect)
{
	OpEdit* edit = ((OpEdit*)widget);

	OpRect rect(drawrect);

	BOOL draw_ghost = (!widget->IsFocused() || edit->GetShowGhostTextWhenFocused()) && edit->GetTextLength() == 0 && edit->ghost_string.Get() && *edit->ghost_string.Get();

	OpRect inner_rect(rect);

	g_IndpWidgetInfo->AddBorder(widget, &inner_rect);

	UINT32 fcol = color.use_default_foreground_color ? fdef : color.foreground_color;

	if (edit->GetFlatMode())
	{
		INT32 state = !widget->IsEnabled() || draw_ghost ? SKINSTATE_DISABLED : 0;
		fcol = edit->GetForegroundColor(g_IndpWidgetInfo->GetSystemColor(state & SKINSTATE_DISABLED ? OP_SYSTEM_COLOR_UI_DISABLED_FONT : OP_SYSTEM_COLOR_UI_FONT), state);
	}
	else
	{
		if (widget->IsEnabled() == FALSE || draw_ghost)
			fcol = g_IndpWidgetInfo->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
	}

	// draw image first
	if (edit->GetForegroundSkin()->HasContent())
	{
		inner_rect.x += 2;
		inner_rect.width -= 2;

		OpRect rect = edit->GetForegroundSkin()->CalculateScaledRect(inner_rect, FALSE, TRUE);
		edit->GetForegroundSkin()->Draw(vd, rect);

		//This compensation is done in PaintContent
		//UINT32 image_width = rect.width + 4;
		//inner_rect.x += image_width;
		//inner_rect.width -= image_width;
	}
	edit->PaintContent(inner_rect, fcol, draw_ghost);

	return TRUE;
}

BOOL IndpWidgetPainter::DrawDropDownHint(const OpRect &drawrect)
{
	DropDownEdit* edit = ((DropDownEdit*)widget);

	OpRect rect(drawrect);

	if (!( widget->IsFocused() && edit->GetShowGhostTextWhenFocused() && edit->GetTextLength() > 0
		   && edit->GetHintText() && *edit->GetHintText()))
		return TRUE;

	OpRect inner_rect(rect);

	g_IndpWidgetInfo->AddBorder(widget, &inner_rect);
	UINT32 hcol = g_IndpWidgetInfo->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);

	edit->PaintHint(inner_rect,hcol);

	return TRUE;
}


BOOL IndpWidgetPainter::DrawDropdown(const OpRect &drawrect)
{
	OpDropDown* dropdown = (OpDropDown*) widget;

	INT32 left = 0;
	INT32 top = 0;
	INT32 right = 0;
	INT32 bottom = 0;
	BOOL lh_ui = widget->LeftHandedUI();

	const char *drop_down_button_str;

	if(lh_ui)
	{
		drop_down_button_str = "Dropdown Left Button Skin";
		if(!widget->GetSkinManager()->GetSkinElement(drop_down_button_str, SKINTYPE_DEFAULT, SKINSIZE_DEFAULT, TRUE))
			return FALSE;
	}
	else
	{
		drop_down_button_str = "Dropdown Button Skin";
	}

	OpStringItem* item = dropdown->GetItemToPaint();

	if (item && item->bg_col != USE_DEFAULT_COLOR)
		return FALSE;

	if (widget->GetBorderSkin())
		widget->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom); 

	OpRect inner_rect(drawrect);

	if(!widget->HasCssBorder())
		inner_rect.Set(drawrect.x + left, drawrect.y + top, drawrect.width - left - right, drawrect.height - top - bottom);

	if (inner_rect.width < 0 || inner_rect.height < 0)
		return FALSE;

	if (dropdown->GetForegroundSkin()->HasContent())
	{
		inner_rect.x += 2;
		inner_rect.width -= 2;

		OpRect image_rect = dropdown->GetForegroundSkin()->CalculateScaledRect(inner_rect, FALSE, TRUE);
		if (lh_ui)
			image_rect.x = drawrect.Right() - image_rect.Right() + drawrect.x;
		dropdown->GetForegroundSkin()->Draw(vd, image_rect);

		UINT32 image_width = image_rect.width + 4;

		inner_rect.x += image_width;
		inner_rect.width -= image_width;
	}

	if(lh_ui)
		inner_rect.width -= GetInfo()->GetDropdownLeftButtonWidth(widget);
	else
		inner_rect.width -= GetInfo()->GetDropdownButtonWidth(widget);

	if (!dropdown->edit && (item || dropdown->ghost_string.Get()))
	{
		OpRect item_rect = inner_rect;
		if (lh_ui)
			item_rect.x = drawrect.Right() - item_rect.Right() + drawrect.x;

		dropdown->SetClipRect(item_rect);
		if (item)
		{
			dropdown->GetPainterManager()->DrawItem(item_rect, item, FALSE, TRUE);
		}
		else
		{
			OpRect string_rect = inner_rect;
			dropdown->AddMargin(&string_rect);
			if (lh_ui)
				string_rect.x = drawrect.Right() - string_rect.Right() + drawrect.x;
			dropdown->ghost_string.Draw(string_rect, vd, g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_DISABLED_FONT));
		}
		dropdown->RemoveClipRect();
	}

	// Draw button

	if (!dropdown->m_dropdown_packed.show_button)
		return TRUE;

	OpRect button_rect = drawrect;

	widget->GetSkinManager()->GetMargin(drop_down_button_str, &left, &top, &right, &bottom);

	button_rect.y += top;
	button_rect.height -= top + bottom;
	button_rect.x += button_rect.width - GetInfo()->GetDropdownButtonWidth(widget) - right;
	button_rect.width = GetInfo()->GetDropdownButtonWidth(widget);

	INT32 state = 0;

	if (!widget->IsEnabled())
		state |= SKINSTATE_DISABLED;

	if (dropdown->m_dropdown_window)
		state |= SKINSTATE_PRESSED;

	if (widget->IsMiniSize())
		state |= SKINSTATE_MINI;

	INT32 hover_value = 0;

	if (dropdown->m_dropdown_packed.is_hovering_button)
	{
		state |= SKINSTATE_HOVER;
		hover_value = 100;
	}

	if (lh_ui)
		button_rect.x = drawrect.Right() - button_rect.Right() + drawrect.x;
	return OpStatus::IsSuccess(widget->GetSkinManager()->DrawElement(vd, drop_down_button_str, button_rect, state, hover_value));
}

BOOL IndpWidgetPainter::DrawItem(const OpRect &drawrect, OpStringItem* item, BOOL is_listbox, BOOL is_focused_item, int indent)
{
	return FALSE;
}

BOOL IndpWidgetPainter::DrawResizeCorner(const OpRect &drawrect, BOOL active)
{
	return OpStatus::IsSuccess(
		g_skin_manager->DrawElement(vd, "Resize Corner Skin", drawrect));
}

BOOL IndpWidgetPainter::DrawWidgetResizeCorner(const OpRect &drawrect, BOOL is_active, SCROLLBAR_TYPE scrollbar)
{
	// No skin support for inactive resize corners, and colored corners should be taken
	// care of by the CSS painter.
	if (!is_active || scrollbar == SCROLLBAR_CSS)
		return FALSE;

	const char *skin = "Element Resize Corner Skin";

	switch (scrollbar)
	{
	case SCROLLBAR_HORIZONTAL:
		skin = "Element Resize Corner Skin Scroll Horizontal";
		break;
	case SCROLLBAR_VERTICAL:
		skin = "Element Resize Corner Skin Scroll Vertical";
		break;
	case SCROLLBAR_BOTH:
		skin = "Element Resize Corner Skin Scroll Both";
		break;
	}

	return OpStatus::IsSuccess(g_skin_manager->DrawElement(vd, skin, drawrect));
}

#ifdef QUICK

BOOL IndpWidgetPainter::DrawTreeViewRow(const OpRect &drawrect, BOOL focused)
{
	UINT32 selected_bgcol;

	if (widget->IsFocused())
	{
		selected_bgcol = g_IndpWidgetInfo->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
	}
	else
	{
		selected_bgcol = g_IndpWidgetInfo->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS);
	}

	vd->SetColor32(selected_bgcol);
	vd->FillRect(drawrect);

	if (widget->IsFocused() && focused)
	{
		DrawFocusRect(drawrect);
	}
	
	return TRUE;
}

BOOL IndpWidgetPainter::DrawProgressbar(const OpRect &drawrect, double percent, INT32 progress_when_total_unknown, OpWidgetString* string, const char *empty_skin, const char *full_skin)
{
	if (!empty_skin || !*empty_skin)
		empty_skin = "Progress Empty Skin";
	if (!full_skin || !*full_skin)
		full_skin  = "Progress Full Skin";

	UINT32 full_color = g_op_ui_info->GetUICSSColor(CSS_VALUE_HighlightText);
	UINT32 empty_color = g_op_ui_info->GetUICSSColor(CSS_VALUE_ButtonText);

	g_skin_manager->GetTextColor(full_skin, &full_color);
	g_skin_manager->GetTextColor(empty_skin, &empty_color);

	OpRect text_rect = drawrect;

	widget->GetBorderSkin()->AddPadding(text_rect);

	OP_STATUS status = OpStatus::OK;
	if (percent == 0 && progress_when_total_unknown)
	{
		OpRect left_rect = drawrect;
		OpRect middle_rect = drawrect;
		OpRect right_rect = drawrect;

		left_rect.width = (progress_when_total_unknown / 1024) % drawrect.width;
		
		middle_rect.x = left_rect.x + left_rect.width;
		middle_rect.width = 10;

		right_rect.x = middle_rect.x + middle_rect.width;
		right_rect.width = drawrect.width - left_rect.width - middle_rect.width;

		if (widget->GetRTL())
		{
			left_rect.x = drawrect.Right() - left_rect.Right() + drawrect.x;
			middle_rect.x = drawrect.Right() - middle_rect.Right() + drawrect.x;
			right_rect.x = drawrect.Right() - right_rect.Right() + drawrect.x;
		}

		if (left_rect.width > 0)
		{
			widget->SetClipRect(left_rect);
			status = g_skin_manager->DrawElement(vd, empty_skin, drawrect, 0, 0, &left_rect);

			if (string && OpStatus::IsSuccess(status))
			{
				string->Draw(text_rect, vd, empty_color);
			}
			widget->RemoveClipRect();
		}

		if (middle_rect.width > 0 && OpStatus::IsSuccess(status))
		{
			widget->SetClipRect(middle_rect);
			status = g_skin_manager->DrawElement(vd, full_skin, drawrect, 0, 0, &middle_rect);

			if (string && OpStatus::IsSuccess(status))
			{
				string->Draw(text_rect, vd, full_color);
			}
			widget->RemoveClipRect();
		}

		if (right_rect.width > 0 && OpStatus::IsSuccess(status))
		{
			widget->SetClipRect(right_rect);
			status = g_skin_manager->DrawElement(vd, empty_skin, drawrect, 0, 0, &right_rect);

			if (string && OpStatus::IsSuccess(status))
			{
				string->Draw(text_rect, vd, empty_color);
			}
			widget->RemoveClipRect();
		}
	}
	else
	{
		OpRect left_rect = drawrect;
		OpRect right_rect = drawrect;

		left_rect.width = static_cast<INT32>(drawrect.width * percent / 100);
		right_rect.x = left_rect.x + left_rect.width;
		right_rect.width = drawrect.width - left_rect.width;

		if (widget->GetRTL())
		{
			left_rect.x = drawrect.Right() - left_rect.Right() + drawrect.x;
			right_rect.x = drawrect.Right() - right_rect.Right() + drawrect.x;
		}

		if (left_rect.width > 0)
		{
			widget->SetClipRect(left_rect);
			status = g_skin_manager->DrawElement(vd, full_skin, drawrect, 0, 0, &left_rect);

			if (string && OpStatus::IsSuccess(status))
			{
				string->Draw(text_rect, vd, full_color);
			}

			widget->RemoveClipRect();
		}

		if (right_rect.width > 0 && OpStatus::IsSuccess(status))
		{
			widget->SetClipRect(right_rect);
			status = g_skin_manager->DrawElement(vd, empty_skin, drawrect, 0, 0, &right_rect);

			if (string && OpStatus::IsSuccess(status))
			{
				string->Draw(text_rect, vd, empty_color);
			}

			widget->RemoveClipRect();
		}
	}

	return OpStatus::IsSuccess(status);
}

#endif // QUICK

BOOL IndpWidgetPainter::DrawPopupableString(const OpRect &drawrect, BOOL is_hovering_button)
{
	INT32 left = 0;
	INT32 top = 0;
	INT32 right = 0;
	INT32 bottom = 0;

	OpRect button_rect = drawrect;

	widget->GetSkinManager()->GetMargin("Dropdown Button Skin", &left, &top, &right, &bottom);

	button_rect.y += top;
	button_rect.height -= top + bottom;
	button_rect.x += button_rect.width - GetInfo()->GetDropdownButtonWidth(widget) - right;
	button_rect.width = GetInfo()->GetDropdownButtonWidth(widget);

	if (widget->GetBorderSkin())
		widget->GetBorderSkin()->GetPadding(&left, &top, &right, &bottom); 

	OpRect inner_rect(drawrect);
	if(!widget->HasCssBorder())
		inner_rect.Set(drawrect.x + left, drawrect.y + top, button_rect.x - drawrect.x - left, drawrect.height - top - bottom);

	if (inner_rect.width < 0 || inner_rect.height < 0)
		return FALSE;

	if (widget->GetForegroundSkin()->HasContent())
	{
		OpRect image_rect = widget->GetForegroundSkin()->CalculateScaledRect(inner_rect, FALSE, TRUE);
		widget->GetForegroundSkin()->Draw(vd, image_rect);

		UINT32 image_width = image_rect.width + 4;

		inner_rect.x += image_width;
		inner_rect.width -= image_width;
	}

	OP_STATUS status;
	if (widget->GetType() == OpTypedObject::WIDGET_TYPE_CALENDAR)
	{
		OpString text;
		status = widget->GetText(text);
		if (OpStatus::IsSuccess(status))
		{
			// Hack: Reuse the list item class
			OpStringItem item;
			status = item.SetString(text.CStr(), widget); // XXX empty string?
			if (OpStatus::IsSuccess(status))
			{
				widget->SetClipRect(inner_rect);
				widget->GetPainterManager()->DrawItem(inner_rect, &item, FALSE, TRUE);
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

	INT32 state = 0;
	INT32 hover_value = 0;

	if (is_hovering_button)
	{
		state |= SKINSTATE_HOVER;
		hover_value = 100;
	}

	status = widget->GetSkinManager()->DrawElement(vd, "Dropdown Button Skin", button_rect, state, hover_value);

	return OpStatus::IsSuccess(status);
}

/* virtual */
BOOL IndpWidgetPainter::DrawSlider(const OpRect& rect, BOOL horizontal, double min, double max, double pos, BOOL highlighted, BOOL pressed_knob, OpRect& out_knob_position, OpPoint& out_start_track, OpPoint& out_end_track)
{
	return FALSE;
}

/* virtual */
BOOL IndpWidgetPainter::DrawSliderFocus(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& focus_rect)
{
	return FALSE;
}

/* virtual */
BOOL IndpWidgetPainter::DrawSliderTrack(const OpRect& slider_rect, const OpRect& track_rect)
{
	OpSlider *slider = (OpSlider *) widget;
	INT32 state = GetSliderState(slider);
	INT32 hover_value = 0;
	return OpStatus::IsSuccess(g_skin_manager->DrawElement(vd, slider->GetTrackSkin(slider->IsHorizontal()), track_rect, state, hover_value));
}

/* virtual */
BOOL IndpWidgetPainter::DrawSliderKnob(const OpRect& slider_rect, const OpRect& track_rect, const OpRect& knob_rect)
{
	OpSlider *slider = (OpSlider *) widget;
	INT32 state = GetSliderState(slider);
	INT32 hover_value = state & SKINSTATE_HOVER ? 100 : 0;
	return OpStatus::IsSuccess(g_skin_manager->DrawElement(vd, slider->GetKnobSkin(slider->IsHorizontal()), knob_rect, state, hover_value));
}

INT32 IndpWidgetPainter::GetSliderState(OpSlider *slider)
{
	INT32 state = 0;
	if (!widget->IsEnabled())
		state |= SKINSTATE_DISABLED;
	if (slider->IsPressingKnob())
		state |= SKINSTATE_PRESSED;
	if (slider->IsHoveringKnob())
		state |= SKINSTATE_HOVER;
	if (widget->IsFocused() && widget->HasFocusRect())
		state |= SKINSTATE_FOCUSED;
	if (widget->GetRTL())
		state |= SKINSTATE_RTL;
	return state;
}

#endif // SKIN_SUPPORT
