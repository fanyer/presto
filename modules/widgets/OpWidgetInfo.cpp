/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpResizeCorner.h"

#ifdef SKIN_SUPPORT
#include "modules/skin/OpSkinManager.h"
#endif

#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpScreenInfo.h"

#include "modules/display/vis_dev.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

void OpWidgetInfo::GetPreferedSize(OpWidget* widget, OpTypedObject::Type type, INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	BOOL has_css_border = widget->HasCssBorder();
	
	switch(widget->GetType())
	{
	case OpTypedObject::WIDGET_TYPE_SCROLLBAR:
		{
			BOOL horizontal = ((OpScrollbar*)widget)->horizontal;
			*w = horizontal ? *w : GetVerticalScrollbarWidth();
			*h = horizontal ? GetHorizontalScrollbarHeight() : *h;
		}
		break;
	case OpTypedObject::WIDGET_TYPE_BUTTON:
		{
			OpButton* btn = (OpButton*) widget;
			if (!widget->IsForm() || widget->parent)
			{
				*w = btn->string.GetWidth() + widget->GetVisualDevice()->GetFontAveCharWidth() * 3;
				*h = btn->string.GetHeight() + btn->string.GetHeight() / 3;
			}
		}
		break;
	case OpTypedObject::WIDGET_TYPE_DROPDOWN:
		{
			OpDropDown* dr = (OpDropDown*) widget;
			ItemHandler* ih = &dr->ih;
			if (ih->widest_item)
			{
				*w = ih->widest_item + GetDropdownButtonWidth(widget);
				*h = ih->highest_item;
				if (!has_css_border)
				{
					*w += 4;
					*h += 4;
				}

				// Add widgets padding
				INT32 left = 0;
				INT32 top = 0;
				INT32 right = 0;
				INT32 bottom = 0;

				dr->GetPadding(&left, &top, &right, &bottom);

				// Remove the item-padding from padding since it is already included from widest_item and highest_item.
				INT32 item_margin_left, item_margin_top, item_margin_right, item_margin_bottom;
				GetItemMargin(item_margin_left, item_margin_top, item_margin_right, item_margin_bottom);
				left = MAX(0, left - item_margin_left);
				top = MAX(0, top - item_margin_top);
				right = MAX(0, right - item_margin_right);
				bottom = MAX(0, bottom - item_margin_bottom);

				*w += left + right;
				*h += top + bottom;
			}
		}
		break;
	case OpTypedObject::WIDGET_TYPE_LISTBOX:
		{
			OpListBox* lb = (OpListBox*) widget;
			ItemHandler* ih = &lb->ih;

			// listboxes sometimes have a diffrent border width than
			// other widgets - see GetBorders
			INT32 left, top, right, bottom;
			GetBorders(lb, left, top, right, bottom);

			if (ih->widest_item)
			{
				*w = lb->ih.widest_item + GetVerticalScrollbarWidth();
				if (!has_css_border)
					*w += left + right;
#ifdef WIDGETS_CHECKBOX_MULTISELECT
				if (ih->is_multiselectable)
				{
#ifdef SKIN_SUPPORT
					OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Checkbox Skin");
					if (skin_elm)
					{
						int tmpw, tmph;
						skin_elm->GetSize(&tmpw, &tmph, 0);
						*w += tmpw;
					}
					else
#endif
						*w += 9;
				}
#endif
			}
			if(rows)
			{
				int itemheight = ih->highest_item;
				if (itemheight == 0) // No item is added yet, so take the value we would get if there is no fontswitching
				{
					INT32 margin_left, margin_top, margin_right, margin_bottom;
					GetItemMargin(margin_left, margin_top, margin_right, margin_bottom);
					itemheight = widget->GetVisualDevice()->GetFontHeight() + margin_top + margin_bottom;
				}

				*h = itemheight * rows;
				if (!has_css_border)
					*h += top + bottom;
			}
		}
		break;
	case OpTypedObject::WIDGET_TYPE_RADIOBUTTON:
	case OpTypedObject::WIDGET_TYPE_CHECKBOX:
		{
			// layout code expects
			// * border  box, if borders are _not_ specified (has_css_border == FALSE)
			// * padding box, if borders are specified       (has_css_border == TRUE)
			// where
			// * border  box is content + padding + border
			// * padding box is content + padding
			// for this to make sense content has to be kept positive.

			// size of border box - 13px should correspond to what
			// other browsers use, see CORE-19950.
			const unsigned short border_box_size = 13;
			*w = border_box_size;
			*h = border_box_size;

			short border_left, border_top, border_right, border_bottom;
			widget->GetBorders(border_left, border_top, border_right, border_bottom);
			const short horizontal_border = border_left + border_right;
			const short vertical_border =   border_top  + border_bottom;

			short horizontal_border_padding = widget->GetPaddingLeft() + widget->GetPaddingRight();
			short vertical_border_padding   = widget->GetPaddingTop()  + widget->GetPaddingBottom();
			// layout expects border box
			if (!has_css_border)
			{
				horizontal_border_padding += horizontal_border;
				vertical_border_padding   += vertical_border;
			}
			// layout expects padding box
			else
			{
				*w -= horizontal_border;
				*h -= vertical_border;
			}
			// [horizontal|vertical]_border_padding now contains:
			// * padding + border when layout expects border box
			// * padding when layout expects padding box

			// minimum content size - must be kept positive or layout
			// breaks (see eg FormBoxToContentBox). this reflects
			// layout's perspective only and decides when we start to
			// grow the border box, enforcing a minimum mark size is
			// done in widget painter.
			const unsigned short min_content_size = 1;
			if (*w < min_content_size + horizontal_border_padding)
				*w = min_content_size + horizontal_border_padding;
			if (*h < min_content_size + vertical_border_padding)
				*h = min_content_size + vertical_border_padding;
		}
		break;
	case OpTypedObject::WIDGET_TYPE_EDIT:
		{
		}
		break;
	case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT:
		{
			if (cols < 1)
				cols = 1;
			if (rows < 1)
				rows = 1;
			if (cols)
			{
				*w = cols * widget->GetVisualDevice()->GetFontAveCharWidth();
				*w += GetVerticalScrollbarWidth();
				if (!has_css_border)
					*w += 4;
				*w += widget->GetPaddingLeft() + widget->GetPaddingRight();
			}
			if (rows)
			{
				OpMultilineEdit* multiedit = (OpMultilineEdit *) widget;
				// Last line is always at least the visible lineheight.
				*h = (rows - 1) * multiedit->GetLineHeight() + multiedit->GetVisibleLineHeight();
				if (!has_css_border)
					*h += 4;
				*h += widget->GetPaddingTop() + widget->GetPaddingBottom();
			}
		}
		break;
	default:
		OP_ASSERT(FALSE); // No known type. No way to signal that.
	};
}

void OpWidgetInfo::GetMinimumSize(OpWidget* widget, OpTypedObject::Type type, INT32* minw, INT32* minh)
{
	*minw = 0;
	*minh = 0;
}

UINT32 OpWidgetInfo::GetSystemColor(OP_SYSTEM_COLOR color)
{
#ifdef SKIN_SUPPORT
	return g_skin_manager->GetSystemColor(color);
#else
	return g_op_ui_info->GetSystemColor(color);
//	return g_pcfontscolors->GetColor(color);
#endif
}

UINT32 OpWidgetInfo::GetVerticalScrollbarWidth()
{
#ifdef SKIN_SUPPORT
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Scrollbar Vertical Skin");
	if (skin_elm)
	{
		INT32 w = 0, h = 0;
		skin_elm->GetSize(&w, &h, 0);
		if (w > 0)
			return w;
	}
#endif
	return g_op_ui_info->GetVerticalScrollbarWidth();
}

UINT32 OpWidgetInfo::GetHorizontalScrollbarHeight()
{
#ifdef SKIN_SUPPORT
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Scrollbar Horizontal Skin");
	if (skin_elm)
	{
		INT32 w = 0, h = 0;
		skin_elm->GetSize(&w, &h, 0);
		if (h > 0)
			return h;
	}
#endif
	return g_op_ui_info->GetHorizontalScrollbarHeight();
}

void OpWidgetInfo::AddBorder(OpWidget* widget, OpRect *rect)
{
	INT32 left, top, right, bottom;
	GetBorders(widget, left, top, right,  bottom);
	rect->x += left;
	rect->y += top;
	rect->width -= left + right;
	rect->height -= top + bottom;
}

void OpWidgetInfo::GetBorders(OpWidget* widget, INT32& left, INT32& top, INT32& right, INT32& bottom)
{
	if (!widget->HasCssBorder())
	{
		// The "popup" border variant of OpListBox, used e.g. in
		// dropdowns, is currently drawn manually. We therefore need
		// to check for it here in order to return the correct border
		// width.
		if (widget->GetType() == OpTypedObject::WIDGET_TYPE_LISTBOX)
		{
			OpListBox *listbox = static_cast<OpListBox*>(widget);

			if (listbox->GetBorderStyle() == OpListBox::BORDER_STYLE_POPUP)
			{
				left = top = right = bottom = WIDGETS_POPUP_BORDER_THICKNESS;
				return;
			}
		}

		// Inset needs to be (2, 2) regardless of skin in order to be spec
		// compliant, so it's up to the skin to look good with this inset.
		left = top = right = bottom = 2;
	}
	else
	{
		left = top = right = bottom = 0;
	}
}

INT32 OpWidgetInfo::GetItemMargin(WIDGET_MARGIN margin)
{
	INT32 left, top, right, bottom;
	GetItemMargin(left, top, right, bottom);
	switch(margin)
	{
	case MARGIN_TOP:	return top;
	case MARGIN_LEFT:	return left;
	case MARGIN_RIGHT:	return right;
	case MARGIN_BOTTOM:	return bottom;
	};
	OP_ASSERT(FALSE); // Should not happen.
	return 0;
}

void OpWidgetInfo::GetItemMargin(INT32 &left, INT32 &top, INT32 &right, INT32 &bottom)
{
#ifdef SKIN_SUPPORT
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Listitem Skin");
	if (skin_elm)
	{
		skin_elm->GetMargin(&left, &top, &right, &bottom, 0);
		return;
	}
#endif
	top = bottom = 1;
	left = right = 2;
}

INT32 OpWidgetInfo::GetDropdownButtonWidth(OpWidget* widget)
{
	if (widget->IsOfType(OpTypedObject::WIDGET_TYPE_DROPDOWN) && !((OpDropDown*)widget)->m_dropdown_packed.show_button)
		return 0;
	INT32 width = GetVerticalScrollbarWidth();
	return width ? width : 16;
}

INT32 OpWidgetInfo::GetDropdownLeftButtonWidth(OpWidget* widget)
{
	if (widget->IsOfType(OpTypedObject::WIDGET_TYPE_DROPDOWN) && !((OpDropDown*)widget)->m_dropdown_packed.show_button)
		return 0;
	INT32 width = GetVerticalScrollbarWidth();
	return width ? width : 16;
}

UINT32 OpWidgetInfo::GetScrollDelay(BOOL bigstep, BOOL first_step)
{
	if (first_step) // Don't everybody hate the extra delay just after pressing a scrollbar arrow?
		return 350;
	if (bigstep)
		return 160;
	else
		return 80;
}

INT32 OpWidgetInfo::GetScrollbarFirstButtonSize()
{
	switch(GetScrollbarArrowStyle())
	{
		case ARROWS_AT_BOTTOM:
			return 0;
		case ARROWS_AT_BOTTOM_AND_TOP:
		case ARROWS_AT_TOP:
			return 2*GetVerticalScrollbarWidth();
		case ARROWS_NORMAL:
			break;
	}
	return GetVerticalScrollbarWidth();
}

INT32 OpWidgetInfo::GetScrollbarSecondButtonSize()
{
	switch(GetScrollbarArrowStyle())
	{
		case ARROWS_AT_TOP:
			return 0;
		case ARROWS_AT_BOTTOM_AND_TOP:
		case ARROWS_AT_BOTTOM:
			return 2*GetVerticalScrollbarWidth();
		case ARROWS_NORMAL:
			break;
	}
	return GetVerticalScrollbarWidth();
}

SCROLLBAR_ARROWS_TYPE OpWidgetInfo::GetScrollbarArrowStyle()
{
	return ARROWS_NORMAL;
}

BOOL OpWidgetInfo::GetScrollbarButtonDirection(BOOL first_button, BOOL horizontal, INT32 pos)
{
	return !first_button;
}

BOOL OpWidgetInfo::GetScrollbarPartHitBy(OpWidget *widget, const OpPoint &pt, SCROLLBAR_PART_CODE &hitPart)
{
	hitPart = INVALID_PART;
	return FALSE;
}

#ifdef PAGED_MEDIA_SUPPORT
INT32 OpWidgetInfo::GetPageControlHeight()
{
# ifdef SKIN_SUPPORT
	if (OpSkinElement* elm = g_skin_manager->GetSkinElement("Page Control Horizontal Skin"))
	{
		INT32 dummy_width, height;

		if (OpStatus::IsSuccess(elm->GetSize(&dummy_width, &height, 0)))
			return height;
	}
# endif // SKIN_SUPPORT

	return 30;
}
#endif // PAGED_MEDIA_SUPPORT
