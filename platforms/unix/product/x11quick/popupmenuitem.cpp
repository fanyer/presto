/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"

#include "platforms/unix/product/x11quick/popupmenuitem.h"

#include "modules/display/vis_dev.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/OpWidgetImage.h"
#include "modules/unicode/unicode.h"
#include "platforms/unix/product/x11quick/popupmenu.h"

ToolkitLibrary::PopupMenuLayout PopupMenuItem::m_layout;

PopupMenuItem::PopupMenuItem(UINT32 state, OpInputAction* action, PopupMenu* submenu, OpWidget* widget)
	: m_action(action)
	, m_image(0)
	, m_submenu(submenu)
	, m_widget(widget)
	, m_state(state)
	, m_accelerator_key(0)
{
}

PopupMenuItem::~PopupMenuItem()
{
	OP_DELETE(m_submenu);
	OP_DELETE(m_image);
	OP_DELETE(m_action);
}

OP_STATUS PopupMenuItem::SetName(const OpStringC& name)
{
	RETURN_IF_ERROR(m_name.Set(name.CStr(), m_widget));
	m_name.SetConvertAmpersand(TRUE);
	m_name.SetDrawAmpersandUnderline(TRUE);
	m_name.SetForceBold(m_state & MI_BOLD);
	m_name.Update();

	if (m_name.Get())
	{
		for (const uni_char* s = m_name.Get(); *s; s++ )
		{
			if (*s == '&' && s[1] != '&')
			{
				m_accelerator_key = Unicode::ToLower(s[1]);
				break;
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS PopupMenuItem::SetWidgetImage(const OpWidgetImage* image)
{
	OP_DELETE(m_image);
	m_image = 0;

	if (!image)
	{
		return OpStatus::OK;
	}

	m_image = OP_NEW(OpWidgetImage, ());
	if (!m_image)
		return OpStatus::ERR_NO_MEMORY;

	m_image->SetWidgetImage(image);

	int skin_state = m_image->GetState();
	if (m_state & MI_DISABLED)
		skin_state |= SKINSTATE_DISABLED;
	if (m_state & MI_CHECKED)
		skin_state |= SKINSTATE_SELECTED;
	if (m_widget->GetRTL())
		skin_state |= SKINSTATE_RTL;
	m_image->SetState(skin_state);

	int width, height;
	m_image->GetSize(&width, &height);

	return OpStatus::OK;
}

OpInputAction* PopupMenuItem::TakeInputAction()
{
	if (m_submenu)
		return m_submenu->TakeInputAction();

	OpInputAction* action = m_action;
	m_action = 0;

	return action;
}

uni_char PopupMenuItem::GetAcceleratorKey() const
{
	return m_accelerator_key;
}

int PopupMenuItem::Paint(OpPainter* painter, const OpRect& rect, bool selected, const PaintProperties& properties)
{
	int skin_state = 0;
	if (m_state & MI_DISABLED)
		skin_state |= SKINSTATE_DISABLED;
	if (selected) // hover
		skin_state |= SKINSTATE_HOVER;
	if (m_widget->GetRTL())
		skin_state |= SKINSTATE_RTL;

	VisualDevice vd;
	vd.BeginPaint(painter, rect, rect);

	// Special attention for checkboxes and radio buttons. We draw those in the
	// toolkit only if there is no provided skin image
	BOOL draw_native_checkmark = FALSE;
	if (g_toolkit_library->HasToolkitSkinning() && (!m_image || !m_image->HasBitmapImage()))
		draw_native_checkmark = m_state & (MI_CHECKED|MI_SELECTED);

	OpSkinElement* skin_element = g_skin_manager->GetSkinElement("Popup Menu Button Skin");
	if (skin_element)
	{
		// Radio buttons and check boxes use SKINSTATE_SELECTED to indicate if the button
		// is active (eg, showing a checkmark) while SKINSTATE_HOVER indicates if item is
		// selected in the menu (eg, with another background)
		int state = skin_state;
		if (draw_native_checkmark)
		{
			if (m_state & MI_CHECKED) // checkbox
				state |= SKINSTATE_PRESSED;
			if (m_state & MI_SELECTED) // radio
				state |= SKINSTATE_SELECTED;
		}
		if (m_submenu)
 			state |= SKINSTATE_FOCUSED; // Right arrow - See DSK-339915 -> ToDo: Find a solution for transfering the information about checkbox, radio and arrow to the toolkit instead of misusing SKINSTATE_*

		skin_element->Draw(&vd, rect, state);
	}
	else
	{
		UINT32 color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON);
		painter->SetColor(OP_GET_R_VALUE(color), OP_GET_G_VALUE(color), OP_GET_B_VALUE(color), OP_GET_A_VALUE(color));
		painter->FillRect(rect);
	}

	int fontheight = properties.normal_font->Height(); // What if boldfont has a different height?
	int elm_height = max(fontheight, properties.max_image_height);
	int normal_incr = elm_height + 2;
	int text_adjust = 1;
	if (properties.max_image_height > fontheight)
		text_adjust += (properties.max_image_height - fontheight) / 2;

	int x = rect.x + m_layout.left_margin;

	if (m_image && !draw_native_checkmark)
	{
		int width, height;
		m_image->GetSize(&width, &height);

		if( width > 0 && height > 0 )
		{
			width = min(width, properties.max_image_width);
			height = min(height, properties.max_image_height);

			int image_adjust_x = 0;
			int image_adjust_y = 1;

			if( width < properties.max_image_width )
				image_adjust_x += (22 - height) / 2;
			if ( height < rect.height)
				image_adjust_y += (rect.height - height) / 2;

			const OpRect image_rect(x + image_adjust_x, rect.y + image_adjust_y, width, height);
			m_image->Draw(&vd, AdjustChildRect(image_rect, rect));
		}
	}

	x += properties.max_image_width > 0 ? properties.max_image_width + m_layout.image_margin : 0;

	UINT32 textcolor;
	if (!skin_element || OpStatus::IsError(skin_element->GetTextColor(&textcolor, skin_state)))
	{
		if (m_state & MI_DISABLED)
			textcolor = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
		else if (selected) // HOVER
			textcolor = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED);
		else
			textcolor = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_MENU);
	}

	painter->SetColor(OP_GET_R_VALUE(textcolor), OP_GET_G_VALUE(textcolor), OP_GET_B_VALUE(textcolor), OP_GET_A_VALUE(textcolor));

	OpFont* font = m_state & MI_BOLD ? properties.bold_font : properties.normal_font;
	painter->SetFont(font);
	// On some systems (ubuntu) the default font is not the same as the font we want to use.
	// Setting the font number corrects that (DSK-315475)
	vd.SetFont(properties.font_number);

	const OpRect name_rect(x, rect.y + text_adjust, m_name.GetWidth(), m_name.GetHeight());
	m_name.Draw(AdjustChildRect(name_rect, rect), &vd, textcolor);

	if (m_shortcut.Get() && *m_shortcut.Get())
	{
		OpRect shortcut_rect(x + properties.max_text_width - m_shortcut.GetWidth(),
				rect.y + text_adjust, m_shortcut.GetWidth(), m_shortcut.GetHeight());

		// Work around the submenu arrow overlapping the shortcut text.  GTK+ menus
		// normally don't have this problem, because they are sane enough to not
		// have _both_ a shortcut and submenu in one entry.
		if (m_submenu)
			shortcut_rect.x -= m_layout.arrow_overlap;

		m_shortcut.Draw(AdjustChildRect(shortcut_rect, rect), &vd, textcolor);
	}

	x += properties.max_text_width;

	if (m_submenu)
	{
		// DSK-318723. Some GTK styles will use all available height. So we limit to the font size
		// We have used 0.5 at the end, but it does not look so nice. We may want to do some more tests
		int y = fontheight > rect.height ? rect.y : rect.y + (int)(((float)(rect.height - fontheight) / 2.0)+0.0);
		int h = fontheight > rect.height ? rect.height : fontheight;

		x -= m_layout.arrow_overlap;
		g_skin_manager->DrawElement(&vd, "Menu Right Arrow",
				AdjustChildRect(OpRect(x, y, m_layout.arrow_width, h), rect), skin_state);
	}

	vd.EndPaint();

	return normal_incr;
}

OpRect PopupMenuItem::AdjustChildRect(const OpRect& child_rect, const OpRect& parent_rect) const
{
	OpRect adjusted_rect = child_rect;
	if (m_widget->GetRTL())
		adjusted_rect.x = parent_rect.x + (parent_rect.Right() - child_rect.Right());
	return adjusted_rect;
}

/*static*/
int PopupMenuSeparator::GetItemHeight()
{
	int width;
	int height = SeparatorHeight; // Default value
	g_skin_manager->GetSize("Menu Separator Skin", &width, &height);
	return height;
}

int PopupMenuSeparator::Paint(OpPainter* painter, const OpRect& rect, bool selected, const PaintProperties& properties)
{
	// Don't paint separator if it is the last element in the menu list
	if (!this->Suc())
		return 0;

	VisualDevice vd;

	vd.BeginPaint(painter, rect, rect);

	int skin_state = 0;
	skin_state |= SKINSTATE_DISABLED;

	g_skin_manager->DrawElement(&vd, "Menu Separator Skin", rect, skin_state);

	vd.EndPaint();

	return rect.height;
}

bool PopupMenuItem::IsChecked()
{
	return m_state & MI_CHECKED;
}

bool PopupMenuItem::IsSelected() 
{
	return m_state & MI_SELECTED;
}

bool PopupMenuItem::IsBold()
{
	return m_state & MI_BOLD;
}

