/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "X11SkinElement.h"

#include "modules/skin/OpSkin.h"
#include "modules/display/vis_dev.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/ui/OpUiInfo.h"

// Elements and order must match NativeType in header file
static const char* s_native_types[] =
{
	"Push Button Skin",
	"Push Default Button Skin",
	"Toolbar Button Skin",
	"Selector Button Skin",
	"Menu Skin",
	"Menu Button Skin",
	"Menu Right Arrow",
	"Menu Separator Skin",
	"Popup Menu Skin",
	"Popup Menu Button Skin",
	"Header Button Skin",
	"Link Button Skin",
	"Tab Button Skin",
	"Pagebar Button Skin",
	"Caption Minimize Button Skin",
	"Caption Close Button Skin",
	"Caption Restore Button Skin",
	"Checkbox Skin",
	"Radio Button Skin",
	"Dropdown Skin",
	"Dropdown Button Skin",
	"Dropdown Left Button Skin",
	"Dropdown Edit Skin",
	"Edit Skin",
	"MultilineEdit Skin",
	"Browser Skin",
	"Progressbar Skin",
	"Start Skin",
	"Search Skin",
	"Treeview Skin",
	"Listbox Skin",
	"Checkmark",
	"Bullet",
	"Window Skin",
	"Browser Window Skin",
	"Dialog Skin",
	"Dialog Page Skin",
	"Dialog Tab Page Skin",
	"Dialog Button Border Skin",
	"Tabs Skin",
	"Hotlist Skin",
	"Hotlist Selector Skin",
	"Hotlist Splitter Skin",
	"Hotlist Panel Header Skin",
	"Scrollbar Horizontal Skin",
	"Scrollbar Horizontal Knob Skin",
	"Scrollbar Horizontal Left Skin",
	"Scrollbar Horizontal Right Skin",
	"Scrollbar Vertical Skin",
	"Scrollbar Vertical Knob Skin",
	"Scrollbar Vertical Up Skin",
	"Scrollbar Vertical Down Skin",
	"Slider Horizontal Track",
	"Slider Horizontal Knob",
	"Slider Vertical Track",
	"Slider Vertical Knob",
	"Status Skin",
	"Mainbar Skin",
	"Mainbar Button Skin",
	"Personalbar Skin",
	"Pagebar Skin",
	"Addressbar Skin",
	"Navigationbar Skin",
	"Viewbar Skin",
	"Mailbar Skin",
	"Chatbar Skin",
	"Cycler Skin",
	"Progress Document Skin",
	"Progress Images Skin",
	"Progress Total Skin",
	"Progress Speed Skin",
	"Progress Elapsed Skin",
	"Progress Status Skin",
	"Progress General Skin",
	"Progress Clock Skin",
	"Progress Empty Skin",
	"Progress Full Skin",
	"Identify As Skin",
	"Dialog Warning",
	"Dialog Error",
	"Dialog Question",
	"Dialog Info",
	"Speed Dial Widget Skin",
	"Horizontal Separator",
	"Vertical Separator",
	"Disclosure Triangle Skin",
	"Sort Ascending",
	"Sort Descending",
	"Pagebar Close Button Skin",
	"Notifier Close Button Skin",
	"Tooltip Skin",
	"Notifier Skin",
	"Extender Button Skin",
	"Listitem Skin",
	NULL,
};

#define CHECKMARK_SIZE 11
#define MAX_MENU_IMAGE_SIZE 22

#define SLIDER_HORIZONTAL_KNOB_HEIGHT 19
#define SLIDER_HORIZONTAL_KNOB_WIDTH 9



namespace NativeSkinConstants
{
	enum ArrowDirection { ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT };
	enum BorderOmission { BORDER_OMIT_TOP=1, BORDER_OMIT_BOTTOM=2 };
};


X11SkinElement::X11SkinElement(OpSkin* skin, const OpStringC8& name, SkinType type, SkinSize size)
  :OpSkinElement(skin, name, type, size)
{
	m_native_type = NATIVE_NOT_SUPPORTED;

	if (size != SKINSIZE_DEFAULT)
		return;

	for (UINT32 i = 0; s_native_types[i]; i++)
	{
		if (name.CompareI(s_native_types[i]) == 0)
		{
			NativeType native_type = (NativeType) (i + 1);

			switch (native_type)
			{
				case NATIVE_PAGEBAR:
				case NATIVE_MAINBAR:
				case NATIVE_PERSONALBAR:
				case NATIVE_ADDRESSBAR:
				case NATIVE_NAVIGATIONBAR:
				case NATIVE_VIEWBAR:
				case NATIVE_PAGEBAR_BUTTON:
				{
					if (type == SKINTYPE_DEFAULT || type == SKINTYPE_BOTTOM)
					{
						m_native_type = native_type;
					}
					break;
				}

				default:
				{
					if (type == SKINTYPE_DEFAULT)
					{
						m_native_type = native_type;
					}
					break;
				}

			}
			break;
		}
	}
}


X11SkinElement::~X11SkinElement()
{
}


OP_STATUS X11SkinElement::GetTextColor(UINT32* color, INT32 state)
{
	switch (m_native_type)
	{
		case NATIVE_MENU_BUTTON:
			// This is the menu bar button. We want same colors as in popup menu
			if (state & (SKINSTATE_SELECTED|SKINSTATE_HOVER))
				*color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED);
			else if (state & SKINSTATE_DISABLED)
				*color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
			else
				*color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_MENU);
			return OpStatus::OK;
	}

	return OpSkinElement::GetTextColor(color, state);
}

OP_STATUS X11SkinElement::GetSize(INT32* width, INT32* height, INT32 state)
{
	switch (m_native_type)
	{
		case NATIVE_SLIDER_HORIZONTAL_TRACK:
		{
			*height = 4;
			return OpStatus::OK;
		}

		case NATIVE_SLIDER_VERTICAL_TRACK:
		{
			*width = 4;
			return OpStatus::OK;
		}

		case NATIVE_SLIDER_HORIZONTAL_KNOB:
		{
			*width = SLIDER_HORIZONTAL_KNOB_WIDTH;
			*height = SLIDER_HORIZONTAL_KNOB_HEIGHT;
			return OpStatus::OK;
		}

		case NATIVE_SLIDER_VERTICAL_KNOB:
		{
			*width = SLIDER_HORIZONTAL_KNOB_HEIGHT;
			*height = SLIDER_HORIZONTAL_KNOB_WIDTH;
			return OpStatus::OK;
		}

		case NATIVE_CHECKMARK:
		case NATIVE_BULLET:
		{
			*width = CHECKMARK_SIZE;
			*height = CHECKMARK_SIZE;
			return OpStatus::OK;
		}

		case NATIVE_MENU_SEPARATOR:
		{
			*width  = 4; // Should the separator be vertical
			*height = 4;
			return OpStatus::OK;
		}

		case NATIVE_DISCLOSURE_TRIANGLE:
		{
			*width = 22;
			*height = 22;
			return OpStatus::OK;
		}

		case NATIVE_SORT_ASCENDING:
		case NATIVE_SORT_DESCENDING:
			*width = 11;
			*height = 11;
			return OpStatus::OK;

		case NATIVE_PAGEBAR_CLOSE_BUTTON:
		case NATIVE_NOTIFIER_CLOSE_BUTTON:
			*width = 11;
			*height = 11;
			return OpStatus::OK;

	}

	return OpSkinElement::GetSize(width, height, state);
}


OP_STATUS X11SkinElement::GetMargin(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	switch (m_native_type)
	{
		case NATIVE_TAB_BUTTON:
			// Required to achieve overlapping buttons and expected look
			*left   = -2;
			*right   = -2;
			*bottom = -2;
			return OpStatus::OK;

		case NATIVE_MAINBAR_BUTTON:
			*right = 2;
			return OpStatus::OK;
	}
	return OpSkinElement::GetMargin(left, top, right, bottom, state);
}


OP_STATUS X11SkinElement::GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom, INT32 state)
{
	switch (m_native_type)
	{
		case NATIVE_TAB_BUTTON:
			*left   = 10;
			*top    = 6;
			*right  = 10;
			*bottom = 3;
			return OpStatus::OK;

		case NATIVE_DIALOG_TAB_PAGE:
			*left   = 10;
			*top    = 10;
			*right  = 10;
			*bottom = 10;
			return OpStatus::OK;

		case NATIVE_DIALOG:
			*left   = 10;
			*top    = 10;
			*right  = 10;
			*bottom = 10;
			return OpStatus::OK;

		case NATIVE_DIALOG_BUTTON_BORDER:
			*left   = 5;
			*top    = 10;
			*right  = 0;
			*bottom = 0;
			return OpStatus::OK;

		case NATIVE_MAINBAR_BUTTON:
			*left   = 4;
			*top    = 3;
			*right  = 5;
			*bottom = 4;
			return OpStatus::OK;
	}

	return OpSkinElement::GetPadding(left, top, right, bottom, state);
}


OP_STATUS X11SkinElement::GetSpacing(INT32* spacing, INT32 state)
{
	switch (m_native_type)
	{
		case NATIVE_MAINBAR_BUTTON:
			*spacing = 2;
			return OpStatus::OK;
	}
	return OpSkinElement::GetSpacing(spacing, state);
}


void X11SkinElement::OverrideDefaults(OpSkinElement::StateElement* se)
{
	// Sizes (required with yaml)
	switch (m_native_type)
	{
	case NATIVE_PUSH_DEFAULT_BUTTON:
	case NATIVE_PUSH_BUTTON:
		if (se->m_width < 110)
			se->m_width = 110;
		if (se->m_height < 25)
			se->m_height = 25;
		break;
	}

	// Margins
	switch (m_native_type)
	{
	case NATIVE_DIALOG:
		se->m_padding_bottom = se->m_padding_left = se->m_padding_right = se->m_padding_top = 10;
		break;
	case NATIVE_TOOLBAR_BUTTON:
	case NATIVE_TAB_BUTTON:
	case NATIVE_PAGEBAR_BUTTON:
	case NATIVE_LIST_ITEM:
	case NATIVE_DIALOG_BUTTON_BORDER:
	case NATIVE_DROPDOWN_BUTTON:
	case NATIVE_DROPDOWN_LEFT_BUTTON:
	case NATIVE_DROPDOWN_EDIT:
		// don't change anything for these
		break;
	default:
		// Default margins for all others
		se->m_margin_bottom = se->m_margin_left = se->m_margin_right = se->m_margin_top = 7;
	}
}


OP_STATUS X11SkinElement::Draw(VisualDevice* vd, OpRect rect, INT32 state, DrawArguments args)
{
	if (m_native_type == NATIVE_NOT_SUPPORTED)
		return OpSkinElement::Draw(vd, rect, state, args);

	UINT32 bgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND);
	UINT32 fgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT);
	UINT32 gray = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON);
	UINT32 lgray = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_LIGHT);
	UINT32 dgray = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK);
	UINT32 dgray2 = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_VERYDARK);
	UINT32 uibg = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_BACKGROUND);
	UINT32 docbg = colorManager->GetBackgroundColor(); // This color can be changed from Prefs dialog

	switch (m_native_type)
	{
		case NATIVE_PUSH_DEFAULT_BUTTON:
		case NATIVE_PUSH_BUTTON:
		case NATIVE_PAGEBAR_BUTTON:
		case NATIVE_HEADER_BUTTON:
		{
			if (state & SKINSTATE_DISABLED)
				fgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);

			if (m_native_type == NATIVE_PUSH_DEFAULT_BUTTON)
			{
				vd->SetColor32(dgray2);
				vd->DrawRect(rect);
				rect = rect.InsetBy(1, 1);
			}
			if (state & SKINSTATE_SELECTED)
				vd->SetColor32(lgray);
			else
				vd->SetColor32(gray);
			vd->FillRect(rect);
			if (state & (SKINSTATE_PRESSED | SKINSTATE_SELECTED))
			{
				// sunken
				Draw3DBorder(vd, dgray, lgray, rect);
				Draw3DBorder(vd, dgray2, gray, rect.InsetBy(1, 1));
			}
			else
			{
				// raised
				DrawFrame(vd, lgray, dgray, dgray2, rect);
			}
			break;
		}
    	case NATIVE_MAINBAR_BUTTON:
		{
			OpSkinElement::Draw(vd, rect, state, args); // or should we call this one on default and remove same call above?
			break;
		}
		case NATIVE_SELECTOR_BUTTON:
		{
			if (state & (SKINSTATE_PRESSED | SKINSTATE_SELECTED))
			{
				vd->SetColor32(dgray);
				vd->FillRect(rect);
				Draw3DBorder(vd, dgray, lgray, rect);
				Draw3DBorder(vd, dgray2, gray, rect.InsetBy(1, 1));
			}
			else if (state & SKINSTATE_HOVER)
			{
				Draw3DBorder(vd, gray, dgray2, rect);
				Draw3DBorder(vd, lgray, dgray, rect.InsetBy(1, 1));
			}
			break;
		}

		case NATIVE_POPUP_MENU_BUTTON:
		{
			if ((state & SKINSTATE_HOVER) && !(state & SKINSTATE_DISABLED))
			{
				vd->SetColor32(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED));
				vd->FillRect(rect);
			}
			if (state & (SKINSTATE_PRESSED|SKINSTATE_SELECTED))
			{
				// SKINSTATE_PRESSED specifies checked element, SKINSTATE_SELECTED specifies radio element
				OpRect r(rect.x, rect.y, MAX_MENU_IMAGE_SIZE, rect.height);
				int x = r.width > CHECKMARK_SIZE ? (r.width - CHECKMARK_SIZE) / 2 : 0;
				int y = r.height > CHECKMARK_SIZE ? (r.height - CHECKMARK_SIZE) / 2 : 0;

				r = r.InsetBy(x,y);

				if(state & SKINSTATE_PRESSED)
					DrawCheckMark(vd, fgcol, r);
				else
					DrawBullet(vd, fgcol, r);
			}
		}
		break;

		case NATIVE_MENU_BUTTON:
			if (state & (SKINSTATE_PRESSED | SKINSTATE_SELECTED | SKINSTATE_HOVER))
			{
				vd->SetColor32(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED));
				vd->FillRect(rect);
				Draw3DBorder(vd, dgray, lgray, rect);
			}
		break;

		case NATIVE_TOOLBAR_BUTTON:
		case NATIVE_LINK_BUTTON:
		{
			if (m_native_type == NATIVE_HEADER_BUTTON ||
				state & (SKINSTATE_PRESSED | SKINSTATE_SELECTED | SKINSTATE_HOVER))
			{
				vd->SetColor32(gray);
				vd->FillRect(rect);
				if (state & (SKINSTATE_PRESSED | SKINSTATE_SELECTED))
					Draw3DBorder(vd, dgray, lgray, rect);
				else
					Draw3DBorder(vd, lgray, dgray, rect.InsetBy(1, 1));
			}
			break;
		}

		case NATIVE_MENU:
		{
			vd->SetColor32(uibg);
			vd->FillRect(rect);
			break;
		}

		case NATIVE_POPUP_MENU:
		{
			vd->SetColor32(uibg);
			vd->FillRect(rect);

			vd->SetColor32(lgray);
			vd->DrawLine(rect.TopLeft(), rect.width, TRUE);
			vd->DrawLine(rect.TopLeft(), rect.height, FALSE);

			vd->SetColor32(dgray);
			vd->DrawLine(OpPoint(rect.x, rect.height-1), rect.width, TRUE);
			vd->DrawLine(OpPoint(rect.x+rect.width-1, rect.y), rect.height, FALSE);
			break;
		}

		case NATIVE_MENU_RIGHT_ARROW:
		{
			if (state & SKINSTATE_DISABLED)
				vd->SetColor32(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED));
			else if (state & SKINSTATE_HOVER)
				vd->SetColor32(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_SELECTED));
			else
				vd->SetColor32(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_UI_MENU));
			DrawArrow(vd, rect, (state & SKINSTATE_RTL) ? NativeSkinConstants::ARROW_LEFT : NativeSkinConstants::ARROW_RIGHT);
			break;
		}

		case NATIVE_MENU_SEPARATOR:
		{
			vd->SetColor32(dgray);
			int y = rect.y+(rect.height-2)/2;
			int x = rect.x+2;
			int w = rect.width-4;
			vd->DrawLine(OpPoint(x, y), w, TRUE);
			vd->SetColor32(lgray);
			vd->DrawLine(OpPoint(x, y+1), w, TRUE);
			break;
		}

		case NATIVE_CHECKMARK:
		{
			DrawCheckMark(vd, fgcol, rect);
			break;
		}
		case NATIVE_BULLET:
		{
			DrawBullet(vd, fgcol, rect);
			break;
		}
		case NATIVE_RADIO_BUTTON:
		{
			if (state & SKINSTATE_DISABLED)
			{
				fgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
				bgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_DISABLED);
			}

			vd->BeginClipping(OpRect(rect.x, rect.y, rect.width-3, rect.height-3));
			vd->SetColor32(dgray);
			vd->FillEllipse(rect);
			vd->EndClipping();

			vd->BeginClipping(OpRect(rect.x+3, rect.y+3, rect.width-3, rect.height-3));
			vd->SetColor32(lgray);
			vd->FillEllipse(rect);
			vd->EndClipping();

			rect = rect.InsetBy(1, 1);

			vd->BeginClipping(OpRect(rect.x, rect.y, rect.width-2, rect.height-2));
			vd->SetColor32(dgray2);
			vd->FillEllipse(rect);
			vd->EndClipping();

			vd->BeginClipping(OpRect(rect.x+2, rect.y+2, rect.width-2, rect.height-2));
			vd->SetColor32(uibg);
			vd->FillEllipse(rect);
			vd->EndClipping();

			vd->SetColor32(bgcol);
			vd->FillEllipse(rect.InsetBy(1, 1));

			if (state & SKINSTATE_SELECTED)
				DrawBullet(vd, fgcol, rect);
			break;
		}
		case NATIVE_CHECKBOX:
		{
			if (state & SKINSTATE_DISABLED)
			{
				fgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
				bgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_DISABLED);
			}
			else if (state & SKINSTATE_PRESSED)
			{
				bgcol = gray;
			}

			vd->SetColor32(bgcol);
			vd->FillRect(rect);
			Draw3DBorder(vd, dgray, lgray, rect);
			Draw3DBorder(vd, dgray2, uibg, rect.InsetBy(1, 1));

			if (state & SKINSTATE_INDETERMINATE)
			{
				DrawIndeterminate(vd, fgcol, rect);
			}
			else if (state & SKINSTATE_SELECTED)
			{
				// Center checkmark
				OpRect r(rect);
				int x = (r.width-CHECKMARK_SIZE)/2 -1;
				int y = (r.height-CHECKMARK_SIZE)/2;
				r.x += x;
				r.y += y;
				r.width = CHECKMARK_SIZE;
				r.height = CHECKMARK_SIZE;
				DrawCheckMark(vd, fgcol, r);
			}
			break;
		}
		case NATIVE_EDIT:
		case NATIVE_MULTILINE_EDIT:
		case NATIVE_LISTBOX:
		case NATIVE_START:
		case NATIVE_SEARCH:
		case NATIVE_TREEVIEW:
		case NATIVE_DROPDOWN:
		case NATIVE_DROPDOWN_EDIT:
		{
			if (state & SKINSTATE_DISABLED)
			{
				fgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TEXT_DISABLED);
				bgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_DISABLED);
			}
			vd->SetColor32(bgcol);
			vd->FillRect(rect);
			Draw3DBorder(vd, dgray, lgray, rect);
			Draw3DBorder(vd, dgray2, uibg, rect.InsetBy(1, 1));
			break;
		}
		case NATIVE_HOTLIST_SELECTOR:
		{
			Draw3DBorder(vd, dgray, lgray, rect);
			Draw3DBorder(vd, dgray2, gray, rect.InsetBy(1, 1));
			break;
		}
		case NATIVE_BROWSER:
		{
			Draw3DBorder(vd, dgray, lgray, rect);
			Draw3DBorder(vd, dgray2, gray, rect.InsetBy(1, 1));
			break;
		}

		case NATIVE_HORIZONTAL_SEPARATOR:
		{
			vd->SetColor(uibg);
			vd->FillRect(rect);
			vd->SetColor32(dgray);
			vd->DrawLine(OpPoint(rect.x, rect.y), rect.width, TRUE);
			vd->SetColor32(lgray);
			vd->DrawLine(OpPoint(rect.x, rect.y+1), rect.width, TRUE);
			break;
		}
		case NATIVE_VERTICAL_SEPARATOR:
		{
			vd->SetColor(uibg);
			vd->FillRect(rect);
			vd->SetColor32(dgray);
			vd->DrawLine(OpPoint(rect.x, rect.y), rect.height, FALSE);
			vd->SetColor32(lgray);
			vd->DrawLine(OpPoint(rect.x, rect.y+1), rect.height, FALSE);
			break;
		}
		case NATIVE_SPEEDDIAL:
		{
			vd->SetColor(docbg);
			vd->FillRect(rect);
			break;
		}

		case NATIVE_DIALOG:
		case NATIVE_HOTLIST:
		case NATIVE_BROWSER_WINDOW:
		{
			vd->SetColor(uibg);
			vd->FillRect(rect);
			break;
		}
		case NATIVE_WINDOW:
		{
			vd->SetColor(uibg);
			vd->FillRect(rect);
			vd->SetColor32(dgray);
			vd->DrawLine(OpPoint(rect.x, rect.y+rect.height-2), rect.width, TRUE);
			vd->SetColor32(lgray);
			vd->DrawLine(OpPoint(rect.x, rect.y+rect.height-1), rect.width, TRUE);
			break;
		}
		case NATIVE_DIALOG_TAB_PAGE:
		{
			rect.y -= 1;
			rect.height += 1;

			// We must fill the rectangle. When we drag a button from the Appearance
			// dialog it gets initialized with this skin as a background.
			vd->SetColor(uibg);
			vd->FillRect(rect);

			DrawFrame(vd, lgray, dgray, dgray2, rect);
			break;
		}
		case NATIVE_TABS:
		{
			// Draws the entire background area of the tab buttons. We only want to draw the
			// line at the bottom (the same as the top line of frame in NATIVE_DIALOG_TAB_PAGE)
			rect.y = rect.height - 1;
			rect.height = 1;
			DrawTopFrameLine(vd, lgray, dgray, dgray2, rect);
			break;
		}
		case NATIVE_TAB_BUTTON:
		{
			if (!(state & SKINSTATE_SELECTED))
			{
				rect.x += 2;
				rect.width -= 4;
			}

			vd->SetColor32(uibg);
			vd->FillRect(rect);

			if (!(state & SKINSTATE_SELECTED))
			{
				rect.y += 2;
				rect.height -= 2;
			}

			DrawTab(vd, lgray, dgray, dgray2, rect, state & SKINSTATE_SELECTED);
			break;
		}
		case NATIVE_CAPTION_MINIMIZE_BUTTON:
		{
			break;
		}
		case NATIVE_CAPTION_CLOSE_BUTTON:
		{
			break;
		}
		case NATIVE_CAPTION_RESTORE_BUTTON:
		{
			break;
		}

		case NATIVE_SCROLLBAR_HORIZONTAL:
		case NATIVE_SCROLLBAR_VERTICAL:
		{
			UINT32 scrollbgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_SCROLLBAR_BACKGROUND);
			vd->SetColor32(scrollbgcol);
			vd->FillRect(rect);
			break;
		}

		case NATIVE_SCROLLBAR_HORIZONTAL_KNOB:
		case NATIVE_SCROLLBAR_VERTICAL_KNOB:
		{
			if (state & SKINSTATE_DISABLED)
			{
				UINT32 scrollbgcol = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_SCROLLBAR_BACKGROUND);
				vd->SetColor32(scrollbgcol);
				vd->FillRect(rect);
			}
			else
			{
				Draw3DBorder(vd, gray, dgray2, rect);
				Draw3DBorder(vd, lgray, dgray, rect.InsetBy(1, 1));
				vd->SetColor32(gray);
				vd->FillRect(rect.InsetBy(2, 2));
			}
			break;
		}
		case NATIVE_SCROLLBAR_HORIZONTAL_LEFT:
			DrawDirectionButton(vd, rect, NativeSkinConstants::ARROW_LEFT, state, fgcol, gray, lgray, dgray, dgray2);
			break;
		case NATIVE_SCROLLBAR_HORIZONTAL_RIGHT:
			DrawDirectionButton(vd, rect, NativeSkinConstants::ARROW_RIGHT, state, fgcol, gray, lgray, dgray, dgray2);
			break;
		case NATIVE_SCROLLBAR_VERTICAL_UP:
			DrawDirectionButton(vd, rect, NativeSkinConstants::ARROW_UP, state, fgcol, gray, lgray, dgray, dgray2);
			break;
		case NATIVE_SCROLLBAR_VERTICAL_DOWN:
			DrawDirectionButton(vd, rect, NativeSkinConstants::ARROW_DOWN, state, fgcol, gray, lgray, dgray, dgray2);
			break;
		case NATIVE_DROPDOWN_BUTTON:
		case NATIVE_DROPDOWN_LEFT_BUTTON:
			rect.x += m_native_type == NATIVE_DROPDOWN_BUTTON ? -2 : 2;
			rect.y += 2;
			rect.height -= 4;
			DrawDirectionButton(vd, rect, NativeSkinConstants::ARROW_DOWN, state, fgcol, gray, lgray, dgray, dgray2);
			break;
		case NATIVE_STATUS:
		{
			Draw3DBorder(vd, dgray, lgray, rect);
			break;
		}
		case NATIVE_HOTLIST_PANEL_HEADER:
		case NATIVE_HOTLIST_SPLITTER:
		case NATIVE_MAINBAR:
		case NATIVE_PERSONALBAR:
		case NATIVE_PAGEBAR:
		case NATIVE_ADDRESSBAR:
		case NATIVE_NAVIGATIONBAR:
		case NATIVE_MAILBAR:
		case NATIVE_CHATBAR:
		case NATIVE_VIEWBAR:
		{
			if (GetType() == SKINTYPE_BOTTOM)
				break;

			vd->SetColor32(dgray);
			vd->DrawLine(OpPoint(rect.x, rect.y+rect.height-2), rect.width, TRUE);
			vd->SetColor32(lgray);
			vd->DrawLine(OpPoint(rect.x, rect.y+rect.height-1), rect.width, TRUE);
			break;
		}

		case NATIVE_CYCLER:
		{
			Draw3DBorder(vd, gray, dgray2, rect);
			break;
		}

		case NATIVE_PROGRESS_IMAGES:
		case NATIVE_PROGRESS_SPEED:
		case NATIVE_PROGRESS_ELAPSED:
		case NATIVE_PROGRESS_STATUS:
		case NATIVE_PROGRESS_GENERAL:
		case NATIVE_PROGRESS_CLOCK:
		case NATIVE_PROGRESS_EMPTY:
		case NATIVE_PROGRESS_FULL:
		case NATIVE_IDENTIFY_AS:
		{
			UINT32 color;
			if (m_native_type == NATIVE_PROGRESS_FULL)
				color = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED);
			else
				color = gray;
			vd->SetColor(color);
			vd->FillRect(rect);
			Draw3DBorder(vd, dgray, lgray, rect);
			break;
		}

		case NATIVE_DISCLOSURE_TRIANGLE:
		{
			vd->SetColor32(gray);
			vd->FillRect(rect);

			int direction = state & SKINSTATE_SELECTED ? NativeSkinConstants::ARROW_DOWN : NativeSkinConstants::ARROW_RIGHT;

			if (!(state & SKINSTATE_DISABLED))
			{
				vd->SetColor32(fgcol);
				DrawArrow(vd, rect, direction);
			}
			else
			{
				rect.x ++;
				rect.y ++;
				vd->SetColor32(lgray);
				DrawArrow(vd, rect, direction);
				rect.x --;
				rect.y --;
				vd->SetColor32(dgray);
				DrawArrow(vd, rect, direction);
			}
			break;
		}

		case NATIVE_SORT_ASCENDING:
		{
			vd->SetColor32(fgcol);
			DrawArrow(vd, rect, NativeSkinConstants::ARROW_UP);
			break;
		}

		case NATIVE_SORT_DESCENDING:
		{
			vd->SetColor32(fgcol);
			DrawArrow(vd, rect, NativeSkinConstants::ARROW_DOWN);
			break;
		}

		case NATIVE_TOOLTIP:
		case NATIVE_NOTIFIER:
		{
			Draw3DBorder(vd, dgray2, dgray2, rect);
			vd->SetColor32(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_TOOLTIP_BACKGROUND));
			vd->FillRect(rect.InsetBy(1, 1));
			break;
		}

		case NATIVE_PAGEBAR_CLOSE_BUTTON:
		{
			if( state & SKINSTATE_HOVER)
			{
				vd->SetColor32(lgray);
				vd->FillRect(rect);
			}
			DrawCloseCross(vd, dgray2, rect);
			break;
		}

		case NATIVE_NOTIFIER_CLOSE_BUTTON:
		{
			if( state & SKINSTATE_HOVER)
			{
				vd->SetColor32(lgray);
				vd->FillRect(rect);
			}
			DrawCloseCross(vd, dgray2, rect);
			break;
		}

		case NATIVE_LIST_ITEM:
		{
			if(state & SKINSTATE_SELECTED)
			{
				vd->SetColor32(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BACKGROUND_SELECTED));
				vd->FillRect(rect);
			}
			break;
		}

		case NATIVE_SLIDER_HORIZONTAL_TRACK:
		{
			int c = rect.height/2 - 1;
			vd->SetColor32(gray);
			vd->DrawLine(OpPoint(rect.x, rect.y+c), rect.width-1, TRUE);
			if (!(state & SKINSTATE_DISABLED))
				vd->SetColor32(dgray);
			vd->DrawLine(OpPoint(rect.x, rect.y+c+1), rect.width-1, TRUE);
			break;
		}

		case NATIVE_SLIDER_VERTICAL_TRACK:
		{
			int c = rect.width/2 - 1;
			vd->SetColor32(gray);
			vd->DrawLine(OpPoint(rect.x+c, rect.y), rect.height-1, FALSE);
			if (!(state & SKINSTATE_DISABLED))
				vd->SetColor32(dgray);
			vd->DrawLine(OpPoint(rect.x+c+1, rect.y), rect.height-1, FALSE);
			break;
		}

		case NATIVE_SLIDER_HORIZONTAL_KNOB:
		case NATIVE_SLIDER_VERTICAL_KNOB:
		{
			OpRect r(rect);
			if (m_native_type==NATIVE_SLIDER_HORIZONTAL_KNOB)
			{
				if (r.height > SLIDER_HORIZONTAL_KNOB_HEIGHT)
				{
					int c = r.y + r.height/2;
					r.y = c - SLIDER_HORIZONTAL_KNOB_HEIGHT/2;
					r.height = SLIDER_HORIZONTAL_KNOB_HEIGHT;
				}
			}

			if (state & SKINSTATE_DISABLED)
			{
				Draw3DBorder(vd, gray, gray, r);
				Draw3DBorder(vd, lgray, dgray, r.InsetBy(1, 1));
			}
			else
			{
				Draw3DBorder(vd, gray, dgray2, r);
				Draw3DBorder(vd, lgray, dgray, r.InsetBy(1, 1));
			}
			vd->SetColor32(gray);
			vd->FillRect(r.InsetBy(2, 2));
			break;
		}
	}

	return OpStatus::OK;
}

void X11SkinElement::DrawDirectionButton(VisualDevice *vd, OpRect rect, int dir, INT32 state,
										 UINT32 fgcol, UINT32 gray, UINT32 lgray, UINT32 dgray, UINT32 dgray2)
{
	vd->SetColor32(gray);
	vd->FillRect(rect);
	if (state & SKINSTATE_PRESSED)
	{
		vd->SetColor32(dgray);
		vd->DrawRect(rect);
		rect.x++;
		rect.y++;
	}
	else
	{
		Draw3DBorder(vd, gray, dgray2, rect);
		Draw3DBorder(vd, lgray, dgray, rect.InsetBy(1, 1));
	}
	// Draw arrow
	if (!(state & SKINSTATE_DISABLED))
	{
		vd->SetColor32(fgcol);
		DrawArrow(vd, rect, dir);
	}
	else
	{
		rect.x ++;
		rect.y ++;
		vd->SetColor32(lgray);
		DrawArrow(vd, rect, dir);
		rect.x --;
		rect.y --;
		vd->SetColor32(dgray);
		DrawArrow(vd, rect, dir);
	}
}

void X11SkinElement::DrawArrow(VisualDevice *vd, const OpRect &rect, int direction)
{
	static INT32 arrow_offset[4] = { 3, 2, 1, 0 };	///< Offset of the arrow scanline
	static INT32 arrow_length[4] = { 1, 3, 5, 7 };	///< Length of the arrow scanline

	int minsize = MIN(rect.width, rect.height);
	int x = rect.x + rect.width/2;
	int y = rect.y + rect.height/2;

	int levels = 4;
	if (minsize < 9)
		levels = 2;
	else if (minsize < 11)
		levels = 3;

	if (direction == NativeSkinConstants::ARROW_UP || direction == NativeSkinConstants::ARROW_DOWN)
	{
		x -= arrow_length[levels-1]/2;
		y -= levels/2;
	}
	else
	{
		x -= levels/2;
		y -= arrow_length[levels-1]/2;
	}

	if (direction == NativeSkinConstants::ARROW_UP)
	{
		for (int i=0; i<levels; i++)
			vd->DrawLine(OpPoint(x+arrow_offset[i], y+i), arrow_length[i], TRUE);
	}
	else if (direction == NativeSkinConstants::ARROW_DOWN)
	{
		for (int i=0; i<levels; i++)
			vd->DrawLine(OpPoint(x+arrow_offset[i], y+4-i), arrow_length[i], TRUE);
	}
	else if (direction == NativeSkinConstants::ARROW_LEFT)
	{
		for (int i=0; i<levels; i++)
			vd->DrawLine(OpPoint(x+i, y+arrow_offset[i]), arrow_length[i], FALSE);
	}
	else if (direction == NativeSkinConstants::ARROW_RIGHT)
	{
		for (int i=0; i<levels; i++)
			vd->DrawLine(OpPoint(x+4-i, y+arrow_offset[i]), arrow_length[i], FALSE);
	}
}

void X11SkinElement::Draw3DBorder(VisualDevice *vd, UINT32 lcolor, UINT32 dcolor, OpRect rect, int omissions)
{
	if (omissions & NativeSkinConstants::BORDER_OMIT_TOP)
	{
		rect.y --;
		rect.height ++;
	}
	if (omissions & NativeSkinConstants::BORDER_OMIT_BOTTOM)
		rect.height ++;

	vd->SetColor32(lcolor);
	if (!(omissions & NativeSkinConstants::BORDER_OMIT_TOP))
		vd->DrawLine(OpPoint(rect.x, rect.y), rect.width-1, TRUE);
	vd->DrawLine(OpPoint(rect.x, rect.y), rect.height-1, FALSE);
	vd->SetColor32(dcolor);
	if (!(omissions & NativeSkinConstants::BORDER_OMIT_BOTTOM))
		vd->DrawLine(OpPoint(rect.x, rect.y + rect.height - 1), rect.width, TRUE);
	vd->DrawLine(OpPoint(rect.x + rect.width - 1, rect.y), rect.height, FALSE);
}

void X11SkinElement::DrawCheckMark(VisualDevice *vd, UINT32 color, const OpRect &rect)
{
	vd->SetColor32(color);
	int x1 = rect.x + 3;
	int y1 = rect.y + 5;
	int x2 = x1 + 2;
	int y2 = y1 + 2;
	vd->DrawLine(OpPoint(x1, y1), OpPoint(x2, y2), 1);
	vd->DrawLine(OpPoint(x1, y1 + 1), OpPoint(x2, y2 + 1), 1);
	vd->DrawLine(OpPoint(x1, y1 + 2), OpPoint(x2, y2 + 2), 1);
	vd->DrawLine(OpPoint(x2, y2), OpPoint(x2 + 5, y2 - 5), 1);
	vd->DrawLine(OpPoint(x2, y2 + 1), OpPoint(x2 + 5, y2 - 5 + 1), 1);
	vd->DrawLine(OpPoint(x2, y2 + 2), OpPoint(x2 + 5, y2 - 5 + 2), 1);
}


void X11SkinElement::DrawIndeterminate(VisualDevice *vd, UINT32 color, const OpRect &rect)
{
	vd->SetColor32(color);
	int x1 = rect.x + 3;
	int y1 = (rect.y + rect.height - 2) / 2;
	int x2  = rect.width - 4;

	vd->DrawLine(OpPoint(x1, y1), OpPoint(x2, y1), 2);
}


void X11SkinElement::DrawBullet(VisualDevice *vd, UINT32 color, const OpRect &rect)
{
	vd->SetColor32(color);
	vd->FillEllipse(rect.InsetBy(3, 3));
}

void X11SkinElement::DrawCloseCross(VisualDevice *vd, UINT32 color, const OpRect &rect)
{
	vd->SetColor32(color);

	int x1 = rect.x + 1;
	int y1 = rect.y + 1;
	int x2 = x1 + rect.width - 2;
	int y2 = y1 + rect.height - 3;

	vd->DrawLine(OpPoint(x1, y1), OpPoint(x2-1, y2), 1);
	vd->DrawLine(OpPoint(x1+1, y1), OpPoint(x2, y2), 1);

	vd->DrawLine(OpPoint(x1, y2-1), OpPoint(x2-1, y1-1), 1);
	vd->DrawLine(OpPoint(x1+1, y2-1), OpPoint(x2, y1-1), 1);
}


void X11SkinElement::DrawFrame(VisualDevice *vd, UINT32 lcolor, UINT32 dcolor1, UINT32 dcolor2, const OpRect &rect)
{
	int x1 = rect.x;
	int y1 = rect.y;

	vd->SetColor32(lcolor);
	vd->DrawLine(OpPoint(x1, y1), rect.width-1, TRUE);
	vd->DrawLine(OpPoint(x1, y1), rect.height-1, FALSE);
	vd->SetColor32(dcolor1);
	vd->DrawLine(OpPoint(x1+rect.width-2, y1+1), rect.height-2, FALSE);
	vd->DrawLine(OpPoint(x1+1, y1+rect.height-2), rect.width-2, TRUE);
	vd->SetColor32(dcolor2);
	vd->DrawLine(OpPoint(x1+rect.width-1, y1), rect.height, FALSE);
	vd->DrawLine(OpPoint(x1, y1+rect.height-1), rect.width, TRUE);
}


void X11SkinElement::DrawTopFrameLine(VisualDevice *vd, UINT32 lcolor, UINT32 dcolor1, UINT32 dcolor2, const OpRect &rect)
{
	int x1 = rect.x;
	int y1 = rect.y;

	vd->SetColor32(lcolor);
	vd->DrawLine(OpPoint(x1, y1), rect.width-1, TRUE);
	vd->SetColor32(dcolor2);
	vd->DrawLine(OpPoint(x1+rect.width-1, y1), 1, TRUE);
}


void X11SkinElement::DrawTab(VisualDevice *vd, UINT32 lcolor, UINT32 dcolor1, UINT32 dcolor2, const OpRect &rect, BOOL is_active)
{
	int x1 = rect.x;
	int y1 = rect.y;
	int x2 = x1 + rect.width  - 1;

	vd->SetColor32(lcolor);
	vd->DrawLine(OpPoint(x1+2, y1), rect.width-4, TRUE);
	vd->DrawLine(OpPoint(x1+1, y1+1), 1, TRUE);
	vd->DrawLine(OpPoint(x1, y1+2), rect.height-2, FALSE);
	vd->SetColor32(dcolor2);
	vd->DrawLine(OpPoint(x2-1, y1+1), 1, TRUE);
	vd->DrawLine(OpPoint(x2, y1+2), rect.height-2, FALSE);
	vd->SetColor32(dcolor1);
	vd->DrawLine(OpPoint(x2-1, y1+2), rect.height-2, FALSE);

	if(!is_active)
	{
		vd->SetColor32(lcolor);
		vd->DrawLine(OpPoint(x1, y1+rect.height-1), rect.width, TRUE);
	}
}
